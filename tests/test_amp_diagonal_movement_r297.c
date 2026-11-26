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

static int32_t travel(s16 species,int enabled,int dirx,int diry,s32 speed,s32 *dy_out)
{
    AvpAmp *a;
    const s32 base=0x00058000;
    avp_amp_set_normalize_diagonal_enemy_movement_speed(enabled);
    InitAMPs();build_level();
    a=amp_req();
    a->creature=species;
    a->mode=chase_player;
    a->animseq=AS_RUN;
    a->energy=a->oldenergy=100;
    a->xpos=base;
    a->ypos=base;
    a->xvel=(s16)speed;
    a->yvel=(s16)speed;
    a->yvector=0;
    a->timer=0;
    a->flags=0;
    x_pos=(u32)(base+dirx*0x20000);
    y_pos=(u32)(base+diry*0x20000);
    /* Human source AI skips every other update; test its actual moving tick. */
    chase_player(a);
    if(species==AC_HUMAN)chase_player(a);
    *dy_out=a->ypos-base;
    return a->xpos-base;
}
static s32 abs_s32(s32 x){return x<0?-x:x;}
/* Reject exactly the diagonal collision candidate. Confirm the real chase
 * falls back to its unscaled Jaguar X-axis motion even when enabled. */
static s32 fallback_x,fallback_y;
static int reject_diagonal(void *user,s32 x,s32 y,s16 height,u16 width)
{
    (void)user;(void)height;(void)width;
    return x!=fallback_x && y!=fallback_y ? AVP_COLL_WALL:AVP_COLL_SAFE;
}
static void test_blocked_diagonal_fallback(void)
{
    AvpRuntimeOps ops={0};
    AvpAmp *a;
    avp_amp_set_normalize_diagonal_enemy_movement_speed(1);
    InitAMPs();build_level();a=amp_req();
    a->creature=AC_ALIEN;a->mode=chase_player;
    a->energy=a->oldenergy=100;a->animseq=AS_RUN;
    a->xpos=fallback_x=0x00058000;
    a->ypos=fallback_y=0x00058000;
    a->xvel=a->yvel=0x1800;
    x_pos=0x00078000;y_pos=0x00078000;
    ops.safe_pos=reject_diagonal;
    avp_runtime_bind(&ops);
    chase_player(a);
    avp_runtime_reset_ops();
    assert(a->xpos==fallback_x+0x1800);
    assert(a->ypos==fallback_y);
}

int main(void)
{
    static const s16 species[]={AC_ALIEN,AC_PREDATOR,AC_HUMAN,
                                   AC_FACEHUG,AC_ACRAWL,AC_AQUEEN};
    static const s32 speeds[]={0x1600,0x1c00,0x1a00,0x1a00,0x1a00,0x2200};
    assert(avp_amp_normalize_diagonal_enemy_movement_speed()==0);
    avp_amp_set_attacks_disabled(1);
    for(unsigned i=0;i<sizeof(species)/sizeof(species[0]);i++){
        s32 speed=speeds[i],y=0,ax,ay;
        for(int sx=-1;sx<=1;sx+=2){
            for(int sy=-1;sy<=1;sy+=2){
                s32 x0,y0,x1,y1;
                /* Direction and each speed component exactly original OFF. */
                x0=travel(species[i],0,sx,sy,speed,&y0);
                assert(x0==sx*speed && y0==sy*speed);
                assert(!avp_amp_normalize_diagonal_enemy_movement_speed());
                /* On: quantized 1/sqrt2 per component, same 4 quadrants. */
                x1=travel(species[i],1,sx,sy,speed,&y1);
                const s32 component=(s32)((int64_t)speed*46341/65536);
                assert(x1==sx*component && y1==sy*component);
                assert((int64_t)x1*x1+(int64_t)y1*y1<=(int64_t)speed*speed+speed);
                assert((int64_t)x1*x1+(int64_t)y1*y1>=(int64_t)(speed-3)*(speed-3));
                assert(avp_amp_normalize_diagonal_enemy_movement_speed());
                /* Straight X/Y must be byte-for-byte equal with toggle OFF/ON. */
                ax=travel(species[i],0,sx,0,speed,&y);
                assert(ax==sx*speed && y==0);
                ax=travel(species[i],1,sx,0,speed,&y);
                assert(ax==sx*speed && y==0);
                ay=travel(species[i],0,0,sy,speed,&y);
                assert(ay==0 && y==sy*speed);
                ay=travel(species[i],1,0,sy,speed,&y);
                assert(ay==0 && y==sy*speed);
                assert(abs_s32(x1)<abs_s32(x0));
            }
        }
    }
    test_blocked_diagonal_fallback();
    avp_amp_set_normalize_diagonal_enemy_movement_speed(0);
    assert(!avp_amp_normalize_diagonal_enemy_movement_speed());
    puts("R297 real AMP chase: 6 enemy types x 4 quadrants, diagonal OFF/ON, axial parity and blocked fallback PASS");
    return 0;
}
