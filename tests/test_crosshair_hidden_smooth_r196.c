/* RE39 R196: the crosshair uses 10 Hz sway history even when the held weapon
 * has no visible first-person overlay. Applies to SP and per-player MP. */
#include "jaguar/avp_gpu/overlay_presentation.h"
#include "pc/pc_crosshair.h"
#include "player.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path)
{
    FILE *f=fopen(path,"rb");long n;char *s;
    assert(f&&fseek(f,0,SEEK_END)==0);
    n=ftell(f);assert(n>0&&fseek(f,0,SEEK_SET)==0);
    s=malloc((size_t)n+1u);assert(s);
    assert(fread(s,1,(size_t)n,f)==(size_t)n);
    s[n]='\0';fclose(f);return s;
}
static void check_binding(const char *path)
{
    char *s=read_file(path);
    const char *sp=strstr(s,"static void present_framebuffer(");
    const char *mp=strstr(s,"int avp_pc_video_local_mp_render_actor(");
    const char *mp_draw=strstr(s,"unsigned avp_pc_video_present_local_mp(");
    assert(sp&&mp&&mp_draw);
    /* Both SP and MP must get the interpolated sway, rather than gating
     * crosshair interpolation on whether an OVER picture was rendered. */
    assert(strstr(sp,"avp_gpu_crosshair_present_state(&v->gpu,&overlay,")!=NULL);
    assert(strstr(mp,"avp_gpu_crosshair_present_state(state,&crosshair_pose,")!=NULL);
    assert(strstr(mp,"v->local_mp_crosshair_swing_x[player]=crosshair_pose.swing_x;")!=NULL);
    assert(strstr(mp_draw,"int dx=v->local_mp_crosshair_swing_x[p];")!=NULL);
    assert(strstr(mp_draw,"sway && v->local_mp_crosshair_swing_enabled[p],dx")!=NULL);
    free(s);
}
static int crosshair_x(AvpFirstPersonOverlayState *p)
{
    unsigned frame[320u*228u]={0};
    int x=avp_pc_crosshair_x(320u,p->swing_enabled,p->swing_x);
    avp_pc_crosshair_draw(frame,320u,320u,228u,0,0,320u,228u,
                          p->swing_enabled,p->swing_x);
    assert(x>=0&&x<320);
    assert(frame[114u*320u+(unsigned)x]==0x00f0f0f0u);
    return x;
}
int main(int argc,char **argv)
{
    static AvpAvpGpuState gpu;
    AvpFirstPersonOverlayState p={0};
    assert(argc==2);check_binding(argv[1]);
    gpu.prev_overlay_snapshot_valid=1;
    gpu.prev_overlay_species=PT_HUMAN;
    gpu.prev_overlay_weapon=1;
    gpu.prev_overlay_visible=0;
    gpu.prev_overlay_id=-1;
    gpu.prev_swing_x=-10;gpu.prev_swing_y=-6;
    gpu.render_alpha=0.0;
    p.overlay_visible=0;p.overlay_id=-1;p.swing_enabled=1;
    p.swing_x=10;p.swing_y=6;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_HUMAN,1);
    assert(p.swing_x==-10&&p.swing_y==-6&&crosshair_x(&p)==150);
    gpu.render_alpha=0.5;
    p.swing_x=10;p.swing_y=6;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_HUMAN,1);
    assert(p.swing_x==0&&p.swing_y==0&&crosshair_x(&p)==160);
    gpu.render_alpha=1.0;
    p.swing_x=10;p.swing_y=6;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_HUMAN,1);
    assert(p.swing_x==10&&p.swing_y==6&&crosshair_x(&p)==170);
    /* Weapon object disappears or reappears during the same held weapon:
     * the crosshair still transitions smoothly, without showing artwork. */
    gpu.render_alpha=0.5;
    gpu.prev_overlay_visible=1;
    p.overlay_visible=0;p.swing_x=10;p.swing_y=6;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_HUMAN,1);
    assert(p.swing_x==0&&crosshair_x(&p)==160);
    gpu.prev_overlay_visible=0;
    p.overlay_visible=1;p.overlay_id=13;p.swing_x=10;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_HUMAN,1);
    assert(p.swing_x==0);
    /* Some weapon states keep OVER logically visible but swap or suppress
     * artwork. Crosshair smoothing follows the equipped weapon, not OP ID. */
    gpu.prev_overlay_visible=1;gpu.prev_overlay_id=10;
    p.overlay_visible=1;p.overlay_id=11;p.swing_x=10;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_HUMAN,1);
    assert(p.swing_x==0);
    /* No interpolation while smoothing off, or across weapon/species changes. */
    p.overlay_visible=0;p.swing_x=10;
    avp_gpu_crosshair_present_state(&gpu,&p,0,PT_HUMAN,1);
    assert(p.swing_x==10);
    p.swing_x=10;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_HUMAN,2);
    assert(p.swing_x==10);
    p.swing_x=10;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_PREDATOR,1);
    assert(p.swing_x==10);
    /* Large discontinuities remain unsmoothed (original 32-unit guard). */
    gpu.prev_swing_x=-60;p.swing_x=10;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_HUMAN,1);
    assert(p.swing_x==10);
    gpu.prev_swing_x=-10;
    gpu.prev_overlay_snapshot_valid=0;
    p.swing_x=10;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_HUMAN,1);
    assert(p.swing_x==10);
    gpu.prev_overlay_snapshot_valid=1;
    p.swing_x=10;p.swing_enabled=0;
    avp_gpu_crosshair_present_state(&gpu,&p,1,PT_HUMAN,1);
    assert(crosshair_x(&p)==160);
    puts("R196 hidden-overlay Smooth Presentation crosshair (SP/MP) PASS");
    return 0;
}
