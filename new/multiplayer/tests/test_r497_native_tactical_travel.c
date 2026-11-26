/* Dry subset of R496's recorded SL5 geometry. Opponents stationary;
 * unrecorded surrounding AMP/acid positions are intentionally absent. */
#define RETAIL_REPLAY
#define SafePos fixture_unused_safe_pos
#define AMPCollisions fixture_unused_amp_collisions
#define main r464_fixture_main
#include "../../pc_video/tests/test_analog_velocity_r464.c"
#undef main
#undef SafePos
#undef AMPCollisions
#define main r492_command_fixture_main
#include "test_r492_peer_melee.c"
#undef main
extern void avp_player_host_analog_aim(int,float,float,float);
extern void avp_player_host_aim_options(int,int);
extern void avp_player_host_live_heading(int,u32);
static AvpMpBotObservation v;
static unsigned peer_blocks;
#ifdef RETAIL_REPLAY
static uint8_t retail[8+64*64*8];
extern u8 r492_retail_safe_pos(u8,s32,s32,u16);
extern void avp_collision_bind_maze(u8 *,u16,u16);
extern void InitDoors(void);
extern u8 fullbits[256];
u32 levfx_ID;
#endif
u8 SafePos(u8 h,s32 px,s32 py,u16 w){
#ifdef RETAIL_REPLAY
    if(v.map.cells==retail+8)return r492_retail_safe_pos(h,px,py,w)==AVP_COLL_SAFE &&
        avp_mp_move_guard_floor(retail+8,64,64,px,py,40u<<9)?AVP_COLL_SAFE:1;
#endif
    (void)h;(void)w;unsigned x=(uint32_t)px>>16,y=(uint32_t)py>>16;
    return x<W&&y<W&&cells[(y*W+x)*8+5]?AVP_COLL_SAFE:0;
}
int AMPCollisions(s32 px,s32 py){
    AvpMpPeerCollisionView peers={0};peers.count=1;peers.active[0]=1;
    peers.x[0]=v.nearby_peer_x;peers.y[0]=v.nearby_peer_y;peers.from_x=x_pos;peers.from_y=y_pos;
    int blocked=!avp_mp_move_guard_peers(&peers,px,py);peer_blocks+=blocked;return blocked;
}
static void apply_native(AvpMpPlayerCommand *c){
    joy_cur=UINT32_MAX;unsigned a=c->digital_actions;
    if(a&(1u<<AVP_MP_FORWARD))joy_cur&=~(1u<<JOY_UP);
    if(a&(1u<<AVP_MP_BACKWARD))joy_cur&=~(1u<<JOY_DOWN);
    if(a&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT))){joy_cur&=~(1u<<FIRE_C);if(a&(1u<<AVP_MP_STRAFE_LEFT))joy_cur&=~(1u<<JOY_LEFT);if(a&(1u<<AVP_MP_STRAFE_RIGHT))joy_cur&=~(1u<<JOY_RIGHT);}
    avp_player_host_aim_options(1,0);avp_player_host_analog_aim(1,c->analog_turn,c->analog_turn_sensitivity,c->analog_turn_speed);
    avp_player_host_analog_motion((c->analog_axis_mask&3u)!=0,c->analog_forward,c->analog_strafe);
    sin_ang=sine((u16)(centre_angle>>16));cos_ang=sine((u16)((centre_angle>>16)+0x4000));MovePlayer();
}
static int replay(unsigned orientation,unsigned behavior){
    setup(&v,orientation);v.behavior=behavior;
    v.map=(AvpMpBotMap){retail+8,64,64,fullbits,NULL,10,NULL,NULL,0};
    v.player_species=AVP_MP_MARINE;v.player_energy=v.max_energy=1000;
    v.marine_unarmed=v.marine_inventory_valid=v.melee_visible=1;v.melee_threat=AVP_MP_BOT_THREAT_ALIEN;
    v.goal=AVP_MP_BOT_LIFT;v.hostile_target=0;v.target_visible=1;
    v.x=0x000a3613;v.y=0x00159d44;v.target_x=0x00098000;v.target_y=0x00158000;
    v.melee_x=0x0009dc25;v.melee_y=0x00158000;
    v.nearby_peer_x=0x000965b4;v.nearby_peer_y=0x001581c4;
    v.prop_count=2;v.props[0]=(AvpMpBotProp){v.nearby_peer_x,v.nearby_peer_y,AVP_MP_PEER_CLEARANCE};
    v.props[1]=(AvpMpBotProp){v.melee_x,v.melee_y,(50u<<9)*85u/100u};
    v.observed_world_level=5;v.heading=0x793b18b3u+orientation*0x20000000u;
    init(PT_HUMAN,v.heading>>16,0);avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    x_pos=v.x;y_pos=v.y;centre_angle=v.heading;peer_blocks=0;
    AvpMpBotState s={0};s.physical_valid=1;s.physical_level=5;s.physical_x=v.x;s.physical_y=v.y;
    uint32_t ox=v.x,oy=v.y;double maxdisp=0;unsigned chosen=0,t;
    for(t=1;t<=60;t++){
        AvpMpPlayerCommand c;v.tick=t;v.x=x_pos;v.y=y_pos;v.heading=centre_angle;
        v.goal=AVP_MP_BOT_LIFT;v.target_x=0x00098000;v.target_y=0x00158000;v.retreat_to_refuge=0;v.target_visible=1;
#ifndef BASELINE
        chosen+=avp_mp_bot_tactical_travel(&s,&v);
#endif
        avp_mp_bot_step_precise(&s,&v,0,&c);apply_native(&c);
        double d=hypot((double)x_pos-ox,(double)y_pos-oy)/65536.0;if(d>maxdisp)maxdisp=d;
        if(hypot((double)x_pos-v.melee_x,(double)y_pos-v.melee_y)>=2.0*65536.0 && maxdisp>=1.0)break;
    }
    printf("orientation=%u behavior=%u displacement=%.4f selected_ticks=%u ticks=%u peer_blocks=%u\n",orientation,behavior,maxdisp,chosen,t,peer_blocks);
    return t<=60;
}
int main(int argc,char **argv){
    if(argc!=3)return 2;FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    if(fread(retail,1,sizeof retail,f)!=sizeof retail){fclose(f);return 2;}fclose(f);
    unsigned opening=(unsigned)strtoul(argv[2],NULL,0);if(opening>128)return 2;
    retail[8+(21*64+10)*8+6]=(uint8_t)opening;
    InitDoors();avp_collision_bind_maze(retail+8,64,64);
    unsigned pass=0;for(unsigned i=0;i<8;i++)for(unsigned b=0;b<3;b++)pass+=replay(i,b);
    printf("Dry retail SL5 replay door opening=%u: %u/24 separated >=2 tiles from observed threat and moved >=1 tile\n",opening,pass);return 0;
}
