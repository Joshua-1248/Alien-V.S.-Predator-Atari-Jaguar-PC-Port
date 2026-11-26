/*
 * RE46 R322 -- Vulkan Xlib swapchain/clear/present diagnostic only.
 * Independently authored for the AVP PC port; no emulator/driver code copied.
 * Build against official Vulkan-Headers, not R320's small discovery ABI.
 * No original-game assets, framebuffers, or gameplay state are accessed here.
 *
 * This is an explicit, standalone WSI bring-up. The game's current 3D,
 * sprites, effects, HUD and frontend remain SOFTWARE until separate parity
 * gates pass; it is incorrect to call this a working hardware game renderer.
 */
#ifndef VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_XLIB_KHR 1
#endif
#include <vulkan/vulkan.h>
#include <X11/Xlib.h>
#include "pc/pc_vulkan_wsi.h"
#include "pc/pc_vulkan_wsi_policy.h"
#include "pc/pc_vulkan_frame_policy.h"
#include "pc/pc_vulkan_upload.h"
#include "pc/pc_vulkan_descriptors.h"
#include "pc/pc_vulkan_spirv_contract.h"
#include "pc/pc_vulkan_opaque_pipeline.h"
#include "pc/pc_vulkan_readback.h"
#include "pc/pc_vulkan_readback_policy.h"
#include "pc/pc_vulkan_geometry_upload.h"
#include "pc/pc_vulkan_ray_columns.h"
#include "pc/pc_vulkan_wall_projection.h"
#include "pc/pc_vulkan_wall_slots.h"
#include "pc/pc_vulkan_wall_gpu_bank.h"
#include "pc/pc_vulkan_wall_draw_batch.h"
#include "pc/pc_vulkan_wall_oracle_capture.h"
#include "pc/pc_vulkan_wall_mixed_fixture.h"
#include "pc/pc_vulkan_wall_live_frame.h"
#include "pc/pc_vulkan_flat_frame.h"
#include "pc/pc_vulkan_layered_frame.h"
#include "pc/pc_hybrid_overlay.h"
#include "pc/pc_hybrid_viewport.h"
#include "pc/pc_hybrid_texture_cache.h"
#include "pc/pc_hybrid_pipeline_cache.h"
#include "pc/pc_hybrid_shader_cache.h"
#include "pc/pc_hybrid_gpu_parity.h"
#include "pc/pc_hybrid_stream_policy.h"
#include "pc/pc_vulkan_hybrid_stream.h"
#include "jaguar/avp_gpu/world_texture_transfer.h"
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define AVP_WSI_MAX_DEVICES 128u
#define AVP_WSI_MAX_FORMATS 128u
#define AVP_WSI_MAX_QUEUES 128u
#define AVP_WSI_MAX_SWAP_IMAGES 64u

typedef struct AvpWsiScope {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkDevice device;
    VkSwapchainKHR swapchain;
    VkCommandPool commands;
    VkSemaphore acquired, finished;
    VkSemaphore *image_finished; /* per-swapchain-image presentation wait */
    VkFence frame_fence;
    VkCommandBuffer frame_command;
    VkPhysicalDevice gpu;
    VkQueue queue;
    VkImage *images;
    AvpVkUploadedR32 diagnostic_texture; /* GPU source image, not framebuffer */
    AvpVkUploadedR32 secondary_texture; /* R337 second synthetic R32 source */
    AvpVkUploadedR32 diagnostic_lut; /* synthetic 256x256 lookup, diagnostic only */
    AvpVkSampledPair diagnostic_descriptors; /* image views + sampler + descriptor set */
    AvpVkSampledPair secondary_descriptors; /* R337 second binding, same layout */
    uint32_t image_count;
    uint32_t queue_family;
    VkExtent2D extent;
    VkFormat swapchain_format;
    int shader_requested;
    int gameplay_presentation; /* distinguish game mode from FIFO-only diagnostics */
    int vsync_requested; /* persistent host Video preference */
    int columns_mode; /* 0 checker, 1 rays, 2 projected, 3 RGB two-slot,
                       * 4 CRY+RGB v3 fixture, 5 original live opaque walls */
    const AvpWorldScenePacket *live_scene; /* caller-owned, synchronous */
    const AvpAvpGpuState *live_state;     /* caller-owned, synchronous */
    VkRenderPass shader_renderpass;
    VkImageView shader_output_view;
    VkFramebuffer shader_framebuffer;
    AvpVkOpaquePipeline shader_pipeline;
    AvpVkReadback shader_readback;
    /* R361 opt-in same-window hybrid present, no completed Software upload. */
    AvpHybridTextureCache hybrid_texture_cache; /* R362 exact-content GPU bank reuse */
    AvpVkHybridStream hybrid_stream; /* R364 reusable geometry + staged foreground */
    /* R378: pending host-coherent source upload staging survives through the
     * GPU frame fence. Source GPU images belong to wall_gpu after adoption. */
    AvpVkDeferredR32Upload hybrid_texture_transfers[AVP_WORLD_SCENE_IMAGE_SLOTS];
    /* R363: immutable graphics pipelines/render pass + one render target per
     * swapchain image. Destroy on resize, device change or shader mutation. */
    AvpHybridPipelineCache hybrid_pipeline_cache;
    AvpHybridShaderCache hybrid_shader_cache; /* R365 validated SPIR-V on unchanged frames */
    AvpPcScanlines hybrid_verified_scanlines;
    int hybrid_gpu_verified; /* first composited image passed GPU readback before present */
    VkImageView *hybrid_views;
    VkFramebuffer *hybrid_framebuffers;
    AvpVkUploadedR32 hybrid_foreground;
    AvpVkSampledPair hybrid_foreground_set;
    AvpVkOpaquePipeline hybrid_foreground_pipeline;
    AvpVkUploadedGeometry diagnostic_geometry; /* synthetic ray stream, GPU storage */
    /* R340: genuine R338->R339 source staging/GPU images per descriptor.
     * Owned only by opt-in two-slot diagnostic, never by game renderer. */
    AvpVkWallGpuBank wall_gpu;
    /* R356: separate floor/ceiling GPU source bank and immutable source
     * snapshot; the original wall-only mode never allocates these. */
    AvpVkFlatFrame flat_frame;
    AvpVkLayeredFrame layered_frame; /* R358 original layered scene source refs */
    AvpVkUploadedGeometry flat_geometry;
    AvpVkWallGpuBank flat_gpu;
    AvpVkSampledPair flat_sets[AVP_WORLD_SCENE_IMAGE_SLOTS];
    AvpVkOpaquePipeline flat_pipeline;
    const char *flat_fragment_spv; /* borrowed path, synchronous call */
    AvpVkWallDrawBatch wall_draws;
    AvpVkWallOracleCapture wall_oracle; /* R347 independent GPU-pixel reference */
    AvpVkSampledPair wall_sets[AVP_WORLD_SCENE_IMAGE_SLOTS];
    /* Standalone diagnostic only: the Vulkan ICD may install Xlib
     * close-display hooks. Keep the instance alive until these hooks run.
     * Reused/embedded callers never transfer X11 ownership here. */
    Display *owned_x11_display;
    Window owned_x11_window;
} AvpWsiScope;

/* R363: target views are owned by the swapchain generation. No view can
 * survive vkDestroySwapchainKHR, and no framebuffer can outlive its pass. */
static void avp_wsi_hybrid_drop_targets(AvpWsiScope *s)
{
    if(!s)return;
    if(s->device){
        for(uint32_t i=0u;i<s->image_count;i++){
            if(s->hybrid_framebuffers && s->hybrid_framebuffers[i])
                vkDestroyFramebuffer(s->device,s->hybrid_framebuffers[i],NULL);
            if(s->hybrid_views && s->hybrid_views[i])
                vkDestroyImageView(s->device,s->hybrid_views[i],NULL);
        }
    }
    free(s->hybrid_framebuffers);
    free(s->hybrid_views);
    s->hybrid_framebuffers=NULL;
    s->hybrid_views=NULL;
}

/* Release persistent hybrid pass in dependency order; called only after
 * the last submitted GPU frame is finished (fence or deviceWaitIdle). */
static void avp_wsi_hybrid_drop_pass(AvpWsiScope *s)
{
    if(!s)return;
    avp_wsi_hybrid_drop_targets(s);
    if(s->device){
        avp_vk_opaque_pipeline_dispose(s->device,&s->hybrid_foreground_pipeline);
        avp_vk_opaque_pipeline_dispose(s->device,&s->shader_pipeline);
        if(s->shader_renderpass)vkDestroyRenderPass(s->device,s->shader_renderpass,NULL);
    }
    s->shader_renderpass=VK_NULL_HANDLE;
    /* A new pipeline/viewport must prove GPU pixel parity again before its
     * first present, even if the CPU scene oracle still passes. */
    s->hybrid_gpu_verified=0;
    if(s->device)avp_vk_readback_destroy(s->device,&s->shader_readback);
    avp_hybrid_pipeline_cache_invalidate(&s->hybrid_pipeline_cache);
}

/* Lazy per-image targets: a swapchain image keeps the same image view and
 * framebuffer across reacquisitions; no per-frame VkFramebuffer churn. */
static int avp_wsi_hybrid_ensure_target(AvpWsiScope *s,uint32_t index)
{
    VkImageViewCreateInfo view={0};
    VkFramebufferCreateInfo framebuffer={0};
    if(!s||!s->device||!s->shader_renderpass||!s->swapchain||
       !s->images||!s->image_count||index>=s->image_count)return 0;
    if(!s->hybrid_views || !s->hybrid_framebuffers){
        /* Never leave half a swapchain-generation allocation installed. */
        avp_wsi_hybrid_drop_targets(s);
        s->hybrid_views=calloc(s->image_count,sizeof(*s->hybrid_views));
        s->hybrid_framebuffers=calloc(s->image_count,sizeof(*s->hybrid_framebuffers));
        if(!s->hybrid_views||!s->hybrid_framebuffers){
            avp_wsi_hybrid_drop_targets(s);
            return 0;
        }
    }
    if(s->hybrid_framebuffers[index])return 1;
    view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image=s->images[index];
    view.viewType=VK_IMAGE_VIEW_TYPE_2D;
    view.format=s->swapchain_format;
    view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount=1u;
    view.subresourceRange.layerCount=1u;
    if(vkCreateImageView(s->device,&view,NULL,&s->hybrid_views[index])!=VK_SUCCESS)
        return 0;
    framebuffer.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass=s->shader_renderpass;
    framebuffer.attachmentCount=1u;
    framebuffer.pAttachments=&s->hybrid_views[index];
    framebuffer.width=s->extent.width;
    framebuffer.height=s->extent.height;
    framebuffer.layers=1u;
    if(vkCreateFramebuffer(s->device,&framebuffer,NULL,&s->hybrid_framebuffers[index])!=VK_SUCCESS)
        return 0;
    return 1;
}

static void avp_wsi_status(AvpVkWsiSmoke *out, const char *fmt, ...)
{
    va_list ap;
    if (!out) return;
    va_start(ap, fmt);
    (void)vsnprintf(out->detail, sizeof(out->detail), fmt, ap);
    va_end(ap);
}

/* vkDeviceWaitIdle before teardown is intentional: one diagnostic frame only.
 * This function never shuts down the existing game's X11 window or GC. */
static void avp_wsi_destroy_internal(AvpWsiScope *s,int keep_instance)
{
    VkInstance retained=keep_instance?s->instance:VK_NULL_HANDLE;
    if (s->device) {
        (void)vkDeviceWaitIdle(s->device);
        avp_vk_readback_destroy(s->device,&s->shader_readback);
        if(s->hybrid_framebuffers||s->hybrid_pipeline_cache.valid||
           s->hybrid_foreground_pipeline.pipeline)
            avp_wsi_hybrid_drop_pass(s);
        avp_vk_opaque_pipeline_dispose(s->device,&s->hybrid_foreground_pipeline);
        avp_vk_sampled_pair_dispose(s->device,&s->hybrid_foreground_set);
        avp_vk_uploaded_r32_dispose(s->device,&s->hybrid_foreground);
        avp_vk_wall_oracle_capture_dispose(&s->wall_oracle);
        avp_vk_geometry_dispose(s->device,&s->diagnostic_geometry);
        avp_vk_geometry_dispose(s->device,&s->flat_geometry);
        avp_vk_opaque_pipeline_dispose(s->device,&s->flat_pipeline);
        avp_vk_opaque_pipeline_dispose(s->device,&s->shader_pipeline);
        if (s->shader_framebuffer) vkDestroyFramebuffer(s->device,s->shader_framebuffer,NULL);
        if (s->shader_output_view) vkDestroyImageView(s->device,s->shader_output_view,NULL);
        if (s->shader_renderpass) vkDestroyRenderPass(s->device,s->shader_renderpass,NULL);
        for (uint32_t slot=0;slot<AVP_WORLD_SCENE_IMAGE_SLOTS;slot++) {
            avp_vk_sampled_pair_dispose(s->device,&s->flat_sets[slot]);
            avp_vk_sampled_pair_dispose(s->device,&s->wall_sets[slot]);
        }
        /* No persistent frame resource is destroyed while a wall descriptor
         * still references its storage buffer or a foreground image view. */
        avp_vk_hybrid_stream_dispose(s->device,&s->hybrid_stream);
        /* DeviceWaitIdle at function entry has completed all deferred copies. */
        for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
            avp_vk_r32_deferred_dispose(s->device,&s->hybrid_texture_transfers[i]);
        avp_vk_sampled_pair_dispose(s->device,&s->secondary_descriptors);
        avp_vk_sampled_pair_dispose(s->device,&s->diagnostic_descriptors);
        avp_vk_uploaded_r32_dispose(s->device,&s->diagnostic_lut);
        /* Descriptor views and GPU reads must finish before image disposal. */
        avp_vk_wall_gpu_bank_dispose(&s->wall_gpu);
        avp_vk_wall_gpu_bank_dispose(&s->flat_gpu);
        avp_vk_uploaded_r32_dispose(s->device,&s->secondary_texture);
        avp_vk_uploaded_r32_dispose(s->device,&s->diagnostic_texture);
        if (s->frame_fence) vkDestroyFence(s->device, s->frame_fence, NULL);
        if (s->image_finished) {
            for (uint32_t i = 0; i < s->image_count; i++) {
                if (s->image_finished[i]) vkDestroySemaphore(s->device, s->image_finished[i], NULL);
            }
        }
        if (s->finished) vkDestroySemaphore(s->device, s->finished, NULL);
        if (s->acquired) vkDestroySemaphore(s->device, s->acquired, NULL);
        if (s->commands) vkDestroyCommandPool(s->device, s->commands, NULL);
        if (s->swapchain) vkDestroySwapchainKHR(s->device, s->swapchain, NULL);
        vkDestroyDevice(s->device, NULL);
    }
    avp_vk_flat_frame_dispose(&s->flat_frame);
    avp_vk_layered_frame_dispose(&s->layered_frame);
    avp_hybrid_texture_cache_invalidate(&s->hybrid_texture_cache);
    avp_hybrid_pipeline_cache_invalidate(&s->hybrid_pipeline_cache);
    avp_hybrid_shader_cache_dispose(&s->hybrid_shader_cache);
    free(s->image_finished);
    free(s->images);
    if (s->instance && s->surface)
        vkDestroySurfaceKHR(s->instance, s->surface, NULL);
    /* Xlib's registered close hooks must run while the ICD which installed
     * them is still resident. Destroying the final VkInstance first can
     * unload that library, leaving a dangling hook in XCloseDisplay.
     * The surface and device are already destroyed at this point. */
    if (s->owned_x11_display) {
        if (s->owned_x11_window)
            XDestroyWindow(s->owned_x11_display, s->owned_x11_window);
        XCloseDisplay(s->owned_x11_display);
    }
    if (s->instance && !keep_instance) vkDestroyInstance(s->instance, NULL);
    memset(s, 0, sizeof(*s));
    s->instance=retained;
}
static void avp_wsi_destroy(AvpWsiScope *s)
{
    avp_wsi_destroy_internal(s,0);
}
/* Keep the ICD resident while Xlib executes the close-display hooks it
 * installed. Only the game-owned borrowed-Display lifetime uses this path. */
static void avp_wsi_suspend(AvpWsiScope *s)
{
    avp_wsi_destroy_internal(s,1);
}

static uint32_t avp_wsi_name_hash(const char *name)
{
    uint32_t h = 2166136261u;
    while (*name) { h ^= (uint8_t)*name++; h *= 16777619u; }
    return h;
}

static int avp_wsi_swapchain_ext(VkPhysicalDevice gpu)
{
    uint32_t n = 0;
    VkExtensionProperties *e = NULL;
    int found = 0;
    if (vkEnumerateDeviceExtensionProperties(gpu, NULL, &n, NULL) != VK_SUCCESS || !n || n > 2048u)
        return 0;
    e = calloc(n, sizeof(*e));
    if (!e) return 0;
    if (vkEnumerateDeviceExtensionProperties(gpu, NULL, &n, e) == VK_SUCCESS) {
        for (uint32_t i = 0; i < n; i++) {
            if (strcmp(e[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) { found = 1; break; }
        }
    }
    free(e);
    return found;
}

/* Choose an actual graphics AND present queue from the same family. No
 * offscreen-only GPU accepted for a windowed Hardware configuration. */
static int avp_wsi_queue(VkPhysicalDevice gpu, VkSurfaceKHR surface, uint32_t *family)
{
    uint32_t n = 0;
    VkQueueFamilyProperties *q;
    int found = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n, NULL);
    if (!n || n > AVP_WSI_MAX_QUEUES) return 0;
    q = calloc(n, sizeof(*q));
    if (!q) return 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n, q);
    for (uint32_t i = 0; i < n; i++) {
        VkBool32 supported = VK_FALSE;
        if (!(q[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !q[i].queueCount) continue;
        if (vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &supported) == VK_SUCCESS && supported) {
            *family = i;
            found = 1;
            break;
        }
    }
    free(q);
    return found;
}

static int avp_wsi_pick(AvpWsiScope *s, const char *requested, AvpVkWsiSmoke *out)
{
    uint32_t count = 0;
    VkPhysicalDevice *devices = NULL;
    VkPhysicalDevice best = VK_NULL_HANDLE;
    uint32_t best_family = UINT32_MAX;
    unsigned best_rank = 0;
    VkResult r = vkEnumeratePhysicalDevices(s->instance, &count, NULL);
    if (r != VK_SUCCESS || !count || count > AVP_WSI_MAX_DEVICES) {
        avp_wsi_status(out, "No Vulkan physical devices (result %d)", (int)r);
        return 0;
    }
    devices = calloc(count, sizeof(*devices));
    if (!devices) { avp_wsi_status(out, "Out of memory (device enumeration)"); return 0; }
    r = vkEnumeratePhysicalDevices(s->instance, &count, devices);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Device enumeration failed: %d", (int)r); free(devices); return 0; }
    for (uint32_t i = 0; i < count; i++) {
        VkPhysicalDeviceProperties prop;
        VkSurfaceCapabilitiesKHR caps;
        uint32_t family;
        uint32_t formats = 0;
        unsigned rank;
        char key[64];
        unsigned duplicate = 0;
        vkGetPhysicalDeviceProperties(devices[i], &prop);
        rank = avp_wsi_device_rank(prop.deviceType);
        if (!rank) continue;
        /* Keep R320's vendor:device:namehash:duplicate discovery key format.
         * Duplicate ordinal follows enumeration order, even for ineligible
         * duplicates. This remains best-effort until UUID keying arrives. */
        for (uint32_t earlier = 0; earlier < i; earlier++) {
            VkPhysicalDeviceProperties prior;
            vkGetPhysicalDeviceProperties(devices[earlier], &prior);
            if (avp_wsi_device_rank(prior.deviceType) && prior.vendorID == prop.vendorID &&
                prior.deviceID == prop.deviceID &&
                avp_wsi_name_hash(prior.deviceName) == avp_wsi_name_hash(prop.deviceName)) duplicate++;
        }
        snprintf(key, sizeof(key), "%08x:%08x:%08x:%u", prop.vendorID,
            prop.deviceID, avp_wsi_name_hash(prop.deviceName), duplicate);
        /* R349: an explicit key is a strict identity constraint. Test it
         * before querying expensive WSI and shader capabilities. */
        if (!avp_wsi_gpu_candidate_rank(prop.deviceType,1,key,requested))
            continue;
        if (s->shader_requested) {
            VkFormatProperties r32={0};
            AvpWsiShaderCaps shader_caps={0};
            const VkPhysicalDeviceLimits *lim=&prop.limits;
            vkGetPhysicalDeviceFormatProperties(devices[i],VK_FORMAT_R32_UINT,&r32);
            shader_caps.r32_optimal_sampled=
                (r32.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)!=0u;
            shader_caps.max_image_dimension_2d=lim->maxImageDimension2D;
            shader_caps.max_storage_buffer_range=lim->maxStorageBufferRange;
            shader_caps.max_push_constants_size=lim->maxPushConstantsSize;
            shader_caps.max_per_stage_sampled_images=lim->maxPerStageDescriptorSampledImages;
            shader_caps.max_set_sampled_images=lim->maxDescriptorSetSampledImages;
            shader_caps.max_per_stage_samplers=lim->maxPerStageDescriptorSamplers;
            shader_caps.max_set_samplers=lim->maxDescriptorSetSamplers;
            shader_caps.max_per_stage_storage_buffers=lim->maxPerStageDescriptorStorageBuffers;
            shader_caps.max_set_storage_buffers=lim->maxDescriptorSetStorageBuffers;
            /* The R348 mixed fixture has a 720-line source image. */
            if (!avp_wsi_shader_caps_sufficient(&shader_caps,
                    s->columns_mode==4 ? 720u : 256u))continue;
        }
        if (!avp_wsi_swapchain_ext(devices[i]) || !avp_wsi_queue(devices[i], s->surface, &family)) continue;
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[i], s->surface, &caps) != VK_SUCCESS) continue;
        /* Shader parity tests require a color attachment that can be copied
         * back. Clear-only diagnostics continue using TRANSFER_DST. */
        if (s->shader_requested) {
            if ((caps.supportedUsageFlags &
                (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) !=
                (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) continue;
        } else if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) continue;
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(devices[i], s->surface, &formats, NULL) != VK_SUCCESS || !formats) continue;
        if (requested && requested[0]) {
            best = devices[i]; best_family = family; break;
        }
        if (rank > best_rank) { best = devices[i]; best_family = family; best_rank = rank; }
    }
    free(devices);
    if (!best) {
        if (requested && requested[0])
            avp_wsi_status(out,"Selected Vulkan GPU unavailable or lacks required WSI/shader capabilities");
        else
            avp_wsi_status(out,"No non-CPU GPU supports required Xlib graphics, presentation and shader capabilities");
        return 0;
    }
    s->gpu = best;
    s->queue_family = best_family;
    {
        VkPhysicalDeviceProperties prop;
        vkGetPhysicalDeviceProperties(best, &prop);
        snprintf(out->device_name, sizeof(out->device_name), "%s", prop.deviceName);
    }
    return 1;
}

static int avp_wsi_make_swapchain(AvpWsiScope *s, uint32_t requested_w,
                                  uint32_t requested_h, AvpVkWsiSmoke *out)
{
    VkSurfaceCapabilitiesKHR caps;
    VkSurfaceFormatKHR *formats = NULL;
    VkSurfaceFormatKHR format = {VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkSwapchainCreateInfoKHR info;
    uint32_t n = 0, image_count, alpha;
    VkPresentModeKHR present_mode=VK_PRESENT_MODE_FIFO_KHR;
    VkResult r;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s->gpu, s->surface, &caps) != VK_SUCCESS) {
        avp_wsi_status(out, "Surface capabilities unavailable"); return 0;
    }
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(s->gpu, s->surface, &n, NULL) != VK_SUCCESS ||
        !n || n > AVP_WSI_MAX_FORMATS) {
        avp_wsi_status(out, "No supported surface formats"); return 0;
    }
    formats = calloc(n, sizeof(*formats));
    if (!formats) { avp_wsi_status(out, "Out of memory (surface formats)"); return 0; }
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(s->gpu, s->surface, &n, formats) != VK_SUCCESS) {
        free(formats); avp_wsi_status(out, "Surface format enumeration failed"); return 0;
    }
    format = formats[0];
    if (n == 1u && format.format == VK_FORMAT_UNDEFINED) {
        format.format = VK_FORMAT_B8G8R8A8_UNORM;
        format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    } else {
        for (uint32_t i = 0; i < n; i++) {
            if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
                formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) { format = formats[i]; break; }
        }
    }
    if (s->shader_requested) {
        int supported=0;
        /* Only linear UNORM formats permit exact comparison with the
         * 8-bit integer CPU oracle. Do not silently compare SRGB bytes. */
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM ||
            format.format == VK_FORMAT_R8G8B8A8_UNORM) supported=1;
        if (!supported) {
            for (uint32_t i=0u;i<n;i++) {
                if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM &&
                    formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    format=formats[i];supported=1;break;
                }
            }
        }
        if (!supported || (caps.supportedUsageFlags &
            (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) !=
            (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT)) {
            free(formats);
            avp_wsi_status(out,"GPU surface lacks readable UNORM shader color output");return 0;
        }
    }
    free(formats);
    s->swapchain_format=format.format;
    {
        AvpWsiExtent size = {0u, 0u};
        AvpWsiExtent req = {requested_w, requested_h};
        AvpWsiExtent cur = {caps.currentExtent.width, caps.currentExtent.height};
        AvpWsiExtent min = {caps.minImageExtent.width, caps.minImageExtent.height};
        AvpWsiExtent max = {caps.maxImageExtent.width, caps.maxImageExtent.height};
        if (!avp_wsi_extent(req, cur, min, max, &size)) {
            avp_wsi_status(out, "Minimized, invalid or zero-size surface; retry after resize"); return 0;
        }
        s->extent.width = size.width; s->extent.height = size.height;
    }
    if (!s->extent.width || !s->extent.height) {
        avp_wsi_status(out, "Minimized or zero-size surface; retry after resize"); return 0;
    }
    image_count = avp_wsi_image_count(caps.minImageCount, caps.maxImageCount);
    if (!image_count) { avp_wsi_status(out, "Invalid surface image-count limits"); return 0; }
    /* pick a supported composite alpha bit (do not assume opaque). */
    alpha = avp_wsi_composite_alpha(caps.supportedCompositeAlpha);
    if (!alpha) { avp_wsi_status(out, "No composite alpha mode"); return 0; }
    memset(&info, 0, sizeof(info));
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = s->surface;
    info.minImageCount = image_count;
    info.imageFormat = format.format;
    info.imageColorSpace = format.colorSpace;
    info.imageExtent = s->extent;
    info.imageArrayLayers = 1u;
    info.imageUsage = s->shader_requested ?
        (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT) :
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = caps.currentTransform;
    info.compositeAlpha = (VkCompositeAlphaFlagBitsKHR)alpha;
    /* R381: do not force V-Sync on GPU gameplay when the Video menu says
     * V-Sync Off. Check the surface's real advertised modes. Standalone
     * diagnostic behavior remains FIFO for deterministic operation. */
    if(s->gameplay_presentation && !s->vsync_requested){
        uint32_t mode_count=0u;
        if(vkGetPhysicalDeviceSurfacePresentModesKHR(s->gpu,s->surface,
                &mode_count,NULL)==VK_SUCCESS && mode_count && mode_count<=64u){
            VkPresentModeKHR modes[64];
            uint32_t mask=(1u<<2); /* FIFO is required by Vulkan WSI */
            if(vkGetPhysicalDeviceSurfacePresentModesKHR(s->gpu,s->surface,
                    &mode_count,modes)==VK_SUCCESS){
                for(uint32_t mi=0u;mi<mode_count;mi++){
                    if(modes[mi]==VK_PRESENT_MODE_IMMEDIATE_KHR)mask|=1u<<0;
                    if(modes[mi]==VK_PRESENT_MODE_MAILBOX_KHR)mask|=1u<<1;
                }
                present_mode=(VkPresentModeKHR)avp_wsi_present_mode(0,mask);
            }
        }
    }
    info.presentMode=present_mode;
    info.clipped = VK_TRUE;
    r = vkCreateSwapchainKHR(s->device, &info, NULL, &s->swapchain);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "vkCreateSwapchainKHR failed: %d", (int)r); return 0; }
    out->swapchain_created = 1;
    if(s->gameplay_presentation)
        fprintf(stderr,"[Hybrid Vulkan] WSI present mode: %s (Video V-Sync %s).\n",
            present_mode==VK_PRESENT_MODE_IMMEDIATE_KHR?"IMMEDIATE":
            (present_mode==VK_PRESENT_MODE_MAILBOX_KHR?"MAILBOX":"FIFO"),
            s->vsync_requested?"On":"Off");
    if (vkGetSwapchainImagesKHR(s->device, s->swapchain, &s->image_count, NULL) != VK_SUCCESS ||
        !s->image_count || s->image_count > AVP_WSI_MAX_SWAP_IMAGES) {
        avp_wsi_status(out, "Invalid swapchain image count"); return 0;
    }
    s->images = calloc(s->image_count, sizeof(*s->images));
    if (!s->images) { avp_wsi_status(out, "Out of memory (swapchain images)"); return 0; }
    if (vkGetSwapchainImagesKHR(s->device, s->swapchain, &s->image_count, s->images) != VK_SUCCESS) {
        avp_wsi_status(out, "Swapchain image enumeration failed"); return 0;
    }
    out->width = s->extent.width;
    out->height = s->extent.height;
    return 1;
}

static int avp_wsi_clear_once(AvpWsiScope *s, AvpVkWsiSmoke *out)
{
    VkCommandPoolCreateInfo pool = {0};
    VkCommandBufferAllocateInfo alloc = {0};
    VkCommandBufferBeginInfo begin = {0};
    VkSemaphoreCreateInfo sem = {0};
    VkCommandBuffer command = VK_NULL_HANDLE;
    VkImageMemoryBarrier before = {0}, after = {0};
    VkClearColorValue blue = {{0.06f, 0.12f, 0.22f, 1.0f}};
    VkImageSubresourceRange range = {0};
    VkSubmitInfo submit = {0};
    VkPresentInfoKHR present = {0};
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    uint32_t index = UINT32_MAX;
    VkResult r;
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.queueFamilyIndex = s->queue_family;
    if ((r = vkCreateCommandPool(s->device, &pool, NULL, &s->commands)) != VK_SUCCESS) {
        avp_wsi_status(out, "Create command pool failed: %d", (int)r); return 0;
    }
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if ((r = vkCreateSemaphore(s->device, &sem, NULL, &s->acquired)) != VK_SUCCESS ||
        (r = vkCreateSemaphore(s->device, &sem, NULL, &s->finished)) != VK_SUCCESS) {
        avp_wsi_status(out, "Create semaphores failed: %d", (int)r); return 0;
    }
    r = vkAcquireNextImageKHR(s->device, s->swapchain, UINT64_MAX, s->acquired, VK_NULL_HANDLE, &index);
    if ((r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) || index >= s->image_count) {
        avp_wsi_status(out, "Acquire failed (resize/out of date?): %d", (int)r); return 0;
    }
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = s->commands;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1u;
    if ((r = vkAllocateCommandBuffers(s->device, &alloc, &command)) != VK_SUCCESS) {
        avp_wsi_status(out, "Allocate command buffer failed: %d", (int)r); return 0;
    }
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if ((r = vkBeginCommandBuffer(command, &begin)) != VK_SUCCESS) {
        avp_wsi_status(out, "Begin command buffer failed: %d", (int)r); return 0;
    }
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1u;
    range.layerCount = 1u;
    before.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    before.srcAccessMask = 0u;
    before.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    before.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; /* previous contents discarded */
    before.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    before.image = s->images[index];
    before.subresourceRange = range;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &before);
    vkCmdClearColorImage(command, s->images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        &blue, 1u, &range);
    after = before;
    after.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    after.dstAccessMask = 0u;
    after.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    after.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &after);
    if ((r = vkEndCommandBuffer(command)) != VK_SUCCESS) {
        avp_wsi_status(out, "End command buffer failed: %d", (int)r); return 0;
    }
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1u;
    submit.pWaitSemaphores = &s->acquired;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &command;
    submit.signalSemaphoreCount = 1u;
    submit.pSignalSemaphores = &s->finished;
    r = vkQueueSubmit(s->queue, 1u, &submit, VK_NULL_HANDLE);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "GPU submit failed: %d", (int)r); return 0; }
    out->command_submitted = 1;
    out->frames_submitted = 1u;
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1u;
    present.pWaitSemaphores = &s->finished;
    present.swapchainCount = 1u;
    present.pSwapchains = &s->swapchain;
    present.pImageIndices = &index;
    r = vkQueuePresentKHR(s->queue, &present);
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        avp_wsi_status(out, "Present failed (resize/device lost?): %d", (int)r); return 0;
    }
    out->presented = 1;
    out->frames_presented = 1u;
    avp_wsi_status(out, "GPU swapchain clear + presentation accepted (%ux%u)",
        out->width, out->height);
    return 1;
}

/* R332: independently constructed, 1-column test stream. This only
 * exercises a real GPU STORAGE_BUFFER transfer when the optional SDK
 * diagnostic is run. It contains no cartridge data or completed pixels. */
static void avp_geom_le32(uint8_t *p,uint32_t v)
{
    for(unsigned i=0;i<4u;i++)p[i]=(uint8_t)(v>>(i*8u));
}
static void avp_geom_le_double(uint8_t *p,double v)
{
    uint64_t n;
    memcpy(&n,&v,sizeof(n));
    avp_geom_le32(p,(uint32_t)n);
    avp_geom_le32(p+4u,(uint32_t)(n>>32));
}
/* R348: mixed CRY16 + RGB hi-res source diagnostic. Unlike R337's two
 * RGB inputs, this validates real CRY LUT shading and the exact v3 DDA
 * row path with independent captured source data, never a CPU framebuffer. */
static int avp_wsi_upload_mixed_wall_geometry(AvpWsiScope *scope,
                                              AvpVkWsiSmoke *out)
{
    AvpVkWallMixedFixture fixture={0};
    AvpWorldGpuStaging scratch={0};
    AvpVkWallSlotPlan plan={0};
    int ok=0;
    if(!avp_vk_wall_mixed_fixture_build(scope->extent.width,
            scope->extent.height,&fixture) ||
       !avp_vk_wall_slots_collect(fixture.geometry.data,
            fixture.geometry.bytes,&plan) ||
       !avp_vk_wall_oracle_capture_build(fixture.geometry.data,
            fixture.geometry.bytes,fixture.sources,fixture.cry_lut,
            65536u,131072u,&scope->wall_oracle) ||
       !avp_vk_wall_gpu_bank_build(scope->gpu,scope->device,scope->queue,
            scope->queue_family,&plan,fixture.sources,65536u,131072u,
            &scratch,&scope->wall_gpu) ||
       !avp_vk_wall_draw_batch_build(fixture.geometry.data,
            fixture.geometry.bytes,&scope->wall_gpu.bank,&scope->wall_draws))
        goto finish;
    if(!avp_vk_geometry_upload(scope->gpu,scope->device,scope->queue,
           scope->queue_family,fixture.geometry.data,fixture.geometry.bytes,
           &scope->diagnostic_geometry))goto finish;
    out->geometry_bytes_uploaded=(uint32_t)fixture.geometry.bytes;
    out->textures_uploaded+=scope->wall_gpu.bank.count;
    out->texture_bytes_uploaded+=(uint32_t)scope->wall_gpu.bank.uploaded_bytes;
    ok=1;
finish:
    avp_world_gpu_staging_dispose(&scratch);
    avp_vk_wall_mixed_fixture_dispose(&fixture);
    if(!ok)avp_wsi_status(out,"Mixed CRY/RGB exact-row GPU geometry or textures failed");
    return ok;
}

/* R350: GPU-owned output from the SAME original ray/texture selection and
 * exact hi-res row-preparation path used by the source-faithful live bridge.
 * Never uploads pre-rendered Software framebuffer pixels. One verified wall
 * frame only; full gameplay still needs floors, ceilings, doors and sprites. */
static int avp_wsi_upload_live_wall_geometry(AvpWsiScope *scope,
                                             AvpVkWsiSmoke *out)
{
    AvpVkWallProjectionStream wire={0};
    AvpVkWallSlotPlan plan={0};
    const AvpGpuRgbImage *sources[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    AvpWorldGpuStaging scratch={0};
    uint32_t *lut=NULL;
    int ok=0;
    const size_t per_image_limit=64u*1024u*1024u;
    const uint64_t total_image_limit=512ull*1024u*1024u;
    if(!scope->live_scene || !scope->live_state ||
       scope->live_scene->width!=scope->extent.width ||
       scope->live_scene->height!=scope->extent.height){
        avp_wsi_status(out,"Live wall scene dimensions differ from Vulkan surface");
        goto done;
    }
    if(!avp_vk_wall_live_prepare(scope->live_scene,scope->live_state,
                                 &wire,&plan,sources)){
        avp_wsi_status(out,"Live ray packet contains unsupported wall geometry or sources");
        goto done;
    }
    if(!plan.used_count){
        avp_wsi_status(out,"Live wall scene has no opaque wall columns");
        goto done;
    }
    lut=malloc(65536u*sizeof(*lut));
    if(!lut || !avp_world_gpu_build_cry_lut(lut,65536u)){
        avp_wsi_status(out,"Failed to obtain original Jaguar CRY lookup");
        goto done;
    }
    /* Retain CPU source pixels and geometry for an INDEPENDENT GPU readback
     * comparison. The caller may release its game textures after return. */
    if(!avp_vk_wall_oracle_capture_build(wire.data,wire.bytes,sources,lut,
          per_image_limit,total_image_limit,&scope->wall_oracle) ||
       !avp_vk_wall_gpu_bank_build(scope->gpu,scope->device,scope->queue,
          scope->queue_family,&plan,sources,per_image_limit,total_image_limit,
          &scratch,&scope->wall_gpu) ||
       !avp_vk_wall_draw_batch_build(wire.data,wire.bytes,
          &scope->wall_gpu.bank,&scope->wall_draws) ||
       !avp_vk_geometry_upload(scope->gpu,scope->device,scope->queue,
          scope->queue_family,wire.data,wire.bytes,&scope->diagnostic_geometry)){
        avp_wsi_status(out,"Live source-to-Vulkan geometry/texture upload failed");
        goto done;
    }
    out->geometry_bytes_uploaded=(uint32_t)wire.bytes;
    out->textures_uploaded+=scope->wall_gpu.bank.count;
    out->texture_bytes_uploaded=(uint32_t)scope->wall_gpu.bank.uploaded_bytes;
    if(scope->columns_mode==6){
        /* The floor/ceiling pass gets an independent source-backed capture
         * and GPU R32_UINT bank. Keep wall upload and its oracle untouched. */
        if(!avp_vk_flat_frame_build(scope->live_scene,scope->live_state,
                                    &scope->flat_frame) ||
           !scope->flat_frame.slots.used_count ||
           !avp_vk_wall_gpu_bank_build(scope->gpu,scope->device,scope->queue,
              scope->queue_family,&scope->flat_frame.slots,
              scope->flat_frame.images,per_image_limit,total_image_limit,
              &scratch,&scope->flat_gpu) ||
           !avp_vk_geometry_upload(scope->gpu,scope->device,scope->queue,
              scope->queue_family,scope->flat_frame.geometry.data,
              scope->flat_frame.geometry.bytes,&scope->flat_geometry)){
            avp_wsi_status(out,"Live floor/ceiling source upload failed (unsupported opaque/nearest scene)");
            goto done;
        }
        out->textures_uploaded+=scope->flat_gpu.bank.count;
        out->texture_bytes_uploaded+=(uint32_t)scope->flat_gpu.bank.uploaded_bytes;
        out->geometry_bytes_uploaded+=(uint32_t)scope->flat_frame.geometry.bytes;
    }
    ok=1;
done:
    free(lut);
    avp_world_gpu_staging_dispose(&scratch);
    avp_vk_wall_projection_destroy(&wire);
    return ok;
}

static int avp_wsi_upload_projected_wall_geometry(AvpWsiScope *scope,
                                                   AvpVkWsiSmoke *out)
{
    if(scope->columns_mode==4)return avp_wsi_upload_mixed_wall_geometry(scope,out);
    AvpWorldScenePacket scene={0};
    AvpVkWallProjectionStream wire={0};
    AvpVkWallSlotPlan slots={0};
    const uint32_t w=scope->extent.width,h=scope->extent.height;
    AvpWorldGpuStaging scratch={0};
    int ok=0;
    scene.width=w;scene.height=h;
    scene.map_width=scene.map_height=1u;
    scene.hit_capacity_per_column=1u;
    scene.image_slot_count=AVP_WORLD_SCENE_IMAGE_SLOTS;
    scene.maze=calloc(8u,1u);
    scene.columns=calloc(w,sizeof(*scene.columns));
    scene.hits=calloc(w,sizeof(*scene.hits));
    if(!scene.maze || !scene.columns || !scene.hits)goto finish;
    scene.wall_image[1u]=5u;
    scene.image[5u]=(AvpWorldImageMeta){16u,16u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    if(scope->columns_mode==3){
        scene.wall_image[2u]=6u;
        scene.image[6u]=(AvpWorldImageMeta){16u,16u,AVP_WORLD_IMAGE_RGB8_ALPHA8};
    }
    for(uint32_t x=0;x<w;++x) {
        scene.columns[x]=(AvpWorldRayColumn){1.0,1u,1u,0u};
        scene.hits[x]=(AvpWorldRayHit){1.0,(double)(x%16u)/16.0,
                                      scope->columns_mode==3 && ((x/16u)&1u)?2u:1u,
                                      0u,1u,1u,0,(int32_t)h};
    }
    if(!avp_vk_wall_projection_pack(&wire,&scene) ||
       !avp_vk_wall_slots_collect(wire.data,wire.bytes,&slots))goto finish;
    if(scope->columns_mode==3) {
        /* The R337 two-slot test now exercises the ACTUAL R338->R339
         * source RGB -> packed integer staging -> GPU image-bank route.
         * Its colors deliberately match the independent R337 pixel oracle. */
        uint8_t rgb_a[16u*16u*3u],rgb_b[16u*16u*3u];
        const AvpGpuRgbImage a={16,16,rgb_a,NULL,NULL};
        const AvpGpuRgbImage b={16,16,rgb_b,NULL,NULL};
        const AvpGpuRgbImage *source[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
        if(slots.used_count!=2u || slots.order[0]!=5u || slots.order[1]!=6u)
            goto finish;
        for(uint32_t y=0u;y<16u;y++)for(uint32_t x=0u;x<16u;x++){
            const uint32_t c0=((x^y)&1u)?0x00336699u:0x00bb8855u;
            const uint32_t c1=((x^y)&1u)?0x00226644u:0x00ddaa66u;
            const size_t i=((size_t)y*16u+x)*3u;
            rgb_a[i]=(uint8_t)(c0>>16u);
            rgb_a[i+1u]=(uint8_t)(c0>>8u);
            rgb_a[i+2u]=(uint8_t)c0;
            rgb_b[i]=(uint8_t)(c1>>16u);
            rgb_b[i+1u]=(uint8_t)(c1>>8u);
            rgb_b[i+2u]=(uint8_t)c1;
        }
        source[5u]=&a;source[6u]=&b;
        /* Capture exactly the source image bytes and geometry submitted
         * to Vulkan, before the temporary RGB arrays and wire disappear. */
        if(!avp_vk_wall_oracle_capture_build(wire.data,wire.bytes,source,
                 NULL,1024u,2048u,&scope->wall_oracle) ||
           !avp_vk_wall_gpu_bank_build(scope->gpu,scope->device,scope->queue,
                 scope->queue_family,&slots,source,1024u,2048u,&scratch,
                 &scope->wall_gpu) ||
           !avp_vk_wall_draw_batch_build(wire.data,wire.bytes,
                 &scope->wall_gpu.bank,&scope->wall_draws))goto finish;
        out->textures_uploaded+=scope->wall_gpu.bank.count;
        out->texture_bytes_uploaded+=(uint32_t)scope->wall_gpu.bank.uploaded_bytes;
    }
    ok=avp_vk_geometry_upload(scope->gpu,scope->device,scope->queue,
          scope->queue_family,wire.data,wire.bytes,&scope->diagnostic_geometry);
    if(ok)out->geometry_bytes_uploaded=(uint32_t)wire.bytes;
finish:
    avp_world_gpu_staging_dispose(&scratch);
    avp_vk_wall_projection_destroy(&wire);
    free(scene.maze);free(scene.columns);free(scene.hits);
    if(!ok)avp_wsi_status(out,"Projected-wall GPU geometry upload failed");
    return ok;
}

/* R358: original windows/doors/portals + flats as resolved SOURCE TEXEL
 * addresses. Original binary transparency is tested on the CPU during the
 * visibility pass; the final GPU tape only refers to pixels with alpha=255.
 * GPU images contain underlying source colors, never pre-rendered frame RGB. */
static int avp_wsi_upload_live_layered_geometry(AvpWsiScope *scope,
                                                AvpVkWsiSmoke *out)
{
    AvpWorldGpuStaging staging={0};
    const AvpGpuRgbImage *sources[AVP_WORLD_SCENE_IMAGE_SLOTS]={0};
    int ok=0;
    if(!scope->live_scene||!scope->live_state||
       scope->live_scene->width!=scope->extent.width||
       scope->live_scene->height!=scope->extent.height){
        avp_wsi_status(out,"Layered scene dimensions mismatch");return 0;
    }
    if(!avp_vk_layered_frame_build(scope->live_scene,scope->live_state,
                                   &scope->layered_frame)||
       !scope->layered_frame.slots.used_count){
        avp_wsi_status(out,"R358 layered source unsupported (alpha/geometry/textures)");
        return 0;
    }
    for(uint32_t i=0u;i<scope->layered_frame.slots.used_count;i++){
        uint32_t slot=scope->layered_frame.slots.order[i];
        sources[slot]=&scope->layered_frame.upload_images[slot];
    }
    if(avp_vk_wall_gpu_bank_build(scope->gpu,scope->device,scope->queue,
           scope->queue_family,&scope->layered_frame.slots,sources,
           64u*1024u*1024u,512ull*1024u*1024u,&staging,&scope->wall_gpu)&&
       avp_vk_geometry_upload(scope->gpu,scope->device,scope->queue,
           scope->queue_family,scope->layered_frame.geometry.data,
           scope->layered_frame.geometry.bytes,&scope->diagnostic_geometry)){
        out->textures_uploaded+=scope->wall_gpu.bank.count;
        out->texture_bytes_uploaded+=(uint32_t)scope->wall_gpu.bank.uploaded_bytes;
        out->geometry_bytes_uploaded=(uint32_t)scope->layered_frame.geometry.bytes;
        ok=1;
    }
    avp_world_gpu_staging_dispose(&staging);
    if(!ok)avp_wsi_status(out,"R358 layered GPU source texture or geometry upload failed");
    return ok;
}

static int avp_wsi_upload_synthetic_geometry(AvpWsiScope *scope,
                                             AvpVkWsiSmoke *out)
{
    if(scope->columns_mode==7)
        return avp_wsi_upload_live_layered_geometry(scope,out);
    if(scope->columns_mode==5 || scope->columns_mode==6)
        return avp_wsi_upload_live_wall_geometry(scope,out);
    if(scope->columns_mode==2 || scope->columns_mode==3 || scope->columns_mode==4)
        return avp_wsi_upload_projected_wall_geometry(scope,out);
    if(scope->columns_mode==1){
        const size_t bytes=avp_vk_diag_ray_columns_size(
            scope->extent.width,scope->extent.height);
        uint8_t *wire=NULL;
        int ok=0;
        if(!bytes || bytes>32u*1024u*1024u){
            avp_wsi_status(out,"Ray-columns diagnostic dimensions exceed limits");
            return 0;
        }
        wire=malloc(bytes);
        if(!wire){avp_wsi_status(out,"Ray-columns diagnostic allocation failed");return 0;}
        if(avp_vk_diag_ray_columns_make(wire,bytes,scope->extent.width,
                 scope->extent.height))
            ok=avp_vk_geometry_upload(scope->gpu,scope->device,scope->queue,
                scope->queue_family,wire,bytes,&scope->diagnostic_geometry);
        free(wire);
        if(!ok){avp_wsi_status(out,"Ray-columns GPU geometry upload failed");return 0;}
        out->geometry_bytes_uploaded=(uint32_t)bytes;
        return 1;
    }
    uint8_t geom[96]={0};
    avp_geom_le32(geom,0x57525641u); /* R331 AVRW header */
    avp_geom_le32(geom+4,1u);
    avp_geom_le32(geom+8,1u); /* width */
    avp_geom_le32(geom+12,1u); /* height */
    avp_geom_le32(geom+16,1u); /* hit count */
    avp_geom_le32(geom+20,32u); /* column table offset */
    avp_geom_le32(geom+24,56u); /* hit table offset */
    avp_geom_le32(geom+36,1u); /* column hit count */
    avp_geom_le_double(geom+40,1.0); /* depth */
    avp_geom_le32(geom+48,1u); /* ready */
    avp_geom_le_double(geom+56,1.0); /* hit distance */
    avp_geom_le_double(geom+64,0.5); /* texture position */
    avp_geom_le32(geom+72,1u); /* wall ID */
    avp_geom_le32(geom+84,1u); /* final hit */
    if(!avp_vk_geometry_upload(scope->gpu,scope->device,scope->queue,
       scope->queue_family,geom,sizeof(geom),&scope->diagnostic_geometry)){
        avp_wsi_status(out,"Vulkan GPU geometry storage upload failed");
        return 0;
    }
    out->geometry_bytes_uploaded=(uint32_t)sizeof(geom);
    return 1;
}

static int avp_wsi_initialize(AvpWsiScope *scope,
                     void *x11_display, uintptr_t x11_window,
                     uint32_t requested_width, uint32_t requested_height,
                     const char *gpu_key, AvpVkWsiSmoke *out)
{
    VkApplicationInfo app = {0};
    VkInstanceCreateInfo instance = {0};
    VkXlibSurfaceCreateInfoKHR surface = {0};
    VkDeviceQueueCreateInfo queue = {0};
    VkDeviceCreateInfo device = {0};
    float priority = 1.0f;
    const char *instance_extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME };
    const char *device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkResult r;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!x11_display || !x11_window || !requested_width || !requested_height) {
        avp_wsi_status(out, "Invalid X11 display/window or dimensions"); return 0;
    }
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "AVP Jaguar Vulkan diagnostic";
    app.apiVersion = VK_API_VERSION_1_0;
    instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance.pApplicationInfo = &app;
    instance.enabledExtensionCount = 2u;
    instance.ppEnabledExtensionNames = instance_extensions;
    if(!scope->instance){
        r = vkCreateInstance(&instance, NULL, &scope->instance);
        if (r != VK_SUCCESS) { avp_wsi_status(out, "Vulkan instance unavailable: %d", (int)r); return 0; }
    }
    out->instance_created = 1;
    surface.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surface.dpy = (Display *)x11_display;
    surface.window = (Window)x11_window;
    r = vkCreateXlibSurfaceKHR(scope->instance, &surface, NULL, &scope->surface);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Xlib surface creation failed: %d", (int)r); return 0; }
    out->surface_created = 1;
    if (!avp_wsi_pick(scope, gpu_key, out)) return 0;
    queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue.queueFamilyIndex = scope->queue_family;
    queue.queueCount = 1u;
    queue.pQueuePriorities = &priority;
    device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device.queueCreateInfoCount = 1u;
    device.pQueueCreateInfos = &queue;
    device.enabledExtensionCount = 1u;
    device.ppEnabledExtensionNames = device_extensions;
    r = vkCreateDevice(scope->gpu, &device, NULL, &scope->device);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Logical device creation failed: %d", (int)r); return 0; }
    out->device_created = 1;
    vkGetDeviceQueue(scope->device, scope->queue_family, 0u, &scope->queue);
    if (!scope->queue) { avp_wsi_status(out, "GPU command queue unavailable"); return 0; }
    if (!avp_wsi_make_swapchain(scope, requested_width, requested_height, out)) return 0;
    /* R359: a game-owned graphics-capable WSI context uses the *real game
     * window* and retains instance/device/swapchain across frames. It must
     * NOT allocate synthetic diagnostic images or pretend to present gameplay
     * while the layered world + native overlays are not fully composited. */
    if (scope->columns_mode == 8) return 1;
    /* A genuine buffer->GPU-image transfer, made from our own synthetic
     * checkerboard (never the game's completed Software framebuffer). This
     * diagnostic image isn't displayed until future shader work is added. */
    if(scope->columns_mode!=3 && scope->columns_mode!=4 &&
       scope->columns_mode!=5 && scope->columns_mode!=6 &&
       scope->columns_mode!=7){
        uint32_t checker[16u*16u];
        for (uint32_t y=0;y<16u;y++) for (uint32_t x=0;x<16u;x++)
            checker[y*16u+x]=0xff000000u | ((x^y)&1u?0x00336699u:0x00bb8855u);
        if (!avp_vk_upload_r32uint(scope->gpu,scope->device,scope->queue,
                scope->queue_family,checker,16u,16u,sizeof(checker),
                &scope->diagnostic_texture)) {
            avp_wsi_status(out,"R32_UINT GPU image upload failed");return 0;
        }
        out->textures_uploaded=1u;
        out->texture_bytes_uploaded=(uint32_t)sizeof(checker);
    }
    /* R340 mode 3 now uploads both textures through the all-or-none
     * real source-image bank after building its validated scene packet. */
    /* R326 diagnostic-only lookup data. The exact Jaguar CRY16 LUT builder
     * belongs to the gameplay scene-transfer layer; do not introduce any
     * ROM dependency into this independent WSI bring-up. */
    {
        const size_t count=256u*256u;
        uint32_t *words=malloc(count*sizeof(*words));
        if (!words) { avp_wsi_status(out,"No memory for diagnostic color lookup");return 0; }
        if(scope->columns_mode==4 || scope->columns_mode==5 ||
           scope->columns_mode==6 || scope->columns_mode==7){
            if(!avp_world_gpu_build_cry_lut(words,count)){
                free(words);
                avp_wsi_status(out,"Original CRY conversion LUT unavailable");
                return 0;
            }
        }else for (size_t i=0;i<count;i++) {
            uint32_t y=(uint32_t)i&255u, chroma=(uint32_t)i>>8u;
            words[i]=(y<<16u)|(chroma<<8u)|y;
        }
        int ok=avp_vk_upload_r32uint(scope->gpu,scope->device,scope->queue,
                scope->queue_family,words,256u,256u,count*sizeof(*words),
                &scope->diagnostic_lut);
        free(words);
        if (!ok) { avp_wsi_status(out,"R32_UINT lookup upload failed");return 0; }
        out->textures_uploaded++;
        out->texture_bytes_uploaded+=(uint32_t)(count*sizeof(uint32_t));
    }
    if (!avp_wsi_upload_synthetic_geometry(scope,out)) return 0;
    if(scope->columns_mode==3 || scope->columns_mode==4 ||
       scope->columns_mode==5 || scope->columns_mode==6){
        /* Bind the committed R339 GPU image for EVERY referenced slot.
         * A failed descriptor leaves no usable draw path; teardown frees all
         * allocations after the GPU has completed its work. */
        if(scope->wall_draws.count!=scope->wall_gpu.bank.count)return 0;
        for(uint32_t i=0u;i<scope->wall_draws.count;i++){
            const uint32_t slot=scope->wall_draws.draws[i].slot;
            const AvpVkUploadedR32 *image=
                avp_vk_wall_gpu_bank_image(&scope->wall_gpu,slot);
            if(!image || !avp_vk_sampled_scene_create(scope->device,image,
                      &scope->diagnostic_lut,&scope->diagnostic_geometry,
                      &scope->wall_sets[slot])){
                avp_wsi_status(out,"R340 wall source descriptor binding failed");
                return 0;
            }
        }
    }else if(scope->columns_mode!=7 && !avp_vk_sampled_scene_create(scope->device,
                &scope->diagnostic_texture,&scope->diagnostic_lut,
                &scope->diagnostic_geometry,&scope->diagnostic_descriptors)) {
        avp_wsi_status(out,"R32_UINT image and ray-buffer descriptor binding failed");
        return 0;
    }
    if(scope->columns_mode==7){
        /* One AVFL source visibility tape; one descriptor per visible
         * original/hires source texture. Order-independent because every
         * pixel references exactly ONE source slot after alpha resolution. */
        for(uint32_t i=0u;i<scope->layered_frame.slots.used_count;i++){
            uint32_t slot=scope->layered_frame.slots.order[i];
            const AvpVkUploadedR32 *image=
                avp_vk_wall_gpu_bank_image(&scope->wall_gpu,slot);
            if(!image || !avp_vk_sampled_scene_create(scope->device,image,
                 &scope->diagnostic_lut,&scope->diagnostic_geometry,
                 &scope->wall_sets[slot])){
                avp_wsi_status(out,"R358 layered source descriptor binding failed");
                return 0;
            }
        }
    }
    if(scope->columns_mode==6){
        /* Bind the EXACT same R32_UINT source format and Jaguar CRY LUT as
         * the tested walls, but use a separate AVFL storage buffer. */
        for(uint32_t i=0u;i<scope->flat_frame.slots.used_count;i++){
            const uint32_t slot=scope->flat_frame.slots.order[i];
            const AvpVkUploadedR32 *image=
                avp_vk_wall_gpu_bank_image(&scope->flat_gpu,slot);
            if(!image || !avp_vk_sampled_scene_create(scope->device,image,
                 &scope->diagnostic_lut,&scope->flat_geometry,
                 &scope->flat_sets[slot])){
                avp_wsi_status(out,"R356 flat source descriptor binding failed");
                return 0;
            }
        }
    }
    return 1;
}


/* R356: exact GPU readback of floors/ceilings with original opaque walls
 * covering them. The independent source-backed CPU oracle owns no rendered
 * framebuffer data and has the same swapchain format and dimensions. */
static int avp_vk_readback_verify_flat_wall_composite(VkDevice device,
    const AvpVkReadback *rb,const AvpVkFlatFrame *flat,
    const AvpVkWallOracleCapture *walls,uint32_t *mismatches)
{
    void *mapped=NULL;
    int ok;
    if(mismatches)*mismatches=UINT32_MAX;
    if(!device||!rb||!rb->memory||!flat||!walls||!mismatches ||
       (uint64_t)rb->width*rb->height*4u!=rb->bytes)return 0;
    if(vkMapMemory(device,rb->memory,0u,rb->bytes,0u,&mapped)!=VK_SUCCESS)
        return 0;
    ok=avp_vk_flat_frame_compare_composite(flat,walls,(uint32_t)rb->format,
        (const uint8_t*)mapped,(size_t)rb->bytes,mismatches);
    vkUnmapMemory(device,rb->memory);
    return ok;
}

/* R358: independent source-layer comparator mapped from real GPU output. */
static int avp_vk_readback_verify_layered_frame(VkDevice device,
    const AvpVkReadback *rb,const AvpVkLayeredFrame *frame,
    uint32_t *mismatches)
{
    void *mapped=NULL;
    int ok;
    if(mismatches)*mismatches=UINT32_MAX;
    if(!device||!rb||!rb->memory||!frame||!mismatches||
       (uint64_t)rb->width*rb->height*4u!=rb->bytes)return 0;
    if(vkMapMemory(device,rb->memory,0u,rb->bytes,0u,&mapped)!=VK_SUCCESS)
        return 0;
    ok=avp_vk_layered_frame_compare(frame,(uint32_t)rb->format,
                (const uint8_t *)mapped,(size_t)rb->bytes,mismatches);
    vkUnmapMemory(device,rb->memory);
    return ok;
}

/* RE46 R327, opt-in one-frame graphics-shader test. Uses synthetic
 * GPU-uploaded source and LUT, never game pixels. A successful present is
 * NOT evidence of Jaguar pixel parity, full-screen correctness or FPS. */
static int avp_wsi_shader_once(AvpWsiScope *s, const char *vs_path,
                                const char *fs_path, AvpVkWsiSmoke *out)
{
    uint32_t *vs=NULL,*fs=NULL,*flat_fs=NULL;
    size_t vs_bytes=0u,fs_bytes=0u,flat_fs_bytes=0u;
    VkAttachmentDescription attachment={0};
    VkAttachmentReference reference={0};
    VkSubpassDescription subpass={0};
    VkSubpassDependency dependencies[2]={{0}};
    VkRenderPassCreateInfo pass={0};
    VkImageViewCreateInfo view={0};
    VkFramebufferCreateInfo framebuffer={0};
    VkCommandPoolCreateInfo pool={0};
    VkCommandBufferAllocateInfo alloc={0};
    VkSemaphoreCreateInfo sem={0};
    VkCommandBufferBeginInfo begin={0};
    VkRenderPassBeginInfo render={0};
    VkClearValue clear={0};
    VkSubmitInfo submit={0};
    VkPresentInfoKHR present={0};
    VkCommandBuffer cmd=VK_NULL_HANDLE;
    VkPipelineStageFlags stage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    AvpVkSamplingPush parameters={2u,0u,0u,0};
    VkFenceCreateInfo fence_info={0};
    VkResult r;
    uint32_t index=UINT32_MAX;
    uint32_t mismatches=UINT32_MAX;
    int submitted=0, completed=0, ok=0;
    if(s->columns_mode==2 || s->columns_mode==3)parameters.wx=5u;
    if (!avp_spirv_read_file(vs_path,AVP_SPIRV_VERTEX,&vs,&vs_bytes) ||
        !avp_spirv_read_file(fs_path,AVP_SPIRV_FRAGMENT,&fs,&fs_bytes)) {
        avp_wsi_status(out,"Shader files missing or invalid SPIR-V stage");goto finish;
    }
    attachment.format=s->swapchain_format;
    attachment.samples=VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    reference.attachment=0u;
    reference.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount=1u;
    subpass.pColorAttachments=&reference;
    dependencies[0].srcSubpass=VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass=0u;
    dependencies[0].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    /* GPU color writes must be visible to vkCmdCopyImageToBuffer after
     * the render pass ends and transitions into TRANSFER_SRC_OPTIMAL. */
    dependencies[1].srcSubpass=0u;
    dependencies[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependencies[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass.attachmentCount=1u;
    pass.pAttachments=&attachment;
    pass.subpassCount=1u;
    pass.pSubpasses=&subpass;
    pass.dependencyCount=2u;
    pass.pDependencies=dependencies;
    if (vkCreateRenderPass(s->device,&pass,NULL,&s->shader_renderpass)!=VK_SUCCESS) {
        avp_wsi_status(out,"Create shader render pass failed");goto finish;
    }
    const AvpVkSampledPair *pipeline_source=&s->diagnostic_descriptors;
    if(s->columns_mode==7){
        if(!s->layered_frame.slots.used_count){
            avp_wsi_status(out,"Empty R358 layered scene draw source");goto finish;
        }
        pipeline_source=&s->wall_sets[s->layered_frame.slots.order[0]];
    }else if(s->columns_mode==3 || s->columns_mode==4 ||
             s->columns_mode==5 || s->columns_mode==6){
        if(!s->wall_draws.count){
            avp_wsi_status(out,"Empty R340 draw batch");goto finish;
        }
        pipeline_source=&s->wall_sets[s->wall_draws.draws[0].slot];
    }
    if (!avp_vk_opaque_pipeline_create(s->device,pipeline_source,
            s->shader_renderpass,s->extent,vs,vs_bytes,fs,fs_bytes,
            &s->shader_pipeline)) {
        avp_wsi_status(out,"Create sampled-image graphics pipeline failed");goto finish;
    }
    if(s->columns_mode==6){
        const uint32_t first=s->flat_frame.slots.order[0];
        if(!s->flat_fragment_spv ||
           !avp_spirv_read_file(s->flat_fragment_spv,AVP_SPIRV_FRAGMENT,
                                &flat_fs,&flat_fs_bytes) ||
           !avp_vk_opaque_pipeline_create(s->device,&s->flat_sets[first],
               s->shader_renderpass,s->extent,vs,vs_bytes,
               flat_fs,flat_fs_bytes,&s->flat_pipeline)){
            avp_wsi_status(out,"Create source-backed floor/ceiling GPU pipeline failed");
            goto finish;
        }
    }
    if (!avp_vk_readback_create(s->gpu,s->device,s->extent.width,
             s->extent.height,s->swapchain_format,&s->shader_readback)) {
        avp_wsi_status(out,"Create shader GPU readback buffer failed");goto finish;
    }
    pool.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.queueFamilyIndex=s->queue_family;
    if (vkCreateCommandPool(s->device,&pool,NULL,&s->commands)!=VK_SUCCESS) {
        avp_wsi_status(out,"Create shader command pool failed");goto finish;
    }
    sem.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(s->device,&sem,NULL,&s->acquired)!=VK_SUCCESS ||
        vkCreateSemaphore(s->device,&sem,NULL,&s->finished)!=VK_SUCCESS) {
        avp_wsi_status(out,"Create shader semaphores failed");goto finish;
    }
    r=vkAcquireNextImageKHR(s->device,s->swapchain,UINT64_MAX,s->acquired,
                            VK_NULL_HANDLE,&index);
    if ((r!=VK_SUCCESS && r!=VK_SUBOPTIMAL_KHR) || index>=s->image_count) {
        avp_wsi_status(out,"Acquire shader swapchain image failed: %d",(int)r);goto finish;
    }
    view.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view.image=s->images[index];
    view.viewType=VK_IMAGE_VIEW_TYPE_2D;
    view.format=s->swapchain_format;
    view.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    view.subresourceRange.levelCount=1u;
    view.subresourceRange.layerCount=1u;
    if (vkCreateImageView(s->device,&view,NULL,&s->shader_output_view)!=VK_SUCCESS) {
        avp_wsi_status(out,"Create shader swapchain image view failed");goto finish;
    }
    framebuffer.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer.renderPass=s->shader_renderpass;
    framebuffer.attachmentCount=1u;
    framebuffer.pAttachments=&s->shader_output_view;
    framebuffer.width=s->extent.width;
    framebuffer.height=s->extent.height;
    framebuffer.layers=1u;
    if (vkCreateFramebuffer(s->device,&framebuffer,NULL,&s->shader_framebuffer)!=VK_SUCCESS) {
        avp_wsi_status(out,"Create shader framebuffer failed");goto finish;
    }
    alloc.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool=s->commands;
    alloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount=1u;
    if (vkAllocateCommandBuffers(s->device,&alloc,&cmd)!=VK_SUCCESS) {
        avp_wsi_status(out,"Allocate shader command buffer failed");goto finish;
    }
    begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd,&begin)!=VK_SUCCESS) {
        avp_wsi_status(out,"Begin shader command buffer failed");goto finish;
    }
    /* Standalone diagnostic uses its one-shot framebuffer.  Gameplay's
     * foreground stream must not be recorded into this command buffer. */
    clear.color.float32[3]=1.0f;
    render.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render.renderPass=s->shader_renderpass;
    render.framebuffer=s->shader_framebuffer;
    render.renderArea.extent=s->extent;
    render.clearValueCount=1u;
    render.pClearValues=&clear;
    vkCmdBeginRenderPass(cmd,&render,VK_SUBPASS_CONTENTS_INLINE);
    if(s->columns_mode==6){
        /* First draw floor/ceiling source texels; missing/horizon texels
         * discard. Fully opaque walls subsequently cover the same pixels. */
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,s->flat_pipeline.pipeline);
        for(uint32_t i=0u;i<s->flat_frame.slots.used_count;i++){
            uint32_t slot=s->flat_frame.slots.order[i];
            parameters.encoding=s->flat_frame.slots.slots[slot].encoding;
            parameters.wx=slot;
            vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
                s->flat_pipeline.layout,0u,1u,&s->flat_sets[slot].set,0u,NULL);
            vkCmdPushConstants(cmd,s->flat_pipeline.layout,VK_SHADER_STAGE_FRAGMENT_BIT,
                0u,(uint32_t)sizeof(parameters),&parameters);
            vkCmdDraw(cmd,3u,1u,0u,0u);
        }
    }
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,s->shader_pipeline.pipeline);
    if(s->columns_mode==7){
        for(uint32_t i=0u;i<s->layered_frame.slots.used_count;i++){
            uint32_t slot=s->layered_frame.slots.order[i];
            parameters.encoding=s->layered_frame.slots.slots[slot].encoding;
            parameters.wx=slot;
            vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
                s->shader_pipeline.layout,0u,1u,&s->wall_sets[slot].set,0u,NULL);
            vkCmdPushConstants(cmd,s->shader_pipeline.layout,VK_SHADER_STAGE_FRAGMENT_BIT,
                0u,(uint32_t)sizeof(parameters),&parameters);
            vkCmdDraw(cmd,3u,1u,0u,0u);
        }
    }else if(s->columns_mode==3 || s->columns_mode==4 ||
       (s->columns_mode==5 || s->columns_mode==6)){
        /* Draw every slot from the validated R340 batch. Descriptor/image
         * lifetime stays immutable until the submitted GPU work completes. */
        for(uint32_t i=0u;i<s->wall_draws.count;i++){
            const AvpVkWallDraw *draw=&s->wall_draws.draws[i];
            parameters.encoding=draw->encoding;
            parameters.wx=draw->slot;
            vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
                s->shader_pipeline.layout,0u,1u,
                &s->wall_sets[draw->slot].set,0u,NULL);
            vkCmdPushConstants(cmd,s->shader_pipeline.layout,VK_SHADER_STAGE_FRAGMENT_BIT,
                0u,(uint32_t)sizeof(parameters),&parameters);
            vkCmdDraw(cmd,3u,1u,0u,0u);
        }
    }else{
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
            s->shader_pipeline.layout,0u,1u,&s->diagnostic_descriptors.set,0u,NULL);
        vkCmdPushConstants(cmd,s->shader_pipeline.layout,VK_SHADER_STAGE_FRAGMENT_BIT,
                           0u,(uint32_t)sizeof(parameters),&parameters);
        vkCmdDraw(cmd,3u,1u,0u,0u);
    }
    vkCmdEndRenderPass(cmd);
    /* This is a genuine image-to-host-buffer GPU copy, NOT a CPU framebuffer
     * upload. The image is restored to PRESENT_SRC before presentation. */
    avp_vk_readback_record(cmd,s->images[index],&s->shader_readback);
    if (vkEndCommandBuffer(cmd)!=VK_SUCCESS) {
        avp_wsi_status(out,"End shader command buffer failed");goto finish;
    }
    submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount=1u;
    submit.pWaitSemaphores=&s->acquired;
    submit.pWaitDstStageMask=&stage;
    submit.commandBufferCount=1u;
    submit.pCommandBuffers=&cmd;
    submit.signalSemaphoreCount=1u;
    submit.pSignalSemaphores=&s->finished;
    fence_info.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(s->device,&fence_info,NULL,&s->frame_fence)!=VK_SUCCESS) {
        avp_wsi_status(out,"Shader validation fence creation failed");goto finish;
    }
    r=vkQueueSubmit(s->queue,1u,&submit,s->frame_fence);
    if (r!=VK_SUCCESS) {avp_wsi_status(out,"Shader GPU submit failed: %d",(int)r);goto finish;}
    submitted=1;
    out->command_submitted=1;
    /* Only inspect mapped GPU memory after this submission is complete. */
    r=vkWaitForFences(s->device,1u,&s->frame_fence,VK_TRUE,UINT64_MAX);
    if (r!=VK_SUCCESS) {avp_wsi_status(out,"Shader GPU completion fence failed: %d",(int)r);goto finish;}
    completed=1;
    if (!(s->columns_mode==7 ?
          avp_vk_readback_verify_layered_frame(s->device,&s->shader_readback,
              &s->layered_frame,&mismatches) :
          s->columns_mode==6 ?
          avp_vk_readback_verify_flat_wall_composite(s->device,&s->shader_readback,
              &s->flat_frame,&s->wall_oracle,&mismatches) :
          (s->columns_mode==3 || s->columns_mode==4 || s->columns_mode==5) ?
          avp_vk_readback_verify_wall_capture(s->device,&s->shader_readback,
              &s->wall_oracle,&mismatches) :
          s->columns_mode==2 ?
          avp_vk_readback_verify_projected_wall(s->device,&s->shader_readback,&mismatches) :
          s->columns_mode==1 ?
          avp_vk_readback_verify_ray_columns(s->device,&s->shader_readback,&mismatches) :
          avp_vk_readback_verify(s->device,&s->shader_readback,&mismatches))) {
        if (mismatches==UINT32_MAX)
            avp_wsi_status(out,"Shader GPU output mapping failed");
        else
            avp_wsi_status(out,"Shader GPU pixel parity failed: mismatched=%u",mismatches);
        goto finish;
    }
    if(s->columns_mode==3){
        /* Retain the older independent fixture oracle as an additional
         * regression: both the source-derived and legacy references must
         * match the actual GPU readback before this diagnostic is a pass. */
        uint32_t old_mismatches=UINT32_MAX;
        if(!avp_vk_readback_verify_two_slot_wall(s->device,
                    &s->shader_readback,&old_mismatches)){
            avp_wsi_status(out,"Legacy two-slot GPU reference failed: mismatched=%u",
                           old_mismatches);
            goto finish;
        }
    }
    out->frames_submitted=1u;
    present.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount=1u;
    present.pWaitSemaphores=&s->finished;
    present.swapchainCount=1u;
    present.pSwapchains=&s->swapchain;
    present.pImageIndices=&index;
    r=vkQueuePresentKHR(s->queue,&present);
    if (r!=VK_SUCCESS && r!=VK_SUBOPTIMAL_KHR) {
        avp_wsi_status(out,"Shader presentation failed: %d",(int)r);goto finish;
    }
    out->presented=1;
    out->frames_presented=1u;
    if(s->columns_mode==7)
        avp_wsi_status(out,"Original live doors/windows/portals + floors GPU frame matched source-backed oracle (%ux%u)",
                       s->extent.width,s->extent.height);
    else if(s->columns_mode==6)
        avp_wsi_status(out,"Original live floor/ceiling + opaque wall GPU frame matched composite source-backed oracle (%ux%u)",
                       s->extent.width,s->extent.height);
    else if(s->columns_mode==5)
        avp_wsi_status(out,"Original live opaque wall GPU frame matched source-backed oracle (%ux%u)",
                       s->extent.width,s->extent.height);
    else if(s->columns_mode==4)
        avp_wsi_status(out,"GPU mixed Jaguar CRY + high-res RGB exact-row wall matched source-backed oracle (%ux%u)",
                       s->extent.width,s->extent.height);
    else if(s->columns_mode==3)
        avp_wsi_status(out,"GPU two-source wall readback matched BOTH source-backed and legacy oracles (%ux%u)",
                       s->extent.width,s->extent.height);
    else if(s->columns_mode==2)
        avp_wsi_status(out,"GPU projected solid-wall source sampling matched CPU oracle (%ux%u)",
                       s->extent.width,s->extent.height);
    else if(s->columns_mode==1)
        avp_wsi_status(out,"GPU ray-column indexing + texture sampling matched CPU oracle (%ux%u)",
                       s->extent.width,s->extent.height);
    else
        avp_wsi_status(out,"GPU integer shader verified against complete CPU checkerboard (%ux%u)",
                       s->extent.width,s->extent.height);
    ok=1;
finish:
    /* A failed wait is not permission to free resources still in flight. */
    if (submitted && !completed) (void)vkDeviceWaitIdle(s->device);
    free(vs);
    free(fs);
    free(flat_fs);
    return ok;
}

int avp_vk_wsi_shader_smoke(void *x11_display, uintptr_t x11_window,
                           uint32_t requested_width, uint32_t requested_height,
                           const char *gpu_key, const char *vertex_spv,
                           const char *fragment_spv, AvpVkWsiSmoke *out)
{
    AvpWsiScope scope={0};
    int ok;
    if (!out) return 0;
    scope.shader_requested=1;
    ok=avp_wsi_initialize(&scope,x11_display,x11_window,requested_width,
                          requested_height,gpu_key,out);
    if (ok) ok=avp_wsi_shader_once(&scope,vertex_spv,fragment_spv,out);
    avp_wsi_destroy(&scope);
    return ok;
}

/* R335 diagnostic: real Vulkan draw reads an entire synthetic ray stream,
 * rather than checking only one fixed ray. Still NOT gameplay rendering. */
int avp_vk_wsi_ray_columns_smoke(void *x11_display, uintptr_t x11_window,
                            uint32_t requested_width, uint32_t requested_height,
                            const char *gpu_key, const char *vertex_spv,
                            const char *fragment_spv, AvpVkWsiSmoke *out)
{
    AvpWsiScope scope={0};
    int ok;
    if(!out)return 0;
    scope.shader_requested=1;
    scope.columns_mode=1;
    ok=avp_wsi_initialize(&scope,x11_display,x11_window,requested_width,
                          requested_height,gpu_key,out);
    if(ok)ok=avp_wsi_shader_once(&scope,vertex_spv,fragment_spv,out);
    avp_wsi_destroy(&scope);
    return ok;
}

/* R336: original projection math + GPU source sampling, still diagnostic-only. */
int avp_vk_wsi_projected_wall_smoke(void *x11_display, uintptr_t x11_window,
                            uint32_t requested_width, uint32_t requested_height,
                            const char *gpu_key, const char *vertex_spv,
                            const char *fragment_spv, AvpVkWsiSmoke *out)
{
    AvpWsiScope scope={0};
    int ok;
    if(!out)return 0;
    scope.shader_requested=1;
    scope.columns_mode=2;
    ok=avp_wsi_initialize(&scope,x11_display,x11_window,requested_width,
                          requested_height,gpu_key,out);
    if(ok)ok=avp_wsi_shader_once(&scope,vertex_spv,fragment_spv,out);
    avp_wsi_destroy(&scope);
    return ok;
}

/* R337: two image uploads, two compatible descriptor sets and two separate
 * fragment passes over one original-projection geometry buffer. Still
 * synthetic: no game texture extraction or production Hardware setting. */
int avp_vk_wsi_two_slot_wall_smoke(void *x11_display, uintptr_t x11_window,
                            uint32_t requested_width, uint32_t requested_height,
                            const char *gpu_key, const char *vertex_spv,
                            const char *fragment_spv, AvpVkWsiSmoke *out)
{
    AvpWsiScope scope={0};
    int ok;
    if(!out)return 0;
    scope.shader_requested=1;
    scope.columns_mode=3;
    ok=avp_wsi_initialize(&scope,x11_display,x11_window,requested_width,
                          requested_height,gpu_key,out);
    if(ok)ok=avp_wsi_shader_once(&scope,vertex_spv,fragment_spv,out);
    avp_wsi_destroy(&scope);
    return ok;
}

/* R348: two actual encoding types, original CRY LUT and exact hi-res rows.
 * Still a diagnostic, not the active Hardware renderer. */
int avp_vk_wsi_mixed_cry_rgb_wall_smoke(void *x11_display, uintptr_t x11_window,
                            uint32_t requested_width, uint32_t requested_height,
                            const char *gpu_key, const char *vertex_spv,
                            const char *fragment_spv, AvpVkWsiSmoke *out)
{
    AvpWsiScope scope={0};
    int ok;
    if(!out)return 0;
    scope.shader_requested=1;
    scope.columns_mode=4;
    ok=avp_wsi_initialize(&scope,x11_display,x11_window,requested_width,
                          requested_height,gpu_key,out);
    if(ok)ok=avp_wsi_shader_once(&scope,vertex_spv,fragment_spv,out);
    avp_wsi_destroy(&scope);
    return ok;
}

/* R350: first direct original live-ray-frame + original/hi-res source-input
 * Vulkan draw. Only plain opaque walls are supported and it is NOT the
 * production Hardware backend. Caller owns the scene/state during this
 * synchronous one-frame diagnostic. No synthetic rays or game framebuffer. */
int avp_vk_wsi_live_wall_frame(void *x11_display, uintptr_t x11_window,
    const AvpWorldScenePacket *scene,const AvpAvpGpuState *state,
    const char *gpu_key,const char *vertex_spv,const char *fragment_spv,
    AvpVkWsiSmoke *out)
{
    AvpWsiScope scope={0};
    int ok;
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    if(!scene || !state || !scene->width || !scene->height ||
       !vertex_spv || !fragment_spv){
        avp_wsi_status(out,"Missing live scene, texture state or shaders");
        return 0;
    }
    scope.shader_requested=1;
    scope.columns_mode=5;
    scope.live_scene=scene;
    scope.live_state=state;
    ok=avp_wsi_initialize(&scope,x11_display,x11_window,scene->width,
                          scene->height,gpu_key,out);
    if(ok)ok=avp_wsi_shader_once(&scope,vertex_spv,fragment_spv,out);
    avp_wsi_destroy(&scope);
    return ok;
}

/* R353: the game may send one real wall-ray frame to a private X11 Vulkan
 * test window. Ownership is explicit so Xlib's Vulkan ICD close-display
 * hooks execute while the VkInstance is still alive (verified in R352).
 * The main game Display/Window are never touched or reparented. */
/* R356: opt-in wall + floor + ceiling REAL SOURCE TEXEL Vulkan pass.
 * Private X11 window is owned here and always closed BEFORE instance unload. */
/* R358: opt-in live original MAZE2 doors, windows, portals and flats,
 * passed as source-coordinate tape. This is still a ONE-frame diagnostic;
 * gameplay/presentation uses Software and unsupported alpha fails closed. */
int avp_vk_wsi_run_owned_xlib_live_layered(void *x11_display,
    uintptr_t x11_window,const AvpWorldScenePacket *scene,
    const AvpAvpGpuState *state,const char *gpu_key,
    const char *vertex_spv,const char *layered_spv,AvpVkWsiSmoke *out)
{
    AvpWsiScope scope={0};
    int ok=0;
    scope.owned_x11_display=(Display *)x11_display;
    scope.owned_x11_window=(Window)x11_window;
    if(!out){avp_wsi_destroy(&scope);return 0;}
    memset(out,0,sizeof(*out));
    if(!x11_display||!x11_window||!scene||!state||
       !scene->width||!scene->height||!vertex_spv||!layered_spv){
        avp_wsi_status(out,"Missing R358 X11 window, scene or shader");
    }else{
        scope.shader_requested=1;
        scope.columns_mode=7;
        scope.live_scene=scene;
        scope.live_state=state;
        ok=avp_wsi_initialize(&scope,x11_display,x11_window,
                              scene->width,scene->height,gpu_key,out);
        if(ok)ok=avp_wsi_shader_once(&scope,vertex_spv,layered_spv,out);
    }
    avp_wsi_destroy(&scope);
    return ok;
}

int avp_vk_wsi_run_owned_xlib_live_wall_flat(void *x11_display,
    uintptr_t x11_window,const AvpWorldScenePacket *scene,
    const AvpAvpGpuState *state,const char *gpu_key,
    const char *vertex_spv,const char *wall_spv,const char *flat_spv,
    AvpVkWsiSmoke *out)
{
    AvpWsiScope scope={0};
    int ok=0;
    scope.owned_x11_display=(Display *)x11_display;
    scope.owned_x11_window=(Window)x11_window;
    if(!out){avp_wsi_destroy(&scope);return 0;}
    memset(out,0,sizeof(*out));
    if(!x11_display||!x11_window||!scene||!state||
       !scene->width||!scene->height||!vertex_spv||!wall_spv||!flat_spv){
        avp_wsi_status(out,"Missing X11 window, source packet or two SPIR-V shaders");
    }else{
        scope.shader_requested=1;
        scope.columns_mode=6;
        scope.live_scene=scene;
        scope.live_state=state;
        scope.flat_fragment_spv=flat_spv;
        ok=avp_wsi_initialize(&scope,x11_display,x11_window,
                              scene->width,scene->height,gpu_key,out);
        if(ok)ok=avp_wsi_shader_once(&scope,vertex_spv,wall_spv,out);
    }
    avp_wsi_destroy(&scope);
    return ok;
}

int avp_vk_wsi_run_owned_xlib_live_wall(void *x11_display, uintptr_t x11_window,
    const AvpWorldScenePacket *scene, const AvpAvpGpuState *state,
    const char *gpu_key, const char *vertex_spv, const char *fragment_spv,
    AvpVkWsiSmoke *out)
{
    AvpWsiScope scope={0};
    int ok=0;
    scope.owned_x11_display=(Display *)x11_display;
    scope.owned_x11_window=(Window)x11_window;
    if(!out){
        avp_wsi_destroy(&scope);
        return 0;
    }
    memset(out,0,sizeof(*out));
    if(!x11_display || !x11_window || !scene || !state ||
       !scene->width || !scene->height || !vertex_spv || !fragment_spv){
        avp_wsi_status(out,"Missing owned X11 window, live scene or SPIR-V");
    }else{
        scope.shader_requested=1;
        scope.columns_mode=5;
        scope.live_scene=scene;
        scope.live_state=state;
        ok=avp_wsi_initialize(&scope,x11_display,x11_window,
                              scene->width,scene->height,gpu_key,out);
        if(ok)ok=avp_wsi_shader_once(&scope,vertex_spv,fragment_spv,out);
    }
    avp_wsi_destroy(&scope);
    return ok;
}

/* R322: keep the R321 one-shot diagnostic available for comparison. */
int avp_vk_wsi_smoke(void *x11_display, uintptr_t x11_window,
                     uint32_t requested_width, uint32_t requested_height,
                     const char *gpu_key, AvpVkWsiSmoke *out)
{
    AvpWsiScope scope = {0};
    int ok = avp_wsi_initialize(&scope, x11_display, x11_window,
                                 requested_width, requested_height, gpu_key, out);
    if (ok) ok = avp_wsi_clear_once(&scope, out);
    avp_wsi_destroy(&scope);
    return ok;
}

/* Swapchain-only resources are not shared across generations. The device and
 * command pool remain alive. The diagnostic uses vkDeviceWaitIdle during
 * recreation: deliberately simple and NOT the final frame-pacing strategy. */
static void avp_wsi_release_images(AvpWsiScope *s)
{
    /* R363: VkFramebuffer/VkImageView must die before their VkSwapchainKHR.
     * vkDeviceWaitIdle is done by the caller before entering this function. */
    if(s->hybrid_framebuffers||s->hybrid_pipeline_cache.valid||
       s->hybrid_foreground_pipeline.pipeline)
        avp_wsi_hybrid_drop_pass(s);
    if (s->device) {
        if (s->image_finished) {
            for (uint32_t i = 0; i < s->image_count; i++) {
                if (s->image_finished[i]) vkDestroySemaphore(s->device, s->image_finished[i], NULL);
            }
        }
        if (s->swapchain) vkDestroySwapchainKHR(s->device, s->swapchain, NULL);
    }
    free(s->image_finished);
    free(s->images);
    s->image_finished = NULL;
    s->images = NULL;
    s->swapchain = VK_NULL_HANDLE;
    s->image_count = 0u;
}

static int avp_wsi_frame_resources(AvpWsiScope *s, AvpVkWsiSmoke *out)
{
    VkCommandPoolCreateInfo pool = {0};
    VkCommandBufferAllocateInfo alloc = {0};
    VkSemaphoreCreateInfo sem = {0};
    VkFenceCreateInfo fence = {0};
    VkResult r;
    pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = s->queue_family;
    r = vkCreateCommandPool(s->device, &pool, NULL, &s->commands);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Persistent command pool: %d", (int)r); return 0; }
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = s->commands;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1u;
    r = vkAllocateCommandBuffers(s->device, &alloc, &s->frame_command);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Persistent command buffer: %d", (int)r); return 0; }
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    r = vkCreateSemaphore(s->device, &sem, NULL, &s->acquired);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Acquire semaphore: %d", (int)r); return 0; }
    fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT; /* first frame has no submission */
    r = vkCreateFence(s->device, &fence, NULL, &s->frame_fence);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Frame fence: %d", (int)r); return 0; }
    return 1;
}

static int avp_wsi_image_semaphores(AvpWsiScope *s, AvpVkWsiSmoke *out)
{
    VkSemaphoreCreateInfo sem = {0};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    s->image_finished = calloc(s->image_count, sizeof(*s->image_finished));
    if (!s->image_finished) { avp_wsi_status(out, "Out of memory for present semaphores"); return 0; }
    for (uint32_t i = 0; i < s->image_count; i++) {
        VkResult r = vkCreateSemaphore(s->device, &sem, NULL, &s->image_finished[i]);
        if (r != VK_SUCCESS) {
            avp_wsi_status(out, "Present semaphore %u: %d", i, (int)r);
            return 0;
        }
    }
    return 1;
}

static int avp_wsi_rebuild(AvpWsiScope *s, uint32_t width, uint32_t height,
                           AvpVkWsiSmoke *out)
{
    VkResult r = vkDeviceWaitIdle(s->device);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Device wait before resize: %d", (int)r); return 0; }
    /* A completed frame has no further need for deferred host staging. A
     * resize is an infrequent explicit idle boundary, safe to free it here. */
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        avp_vk_r32_deferred_dispose(s->device,&s->hybrid_texture_transfers[i]);
    avp_wsi_release_images(s);
    if (!avp_wsi_make_swapchain(s, width, height, out)) return 0;
    if (!avp_wsi_image_semaphores(s, out)) return 0;
    out->swapchain_recreations++;
    return 1;
}

/* R322 persistent-frame command lifecycle. Every frame records and submits a
 * genuine GPU command. It does not upload/copy the CPU game framebuffer.
 * One graphics submission at a time keeps the acquire semaphore safe to reuse.
 * Present-wait semaphores are per SWAPCHAIN IMAGE, avoiding early reuse when
 * presentation outlives the graphics queue fence. */
static int avp_wsi_present_diagnostic_frame(AvpWsiScope *s, uint32_t ordinal,
                                            AvpVkWsiSmoke *out)
{
    VkCommandBufferBeginInfo begin = {0};
    VkImageSubresourceRange range = {0};
    VkImageMemoryBarrier before = {0}, after;
    VkClearColorValue color = {{0.03f, 0.08f, 0.16f, 1.0f}};
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit = {0};
    VkPresentInfoKHR present = {0};
    uint32_t index = UINT32_MAX;
    VkResult r;
    r = vkWaitForFences(s->device, 1u, &s->frame_fence, VK_TRUE, UINT64_MAX);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Frame fence wait failed: %d", (int)r); return -1; }
    r = vkAcquireNextImageKHR(s->device, s->swapchain, UINT64_MAX,
                               s->acquired, VK_NULL_HANDLE, &index);
    if (avp_wsi_result_action((int32_t)r) == AVP_WSI_RESULT_OUT_OF_DATE) return 0;
    if (avp_wsi_result_action((int32_t)r) == AVP_WSI_RESULT_FATAL || index >= s->image_count) {
        avp_wsi_status(out, "Image acquire failed: %d", (int)r); return -1;
    }
    /* Suboptimal is still drawable; recreate after the present. */
    int recreate = avp_wsi_result_action((int32_t)r) == AVP_WSI_RESULT_SUBOPTIMAL;
    r = vkResetCommandPool(s->device, s->commands, 0u);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Reset command pool: %d", (int)r); return -1; }
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    r = vkBeginCommandBuffer(s->frame_command, &begin);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Begin diagnostic frame: %d", (int)r); return -1; }
    /* Animate the clear color slightly to show more than one GPU frame. */
    color.float32[0] += (float)(ordinal % 24u) / 160.0f;
    color.float32[1] += (float)(ordinal % 48u) / 240.0f;
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1u;
    range.layerCount = 1u;
    before.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    before.srcAccessMask = 0u;
    before.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    before.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; /* fully discard old contents */
    before.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    before.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    before.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    before.image = s->images[index];
    before.subresourceRange = range;
    vkCmdPipelineBarrier(s->frame_command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &before);
    vkCmdClearColorImage(s->frame_command, s->images[index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1u, &range);
    after = before;
    after.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    after.dstAccessMask = 0u;
    after.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    after.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(s->frame_command, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0u, 0u, NULL, 0u, NULL, 1u, &after);
    r = vkEndCommandBuffer(s->frame_command);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "End diagnostic frame: %d", (int)r); return -1; }
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1u;
    submit.pWaitSemaphores = &s->acquired;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &s->frame_command;
    submit.signalSemaphoreCount = 1u;
    submit.pSignalSemaphores = &s->image_finished[index];
    r = vkResetFences(s->device, 1u, &s->frame_fence);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "Reset frame fence: %d", (int)r); return -1; }
    r = vkQueueSubmit(s->queue, 1u, &submit, s->frame_fence);
    if (r != VK_SUCCESS) { avp_wsi_status(out, "GPU frame submit: %d", (int)r); return -1; }
    out->command_submitted = 1;
    out->frames_submitted++;
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1u;
    present.pWaitSemaphores = &s->image_finished[index];
    present.swapchainCount = 1u;
    present.pSwapchains = &s->swapchain;
    present.pImageIndices = &index;
    r = vkQueuePresentKHR(s->queue, &present);
    if (avp_wsi_result_action((int32_t)r) == AVP_WSI_RESULT_OUT_OF_DATE) return 0;
    if (avp_wsi_result_action((int32_t)r) == AVP_WSI_RESULT_FATAL) {
        avp_wsi_status(out, "GPU frame present failed: %d", (int)r); return -1;
    }
    out->presented = 1;
    out->frames_presented++;
    return (avp_wsi_result_action((int32_t)r) == AVP_WSI_RESULT_SUBOPTIMAL || recreate) ? 0 : 1;
}

/* Opt-in multi-frame WSI diagnostic; never connected to the game renderer.
 * Bounded retries prevent minimized/unstable surfaces from spinning forever.
 * Return success only if the full requested number of frames were presented. */
/* Shared bounded frame driver for both borrowed and owned X11 lifetimes.
 * The owner decides when to destroy the display, never this frame loop. */
static int avp_wsi_loop_run(AvpWsiScope *scope,
                            void *x11_display, uintptr_t x11_window,
                            uint32_t requested_width, uint32_t requested_height,
                            const char *gpu_key, uint32_t frame_limit,
                            AvpVkWsiSmoke *out)
{
    Display *display = (Display *)x11_display;
    XWindowAttributes attrs;
    uint32_t retries = 0;
    int ok = 0;
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!frame_limit || frame_limit > 4096u || !display || !x11_window) {
        avp_wsi_status(out, "Invalid frame count or X11 window (valid: 1..4096)");
        return 0;
    }
    if (!avp_wsi_initialize(scope, x11_display, x11_window,
                             requested_width, requested_height, gpu_key, out)) return 0;
    if (!avp_wsi_frame_resources(scope, out) ||
        !avp_wsi_image_semaphores(scope, out)) return 0;
    while (out->frames_presented < frame_limit) {
        int status;
        if (!XGetWindowAttributes(display, (Window)x11_window, &attrs)) {
            avp_wsi_status(out, "X11 window destroyed"); break;
        }
        if (attrs.width <= 0 || attrs.height <= 0) {
            avp_wsi_status(out, "Window minimized to zero-size surface"); break;
        }
        if (avp_wsi_window_extent_changed(scope->extent.width, scope->extent.height,
                                          (uint32_t)attrs.width, (uint32_t)attrs.height)) {
            if (!avp_wsi_rebuild(scope, (uint32_t)attrs.width,
                                 (uint32_t)attrs.height, out)) break;
        }
        status = avp_wsi_present_diagnostic_frame(scope, out->frames_presented, out);
        if (status < 0) break;
        if (status == 0) {
            if (!avp_wsi_retry_allowed(++retries)) {
                avp_wsi_status(out, "Swapchain remains out of date after 8 retries");
                break;
            }
            if (!avp_wsi_rebuild(scope, (uint32_t)attrs.width,
                                 (uint32_t)attrs.height, out)) break;
        } else {
            retries = 0u;
        }
    }
    if (out->frames_presented == frame_limit) {
        avp_wsi_status(out, "Persistent Vulkan clear presented %u/%u frames; %u recreations",
             out->frames_presented, frame_limit, out->swapchain_recreations);
        ok = 1;
    }
    return ok;
}

int avp_vk_wsi_loop(void *x11_display, uintptr_t x11_window,
                    uint32_t requested_width, uint32_t requested_height,
                    const char *gpu_key, uint32_t frame_limit, AvpVkWsiSmoke *out)
{
    AvpWsiScope scope = {0};
    int ok = avp_wsi_loop_run(&scope, x11_display, x11_window,
                              requested_width, requested_height,
                              gpu_key, frame_limit, out);
    avp_wsi_destroy(&scope);
    return ok;
}

/* One-shot executable's X11 Display/Window are exclusively transferred to
 * this function. A borrowed display must continue using the public APIs
 * above. This explicit ownership is required for correct Vulkan/Xlib
 * close-hook lifetime: surface -> window/display -> instance. */
int avp_vk_wsi_run_owned_xlib_diagnostic(void *x11_display, uintptr_t x11_window,
     uint32_t requested_width, uint32_t requested_height, const char *gpu_key,
     AvpVkWsiDiagnosticMode mode, uint32_t frame_limit,
     const char *vertex_spv, const char *fragment_spv, AvpVkWsiSmoke *out)
{
    AvpWsiScope scope = {0};
    int ok = 0;
    scope.owned_x11_display = (Display *)x11_display;
    scope.owned_x11_window = (Window)x11_window;
    if (!out) {
        avp_wsi_destroy(&scope);
        return 0;
    }
    if (mode == AVP_VK_WSI_DIAG_LOOP) {
        ok = avp_wsi_loop_run(&scope, x11_display, x11_window,
                    requested_width, requested_height, gpu_key, frame_limit, out);
    } else if (mode >= AVP_VK_WSI_DIAG_CLEAR &&
               mode <= AVP_VK_WSI_DIAG_MIXED_CRY_RGB) {
        scope.shader_requested = mode != AVP_VK_WSI_DIAG_CLEAR;
        if (scope.shader_requested && (!vertex_spv || !fragment_spv)) {
            memset(out, 0, sizeof(*out));
            avp_wsi_status(out, "Missing diagnostic SPIR-V files");
        } else {
            scope.columns_mode = scope.shader_requested ?
                (int)mode - (int)AVP_VK_WSI_DIAG_SHADER : 0;
            ok = avp_wsi_initialize(&scope, x11_display, x11_window,
                         requested_width, requested_height, gpu_key, out);
            if (ok) ok = scope.shader_requested ?
                avp_wsi_shader_once(&scope, vertex_spv, fragment_spv, out) :
                avp_wsi_clear_once(&scope, out);
        }
    } else {
        memset(out, 0, sizeof(*out));
        avp_wsi_status(out, "Invalid standalone Vulkan diagnostic mode");
    }
    if (ok && mode != AVP_VK_WSI_DIAG_LOOP) sleep(1u);
    avp_wsi_destroy(&scope);
    return ok;
}

/* R359: game-owned, borrowed-X11 Vulkan lifetime. One VkInstance is
 * retained through EVERY Xlib close-display hook. The game first calls
 * suspend() while the X11 Window/Display are alive, closes its Display,
 * then calls destroy() to unload the Vulkan ICD safely. */
struct AvpVkWsiGameplayContext {
    AvpWsiScope scope;
    AvpVkWsiSmoke report;
    uint32_t requested_width, requested_height;
    uint32_t consecutive_rebuilds;
    int rebuild_pending;
    int surface_paused; /* zero-size X11 window; retry after restore */
    int ready;
};

/* A failed GPU selection may still have loaded the Vulkan ICD and installed
 * Xlib callbacks. Never unload that instance before the borrowed Display is
 * closed. Keep the context alive, even when the GPU cannot start. */
static int avp_vk_wsi_gameplay_start(AvpVkWsiGameplayContext *ctx,
    void *display,uintptr_t window,uint32_t w,uint32_t h,
    const char *gpu_key,int vsync_enabled)
{
    if(!ctx)return 0;
    avp_wsi_suspend(&ctx->scope);
    ctx->scope.columns_mode=8; /* no synthetic diagnostic data */
    ctx->scope.shader_requested=1; /* require usable fragment output */
    ctx->scope.gameplay_presentation=1;
    ctx->scope.vsync_requested=!!vsync_enabled;
    ctx->ready=0;
    ctx->rebuild_pending=0;
    ctx->surface_paused=0;
    ctx->consecutive_rebuilds=0u;
    ctx->requested_width=w;
    ctx->requested_height=h;
    if(!avp_wsi_initialize(&ctx->scope,display,window,w,h,gpu_key,&ctx->report) ||
       !avp_wsi_frame_resources(&ctx->scope,&ctx->report) ||
       !avp_wsi_image_semaphores(&ctx->scope,&ctx->report)){
        avp_wsi_suspend(&ctx->scope);
        return 0;
    }
    ctx->ready=1;
    avp_wsi_status(&ctx->report,
        "Persistent game-window Vulkan context prepared; rendering remains Software");
    return 1;
}

AvpVkWsiGameplayContext *avp_vk_wsi_gameplay_create(
    void *display,uintptr_t window,uint32_t w,uint32_t h,
    const char *gpu_key,int vsync_enabled,AvpVkWsiSmoke *out)
{
    AvpVkWsiGameplayContext *ctx=calloc(1u,sizeof(*ctx));
    if(!out){free(ctx);return NULL;}
    memset(out,0,sizeof(*out));
    if(!ctx){avp_wsi_status(out,"Vulkan context allocation failed");return NULL;}
    (void)avp_vk_wsi_gameplay_start(ctx,display,window,w,h,gpu_key,vsync_enabled);
    *out=ctx->report;
    return ctx; /* even after failure: instance must survive XCloseDisplay */
}

int avp_vk_wsi_gameplay_reconfigure(AvpVkWsiGameplayContext *ctx,
    void *display,uintptr_t window,uint32_t w,uint32_t h,
    const char *gpu_key,int vsync_enabled,AvpVkWsiSmoke *out)
{
    int ok=avp_vk_wsi_gameplay_start(ctx,display,window,w,h,gpu_key,vsync_enabled);
    if(ctx && out)*out=ctx->report;
    return ok;
}

int avp_vk_wsi_gameplay_frame_busy(const AvpVkWsiGameplayContext *ctx)
{
    /* The gameplay command pool and persistently mapped geometry belong to
     * a single frame. Never reuse them while its submit remains in flight.
     * This check does not modify any Vulkan object or the Jaguar frame. */
    return ctx && ctx->ready && ctx->scope.device && ctx->scope.frame_fence &&
        vkGetFenceStatus(ctx->scope.device,ctx->scope.frame_fence)==VK_NOT_READY;
}
int avp_vk_wsi_gameplay_ready(const AvpVkWsiGameplayContext *ctx)
{
    return ctx && ctx->ready;
}

int avp_vk_wsi_gameplay_rebuild_pending(const AvpVkWsiGameplayContext *ctx)
{
    return ctx&&ctx->ready&&ctx->rebuild_pending;
}

int avp_vk_wsi_gameplay_resize(AvpVkWsiGameplayContext *ctx,
    uint32_t width,uint32_t height,AvpVkWsiSmoke *out)
{
    int restored;
    if(!ctx || !ctx->ready)return 0;
    if(avp_wsi_surface_paused(width,height)){
        /* X11 is allowed to report a transient zero-sized client area while
         * minimizing or unmapping. Do not tear down the owned Vulkan device;
         * no swapchain image can be presented to a zero-sized window. */
        ctx->surface_paused=1;
        ctx->rebuild_pending=1;
        avp_wsi_status(&ctx->report,
            "Hybrid window minimized; swapchain rebuild deferred until restore");
        if(out)*out=ctx->report;
        return 1;
    }
    restored=ctx->surface_paused;
    if(restored){
        /* A minimize/restore cycle is a user window transition, not a
         * repeating driver failure; allow rebuilding even at the old size. */
        ctx->surface_paused=0;
        ctx->consecutive_rebuilds=0u;
    }
    if(avp_wsi_hybrid_rebuild_needed(ctx->requested_width,ctx->requested_height,
                                     width,height,ctx->rebuild_pending)){
        const int was_recovery=ctx->rebuild_pending && !restored;
        /* Ordinary user resizing is not a failing WSI recovery loop. */
        if(!was_recovery)ctx->consecutive_rebuilds=0u;
        /* Persistent SUBOPTIMAL cannot cause infinite device-idle rebuilds.
         * Reset this limit only after a clean successful presentation. */
        if(!avp_wsi_retry_allowed(ctx->consecutive_rebuilds)){
            avp_wsi_status(&ctx->report,"Repeated Vulkan swapchain rebuilds; Software fallback");
            ctx->ready=0;
            if(out)*out=ctx->report;
            avp_wsi_suspend(&ctx->scope);
            return 0;
        }
        if(!avp_wsi_rebuild(&ctx->scope,width,height,&ctx->report)){
            ctx->ready=0;
            if(out)*out=ctx->report;
            avp_wsi_suspend(&ctx->scope);
            return 0;
        }
        ctx->requested_width=width;
        ctx->requested_height=height;
        ctx->rebuild_pending=0;
        if(was_recovery)ctx->consecutive_rebuilds++;
    }
    if(out)*out=ctx->report;
    return 1;
}

void avp_vk_wsi_gameplay_suspend(AvpVkWsiGameplayContext *ctx)
{
    if(!ctx)return;
    avp_wsi_suspend(&ctx->scope);
    ctx->ready=0;
    ctx->surface_paused=0;
    ctx->rebuild_pending=0;
}

/* MUST be called after XCloseDisplay, not from a video menu or resize. */
void avp_vk_wsi_gameplay_destroy(AvpVkWsiGameplayContext *ctx)
{
    if(!ctx)return;
    avp_wsi_destroy(&ctx->scope);
    free(ctx);
}

/* R376: host wall-clock checkpoints for real RTX stalls (not GPU timestamps).
 * Only emit reports when AVP_VULKAN_TIMINGS is enabled. */
static uint64_t avp_wsi_time_ns(void)
{
    struct timespec now={0};
    if(clock_gettime(CLOCK_MONOTONIC,&now)!=0)return 0u;
    return (uint64_t)now.tv_sec*1000000000ull+(uint64_t)now.tv_nsec;
}
/* RE47 R361: opt-in ACTUAL game-window Vulkan world+foreground submission.
 * GPU fragments sample the original source-coordinate AVFL tape and source
 * CRY/RGB images, then the already-composited actor/weapon/HUD DIFFERENCES.
 * The Software image remains available for every failed/unsupported frame.
 * Frame upload is synchronous and still incomplete for production.
 * R362 retains identical textures; R363 retains compatible Vulkan graphics
 * pipelines, render pass, and framebuffers until shader/layout/resize change. */
static void avp_wsi_hybrid_release_frame(AvpWsiScope *s,int retain_texture_bank,
    int retain_geometry,int retain_foreground)
{
    if(!s||!s->device)return;
    /* The previous frame fence must be complete. Keep descriptors pointing
     * at unchanged source images and the SAME reusable world buffer. */
    if(!retain_foreground)
        avp_vk_sampled_pair_dispose(s->device,&s->hybrid_foreground_set);
    if(!retain_geometry||!retain_texture_bank){
        for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
            avp_vk_sampled_pair_dispose(s->device,&s->wall_sets[i]);
    }
    if(!retain_texture_bank){
        avp_vk_wall_gpu_bank_dispose(&s->wall_gpu);
        avp_hybrid_texture_cache_invalidate(&s->hybrid_texture_cache);
    }
}

int avp_vk_wsi_gameplay_present_hybrid(AvpVkWsiGameplayContext *ctx,
    const AvpVkLayeredFrame *layered,const AvpHybridOverlay *overlay,
    uint32_t image_width,uint32_t image_height,
    const char *vertex_spv,const char *world_spv,const char *foreground_spv,
    const AvpPcScanlines *scanlines,AvpVkWsiSmoke *out)
{
    AvpWsiScope *s;
    uint32_t *vs=NULL,*fs_world=NULL,*fs_fg=NULL;
    size_t vs_bytes=0u,world_bytes=0u,fg_bytes=0u;
    VkAttachmentDescription attachment={0};
    VkAttachmentReference reference={0};
    VkSubpassDescription subpass={0};
    VkSubpassDependency deps[2]={{0}};
    VkRenderPassCreateInfo pass={0};
    VkCommandBufferBeginInfo begin={0};
    VkRenderPassBeginInfo render={0};
    VkClearValue clear={0};
    VkSubmitInfo submit={0};
    VkPresentInfoKHR present={0};
    VkPipelineStageFlags wait_stage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkRect2D rect={{0,0},{0,0}};
    AvpHybridViewport placement={0};
    AvpHybridPipelineKey pipeline_key={0};
    AvpVkHybridPush push={0};
    _Static_assert(sizeof(AvpVkHybridPush)==56u,"scanline push layout");
    uint32_t image_index=UINT32_MAX;
    VkResult r;
    int ok=0, reuse_pipeline=0;
    int resize_geometry=0,resize_foreground=0,rebuilt_geometry=0,rebuilt_foreground=0;
    uint64_t tt[7]={0};
    int timed=0;
    uint32_t newly_uploaded=0u;
    uint64_t newly_uploaded_bytes=0u;
    int foreground_transfer=0;
    int strict_texture_verification=0;
    int recoverable_skip=0;
    size_t planned_geometry=0u;
    if(out)memset(out,0,sizeof(*out));
    /* A minimized X11 surface retains its device but must not acquire or
     * submit against the old swapchain until the size is restored. */
    if(ctx && ctx->ready && ctx->surface_paused){
        avp_wsi_status(out,"Hybrid surface paused; Software until restore");
        return 0;
    }
    if(!ctx||!out||!ctx->ready||!layered||!overlay||!overlay->valid||
       !overlay->world||!overlay->foreground||!vertex_spv||!world_spv||
       !foreground_spv||!image_width||!image_height)return 0;
    s=&ctx->scope;
    if(!s->device||!s->swapchain||!s->frame_fence||!s->frame_command||
       !s->image_finished||!s->acquired||!layered->slots.used_count||
       !layered->geometry.data||overlay->width!=layered->layers.width||
       overlay->height!=layered->layers.height||
       (uint64_t)overlay->width*overlay->height>SIZE_MAX/4u||
       overlay->width>8192u||overlay->height>8192u||
       image_width>s->extent.width||image_height>s->extent.height||
       !avp_vk_layered_frame_ready_for_present(layered)){
        avp_wsi_status(out,"Hybrid scene/foreground/window contract mismatch");return 0;
    }
    {const char *setting=getenv("AVP_VULKAN_TIMINGS");
     timed=setting&&strcmp(setting,"0")!=0;}
    /* Developer-only exhaustive Vulkan output check after EVERY new texture.
     * Normal mode proves the first GPU shader/compositor frame, checks source
     * byte-exactly on upload, and compares the Software world every frame.
     * Repeating full swapchain readback on each first-visible texture causes
     * a synchronous fence wait and explains camera-turn frame stalls. */
    {const char *strict=getenv("AVP_VULKAN_VERIFY_TEXTURE_CHANGES");
     strict_texture_verification=strict&&strcmp(strict,"0")!=0;}
    if(timed)tt[0]=avp_wsi_time_ns();
    /* The previous GPU draw must finish before destroying its descriptors
     * or reusing the single persistent command buffer. Swapchain present
     * semaphores are instead retained per IMAGE across frames. */
    r=vkWaitForFences(s->device,1u,&s->frame_fence,VK_TRUE,UINT64_MAX);
    if(r!=VK_SUCCESS){avp_wsi_status(out,"Hybrid previous GPU fence failed: %d",(int)r);goto end;}
    if(timed)tt[1]=avp_wsi_time_ns();
    /* R378: the prior frame fence is the lifetime boundary for all transfer
     * staging. No per-texture queue waits or pool creation on this frame. */
    for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        avp_vk_r32_deferred_dispose(s->device,&s->hybrid_texture_transfers[i]);
    /* R376: preserve textures independently by slot, even while a wall,
     * floor or door is off camera. A changing visible-slot plan must NOT
     * destroy and synchronously reupload otherwise identical GPU images. */
    if(!avp_hybrid_stream_capacity(layered->geometry.bytes,
            (size_t)s->hybrid_stream.geometry_capacity,
            AVP_HYBRID_GEOMETRY_MAX_BYTES,&planned_geometry,&resize_geometry)){
        avp_wsi_status(out,"Hybrid dynamic world buffer exceeds supported limit");goto end;
    }
    (void)planned_geometry;
    resize_foreground=!s->hybrid_stream.foreground.image||
        s->hybrid_stream.foreground.width!=overlay->width||
        s->hybrid_stream.foreground.height!=overlay->height;
    avp_wsi_hybrid_release_frame(s,1,!resize_geometry,!resize_foreground);
    if(!s->diagnostic_lut.image){
        uint32_t *lut=malloc(65536u*sizeof(uint32_t));
        int built=lut&&avp_world_gpu_build_cry_lut(lut,65536u);
        if(built)built=avp_vk_upload_r32uint(s->gpu,s->device,s->queue,
            s->queue_family,lut,256u,256u,65536u*sizeof(uint32_t),
            &s->diagnostic_lut);
        free(lut);
        if(!built){avp_wsi_status(out,"Hybrid CRY lookup upload failed");goto end;}
    }
    if(!avp_vk_hybrid_stream_geometry(s->gpu,s->device,
           layered->geometry.data,layered->geometry.bytes,
           &s->hybrid_stream,&rebuilt_geometry)){
        avp_wsi_status(out,"Hybrid original source geometry upload failed");goto end;
    }
    for(uint32_t i=0u;i<layered->slots.used_count;i++){
        uint32_t slot=layered->slots.order[i];
        const AvpVkWallPixelSource *source=&layered->pixels[slot];
        int existing=avp_vk_wall_gpu_bank_image(&s->wall_gpu,slot)!=NULL;
        if(existing && avp_hybrid_texture_cache_slot_matches(
                           &s->hybrid_texture_cache,layered,slot))continue;
        /* Existing descriptors must be destroyed before changing their
         * image. The old submitted frame is fence-complete above. */
        {
            AvpVkDeferredR32Upload *upload=&s->hybrid_texture_transfers[slot];
            if(!avp_vk_r32_deferred_prepare(s->gpu,s->device,source->words,
                source->width,source->height,
                (size_t)source->width*source->height*4u,upload)){
                avp_wsi_status(out,"Hybrid deferred source staging failed");goto end;
            }
            /* The old descriptor may refer to an old image. Both are safe to
             * retire now that the previous frame fence has completed. */
            avp_vk_sampled_pair_dispose(s->device,&s->wall_sets[slot]);
            if(!avp_vk_wall_gpu_bank_adopt_slot(s->device,slot,
                       source->encoding,&upload->image,&s->wall_gpu)){
                avp_wsi_status(out,"Hybrid deferred texture adoption failed");goto end;
            }
        }
        newly_uploaded++;
        newly_uploaded_bytes+=(uint64_t)source->width*source->height*4u;
        /* Snapshot allocation failure is fail-closed: never mistake a GPU
         * image for a verified copy on a later frame. */
        if(!avp_hybrid_texture_cache_slot_capture(&s->hybrid_texture_cache,
                                                   layered,slot)){
            avp_wsi_status(out,"Hybrid per-slot exact source snapshot failed");goto end;
        }
        if(avp_hybrid_gpu_reproof_for_texture_change(
                s->hybrid_gpu_verified,1,strict_texture_verification))
            s->hybrid_gpu_verified=0;
    }
    if(timed)tt[2]=avp_wsi_time_ns();
    for(uint32_t i=0u;i<layered->slots.used_count;i++){
        uint32_t slot=layered->slots.order[i];
        const AvpVkUploadedR32 *image=avp_vk_wall_gpu_bank_image(&s->wall_gpu,slot);
        if(!image||!(s->wall_sets[slot].set?
               avp_vk_sampled_scene_update_geometry(s->device,
                   &s->hybrid_stream.geometry,&s->wall_sets[slot]):
               avp_vk_sampled_scene_create(s->device,image,
                   &s->diagnostic_lut,&s->hybrid_stream.geometry,
                   &s->wall_sets[slot]))){
            avp_wsi_status(out,"Hybrid original source descriptor creation failed");goto end;
        }
    }
    if(!avp_vk_hybrid_stream_foreground(s->gpu,s->device,
           overlay->foreground,overlay->width,overlay->height,
           &s->hybrid_stream,&rebuilt_foreground)||
       (!s->hybrid_foreground_set.set&&
        !avp_vk_sampled_pair_create(s->device,&s->hybrid_stream.foreground,
           &s->diagnostic_lut,&s->hybrid_foreground_set))){
        avp_wsi_status(out,"Hybrid actor/weapon/HUD foreground upload failed");goto end;
    }
    /* transfer remains dirty until a copy is recorded. Count only actual
     * source-pixel uploads, not reused images with identical contents. */
    foreground_transfer=s->hybrid_stream.foreground_dirty ||
        !s->hybrid_stream.foreground_shader_layout;
    if(!avp_hybrid_shader_cache_refresh(&s->hybrid_shader_cache,
           vertex_spv,world_spv,foreground_spv)){
        avp_wsi_status(out,"Hybrid SPIR-V missing, modified mid-load or invalid");goto end;
    }
    /* Borrow immutable, stage-validated SPIR-V for the entire frame. The
     * exact-byte pipeline cache still notices every revalidated mutation. */
    vs=s->hybrid_shader_cache.entries[0].words;
    vs_bytes=s->hybrid_shader_cache.entries[0].bytes;
    fs_world=s->hybrid_shader_cache.entries[1].words;
    world_bytes=s->hybrid_shader_cache.entries[1].bytes;
    fs_fg=s->hybrid_shader_cache.entries[2].words;
    fg_bytes=s->hybrid_shader_cache.entries[2].bytes;
    if(!avp_hybrid_viewport_make(s->extent.width,s->extent.height,
            image_width,image_height,overlay->width,overlay->height,&placement)){
        avp_wsi_status(out,"Hybrid centered image region unsupported");goto end;
    }
    rect.offset.x=(int32_t)placement.x;
    rect.offset.y=(int32_t)placement.y;
    rect.extent.width=placement.width;
    rect.extent.height=placement.height;
    pipeline_key.target_width=s->extent.width;
    pipeline_key.target_height=s->extent.height;
    pipeline_key.target_format=(uint32_t)s->swapchain_format;
    pipeline_key.region_x=(uint32_t)rect.offset.x;
    pipeline_key.region_y=(uint32_t)rect.offset.y;
    pipeline_key.region_width=rect.extent.width;
    pipeline_key.region_height=rect.extent.height;
    pipeline_key.push_bytes=(uint32_t)sizeof(push);
    pipeline_key.vertex=vs;pipeline_key.vertex_bytes=vs_bytes;
    pipeline_key.world=fs_world;pipeline_key.world_bytes=world_bytes;
    pipeline_key.foreground=fs_fg;pipeline_key.foreground_bytes=fg_bytes;
    if(!avp_hybrid_pipeline_key_valid(&pipeline_key)){
        avp_wsi_status(out,"Hybrid pipeline shader/region contract invalid");goto end;
    }
    reuse_pipeline=s->shader_renderpass && s->shader_pipeline.pipeline &&
        s->hybrid_foreground_pipeline.pipeline &&
        avp_hybrid_pipeline_cache_matches(&s->hybrid_pipeline_cache,&pipeline_key);
    if(!reuse_pipeline){
        /* Fence completed above: old descriptor bindings, pipelines, views
         * and framebuffers are no longer used by GPU submissions. */
        avp_wsi_hybrid_drop_pass(s);
        attachment.format=s->swapchain_format;
    attachment.samples=VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    /* Both verified and unverified frames use the same render pass. A
     * transfer-source layout permits a first-frame pixel proof without
     * publishing an unchecked frame to the desktop. */
    attachment.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    reference.attachment=0u;
    reference.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount=1u;
    subpass.pColorAttachments=&reference;
    deps[0].srcSubpass=VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass=0u;
    deps[0].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass=0u;
    deps[1].dstSubpass=VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
    deps[1].srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass.attachmentCount=1u;
    pass.pAttachments=&attachment;
    pass.subpassCount=1u;
    pass.pSubpasses=&subpass;
    pass.dependencyCount=2u;
    pass.pDependencies=deps;
    if(vkCreateRenderPass(s->device,&pass,NULL,&s->shader_renderpass)!=VK_SUCCESS){
        avp_wsi_status(out,"Hybrid render pass creation failed");goto end;
    }
    if(!avp_vk_opaque_pipeline_create_region(s->device,
           &s->wall_sets[layered->slots.order[0]],s->shader_renderpass,
           s->extent,rect,(uint32_t)sizeof(push),1,vs,vs_bytes,
           fs_world,world_bytes,&s->shader_pipeline)||
       !avp_vk_opaque_pipeline_create_region(s->device,
           &s->hybrid_foreground_set,s->shader_renderpass,s->extent,rect,(uint32_t)sizeof(push),0,
           vs,vs_bytes,fs_fg,fg_bytes,&s->hybrid_foreground_pipeline)){
        avp_wsi_status(out,"Hybrid source-world/foreground graphics pipeline failed");goto end;
    }
        /* Exact byte snapshot failure is safe: drawing works now, but the
         * next frame will rebuild rather than blindly reuse shaders. */
        (void)avp_hybrid_pipeline_cache_capture(&s->hybrid_pipeline_cache,&pipeline_key);
    }
    /* Vulkan draws remain invisible until the first real GPU image proves
     * identical to the original complete Software frame, at this window's
     * exact nearest-neighbor resolution. Failed allocations fall back. */
    /* A different post-process must earn its own real GPU pixel proof. */
    if(avp_scanline_active(scanlines))push.scanlines=*scanlines;
    if(memcmp(&s->hybrid_verified_scanlines,&push.scanlines,sizeof(push.scanlines))!=0)
        s->hybrid_gpu_verified=0;
    if(!s->hybrid_gpu_verified&&!s->shader_readback.buffer &&
       !avp_vk_readback_create(s->gpu,s->device,s->extent.width,
            s->extent.height,s->swapchain_format,&s->shader_readback)){
        avp_wsi_status(out,"Hybrid first-frame readback allocation failed");goto end;
    }
    if(timed)tt[3]=avp_wsi_time_ns();
    r=vkAcquireNextImageKHR(s->device,s->swapchain,UINT64_MAX,s->acquired,
                            VK_NULL_HANDLE,&image_index);
    if(avp_wsi_result_action((int32_t)r)==AVP_WSI_RESULT_OUT_OF_DATE){
        /* No image was acquired or submitted. Preserve the Vulkan device,
         * present this frame through Software, and rebuild next frame. */
        ctx->rebuild_pending=1;
        recoverable_skip=1;
        avp_wsi_status(out,"Hybrid acquire OUT_OF_DATE; swapchain rebuild pending");
        goto end;
    }
    if((r!=VK_SUCCESS&&r!=VK_SUBOPTIMAL_KHR)||image_index>=s->image_count){
        avp_wsi_status(out,"Hybrid swapchain acquire failed: %d",(int)r);goto end;
    }
    if(r==VK_SUBOPTIMAL_KHR)ctx->rebuild_pending=1;
    if(timed)tt[4]=avp_wsi_time_ns();
    if(!avp_wsi_hybrid_ensure_target(s,image_index)){
        avp_wsi_status(out,"Hybrid persistent swapchain target creation failed");goto end;
    }
    push.source_width=overlay->width;
    push.source_height=overlay->height;
    push.region_x=(uint32_t)rect.offset.x;
    push.region_y=(uint32_t)rect.offset.y;
    push.region_width=rect.extent.width;
    push.region_height=rect.extent.height;
    if(vkResetCommandPool(s->device,s->commands,0u)!=VK_SUCCESS){
        avp_wsi_status(out,"Hybrid command pool reset failed");goto end;
    }
    begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if(vkBeginCommandBuffer(s->frame_command,&begin)!=VK_SUCCESS){
        avp_wsi_status(out,"Hybrid command begin failed");goto end;
    }
    /* R378: pack ALL changed source textures into this game's existing
     * command buffer, before the first fragment sample. The previous frame
     * fence protects both old images and the staging lifetimes. */
    for(uint32_t i=0u;i<layered->slots.used_count;i++){
        uint32_t slot=layered->slots.order[i];
        AvpVkDeferredR32Upload *upload=&s->hybrid_texture_transfers[slot];
        if(upload->staging && !avp_vk_r32_deferred_record(s->frame_command,upload)){
            avp_wsi_status(out,"Hybrid deferred texture recording failed");goto end;
        }
    }
    /* R364 foreground upload participates in the SAME frame submit. No
     * dedicated transfer pool, vkQueueSubmit or fence wait per frame. */
    avp_vk_hybrid_stream_record_foreground(s->frame_command,&s->hybrid_stream);
    clear.color.float32[3]=1.0f;
    render.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render.renderPass=s->shader_renderpass;
    render.framebuffer=s->hybrid_framebuffers[image_index];
    render.renderArea.extent=s->extent;
    render.clearValueCount=1u;
    render.pClearValues=&clear;
    vkCmdBeginRenderPass(s->frame_command,&render,VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(s->frame_command,VK_PIPELINE_BIND_POINT_GRAPHICS,
                      s->shader_pipeline.pipeline);
    for(uint32_t i=0u;i<layered->slots.used_count;i++){
        uint32_t slot=layered->slots.order[i];
        uint32_t sc_x,sc_y,sc_w,sc_h;
        VkRect2D scissor;
        if(!avp_vk_layered_frame_slot_scissor(layered,slot,
            (uint32_t)rect.offset.x,(uint32_t)rect.offset.y,
            rect.extent.width,rect.extent.height,
            &sc_x,&sc_y,&sc_w,&sc_h)){
            avp_wsi_status(out,"Hybrid texture scissor contract mismatch");
            goto end;
        }
        if(!sc_w||!sc_h)continue;
        scissor.offset.x=(int32_t)sc_x;
        scissor.offset.y=(int32_t)sc_y;
        scissor.extent.width=sc_w;
        scissor.extent.height=sc_h;
        vkCmdSetScissor(s->frame_command,0u,1u,&scissor);
        push.encoding=layered->slots.slots[slot].encoding;
        push.slot=slot;
        vkCmdBindDescriptorSets(s->frame_command,VK_PIPELINE_BIND_POINT_GRAPHICS,
             s->shader_pipeline.layout,0u,1u,&s->wall_sets[slot].set,0u,NULL);
        vkCmdPushConstants(s->frame_command,s->shader_pipeline.layout,
             VK_SHADER_STAGE_FRAGMENT_BIT,0u,sizeof(push),&push);
        vkCmdDraw(s->frame_command,3u,1u,0u,0u);
    }
    push.encoding=2u;push.slot=0u;
    vkCmdBindPipeline(s->frame_command,VK_PIPELINE_BIND_POINT_GRAPHICS,
                      s->hybrid_foreground_pipeline.pipeline);
    vkCmdBindDescriptorSets(s->frame_command,VK_PIPELINE_BIND_POINT_GRAPHICS,
            s->hybrid_foreground_pipeline.layout,0u,1u,
            &s->hybrid_foreground_set.set,0u,NULL);
    vkCmdPushConstants(s->frame_command,s->hybrid_foreground_pipeline.layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT,0u,sizeof(push),&push);
    vkCmdDraw(s->frame_command,3u,1u,0u,0u);
    vkCmdEndRenderPass(s->frame_command);
    if(!s->hybrid_gpu_verified){
        /* This existing readback helper copies the real GPU color attachment
         * and returns the swapchain image to PRESENT_SRC_KHR. */
        avp_vk_readback_record(s->frame_command,s->images[image_index],
            &s->shader_readback);
    }else{
        VkImageMemoryBarrier present_barrier={0};
        present_barrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        present_barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        present_barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        present_barrier.image=s->images[image_index];
        present_barrier.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        present_barrier.subresourceRange.levelCount=1u;
        present_barrier.subresourceRange.layerCount=1u;
        present_barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        present_barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(s->frame_command,VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0u,0u,NULL,0u,NULL,
            1u,&present_barrier);
    }
    if(vkEndCommandBuffer(s->frame_command)!=VK_SUCCESS){
        avp_wsi_status(out,"Hybrid command end failed");goto end;
    }
    submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount=1u;
    submit.pWaitSemaphores=&s->acquired;
    submit.pWaitDstStageMask=&wait_stage;
    submit.commandBufferCount=1u;
    submit.pCommandBuffers=&s->frame_command;
    submit.signalSemaphoreCount=1u;
    submit.pSignalSemaphores=&s->image_finished[image_index];
    if(vkResetFences(s->device,1u,&s->frame_fence)!=VK_SUCCESS){
        avp_wsi_status(out,"Hybrid frame fence reset failed");goto end;
    }
    r=vkQueueSubmit(s->queue,1u,&submit,s->frame_fence);
    if(r!=VK_SUCCESS){
        avp_wsi_status(out,"Hybrid GPU command submission failed: %d",(int)r);goto end;
    }
    if(timed)tt[5]=avp_wsi_time_ns();
    out->command_submitted=1;
    out->frames_submitted=1u;
    if(!s->hybrid_gpu_verified){
        uint32_t mismatches=UINT32_MAX;
        void *readback=NULL;
        int matched=0;
        /* This verification fence is paid only on a newly built pipeline,
         * resized swapchain or changed source texture bank. No unverified
         * frame is ever submitted to the compositor. */
        r=vkWaitForFences(s->device,1u,&s->frame_fence,VK_TRUE,UINT64_MAX);
        if(r!=VK_SUCCESS){
            avp_wsi_status(out,"Hybrid pixel-proof GPU fence failed: %d",(int)r);
            goto end;
        }
        if(vkMapMemory(s->device,s->shader_readback.memory,0u,
             s->shader_readback.bytes,0u,&readback)==VK_SUCCESS){
            matched=avp_hybrid_gpu_parity_compare_scanlines((const uint8_t*)readback,
                (size_t)s->shader_readback.bytes,s->extent.width,
                s->extent.height,(uint32_t)s->swapchain_format,
                &placement,overlay,&push.scanlines,&mismatches);
            vkUnmapMemory(s->device,s->shader_readback.memory);
        }
        if(!matched){
            avp_wsi_status(out,
                "Hybrid real GPU frame mismatches Software (%u pixels); Software fallback",
                mismatches);
            goto end;
        }
        s->hybrid_verified_scanlines=push.scanlines;
        s->hybrid_gpu_verified=1;
    }
    if(timed)tt[6]=avp_wsi_time_ns();
    present.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount=1u;
    present.pWaitSemaphores=&s->image_finished[image_index];
    present.swapchainCount=1u;
    present.pSwapchains=&s->swapchain;
    present.pImageIndices=&image_index;
    r=vkQueuePresentKHR(s->queue,&present);
    if(avp_wsi_result_action((int32_t)r)==AVP_WSI_RESULT_OUT_OF_DATE){
        /* Submission finished, but presentation was refused because the
         * surface changed. Display Software and rebuild; do not reuse this
         * swapchain or its present semaphores until deviceWaitIdle. */
        ctx->rebuild_pending=1;
        recoverable_skip=1;
        avp_wsi_status(out,"Hybrid present OUT_OF_DATE; swapchain rebuild pending");
        goto end;
    }
    if(r!=VK_SUCCESS&&r!=VK_SUBOPTIMAL_KHR){
        avp_wsi_status(out,"Hybrid GPU present failed: %d",(int)r);
        goto end;
    }
    if(r==VK_SUBOPTIMAL_KHR)ctx->rebuild_pending=1;
    if(timed){
        uint64_t end_ns=avp_wsi_time_ns();
        if(end_ns>tt[0] && end_ns-tt[0]>100000000ull){
            const double to_ms=1.0/1000000.0;
            fprintf(stderr,"[Hybrid Vulkan WSI spike] total=%.2fms "
                "fence=%.2fms staging+geometry=%.2fms "
                "descriptors+pipeline=%.2fms acquire=%.2fms "
                "record+submit=%.2fms GPU_proof=%.2fms "
                "present=%.2fms changed_slots=%u bytes=%llu\n",
                (end_ns-tt[0])*to_ms,(tt[1]-tt[0])*to_ms,
                (tt[2]-tt[1])*to_ms,(tt[3]-tt[2])*to_ms,
                (tt[4]-tt[3])*to_ms,(tt[5]-tt[4])*to_ms,
                (tt[6]-tt[5])*to_ms,(end_ns-tt[6])*to_ms,
                newly_uploaded,(unsigned long long)newly_uploaded_bytes);
        }
    }
    out->presented=1;
    out->frames_presented=1u;
    out->width=s->extent.width;
    out->height=s->extent.height;
    snprintf(out->device_name,sizeof(out->device_name),"%s",ctx->report.device_name);
    out->textures_uploaded=newly_uploaded+
        (foreground_transfer?1u:0u);
    out->texture_bytes_uploaded=(uint32_t)(newly_uploaded_bytes+
            (foreground_transfer?(uint64_t)overlay->width*overlay->height*4u:0u));
    out->geometry_bytes_uploaded=(uint32_t)s->hybrid_stream.geometry_bytes_written;
    (void)rebuilt_geometry;
    (void)rebuilt_foreground;
    avp_wsi_status(out,ctx->rebuild_pending?
        "Hybrid GPU frame presented; swapchain recreation queued":
        "Hybrid Vulkan GPU source world + reusable actor/HUD foreground presented");
    if(!ctx->rebuild_pending)ctx->consecutive_rebuilds=0u;
    ok=1;
end:
    /* R378: a recoverable OUT_OF_DATE acquire can happen after staging and
     * adopting new GPU images but BEFORE any copy was submitted. They are
     * not shader-readable yet. Never retain an exact-content certificate
     * for an image whose deferred upload was abandoned. Otherwise a resize
     * would incorrectly reuse its undefined GPU contents indefinitely. */
    if(recoverable_skip && !out->command_submitted){
        avp_hybrid_texture_cache_invalidate(&s->hybrid_texture_cache);
        s->hybrid_gpu_verified=0;
        for(uint32_t i=0u;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
            avp_vk_r32_deferred_dispose(s->device,&s->hybrid_texture_transfers[i]);
    }
    /* Shader words are owned by the persistent context, not this frame. */
    if(!ok&&!recoverable_skip){
        /* Unrecoverable command/GPU/parity faults still invalidate the
         * context. Only OUT_OF_DATE may safely retain it for rebuild. */
        ctx->ready=0;
    }
    return ok;
}
