/* PC presentation regression: the next interpolation begins at the frame
 * actually shown for an Alien move-in, not the undrawn frame0 CPU pose.
 * The original Jaguar gameplay interpreter is never modified here. */
#include "jaguar/avp_gpu/software_renderer.h"
#include "mazescrn.h"
#include "player.h"
#include "amp.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

AvpAmp level1amps[AVP_NUM_AMPS];
s16 player_type=PT_ALIEN, cur_wepno=2;
s32 wep_x=24*65536L, wep_y=72*65536L;
s16 swing_x=0, swing_y=0;
static AvpFirstPersonOverlayState simulated;
u32 avp_amp_slot_generation(unsigned slot){(void)slot;return 1u;}
void avp_mazescrn_get_overlay_state(AvpFirstPersonOverlayState *out){*out=simulated;}

int main(void){
    AvpAvpGpuState *s=calloc(1,sizeof(*s));
    assert(s);
    simulated.overlay_visible=1;simulated.overlay_id=11;
    simulated.intro_preview=1;
    simulated.intro_x=24*65536L;simulated.intro_y=132*65536L;
    avp_gpu_snapshot_sim(s,NULL,0,0);
    assert(s->prev_overlay_species==PT_ALIEN && s->prev_overlay_weapon==2);
    assert(s->prev_wep_x==simulated.intro_x && s->prev_wep_y==simulated.intro_y);
    assert(wep_x==24*65536L && wep_y==72*65536L);
    /* After consuming the authored command, the CPU pose becomes the next
     * visible end of the interpolation; no phantom 72->132 drop occurs. */
    simulated.intro_preview=0;wep_x=24*65536L;wep_y=102*65536L;
    avp_gpu_snapshot_sim(s,NULL,0,0);
    assert(s->prev_wep_x==wep_x && s->prev_wep_y==wep_y);
    /* The user-verified R202 Marine path is deliberately not changed. */
    player_type=PT_HUMAN;cur_wepno=1;
    simulated.intro_preview=1;simulated.intro_y=90*65536L;
    wep_x=0;wep_y=0;
    avp_gpu_snapshot_sim(s,NULL,0,0);
    assert(s->prev_wep_x==0 && s->prev_wep_y==0);
    player_type=PT_PREDATOR;cur_wepno=4;
    simulated.intro_preview=1;wep_x=7*65536L;wep_y=9*65536L;
    avp_gpu_snapshot_sim(s,NULL,0,0);
    assert(s->prev_wep_x==wep_x && s->prev_wep_y==wep_y);
    free(s);
    return 0;
}
