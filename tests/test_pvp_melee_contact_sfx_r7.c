/* R7: PvP hit feedback must choose exactly the original melee-vs-AMP cues.
 * No sound may be emitted on a miss or by a Marine hitscan weapon. */
#include "mazescrn.h"
#include "player.h"
#include "avp_sfx_ids.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(avp_mazescrn_melee_contact_sfx(PT_ALIEN,1)==AVP_FX_CLAW2); /* tail */
    assert(avp_mazescrn_melee_contact_sfx(PT_ALIEN,2)==AVP_FX_CLAW2); /* claw */
    assert(avp_mazescrn_melee_contact_sfx(PT_ALIEN,3)==AVP_FX_CLAW2); /* bite */
    assert(avp_mazescrn_melee_contact_sfx(PT_PREDATOR,1)==AVP_FX_MELEE); /* stick */
    assert(avp_mazescrn_melee_contact_sfx(PT_PREDATOR,4)==AVP_FX_CLAW2); /* blade */
    for(int weapon=1;weapon<=4;weapon++)
        assert(avp_mazescrn_melee_contact_sfx(PT_HUMAN,(s16)weapon)==0);
    assert(avp_mazescrn_melee_contact_sfx(PT_PREDATOR,2)==0); /* cannon */
    assert(avp_mazescrn_melee_contact_sfx(PT_PREDATOR,3)==0); /* disc */
    assert(avp_mazescrn_melee_contact_sfx(PT_ALIEN,0)==0);
    puts("native melee impact sound parity: PASS");
    return 0;
}
