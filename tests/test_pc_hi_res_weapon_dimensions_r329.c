/* R329: validate weapon PNG canvas is not constrained to native integer scale. */
#include "jaguar/avp_gpu/hires_sprite_pack.h"
#include "jaguar/avp_gpu/fp_overlay.h"
#include <assert.h>
#include <stdint.h>

static void check(unsigned file,int w,int h)
{
    AvpFpOverlayDesc d;
    AvpGpuRgbImage png={0};
    unsigned i;
    png.width=w;png.height=h;
    png.rgb=(uint8_t *)(uintptr_t)1u;
    png.alpha=(uint8_t *)(uintptr_t)2u;
    for(i=0;i<40u;i++){
        if(!avp_gpu_fp_overlay_desc((int)i,&d))break;
        if((unsigned)d.file==file){
            assert(avp_gpu_hires_fp_image_usable(&png,d.width,d.height));
            assert(!avp_gpu_hires_fp_image_usable(&png,0,d.height));
            assert(!avp_gpu_hires_fp_image_usable(&png,d.width,0));
            png.alpha=NULL;
            assert(!avp_gpu_hires_fp_image_usable(&png,d.width,d.height));
            return;
        }
    }
    assert(0 && "weapon ID missing from original descriptor table");
}
int main(void)
{
    AvpGpuRgbImage img={0};
    check(133u,1247,1261); /* user's provided 133.png, almost native 13x */
    check(154u,1254,1254); /* user's provided 154.png, non-native aspect */
    check(149u,704,376);  /* ordinary 4x screenshot/artwork */
    check(135u,1453,791); /* arbitrary correct RGBA */
    img.rgb=(uint8_t *)(uintptr_t)1u;
    img.alpha=(uint8_t *)(uintptr_t)2u;
    assert(!avp_gpu_hires_fp_image_usable(&img,96,97));
    img.width=1247;img.height=1261;
    assert(avp_gpu_hires_fp_image_usable(&img,96,97));
    img.rgb=NULL;
    assert(!avp_gpu_hires_fp_image_usable(&img,96,97));
    return 0;
}
