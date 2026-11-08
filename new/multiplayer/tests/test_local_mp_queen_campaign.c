/* Regression: Queen death must follow campaign objectives, not the species
 * of an enemy AI target (which can be Predator in an Alien campaign). */
#include "amp.h"
#include "player.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
int main(void)
{
    avp_amp_mp_set_campaign_type(-1);
    CHECK(avp_amp_mp_queen_predator_ending(PT_PREDATOR));
    CHECK(!avp_amp_mp_queen_predator_ending(PT_ALIEN));
    CHECK(!avp_amp_mp_queen_predator_ending(PT_HUMAN));
    avp_amp_mp_set_campaign_type(PT_ALIEN);
    CHECK(!avp_amp_mp_queen_predator_ending(PT_PREDATOR));
    CHECK(!avp_amp_mp_queen_predator_ending(PT_ALIEN));
    avp_amp_mp_set_campaign_type(PT_HUMAN);
    CHECK(!avp_amp_mp_queen_predator_ending(PT_PREDATOR));
    avp_amp_mp_set_campaign_type(PT_PREDATOR);
    CHECK(avp_amp_mp_queen_predator_ending(PT_ALIEN));
    CHECK(avp_amp_mp_queen_predator_ending(PT_HUMAN));
    avp_amp_mp_set_campaign_type(-1);
    CHECK(!avp_amp_mp_queen_predator_ending(PT_ALIEN));
    CHECK(avp_amp_mp_queen_predator_ending(PT_PREDATOR));
    puts("PASS: original Queen completion follows co-op campaign, while unscoped retail follows actor");
    return 0;
}
