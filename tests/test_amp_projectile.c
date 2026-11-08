#include "amp.h"
#include "player.h"
#include <assert.h>
#include <stddef.h>

#include "collision.h"
#include "hud.h"
#include "avp_runtime.h"
#include "avp_sfx_ids.h"
s16 cur_level=1,num_cocoons;
u32 ccn_xsave,ccn_ysave;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
s16 game_over,key_lock; u8 acs_level;
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *from,const AvpXY *to){(void)from;(void)to;return 0;}
s32 score;
u32 fire_distance;
u32 x_pos=0x120000,y_pos=0x340000;u16 invisflag=0x0800; volatile u16 seed0=1,seed1=2; s16 player_energy,player_type,use_cocoon;
s32 sin_d0(u16 a){(void)a;return 0;} s32 cos_d0(u16 a){(void)a;return 0x4000;}
u16 avp_random(void){u16 r=seed0;seed0=(u16)(seed0+1);return r;}
static unsigned effect_calls,last_effect;
static int blocked;
static void effect_sfx(void *u,unsigned id){(void)u;++effect_calls;last_effect=id;}
static int collision_stub(void *u,s32 x,s32 y,s16 height,u16 width)
{(void)u;(void)x;(void)y;(void)height;(void)width;return blocked?AVP_COLL_WALL:AVP_COLL_SAFE;}
static s32 tx,ty;static void chase(AvpAmp*a,s32 x,s32 y){(void)a;tx=x;ty=y;}
int main(void){AvpAmp*a;build_level();/* The slot may have belonged to a static AMP in a previous level.
 * Spawning a weapon must not inherit its host-only update suppression. */
 level1amps[0].host_static=1;
 level1amps[0].aux_ptr=(void*)&level1amps[1]; /* stale pointer from old occupant */
 a=player_weapon(0x4000,0,0,77,0,0);assert(a==&level1amps[0]);
 assert(a->host_static==0&&a->aux_ptr==NULL);assert(a->creature==AC_SMART&&a->yoffset==20);assert(a->xvel==-1&&a->yvel==77);assert((a->flags&(1u<<AMP_PLAYER))!=0);assert(a->timer==1);
 avp_amp_set_chase_callback(chase);a->flags=0;a->xvector=0;invisflag=1;lostplayer(a);assert(tx==0x7fff0000||tx==0);invisflag=0;tx=ty=0x1234;lostplayer(a);assert(a->mode==chase_player&&a->xvector==0);assert(tx==0x1234&&ty==0x1234);
 /* R61: a malformed/stationary projectile must not remain suspended and
  * occupy an AMP slot forever. Preserve normal delay and impact modes. */
 InitAMPs();build_level();avp_amp_set_reset_counter(77);invisflag=0;
 a=player_weapon(0,0,0,77,0,0);
 assert(a==&level1amps[0]&&a->xvector==0&&a->yvector==0);
 pre_discmove(a);assert(a->mode==discmove);
 discmove(a);assert(a->mode!=discmove);
 UpdateAMPs();assert(a->mode==NULL);
 assert(amp_req()==a);
 InitAMPs();build_level();avp_amp_set_reset_counter(77);
 a=player_weapon(0,0,1,77,0,0);a->timer=3;
 pre_flame_on(a);assert(a->mode==flame_on);
 flame_on(a);assert(a->timer==2&&a->mode==flame_on);
 flame_on(a);assert(a->timer==1&&a->mode==flame_on);
 flame_on(a);assert(a->timer==0&&a->mode!=flame_on);
 UpdateAMPs();assert(a->mode==NULL);
 assert(amp_req()==a);
 InitAMPs();build_level();avp_amp_set_reset_counter(77);
 a=player_weapon(0,0,2,77,0,0);
 pre_flame_on(a);flame_on(a);
 /* An impact-triggered bolt must explode and return its own slot. */
 assert(a->mode==NULL);
 assert(amp_req()==a);
 /* R62: a Marine volley remains active if its first flame expires while
  * another flame is still waiting out its original launch delay. */
 InitAMPs();build_level();avp_amp_set_reset_counter(77);
 {
     AvpAmp *first=player_weapon(0,0,1,77,0,0);
     AvpAmp *second=player_weapon(0,0,1,77,0,0);
     second->timer=3;
     pre_flame_on(second);flame_on(second);
     assert(discflag==1 && second->mode==flame_on && second->timer==2);
     first->timer=0;
     pre_flame_on(first);flame_on(first);
     assert(first->mode!=flame_on);
     first->mode(first);
     assert(first->mode==NULL);
     assert(second->mode==flame_on);
     /* R272: AMP.S:4276-4278 .killflame2 clears discflag unconditionally;
      * the waiting flame's flame_on (AMP.S:4230) sets it again next tick. */
     assert(discflag==0);
     flame_on(second);
     assert(discflag==1);
     second->timer=0;
     flame_on(second);
     assert(second->mode!=flame_on);
     second->mode(second);
     assert(discflag==0);
 }
 /* The actual Smart Disc collision paths must request their original
  * contact cues through the normal runtime sound bridge (not just the
  * throw sound asserted by the species-weapon command-cycle test). */
 {
     AvpRuntimeOps ops={0};
     AvpAmp *victim,*disc;
     ops.play_sfx=effect_sfx;ops.safe_pos=collision_stub;
     avp_runtime_bind(&ops);
     InitAMPs();build_level();
     blocked=0;effect_calls=0;last_effect=0;
     victim=amp_req();victim->xpos=(s32)x_pos;victim->ypos=(s32)y_pos;
     victim->mode=discmove;victim->flags=(1u<<AMP_KILLABLE);
     victim->energy=100;amp_setgrid(victim);
     disc=amp_req();disc->xpos=victim->xpos;disc->ypos=victim->ypos;
     disc->creature=AC_SMART;disc->flags=(1u<<AMP_PLAYER);
     disc->xvel=-1;disc->xvector=512;disc->yvector=0;
     disc->mode=discmove;
     discmove(disc);
     assert(victim->energy==50 && last_effect==AVP_FX_QHIT && effect_calls==1);
     assert(disc->mode!=discmove);
     InitAMPs();build_level();
     blocked=1;effect_calls=0;last_effect=0;
     disc=amp_req();disc->xpos=(s32)x_pos;disc->ypos=(s32)y_pos;
     disc->creature=AC_SMART;disc->flags=(1u<<AMP_PLAYER);
     disc->xvel=-1;disc->xvector=512;disc->yvector=0;
     disc->mode=discmove;
     discmove(disc);
     assert(last_effect==AVP_FX_DSK1A && effect_calls==1);
     avp_runtime_bind(NULL);
 }
 /* R256: original AMP.S performs 32-bit ADD.L/SUB.L for projectile
  * coordinates.  Compare the actual player_weapon spawn against unsigned
  * register arithmetic for every kind and extreme signed coordinate. */
 {
     static const u32 starts[]={0u,1u,0x7fffffffu,0x80000000u,
                                  0xfffffffeu,0xffffffffu,0x40000000u};
     static const s32 headings[]={0,0x4000,-0x4000,INT32_MAX,INT32_MIN};
     static const s32 offset_angles[]={0,1,-1,INT32_MAX,INT32_MIN};
     for(unsigned kind=0;kind<3u;++kind)
     for(unsigned i=0;i<sizeof(starts)/sizeof(starts[0]);++i)
     for(unsigned j=0;j<sizeof(headings)/sizeof(headings[0]);++j)
     for(unsigned k=0;k<sizeof(offset_angles)/sizeof(offset_angles[0]);++k){
         u32 expected_x,expected_y;
         s32 sn=offset_angles[k],cs=offset_angles[(k+2u)%5u];
         InitAMPs();build_level();
         x_pos=starts[i];y_pos=starts[(i+3u)%7u];
         a=player_weapon(headings[j],headings[(j+1u)%5u],(u16)kind,77,sn,cs);
         expected_x=x_pos;expected_y=y_pos;
         if(kind==2u){expected_x-=(u32)(sn>>2);expected_y-=(u32)(cs>>2);}
         expected_x+=(u32)a->xvector;expected_y+=(u32)a->yvector;
         assert((u32)a->xpos==expected_x);
         assert((u32)a->ypos==expected_y);
     }
 }
 return 0;}
