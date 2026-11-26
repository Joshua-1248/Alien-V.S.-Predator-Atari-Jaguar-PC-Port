/* Standalone: actual empty-Marine host selection, bot command, MovePlayer,
 * and existing corpse claim. Controlled open-floor and sight fixtures;
 * no synthetic loot injected into the live game. */
#define main r493_host_fixture_main
#include "test_r493_corpse_supply.c"
#undef main
#define main r493_analog_fixture_main
#include "../../pc_video/tests/test_analog_velocity_r464.c"
#undef main
extern void avp_player_host_analog_aim(int,float,float,float);
extern void avp_player_host_aim_options(int,int);
extern void avp_player_host_live_heading(int,u32);
static void native_command(const AvpMpPlayerCommand *c){
    unsigned a=c->digital_actions;joy_cur=UINT32_MAX;
    if(a&(1u<<AVP_MP_FORWARD))joy_cur&=~(1u<<JOY_UP);
    if(a&(1u<<AVP_MP_BACKWARD))joy_cur&=~(1u<<JOY_DOWN);
    if(a&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT))){
        joy_cur&=~(1u<<FIRE_C);
        if(a&(1u<<AVP_MP_STRAFE_LEFT))joy_cur&=~(1u<<JOY_LEFT);
        if(a&(1u<<AVP_MP_STRAFE_RIGHT))joy_cur&=~(1u<<JOY_RIGHT);
    }
    avp_player_host_aim_options(1,0);
    avp_player_host_analog_aim(1,c->analog_turn,c->analog_turn_sensitivity,c->analog_turn_speed);
    avp_player_host_analog_motion((c->analog_axis_mask&3u)!=0,c->analog_forward,c->analog_strafe);
    sin_ang=sine((u16)(centre_angle>>16));cos_ang=sine((u16)((centre_angle>>16)+0x4000));MovePlayer();
}
int main(void){
    AvpMpWalkHost *t=calloc(1,sizeof *t);AvpMpNativeWorld *w=calloc(1,sizeof *w);
    AvpMpActorNativeState *a=calloc(1,sizeof *a);assert(t&&w&&a);
    uint8_t bytes[8+8*24*24]={0},flags[256]={0};
    AvpMpRetailMap map={bytes,sizeof bytes,24,24,1,1};
    for(unsigned i=0;i<24*24;i++)bytes[8+i*8+5]=1;
    unsigned passed=0;
    for(unsigned h=0;h<8;h++)for(unsigned behavior=0;behavior<3;behavior++){
        memset(t,0,sizeof *t);memset(w,0,sizeof *w);memset(a,0,sizeof *a);
        t->actors=t->players=1;t->dm.enabled=1;w->map=&map;
        a->weapon.ammo_info[0].max=100;
        init(PT_HUMAN,h*8192u,0);avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
        x_pos=y_pos=0xa8000;
        assert(avp_mp_corpse_add(&t->corpses,1,0,0xc8000,0xa8000,0,42));
        AvpMpBotObservation v={0};v.map=(AvpMpBotMap){bytes+8,24,24,flags,NULL,0,NULL,NULL,0};
        v.deathmatch=1;v.marine_owned=2;v.marine_unarmed=1;v.marine_inventory_valid=1;
        v.player_energy=v.max_energy=1000;v.observed_world_level=1;v.behavior=behavior;
        int sight=1;AvpMpBotState state={0};unsigned step;
        for(step=0;step<80;step++){
            v.tick=100+step;v.x=x_pos;v.y=y_pos;v.heading=centre_angle;
            walk_bot_empty_marine(t,0,a,w,&v,visible,&sight);
            assert(v.goal==AVP_MP_BOT_SUPPLY && v.target_slot==WALK_BOT_CORPSE_SLOT_BASE);
            AvpMpPlayerCommand cmd;avp_mp_bot_step_precise(&state,&v,0,&cmd);native_command(&cmd);
            if(avp_mp_corpse_claim_at(&t->corpses,1,0,x_pos,y_pos)){passed++;break;}
        }
        printf("heading=%u behavior=%u claimed=%u steps=%u\n",h,behavior,step<80,step+1);
    }
    free(a);free(w);free(t);printf("Native corpse navigation: %u/24 reached actual claim tile\n",passed);return passed!=24;
}
