/* RE34 R77: PC background-first title interval, independent per-logo phases,
 * automatic menu handoff. Synthetic samples only; no ROM artwork bundled.
 */
#define _DEFAULT_SOURCE 1
#include "../src/jaguar/avp_gpu/frontend_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>
char mjp_games[4]="xxx";
static unsigned pixel_at(const AvpJaguarFramebuffer *fb,unsigned x,unsigned y)
{ return fb->pixels[(size_t)y*fb->width+x]; }
int main(void)
{
    static AvpAvpGpuState st;
    static uint32_t pixels[320u*240u];
    static unsigned char backdrop[3]={64u,64u,64u};
    static unsigned char alien[320u*122u*3u];
    static unsigned char predator[320u*76u*3u];
    AvpJaguarFramebuffer fb={0};
    fb.width=320u;fb.height=240u;fb.pixels=pixels;
    for(unsigned i=0;i<sizeof(alien);i+=3u){alien[i]=240u;alien[i+1]=40u;alien[i+2]=24u;}
    for(unsigned i=0;i<sizeof(predator);i+=3u){predator[i]=48u;predator[i+1]=216u;predator[i+2]=80u;}
    st.frontend_img[0][0]=(AvpGpuRgbImage){1,1,backdrop,NULL};
    st.frontend_img[0][3]=(AvpGpuRgbImage){320,122,alien,NULL};
    st.frontend_img[0][2]=(AvpGpuRgbImage){320,76,predator,NULL};
    st.frontend_loaded[0][0]=st.frontend_loaded[0][3]=st.frontend_loaded[0][2]=1u;
    avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
    avp_gpu_frontend_event(&st,0x5449544cu,0,0); /* TITL */
    for(unsigned t=0;t<=32u;t++){
        st.frontend_title_reveal_tick=(unsigned char)t;
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        /* This pixel lies on the ALIEN original phase-1 source rectangle.
         * In the background-only period it must be the background. */
        assert(pixel_at(&fb,290u,46u)!=0xf02818u);
        assert(pixel_at(&fb,155u,48u)!=0x30d850u);
    }
    st.frontend_title_reveal_tick=33u;
    avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
    assert(pixel_at(&fb,290u,46u)==0xf02818u);
    st.frontend_title_reveal_tick=64u; /* Predator's first visible phase */
    avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
    assert(pixel_at(&fb,290u,46u)!=0xf02818u);
    st.frontend_title_reveal_tick=65u;
    avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
    /* Original Predator phase-1 y=(2*28+216-4-28)/2=120. */
    assert(pixel_at(&fb,12u,120u)==0x30d850u);
    st.frontend_title_reveal_tick=112u;
    avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
    assert(pixel_at(&fb,18u,46u)==0xf02818u);
    assert(pixel_at(&fb,155u,48u)==0x30d850u);
    /* Replay 141 original CPU ticks: only the hosted presentation clock
     * caps at 128; the original NewTitle VBL count is not changed. */
    st.frontend_title_reveal_tick=0u;
    for(unsigned i=0;i<141u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
    assert(st.frontend_title_reveal_tick==128u);
    avp_gpu_frontend_event(&st,0x544d454eu,0,0);
    avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
    assert(pixel_at(&fb,18u,46u)!=0u);
    puts("R77 background-first title and independent logo presentation PASS");
    return 0;
}
