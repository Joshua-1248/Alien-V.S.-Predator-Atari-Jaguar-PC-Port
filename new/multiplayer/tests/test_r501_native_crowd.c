/* R500 tick 200, SL5, player 13. Fixed observed actors; native wall and movement routines. */
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
    AvpMpPeerCollisionView peers={0};peers.from_x=x_pos;peers.from_y=y_pos;
    for(unsigned i=0;i<v.prop_count;i++){
        AvpMpBotProp *p=&v.props[i];
        if(p->halfwidth==AVP_MP_PEER_CLEARANCE){unsigned n=peers.count++;peers.active[n]=1;peers.x[n]=p->x;peers.y[n]=p->y;}
        else if(llabs((long long)px-p->x)<p->halfwidth && llabs((long long)py-p->y)<p->halfwidth)return 1;
    }
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
static unsigned near_acid_replay(unsigned orientation,unsigned behavior,unsigned goal, unsigned grid_only){
    memcpy(retail,original_retail,sizeof retail);InitDoors();cur_level=5;
    avp_doors_bind_maze(retail+8,64,64);avp_doors_bind_level_meta(retail_meta,AVP_MAX_LEVEL);
    avp_doors_bind_panel_ids(&ids);acs_level=10;retail[8+(21*64+10)*8+6]=128;
    uses=opened=0;setup(&v,orientation);v.behavior=behavior;
    v.map=(AvpMpBotMap){retail+8,64,64,fullbits,&retail_meta[4],10,risk,NULL,0};
    v.player_species=AVP_MP_MARINE;v.player_energy=v.max_energy=1000;
    v.marine_unarmed=v.marine_inventory_valid=v.melee_visible=1;v.melee_threat=AVP_MP_BOT_THREAT_ALIEN;
    v.goal=goal;v.hostile_target=0;v.target_visible=1;
    v.x=0x000a73e3;v.y=0x00159c5a;v.target_x=0x00098000;v.target_y=0x00158000;
    v.melee_x=0x000a1aaf;v.melee_y=0x001569e7;
    v.nearby_peer_x=0x000b567b;v.nearby_peer_y=0x0015e128;
    const AvpMpBotProp observed[]={
      {0x000a1aaf,0x001569e7,21760},{0x000b0026,0x00158000,21760},
      {0x000c30c8,0x00158000,21760},{0x000b8000,0x00164aa2,21760},
      {0x000bcb77,0x00178000,21760},{0x000b561a,0x0015380b,40960},
      {0x000b567b,0x0015e128,40960},{0x000cd752,0x00172510,40960}};
    v.prop_count=sizeof observed/sizeof *observed;memcpy(v.props,observed,sizeof observed);
    v.acid_count=0;memset(risk,0,sizeof risk);
    v.observed_world_level=5;v.heading=0x6b076a23u+orientation*0x20000000u;
    init(PT_HUMAN,v.heading>>16,0);avp_player_host_aim_options(0,0);avp_player_host_live_heading(0,0);
    x_pos=v.x;y_pos=v.y;centre_angle=v.heading;peer_blocks=0;
    if(grid_only){
        /* Diagnostic connectivity, no AI decisions, native wall/peer
         * predicate and fixed observed NPC squares. Sample 1/64 tile
         * within a 4x4 tile box centred on the recorded pose. Not a
         * proof of continuous-space impossibility or full live replay. */
        enum {G=257,M=128,N=G*G};
        static unsigned char seen[N];static unsigned queue[N];
        uint32_t ox=v.x,oy=v.y;double maxdisp=0,maxsep=0;
        for(unsigned scenario=0;scenario<3;scenario++){
            retail[8+(21*64+10)*8+6]=(unsigned char)(scenario*64);
            memset(seen,0,sizeof seen);unsigned head=0,tail=1;
            queue[0]=M*G+M;seen[queue[0]]=1;maxdisp=maxsep=0;
            while(head<tail){
                unsigned n=queue[head++];int ix=(int)(n%G),iy=(int)(n/G);
                s32 ax=(s32)ox+(ix-M)*1024,ay=(s32)oy+(iy-M)*1024;
                maxdisp=fmax(maxdisp,hypot((double)ax-ox,(double)ay-oy)/65536.0);
                maxsep=fmax(maxsep,hypot((double)ax-v.melee_x,(double)ay-v.melee_y)/65536.0);
                for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
                    int nx=ix+dx,ny=iy+dy;if((!dx&&!dy)||nx<0||ny<0||nx>=G||ny>=G)continue;
                    unsigned j=(unsigned)(ny*G+nx);if(seen[j])continue;
                    s32 bx=ax+dx*1024,by=ay+dy*1024;
                    x_pos=ax;y_pos=ay;
                    if(SafePos(80,bx,by,40)!=AVP_COLL_SAFE || AMPCollisions(bx,by) ||
                       SafePos(80,(ax+bx)/2,(ay+by)/2,40)!=AVP_COLL_SAFE || AMPCollisions((ax+bx)/2,(ay+by)/2))continue;
                    seen[j]=1;queue[tail++]=j;
                }
            }
            printf("door=%u reachable_grid_points=%u max_displacement=%.5f max_threat_separation=%.5f\n",scenario*64,tail,maxdisp,maxsep);
        }
        return 1;
    }
    AvpMpBotState s={0};s.physical_valid=1;s.physical_level=5;s.physical_x=v.x;s.physical_y=v.y;
    uint32_t ox=v.x,oy=v.y;double maxdisp=0;unsigned chosen=0,contact=0,t;
    for(t=1;t<=60;t++){
        AvpMpPlayerCommand c;v.tick=t;v.x=x_pos;v.y=y_pos;v.heading=centre_angle;
        v.goal=goal;v.target_x=0x00098000;v.target_y=0x00158000;v.retreat_to_refuge=0;v.target_visible=1;
        v.active_door_count=0;
        if(door1.ptr)v.active_door_offsets[v.active_door_count++]=(uint32_t)(door1.ptr-(retail+8));
        if(door2.ptr)v.active_door_offsets[v.active_door_count++]=(uint32_t)(door2.ptr-(retail+8));
        chosen+=avp_mp_bot_tactical_travel(&s,&v);
        avp_mp_bot_step_precise(&s,&v,0,&c);apply_native(&c);doors_tick(&c);
        contact+=v.acid_count && llabs((long long)x_pos-v.acid[0].x)<=(30u<<9) && llabs((long long)y_pos-v.acid[0].y)<=(30u<<9);
        double d=hypot((double)x_pos-ox,(double)y_pos-oy)/65536.0;if(d>maxdisp)maxdisp=d;
        if(hypot((double)x_pos-v.melee_x,(double)y_pos-v.melee_y)>=2.0*65536.0 && maxdisp>=1.0)break;
    }
    printf("goal=%u orientation=%u behavior=%u displacement=%.4f selected_ticks=%u ticks=%u acid_ticks=%u\n",goal,orientation,behavior,maxdisp,chosen,t,contact);
    return t<=60 && !contact;
}
int main(int argc,char **argv){
    if(argc!=2 && argc!=3)return 2;
    AvpRomImage rom={0};rom.size=4194304;rom.data=malloc(rom.size);rom.exact_retail_sha256=1;
    FILE *f=fopen(argv[1],"rb");if(!f||!rom.data||fread(rom.data,1,rom.size,f)!=rom.size||fgetc(f)!=EOF)return 2;fclose(f);
    uint8_t *decoded=NULL;size_t size=0;
    if(!avp_resources_decode_file(&rom,91u,NULL,&decoded,&size)||size!=sizeof original_retail)return 2;
    memcpy(original_retail,decoded,size);free(decoded);
    if(!avp_resources_retail_panel_info(&rom,5,&ids,panel_list,NULL))return 2;
    free(rom.data);memcpy(retail,original_retail,sizeof retail);InitDoors();avp_collision_bind_maze(retail+8,64,64);
    if(argc==3 && !strcmp(argv[2],"--grid")){near_acid_replay(0,0,AVP_MP_BOT_LIFT,1);return 0;}
    const unsigned goals[]={AVP_MP_BOT_LIFT};
    unsigned total=0,pass=0;
    for(unsigned k=0;k<sizeof goals/sizeof *goals;k++){
        unsigned n=0;for(unsigned i=0;i<8;i++)for(unsigned b=0;b<3;b++)n+=near_acid_replay(i,b,goals[k],0);
        printf("goal %u dry retreat: %u/24\n",goals[k],n);total+=24;pass+=n;
    }
    printf("%u/%u dry retail-map retreats from observed crowd\n",pass,total);
    return pass==total || argc==3?0:1;
}
