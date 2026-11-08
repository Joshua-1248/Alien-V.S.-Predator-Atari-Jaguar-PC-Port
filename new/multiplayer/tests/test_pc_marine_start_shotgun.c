#include "pc/pc_marine_start.h"
#include "local_mp_actor_native.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

static void reset(AvpAmmoInfo ammo[6],u8 *owned,s16 *next)
{
    memset(ammo,0,sizeof(AvpAmmoInfo)*6u);
    ammo[0].max=44;ammo[1].max=264;ammo[2].max=88;
    ammo[3].max=88;
    *owned=0;*next=1;
}
int main(void)
{
    AvpAmmoInfo ammo[6];u8 owned; s16 next;
    AvpMpActorNativeState actor[2];
    reset(ammo,&owned,&next);
    assert(!avp_pc_marine_start_shotgun_apply(0,0,PT_HUMAN,&owned,&next,ammo));
    assert(owned==0&&ammo[0].cur==0&&next==1);
    assert(!avp_pc_marine_start_shotgun_apply(1,1,PT_HUMAN,&owned,&next,ammo));
    assert(owned==0&&ammo[0].cur==0);
    assert(!avp_pc_marine_start_shotgun_apply(1,0,PT_ALIEN,&owned,&next,ammo));
    assert(!avp_pc_marine_start_shotgun_apply(1,0,PT_PREDATOR,&owned,&next,ammo));
    assert(owned==0&&ammo[0].cur==0);
    assert(avp_pc_marine_start_shotgun_apply(1,0,PT_HUMAN,&owned,&next,ammo));
    assert(owned==2u&&next==1&&ammo[0].cur==44);
    assert(ammo[1].cur==0&&ammo[2].cur==0&&ammo[3].cur==0);
    /* S67 walk_create_actor copies the original initialized snapshot into
     * each player. Check the weapon ownership fields remain in sync. */
    memset(actor,0,sizeof(actor));
    actor[0].player.player_type=PT_HUMAN;
    actor[0].player.cur_weps=owned;
    actor[0].player.new_wepno=next;
    actor[0].weapon.cur_weps=owned;
    actor[0].weapon.new_wepno=next;
    memcpy(actor[0].weapon.ammo_info,ammo,sizeof(ammo));
    actor[1]=actor[0];
    assert(actor[1].player.cur_weps==2u);
    assert(actor[1].weapon.cur_weps==2u);
    assert(actor[1].weapon.ammo_info[0].cur==44);
    actor[1].weapon.ammo_info[0].cur=43;
    assert(actor[0].weapon.ammo_info[0].cur==44); /* Independent ammo. */
    puts("Marine full-shotgun start: PASS (disabled/save/species/2 actors)");
    return 0;
}
