/* R378: no driver required. Check deferred-image ownership and replacement
 * against the same GPU-bank bookkeeping used by the real game renderer. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "pc/pc_vulkan_wall_gpu_bank.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define H(T,n) ((T)(uintptr_t)(n))
static int destroyed_images=0,destroyed_bytes=0;
/* This mock is deliberately the only Vulkan-side disposal dependency. */
void avp_vk_uploaded_r32_dispose(VkDevice d,AvpVkUploadedR32 *u)
{
    assert(d==H(VkDevice,2));
    if(u->image){assert(u->memory);destroyed_images++;destroyed_bytes+=(int)(u->width*u->height*4u);}
    memset(u,0,sizeof(*u));
}
static AvpVkUploadedR32 make(uintptr_t id,uint32_t w,uint32_t h)
{
    AvpVkUploadedR32 result={0};
    result.image=H(VkImage,id);
    result.memory=H(VkDeviceMemory,id+100u);
    result.width=w;result.height=h;
    return result;
}
int main(void)
{
    AvpVkWallGpuBank bank={0};
    AvpVkUploadedR32 a=make(10,16,16),b=make(11,8,8),c=make(12,32,8);
    assert(!avp_vk_wall_gpu_bank_adopt_slot(H(VkDevice,2),
        AVP_WORLD_SCENE_IMAGE_SLOTS,AVP_WORLD_IMAGE_RGB8_ALPHA8,&a,&bank));
    assert(!avp_vk_wall_gpu_bank_adopt_slot(H(VkDevice,2),
        0,AVP_WORLD_IMAGE_ABSENT,&a,&bank));
    assert(a.image&&bank.bank.count==0u);
    assert(avp_vk_wall_gpu_bank_adopt_slot(H(VkDevice,2),
        5,AVP_WORLD_IMAGE_RGB8_ALPHA8,&a,&bank));
    assert(!a.image&&!a.memory&&bank.bank.count==1u&&bank.bank.order[0]==5u);
    assert(bank.bank.uploaded_bytes==16u*16u*4u);
    assert(avp_vk_wall_gpu_bank_image(&bank,5)->width==16u);
    assert(avp_vk_wall_gpu_bank_adopt_slot(H(VkDevice,2),
        5,AVP_WORLD_IMAGE_CRY16_ALPHA8,&b,&bank));
    assert(!b.image&&destroyed_images==1&&bank.bank.count==1u);
    assert(bank.bank.uploaded_bytes==8u*8u*4u);
    assert(avp_vk_wall_gpu_bank_image(&bank,5)->height==8u);
    assert(avp_vk_wall_gpu_bank_adopt_slot(H(VkDevice,2),
        6,AVP_WORLD_IMAGE_RGB8_ALPHA8,&c,&bank));
    assert(bank.bank.count==2u&&bank.bank.order[1]==6u);
    assert(bank.bank.uploaded_bytes==8u*8u*4u+32u*8u*4u);
    assert(!c.image);
    avp_vk_wall_gpu_bank_dispose(&bank);
    assert(bank.bank.count==0u&&!bank.owner_device&&destroyed_images==3);
    assert(destroyed_bytes==16*16*4+8*8*4+32*8*4);
    puts("R378 GPU-bank adoption: ownership, replacement, capacity and cleanup PASS");
    return 0;
}
