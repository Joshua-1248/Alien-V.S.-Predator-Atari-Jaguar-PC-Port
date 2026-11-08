#include "local_mp_native_backend.h"
#include <stdlib.h>
#include <string.h>

int avp_mp_native_backend_valid(const AvpMpNativeBackendHost *h)
{
    return h && h->profiles && h->create_actor && h->create_level &&
        h->destroy_level && h->join_level && h->leave_level &&
        h->prepare_player1 && h->advance_actor && h->advance_level && h->draw_actor;
}

static int create_actor(void *u,unsigned player,unsigned species,void **out)
{
    AvpMpNativeBackendHost *h=(AvpMpNativeBackendHost *)u;
    AvpMpActorNativeState *a;
    if(!out || !avp_mp_native_backend_valid(h) || player>=AVP_LOCAL_MP_MAX_ACTORS)return 0;
    *out=NULL;
    a=(AvpMpActorNativeState *)calloc(1,sizeof(*a));
    if(!a)return 0;
    if(!h->create_actor(h->user,player,species,a) ||
       !avp_mp_actor_native_valid(a)) {
        free(a);return 0;
    }
    *out=a;return 1;
}
static void destroy_actor(void *u,void *a)
{(void)u;free(a);}
static int create_level(void *u,unsigned campaign,unsigned level,void **out)
{
    AvpMpNativeBackendHost *h=(AvpMpNativeBackendHost *)u;
    AvpMpNativeWorld *w;
    if(!out || !avp_mp_native_backend_valid(h))return 0;
    *out=NULL;
    w=(AvpMpNativeWorld *)calloc(1,sizeof(*w));
    if(!w)return 0;
    if(!h->create_level(h->user,campaign,level,w) ||
        !avp_mp_native_world_valid(w)) {
        /* The factory must clean up its own failed partial initialization. */
        free(w);return 0;
    }
    *out=w;return 1;
}
static void destroy_level(void *u,void *w)
{
    AvpMpNativeBackendHost *h=(AvpMpNativeBackendHost *)u;
    if(w && h && h->destroy_level) h->destroy_level(h->user,(AvpMpNativeWorld *)w);
    free(w);
}
static int enter_level(void *u,void *w,unsigned player,void *a)
{
    AvpMpNativeBackendHost *h=(AvpMpNativeBackendHost *)u;
    if(!avp_mp_native_backend_valid(h) || !w || !a)return 0;
    return h->join_level(h->user,(AvpMpNativeWorld *)w,player,
                         (AvpMpActorNativeState *)a);
}
static int enter_level_group(void *u,void *w,unsigned mask,
                             void *const players[AVP_LOCAL_MP_MAX_ACTORS])
{
    AvpMpNativeBackendHost *h=(AvpMpNativeBackendHost *)u;
    AvpMpActorNativeState staged[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS]={0};
    unsigned p;
    if(!h||!h->join_group||!w||!mask||(mask&~((1u<<AVP_LOCAL_MP_MAX_ACTORS)-1u))||!players)return 0;
    for(p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(mask&(1u<<p)){
        if(!players[p])return 0;
        staged[p]=*(const AvpMpActorNativeState *)players[p];
        actors[p]=&staged[p];
    }
    if(!h->join_group(h->user,(AvpMpNativeWorld *)w,mask,actors))return 0;
    for(p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(mask&(1u<<p)){
        if(!avp_mp_actor_native_valid(&staged[p]))return 0;
    }
    for(p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(mask&(1u<<p))
        *(AvpMpActorNativeState *)players[p]=staged[p];
    return 1;
}
static void exit_level(void *u,void *w,unsigned player,void *a)
{
    AvpMpNativeBackendHost *h=(AvpMpNativeBackendHost *)u;
    if(h && h->leave_level && w && a)
        h->leave_level(h->user,(AvpMpNativeWorld *)w,player,
                       (AvpMpActorNativeState *)a);
}
typedef struct AvpMpBackendTick {
    AvpMpNativeBackendHost *host;
    AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned mask;
} AvpMpBackendTick;
static int actor_step(void *u,unsigned p)
{
    AvpMpBackendTick *t=(AvpMpBackendTick *)u;
    return t->host->advance_actor(t->host->user,p);
}
static int world_step(void *u)
{
    AvpMpBackendTick *t=(AvpMpBackendTick *)u;
    /* Even a world occupied only by a dying player needs a fresh source AMP
     * snapshot: that player's movement callback intentionally does not run. */
    if(t->host->snapshot_world_presentation &&
       !t->host->snapshot_world_presentation(t->host->user,t->mask))return 0;
    if(t->host->choose_amp_target &&
       !avp_mp_amp_step_targets(t->actors,t->mask,
                                 t->host->choose_amp_target,t->host->user))
        return 0;
    if(!t->host->advance_level(t->host->user))return 0;
    if(t->host->advance_actor_post_world){
        for(unsigned p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(t->mask&(1u<<p)){
            AvpMpActorNativeScope scope={0};
            int stepped,restored;
            if(!avp_mp_actor_scope_enter(&scope,t->actors[p]))return 0;
            stepped=t->host->advance_actor_post_world(t->host->user,p);
            restored=avp_mp_actor_scope_leave(&scope,t->actors[p]);
            if(!stepped||!restored)return 0;
        }
    }
    return 1;
}
static int tick_level(void *u,void *level,unsigned mask,
                      void *const players[AVP_LOCAL_MP_MAX_ACTORS],
                      const AvpMpPlayerCommand commands[AVP_LOCAL_MP_MAX_ACTORS],
                      uint64_t tick)
{
    AvpMpNativeBackendHost *h=(AvpMpNativeBackendHost *)u;
    AvpMpActorNativeState *a[AVP_LOCAL_MP_MAX_ACTORS]={0};
    AvpMpActorNativeState staged[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned p;
    /* Headless bots have no physical device or human F12 profile. Never
     * borrow P2's mouse tuning or controller state for P5-P8. */
    static const AvpMpInputProfile headless={
        .mouse_sensitivity=1.0f,
        .gamepad_turn_sensitivity=1.0f,
        .gamepad_turn_speed=1.0f
    };
    AvpMpBackendTick native_tick={0};
    (void)tick;
    if(!avp_mp_native_backend_valid(h)||!level||!players||!commands||
       !mask||(mask&~((1u<<AVP_LOCAL_MP_MAX_ACTORS)-1u)))return 0;
    /* Validate ALL input samples before publishing any player mutation. */
    for(p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(mask&(1u<<p)) {
        if(!players[p])return 0;
        staged[p]=*(const AvpMpActorNativeState *)players[p];
        if(p==0) {
            if(!h->prepare_player1(h->user,&staged[p],&commands[p]))return 0;
        }else if(!avp_mp_native_input_prepare(&staged[p],&commands[p],
                                               p<AVP_LOCAL_MP_MAX_PLAYERS?&h->profiles->player[p]:&headless))return 0;
    }
    for(p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(mask&(1u<<p)) {
        *(AvpMpActorNativeState *)players[p]=staged[p];
        a[p]=(AvpMpActorNativeState *)players[p];
    }
    native_tick.host=h;
    native_tick.mask=mask;
    for(p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)native_tick.actors[p]=a[p];
    return avp_mp_native_run_world((AvpMpNativeWorld *)level,a,mask,
                                   actor_step,world_step,&native_tick);
}
static int render_actor(void *u,void *level,unsigned player,void *actor,
                        AvpLocalMpSourceFrame *out)
{
    AvpMpNativeBackendHost *h=(AvpMpNativeBackendHost *)u;
    AvpMpNativeScope scope={0};
    int drawn,actor_ok,level_ok;
    if(!avp_mp_native_backend_valid(h)||!level||!actor||!out)return 0;
    if(!avp_mp_native_world_enter(&scope,(AvpMpNativeWorld *)level))return 0;
    if(!avp_mp_native_actor_enter(&scope,(AvpMpActorNativeState *)actor)){
        (void)avp_mp_native_world_leave(&scope,(AvpMpNativeWorld *)level);
        return 0;
    }
    drawn=h->draw_actor(h->user,(AvpMpNativeWorld *)level,player,
                        (AvpMpActorNativeState *)actor,out);
    actor_ok=avp_mp_native_actor_leave(&scope,(AvpMpActorNativeState *)actor);
    level_ok=avp_mp_native_world_leave(&scope,(AvpMpNativeWorld *)level);
    return drawn && actor_ok && level_ok && out->pixels;
}
AvpMpWorldBackend avp_mp_native_backend_ops(void)
{
    AvpMpWorldBackend ops;
    memset(&ops,0,sizeof(ops));
    ops.create_player=create_actor;
    ops.destroy_player=destroy_actor;
    ops.create_world=create_level;
    ops.destroy_world=destroy_level;
    ops.enter_world=enter_level;
    ops.enter_world_group=enter_level_group;
    ops.exit_world=exit_level;
    ops.tick_world=tick_level;
    ops.render_player=render_actor;
    return ops;
}
