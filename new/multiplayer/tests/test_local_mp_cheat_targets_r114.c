/* R114: none of the per-player no-attack/no-latch switches may protect
 * another player or a bot. Retained AMP target locks use this same mapper. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "local_mp_cheat_targets.h"

static void check(AvpMpInputConfig *p,unsigned who,int p1a,int p1h,int a,int h)
{
    AvpMpTargetCheatFlags f=avp_mp_target_cheats(p,who,p1a,p1h);
    assert(f.no_enemy_attack==a);
    assert(f.no_facehug==h);
}
int main(void)
{
    AvpMpInputConfig p;
    memset(&p,0,sizeof(p));
    p.player[1].cheat_no_facehug=1;
    p.player[2].cheat_no_enemy_attack=1;
    p.player[3].cheat_no_facehug=1;
    p.player[3].cheat_no_enemy_attack=1;
    /* A P1 cheat never spreads to P2-P4; the independently selected P2
     * latch restriction does not suppress P2's ordinary enemy attacks. */
    check(&p,0,1,1,1,1);
    check(&p,1,1,1,0,1);
    check(&p,2,1,1,1,0);
    check(&p,3,1,1,1,1);
    check(&p,4,1,1,0,0); /* bot, NOT P2 */
    check(&p,15,1,1,0,0);
    /* After changing target/locked target, flag computation is stateless.
     * Nothing from a previous enemy visit is inherited. */
    for(unsigned j=0;j<300u;j++){
        check(&p,2,0,0,1,0);
        check(&p,1,0,0,0,1);
        check(&p,0,0,0,0,0);
        check(&p,3,0,0,1,1);
        check(&p,5,0,0,0,0);
    }
    p.player[1].cheat_no_facehug=0;
    check(&p,1,0,0,0,0);
    puts("PASS: P1-P4 and bots keep independent enemy-attack/facehugger cheats");
    return 0;
}
