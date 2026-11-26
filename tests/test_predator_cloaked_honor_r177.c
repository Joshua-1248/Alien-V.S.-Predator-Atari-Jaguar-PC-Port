/* RE39 R177: retail Predator honor parity plus independent opt-in cloaked-
 * kill policies. Uses the real do_score in AMP, not a scoring reimplementation. */
#include <assert.h>
#include <stdio.h>
#include "amp.h"
#include "player.h"

s32 score;
s16 player_type,player_energy;
u32 x_pos,y_pos;

static s32 score_kill(s16 species,int no_loss,int half_value,
                      int cloaked,int close,int killed)
{
    AvpAmp a={0};
    avp_amp_set_predator_cloak_honor_options(no_loss,half_value);
    avp_amp_mp_set_campaign_type(-1);
    player_type=species;
    score=100000;
    a.creature=AC_HUMAN;
    a.flags=(u16)((killed?(1u<<AMP_PHIT):0u)|
                  (cloaked?(1u<<AMP_INVHIT):0u)|
                  (close?(1u<<AMP_CLOSEHIT):0u));
    do_score(&a);
    return score;
}
int main(void)
{
    /* Retail: cloaked close = -5000; cloaked ranged = -10000.
     * Visible close = +5000; visible ranged = +2500.
     * Human victim base honor = 5000 (AMP.S). */
    assert(score_kill(PT_PREDATOR,0,0,1,1,1)==95000);
    assert(score_kill(PT_PREDATOR,0,0,1,0,1)==90000);
    assert(score_kill(PT_PREDATOR,0,0,0,1,1)==105000);
    assert(score_kill(PT_PREDATOR,0,0,0,0,1)==102500);
    /* Each standalone enhancement plus combined priority. */
    assert(score_kill(PT_PREDATOR,1,0,1,1,1)==100000);
    assert(score_kill(PT_PREDATOR,1,0,1,0,1)==100000);
    assert(score_kill(PT_PREDATOR,0,1,1,1,1)==102500);
    assert(score_kill(PT_PREDATOR,0,1,1,0,1)==101250);
    assert(score_kill(PT_PREDATOR,1,1,1,1,1)==102500);
    assert(score_kill(PT_PREDATOR,1,1,1,0,1)==101250);
    /* A visible kill never uses either cloaked override. */
    for(int n=0;n<2;n++)for(int h=0;h<2;h++){
        assert(score_kill(PT_PREDATOR,n,h,0,1,1)==105000);
        assert(score_kill(PT_PREDATOR,n,h,0,0,1)==102500);
        assert(score_kill(PT_PREDATOR,n,h,1,1,0)==100000);
        assert(score_kill(PT_HUMAN,n,h,1,0,1)==105000);
        assert(score_kill(PT_ALIEN,n,h,1,0,1)==120000);
    }
    /* Multiplayer keeps original Jaguar honor; F12 settings are SP-only. */
    player_type=PT_PREDATOR;
    avp_amp_set_predator_cloak_honor_options(1,1);
    avp_amp_mp_set_campaign_type(PT_PREDATOR);
    {
        AvpAmp a={0};
        a.creature=AC_HUMAN;
        a.flags=(u16)((1u<<AMP_PHIT)|(1u<<AMP_INVHIT));
        score=100000;
        do_score(&a);
        assert(score==90000);
    }
    /* Campaign co-op must share the rule; Deathmatch was deliberately
     * excluded above. Both flags remain independent in co-op. */
    avp_amp_mp_set_predator_honor_coop(1);
    {
        AvpAmp a={0};
        a.creature=AC_HUMAN;
        a.flags=(u16)((1u<<AMP_PHIT)|(1u<<AMP_INVHIT)|(1u<<AMP_CLOSEHIT));
        score=100000;do_score(&a);assert(score==102500);
        avp_amp_set_predator_cloak_honor_options(1,0);
        score=100000;do_score(&a);assert(score==100000);
    }
    avp_amp_mp_set_campaign_type(-1);
    /* MP teardown must clear the co-op exception. */
    avp_amp_mp_set_campaign_type(PT_PREDATOR);
    score=100000;
    {
        AvpAmp a={0};a.creature=AC_HUMAN;
        a.flags=(u16)((1u<<AMP_PHIT)|(1u<<AMP_INVHIT)|(1u<<AMP_CLOSEHIT));
        do_score(&a);assert(score==95000);
    }
    avp_amp_mp_set_campaign_type(-1);
    avp_amp_set_predator_cloak_honor_options(0,0);
    puts("R177 Predator cloaked-kill honor: default, no loss, half, priority, SP/co-op, DM excluded PASS");
    return 0;
}
