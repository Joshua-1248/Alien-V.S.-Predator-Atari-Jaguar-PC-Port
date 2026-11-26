/* RE41 R237 — replay R235 Jaguar initial credentials and require security-door
 * behavior for species even when stale multiplayer/save state lowers clearance.
 * Numeric locks still apply to Marines only; authored special restrictions stay. */
#include "doors.h"
#include "amp.h"
#include "avp_runtime.h"
#include "hud_message.h"
#include "joypad.h"
#include "player.h"
#include "avp_sfx_ids.h"
#include <stdio.h>
#include <string.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

static unsigned sound_count, last_sound, access_code, denied_count, accd_count, accg_count;
static const AvpHudMessageStep *notice;
static void sound_fx(void *u,unsigned id){(void)u; ++sound_count;last_sound=id;if(id==AVP_FX_ACCD)++accd_count;if(id==AVP_FX_ACCG)++accg_count;}
static const AvpRuntimeOps ops={.play_sfx=sound_fx};
const AvpRuntimeOps *avp_runtime_ops(void){return &ops;}
u32 new_pos,levfx_ID,x_pos,y_pos,centre_angle;
s16 player_type,cur_level,new_level;
s32 new_x,new_y;
volatile u32 joy_cur=0xffffffffu,joy_edge=0xffffffffu;
u8 panel_list[AVP_MAX_PANELS];
static AvpAmp amp_storage[AVP_NUM_AMPS];
AvpAmp *amp_data=amp_storage;
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
const AvpHudMessageStep avp_msg_pressure[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_notsecure[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_airlocked[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_jammed[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_jlift[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_escape[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_access_denied[]={{0,{0,0,0}}};
void avp_hudmsg_queue(const AvpHudMessageStep *p){notice=p;if(p==avp_msg_access_denied)++denied_count;}
void avp_hudmsg_set_access_code(unsigned n){access_code=n;}
u32 DoEffect(unsigned id){return id;}

#define CHECK(expr) do { if(!(expr)){fprintf(stderr,"R235 FAIL line %d: %s\n",__LINE__,#expr);return 1;}++checks; } while(0)
static unsigned checks;

int main(void)
{
    static u8 maze[64u*64u*8u];
    const unsigned cell=(10u*64u+10u)*8u;
    const unsigned door_off=cell+8u+6u;
    AvpDoorAccessEntry access[]={{(u16)door_off,7}};
    const AvpDoorLevelMeta meta={.access=access,.access_count=1u};
    const s16 species[]={PT_HUMAN,PT_ALIEN,PT_PREDATOR,PT_HUMAN};
    const unsigned clearances[]={0u,0u,0u,7u};
    unsigned i;

    avp_doors_bind_maze(maze,64u,64u);
    avp_doors_bind_panel_ids(NULL);
    cur_level=1;
    avp_doors_bind_level_meta(&meta,1u);
    for(i=0;i<4u;++i){
        memset(maze,0,sizeof maze);
        maze[cell+2u]=0x20u; /* valid door panel facing right */
        InitDoors();ResetAccess();
        player_type=species[i];
        InitAccess();
        if(player_type==PT_HUMAN)acs_level=(u8)clearances[i];
        CHECK(acs_level==(player_type==PT_HUMAN?clearances[i]:10u));
        x_pos=(10u<<16)|0x8000u;
        y_pos=(10u<<16)|0x8000u;
        centre_angle=0u;
        new_pos=cell;
        joy_cur=joy_edge=0xffffffffu & ~(1u<<FIRE_B);
        notice=NULL;denied_count=0;access_code=0;sound_count=0;last_sound=0;accd_count=accg_count=0;
        if(player_type==PT_ALIEN)avp_doors_pc_alien_use(1,1);
        else avp_doors_pc_alien_use(0,0);
        DoorKeys();
        avp_doors_pc_alien_use(0,0);
        if(i==0u){
            CHECK(notice==avp_msg_access_denied && denied_count==1u);
            CHECK(access_code==7u && accd_count==1u);
            CHECK(maze[door_off]==0u);
            MoveDoors();CHECK(maze[door_off]==0u);
        } else {
            CHECK(notice!=avp_msg_access_denied && denied_count==0u);
            CHECK(access_code==0u);
            MoveDoors();CHECK(maze[door_off]==4u);
            if(player_type==PT_HUMAN)CHECK(accg_count==1u);
            else CHECK(accg_count==0u);
        }
    }
    /* Jammed/unused restrictions are not security clearances: Aliens must
     * still encounter the authored special-door restrictions. */
    for(i=0;i<2u;++i){
        memset(maze,0,sizeof maze);maze[cell+2u]=0x20u;
        InitDoors();ResetAccess();
        access[0].code=i?AVP_ACC_UNUSED:AVP_ACC_JAMMED;
        player_type=PT_ALIEN;InitAccess();
        notice=NULL;denied_count=0;
        avp_doors_pc_alien_use(1,1);DoorKeys();avp_doors_pc_alien_use(0,0);
        CHECK(maze[door_off]==0u);
        CHECK(notice==(i?avp_msg_airlocked:avp_msg_jammed));
        CHECK(denied_count==0u);
        MoveDoors();CHECK(maze[door_off]==0u);
    }
    /* An unexpected restore can leave a non-Marine access credential below
     * what the Jaguar initializes. Security hatches remain usable by species. */
    for(i=0u;i<2u;++i){
        memset(maze,0,sizeof maze);maze[cell+2u]=0x20u;
        InitDoors();ResetAccess();access[0].code=7;
        player_type=i?PT_PREDATOR:PT_ALIEN;
        InitAccess();CHECK(acs_level==10u);
        acs_level=0u;notice=NULL;denied_count=0u;access_code=0u;
        x_pos=(10u<<16)|0x8000u;y_pos=(10u<<16)|0x8000u;
        centre_angle=0u;new_pos=cell;
        joy_cur=joy_edge=0xffffffffu & ~(1u<<FIRE_B);
        avp_doors_pc_alien_use(player_type==PT_ALIEN,1);
        DoorKeys();avp_doors_pc_alien_use(0,0);
        CHECK(notice!=avp_msg_access_denied && denied_count==0u);
        CHECK(access_code==0u);
        MoveDoors();CHECK(maze[door_off]==4u);
    }
    printf("R235 retail InitAccess security door behavior: %u checks PASS\n",checks);
    return 0;
}
