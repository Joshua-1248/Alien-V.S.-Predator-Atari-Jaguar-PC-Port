/* RE47 R339: actual R338 source->R325 Vulkan image upload adapter.
 * Optional Vulkan-SDK diagnostics only; no production renderer changes. */
#include "pc/pc_vulkan_wall_gpu_bank.h"
#include <stdlib.h>
#include <string.h>
typedef struct GpuBankContext {
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue queue;
    uint32_t family;
} GpuBankContext;
static int gpu_acquire(void *opaque,uint32_t slot,const uint32_t *words,
    uint32_t width,uint32_t height,uint32_t encoding,size_t bytes,
    void **resource)
{
    GpuBankContext *ctx=(GpuBankContext *)opaque;
    AvpVkUploadedR32 *image;
    (void)slot;
    if (!resource || !ctx ||
        (encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8 &&
         encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8)) return 0;
    *resource=NULL;
    image=(AvpVkUploadedR32 *)calloc(1u,sizeof(*image));
    if (!image)return 0;
    if (!avp_vk_upload_r32uint(ctx->gpu,ctx->device,ctx->queue,ctx->family,
                              words,width,height,bytes,image)) {
        /* avp_vk_upload_r32uint() cleans up its own failed uploads. */
        free(image);
        return 0;
    }
    *resource=image;
    return 1;
}
static void gpu_release(void *opaque,uint32_t slot,void *resource)
{
    GpuBankContext *ctx=(GpuBankContext *)opaque;
    AvpVkUploadedR32 *image=(AvpVkUploadedR32 *)resource;
    (void)slot;
    if (!image || !ctx) return;
    avp_vk_uploaded_r32_dispose(ctx->device,image);
    free(image);
}
int avp_vk_wall_gpu_bank_build(VkPhysicalDevice gpu,VkDevice device,
    VkQueue queue,uint32_t family,const AvpVkWallSlotPlan *plan,
    const AvpGpuRgbImage *const sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    size_t max_texture_bytes,uint64_t max_total_bytes,
    AvpWorldGpuStaging *scratch,AvpVkWallGpuBank *out)
{
    GpuBankContext ctx={gpu,device,queue,family};
    AvpVkWallGpuBank temp={0};
    if (!gpu || !device || !queue || !out || out->owner_device ||
        out->bank.count || out->bank.uploaded_bytes) return 0;
    for (uint32_t i=0;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        if (out->bank.slots[i].resource || out->bank.slots[i].width ||
            out->bank.slots[i].height || out->bank.slots[i].encoding ||
            out->bank.order[i]) return 0;
    if (!avp_vk_wall_image_bank_build(plan,sources,max_texture_bytes,
        max_total_bytes,scratch,gpu_acquire,gpu_release,&ctx,&temp.bank))
        return 0;
    temp.owner_device=device;
    *out=temp;
    return 1;
}
void avp_vk_wall_gpu_bank_dispose(AvpVkWallGpuBank *bank)
{
    GpuBankContext ctx={0};
    if (!bank)return;
    ctx.device=bank->owner_device;
    if (!ctx.device && bank->bank.count)return; /* refuse orphan cleanup */
    avp_vk_wall_image_bank_dispose(&bank->bank,gpu_release,&ctx);
    bank->owner_device=VK_NULL_HANDLE;
}
const AvpVkUploadedR32 *avp_vk_wall_gpu_bank_image(
    const AvpVkWallGpuBank *bank,uint32_t slot)
{
    if (!bank || !bank->owner_device || slot>=AVP_WORLD_SCENE_IMAGE_SLOTS)
        return NULL;
    return (const AvpVkUploadedR32 *)bank->bank.slots[slot].resource;
}

int avp_vk_wall_gpu_bank_replace_slot(VkPhysicalDevice gpu,VkDevice device,
    VkQueue queue,uint32_t family,uint32_t slot,
    const uint32_t *words,uint32_t width,uint32_t height,
    uint32_t encoding,size_t bytes,AvpVkWallGpuBank *bank)
{
    GpuBankContext ctx={gpu,device,queue,family};
    AvpVkWallImageEntry *entry;
    void *candidate=NULL;
    uint64_t old_bytes=0u;
    if(!bank||!gpu||!device||!queue||!words||
       slot>=AVP_WORLD_SCENE_IMAGE_SLOTS||
       (bank->owner_device && bank->owner_device!=device)||
       bank->bank.count>=AVP_WORLD_SCENE_IMAGE_SLOTS+1u||
       !width||!height||width>8192u||height>8192u||
       bytes!=(size_t)width*height*4u||bytes>64u*1024u*1024u||
       (encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8 &&
        encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8))return 0;
    entry=&bank->bank.slots[slot];
    if(entry->resource){
        old_bytes=(uint64_t)entry->width*entry->height*4u;
    }else if(bank->bank.count>=AVP_WORLD_SCENE_IMAGE_SLOTS)return 0;
    if(bank->bank.uploaded_bytes<old_bytes||
       bank->bank.uploaded_bytes-old_bytes+bytes>512ull*1024u*1024u)
        return 0;
    if(!gpu_acquire(&ctx,slot,words,width,height,encoding,bytes,&candidate))
        return 0;
    if(entry->resource)gpu_release(&ctx,slot,entry->resource);
    else bank->bank.order[bank->bank.count++]=slot;
    entry->resource=candidate;entry->width=width;
    entry->height=height;entry->encoding=encoding;
    bank->bank.uploaded_bytes=bank->bank.uploaded_bytes-old_bytes+bytes;
    bank->owner_device=device;
    return 1;
}


int avp_vk_wall_gpu_bank_adopt_slot(VkDevice device,uint32_t slot,
    uint32_t encoding,AvpVkUploadedR32 *source,AvpVkWallGpuBank *bank)
{
    GpuBankContext ctx={0};
    AvpVkWallImageEntry *entry;
    AvpVkUploadedR32 *owned;
    uint64_t before=0u,bytes=0u;
    if(!device||!source||!source->image||!source->memory||
       !source->width||!source->height||source->width>8192u||source->height>8192u||
       !bank||slot>=AVP_WORLD_SCENE_IMAGE_SLOTS||
       (bank->owner_device&&bank->owner_device!=device)||
       (encoding!=AVP_WORLD_IMAGE_RGB8_ALPHA8&&
        encoding!=AVP_WORLD_IMAGE_CRY16_ALPHA8))return 0;
    entry=&bank->bank.slots[slot];
    bytes=(uint64_t)source->width*source->height*4u;
    if(bytes>64ull*1024u*1024u)return 0;
    if(entry->resource)before=(uint64_t)entry->width*entry->height*4u;
    else if(bank->bank.count>=AVP_WORLD_SCENE_IMAGE_SLOTS)return 0;
    if(bank->bank.uploaded_bytes<before||
       bank->bank.uploaded_bytes-before+bytes>512ull*1024u*1024u)return 0;
    owned=(AvpVkUploadedR32*)malloc(sizeof(*owned));
    if(!owned)return 0;
    *owned=*source;
    ctx.device=device;
    if(entry->resource)gpu_release(&ctx,slot,entry->resource);
    else bank->bank.order[bank->bank.count++]=slot;
    entry->resource=owned;
    entry->width=source->width;
    entry->height=source->height;
    entry->encoding=encoding;
    bank->bank.uploaded_bytes=bank->bank.uploaded_bytes-before+bytes;
    bank->owner_device=device;
    memset(source,0,sizeof(*source));
    return 1;
}
