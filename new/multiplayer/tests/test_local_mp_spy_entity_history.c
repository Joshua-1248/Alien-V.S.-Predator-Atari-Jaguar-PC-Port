#include "local_mp_spy_entity_history.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static u32 generation(unsigned i){return 900u+i;}

int main(void)
{
    AvpAvpGpuState *viewer=calloc(1,sizeof *viewer);
    AvpAvpGpuState *subject=calloc(1,sizeof *subject);
    AvpAmp *live=calloc(AVP_NUM_AMPS,sizeof *live);
    AvpMpSpyEntityHistory saved;
    assert(viewer&&subject&&live);
    viewer->have_prev_sim=1;subject->have_prev_sim=1;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        viewer->amp_prev_x[i]=1000+(s32)i;
        viewer->amp_prev_y[i]=2000+(s32)i;
        viewer->amp_prev_angle[i]=(u16)(11u+i);
        viewer->amp_prev_generation[i]=100+i;
        subject->amp_prev_x[i]=3000+(s32)i;
        subject->amp_prev_y[i]=4000+(s32)i;
        subject->amp_prev_angle[i]=(u16)(22u+i);
        subject->amp_prev_generation[i]=200+i;
        live[i].xpos=5000+(s32)i;
        live[i].ypos=6000+(s32)i;
        live[i].angle=(u16)(33u+i);
    }
    /* A human on another sub-level supplies its OWN prior AMP poses. */
    avp_mp_spy_entities_begin(viewer,subject,1,live,generation,&saved);
    assert(viewer->amp_prev_x[25]==3025);
    assert(viewer->amp_prev_y[25]==4025);
    assert(viewer->amp_prev_angle[25]==47);
    assert(viewer->amp_prev_generation[25]==225);
    avp_mp_spy_entities_end(viewer,&saved);
    assert(viewer->amp_prev_x[25]==1025);
    assert(viewer->amp_prev_y[25]==2025);
    assert(viewer->amp_prev_angle[25]==36);
    assert(viewer->amp_prev_generation[25]==125);
    assert(viewer->have_prev_sim==1);
    /* A headless bot has no previous AMP poses: current==previous. */
    avp_mp_spy_entities_begin(viewer,NULL,0,live,generation,&saved);
    assert(viewer->amp_prev_x[25]==5025);
    assert(viewer->amp_prev_y[25]==6025);
    assert(viewer->amp_prev_angle[25]==58);
    assert(viewer->amp_prev_generation[25]==925);
    avp_mp_spy_entities_end(viewer,&saved);
    assert(viewer->amp_prev_x[25]==1025);
    /* If a subject's saved view refers to an OTHER level, don't use it,
       even when both levels happen to use the same 64x64 maze dimensions. */
    avp_mp_spy_entities_begin(viewer,subject,0,live,generation,&saved);
    assert(viewer->amp_prev_x[25]==5025);
    avp_mp_spy_entities_end(viewer,&saved);
    /* Prior simulation validity is not permanently borrowed. */
    viewer->have_prev_sim=0;
    avp_mp_spy_entities_begin(viewer,subject,1,live,generation,&saved);
    assert(viewer->have_prev_sim==1);
    avp_mp_spy_entities_end(viewer,&saved);
    assert(viewer->have_prev_sim==0);
    free(live);free(subject);free(viewer);
    puts("spy entity history: cross-level, bots, stale levels, restore PASS");
    return 0;
}
