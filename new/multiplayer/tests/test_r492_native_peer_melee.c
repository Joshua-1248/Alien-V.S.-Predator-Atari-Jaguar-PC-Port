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
    peers.x[0]=v.target_x;peers.y[0]=v.target_y;peers.from_x=x_pos;peers.from_y=y_pos;
    int blocked=!avp_mp_move_guard_peers(&peers,px,py);peer_blocks+=blocked;return blocked;
}
static void apply_native(AvpMpPlayerCommand *c){
    joy_cur=UINT32_MAX;unsigned a=c->digital_actions;
    if(a&(1u<<AVP_MP_FORWARD))joy_cur&=~(1u<<JOY_UP);
    if(a&(1u<<AVP_MP_BACKWARD))joy_cur&=~(1u<<JOY_DOWN);
    if(a&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT))){joy_cur&=~(1u<<OPTION);if(a&(1u<<AVP_MP_STRAFE_LEFT))joy_cur&=~(1u<<JOY_LEFT);if(a&(1u<<AVP_MP_STRAFE_RIGHT))joy_cur&=~(1u<<JOY_RIGHT);}
    avp_player_host_aim_options(1,0);avp_player_host_analog_aim(1,c->analog_turn,c->analog_turn_sensitivity,c->analog_turn_speed);
    avp_player_host_analog_motion((c->analog_axis_mask&3u)!=0,c->analog_forward,c->analog_strafe);
    sin_ang=sine((u16)(centre_angle>>16));cos_ang=sine((u16)((centre_angle>>16)+0x4000));MovePlayer();
}
static int run(unsigned orient,unsigned behavior){
    setup(&v,orient);v.behavior=behavior;
#ifdef RETAIL_REPLAY
    /* Exact R491 tick-34 coordinates; retail static walls/door bytes, no
     * reconstruction of the unlogged other AMP or live door state. */
    v.map=(AvpMpBotMap){retail+8,64,64,fullbits,NULL,10,NULL,NULL,0};
    v.x=orient?0x0028cbe7:0x002850df;v.y=orient?0x0016a420:0x0017466a;
    v.target_x=orient?0x002850df:0x0028cbe7;v.target_y=orient?0x0017466a:0x0016a420;
    v.heading=orient?0xa5922eb1:0x25922eb0;
    v.nearby_peer_x=v.target_x;v.nearby_peer_y=v.target_y;
    v.props[0]=(AvpMpBotProp){v.target_x,v.target_y,AVP_MP_PEER_CLEARANCE};
#endif
    init(PT_ALIEN,v.heading>>16,0);avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    x_pos=v.x;y_pos=v.y;centre_angle=v.heading;peer_blocks=0;
    uint32_t ox=x_pos,oy=y_pos;AvpMpBotState s={0};unsigned align=0,attack=0,t;
    for(t=1;t<=80;t++){
        AvpMpPlayerCommand c;v.tick=t;v.x=x_pos;v.y=y_pos;v.heading=centre_angle;
        avp_mp_bot_step_precise(&s,&v,0,&c);
#ifndef BASELINE
        align+=s.peer_melee_align!=0;
#endif
        if(c.actions&ATTACKS){attack=1;break;}
        apply_native(&c);
        assert(llabs((long long)x_pos-v.target_x)>=AVP_MP_PEER_CLEARANCE || llabs((long long)y_pos-v.target_y)>=AVP_MP_PEER_CLEARANCE);
    }
    printf("orientation=%u behavior=%u attack=%u ticks=%u align=%u peer_blocks=%u displacement=%.4f\n",orient,behavior,attack,t,align,peer_blocks,hypot((double)x_pos-ox,(double)y_pos-oy)/65536.0);
    return attack;
}
#ifdef RETAIL_REPLAY
int main(int argc,char **argv){
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    if(fread(retail,1,sizeof retail,f)!=sizeof retail){fclose(f);return 2;}fclose(f);
    InitDoors();avp_collision_bind_maze(retail+8,64,64);
    unsigned pass=0;for(unsigned o=0;o<2;o++)for(unsigned b=0;b<3;b++)pass+=run(o,b);
    printf("Retail wall replay: %u/6 reached attack command\n",pass);
#ifndef BASELINE
    return pass!=6;
#else
    return 0;
#endif
}
#else
int main(void){unsigned pass=0;for(unsigned o=0;o<8;o++)for(unsigned b=0;b<3;b++)pass+=run(o,b);printf("Native peer alignment: %u/24 reached attack command\n",pass);
#ifndef BASELINE
    return pass!=24;
#else
    return 0;
#endif
}

#endif
