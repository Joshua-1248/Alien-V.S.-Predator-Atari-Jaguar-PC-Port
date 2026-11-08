#include "local_mp_worlds.h"
#include <string.h>

static int backend_ok(const AvpMpWorldBackend *o)
{
    return o && o->create_player && o->destroy_player && o->create_world &&
           o->destroy_world && o->enter_world && o->exit_world &&
           o->tick_world && o->render_player;
}

static AvpMpActiveWorld *find_world(AvpMpSession *s, unsigned level)
{
    unsigned w;
    for (w = 0; w < AVP_LOCAL_MP_MAX_ACTORS; ++w)
        if (s->worlds[w].context && s->worlds[w].campaign == s->setup.campaign &&
            s->worlds[w].level == level) return &s->worlds[w];
    return NULL;
}

static AvpMpActiveWorld *player_world(AvpMpSession *s, unsigned player)
{
    AvpMpActiveWorld *w;
    if (!s || !s->running || player >= avp_local_mp_total_actors(&s->setup)) return NULL;
    w = find_world(s, s->setup.level[player]);
    return w && (w->member_mask & (1u << player)) ? w : NULL;
}

void avp_mp_session_stop(AvpMpSession *s)
{
    unsigned w, p;
    if (!s) return;
    if (backend_ok(&s->backend)) {
        for (w = 0; w < AVP_LOCAL_MP_MAX_ACTORS; ++w) {
            AvpMpActiveWorld *world = &s->worlds[w];
            if (!world->context) continue;
            for (p = 0; p < AVP_LOCAL_MP_MAX_ACTORS; ++p)
                if ((world->member_mask & (1u << p)) && s->player[p])
                    s->backend.exit_world(s->user, world->context, p, s->player[p]);
            s->backend.destroy_world(s->user, world->context);
        }
        for (p = 0; p < AVP_LOCAL_MP_MAX_ACTORS; ++p)
            if (s->player[p]) s->backend.destroy_player(s->user, s->player[p]);
    }
    memset(s, 0, sizeof(*s));
}

int avp_mp_session_start(AvpMpSession *s, const AvpLocalMpSetup *setup,
                         const AvpMpWorldBackend *ops, void *user)
{
    unsigned p, i;
    AvpLocalMpSetup normalized;
    if (!s || !setup || !backend_ok(ops) || s->running) return 0;
    normalized = *setup;
    avp_local_mp_validate(&normalized);
    if (!normalized.enabled) return 0;
    /* start() requires a zeroed/previously stopped object. Do not guess
     * ownership of arbitrary pointers in an uninitialized object. */
    memset(s, 0, sizeof(*s));
    s->backend = *ops;
    s->user = user;
    s->setup = normalized;
    s->running = 1;
    for (p = 0; p < avp_local_mp_total_actors(&normalized); ++p) {
        AvpMpActiveWorld *w;
        if (!ops->create_player(user, p, normalized.species[p], &s->player[p]) ||
            !s->player[p]) goto fail;
        w = find_world(s, normalized.level[p]);
        if (!w) {
            for (i = 0; i < AVP_LOCAL_MP_MAX_ACTORS; ++i)
                if (!s->worlds[i].context) break;
            if (i == AVP_LOCAL_MP_MAX_ACTORS) goto fail;
            w = &s->worlds[i];
            if (!ops->create_world(user, normalized.campaign, normalized.level[p],
                                   &w->context) || !w->context) goto fail;
            w->campaign = normalized.campaign;
            w->level = normalized.level[p];
        }
        if (!ops->enter_world(user, w->context, p, s->player[p])) goto fail;
        w->member_mask |= 1u << p;
    }
    return 1;
fail:
    avp_mp_session_stop(s);
    return 0;
}

int avp_mp_session_tick(AvpMpSession *s,
                        const AvpMpPlayerCommand commands[AVP_LOCAL_MP_MAX_ACTORS])
{
    unsigned i;
    if (!s || !s->running || s->faulted || !commands) return 0;
    for (i = 0; i < AVP_LOCAL_MP_MAX_ACTORS; ++i) {
        AvpMpActiveWorld *w = &s->worlds[i];
        if (!w->context) continue;
        if (!s->backend.tick_world(s->user, w->context, w->member_mask,
                                   s->player, commands, s->tick + 1u)) {
            s->faulted = 1;
            return 0;
        }
        ++w->ticks;
    }
    ++s->tick;
    return 1;
}

int avp_mp_session_move(AvpMpSession *s, unsigned player, unsigned new_level)
{
    AvpMpActiveWorld *old, *dest;
    void *fresh = NULL;
    unsigned i;
    if (!s || !s->running || s->faulted || player >= avp_local_mp_total_actors(&s->setup)) return 0;
    old = player_world(s, player);
    if (!old) { s->faulted = 1; return 0; }
    if (old->level == new_level) return 1;
    dest = find_world(s, new_level);
    if (!dest) {
        /* A full four-world session can still travel: its departing player
         * necessarily owns a singleton world, whose slot can be replaced. */
        for (i = 0; i < AVP_LOCAL_MP_MAX_ACTORS; ++i)
            if (!s->worlds[i].context) break;
        if (i == AVP_LOCAL_MP_MAX_ACTORS) {
            if (old->member_mask != (1u << player)) return 0;
            dest = old;
        } else dest = &s->worlds[i];
        if (!s->backend.create_world(s->user, s->setup.campaign, new_level, &fresh) ||
            !fresh) return 0;
    }
    if (!s->backend.enter_world(s->user, fresh ? fresh : dest->context,
                                player, s->player[player])) {
        if (fresh) s->backend.destroy_world(s->user, fresh);
        return 0;
    }
    /* Transition commits ONLY after destination creation and join succeed. */
    s->backend.exit_world(s->user, old->context, player, s->player[player]);
    old->member_mask &= ~(1u << player);
    if (!old->member_mask) {
        s->backend.destroy_world(s->user, old->context);
        memset(old, 0, sizeof(*old));
    }
    if (fresh) {
        dest->context = fresh;
        dest->campaign = s->setup.campaign;
        dest->level = new_level;
        dest->member_mask = 0;
        dest->ticks = 0;
    }
    dest->member_mask |= 1u << player;
    s->setup.level[player] = new_level;
    return 1;
}

int avp_mp_session_move_group(AvpMpSession *s,unsigned mask,unsigned new_level)
{
    AvpMpActiveWorld *old=NULL,*dest=NULL;
    void *fresh=NULL;
    unsigned p,i;
    if(!s||!s->running||s->faulted||!mask||
       (mask&~((1u<<avp_local_mp_total_actors(&s->setup))-1u))||!new_level||
       !s->backend.enter_world_group)return 0;
    for(p=0;p<avp_local_mp_total_actors(&s->setup);p++)if(mask&(1u<<p)){
        AvpMpActiveWorld *w=player_world(s,p);
        if(!w||!s->player[p]||(old&&old!=w))return 0;
        old=w;
    }
    if(!old||old->level==new_level)return 0;
    dest=find_world(s,new_level);
    if(!dest){
        for(i=0;i<AVP_LOCAL_MP_MAX_ACTORS;i++)
            if(!s->worlds[i].context)break;
        if(i==AVP_LOCAL_MP_MAX_ACTORS){
            /* Recycle only when the entire source-world population leaves. */
            if(old->member_mask!=mask)return 0;
            dest=old;
        }else dest=&s->worlds[i];
        if(!s->backend.create_world(s->user,s->setup.campaign,new_level,&fresh)||
           !fresh)return 0;
    }
    /* Batch join's failure-atomic contract is essential: an exit may only
     * commit once ALL arriving passengers and their positions are valid. */
    if(!s->backend.enter_world_group(s->user,fresh?fresh:dest->context,
                                     mask,s->player)){
        if(fresh)s->backend.destroy_world(s->user,fresh);
        return 0;
    }
    for(p=0;p<avp_local_mp_total_actors(&s->setup);p++)if(mask&(1u<<p))
        s->backend.exit_world(s->user,old->context,p,s->player[p]);
    old->member_mask&=~mask;
    if(!old->member_mask){
        s->backend.destroy_world(s->user,old->context);
        memset(old,0,sizeof(*old));
    }
    if(fresh){
        dest->context=fresh;dest->campaign=s->setup.campaign;
        dest->level=new_level;dest->member_mask=0;dest->ticks=0;
    }
    dest->member_mask|=mask;
    for(p=0;p<avp_local_mp_total_actors(&s->setup);p++)if(mask&(1u<<p))
        s->setup.level[p]=new_level;
    return 1;
}

unsigned avp_mp_session_compose(AvpMpSession *s, const AvpLocalMpOutputFrame *output,
                                uint32_t clear_color)
{
    AvpLocalMpSourceFrame frames[AVP_LOCAL_MP_MAX_PLAYERS] = {{0}};
    unsigned p;
    if (!s || !s->running || s->faulted) return 0;
    for (p = 0; p < s->setup.players; ++p) {
        AvpMpActiveWorld *w = player_world(s, p);
        unsigned q;
        if (!w || !s->backend.render_player(s->user, w->context, p,
                                             s->player[p], &frames[p]) ||
            !frames[p].pixels) return 0;
        /* A reused frame buffer is not an independently rendered view;
         * reject it before ever touching the presentation destination. */
        for (q = 0; q < p; ++q)
            if (frames[q].pixels == frames[p].pixels) return 0;
    }
    return avp_local_mp_compose(&s->setup, frames, output, clear_color);
}

unsigned avp_mp_session_world_count(const AvpMpSession *s)
{
    unsigned n = 0, i;
    if (!s || !s->running) return 0;
    for (i = 0; i < AVP_LOCAL_MP_MAX_ACTORS; ++i)
        if (s->worlds[i].context) ++n;
    return n;
}

const AvpMpActiveWorld *avp_mp_session_player_world(const AvpMpSession *s,
                                                   unsigned player)
{
    return player_world((AvpMpSession *)s, player);
}
