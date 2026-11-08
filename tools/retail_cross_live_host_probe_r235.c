/* R235 offline comparison against the actual, unmodified frontend renderer.
 * Synthetic pixels only; never linked into or instrumented in the game.
 */
#include "jaguar/avp_gpu/frontend_renderer.h"
#include "jaguar/avp_gpu/frontend_fade.h"
#include "jaguar/framebuffer.h"
#include "mjp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char mjp_games[4]="xxx";
extern unsigned int r234_original_cross_word(unsigned int,unsigned int,unsigned);

enum { PWIDTH=192, PHEIGHT=144, FBWIDTH=320, FBHEIGHT=228 };
static unsigned char old_pixels[PWIDTH*PHEIGHT*3];
static unsigned char new_pixels[PWIDTH*PHEIGHT*3];

static void put_pixel(unsigned char *image,unsigned x,unsigned y,unsigned short p)
{
    unsigned char r=0,g=0,b=0;
    avp_gpu_rgb16_unpack(p,&r,&g,&b);
    size_t at=3u*((size_t)y*PWIDTH+x);
    image[at+0]=r;image[at+1]=g;image[at+2]=b;
}
static unsigned short pack_host(unsigned int rgb)
{
    return avp_gpu_rgb16_pack((unsigned char)(rgb>>16),
                              (unsigned char)(rgb>>8),(unsigned char)rgb);
}
int main(void)
{
    AvpAvpGpuState *st=calloc(1,sizeof(*st));
    AvpJaguarFramebuffer fb={0};
    assert(st&&avp_jaguar_framebuffer_init(&fb,FBWIDTH,FBHEIGHT));
    avp_gpu_frontend_enter_mode(st,AVP_MJP_INTRO);
    for(unsigned y=0;y<PHEIGHT;y++)for(unsigned x=0;x<PWIDTH;x++){
        /* The first image contains a bright background with deliberately
         * contrasting packed RGB16 pairs; both images are fully opaque. */
        const unsigned short a=(x&1u)?0x4A44u:0xEA09u;
        const unsigned short b=(x&1u)?0xFC14u:0x548Fu;
        put_pixel(old_pixels,x,y,a);
        put_pixel(new_pixels,x,y,b);
    }
    st->frontend_img[AVP_MJP_INTRO][0].width=PWIDTH;
    st->frontend_img[AVP_MJP_INTRO][0].height=PHEIGHT;
    st->frontend_img[AVP_MJP_INTRO][0].rgb=old_pixels;
    st->frontend_img[AVP_MJP_INTRO][1].width=PWIDTH;
    st->frontend_img[AVP_MJP_INTRO][1].height=PHEIGHT;
    st->frontend_img[AVP_MJP_INTRO][1].rgb=new_pixels;
    st->frontend_loaded[AVP_MJP_INTRO][0]=1;
    st->frontend_loaded[AVP_MJP_INTRO][1]=1;
    avp_gpu_frontend_event(st,0x4D4A5050u,0,1); /* real MJPP event */
    assert(st->frontend_page_cross_active);
    /* R280: the CPU publishes picture X and CROSS stage per VBL (MJPX):
     * Do_Intro page 1 ($4BED4=-1, $4BED8=10) is X=58 for D5=0..7, then
     * stages 1..16 at 18+4*min(16-stage,10). */
    for(unsigned tick=0;tick<24;tick++){
        unsigned min_x=FBWIDTH,max_x=0,seen=0;
        const unsigned stage=tick<8u?0u:tick-7u;
        const unsigned d1=tick<8u?10u:(16u-stage<10u?16u-stage:10u);
        const unsigned want_x=18u+4u*d1;
        avp_gpu_frontend_event(st,0x4D4A5058u,(int)want_x,(int)stage); /* real MJPX */
        avp_gpu_frontend_event(st,0x5449434Bu,0,0); /* real TICK event */
        assert(st->frontend_page_cross_phase==stage);
        avp_gpu_frontend_present(st,&fb,AVP_MJP_INTRO,0);
        for(unsigned x=0;x<FBWIDTH;x++){
            if(fb.pixels[50u*FBWIDTH+x]){
                if(x<min_x)min_x=x;
                if(x>max_x)max_x=x;
                seen++;
            }
        }
        assert(seen>0&&min_x==want_x&&max_x==want_x+191u);
        unsigned old_word=0xEA094A44u,new_word=0x548FFC14u;
        unsigned high=pack_host(fb.pixels[50u*FBWIDTH+want_x+6u]);
        unsigned low=pack_host(fb.pixels[50u*FBWIDTH+want_x+7u]);
        unsigned actual=(high<<16)|low;
        unsigned original=r234_original_cross_word(old_word,new_word,stage);
        printf("TICK=%u PHASE=%u HOST_X=%u..%u HOST_RGB16=%08X ORIGINAL_GPU_RGB16=%08X MATCH=%u\n",
               tick+1u,stage,min_x,max_x,actual,original,actual==original);
        assert(actual==original);
    }
    avp_gpu_frontend_event(st,0x4D4A5053u,0,1); /* real MJPS */
    assert(!st->frontend_page_cross_active);
    avp_jaguar_framebuffer_destroy(&fb);
    free(st);
    puts("PASS live host renderer probe: MJPP, 8 slide + 16 CROSS MJPX frames, MJPS");
    return 0;
}
