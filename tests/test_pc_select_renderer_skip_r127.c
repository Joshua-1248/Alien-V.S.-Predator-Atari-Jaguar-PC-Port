#include "jaguar/avp_gpu/frontend_renderer.h"
#include "mjp.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
int main(void)
{
    AvpAvpGpuState *s=(AvpAvpGpuState*)calloc(1u,sizeof(*s));
    assert(s);
    s->frontend_mode=AVP_MJP_SELECT;
    avp_gpu_frontend_select_briefing(s,"MARINE\0SCOUT\0\0","LOADING SIMULATION...");
    assert(s->frontend_select_total_chars==11u);
    assert(s->frontend_select_visible_chars==0u);
    assert(!avp_gpu_frontend_select_loading_ready(s));
    avp_gpu_frontend_event(s,0x5449434bu,0,0);
    assert(s->frontend_select_visible_chars==1u);
    avp_gpu_frontend_select_skip_briefing(s);
    assert(s->frontend_select_visible_chars==11u);
    assert(avp_gpu_frontend_select_loading_ready(s));
    avp_gpu_frontend_enter_mode(s,AVP_MJP_TITLE);
    assert(!avp_gpu_frontend_select_loading_ready(s));
    avp_gpu_frontend_select_skip_briefing(s);
    assert(!avp_gpu_frontend_select_loading_ready(s));
    free(s);
    puts("PASS: R127 SELECT skip reveals source text and bypasses synthetic hold; resets on scene exit");
    return 0;
}
