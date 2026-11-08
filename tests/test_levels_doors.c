#include "levels.h"
#include "doors.h"
#include "player.h"
#include "amp.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include "hud_message.h"
#include "joypad.h"
#include "pc/pc_input_routing.h"
#include "avp_runtime.h"
static const AvpRuntimeOps silent_runtime={0};
const AvpRuntimeOps *avp_runtime_ops(void){return &silent_runtime;}

u32 new_pos,levfx_ID;
const AvpHudMessageStep avp_msg_pressure[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_notsecure[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_airlocked[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_jammed[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_jlift[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_escape[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_access_denied[]={{0,{0,0,0}}};
static const AvpHudMessageStep *last_door_notice;
void avp_hudmsg_queue(const AvpHudMessageStep *p){last_door_notice=p;}
void avp_hudmsg_set_access_code(unsigned n){(void)n;}
u32 DoEffect(unsigned id){return id;}
u32 x_pos,y_pos,centre_angle; s16 player_type; uintptr_t savegame;
volatile u32 joy_cur=0xffffffffu,joy_edge=0xffffffffu;
AvpAmp level1amps[AVP_NUM_AMPS]; AvpAmp *amps_at=level1amps,*amp_data=level1amps;
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
u8 objmap[AVP_AMP_OBJ_ROW_BYTES*AVP_AMP_OBJ_H];

u32 ccn_xsave,ccn_ysave;
u8 in_title,in_select;
static unsigned fade_waits,music_stops;
void WaitFXFa(void){++fade_waits;}
void StopMusi(void){++music_stops;}
void ResetScale(void){}
void ResetSprites(void){}
void restore_level(void){}
void ResetPlayer(void){}
u8 *avp_collision_maze_data(void){return 0;}
u16 avp_collision_maze_width(void){return 0;}
u16 avp_collision_maze_height(void){return 0;}

void save_level(void){};
static unsigned released_spawn_amp;
void amp_release(unsigned n){released_spawn_amp=n;}

int main(void){
 AvpLevelInfo lv[15]; u8 maze[4*4*8]; unsigned i;
 memset(lv,0,sizeof(lv)); lv[2].start_x=0x111000; lv[2].start_y=0x222000; lv[2].start_angle=0x40000000;
 avp_levels_set_table(lv,15); avp_levels_set_save_words(0,0); player_type=PT_HUMAN; InitLevels();
 assert(cur_level==3); assert(x_pos==0x111000); assert(y_pos==0x222000);
 /* Every new campaign must inherit the selected species' retail start,
    rather than leaking the previous Alien level into a Marine playthrough. */
 lv[13].start_x=0x333000;lv[13].start_y=0x444000;
 lv[14].start_x=0x555000;lv[14].start_y=0x666000;
 player_type=PT_ALIEN;InitLevels();
 assert(cur_level==14&&x_pos==0x333000&&y_pos==0x444000);
 player_type=PT_PREDATOR;InitLevels();
 assert(cur_level==15&&x_pos==0x555000&&y_pos==0x666000);
 player_type=PT_HUMAN;InitLevels();
 assert(cur_level==3&&x_pos==0x111000&&y_pos==0x222000);
 /* MAIN.S passes a real slot pointer in savegame; LEVELS.S reads it directly. */
 { u8 slot[20]={0}; u32 packed=(5u<<2); slot[16]=(u8)(packed>>24);slot[17]=(u8)(packed>>16);slot[18]=(u8)(packed>>8);slot[19]=(u8)packed;
   savegame=(uintptr_t)slot; InitLevels(); assert(cur_level==5); savegame=0; }
 InitDoors(); assert(fullbits[0]==0); assert(fullbits[0x20]&AVP_WB_DOOR); assert(fullbits[0xa0]&AVP_WB_DOOR);
 player_type=PT_HUMAN; InitAccess(); assert(acs_level==0);
 player_type=PT_ALIEN; InitAccess(); assert(acs_level==10);
 player_type=PT_PREDATOR; InitAccess(); assert(acs_level==10);
 player_type=PT_HUMAN;
 memset(maze,0,sizeof(maze)); avp_doors_bind_maze(maze,2,2); cur_level=3;
 /* R208: the host's lift-only joystick bridge looks up the exact DoorKeys
  * facing wall/panel ID; ordinary doors and empty faces are not lifts. */
 { AvpDoorPanelIds ids={.lift_back=60,.lift_side=61,
        .wall2=0xff,.airduct1=0xff,.computer1=0xff,
        .computer2=0xff,.medical_computer=0xff};
   memset(maze,0,sizeof maze);
   avp_doors_bind_panel_ids(&ids);
   memset(panel_list,0xff,sizeof panel_list);
   panel_list[9]=60; panel_list[10]=61; panel_list[11]=62;
   x_pos=y_pos=centre_angle=new_pos=0; /* facing right wall */
   maze[2]=10;
   assert(avp_doors_player_facing_lift());
   /* Real DOORS.S level transfer with the analog-created original Jaguar
    * input bits (no shortcut directly to new_level).  The authored lift door
    * is already open, so lift_exit() changes to the adjacent sublevel. */
   maze[0]=64;maze[6]=0;cur_level=3;new_level=0;
   joy_cur=joy_edge=avp_pc_lift_analog_pad(0xffffffffu&~(1u<<FIRE_B),1,
                                               avp_doors_player_facing_lift(),0.5f);
   /* R20: Deathmatch's default single-level mode blocks the REAL native
    * lift input, while the enabled modifier and co-op retain that path. */
   avp_doors_mp_restrict_interactions(1,0);
   last_door_notice=NULL;DoorKeys();
   assert(new_level==0 && last_door_notice==avp_msg_jlift);
   avp_doors_mp_restrict_interactions(1,1);
   new_level=0;last_door_notice=NULL;DoorKeys();
   assert(new_level==2 && last_door_notice!=avp_msg_jlift);
   new_level=0;lift_key=0;joy_cur=joy_edge=avp_pc_lift_analog_pad(0xffffffffu&~(1u<<FIRE_B),1,
                                               avp_doors_player_facing_lift(),-0.5f);
   DoorKeys();
   assert(new_level==4);
   joy_cur=joy_edge=0xffffffffu;new_level=0;lift_key=0;
   maze[0]=0;
   maze[2]=11;
   assert(avp_doors_player_facing_lift());
   maze[2]=12;
   assert(!avp_doors_player_facing_lift());
   maze[2]=0;
   assert(!avp_doors_player_facing_lift());
   maze[2]=10;new_pos=(u32)(2u*2u*8u); /* force fallback cell */
   assert(avp_doors_player_facing_lift());
   centre_angle=0x40000000u; /* different facing wall */
   assert(!avp_doors_player_facing_lift());
   new_pos=0;centre_angle=0;
   avp_doors_mp_restrict_interactions(0,1);
   avp_doors_bind_panel_ids(NULL);
   memset(panel_list,0xff,sizeof panel_list);
 }
 /* R20: DM terminal / duct / airlock rules in the actual DoorKeys path.
  * Original panel IDs and authored exit tables are supplied to the test. */
 {
    AvpDoorPanelIds ids={.lift_back=60,.lift_side=61,
      .wall2=54,.airduct1=53,.computer1=55,.computer2=56,
      .medical_computer=57};
    const AvpDoorExitEntry exit[]={ {.panel_offset=2u,.x=1u,.y=1u,.level=4u} };
    const AvpDoorAccessEntry access[]={ {.door_offset=14u,.code=AVP_ACC_EXIT} };
    const AvpDoorInterlockEntry inter[]={ {.door_offset=14u,.required_offsets=NULL,.required_count=0u} };
    AvpDoorLevelMeta meta[15]={0};
    meta[2].exits=exit;meta[2].exit_count=1u;
    meta[2].access=access;meta[2].access_count=1u;
    meta[2].interlocks=inter;meta[2].interlock_count=1u;
    InitDoors();memset(maze,0,sizeof maze);
    avp_doors_bind_maze(maze,2u,2u);
    avp_doors_bind_panel_ids(&ids);
    avp_doors_bind_level_meta(meta,15u);
    memset(panel_list,0xff,sizeof panel_list);
    panel_list[11]=55u; /* computer1 */
    panel_list[12]=54u; /* wall2 duct */
    x_pos=y_pos=centre_angle=new_pos=0;cur_level=3;ResetAccess();
    joy_edge=joy_cur=0xffffffffu&~(1u<<FIRE_B);
    /* Terminals disabled in DM regardless of travel option. */
    maze[2]=12;comp_panel=0;
    avp_doors_mp_restrict_interactions(1,1);DoorKeys();
    assert(comp_panel==0);
    avp_doors_mp_restrict_interactions(0,1);DoorKeys();
    assert(comp_panel==55);comp_panel=0;
    /* Ducts do not teleport or produce a HUD notice when DM travel off. */
    maze[2]=13;new_level=0;
    avp_doors_mp_restrict_interactions(1,0);DoorKeys();
    assert(new_level==0);
    avp_doors_mp_restrict_interactions(1,1);DoorKeys();
    assert(new_level==4);
    new_level=0;
    /* The exit access rule for a normal airlock is intercepted BEFORE
     * original pressure FX / Level change only in DM no-travel mode. */
    maze[2]=0x20;maze[14]=0;
    avp_doors_mp_restrict_interactions(1,0);
    last_door_notice=NULL;DoorKeys();
    assert(new_level==0 && last_door_notice==avp_msg_airlocked);
    avp_doors_mp_restrict_interactions(1,1);
    last_door_notice=NULL;DoorKeys();
    assert(new_level==4 && last_door_notice==avp_msg_pressure);
    new_level=0;
    avp_doors_mp_restrict_interactions(0,1);
    avp_doors_bind_level_meta(NULL,0u);
    avp_doors_bind_panel_ids(NULL);
    memset(panel_list,0xff,sizeof panel_list);
    InitDoors();
 }
 /* cell 0: centre side door wall id 0x20, opening state 13 */
 maze[0]=0x20; maze[6]=13; SaveDoors(); maze[6]=3; ResetDoors(); assert(maze[6]==13);
 for(i=0;i<AVP_MAX_PANELS;i++) assert(panel_list[i]==0xffu);

 /* A reloaded/rebound level may have a different authored door count.
  * Growing its saved span must not overwrite the next level's door bytes. */
 {
    InitDoors();
    cur_level=3;
    memset(maze,0,sizeof maze);
    maze[0]=0x20;maze[6]=11;
    SaveDoors();                    /* Level 3 occupies one byte. */
    cur_level=4;
    memset(maze,0,sizeof maze);
    maze[1]=0x20;maze[7]=22;
    SaveDoors();                    /* Level 4 directly follows it. */
    cur_level=3;
    memset(maze,0,sizeof maze);
    maze[0]=0x20;maze[6]=33;
    maze[8+1]=0x20;maze[8+7]=44;
    SaveDoors();                    /* Level 3 now needs two bytes. */
    maze[6]=maze[8+7]=0;
    ResetDoors();
    assert(maze[6]==33 && maze[8+7]==44);
    cur_level=4;
    memset(maze,0,sizeof maze);
    maze[1]=0x20;
    ResetDoors();
    assert(maze[7]==22);           /* Must not be overwritten by level 3. */
    InitDoors();
 }
 /* DOORS.S DoorKeys local `move_doors` label is only an exit path; the
  * exported MoveDoors:: advances the byte once later in UpdatePlayer. */
 memset(maze,0,sizeof(maze)); avp_doors_bind_maze(maze,2,2); cur_level=3;
 new_pos=0; x_pos=0; y_pos=0; centre_angle=0; /* facing RIGHT (side 2) */
 maze[2]=0x20; /* ordinary door panel on right side of cell 0 */
 joy_edge=0xffffffffu & ~(1u<<FIRE_B); joy_cur=0xffffffffu;
 DoorKeys();
 assert(maze[8+6]==0); /* selection/reversal only: no physical step yet */
 MoveDoors();
 assert(maze[8+6]==4);
 /* A stale collision-grid owner across the room must not make a door
  * reopen when it is supposed to close. A real nearby owner still does. */
 {
    AvpAmp *old=&level1amps[0];
    memset(level1amps,0,sizeof(level1amps));
    memset(collmap,0,sizeof(collmap));
    old->mode=(AvpAmpModeFn)(uintptr_t)1;
    old->xpos=0x00081000;old->ypos=0x00008000;
    collmap[1]=1u;  /* door neighbor (1,0), not actor's actual (8,0) */
    joy_edge=0xffffffffu & ~(1u<<FIRE_B);
    DoorKeys(); /* reverse door from opening to closing */
    x_pos=0x00038000u;y_pos=0x00038000u;
    MoveDoors();
    assert(maze[8+6]==0u);
    /* Same scenario with a genuinely blocking actor: closure must reverse. */
    memset(collmap,0,sizeof(collmap));
    maze[8+6]=0;
    x_pos=y_pos=0u;new_pos=0;
    joy_edge=0xffffffffu & ~(1u<<FIRE_B);
    DoorKeys(); /* open from closed */
    MoveDoors();
    assert(maze[8+6]==4u);
    joy_edge=0xffffffffu & ~(1u<<FIRE_B);
    DoorKeys(); /* initiate close */
    old->xpos=0x00011000;old->ypos=0x00008000;
    collmap[1]=1u;
    x_pos=0x00038000u;y_pos=0x00038000u;
    MoveDoors();
    assert(maze[8+6]>4u);
    memset(collmap,0,sizeof(collmap));
 }
 joy_edge=joy_cur=0xffffffffu;
 /* R257: Alien PC Use must never be the bite button, nor can Bite operate
  * an ordinary door. Jaguar virtual-pad mode still uses original B. */
 {
     InitDoors();player_type=PT_ALIEN;
     memset(maze,0,sizeof maze);avp_doors_bind_maze(maze,2,2);
     maze[2]=0x20;new_pos=0;x_pos=y_pos=centre_angle=0;
     avp_doors_pc_alien_use(1,0);
     joy_edge=joy_cur=0xffffffffu&~(1u<<FIRE_B); /* bite without use */
     DoorKeys();MoveDoors();assert(maze[8+6]==0);
     avp_doors_pc_alien_use(1,1); /* use without bite */
     joy_edge=joy_cur=0xffffffffu;
     DoorKeys();MoveDoors();assert(maze[8+6]==4);
     DoorKeys();MoveDoors();assert(maze[8+6]==8); /* held use doesn't toggle */
     avp_doors_pc_alien_use(1,0);DoorKeys();
     avp_doors_pc_alien_use(0,0);
     InitDoors();memset(maze,0,sizeof maze);maze[2]=0x20;
     joy_edge=joy_cur=0xffffffffu&~(1u<<FIRE_B);
     DoorKeys();MoveDoors();assert(maze[8+6]==4); /* retail B */
     joy_edge=joy_cur=0xffffffffu;
     player_type=PT_HUMAN;
 }
 /* R290 original LEVELS.S spawn-square cleanup zeros the active mode only.
    * Grid contents and AMP allocation state are NOT changed. */
 {
     AvpAmp *a=&level1amps[0], *other=&level1amps[1];
     unsigned cell=9u*64u+8u, off=9u*128u+16u;
     x_pos=0x00088000u;y_pos=0x00098000u;
     memset(level1amps,0,sizeof(level1amps));
     memset(collmap,0,sizeof(collmap));memset(objmap,0,sizeof(objmap));
     a->mode=(AvpAmpModeFn)(uintptr_t)1;
     a->xpos=(s32)x_pos;a->ypos=(s32)y_pos;a->creature=AC_HUMAN;
     a->host_static=1;
     other->mode=(AvpAmpModeFn)(uintptr_t)1;
     other->xpos=(s32)x_pos;other->ypos=(s32)y_pos;other->creature=AC_GEN;
     collmap[cell]=1;objmap[off+1]=1;
     KillBastards();
     assert(released_spawn_amp==0u);
     assert(!a->mode&&!a->host_static);
     assert(collmap[cell]==1&&objmap[off]==0&&objmap[off+1]==1);
     assert(other->mode!=NULL);
     a->mode=(AvpAmpModeFn)(uintptr_t)1;
     collmap[cell]=2;objmap[off+1]=2;
     KillBastards();
     assert(collmap[cell]==2&&objmap[off+1]==2);
 }
 /* Keep all four original protected creature types intact.
  * The R278 assembly comparison independently records mode-only cleanup. */
 {
     static const u16 protected_types[]={AC_COCOON,AC_AQUEEN,AC_AQSHIELD,AC_GEN};
     AvpAmp *a=&level1amps[0];
     const unsigned cell=7u*AVP_AMP_GRID_W+6u;
     const unsigned off=7u*AVP_AMP_OBJ_ROW_BYTES+12u;
     x_pos=0x00068000u;y_pos=0x00078000u;
     for(unsigned i=0;i<sizeof protected_types/sizeof protected_types[0];++i){
         unsigned old_release=released_spawn_amp;
         memset(level1amps,0,sizeof level1amps);
         memset(collmap,0,sizeof collmap);
         memset(objmap,0,sizeof objmap);
         a->mode=(AvpAmpModeFn)(uintptr_t)1;
         a->xpos=(s32)x_pos;a->ypos=(s32)y_pos;
         a->creature=protected_types[i];
         collmap[cell]=1u;objmap[off+1]=1u;
         KillBastards();
         assert(a->mode!=NULL);
         assert(collmap[cell]==1u&&objmap[off+1]==1u);
         assert(released_spawn_amp==old_release);
     }
     /* Dead stationary AMPs are saved as corpses, not live cleanup victims. */
     {
         unsigned old_release=released_spawn_amp;
         memset(level1amps,0,sizeof level1amps);
         memset(collmap,0,sizeof collmap);
         a->xpos=(s32)x_pos;a->ypos=(s32)y_pos;
         a->mode=NULL;a->host_static=1;a->astype=53;
         a->flags=1u<<AMP_DEAD;
         KillBastards();
         assert(a->host_static&&a->astype==53);
         assert(a->flags&(1u<<AMP_DEAD));
         assert(released_spawn_amp==old_release);
     }
 }
 /* PostFirst's ScreenOff transition stops title music after character
    selection, while leaving gameplay-only and front-end-free paths alone. */
 in_title=1u;in_select=1u;ScreenOff();
 assert(!in_title&&!in_select&&fade_waits==1u&&music_stops==1u);
 ScreenOff();assert(music_stops==1u);
 in_title=0u;in_select=0u;ScreenOff();assert(music_stops==1u);
 return 0;
}
