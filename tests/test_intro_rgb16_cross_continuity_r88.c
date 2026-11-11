/* RE35 R88: source-accurate RGB16 cross/settled picture consistency.
 * The retail Jaguar MJP image OP reads packed RGB16. An RGB888 settled
 * picture introduces a false colour flash at CROSS completion.
 * Synthetic image data only; no original ROM or retail imagery embedded. */
#include "jaguar/avp_gpu/frontend_renderer.h"
#include "jaguar/avp_gpu/frontend_fade.h"
#include "jaguar/framebuffer.h"
#include "mjp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char mjp_games[4]="xxx";

static uint32_t original_dac(unsigned r,unsigned g,unsigned b)
{
    uint8_t rr,gg,bb;
    avp_gpu_rgb16_unpack(avp_gpu_rgb16_pack((u8)r,(u8)g,(u8)b),&rr,&gg,&bb);
    return ((uint32_t)rr<<16)|((uint32_t)gg<<8)|bb;
}

static void run_mode(unsigned mode)
{
    enum { WIDTH=320, HEIGHT=254, IMG_W=192, IMG_H=144 };
    AvpAvpGpuState state;
    AvpJaguarFramebuffer frame={0};
    uint8_t *src1=(uint8_t*)malloc((size_t)IMG_W*IMG_H*3u);
    uint8_t *src2=(uint8_t*)malloc((size_t)IMG_W*IMG_H*3u);
    uint32_t *cross=(uint32_t*)malloc((size_t)WIDTH*HEIGHT*sizeof(uint32_t));
    assert(src1 && src2 && cross);
    assert(avp_jaguar_framebuffer_init(&frame,WIDTH,HEIGHT));
    memset(&state,0,sizeof(state));
    avp_gpu_frontend_enter_mode(&state,mode);
    for(unsigned y=0;y<IMG_H;y++)for(unsigned x=0;x<IMG_W;x++){
        size_t at=3u*((size_t)y*IMG_W+x);
        src1[at]=(uint8_t)((x*3u+y*7u+3u)&255u);
        src1[at+1]=(uint8_t)((x+y*13u+2u)&255u);
        src1[at+2]=(uint8_t)((x*11u+y*17u+5u)&255u);
        src2[at]=(uint8_t)((x*13u+y*19u+7u)&255u);
        src2[at+1]=(uint8_t)((x*23u+y*3u+1u)&255u);
        src2[at+2]=(uint8_t)((x*5u+y*29u+6u)&255u);
    }
    state.frontend_img[mode][0]=(AvpGpuRgbImage){IMG_W,IMG_H,src1,NULL};
    state.frontend_img[mode][1]=(AvpGpuRgbImage){IMG_W,IMG_H,src2,NULL};
    state.frontend_loaded[mode][0]=1u;
    state.frontend_loaded[mode][1]=1u;
    /* Original MJPX x=58 is mode 0's unscaled picture object. */
    avp_gpu_frontend_event(&state,0x4d4a5050u,0,1); /* MJPP 0 -> 1 */
    assert(state.frontend_page_cross_active);
    avp_gpu_frontend_event(&state,0x4d4a5058u,58,16); /* GPU final kernel */
    avp_gpu_frontend_present(&state,&frame,mode,0);
    memcpy(cross,frame.pixels,(size_t)WIDTH*HEIGHT*sizeof(uint32_t));
    avp_gpu_frontend_event(&state,0x4d4a5053u,0,1); /* MJPS completion */
    avp_gpu_frontend_present(&state,&frame,mode,0);
    assert(!state.frontend_page_cross_active);
    for(unsigned y=0;y<HEIGHT;y++)for(unsigned x=0;x<WIDTH;x++){
        const unsigned idx=y*WIDTH+x;
        /* No RGB888 colour jump from final GPU phase to settled OP. */
        assert(frame.pixels[idx]==cross[idx]);
        if(x>=58u&&x<58u+IMG_W&&y>=30u&&y<30u+IMG_H){
            const size_t i=3u*((size_t)(y-30u)*IMG_W+(x-58u));
            assert(frame.pixels[idx]==original_dac(src2[i],src2[i+1],src2[i+2]));
        }else assert(frame.pixels[idx]==0u);
    }
    avp_jaguar_framebuffer_destroy(&frame);
    free(src1);free(src2);free(cross);
}

/* RE35 R93: the original FILES 235 '_' glyph has no ink. The live
 * cursor is cell DEL (0x7F), 84 opaque RGB16 pixels, at the next X. */
static void test_live_story_cursor(void)
{
    AvpAvpGpuState state;
    AvpJaguarFramebuffer frame={0};
    unsigned char *font=(unsigned char*)calloc(224u*39u*2u,1u);
    assert(font && avp_jaguar_framebuffer_init(&frame,320,254));
    /* Model the ROM's actual font arrangement: '_' is blank; only DEL
     * carries the solid green block. This catches R92's invisible cursor. */
    const unsigned idx=0x7fu-(unsigned)'!';
    const unsigned gx=(idx&31u)*7u,gy=(idx>>5)*13u;
    for(unsigned y=0;y<13u;y++)for(unsigned x=0;x<7u;x++){
        size_t at=2u*((size_t)(gy+y)*224u+gx+x);
        if(y>0u){font[at]=0x5au;font[at+1]=0xffu;}
    }
    memset(&state,0,sizeof(state));
    avp_gpu_frontend_enter_mode(&state,AVP_MJP_INTRO);
    state.mjp_font=font;state.mjp_font_size=224u*39u*2u;
    avp_gpu_frontend_event(&state,0x4d4a5043u,31,0);
    avp_gpu_frontend_present(&state,&frame,AVP_MJP_INTRO,0);
    assert(frame.pixels[180u*320u+31u]==0u);
    assert(frame.pixels[181u*320u+31u]!=0u);
    avp_gpu_frontend_event(&state,0x4d4a5043u,38,0);
    avp_gpu_frontend_present(&state,&frame,AVP_MJP_INTRO,0);
    assert(frame.pixels[181u*320u+31u]==0u);
    assert(frame.pixels[181u*320u+38u]!=0u);
    avp_jaguar_framebuffer_destroy(&frame);
    free(font);
}
int main(void)
{
    test_live_story_cursor();
    run_mode(AVP_MJP_INTRO);
    run_mode(AVP_MJP_ESCAPE);
    puts("RE35 R88 MJP scene: stage-16 to settled OP RGB16 continuity PASS");
    return 0;
}
