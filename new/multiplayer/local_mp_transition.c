#include "local_mp_transition.h"
#include <string.h>
int avp_mp_transition_queue(AvpMpTransitionQueue *q,unsigned player,
                             int16_t level,int32_t x,int32_t y,int cocoon)
{
    AvpMpTransitionRequest r;
    if(!q||player>=AVP_LOCAL_MP_MAX_ACTORS||level<1||level>15)return 0;
    if(q->player[player].valid){
        const AvpMpTransitionRequest *old=&q->player[player];
        /* DoorKeys can report the SAME exit over successive original ticks.
         * Preserve the first intent; reject conflicting requests. */
        return old->target_level==level && old->new_x==x && old->new_y==y &&
               old->use_cocoon==(cocoon!=0);
    }
    memset(&r,0,sizeof(r));
    r.target_level=level;r.new_x=x;r.new_y=y;
    r.use_cocoon=cocoon!=0;r.valid=1u;
    q->player[player]=r;
    return 1;
}
int avp_mp_transition_same_destination(const AvpMpTransitionRequest *leader,
                                       const AvpMpTransitionRequest *other)
{
    return leader && other && leader->valid && other->valid &&
           !leader->use_cocoon && !other->use_cocoon &&
           leader->new_x>=0 && leader->new_y>=0 &&
           other->new_x>=0 && other->new_y>=0 &&
           leader->target_level==other->target_level;
}
int avp_mp_transition_peek(const AvpMpTransitionQueue *q,unsigned player,
                            AvpMpTransitionRequest *out)
{
    if(!q||!out||player>=AVP_LOCAL_MP_MAX_ACTORS||!q->player[player].valid)return 0;
    *out=q->player[player];return 1;
}
int avp_mp_transition_commit(AvpMpTransitionQueue *q,unsigned player,
                              AvpMpTransitionRequest *out)
{
    if(!avp_mp_transition_peek(q,player,out))return 0;
    memset(&q->player[player],0,sizeof(q->player[player]));return 1;
}
