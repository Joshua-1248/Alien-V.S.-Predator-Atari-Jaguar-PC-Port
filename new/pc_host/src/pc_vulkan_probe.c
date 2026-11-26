/*
 * RE46 R320 -- Vulkan physical-device discovery (not a rendering backend).
 * Copyright (c) the AVP PC port contributors.
 *
 * Uses only published Vulkan 1.0 ABI declarations for the small discovery
 * interface. No graphics-engine, emulator, loader or driver implementation
 * code is copied. The production backend should replace these local ABI
 * declarations with the official Khronos Vulkan-Headers before it creates
 * swapchains or rendering pipelines. Official header licensing:
 * https://github.com/KhronosGroup/Vulkan-Headers (Apache-2.0 OR MIT).
 *
 * Runtime loader is a system library (vulkan-1.dll / libvulkan.so.1), never
 * packaged with the game. Absence, error, or CPU-only Vulkan is nonfatal.
 */
#include "pc/pc_vulkan_probe.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/* Small, private, standard Vulkan C ABI subset; pointer/enum widths match
 * the Khronos 1.0 headers. This file deliberately does NOT claim to implement
 * the Vulkan rendering API. */
typedef int32_t AvpVkResult;
typedef void *AvpVkInstance;
typedef void *AvpVkPhysicalDevice;
typedef uint64_t AvpVkSurface;  /* non-dispatchable 64-bit handle */
typedef struct AvpVkAppInfo {
    uint32_t sType; const void *pNext; const char *pApplicationName;
    uint32_t applicationVersion; const char *pEngineName;
    uint32_t engineVersion, apiVersion;
} AvpVkAppInfo;
typedef struct AvpVkInstanceInfo {
    uint32_t sType; const void *pNext; uint32_t flags;
    const AvpVkAppInfo *pApplicationInfo;
    uint32_t enabledLayerCount; const char *const *ppEnabledLayerNames;
    uint32_t enabledExtensionCount; const char *const *ppEnabledExtensionNames;
} AvpVkInstanceInfo;
typedef struct AvpVkExtensionInfo { char extensionName[256]; uint32_t specVersion; } AvpVkExtensionInfo;
typedef struct AvpVkProperties {
    uint32_t apiVersion, driverVersion, vendorID, deviceID, deviceType;
    char deviceName[256]; uint8_t pipelineCacheUUID[16];
    /* The rest of VkPhysicalDeviceProperties is VkPhysicalDeviceLimits and
     * VkPhysicalDeviceSparseProperties (well under 4 KiB for Vulkan 1.0).
     * Keep an aligned over-sized tail so the Vulkan driver can fill it all. */
    uint64_t unused_limits_and_sparse[512];
} AvpVkProperties;
typedef struct AvpVkQueueInfo {
    uint32_t queueFlags, queueCount, timestampValidBits;
    uint32_t minImageTransferGranularity[3];
} AvpVkQueueInfo;
#ifndef _WIN32
typedef struct AvpVkXlibSurfaceInfo {
    uint32_t sType; const void *pNext; uint32_t flags;
    void *dpy; unsigned long window;
} AvpVkXlibSurfaceInfo;
#endif
/* Fail during compilation if a changed compiler ABI breaks our public prefix. */
typedef char avp_vk_prop_name_offset_check[(offsetof(AvpVkProperties,deviceName)==20u)?1:-1];
typedef char avp_vk_prop_uuid_offset_check[(offsetof(AvpVkProperties,pipelineCacheUUID)==276u)?1:-1];
typedef char avp_vk_queue_size_check[(sizeof(AvpVkQueueInfo)==24u)?1:-1];

#ifdef _WIN32
#define AVP_VK_CALL WINAPI
#else
#define AVP_VK_CALL
#endif
typedef void (AVP_VK_CALL *AvpVkVoidProc)(void);
typedef AvpVkVoidProc (AVP_VK_CALL *AvpVkGetProc)(AvpVkInstance,const char*);
typedef AvpVkResult (AVP_VK_CALL *AvpVkCreateInstance)(const AvpVkInstanceInfo*,const void*,AvpVkInstance*);
typedef AvpVkResult (AVP_VK_CALL *AvpVkEnumExtensions)(const char*,uint32_t*,AvpVkExtensionInfo*);
typedef AvpVkResult (AVP_VK_CALL *AvpVkEnumDevices)(AvpVkInstance,uint32_t*,AvpVkPhysicalDevice*);
typedef void (AVP_VK_CALL *AvpVkDestroyInstance)(AvpVkInstance,const void*);
typedef void (AVP_VK_CALL *AvpVkGetProps)(AvpVkPhysicalDevice,AvpVkProperties*);
typedef void (AVP_VK_CALL *AvpVkGetQueues)(AvpVkPhysicalDevice,uint32_t*,AvpVkQueueInfo*);
typedef AvpVkResult (AVP_VK_CALL *AvpVkEnumDevExt)(AvpVkPhysicalDevice,const char*,uint32_t*,AvpVkExtensionInfo*);
typedef AvpVkResult (AVP_VK_CALL *AvpVkSurfaceSupport)(AvpVkPhysicalDevice,uint32_t,AvpVkSurface,uint32_t*);
typedef void (AVP_VK_CALL *AvpVkDestroySurface)(AvpVkInstance,AvpVkSurface,const void*);
#ifndef _WIN32
typedef AvpVkResult (AVP_VK_CALL *AvpVkCreateXlib)(AvpVkInstance,const AvpVkXlibSurfaceInfo*,const void*,AvpVkSurface*);
#endif

#ifdef _WIN32
typedef HMODULE AvpVkLib;
static AvpVkLib avp_vk_open(void){return LoadLibraryA("vulkan-1.dll");}
static void avp_vk_close(AvpVkLib l){if(l)FreeLibrary(l);}
static AvpVkVoidProc avp_vk_symbol(AvpVkLib l,const char *name){return (AvpVkVoidProc)GetProcAddress(l,name);}
#else
typedef void *AvpVkLib;
static AvpVkLib avp_vk_open(void){return dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);}
static void avp_vk_close(AvpVkLib l){if(l)dlclose(l);}
static AvpVkVoidProc avp_vk_symbol(AvpVkLib l,const char *name){return (AvpVkVoidProc)dlsym(l,name);}
#endif

static uint32_t avp_vk_hash_name(const char *s)
{
    uint32_t h=2166136261u;
    while(*s){h^=(uint8_t)*s++;h*=16777619u;}
    return h;
}
static int avp_vk_has_ext(const AvpVkExtensionInfo *e,uint32_t n,const char *name)
{
    for(uint32_t i=0;i<n;i++)if(!strncmp(e[i].extensionName,name,sizeof(e[i].extensionName)))return 1;
    return 0;
}
/* R333: drivers can report more than 128 device extensions. A hard-coded
 * 128-entry cutoff incorrectly classified capable GPUs as having no WSI.
 * Do not claim swapchain compatibility unless the complete enumeration
 * actually succeeds. This implementation uses public Vulkan 1.0 APIs. */
static int avp_vk_has_device_swapchain(AvpVkEnumDevExt fn,
                                       AvpVkPhysicalDevice physical)
{
    uint32_t count=0u;
    AvpVkExtensionInfo *extensions;
    int supported=0;
    if (!fn || fn(physical,NULL,&count,NULL)!=0 ||
        count==0u || count>4096u) return 0;
    extensions=(AvpVkExtensionInfo*)calloc(count,sizeof(*extensions));
    if (!extensions) return 0;
    if (fn(physical,NULL,&count,extensions)==0)
        supported=avp_vk_has_ext(extensions,count,"VK_KHR_swapchain");
    free(extensions);
    return supported;
}

static unsigned avp_vk_existing_duplicates(const AvpVulkanDevices *out,
    uint32_t vendor,uint32_t device,uint32_t namehash)
{
    unsigned n=0;
    for(unsigned i=0;i<out->count;i++){
        const AvpVulkanGpu *g=&out->gpu[i];
        if(g->vendor_id==vendor&&g->device_id==device&&avp_vk_hash_name(g->name)==namehash)n++;
    }
    return n;
}
int avp_vk_find_selected(const AvpVulkanDevices *devices,const char *key)
{
    if(!devices||!key||!key[0])return -1; /* automatic */
    for(unsigned i=0;i<devices->count;i++)
        if(!strcmp(devices->gpu[i].key,key))return (int)i;
    return -1; /* unavailable device means automatic fallback */
}

int avp_vk_choose_compatible(const AvpVulkanDevices *devices,const char *key)
{
    static const uint32_t types[]={2u,1u,3u}; /* discrete, integrated, virtual */
    int requested=avp_vk_find_selected(devices,key);
    if(!devices)return -1;
    if(requested>=0 && devices->gpu[requested].surface_compatible)return requested;
    for(unsigned t=0;t<3u;t++)
        for(unsigned i=0;i<devices->count;i++)
            if(devices->gpu[i].device_type==types[t] && devices->gpu[i].surface_compatible)
                return (int)i;
    return -1;
}

void avp_vk_enumerate(AvpVulkanDevices *out,void *native_display,uintptr_t native_window)
{
    AvpVkLib lib=NULL;
    AvpVkGetProc get=NULL;
    AvpVkCreateInstance create=NULL;
    AvpVkEnumExtensions exts=NULL;
    AvpVkEnumDevices enumerate=NULL;
    AvpVkDestroyInstance destroy=NULL;
    AvpVkGetProps properties=NULL;
    AvpVkGetQueues queues=NULL;
    AvpVkEnumDevExt device_exts=NULL;
    AvpVkSurfaceSupport surface_support=NULL;
    AvpVkDestroySurface destroy_surface=NULL;
    AvpVkInstance instance=NULL;
    AvpVkSurface surface=0u;
    AvpVkExtensionInfo extensions[128];
    AvpVkPhysicalDevice physical[128];
    uint32_t n=0,device_count=0;
    int can_surface=0;
#ifndef _WIN32
    AvpVkCreateXlib create_surface=NULL;
#endif
    if(!out)return;
    memset(out,0,sizeof(*out));
    snprintf(out->status,sizeof(out->status),"Vulkan loader not found");
    lib=avp_vk_open();
    if(!lib)return;
    out->loader_found=1;
    get=(AvpVkGetProc)avp_vk_symbol(lib,"vkGetInstanceProcAddr");
    if(!get){snprintf(out->status,sizeof(out->status),"Vulkan loader missing entry point");goto done;}
    create=(AvpVkCreateInstance)get(NULL,"vkCreateInstance");
    exts=(AvpVkEnumExtensions)get(NULL,"vkEnumerateInstanceExtensionProperties");
    if(!create||!exts){snprintf(out->status,sizeof(out->status),"Vulkan discovery entry points unavailable");goto done;}
    if(exts(NULL,&n,NULL)==0&&n>0&&n<=128u){
        if(exts(NULL,&n,extensions)==0){
#ifndef _WIN32
            can_surface=native_display&&native_window &&
                avp_vk_has_ext(extensions,n,"VK_KHR_surface")&&
                avp_vk_has_ext(extensions,n,"VK_KHR_xlib_surface");
#else
            /* Win32 WSI will be implemented and tested separately. Do not
             * report a Windows GPU as presentation-compatible prematurely. */
            (void)native_display;(void)native_window;
#endif
        }
    }
    {
        const char *enabled[] = {"VK_KHR_surface","VK_KHR_xlib_surface"};
        AvpVkAppInfo app={0,NULL,"AVP Jaguar PC Port",0,"AVP independent Vulkan",0,(1u<<22)};
        AvpVkInstanceInfo info={1,NULL,0,&app,0,NULL,can_surface?2u:0u,can_surface?enabled:NULL};
        if(create(&info,NULL,&instance)!=0||!instance){
            snprintf(out->status,sizeof(out->status),"No compatible Vulkan driver (instance creation failed)");
            goto done;
        }
    }
    out->instance_created=1;
    enumerate=(AvpVkEnumDevices)get(instance,"vkEnumeratePhysicalDevices");
    destroy=(AvpVkDestroyInstance)get(instance,"vkDestroyInstance");
    properties=(AvpVkGetProps)get(instance,"vkGetPhysicalDeviceProperties");
    queues=(AvpVkGetQueues)get(instance,"vkGetPhysicalDeviceQueueFamilyProperties");
    device_exts=(AvpVkEnumDevExt)get(instance,"vkEnumerateDeviceExtensionProperties");
    if(!enumerate||!properties||!destroy||!queues||!device_exts){
        snprintf(out->status,sizeof(out->status),"Vulkan driver missing discovery functions");goto done;
    }
#ifndef _WIN32
    if(can_surface){
        create_surface=(AvpVkCreateXlib)get(instance,"vkCreateXlibSurfaceKHR");
        destroy_surface=(AvpVkDestroySurface)get(instance,"vkDestroySurfaceKHR");
        surface_support=(AvpVkSurfaceSupport)get(instance,"vkGetPhysicalDeviceSurfaceSupportKHR");
        if(create_surface&&destroy_surface&&surface_support){
            AvpVkXlibSurfaceInfo si={1000004000u,NULL,0u,native_display,(unsigned long)native_window};
            if(create_surface(instance,&si,NULL,&surface)==0&&surface)out->surface_created=1;
        }
    }
#else
    (void)surface_support;
    (void)destroy_surface;
#endif
    if(enumerate(instance,&device_count,NULL)!=0 || !device_count){
        snprintf(out->status,sizeof(out->status),"No Vulkan devices reported by driver");goto done;
    }
    if(device_count>128u)device_count=128u;
    if(enumerate(instance,&device_count,physical)!=0){
        snprintf(out->status,sizeof(out->status),"Vulkan physical-device enumeration failed");goto done;
    }
    for(uint32_t i=0;i<device_count&&out->count<AVP_VK_MAX_GPUS;i++){
        AvpVkProperties props;
        AvpVulkanGpu *dst;
        uint32_t hash,dupe;
        memset(&props,0,sizeof(props));
        properties(physical[i],&props);
        if(props.deviceType!=1u&&props.deviceType!=2u&&props.deviceType!=3u)continue;
        dst=&out->gpu[out->count];
        dst->vendor_id=props.vendorID;dst->device_id=props.deviceID;
        dst->device_type=props.deviceType;dst->api_version=props.apiVersion;
        memcpy(dst->name,props.deviceName,sizeof(dst->name));
        dst->name[sizeof(dst->name)-1u]=0;
        hash=avp_vk_hash_name(dst->name);
        dupe=avp_vk_existing_duplicates(out,dst->vendor_id,dst->device_id,hash);
        snprintf(dst->key,sizeof(dst->key),"%08x:%08x:%08x:%u",
            dst->vendor_id,dst->device_id,hash,dupe);
        snprintf(dst->wsi_detail,sizeof(dst->wsi_detail),"No X11 surface");
        if(surface && !surface_support)
            snprintf(dst->wsi_detail,sizeof(dst->wsi_detail),"WSI query unavailable");
        if(surface && surface_support){
            uint32_t qn=0;
            snprintf(dst->wsi_detail,sizeof(dst->wsi_detail),"Swapchain unsupported");
            if (avp_vk_has_device_swapchain(device_exts,physical[i])) {
                snprintf(dst->wsi_detail,sizeof(dst->wsi_detail),"No graphics+present queue");
                queues(physical[i],&qn,NULL);
                if(qn>0u && qn<=128u){
                    AvpVkQueueInfo q[128];
                    memset(q,0,sizeof(q));
                    queues(physical[i],&qn,q);
                    for(uint32_t j=0;j<qn;j++){
                        uint32_t supported=0;
                        if((q[j].queueFlags&1u)&&q[j].queueCount>0u&&
                            surface_support(physical[i],j,surface,&supported)==0&&supported){
                            dst->surface_compatible=1;out->surface_compatible_count++;
                            snprintf(dst->wsi_detail,sizeof(dst->wsi_detail),"X11 presentation supported");
                            break;
                        }
                    }
                }
            }
        }
        fprintf(stderr,"[Vulkan WSI] %s: %s\n",dst->name,dst->wsi_detail);
        out->count++;
    }
    if(!out->count)snprintf(out->status,sizeof(out->status),"No Vulkan hardware GPUs found (CPU drivers excluded)");
    else if(!out->surface_created)snprintf(out->status,sizeof(out->status),
        "%u Vulkan hardware GPU(s); window presentation not verified",out->count);
    else snprintf(out->status,sizeof(out->status),
        "%u Vulkan hardware GPU(s); %u with X11 presentation support",out->count,out->surface_compatible_count);
done:
    if(surface&&destroy_surface)destroy_surface(instance,surface,NULL);
    if(instance&&destroy)destroy(instance,NULL);
    avp_vk_close(lib);
}
