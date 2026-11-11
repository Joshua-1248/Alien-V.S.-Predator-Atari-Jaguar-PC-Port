/* RE34 R79 -- synthetic/no-ROM regression for the Jaguar-model reveal curves.
 * The title equations are PC presentation only; game state never changes. */
#define _DEFAULT_SOURCE 1
#define AVP_PC_REFERENCE_TITLE_TIMING 1
#include "../src/jaguar/avp_gpu/frontend_renderer.c"
#include <assert.h>
#include <stdio.h>
char mjp_games[4]="xxx";
static unsigned sample_g(const AvpJaguarFramebuffer *fb,unsigned x,unsigned y)
{ return (fb->pixels[(size_t)y*fb->width+x]>>8)&255u; }
static void show(AvpAvpGpuState *s,AvpJaguarFramebuffer *fb,unsigned tick)
{
    s->frontend_title_reveal_tick=tick;
    avp_gpu_frontend_present(s,fb,AVP_MJP_TITLE,0);
}
int main(void)
{
    static AvpAvpGpuState st;
    static u32 pixels[320u*240u];
    static u8 background[3]={64u,64u,64u};
    static u8 predator[320u*76u*3u];
    AvpJaguarFramebuffer fb={0};
    unsigned bg_partial,bg_full,pred_first,pred_dip,pred_full;
    fb.width=320u;fb.height=240u;fb.pixels=pixels;
    for(size_t i=0u;i<sizeof predator;i+=3u){
        predator[i]=48u;predator[i+1u]=216u;predator[i+2u]=80u;
    }
    st.frontend_img[0][0]=(AvpGpuRgbImage){1,1,background,NULL};
    st.frontend_img[0][2]=(AvpGpuRgbImage){320,76,predator,NULL};
    st.frontend_loaded[0][0]=st.frontend_loaded[0][2]=1u;
    avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
    avp_gpu_frontend_event(&st,0x5449544cu,0,0);
    /* R78 had reached full backdrop brightness by field 361 (local 47).
     * The actual model trace remains partially dim at that field. */
    show(&st,&fb,AVP_PC_TITLE_BACKGROUND_START);
    assert(sample_g(&fb,200u,200u)==0u);
    show(&st,&fb,47u);
    bg_partial=sample_g(&fb,200u,200u);
    assert(bg_partial>0u);
    show(&st,&fb,AVP_PC_TITLE_BACKGROUND_FULL);
    bg_full=sample_g(&fb,200u,200u);
    assert(bg_partial<bg_full && bg_full==64u);
    assert(frontend_title_predator_reveal(82u)==0u);
    assert(frontend_title_predator_reveal(106u)==5u);
    assert(frontend_title_predator_reveal(112u)==4u);
    assert(frontend_title_predator_reveal(124u)==2u);
    assert(frontend_title_predator_reveal(142u)==11u);
    assert(frontend_title_predator_reveal(154u)==16u);
    show(&st,&fb,106u);
    pred_first=sample_g(&fb,12u,120u);
    show(&st,&fb,124u);
    pred_dip=sample_g(&fb,12u,120u);
    show(&st,&fb,154u);
    pred_full=sample_g(&fb,12u,120u);
    assert(pred_first>pred_dip && pred_dip>bg_full);
    assert(pred_full>pred_first);
    show(&st,&fb,AVP_PC_TITLE_PREDATOR_MOVE);
    assert(sample_g(&fb,12u,120u)==pred_full);
    puts("R79 ROM-model background and two-wave PREDATOR reveal PASS");
    return 0;
}
