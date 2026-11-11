/* RE36 R101: characterize the PC CROSS *logical field* model, NOT GPU cycles.
 * This test includes the current source verbatim and intercepts One_Tick's
 * VSync callback.  Additional hypothetical host redraws are NOT VBlanks.
 * The 2-field/stage policy is an approximation from a BigPEmu recording,
 * not an assertion about original Jaguar GPU/Blitter/TOM arbitration. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/pc_attract_fields.h"
#include "pc/pc_frontend_field_clock.h"
#include "../src/game/mjp.c"

volatile u32 joy_cur=0xffffffffu;
volatile u32 one_second_ticks=60u;

static unsigned field_count,phase,xpos,phase_event_count,presented_on_event;
static unsigned transitions_started,transitions_done,extra_host_redraws,redraw_multiplier;
static unsigned field_phase[40],field_x[40];
static FILE *timeline;

static void front_event(void *u,unsigned event,s32 a,s32 b)
{
    (void)u;
    if(event==0x4d4a5050u){ /* MJPP */
        assert(field_count==0u);
        ++transitions_started;
    }else if(event==0x4d4a5058u){ /* MJPX: source position and phase */
        assert(!avp_pc_frontend_present_on_event(AVP_MJP_INTRO,event));
        assert(!avp_pc_frontend_present_on_event(AVP_MJP_ESCAPE,event));
        ++phase_event_count;
        xpos=(unsigned)a;
        phase=(unsigned)b;
    }else if(event==0x4d4a5053u){ /* MJPS */
        assert(field_count==40u&&phase==16u);
        assert(!avp_pc_frontend_present_on_event(AVP_MJP_INTRO,event));
        ++transitions_done;
    }else if(event==0x5449434bu){ /* TICK, already displayed by VSync */
        assert(!avp_pc_frontend_present_on_event(AVP_MJP_INTRO,event));
    }else if(avp_pc_frontend_present_on_event(AVP_MJP_INTRO,event)){
        ++presented_on_event;
    }
}
static const AvpRuntimeOps ops={.frontend_event=front_event};
const AvpRuntimeOps *avp_runtime_ops(void){return &ops;}

/* Each call is a Jaguar logical field. Redrawing a high-Hz PC display in
 * between fields must not issue an additional original MJP One_Tick. */
void VSync(void)
{
    assert(field_count<40u);
    field_phase[field_count]=phase;
    field_x[field_count]=xpos;
    if(timeline && redraw_multiplier==1u)
        fprintf(timeline,"%u,%u,%u\n",field_count+1u,phase,xpos);
    ++field_count;
    for(unsigned i=1u;i<redraw_multiplier;++i){
        /* Not a video-field callback, and not a fresh GPU publication. */
        ++extra_host_redraws;
    }
}

static void run(unsigned hz,unsigned logical_hz,unsigned *canonical)
{
    assert(hz%logical_hz==0u);
    redraw_multiplier=hz/logical_hz;
    field_count=phase=xpos=phase_event_count=0u;
    presented_on_event=transitions_started=transitions_done=extra_host_redraws=0u;
    mjp_slide_dir=1;
    mjp_transition(AVP_MJP_PAGE_BLANK,0u);
    assert(transitions_started==1u&&transitions_done==1u);
    assert(field_count==40u&&phase_event_count==24u);
    assert(presented_on_event==0u);
    assert(extra_host_redraws==40u*(redraw_multiplier-1u));
    for(unsigned f=0u;f<40u;++f){
        unsigned expected_stage=f<8u?0u:1u+(f-8u)/2u;
        /* Stage n is produced at x = 0x12 + 4*(n+8) in the +1 path. */
        unsigned expected_x=0x12u+4u*(f<8u?f:expected_stage+8u);
        assert(field_phase[f]==expected_stage);
        assert(field_x[f]==expected_x);
        if(!canonical)continue;
        assert(field_phase[f]==canonical[f]);
    }
}
int main(int argc,char **argv)
{
    unsigned expected[40];
    if(argc!=1 && (argc!=3 || strcmp(argv[1],"--csv")!=0)){
        fprintf(stderr,"usage: %s [--csv timeline.csv]\n",argv[0]);return 2;
    }
    if(argc==3){timeline=fopen(argv[2],"w");if(!timeline){perror(argv[2]);return 2;}
        fputs("field,host_stage,picture_x\n",timeline);}
    one_second_ticks=60u;
    run(60u,60u,NULL);
    memcpy(expected,field_phase,sizeof(expected));
    run(120u,60u,expected);
    run(240u,60u,expected);
    run(480u,60u,expected);
    /* A PAL logical field is 20 ms, regardless of extra host redraws. */
    one_second_ticks=50u;
    run(50u,50u,expected);
    run(100u,50u,expected);
    run(200u,50u,expected);
    assert(avp_pc_mjp_next_deadline(0u,100u,50u)==20000100ull);
    assert(avp_pc_mjp_next_deadline(0u,100u,60u)==16666767ull);
    if(timeline)fclose(timeline);
    puts("PASS: R101 PC modeled CROSS 8+16x2 logical fields; no extra MJP event presentation at 60/120/240/480 Hz, PAL 50/100/200 Hz");
    puts("LIMIT: source-level host cadence regression only; GPU/Blitter/OP hardware timing remains unverified");
    return 0;
}
