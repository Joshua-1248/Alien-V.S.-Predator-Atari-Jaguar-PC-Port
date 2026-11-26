/* RE47 R339: atomic lifetime policy for per-source-slot GPU wall images.
 * Independent of Vulkan headers; actual GPU callbacks live in the optional
 * SDK-enabled pc_vulkan_wall_gpu_bank.c module. */
#include "pc/pc_vulkan_wall_image_bank.h"
#include <string.h>

typedef struct BankBuildContext {
    AvpVkWallImageBank candidate;
    AvpVkWallImageAcquireFn acquire;
    AvpVkWallImageReleaseFn release;
    void *user;
} BankBuildContext;

static int bank_is_empty(const AvpVkWallImageBank *bank)
{
    if (!bank || bank->count || bank->uploaded_bytes) return 0;
    for (uint32_t i=0;i<AVP_WORLD_SCENE_IMAGE_SLOTS;i++)
        if (bank->slots[i].resource || bank->slots[i].width ||
            bank->slots[i].height || bank->slots[i].encoding ||
            bank->order[i])return 0;
    return 1;
}
void avp_vk_wall_image_bank_dispose(AvpVkWallImageBank *bank,
    AvpVkWallImageReleaseFn release,void *user)
{
    if (!bank) return;
    /* A live bank requires a real release callback. Refuse to lose handles
     * if the caller accidentally omits its backend destruction function. */
    if (!release && !bank_is_empty(bank)) return;
    if (bank->count>AVP_WORLD_SCENE_IMAGE_SLOTS) return;
    for (uint32_t n=bank->count;n>0u;n--) {
        uint32_t slot=bank->order[n-1u];
        if (slot<AVP_WORLD_SCENE_IMAGE_SLOTS && bank->slots[slot].resource)
            release(user,slot,bank->slots[slot].resource);
    }
    memset(bank,0,sizeof(*bank));
}
static int bank_acquire(void *opaque,uint32_t slot,const uint32_t *words,
    uint32_t width,uint32_t height,uint32_t encoding,size_t bytes)
{
    BankBuildContext *ctx=(BankBuildContext *)opaque;
    AvpVkWallImageBank *b=&ctx->candidate;
    void *resource=NULL;
    int success;
    if (slot>=AVP_WORLD_SCENE_IMAGE_SLOTS || b->count>=AVP_WORLD_SCENE_IMAGE_SLOTS ||
        b->slots[slot].resource || !ctx->acquire || !ctx->release)
        return 0;
    success=ctx->acquire(ctx->user,slot,words,width,height,encoding,
                         bytes,&resource);
    if (!resource) return 0;
    /* Avoid double-free of a backend accidentally returning one resource
     * for multiple slots. The existing owner will dispose it exactly once.
     * A backend returning an alias MUST NOT use that handle independently. */
    for (uint32_t i=0u;i<b->count;i++)
        if (b->slots[b->order[i]].resource==resource)
            return 0;
    if (!success) {
        ctx->release(ctx->user,slot,resource);
        return 0;
    }
    b->slots[slot].resource=resource;
    b->slots[slot].width=width;
    b->slots[slot].height=height;
    b->slots[slot].encoding=encoding;
    b->order[b->count++]=slot;
    b->uploaded_bytes+=(uint64_t)bytes;
    return 1;
}
int avp_vk_wall_image_bank_build(const AvpVkWallSlotPlan *plan,
    const AvpGpuRgbImage *const sources[AVP_WORLD_SCENE_IMAGE_SLOTS],
    size_t max_texture_bytes,uint64_t max_total_bytes,
    AvpWorldGpuStaging *scratch,
    AvpVkWallImageAcquireFn acquire,AvpVkWallImageReleaseFn release,
    void *user,AvpVkWallImageBank *out)
{
    BankBuildContext ctx={0};
    AvpVkWallUploadStats stats={0};
    if (!out || !bank_is_empty(out) || !acquire || !release) return 0;
    ctx.acquire=acquire;ctx.release=release;ctx.user=user;
    if (!avp_vk_wall_source_upload(plan,sources,max_texture_bytes,
                                   max_total_bytes,scratch,bank_acquire,
                                   &ctx,&stats) ||
        ctx.candidate.count!=stats.slots_uploaded ||
        ctx.candidate.uploaded_bytes!=stats.source_bytes_uploaded) {
        avp_vk_wall_image_bank_dispose(&ctx.candidate,release,user);
        return 0;
    }
    *out=ctx.candidate;
    return 1;
}
