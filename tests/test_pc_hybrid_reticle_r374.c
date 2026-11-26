/* R374: the Vulkan shaped host reticle must exactly match the pre-existing
 * Software crosshair's pixel mask/colors and smooth weapon swing placement. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "pc/pc_crosshair.h"
#include "pc/pc_hybrid_reticle.h"

static void check(unsigned width,unsigned height,int sway,int swing)
{
    uint32_t *pic=(uint32_t*)calloc((size_t)width*height,sizeof(uint32_t));
    assert(pic);
    avp_pc_crosshair_draw(pic,width,width,height,0,0,width,height,sway,swing);
    int unit=avp_pc_hybrid_reticle_unit(width,height);
    int cx=avp_pc_crosshair_x(width,sway,swing),cy=(int)(height/2u);
    unsigned count=0u;
    for(unsigned y=0u;y<height;y++)for(unsigned x=0u;x<width;x++){
        uint32_t rgb=0u;
        int dx=(int)x-cx,dy=(int)y-cy;
        int included=avp_pc_hybrid_reticle_pixel(unit,dx,dy,&rgb);
        assert(pic[(size_t)y*width+x]==(included?rgb:0u));
        count+=(unsigned)included;
    }
    assert(count>0);
    free(pic);
}
int main(void)
{
    check(320u,228u,0,0);
    check(640u,480u,1,8);
    check(1280u,720u,1,-12);
    check(1920u,1080u,1,24);
    check(2560u,1440u,1,-24);
    puts("R374 crosshair shaped X11 overlay matches original Software crosshair PASS");
    return 0;
}
