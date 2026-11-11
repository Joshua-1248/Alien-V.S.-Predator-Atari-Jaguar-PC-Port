/* RE34 R83: the title image must not jump when the renderer switches from
 * the original-ROM scanout to the PC MJP menu/credits layer.
 * Use a distinctive source-row colour in the original native 320x240 scanout
 * and in the same FILES-208-like 320x280 source. Verify each of the 240
 * *displayed* rows, not merely the top-left raw framebuffer coordinate. */
#include "pc/pc_title_native.h"
#include "jaguar/avp_gpu/frontend_renderer.h"
#include "jaguar/avp_gpu/frontend_fade.h"
#include "jaguar/cry.h"
#include "jaguar/framebuffer.h"
#include "mjp.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char mjp_games[4]="xxx";

static uint32_t host_rgb16(unsigned r,unsigned g,unsigned b)
{
    u8 rr,gg,bb;
    avp_gpu_rgb16_unpack(avp_gpu_rgb16_pack((u8)r,(u8)g,(u8)b),&rr,&gg,&bb);
    return (uint32_t)rr<<16|(uint32_t)gg<<8|bb;
}

int main(void)
{
    enum { W=320, H=240, STAGING_H=254, SOURCE_H=280 };
    AvpAvpGpuState st;
    AvpJaguarFramebuffer native={0},menu={0},small={0};
    uint8_t *backdrop=(uint8_t*)malloc((size_t)W*SOURCE_H*3u);
    uint32_t *native_source=(uint32_t*)malloc((size_t)W*H*sizeof(uint32_t));
    unsigned mismatches=0u;
    assert(backdrop&&native_source);
    assert(avp_jaguar_framebuffer_init(&native,W,STAGING_H));
    assert(avp_jaguar_framebuffer_init(&menu,W,STAGING_H));
    assert(avp_jaguar_framebuffer_init(&small,W,H));
    memset(&st,0,sizeof(st));
    for(unsigned y=0;y<SOURCE_H;y++)for(unsigned x=0;x<W;x++){
        const size_t p=(size_t)y*W+x;
        const unsigned r=(y*7u+x*3u)&255u;
        const unsigned g=(y*13u+x*5u)&255u;
        const unsigned b=(y*17u+x)&255u;
        backdrop[p*3u]=(uint8_t)r;
        backdrop[p*3u+1u]=(uint8_t)g;
        backdrop[p*3u+2u]=(uint8_t)b;
        if(y<H)native_source[p]=host_rgb16(r,g,b);
    }
    assert(avp_pc_title_native_copy_rgb(native_source,W,H,W,&native));
    avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
    st.frontend_img[AVP_MJP_TITLE][0]=(AvpGpuRgbImage){W,SOURCE_H,backdrop,NULL};
    st.frontend_loaded[AVP_MJP_TITLE][0]=1u;
    /* Menu requested, but not dimmed and lettering still pending: the title
     * bitmap must be IDENTICAL to the final full-bright native scanout. */
    st.frontend_title_splash=0u;
    st.frontend_title_menu_pending=1u;
    st.frontend_title_menu_dim_phase=16u;
    avp_gpu_frontend_present(&st,&menu,AVP_MJP_TITLE,0);
    for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++){
        const size_t sampled=(size_t)(y+14u)*W+x;
        if(menu.pixels[sampled]!=native.pixels[sampled])mismatches++;
    }
    assert(mismatches==0u);
    /* The 14 spare top rows stay black. */
    for(unsigned y=0;y<14u;y++)for(unsigned x=0;x<W;x++)
        assert(menu.pixels[(size_t)y*W+x]==0u);
    /* Original title dims in-place: that should change ONLY colour, not the
     * Y coordinate (including the last visible line). */
    st.frontend_title_menu_dim_phase=6u;
    avp_gpu_frontend_present(&st,&menu,AVP_MJP_TITLE,0);
    for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++){
        const size_t sampled=(size_t)(y+14u)*W+x;
        const uint32_t c=native.pixels[sampled];
        const u16 p=avp_gpu_rgb16_pack((u8)(c>>16),(u8)(c>>8),(u8)c);
        u8 r,g,b;
        avp_gpu_rgb16_unpack(avp_gpu_rgb16_title_menu_dim(p,3u),&r,&g,&b);
        assert(menu.pixels[sampled]==((uint32_t)r<<16|(uint32_t)g<<8|b));
    }
    /* Foreground menu glyphs already carry the 14-row staging offset.
     * Moving the entire finished frame instead of the artwork layer would
     * displace SELECT CHARACTER down another 14 pixels. */
    {
        uint8_t *font=(uint8_t*)calloc(320u*96u*2u,1u);
        const unsigned idx=(unsigned)'S'-0x21u;
        const unsigned gx=(idx&31u)*11u,gy=(4u+((idx>>5)&1u))*16u;
        u8 r,g,b;
        assert(font);
        for(unsigned yy=0;yy<16u;yy++)for(unsigned xx=0;xx<10u;xx++){
            const size_t o=2u*((size_t)(gy+yy)*320u+(gx+xx));
            font[o]=0x11u;font[o+1u]=0xc0u;
        }
        st.avp_font=font;
        st.avp_font_size=320u*96u*2u;
        st.frontend_title_menu_pending=0u;
        avp_gpu_frontend_present(&st,&menu,AVP_MJP_TITLE,0);
        avp_gpu_rgb16_unpack(0x11c0u,&r,&g,&b);
        assert(menu.pixels[112u*W+62u]==
               ((uint32_t)r<<16|(uint32_t)g<<8|b));
        assert(menu.pixels[98u*W+62u]!=menu.pixels[112u*W+62u]);
        free(font);st.avp_font=NULL;st.avp_font_size=0u;
    }
    /* CREDITS reuses the same title bitmap and 3-pass original dim. No
     * second 14-line movement is permitted on menu -> credits either. */
    avp_gpu_frontend_present(&st,&menu,AVP_MJP_CREDITS,0);
    for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++){
        const size_t sampled=(size_t)(y+14u)*W+x;
        const uint32_t c=native.pixels[sampled];
        const u16 p=avp_gpu_rgb16_pack((u8)(c>>16),(u8)(c>>8),(u8)c);
        u8 r,g,b;
        avp_gpu_rgb16_unpack(avp_gpu_rgb16_title_menu_dim(p,3u),&r,&g,&b);
        assert(menu.pixels[sampled]==((uint32_t)r<<16|(uint32_t)g<<8|b));
    }
    /* Non-PC frontend target remains in native coordinates; this is not an
     * accidental 14-line change to the Jaguar GPU bitmap origin itself. */
    st.frontend_title_menu_pending=1u;
    st.frontend_title_menu_dim_phase=16u;
    avp_gpu_frontend_present(&st,&small,AVP_MJP_TITLE,0);
    for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++)
        assert(small.pixels[(size_t)y*W+x]==native_source[(size_t)y*W+x]);
    avp_jaguar_framebuffer_destroy(&native);
    avp_jaguar_framebuffer_destroy(&menu);
    avp_jaguar_framebuffer_destroy(&small);
    free(backdrop);free(native_source);
    puts("R83 title/menu/credits 240-line source aperture continuity: PASS");
    return 0;
}
