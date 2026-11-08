#include "local_mp_amp_targets.h"
#include "local_mp_native_context.h"
#include "joypad.h"
#include <stddef.h>

/* Defined inside the original AMP.S C unit by the guarded .inc above. */
extern int avp_mp_amp_native_update_scoped(void *ctx,
                 int (*enter)(void *, const AvpAmp *, unsigned),
                 int (*leave)(void *),
                 int (*acid)(void *, const AvpAmp *));

typedef struct AvpMpAmpTargetVisit {
    AvpMpActorNativeState *const *actors;
    const AvpMpActorNativeState *candidates[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned mask;
    AvpMpAmpChooseTarget choose;
    void *user;
    AvpMpActorNativeScope scope;
    AvpMpActorNativeState *selected;
    int prior_attack_disabled,prior_facehug_disabled;
} AvpMpAmpTargetVisit;

static int visit_enter(void *ctx, const AvpAmp *amp, unsigned index)
{
    AvpMpAmpTargetVisit *v=(AvpMpAmpTargetVisit *)ctx;
    int target;
    v->prior_attack_disabled=avp_amp_attacks_disabled();
    v->prior_facehug_disabled=avp_amp_facehug_attach_disabled();
    target=v->choose(v->user,amp,index,v->mask,v->candidates);
    /* A split-world party may leave a world with only a dead player.
     * No living target is normal during their fade/respawn, not a broken
     * callback. The original enemy modes need a bound live player, so
     * skip this mode until one becomes available. A genuinely invalid
     * target still faults the tick instead of retargeting a stale actor. */
    if(target==-1){
        avp_amp_set_attacks_disabled(v->prior_attack_disabled);
        avp_amp_set_facehug_attach_disabled(v->prior_facehug_disabled);
        return 2;
    }
    if(target<0 || (unsigned)target>=AVP_LOCAL_MP_MAX_ACTORS || !(v->mask&(1u<<(unsigned)target))){
        avp_amp_set_attacks_disabled(v->prior_attack_disabled);
        avp_amp_set_facehug_attach_disabled(v->prior_facehug_disabled);
        return 0;
    }
    v->selected=v->actors[target];
    if(!avp_mp_actor_scope_enter(&v->scope,v->selected)){
        avp_amp_set_attacks_disabled(v->prior_attack_disabled);
        avp_amp_set_facehug_attach_disabled(v->prior_facehug_disabled);
        return 0;
    }
    /* The original AMP method now sees the selected co-op actor, not a
     * single-player campaign. Do not leave this switch active across AMPs. */
    avp_amp_set_marine_multiplayer_target(1);
    return 1;
}
static int visit_leave(void *ctx)
{
    AvpMpAmpTargetVisit *v=(AvpMpAmpTargetVisit *)ctx;
    int ok;
    avp_amp_set_marine_multiplayer_target(0);
    ok=avp_mp_actor_scope_leave(&v->scope,v->selected);
    v->selected=NULL;
    avp_amp_set_attacks_disabled(v->prior_attack_disabled);
    avp_amp_set_facehug_attach_disabled(v->prior_facehug_disabled);
    return ok;
}
/* A dead Alien's acid has no exclusive enemy target. Run the *original*
 * contact/damage operation under each living local actor in this level.
 * The caller advances the shared puddle's ALTFRAME/sound exactly once.
 * Do not change the current AI target lock or retain any actor scope. */
static int visit_acid(void *ctx, const AvpAmp *puddle)
{
    AvpMpAmpTargetVisit *v=(AvpMpAmpTargetVisit *)ctx;
    int contacts=0;
    for(unsigned p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(v->mask&(1u<<p)){
        AvpMpActorNativeState *a=v->actors[p];
        AvpMpActorNativeScope scope={0};
        int hit,restored;
        if(!a || a->player.player_energy<=0 || a->player.game_over)continue;
        if(!avp_mp_actor_scope_enter(&scope,a))return -1;
        hit=avp_amp_acid_contact_player(puddle);
        restored=avp_mp_actor_scope_leave(&scope,a);
        if(!restored)return -1;
        contacts+=hit;
    }
    return contacts;
}

/* PC's existing single-player AMP scheduler supplies a drifting, eight-phase
 * emulated TOM beam sample. The shared-world native scheduler must do the
 * same: the original enemy Marine ranged-fire gate requires beam&7 == 2.
 * A zero/unadvanced beam permanently disables that Marine attack. Each
 * world has its own original readpad frame counter (bound by native scope).
 * This function is also used by the isolated regression test. */
uint32_t avp_mp_amp_host_beam_value(uint32_t world_frame)
{
    return world_frame + (world_frame >> 4);
}

int avp_mp_amp_step_targets(AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS],
                            unsigned mask, AvpMpAmpChooseTarget choose, void *user)
{
    AvpMpAmpTargetVisit v={0};
    unsigned i;
    if(!actors || !choose || !mask || (mask&~((1u<<AVP_LOCAL_MP_MAX_ACTORS)-1u)) ||
       !avp_mp_native_world_scope_active() || avp_mp_actor_scope_active())return 0;
    /* Same PC-only emulated video-beam cadence as pc_main.c's UpdateAMPs
     * path; install it ONCE while the destination world's original AMP
     * globals are bound, never on a particular player or the host world.
     * Original single-player UpdateAMPs remains untouched. */
    avp_amp_set_beam_counter(avp_mp_amp_host_beam_value(frame_counter));
    v.actors=actors;v.mask=mask;v.choose=choose;v.user=user;
    for(i=0;i<AVP_LOCAL_MP_MAX_ACTORS;i++)if(mask&(1u<<i)){
        if(!actors[i] || !avp_mp_actor_native_valid(actors[i]))return 0;
        v.candidates[i]=actors[i];
    }
    return avp_mp_amp_native_update_scoped(&v,visit_enter,visit_leave,visit_acid);
}
