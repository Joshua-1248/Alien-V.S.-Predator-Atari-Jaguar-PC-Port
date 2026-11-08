/* R31: exercise actual DOORS.S translation with an isolated relocated
 * Map Maker table, including the original access, pressure, door movement,
 * and a return transition on an untouched level. No ROM or live game needed. */
#include "map_maker_playtest.h"
#include "doors.h"
#include "amp.h"
#include "avp_runtime.h"
#include "hud_message.h"
#include "joypad.h"
#include "player.h"
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr,"R31 door-flow check %u at line %d: %s\n",checks,__LINE__,#x);return 1; \
} } while (0)

/* Minimal PC runtime boundary. The original src/game/doors.c is linked,
 * unmodified, rather than a mock implementation of its decision logic. */
static const AvpRuntimeOps silent_ops={0};
const AvpRuntimeOps *avp_runtime_ops(void){return &silent_ops;}
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
static const AvpHudMessageStep *notice;
static unsigned access_notice;
void avp_hudmsg_queue(const AvpHudMessageStep *p){notice=p;}
void avp_hudmsg_set_access_code(unsigned n){access_notice=n;}
u32 DoEffect(unsigned id){return id;}
const AvpAmpPlacement *avp_resources_retail_amp_list(unsigned group,unsigned level)
{(void)group;(void)level;return NULL;}
void avp_amp_bind_level_list(unsigned group,unsigned level,const AvpAmpPlacement *list)
{(void)group;(void)level;(void)list;}

static void face(unsigned level,unsigned panel_offset,u32 angle)
{
    unsigned cell=panel_offset/8u;
    cur_level=(s16)level;
    new_pos=cell*8u;
    x_pos=((u32)(cell%64u)<<16)|0x8000u;
    y_pos=((u32)(cell/64u)<<16)|0x8000u;
    centre_angle=angle;
    joy_cur=0xffffffffu;
    joy_edge=0xffffffffu&~(1u<<FIRE_B);
    new_level=0;new_x=new_y=0;
    notice=NULL;
    avp_doors_host_clear_exit_kind();
    ResetAccess();
}

int main(void)
{
    static AvpMapMaker draft;
    static u8 maze[AVP_MM_MAX_BYTES];
    unsigned n;
    memset(&draft,0,sizeof draft);
    draft.w=draft.h=64u;draft.level=1u;draft.script_modified=1u;
    n=avp_mm_script_copy_level(1u,draft.script_rows,AVP_MM_MAX_SCRIPTS);
    CHECK(n==15u); draft.script_count=n;
    for(unsigned i=0;i<n;i++){
        AvpMmScriptRow *r=&draft.script_rows[i];
        if(r->offset==1169u)r->offset=1170u; /* TOP => RIGHT panel */
        if(r->offset==1175u)r->offset=1182u; /* next cell LEFT state */
        if(r->kind==4u&&r->arg==1175)r->arg=1182;
    }
    CHECK(avp_mm_script_validate(draft.script_rows,n,1u));
    CHECK(!avp_mm_playtest_validate_relocated_geometry(&draft));
    /* A real Jaguar door panel at the rotated right face, with canonical
     * state owned by the next cell; required door must be closed. */
    memset(maze,0,sizeof maze);
    maze[1170]=0x20u;
    maze[1182]=0u;
    maze[3223]=4u;
    memcpy(draft.cells,maze,sizeof maze);
    CHECK(avp_mm_playtest_validate_relocated_geometry(&draft));
    InitDoors();InitAccess();
    memset(panel_list,0xff,sizeof panel_list);
    avp_doors_bind_panel_ids(NULL);
    avp_doors_mp_restrict_interactions(0,1);
    avp_doors_bind_maze(maze,64u,64u);
    avp_doors_bind_retail_meta();
    CHECK(avp_mm_playtest_bind_scripts(&draft));
    face(1u,1170u,0u); /* RIGHT face, matching relocated panel field 2 */
    DoorKeys();
    CHECK(notice==avp_msg_notsecure);
    CHECK(new_level==0);
    CHECK(maze[1182]==0u);
    /* Closing the required door allows the moved airlock to transfer. */
    maze[3223]=0u;
    face(1u,1170u,0u);DoorKeys();
    CHECK(notice==avp_msg_pressure);
    CHECK(new_level==15);
    CHECK((u32)new_x>>16==22u&&(u32)new_y>>16==27u);
    CHECK(avp_doors_host_exit_kind()==AVP_DOORS_EXIT_AIRLOCK);
    CHECK(maze[1182]==0u); /* airlock doesn't animate ordinary door */
    /* Retail tables for untouched levels survive the edited-level binding. */
    memset(panel_list,0xff,sizeof panel_list);
    panel_list[12]=53u; /* synthetic AIRDUCT1 file-ID mapping */
    AvpDoorPanelIds ids={.lift_back=0xff,.lift_side=0xff,.wall2=0xff,
       .airduct1=53u,.computer1=0xff,.computer2=0xff,.medical_computer=0xff};
    avp_doors_bind_panel_ids(&ids);
    maze[15539]=13u; /* untouched level 15 exit leading back to level 1 */
    face(15u,15539u,0xc0000000u); /* BOTTOM face */
    DoorKeys();
    CHECK(new_level==1);
    CHECK((u32)new_x>>16==18u&&(u32)new_y>>16==5u);
    CHECK(avp_doors_host_exit_kind()==AVP_DOORS_EXIT_NONE);
    /* Retail copy rebind truly removes the relocated table without modifying
     * the source: the relocated wall behaves like an ordinary door. */
    avp_doors_bind_retail_meta();
    avp_doors_bind_panel_ids(NULL);
    face(1u,1170u,0u);DoorKeys();MoveDoors();
    CHECK(new_level==0);
    CHECK(maze[1182]==4u);
    /* Rebinding edited scripts restores moved interlock+exit behavior. */
    CHECK(avp_mm_playtest_bind_scripts(&draft));
    InitDoors();maze[1182]=0;
    face(1u,1170u,0u);DoorKeys();
    CHECK(notice==avp_msg_pressure);
    CHECK(new_level==15);
    /* Invalid draft is rejected before replacing the currently bound table. */
    draft.script_rows[0].arg=1234;
    CHECK(!avp_mm_playtest_bind_scripts(&draft));
    face(1u,1170u,0u);DoorKeys();
    CHECK(new_level==15); /* still the previously validated private table */
    /* A second independent modified level: a keyed door's security state
     * follows TOP->RIGHT, while the other fourteen levels retain retail
     * records. This is the actual access_ok / MoveDoors codepath. */
    {
        static AvpMapMaker keyed;
        memset(&keyed,0,sizeof keyed);
        keyed.level=2u;keyed.w=keyed.h=64u;keyed.script_modified=1u;
        keyed.script_count=avp_mm_script_copy_level(2u,keyed.script_rows,AVP_MM_MAX_SCRIPTS);
        CHECK(keyed.script_count==25u);
        for(unsigned i=0;i<keyed.script_count;i++)if(keyed.script_rows[i].offset==10415u)
            keyed.script_rows[i].offset=10422u;
        CHECK(avp_mm_script_validate(keyed.script_rows,keyed.script_count,2u));
        memset(maze,0,sizeof maze);maze[10410]=0x20u;
        memcpy(keyed.cells,maze,sizeof maze);
        CHECK(avp_mm_playtest_validate_relocated_geometry(&keyed));
        InitDoors();InitAccess();
        avp_doors_bind_maze(maze,64u,64u);
        CHECK(avp_mm_playtest_bind_scripts(&keyed));
        face(2u,10410u,0u);DoorKeys();
        CHECK(notice==avp_msg_access_denied);
        CHECK(access_notice==9u);
        CHECK(maze[10422]==0u);
        CHECK(new_level==0);
        acs_level=9u;face(2u,10410u,0u);DoorKeys();MoveDoors();
        CHECK(notice!=avp_msg_access_denied);
        CHECK(maze[10422]==4u);
        CHECK(avp_mm_playtest_capture_noncanonical_door_states(
            &keyed,2u,maze,sizeof maze)==1u);
        SaveDoors();maze[10422]=0;ResetDoors();
        CHECK(maze[10422]==0u); /* real SaveDoors doesn't own this face */
        CHECK(avp_mm_playtest_restore_noncanonical_door_states(
            &keyed,2u,maze,sizeof maze)==1u);
        CHECK(maze[10422]==4u);
        /* Still only level 2 is replaced. The original level 1 airlock
         * must keep its original panel/state offsets and destination. */
        maze[1169]=0x20u;maze[3223]=0;maze[1175]=0;
        face(1u,1169u,0x40000000u);DoorKeys();
        CHECK(notice==avp_msg_pressure);
        CHECK(new_level==15);
        CHECK((u32)new_x>>16==22u&&(u32)new_y>>16==27u);
        CHECK(avp_doors_host_exit_kind()==AVP_DOORS_EXIT_AIRLOCK);
        /* Original table rebind: relocated keyed door is no longer locked. */
        avp_doors_bind_retail_meta();InitDoors();
        maze[10422]=0;face(2u,10410u,0u);DoorKeys();MoveDoors();
        CHECK(maze[10422]==4u);
    }
    printf("R31 native door-flow integration: %u PASS\n",checks);
    return 0;
}
