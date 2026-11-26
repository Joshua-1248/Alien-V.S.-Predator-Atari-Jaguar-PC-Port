/* R297: measure the REAL AMP chase displacement, not a cloned calculator.
 * Enemy AMP.S full X+Y step is the Jaguar baseline; opt-in corrects only
 * successful diagonal movement. Player controls and projectile code are not
 * involved. Test NPC types include special chase modes (Queen, Human).
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "amp.h"
#include "player.h"
#include "collision.h"
#include "hud.h"
#include "avp_runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level;
u32 ccn_xsave,ccn_ysave,x_pos,y_pos,fire_distance;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
s32 score;
u16 invisflag;
s16 player_energy,player_type,use_cocoon;
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
u16 avp_random(void){return 1;}

/* Range boundary confirmed against retail $163d0/$17e66. */
int main(void){
    const s32 q=0x58000;unsigned cases=0;
    for(int normalized=0;normalized<2;normalized++)for(int movement=0;movement<2;movement++){
        avp_amp_set_normalize_diagonal_contact_range(normalized);
        avp_amp_set_normalize_diagonal_enemy_movement_speed(movement);
        for(int sx=-1;sx<=1;sx+=2)for(int sy=-1;sy<=1;sy+=2){
            InitAMPs();build_level();AvpAmp*a=amp_req();
            a->creature=AC_AQUEEN;a->xpos=a->ypos=q;a->energy=a->oldenergy=3000;
            a->xvel=a->yvel=(0x2200*85)/100;
            x_pos=(u32)(q+sx*(70<<9));y_pos=(u32)(q+sy*(70<<9));
            lockxxx(a);a->timer=0;a->mode(a);
            assert(a->xpos==q && a->ypos==q);assert(a->animseq>=AS_FIGHT1);++cases;
            /* Just beyond the square, the original pursuit still moves. */
            InitAMPs();build_level();a=amp_req();
            a->creature=AC_AQUEEN;a->xpos=a->ypos=q;a->energy=a->oldenergy=3000;
            a->xvel=a->yvel=(0x2200*85)/100;
            x_pos=(u32)(q+sx*(91<<9));y_pos=(u32)(q+sy*(70<<9));
            lockxxx(a);a->timer=0;a->mode(a);
            assert(a->xpos!=q || a->ypos!=q);++cases;
        }
    }
    printf("PASS %u Queen original approach gate cases (four directions, both enhancements)\n",cases);return 0;
}
