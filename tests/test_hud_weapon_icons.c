#include "jaguar/avp_gpu/hud_weapon_icons.h"
#include <assert.h>

int main(void)
{
    /* Every icon rectangle comes from the original RDB.DTA record header. */
    assert(avp_hud_weapon_rect(0,1).x==246);
    assert(avp_hud_weapon_rect(0,1).y==48);
    assert(avp_hud_weapon_rect(0,1).width==50);
    assert(avp_hud_weapon_rect(0,4).y==176);
    assert(avp_hud_weapon_rect(1,1).y==145);
    assert(avp_hud_weapon_rect(1,2).x==255);
    assert(avp_hud_weapon_rect(1,3).height==31);
    assert(avp_hud_weapon_rect(2,5).x==202);
    assert(avp_hud_weapon_rect(2,6).y==75);
    assert(avp_hud_weapon_rect(2,7).width==0);

    /* The original no-source-fill Blitter command clears precisely the
     * even/even and odd/odd checkerboard pixels (relative to icon origin). */
    assert(avp_gpu_weapon_icon_pixel(230u,0,246,48,0u,0,0,0,0)==0u);
    assert(avp_gpu_weapon_icon_pixel(230u,0,247,48,0u,0,0,0,0)==230u);
    assert(avp_gpu_weapon_icon_pixel(230u,0,246,49,0u,0,0,0,0)==230u);
    assert(avp_gpu_weapon_icon_pixel(230u,0,247,49,0u,0,0,0,0)==0u);
    assert(avp_gpu_weapon_icon_pixel(230u,0,296,48,0u,0,0,0,0)==230u);
    /* $EB52's second pass can shade one pixel beyond odd rectangles. */
    assert(avp_gpu_weapon_icon_pixel(230u,0,245+51,176+19,0u,0,0,0,0)==0u);
    assert(avp_gpu_weapon_icon_pixel(230u,0,245+52,176+19,0u,0,0,0,0)==230u);
    assert(avp_gpu_weapon_icon_pixel(0u,0,246,48,2u,1,6,0,0)==0u);

    /* All six original fade steps track the equipped weapon's 10 Hz
     * in/out brightness, without modifying the others. */
    for(int n=0;n<=6;n++) {
        assert(avp_gpu_weapon_icon_pixel(225u,0,246,48,1u<<1,1,n,0,0)==(u8)(225+n));
        assert(avp_gpu_weapon_icon_pixel(225u,0,249,88,1u<<2,1,n,0,0)==225u);
        assert(avp_gpu_weapon_icon_pixel(225u,1,255,39,1u<<2,2,n,0,0)==(u8)(225+n));
        assert(avp_gpu_weapon_icon_pixel(225u,1,244,97,1u<<3,0,0,3,n)==(u8)(225+n));
        assert(avp_gpu_weapon_icon_pixel(225u,2,259,129,1u<<3,3,n,0,0)==(u8)(225+n));
    }
    /* Unowned Alien and Predator icon silhouettes use the same mask. */
    assert(avp_gpu_weapon_icon_pixel(226u,1,246,145,0x0cu,0,0,0,0)==0u);
    assert(avp_gpu_weapon_icon_pixel(226u,2,259,129,0u,0,0,0,0)==0u);
    assert(avp_gpu_weapon_icon_pixel(226u,2,260,129,0u,0,0,0,0)==226u);
    /* Predator cloak/medkit original PX fade path remains authoritative. */
    assert(avp_gpu_weapon_icon_pixel(225u,2,202,164,1u<<5,5,6,0,0)==225u);
    assert(avp_gpu_weapon_icon_pixel(225u,2,31,75,1u<<6,6,6,0,0)==225u);
    return 0;
}
