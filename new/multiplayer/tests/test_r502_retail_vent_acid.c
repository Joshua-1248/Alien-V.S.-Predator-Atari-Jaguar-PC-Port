/* Authenticated vent maps with controlled two-puddle crossings and native movement. */
/* Dry subset of R496's recorded SL5 geometry. Opponents stationary;
 * unrecorded surrounding AMP/acid positions are intentionally absent. */
#define RETAIL_REPLAY
#define SafePos fixture_unused_safe_pos
#define AMPCollisions fixture_unused_amp_collisions
#define lift_key r498_fixture_lift_key
#define main r464_fixture_main
#include "../../pc_video/tests/test_analog_velocity_r464.c"
#undef main
#undef lift_key
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
static uint8_t retail[8+64*64*8],original_retail[sizeof retail];
extern u8 r492_retail_safe_pos(u8,s32,s32,u16);
extern void avp_collision_bind_maze(u8 *,u16,u16);
extern void InitDoors(void);
extern u8 fullbits[256];
#include "hud_message.h"
#include "avp_resources.h"
#include "../../../new/jaguar/retail_data/door_retail_meta.c"
u32 levfx_ID;
u8 panel_list[AVP_MAX_PANELS];
s16 cur_level,new_level;
s32 new_x,new_y;
volatile u32 joy_edge;
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
AvpAmp *amp_data;
#define MSG(n) const AvpHudMessageStep avp_msg_##n[]={{0,{NULL,NULL,NULL}}}
MSG(pressure); MSG(notsecure); MSG(airlocked); MSG(jammed); MSG(jlift); MSG(escape); MSG(access_denied);
void avp_hudmsg_queue(const AvpHudMessageStep *m){(void)m;}
void avp_hudmsg_set_access_code(unsigned code){(void)code;}
u32 DoEffect(unsigned id){(void)id;return 1u;}
#include "../../../src/game/doors.c"
static AvpDoorPanelIds ids;
static unsigned uses,opened;
static void doors_tick(const AvpMpPlayerCommand *c){
    new_pos=((y_pos>>16)*64u+(x_pos>>16))*8u;
    joy_edge=joy_cur=UINT32_MAX;
    if(c->digital_actions&(1u<<AVP_MP_USE)){
        joy_edge=joy_cur=~(1u<<FIRE_B);uses++;
    }
    DoorKeys();MoveDoors();
    if(retail[8+(21*64+10)*8+6])opened=1;
}

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
static uint8_t risk[4096];
static unsigned X,Y,axis,span;
static uint32_t center(unsigned value){return (value<<16)|32768u;}
static uint32_t xx(unsigned i){return center(X+(axis==0?i:0));}
static uint32_t yy(unsigned i){return center(Y+(axis==1?i:0));}
static int choose_corridor(unsigned level,unsigned requested_axis){
    AvpMpBotMap m={retail+8,64,64,fullbits,&retail_meta[level-1],10,risk,NULL,0};
    axis=requested_axis;
    for(Y=1;Y<58;Y++)for(X=1;X<58;X++){
        span=4;int clear=1;
        memset(risk,0,sizeof risk);
        for(unsigned i=0;i<=span;i++){
            if(r492_retail_safe_pos(80,xx(i),yy(i),40)!=AVP_COLL_SAFE){clear=0;break;}
            if(i<span){AvpMpBotRoute r={0};
                if(!avp_mp_bot_route(&m,xx(i)>>16,yy(i)>>16,xx(i+1)>>16,yy(i+1)>>16,&r)||
                   r.next_x!=xx(i+1)>>16 || r.next_y!=yy(i+1)>>16 || r.requires_door_use || !r.door_is_open){clear=0;break;}
            }
        }
        if(!clear || avp_mp_bot_alien_death_passable(&m,xx(1),yy(1)) ||
           avp_mp_bot_alien_death_passable(&m,xx(2),yy(2)))continue;
        risk[(yy(1)>>16)*64+(xx(1)>>16)]=3u;risk[(yy(2)>>16)*64+(xx(2)>>16)]=3u;
        AvpMpBotRoute dry={0};if(avp_mp_bot_route(&m,xx(0)>>16,yy(0)>>16,xx(span)>>16,yy(span)>>16,&dry))continue;
        memset(risk,0,sizeof risk);return 1;
    }
    return 0;
}
static unsigned scenario(unsigned level,unsigned sp,unsigned heading,unsigned reverse){
    AvpMpBotState s={0};memset(&v,0,sizeof v);memset(risk,0,sizeof risk);
    v.map=(AvpMpBotMap){retail+8,64,64,fullbits,&retail_meta[level-1],10,risk,NULL,0};
    v.deathmatch=1;v.player_index=2;v.player_species=sp/4;v.player_energy=v.max_energy=1000;
    v.observed_world_level=level;v.has_target=1;v.goal=AVP_MP_BOT_SUPPLY;v.player_collision_disabled=1;
    const unsigned start=reverse?span:0,goal=reverse?0:span;
    init(sp,heading*8192,0);avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    x_pos=xx(start);y_pos=yy(start);v.target_x=xx(goal);v.target_y=yy(goal);
    v.acid_count=2;for(unsigned i=0;i<2;i++)v.acid[i]=(AvpMpBotAcid){xx(i+1),yy(i+1)};
    unsigned crossed=0,returned=0,entries=0,prior=0,exposure=0,flip_exposure=0;
    for(unsigned t=1;t<=500;t++){
        AvpMpPlayerCommand c;v.tick=t;v.x=x_pos;v.y=y_pos;v.heading=centre_angle;
        memset(risk,0,sizeof risk);for(unsigned i=0;i<2;i++)avp_mp_bot_acid_risk_mark(risk,v.acid[i].x,v.acid[i].y);
        avp_mp_bot_acid_route_memory(&s,&v,risk);
        if(!crossed && hypot((double)((int64_t)v.x-xx(goal))/65536.0,(double)((int64_t)v.y-yy(goal))/65536.0)<0.40)crossed=t;
        if(crossed){
            const unsigned dest=(t-crossed<60 && (t&1u))?goal:start;
            v.target_x=xx(dest);v.target_y=yy(dest);v.goal=dest==goal?AVP_MP_BOT_PATROL:AVP_MP_BOT_SUPPLY;
            if(t-crossed>=60 && hypot((double)((int64_t)v.x-xx(start))/65536.0,(double)((int64_t)v.y-yy(start))/65536.0)<0.40){returned=t;break;}
        }
        unsigned cur=0;for(unsigned i=0;i<2;i++)if(llabs((int64_t)v.x-v.acid[i].x)<=(30u<<9) && llabs((int64_t)v.y-v.acid[i].y)<=(30u<<9))cur|=1u<<i;
        entries+=!!(cur&~prior);prior=cur;exposure+=!!cur;
        if(crossed && t-crossed<60)flip_exposure+=!!cur;
        avp_mp_bot_step_precise(&s,&v,0,&c);apply_native(&c);
    }
    const unsigned pass=crossed && returned && entries==4 && !flip_exposure;
    if(!pass)printf("FAIL level=%u species=%u heading=%u reverse=%u crossed=%u returned=%u entries=%u contact=%u flip=%u xy=%.3f,%.3f\n",level,sp/4,heading,reverse,crossed,returned,entries,exposure,flip_exposure,x_pos/65536.0,y_pos/65536.0);
    return pass;
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    AvpRomImage rom={0};rom.size=4194304;rom.data=malloc(rom.size);rom.exact_retail_sha256=1;
    FILE *f=fopen(argv[1],"rb");if(!f||!rom.data||fread(rom.data,1,rom.size,f)!=rom.size||fgetc(f)!=EOF)return 2;fclose(f);
    unsigned pass=0,total=0,levels=0;
    for(unsigned level=6;level<=13;level++){
        uint8_t *decoded=NULL;size_t size=0;
        if(!avp_resources_decode_file(&rom,86u+level,NULL,&decoded,&size)||size!=sizeof retail)return 2;
        memcpy(retail,decoded,size);free(decoded);InitDoors();avp_collision_bind_maze(retail+8,64,64);
        for(unsigned a=0;a<2;a++){
        if(!choose_corridor(level,a)){printf("SKIP level=%u axis=%u: no selected four-edge straight isolated corridor\n",level,a);continue;}
        levels++;unsigned local=0;
        for(unsigned sp=0;sp<=8;sp+=8)for(unsigned h=0;h<8;h++)for(unsigned rev=0;rev<2;rev++)local+=scenario(level,sp,h,rev);
        printf("level=%u start=%u,%u axis=%u: %u/32\n",level,X,Y,axis,local);pass+=local;total+=32;
        }
    }
    free(rom.data);printf("%u/%u native retail vent return scenarios across %u selected map/axis pairs\n",pass,total,levels);
    return !levels || pass!=total;
}
