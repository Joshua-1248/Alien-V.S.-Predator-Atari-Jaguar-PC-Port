/* Recorded peer separation with controlled open floor and actual MovePlayer.
 * Peer clearance uses the production multiplayer guard. Not a hit/damage test. */
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
static int run(unsigned orient,unsigned behavior){
    setup(&v,orient);v.behavior=behavior;
    v.player_species=AVP_MP_MARINE;v.player_energy=v.max_energy=1000;
    v.goal=AVP_MP_BOT_PATROL;v.hostile_target=v.target_visible=0;
    v.marine_unarmed=1;v.target_slot=AVP_NUM_AMPS;v.heading=0xbecacb39u+orient*0x20000000u;
    v.x=0x384efu;v.y=0x3d9a0u;v.nearby_peer_x=0x38000u;v.nearby_peer_y=0x48000u;
    v.target_x=0x38000u;v.target_y=0x58000u;
#ifdef RETAIL_REPLAY
    v.map=(AvpMpBotMap){retail+8,64,64,fullbits,NULL,10,NULL,NULL,0};
    v.x=0x001f84ef;v.y=0x000dd9a0;v.nearby_peer_x=0x001f8000;v.nearby_peer_y=0x000e8000;
    v.target_x=0x001d8000;v.target_y=0x002b8000;
#endif
    v.props[0]=(AvpMpBotProp){v.nearby_peer_x,v.nearby_peer_y,AVP_MP_PEER_CLEARANCE};
    init(PT_HUMAN,v.heading>>16,0);avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    x_pos=v.x;y_pos=v.y;centre_angle=v.heading;peer_blocks=0;
    uint32_t ox=x_pos,oy=y_pos;AvpMpBotState s={0};unsigned unblock=0,t,completed=0;
    s.physical_valid=1;s.physical_level=v.observed_world_level;s.physical_x=v.x;s.physical_y=v.y;
    s.objective_requested=1;s.objective_stall=6;
    double maxdisp=0;
    for(t=1;t<=100;t++){
        AvpMpPlayerCommand c;v.tick=t;v.x=x_pos;v.y=y_pos;v.heading=centre_angle;
        avp_mp_bot_step_precise(&s,&v,0,&c);
#ifndef BASELINE
        unblock+=s.peer_unblock!=0;
#endif
        apply_native(&c);
        assert(llabs((long long)x_pos-v.nearby_peer_x)>=AVP_MP_PEER_CLEARANCE || llabs((long long)y_pos-v.nearby_peer_y)>=AVP_MP_PEER_CLEARANCE);
        double d=hypot((double)x_pos-ox,(double)y_pos-oy)/65536.0;if(d>maxdisp)maxdisp=d;
        #ifdef RETAIL_REPLAY
        if((x_pos>>16)==30u && (y_pos>>16)==13u &&
           hypot((double)x_pos-v.nearby_peer_x,(double)y_pos-v.nearby_peer_y)>=1.3*65536.0){completed=1;break;}
#else
        if(maxdisp>=0.5){completed=1;break;}
#endif
    }
    printf("heading=%u behavior=%u displacement=%.4f ticks=%u unblock=%u peer_blocks=%u\n",orient,behavior,maxdisp,t,unblock,peer_blocks);
    return completed;
}
int main(int argc,char **argv){
#ifdef RETAIL_REPLAY
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    if(fread(retail,1,sizeof retail,f)!=sizeof retail){fclose(f);return 2;}fclose(f);
    InitDoors();avp_collision_bind_maze(retail+8,64,64);
#else
    (void)argc;(void)argv;
#endif
    unsigned pass=0;for(unsigned o=0;o<8;o++)for(unsigned b=0;b<3;b++)pass+=run(o,b);
    #ifdef RETAIL_REPLAY
    printf("Native peer-clearance: %u/24 reached adjacent retreat tile with >=1.3 tiles peer separation\n",pass);
#else
    printf("Native peer-clearance: %u/24 achieved half-tile displacement\n",pass);
#endif
#ifndef BASELINE
    return pass!=24;
#else
    return 0;
#endif
}
