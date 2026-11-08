/* R286: retail PLAYER.S give_wep and give_ammo share a0 only in the
 * weapon-acquisition fall-through; independent pickups remain last-writer. */
#include "hud_message.h"
#include "collectables.h"
#include "weapons.h"
#include "player.h"
#include "amp.h"
#include "levels.h"
#include "maze.h"
#include <assert.h>
#include <string.h>

s16 player_type,player_energy,max_energy;
u8 cur_weps,old_weps,acs_level;
s16 cur_wepno,new_wepno;
AvpAmp amps[AVP_NUM_AMPS],*amp_data=amps,*amps_at=amps;
u8 objmap[AVP_AMP_OBJ_ROW_BYTES*AVP_AMP_OBJ_H];
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
s16 cur_level=1;
u32 x_pos,y_pos;
static void quiet_sfx(unsigned id){(void)id;}
static void assert_first_two(const char *a,const char *b)
{
    const AvpHudMessageStep *p;
    UpdtHUDMsg();
    p=avp_hudmsg_current();
    assert(p && p->line[0] && p->line[1]);
    assert(!strcmp(p->line[0],a));
    assert(!strcmp(p->line[1],b));
}
int main(void)
{
    player_type=PT_HUMAN;
    avp_collect_set_callbacks(avp_hudmsg_pickup,quiet_sfx);
    ammo_info[0].max=100; ammo_info[0].cur=0;
    ammo_info[1].max=100; ammo_info[1].cur=0;

    /* A new weapon's magazine enters give_ammo using the weapon message. */
    InitHUDMsg();cur_weps=0;
    assert(CollectIt(COLL_PULSERIFLE,60));
    assert_first_two("PULSE RIFLE","COLLECTED");

    /* A separate ammo pickup in the same tick writes a different notice. */
    InitHUDMsg();cur_weps=0;ammo_info[0].cur=0;ammo_info[1].cur=0;
    assert(CollectIt(COLL_PULSERIFLE,60));
    assert(CollectIt(COLL_SHOTAMMO,15));
    assert_first_two("SHOTGUN","ROUNDS");

    /* A weapon that is already owned yields its ammunition notice. */
    InitHUDMsg();cur_weps=(u8)(1u<<2);ammo_info[1].cur=0;
    assert(CollectIt(COLL_PULSERIFLE,60));
    assert_first_two("PULSE RIFLE","ROUNDS");
    return 0;
}
