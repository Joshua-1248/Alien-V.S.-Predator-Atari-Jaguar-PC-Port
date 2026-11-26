/* R267: keep the last authored SELECT briefing frame across End_Select.
 * The commercial ROM is not needed: the two portrait colors are synthetic.
 */
#include "jaguar/avp_gpu/frontend_renderer.h"
#include "jaguar/avp_gpu/state.h"
#include "jaguar/framebuffer.h"
#include "mjp.h"
#include "player.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char mjp_games[4]="xxx";

int main(void)
{
    AvpAvpGpuState *s=calloc(1u,sizeof(*s));
    AvpJaguarFramebuffer fb={0};
    u32 *before;
    const size_t count=320u*254u;
    const size_t bytes=count*sizeof(u32);
    const size_t image_bytes=320u*240u*3u;
    assert(s && avp_jaguar_framebuffer_init(&fb,320u,254u));
    before=malloc(bytes);
    assert(before);
    avp_gpu_frontend_enter_mode(s,AVP_MJP_SELECT);
    /* Marine carousel card (red) and expanded detail (green) must never
     * become interchangeable during the final PRESS FIRE handoff. */
    for(unsigned role=3u;role<=4u;role++){
        u8 *image=calloc(image_bytes,1u);
        assert(image);
        for(size_t p=0;p<image_bytes;p+=3u)
            image[p+(role==3u?0u:1u)]=224u;
        s->frontend_img[AVP_MJP_SELECT][role]=(AvpGpuRgbImage){320,240,image,NULL,NULL};
        s->frontend_loaded[AVP_MJP_SELECT][role]=1u;
    }
    for(unsigned i=0u;i<3u;i++){
        s->frontend_select_pose_valid[i]=1u;
        s->frontend_select_pose_species[i]=PT_HUMAN;
        s->frontend_select_pose_scale[i]=16;
        s->frontend_select_pose_x[i]=24+(int)i*90;
        s->frontend_select_pose_y[i]=38;
    }
    s->frontend_selected_species=PT_HUMAN;
    s->frontend_select_detail=1u;
    s->frontend_dim_active=1u;
    s->frontend_dim_phase=7u;
    avp_gpu_frontend_select_briefing(s,"MARINE\0\0","LOADING SIMULATION...");
    avp_gpu_frontend_select_skip_briefing(s);
    avp_gpu_frontend_select_prompt(s,"PRESS FIRE");
    avp_gpu_frontend_present(s,&fb,AVP_MJP_SELECT,0);
    memcpy(before,fb.pixels,bytes);
    assert(before[60u*320u+160u]!=0u);
    avp_gpu_frontend_event(s,0x53454e44u,0,0); /* LEVELS::ScreenOff SEND */
    avp_gpu_frontend_present(s,&fb,AVP_MJP_SELECT,0);
    assert(memcmp(before,fb.pixels,bytes)==0 && "End_Select flashed carousel instead of retaining last briefing");
    for(unsigned i=0u;i<5u;i++){
        avp_gpu_frontend_event(s,0x5449434bu,0,0);
        avp_gpu_frontend_present(s,&fb,AVP_MJP_SELECT,0);
        assert(memcmp(before,fb.pixels,bytes)==0 && "post-select fade wait changed the retained frame");
    }
    /* The old frame is then replaced by normal gameplay; entering SELECT
     * later must still create a fresh carousel without stale details. */
    avp_gpu_frontend_enter_mode(s,AVP_MJP_TITLE);
    avp_gpu_frontend_enter_mode(s,AVP_MJP_SELECT);
    assert(s->frontend_select_detail==0u);
    assert(!s->frontend_select_brief_active);
    for(unsigned role=3u;role<=4u;role++)free(s->frontend_img[AVP_MJP_SELECT][role].rgb);
    free(before);
    avp_jaguar_framebuffer_destroy(&fb);
    free(s);
    puts("PASS: R267 original SELECT detail frame remains visible after SEND until gameplay handoff");
    return 0;
}
