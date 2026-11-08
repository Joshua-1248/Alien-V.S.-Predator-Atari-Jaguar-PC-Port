#ifndef AVP_LOCAL_MP_AMBIENT_POLICY_H
#define AVP_LOCAL_MP_AMBIENT_POLICY_H
#include "local_mp.h"
/* PC-only shared-audio policy.  Original RDB.DTA descriptors supply the
 * actual sound; only level selection is new multiplayer behavior.  The
 * Sub-Level 2/3/4 ambience is selected via retail sub-level 2 (no sample
 * duplication, no substituted music asset). Returns 0 for invalid input. */
static inline unsigned avp_mp_ambient_target(const unsigned *levels,unsigned count)
{
    unsigned common;
    if(!levels||!count||count>AVP_LOCAL_MP_MAX_PLAYERS)return 0;
    common=levels[0];
    if(common<1u||common>15u)return 0;
    for(unsigned p=1;p<count;p++){
        if(levels[p]<1u||levels[p]>15u)return 0;
        if(levels[p]!=common)common=0u;
    }
    return common?common:2u;
}
#endif
