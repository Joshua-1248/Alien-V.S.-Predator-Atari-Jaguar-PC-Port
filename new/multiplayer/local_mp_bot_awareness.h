#ifndef AVP_LOCAL_MP_BOT_AWARENESS_H
#define AVP_LOCAL_MP_BOT_AWARENESS_H
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
/* PC-only bot knowledge; no movement, damage or original AMP state changes.
 * The HUD/MAZE4 tracker scales 1 tile to 3 metres and computes:
 * 3 * max(dx,dy) + min(dx,dy) < 15 metres. Sight must also be inside
 * the forward viewing cone and the ORIGINAL LineOfSight test. */
#define AVP_BOT_MEMORY_TICKS 40u /* last observed position, 4 original seconds */
#define AVP_BOT_SIGHT_TILES 8u
#define AVP_BOT_TRAIL_LENGTH 8u
#define AVP_BOT_TRAIL_MIN_STEP 16384u /* 0.25 original tile */
#define AVP_BOT_FOV_COS 0.34202014332566873 /* forward +/-70 degrees */
/* Invisible PC-side target movement samples. These are NOT rendered and
 * cannot confer knowledge without a current forward sight ray to the sample.
 * One trace is kept per actual AMP generation / player life / source level. */
static inline int avp_bot_in_view(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
typedef struct AvpMpBotTrailPoint {
    uint32_t x,y;
    uint64_t tick;
} AvpMpBotTrailPoint;
typedef struct AvpMpBotTrail {
    AvpMpBotTrailPoint points[AVP_BOT_TRAIL_LENGTH];
    uint32_t serial;
    unsigned level,next,count;
} AvpMpBotTrail;
typedef int (*AvpBotBreadcrumbLos)(void *,uint32_t,uint32_t,
                                      uint32_t,uint32_t);
static inline void avp_bot_trail_clear(AvpMpBotTrail *t)
{ if(t){ t->count=0u;t->next=0u;t->serial=0u;t->level=0u; } }
static inline void avp_bot_trail_sample(AvpMpBotTrail *t,unsigned level,
    uint32_t serial,uint64_t tick,uint32_t x,uint32_t y)
{
    const AvpMpBotTrailPoint *last;
    if(!t)return;
    if(!serial)serial=1u;
    if(t->level!=level||t->serial!=serial){t->count=t->next=0u;}
    t->level=level;t->serial=serial;
    if(t->count){
        last=&t->points[(t->next+AVP_BOT_TRAIL_LENGTH-1u)%AVP_BOT_TRAIL_LENGTH];
        if(tick<=last->tick || (
           (uint64_t)llabs((long long)((int64_t)x-(int64_t)last->x))<AVP_BOT_TRAIL_MIN_STEP &&
           (uint64_t)llabs((long long)((int64_t)y-(int64_t)last->y))<AVP_BOT_TRAIL_MIN_STEP))
            return;
    }
    t->points[t->next]=(AvpMpBotTrailPoint){x,y,tick};
    t->next=(t->next+1u)%AVP_BOT_TRAIL_LENGTH;
    if(t->count<AVP_BOT_TRAIL_LENGTH)++t->count;
}
/* Newest CURRENTLY VISIBLE trace point wins. A trace behind a closed wall,
 * outside the bot's visual cone, or older than its native memory does not
 * reveal the target. Original native LineOfSight is supplied by caller. */
static inline int avp_bot_trail_visible(const AvpMpBotTrail *t,
    unsigned level,uint32_t serial,uint64_t now,uint32_t heading,
    uint32_t eye_x,uint32_t eye_y,AvpBotBreadcrumbLos los,void *ctx,
    AvpMpBotTrailPoint *found)
{
    if(!t||!los||!found||!t->count||t->level!=level||
       t->serial!=(serial?serial:1u))return 0;
    for(unsigned i=0;i<t->count;i++){
        const unsigned idx=(t->next+AVP_BOT_TRAIL_LENGTH-1u-i)%AVP_BOT_TRAIL_LENGTH;
        const AvpMpBotTrailPoint *b=&t->points[idx];
        if(now<b->tick || now-b->tick>AVP_BOT_MEMORY_TICKS)continue;
        if(!avp_bot_in_view(heading,eye_x,eye_y,b->x,b->y))continue;
        if(los(ctx,eye_x,eye_y,b->x,b->y)){*found=*b;return 1;}
    }
    return 0;
}
typedef struct AvpMpBotContact {
    uint32_t x,y,serial;
    unsigned level,known,sight,tracker;
    uint64_t last_seen_tick,investigated_tick;
} AvpMpBotContact;
static inline uint64_t avp_bot_abs_offset(int64_t n)
{ return (uint64_t)(n<0?-n:n); }
static inline int avp_bot_motion_range(uint32_t ax,uint32_t ay,uint32_t bx,uint32_t by)
{
    uint64_t dx=avp_bot_abs_offset((int64_t)ax-(int64_t)bx);
    uint64_t dy=avp_bot_abs_offset((int64_t)ay-(int64_t)by);
    const uint64_t major=dx>dy?dx:dy,minor=dx>dy?dy:dx;
    return major*3u+minor < ((uint64_t)15u<<16);
}
static inline int avp_bot_in_view(uint32_t heading,uint32_t x,uint32_t y,
                                   uint32_t tx,uint32_t ty)
{
    double dx=(double)((int64_t)tx-(int64_t)x);
    double dy=(double)((int64_t)ty-(int64_t)y);
    double angle=(double)(heading>>16)*(6.2831853071795864769/65536.0);
    double distance=sqrt(dx*dx+dy*dy);
    if(distance<1.0)return 1;
    if(distance>=((double)AVP_BOT_SIGHT_TILES*65536.0))return 0;
    return (dx*cos(angle)-dy*sin(angle))>=distance*AVP_BOT_FOV_COS;
}
/* Serial is the actual original per-world AMP slot generation. For player
 * contacts it is the target's death count + 1: respawns invalidate old intel.
 * Sight and radar flags represent THIS tick ONLY. Cached positions never
 * update through walls and expire after a short search. */
/* Remember a sighted breadcrumb at its ORIGINAL sampling time, not the
 * current time: repeatedly looking at one stale mark does not create a
 * permanent radar track. Never replace more recent direct sight/tracker intel. */
static inline void avp_bot_contact_breadcrumb(AvpMpBotContact *c,
    const AvpMpBotTrailPoint *p,uint64_t now)
{
    if(!c||!p||now<p->tick||now-p->tick>AVP_BOT_MEMORY_TICKS ||
       c->sight||c->tracker || p->tick<=c->investigated_tick ||
       (c->known && c->last_seen_tick>=p->tick))return;
    c->x=p->x;c->y=p->y;c->last_seen_tick=p->tick;c->known=1u;
}
/* Reaching an old breadcrumb with no fresh sight means SEARCH, not waiting
 * motionless for the old clue to expire. The same clue cannot reacquire the
 * target on the next tick; newer visible steps still can. */
static inline void avp_bot_contact_investigated(AvpMpBotContact *c)
{
    if(!c)return;
    if(c->known && c->last_seen_tick>c->investigated_tick)
        c->investigated_tick=c->last_seen_tick;
    c->known=0u;
}
static inline void avp_bot_contact_update_budget(AvpMpBotContact *c,
    unsigned level,uint32_t serial,uint64_t tick,uint32_t x,uint32_t y,
    int visible,int radar,unsigned memory_ticks)
{
    if(!c)return;
    if(!serial)serial=1u;
    if(c->level!=level||c->serial!=serial){
        c->known=0u;c->last_seen_tick=0u;c->investigated_tick=0u;
    }
    c->level=level;c->serial=serial;
    c->sight=!!visible;c->tracker=!!radar;
    if(visible||radar){
        c->x=x;c->y=y;c->known=1u;c->last_seen_tick=tick;
    }else if(c->known && (tick<c->last_seen_tick ||
                tick-c->last_seen_tick>memory_ticks))c->known=0u;
}
/* Existing callers retain the original 4-second Balanced memory window. */
static inline void avp_bot_contact_update(AvpMpBotContact *c,
    unsigned level,uint32_t serial,uint64_t tick,uint32_t x,uint32_t y,
    int visible,int radar)
{
    avp_bot_contact_update_budget(c,level,serial,tick,x,y,visible,radar,
                                  AVP_BOT_MEMORY_TICKS);
}
#endif
