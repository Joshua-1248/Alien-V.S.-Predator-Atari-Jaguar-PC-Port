/* RE39 R193: actual original DOORS.S state machine with optional Predator
 * melee entry, real metadata lookup, one-way opening, and jammed-use message. */
#include "doors.h"
#include "amp.h"
#include "avp_runtime.h"
#include "hud_message.h"
#include "joypad.h"
#include "player.h"
#include "avp_sfx_ids.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static unsigned sounds,last_sound,notices;
static const AvpHudMessageStep *notice;
static void sound_fx(void *u,unsigned id){(void)u;sounds++;last_sound=id;}
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
void avp_hudmsg_queue(const AvpHudMessageStep *p){notice=p;notices++;}
void avp_hudmsg_set_access_code(unsigned n){(void)n;}
u32 DoEffect(unsigned id){return id;}
static unsigned checks;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"R193 failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)

int main(void)
{
    static u8 maze[64u*64u*8u];
    const unsigned cell=(10u*64u+10u)*8u;
    const unsigned door_off=cell+8u+6u; /* right side's next-cell LEFT door state */
    const AvpDoorAccessEntry access[]={ {door_off,AVP_ACC_JAMMED} };
    const AvpDoorLevelMeta metadata={.access=access,.access_count=1};
    memset(maze,0,sizeof maze);
    player_type=PT_PREDATOR;cur_level=1;
    new_pos=cell;x_pos=(10u<<16)|0xa000u;y_pos=(10u<<16)|0x8000u;
    centre_angle=0u; /* right-facing, source exact facing_side() */
    maze[cell+2u]=0x20u; /* authored door panel, subtype 0 */
    avp_doors_bind_panel_ids(NULL);
    avp_doors_bind_maze(maze,64u,64u);
    avp_doors_bind_level_meta(&metadata,1u);
    avp_doors_mp_restrict_interactions(0,1);
    InitDoors();ResetAccess();
    /* Disabled by default: a Predator melee swing cannot bypass the original
     * JAMMED metadata, and the normal Use command still shows the message. */
    avp_doors_predator_jammed_attack_enabled(0);
    CHECK(!avp_doors_predator_melee_jammed(1,0x9000u));
    CHECK(maze[door_off]==0);
    joy_edge=0xffffffffu & ~(1u<<FIRE_B);
    DoorKeys();
    CHECK(notice==avp_msg_jammed);
    CHECK(maze[door_off]==0);
    /* Only the two specified Predator melee weapons may activate a jam. */
    avp_doors_predator_jammed_attack_enabled(1);
    CHECK(!avp_doors_predator_melee_jammed(2,0x9000u));
    CHECK(!avp_doors_predator_melee_jammed(3,0x9000u));
    CHECK(!avp_doors_predator_melee_jammed(1,0u));
    player_type=PT_HUMAN;
    CHECK(!avp_doors_predator_melee_jammed(1,0x9000u));
    player_type=PT_PREDATOR;
    /* Same jammed door is beyond the weapon's actual reach. */
    x_pos=(10u<<16)|0x4000u; /* 0xc000 from right edge */
    CHECK(!avp_doors_predator_melee_jammed(4,0x9000u));
    x_pos=(10u<<16)|0xa000u;
    avp_doors_mp_restrict_interactions(1,0);
    CHECK(!avp_doors_predator_melee_jammed(4,0x9000u));
    avp_doors_mp_restrict_interactions(0,1);
    sounds=notices=0;notice=NULL;
    CHECK(avp_doors_predator_melee_jammed(4,0x9000u));
    CHECK(last_sound==AVP_FX_RISE2);
    CHECK(sounds==1);
    CHECK(maze[door_off]==0); /* initial strike starts the authored animation */
    MoveDoors();
    CHECK(maze[door_off]==4u);
    /* Even during the opening animation Use must say JAMMED, not reverse it. */
    notice=NULL;DoorKeys();
    CHECK(notice==avp_msg_jammed);
    MoveDoors();
    CHECK(maze[door_off]==8u);
    for(unsigned i=0;i<20u;i++)MoveDoors();
    CHECK(maze[door_off]==56u); /* original subtype-0 door limit */
    CHECK(!avp_doors_predator_melee_jammed(1,0x9000u));
    avp_doors_predator_jammed_attack_enabled(0);
    notice=NULL;DoorKeys();
    CHECK(notice==avp_msg_jammed); /* never closes, even after disabling option */
    CHECK(maze[door_off]==56u);
    MoveDoors();CHECK(maze[door_off]==56u);
    /* Combi-Stick must independently open a *closed* jammed door with one
     * hit. Reinitialize the authored door and let the actual door state
     * machine animate its opening; this also catches weapon-ID drift. */
    maze[door_off]=0u;
    InitDoors();ResetAccess();
    avp_doors_predator_jammed_attack_enabled(1);
    sounds=0;notice=NULL;
    CHECK(avp_doors_predator_melee_jammed(1,0x9000u));
    CHECK(sounds==1 && last_sound==AVP_FX_RISE2);
    MoveDoors();CHECK(maze[door_off]==4u);
    for(unsigned i=0;i<20u;i++)MoveDoors();
    CHECK(maze[door_off]==56u);
    notice=NULL;DoorKeys();CHECK(notice==avp_msg_jammed);
    MoveDoors();CHECK(maze[door_off]==56u);
    printf("R193 Predator jammed-door original animation/access: %u PASS\n",checks);
    return 0;
}
