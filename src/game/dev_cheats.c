#include "dev_cheats.h"
#include "player.h"
#include "weapons.h"
#include "mazescrn.h"
#include "collectables.h"
#include "doors.h"

static int infinite_health;
static int infinite_ammo;

/* The cheat grants temporary access, without taking away a tracker obtained
   through the original game.  The collection callback clears the temporary
   ownership marker if a real tracker is picked up while the cheat is on. */
static int tracker_cheat_enabled;
static int tracker_cheat_borrowed;

void avp_dev_cheats_set_tracker(int enabled)
{
    tracker_cheat_enabled=enabled?1:0;
    if(!tracker_cheat_enabled){
        if(tracker_cheat_borrowed && player_type==PT_HUMAN)show_mt=0;
        tracker_cheat_borrowed=0;
    }else if(player_type==PT_HUMAN && !show_mt){
        show_mt=-1;
        tracker_cheat_borrowed=1;
    }
}
int avp_dev_cheats_tracker_enabled(void){return tracker_cheat_enabled;}
int avp_dev_cheats_tracker_borrowed(void){return tracker_cheat_borrowed;}
void avp_dev_cheats_tracker_borrowed_restore(int borrowed){tracker_cheat_borrowed=!!borrowed;}
void avp_dev_cheats_tracker_collected(void){tracker_cheat_borrowed=0;}
void avp_dev_cheats_tracker_restored(void){tracker_cheat_borrowed=0;}

void avp_dev_cheats_adjust_security(int delta){if(player_type==PT_HUMAN){int level=(int)acs_level+delta;if(level<0)level=0;if(level>10)level=10;acs_level=(u8)level;}}

void avp_dev_cheats_restore_health(void)
{
    player_energy=max_energy;
    old_energy=max_energy;
    pain=0;
}

void avp_dev_cheats_restore_ammo(void)
{
    fill_weps();
}

void avp_dev_cheats_give_weapons(void)
{
    if(player_type==PT_HUMAN)cur_weps=0x1eu;       /* weapons 1..4 */
    else if(player_type==PT_PREDATOR)cur_weps=0x7eu; /* all six Predator equipment slots */
    /* Alien starts with its complete three-weapon set; leave it untouched. */
    old_weps=cur_weps;
    fill_weps();
}

void avp_dev_cheats_set_infinite_health(int enabled){infinite_health=enabled?1:0;if(infinite_health)avp_dev_cheats_restore_health();}
void avp_dev_cheats_set_infinite_ammo(int enabled){infinite_ammo=enabled?1:0;if(infinite_ammo)avp_dev_cheats_restore_ammo();}
int avp_dev_cheats_infinite_health(void){return infinite_health;}
int avp_dev_cheats_infinite_ammo(void){return infinite_ammo;}
void avp_dev_cheats_tick(void)
{
    if(infinite_health)avp_dev_cheats_restore_health();
    if(infinite_ammo)avp_dev_cheats_restore_ammo();
}
