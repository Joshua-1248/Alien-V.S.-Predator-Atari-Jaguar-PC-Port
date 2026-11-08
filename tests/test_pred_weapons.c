#include "weapons.h"
#include "player.h"
#include <assert.h>
s32 score; static int notice_count,last_notice,last_sfx;
static void n(enum AvpWeaponNotice x){++notice_count;last_notice=x;}
static void s(unsigned x){last_sfx=(int)x;}
int main(void){
 avp_weapons_set_callbacks(n,s); cur_weps=(1u<<4); cur_wepno=4; new_wepno=0; InitPredAvail();
 score=149999; PredAvail(); assert(cur_weps==(1u<<4));
 score=150000; PredAvail(); assert(cur_weps&(1u<<1)); assert(new_wepno==1); assert(last_notice==AVP_WNOTICE_GAIN_COMBI); assert(last_sfx==2);
 score=800000; PredAvail(); assert(cur_weps&(1u<<3)); assert(cur_weps&(1u<<2)); assert(new_wepno==2);
 /* R272: MAZESCRN.S:1446-1462 compares only cur_wepno with each revoked
  * weapon.  Cannon held, score 800000->100000: step 1 revokes the cannon
  * and requests disc (3); steps 2-3 revoke disc/combi without touching
  * new_wepno.  The request for the revoked disc is later cancelled by
  * MAZESCRN.S:2524-2536 -- there is no wrist-blade fallback. */
 cur_wepno=2; score=100000; PredAvail(); assert(!(cur_weps&(1u<<2))); assert(!(cur_weps&(1u<<3))); assert(!(cur_weps&(1u<<1))); assert(new_wepno==3); assert(last_sfx==1);
 assert(cur_weps&(1u<<4));assert(!(cur_weps&((1u<<1)|(1u<<2)|(1u<<3))));
 /* R272: thresholds are unsigned (BHS/BLO): a negative score is a huge
  * unsigned value and awards every rank rather than revoking. */
 cur_weps=(1u<<4); cur_wepno=4; new_wepno=0; InitPredAvail();
 score=-5; PredAvail(); assert((cur_weps&0x1eu)==0x1eu); assert(new_wepno==2); assert(last_sfx==2);
 /* Revoking the equipped disc while combi-stick remains also selects the
  * highest genuinely available weapon, not a stale pending award. */
 cur_weps=(u8)((1u<<4)|(1u<<1));cur_wepno=3;new_wepno=3;
 score=100000;InitPredAvail();score=400000;PredAvail();
 score=200000;PredAvail();assert(new_wepno==1);
 /* Prev/Next must never expose slot 0 or Predator ability slots 5/6. */
 cur_weps=(u8)((1u<<4)|(1u<<5)|(1u<<6)); cur_wepno=4; new_wepno=0;
 avp_weapons_cycle(1,6);assert(new_wepno==4);
 avp_weapons_cycle(-1,6);assert(new_wepno==4);
 cur_weps=(u8)((1u<<1)|(1u<<3)|(1u<<5));cur_wepno=1;new_wepno=0;
 avp_weapons_cycle(-1,6);assert(new_wepno==3);
 avp_weapons_cycle(1,6);assert(new_wepno==1);
 return 0;
}
