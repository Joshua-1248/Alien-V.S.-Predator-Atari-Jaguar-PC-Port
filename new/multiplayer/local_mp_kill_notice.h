#ifndef AVP_LOCAL_MP_KILL_NOTICE_H
#define AVP_LOCAL_MP_KILL_NOTICE_H
#include "local_mp.h"
#include <stdint.h>

/* PC-local Deathmatch HUD notifications. No original Jaguar message/state
 * changes; time is measured in the authoritative 10 Hz multiplayer ticks. */
enum {
    AVP_MP_NOTICE_NONE=0u,
    AVP_MP_NOTICE_KILLED=1u,
    AVP_MP_NOTICE_KILLED_BY=2u
};
#define AVP_MP_NOTICE_DURATION_TICKS 30u

typedef struct AvpMpKillNotice {
    unsigned kind;
    unsigned other_player; /* 1-based roster number, includes bots P5..P16 */
    uint64_t expires_at;
} AvpMpKillNotice;

static inline void avp_mp_kill_notice_reset(AvpMpKillNotice *n)
{
    if(!n)return;
    n->kind=AVP_MP_NOTICE_NONE;
    n->other_player=0u;
    n->expires_at=0u;
}

/* Only real scored PvP eliminations create notifications. Non-player deaths,
 * self-damage and invalid actors never produce KILLED/KILLED BY messages. */
static inline void avp_mp_kill_notice_record(
    AvpMpKillNotice notices[AVP_LOCAL_MP_MAX_PLAYERS],
    unsigned humans,unsigned actors,unsigned killer,unsigned victim,
    uint64_t tick)
{
    if(!notices||humans>AVP_LOCAL_MP_MAX_PLAYERS||
       actors>AVP_LOCAL_MP_MAX_ACTORS||
       killer>=actors||victim>=actors||killer==victim)return;
    if(killer<humans){
        notices[killer].kind=AVP_MP_NOTICE_KILLED;
        notices[killer].other_player=victim+1u;
        notices[killer].expires_at=tick+AVP_MP_NOTICE_DURATION_TICKS;
    }
    if(victim<humans){
        notices[victim].kind=AVP_MP_NOTICE_KILLED_BY;
        notices[victim].other_player=killer+1u;
        notices[victim].expires_at=tick+AVP_MP_NOTICE_DURATION_TICKS;
    }
}

static inline unsigned avp_mp_kill_notice_active(
    const AvpMpKillNotice *n,uint64_t tick,unsigned *other_player)
{
    if(!n||tick>=n->expires_at||
       (n->kind!=AVP_MP_NOTICE_KILLED&&n->kind!=AVP_MP_NOTICE_KILLED_BY)||
       n->other_player<1u||n->other_player>AVP_LOCAL_MP_MAX_ACTORS)
        return AVP_MP_NOTICE_NONE;
    if(other_player)*other_player=n->other_player;
    return n->kind;
}
/* Match the HUD_MSG pickup's 5-tick illuminated/blank flash cadence.
 * The existing 30-tick notice lifetime is unchanged; 3 bright periods and
 * 3 dark periods use simulation ticks, never number of presented frames. */
static inline unsigned avp_mp_kill_notice_visible(
    const AvpMpKillNotice *n,uint64_t tick,unsigned *other_player)
{
    if(!n || tick>=n->expires_at ||
       n->expires_at<AVP_MP_NOTICE_DURATION_TICKS ||
       tick<n->expires_at-AVP_MP_NOTICE_DURATION_TICKS ||
       (((tick-(n->expires_at-AVP_MP_NOTICE_DURATION_TICKS))/5u)&1u))
        return AVP_MP_NOTICE_NONE;
    return avp_mp_kill_notice_active(n,tick,other_player);
}


#endif
