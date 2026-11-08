/* R259: original EEPROM tracker bit must survive the PostFirst StdHUD pass.
 * Exercise production MakeSave and StdHUD; the InitHUD stub models the real
 * InitHUD reset that previously discarded saved Marine inventory state. */
#include "savegame.h"
#include "eeprom.h"
#include "player.h"
#include "collectables.h"
#include "weapons.h"
#include "hud.h"
#include "mazescrn.h"
#include "levels.h"
#include "maze.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

u32 x_pos,y_pos,centre_angle;
s16 player_type,player_energy;
s32 score;
AvpAmmoInfo ammo_info[6];
s16 show_mt;
u8 acs_level,cur_weps,old_weps;
s16 cur_level,new_level;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
uintptr_t savegame;
u8 SaveCont[AVP_SAVE_SIZE],cartcopy[AVP_EE_SIZE];
static int hud_resets;
void InitHUD(void) { ++hud_resets; show_mt=0; }

static u32 read32(const u8 *p)
{ return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3]; }

static void check_marine(int acquired)
{
    u8 *sv=cartcopy+AVP_SAVE_SIZE;
    player_type=PT_HUMAN;
    show_mt=acquired?-1:0;
    MakeSave(1);
    assert(!!(read32(sv+16)&0x800u)==!!acquired);
    /* Simulate a fresh process after loading EEPROM: inventory must be
     * obtained from the saved bits, not any stale global state. */
    show_mt=acquired?0:-1;
    savegame=(uintptr_t)sv;
    StdHUD();
    assert(show_mt==(acquired?-1:0));
    assert(hud_resets>0);
    savegame=0;
    /* A new Marine campaign must not inherit the old pickup. */
    show_mt=-1;
    StdHUD();
    assert(show_mt==0);
}

int main(void)
{
    memset(cartcopy,0,sizeof cartcopy);
    x_pos=0x00228000u;y_pos=0x00218000u;
    centre_angle=0x43000000u;
    player_energy=1000;cur_level=5;
    check_marine(0);
    check_marine(1);
    check_marine(0); /* loading a no-tracker save clears stale prior state */
    /* Only a Marine consumes the saved tracker state. */
    player_type=PT_ALIEN;
    show_mt=-1;
    savegame=(uintptr_t)(cartcopy+AVP_SAVE_SIZE);
    StdHUD();
    assert(show_mt==0);
    puts("R259 Marine motion tracker EEPROM/StdHUD reinitialization PASS");
    return 0;
}
