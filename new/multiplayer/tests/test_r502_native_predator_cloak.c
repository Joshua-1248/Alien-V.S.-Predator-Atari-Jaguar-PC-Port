/* Native PlayerWeapons/OPTION edge and fade feedback, with a fixed visible target. */
#define main r464_original_main
#include "../../pc_video/tests/test_analog_velocity_r464.c"
#undef main
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <string.h>
volatile u32 joy_edge=UINT32_MAX;
void StopLoop(u32 h){(void)h;}
u32 TFX(unsigned id){(void)id;return 0;}
s16 new_wepno;
u16 cur_weps;
static unsigned run(int stuck){
    AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpPlayerCommand c;
    init(PT_PREDATOR,0,0);cur_weps=1u<<3;new_wepno=3;
    v.player_species=AVP_MP_PREDATOR;v.deathmatch=1;v.behavior=2;v.has_target=v.hostile_target=v.target_visible=1;
    v.goal=AVP_MP_BOT_HOSTILE;v.predator_owned=cur_weps;v.current_weapon=3;
    v.x=10u<<16;v.y=10u<<16;v.target_x=17u<<16;v.target_y=v.y;
    v.player_energy=v.max_energy=1000;v.dm_opponents_on_level=1;
    invis_act=stuck?0:255;invis_stat=stuck?0:2;invisflag=1;predo=predo_count=0;
    joy_cur=joy_edge=UINT32_MAX;
    unsigned first=0,cloak_edges=0;
    for(unsigned t=1;t<=12;t++){
        v.tick=t;v.invis_act=invis_act;v.invis_stat=invis_stat;v.invisflag=invisflag;
        avp_mp_bot_step_precise(&s,&v,0,&c);
        u32 old=joy_cur,pad=UINT32_MAX;
        if(c.digital_actions&(1u<<AVP_MP_PRED_CLOAK))pad&=~(1u<<OPTION);
        if(c.digital_actions&(1u<<AVP_MP_FIRE))pad&=~(1u<<FIRE_A);
        joy_edge=~(old^pad)|pad;joy_cur=pad;
        if(!(joy_edge&(1u<<OPTION)))cloak_edges++;
        PlayerWeapons();
        if(wep_fire && !first){first=t;assert(one_fire);if(!stuck)assert(!invis_act&&!invis_stat&&!invisflag);else assert(invisflag);}
    }
    printf("native stuck=%d first_fire=%u option_edges=%u final_flags=%u,%u,%u\n",stuck,first,cloak_edges,invis_act,invis_stat,invisflag);
    return first;
}
int main(void){unsigned a=run(0),b=run(1);assert(a>0&&a<8&&b==8);puts("PASS native cloak/attack input and stuck-flag fallback");return 0;}
