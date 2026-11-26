/* RE39 R195: crosshair and first-person weapon share the SAME Smooth
 * Presentation interpolation; this must not modify Jaguar aim/gameplay.
 * The source integration check prevents a primitive-only false positive. */
#include "jaguar/avp_gpu/overlay_presentation.h"
#include "pc/pc_crosshair.h"
#include "player.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text(const char *path)
{
    FILE *f=fopen(path,"rb");long n;char *s;
    assert(f && fseek(f,0,SEEK_END)==0);
    n=ftell(f);assert(n>0 && fseek(f,0,SEEK_SET)==0);
    s=(char*)malloc((size_t)n+1u);assert(s);
    assert(fread(s,1,(size_t)n,f)==(size_t)n);
    s[n]='\0';fclose(f);return s;
}
static void check_production_binding(const char *host_path,const char *renderer_path)
{
    char *host=read_text(host_path),*render=read_text(renderer_path);
    const char *p=strstr(host,"static void present_framebuffer(");
    const char *q=p?strstr(p,"#ifdef AVP_LOCAL_MULTIPLAYER"):NULL;
    const char *call=p?strstr(p,"avp_gpu_crosshair_present_state(&v->gpu,&overlay,v->cfg.smooth_presentation,"):NULL;
    const char *draw=p?strstr(p,"avp_pc_crosshair_draw(v->present_pixels"):NULL;
    const char *r=strstr(render,"static void overlay_first_person(");
    const char *rend=r?strstr(r,"/* The world may use host resolution"):NULL;
    const char *interp=r?strstr(r,"avp_gpu_overlay_present_state(state,&st,smooth_presentation,player_type,cur_wepno);"):NULL;
    assert(p && q && call && draw && p<call && call<draw && draw<q);
    assert(r && rend && interp && r<interp && interp<rend);
    free(host);free(render);
}
static int rendered_crosshair_x(const AvpFirstPersonOverlayState *st)
{
    uint32_t frame[320u*228u]={0};
    avp_pc_crosshair_draw(frame,320u,320u,228u,0,0,320u,228u,
                          st->swing_enabled,st->swing_x);
    int x=avp_pc_crosshair_x(320u,st->swing_enabled,st->swing_x);
    assert(x>=0 && x<320 && frame[114u*320u+(unsigned)x]==0x00f0f0f0u);
    return x;
}
int main(int argc,char **argv)
{
    static AvpAvpGpuState state;
    const AvpFirstPersonOverlayState initial={
        .overlay_id=13,.overlay_visible=1,.swing_enabled=1,
        .x=2L<<16,.y=4L<<16,.swing_x=8,.swing_y=12
    };
    AvpFirstPersonOverlayState st;
    assert(argc==3);
    check_production_binding(argv[1],argv[2]);
    state.prev_overlay_snapshot_valid=1;
    state.prev_overlay_visible=1;
    state.prev_overlay_id=13;
    state.prev_overlay_species=PT_HUMAN;
    state.prev_overlay_weapon=1;
    state.prev_wep_x=-(2L<<16);
    state.prev_wep_y=0;
    state.prev_swing_x=-8;
    state.prev_swing_y=-12;
    /* Normal render interpolation at every host display frame: the crosshair
     * follows the exact offset of the first-person weapon (not raw swing). */
    state.render_alpha=0.0;
    st=initial;
    avp_gpu_overlay_present_state(&state,&st,1,PT_HUMAN,1);
    assert(st.swing_x==-8 && st.swing_y==-12 && rendered_crosshair_x(&st)==152);
    assert(st.x==state.prev_wep_x);
    state.render_alpha=0.5;
    st=initial;
    avp_gpu_overlay_present_state(&state,&st,1,PT_HUMAN,1);
    assert(st.swing_x==0 && st.swing_y==0 && rendered_crosshair_x(&st)==160);
    assert(st.x==0 && st.y==(2L<<16));
    state.render_alpha=1.0;
    st=initial;
    avp_gpu_overlay_present_state(&state,&st,1,PT_HUMAN,1);
    assert(st.swing_x==8 && st.swing_y==12 && rendered_crosshair_x(&st)==168);
    /* No smoothing: the original 10-Hz weapon/crosshair position wins. */
    state.render_alpha=0.5;
    st=initial;
    avp_gpu_overlay_present_state(&state,&st,0,PT_HUMAN,1);
    assert(st.swing_x==8 && rendered_crosshair_x(&st)==168);
    /* No interpolation between unrelated weapon sprites or intro previews. */
    st=initial;st.overlay_id=14;
    avp_gpu_overlay_present_state(&state,&st,1,PT_HUMAN,1);
    assert(st.swing_x==8 && st.x==initial.x);
    st=initial;st.intro_preview=1;
    avp_gpu_overlay_present_state(&state,&st,1,PT_HUMAN,1);
    assert(st.swing_x==8 && st.x==initial.x);
    state.prev_overlay_visible=0;
    st=initial;
    avp_gpu_overlay_present_state(&state,&st,1,PT_HUMAN,1);
    assert(st.swing_x==8);
    state.prev_overlay_visible=1;
    state.prev_swing_x=-40;
    st=initial;
    avp_gpu_overlay_present_state(&state,&st,1,PT_HUMAN,1);
    assert(st.swing_x==8); /* >32 jump: original snap/discontinuity policy */
    /* Alien same-weapon frame change is intentionally continuous. */
    state.prev_swing_x=-8;
    state.prev_overlay_species=PT_ALIEN;
    state.prev_overlay_weapon=2;
    st=initial;st.overlay_id=99;
    avp_gpu_overlay_present_state(&state,&st,1,PT_ALIEN,2);
    assert(st.swing_x==0 && st.x==0 && rendered_crosshair_x(&st)==160);
    st=initial;st.overlay_id=99;
    avp_gpu_overlay_present_state(&state,&st,1,PT_ALIEN,3);
    assert(st.swing_x==8); /* switched weapon: no cross-weapon drift */
    /* No effect on reticle location if per-player sway option is disabled. */
    st=initial;st.swing_enabled=0;
    assert(rendered_crosshair_x(&st)==160);
    puts("R195 P1 Smooth Presentation crosshair/weapon interpolation PASS");
    return 0;
}
