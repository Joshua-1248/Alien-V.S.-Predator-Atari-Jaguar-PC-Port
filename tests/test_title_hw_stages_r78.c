/* PC R78 title presentation field boundaries, synthetic ROM-free source. */
#define _DEFAULT_SOURCE 1
#define AVP_PC_REFERENCE_TITLE_TIMING 1
#include "../src/jaguar/avp_gpu/frontend_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>
char mjp_games[4]="xxx";
static u32 at(const AvpJaguarFramebuffer *fb,unsigned x,unsigned y)
{ return fb->pixels[(size_t)y*fb->width+x]; }
static void show(AvpAvpGpuState *st,AvpJaguarFramebuffer *fb,unsigned tick)
{
    st->frontend_title_reveal_tick=tick;
    avp_gpu_frontend_present(st,fb,AVP_MJP_TITLE,0);
}
int main(void)
{
    static AvpAvpGpuState st;
    static u32 pixels[320u*240u];
    static u8 background[3]={64u,64u,64u};
    static u8 alien[320u*122u*3u];
    static u8 predator[320u*76u*3u];
    static u8 stamp_a[96u*40u*3u];
    static u8 stamp_p[160u*40u*3u];
    static u8 legal[320u*40u*3u];
    AvpJaguarFramebuffer fb={0};
    fb.width=320u;fb.height=240u;fb.pixels=pixels;
    for(size_t i=0u;i<sizeof(alien);i+=3u){alien[i]=240u;alien[i+1u]=40u;alien[i+2u]=24u;}
    for(size_t i=0u;i<sizeof(predator);i+=3u){predator[i]=48u;predator[i+1u]=216u;predator[i+2u]=80u;}
    for(size_t i=0u;i<sizeof(stamp_a);i+=3u){stamp_a[i]=240u;stamp_a[i+1u]=40u;stamp_a[i+2u]=24u;}
    for(size_t i=0u;i<sizeof(stamp_p);i+=3u){stamp_p[i]=48u;stamp_p[i+1u]=216u;stamp_p[i+2u]=80u;}
    legal[(5u*320u+8u)*3u]=240u;
    st.frontend_img[0][0]=(AvpGpuRgbImage){1,1,background,NULL};
    st.frontend_img[0][3]=(AvpGpuRgbImage){320,122,alien,NULL};
    st.frontend_img[0][2]=(AvpGpuRgbImage){320,76,predator,NULL};
    st.frontend_img[0][5]=(AvpGpuRgbImage){96,40,stamp_a,NULL};
    st.frontend_img[0][6]=(AvpGpuRgbImage){160,40,stamp_p,NULL};
    st.frontend_img[0][4]=(AvpGpuRgbImage){320,40,legal,NULL};
    for(unsigned r=0;r<=6u;r++)st.frontend_loaded[0][r]=1u;
    avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
    avp_gpu_frontend_event(&st,0x5449544cu,0,0);
    show(&st,&fb,0u);
    assert(at(&fb,200u,200u)==0u);                /* blank first frame */
    show(&st,&fb,AVP_PC_TITLE_ALIEN_FIRST-1u);
    assert(at(&fb,290u,32u)==0u);                 /* before first logo pixel */
    show(&st,&fb,AVP_PC_TITLE_ALIEN_FIRST);
    assert(at(&fb,290u,32u)==0xf02818u);         /* first ALIEN phase */
    assert(at(&fb,200u,200u)==0u);                /* black still behind logo */
    show(&st,&fb,AVP_PC_TITLE_BACKGROUND_START);
    assert(at(&fb,200u,200u)==0u);
    show(&st,&fb,AVP_PC_TITLE_BACKGROUND_FULL);
    const u32 full_bg=at(&fb,200u,200u);
    assert(full_bg!=0u);                          /* background has now filled */
    show(&st,&fb,AVP_PC_TITLE_ALIEN_STAMP);
    assert(at(&fb,15u,33u)==0xf02818u);          /* permanent ALIEN wordmark */
    show(&st,&fb,AVP_PC_TITLE_PREDATOR_VISIBLE);
    assert(at(&fb,12u,106u)==full_bg);           /* no early PREDATOR pixels */
    show(&st,&fb,AVP_PC_TITLE_PREDATOR_MOVE-1u);
    assert(at(&fb,12u,120u)==0x30d850u);         /* large GPU-stage PREDATOR */
    show(&st,&fb,AVP_PC_TITLE_PREDATOR_MOVE);
    assert(at(&fb,12u,120u)==0x30d850u);         /* first Update2 phase */
    show(&st,&fb,AVP_PC_TITLE_PREDATOR_STAMP);
    assert(at(&fb,153u,33u)==0x30d850u);         /* permanent PREDATOR mark */
    show(&st,&fb,AVP_PC_TITLE_LEGAL-1u);
    assert(at(&fb,8u,187u)==full_bg);
    show(&st,&fb,AVP_PC_TITLE_LEGAL);
    assert(at(&fb,8u,187u)==0xf00000u);          /* ROM-derived legal bitmap */
    st.frontend_title_reveal_tick=0u;
    for(unsigned k=0u;k<AVP_PC_TITLE_SEQUENCE_FIELDS+25u;k++)
        avp_gpu_frontend_event(&st,0x5449434bu,0,0);
    assert(st.frontend_title_reveal_tick==AVP_PC_TITLE_SEQUENCE_FIELDS);
    avp_gpu_frontend_event(&st,AVP_PC_TITLE_SKIP_EVENT,225,0);
    assert(st.frontend_title_reveal_tick==225u);
    avp_gpu_frontend_event(&st,AVP_PC_TITLE_SKIP_EVENT,999,0);
    assert(st.frontend_title_reveal_tick==AVP_PC_TITLE_SEQUENCE_FIELDS);
    avp_gpu_frontend_event(&st,0x544d454eu,0,0); /* automatic MJP menu */
    assert(!st.frontend_title_splash && st.frontend_title_menu_pending);
    puts("R78 model-aligned CPU/GPU/OP title presentation boundaries PASS");
    return 0;
}
