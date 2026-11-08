/* R272: HUD.S:855-862 InitCocoons clears num_cocoons and writes only
 * cocoon_frame=COCOON_EMPTY.  An EMPTY slot keeps the previous game's
 * time/level/x/y, which MAZESCRN.S encode_cocoon still packs into saves. */
#include "hud.h"
#include "maze.h"
#include "player.h"
#include "eeprom.h"
#include <assert.h>
#include <stdio.h>
s16 player_type=PT_HUMAN;
uintptr_t savegame;
int main(void)
{
    cocoon_data[0].frame=7;cocoon_data[0].time=3;cocoon_data[0].level=2;
    cocoon_data[0].x=0x00128000u;cocoon_data[0].y=0x000a8000u;num_cocoons=1;
    InitCocoons();
    assert(num_cocoons==0);
    for(unsigned i=0;i<AVP_MAX_COCOONS;i++)assert(cocoon_data[i].frame==AVP_COCOON_EMPTY);
    assert(cocoon_data[0].time==3&&cocoon_data[0].level==2);
    assert(cocoon_data[0].x==0x00128000u&&cocoon_data[0].y==0x000a8000u);
    puts("R272 InitCocoons original frame-only clear PASS");
    return 0;
}
