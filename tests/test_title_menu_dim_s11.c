/* RE24-S11: retail title->menu dim = three cumulative in-place GPU fade passes
 * (15/16, 13/16, 11/16), established by executing the retail ROM on jagref and
 * matching all 76800 pixels.  Synthetic pixels only; no ROM content. */
#include "jaguar/avp_gpu/frontend_fade.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* composition equals successive fractions, NOT one 6/16 fraction */
    for(unsigned v=0;v<65536u;v+=37u){
        u16 a=avp_gpu_rgb16_fraction((u16)v,15u);
        u16 b=avp_gpu_rgb16_fraction(a,13u);
        u16 c=avp_gpu_rgb16_fraction(b,11u);
        assert(avp_gpu_rgb16_title_menu_dim((u16)v,0u)==(u16)v);
        assert(avp_gpu_rgb16_title_menu_dim((u16)v,1u)==a);
        assert(avp_gpu_rgb16_title_menu_dim((u16)v,2u)==b);
        assert(avp_gpu_rgb16_title_menu_dim((u16)v,3u)==c);
        assert(avp_gpu_rgb16_title_menu_dim((u16)v,9u)==c);
    }
    /* reference-machine observation: pure red 16 -> 6 after three passes;
     * a single 6/16 fraction would give 6 as well, but red 24 -> 11 (not 9) */
    assert((avp_gpu_rgb16_title_menu_dim((u16)(16u<<11),3u)>>11)==6u);
    assert((avp_gpu_rgb16_title_menu_dim((u16)(24u<<11),3u)>>11)==11u);
    assert((avp_gpu_rgb16_fraction((u16)(24u<<11),6u)>>11)==9u);
    assert((avp_gpu_rgb16_title_menu_dim((u16)(31u<<11),3u)>>11)==13u);
    /* field timing: whole passes after 2, 6, 10 fields */
    assert(avp_gpu_title_menu_dim_passes(0)==0u&&avp_gpu_title_menu_dim_passes(1)==0u);
    assert(avp_gpu_title_menu_dim_passes(2)==1u&&avp_gpu_title_menu_dim_passes(6)==2u);
    assert(avp_gpu_title_menu_dim_passes(10)==3u&&avp_gpu_title_menu_dim_passes(50)==3u);
    puts("Title menu dim (retail three-pass) tests passed");
    return 0;
}
