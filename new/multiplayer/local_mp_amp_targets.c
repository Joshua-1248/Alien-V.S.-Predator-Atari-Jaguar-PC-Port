#include "local_mp_amp_targets.h"
#include "local_mp_native_context.h"
#include "joypad.h"
#include <stddef.h>

/* Defined inside the original AMP.S C unit by the guarded .inc above. */
extern int avp_mp_amp_native_update_scoped(void *ctx,
                 int (*enter)(void *, const AvpAmp *, unsigned),
                 int (*leave)(void *),
                 int (*acid)(void *, const AvpAmp *),int local_acid_audio);

typedef struct AvpMpAmpTargetVisit {
    AvpMpActorNativeState *const *actors;
    const AvpMpActorNativeState *candidates[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpActorMask mask;
    AvpMpAmpChooseTarget choose;
    void *user;
    AvpMpActorNativeScope scope;
    AvpMpActorNativeState *selected;
    int prior_attack_disabled,prior_facehug_disabled;
    AvpMpActorMask *acid_contact_masks;
    AvpMpAmpPlayerAcidCue acid_cue;
    AvpMpAmpEnemySoundScope enemy_sound;
    const AvpAmp *active_enemy;
    unsigned selected_index;
    AvpMpAmpActorDamage on_damage;
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
    if(target<0 || (unsigned)target>=AVP_LOCAL_MP_MAX_ACTORS || !(v->mask&(AVP_MP_ACTOR_BIT((unsigned)target)))){
        avp_amp_set_attacks_disabled(v->prior_attack_disabled);
        avp_amp_set_facehug_attach_disabled(v->prior_facehug_disabled);
        return 0;
    }
    v->selected_index=(unsigned)target;
    v->selected=v->actors[target];
    if(!avp_mp_actor_scope_enter(&v->scope,v->selected)){
        avp_amp_set_attacks_disabled(v->prior_attack_disabled);
        avp_amp_set_facehug_attach_disabled(v->prior_facehug_disabled);
        return 0;
    }
    /* The original AMP method now sees the selected co-op actor, not a
     * single-player campaign. Do not leave this switch active across AMPs. */
    avp_amp_set_marine_multiplayer_target(1);
    v->active_enemy=amp;
    if(v->on_damage)v->on_damage(v->user,v->selected_index,1);
    if(v->enemy_sound)v->enemy_sound(v->user,amp,(unsigned)cur_level,1);
    return 1;
}
static int visit_leave(void *ctx)
{
    AvpMpAmpTargetVisit *v=(AvpMpAmpTargetVisit *)ctx;
    int ok;
    if(v->enemy_sound && v->active_enemy)
        v->enemy_sound(v->user,v->active_enemy,(unsigned)cur_level,0);
    v->active_enemy=NULL;
    avp_amp_set_marine_multiplayer_target(0);
    if(v->on_damage)v->on_damage(v->user,v->selected_index,0);
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
    AvpMpActorMask contact_mask=0u;
    const unsigned index=(unsigned)(puddle-amp_data);
    if(index>=AVP_NUM_AMPS)return -1;
    for(unsigned p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(v->mask&(AVP_MP_ACTOR_BIT(p))){
        AvpMpActorNativeState *a=v->actors[p];
        AvpMpActorNativeScope scope={0};
        int hit,restored;
        if(!a || a->player.player_energy<=0 || a->player.game_over)continue;
        if(!avp_mp_actor_scope_enter(&scope,a))return -1;
        if(v->on_damage)v->on_damage(v->user,p,1);
        hit=avp_amp_acid_contact_player(puddle);
        if(v->on_damage)v->on_damage(v->user,p,0);
        restored=avp_mp_actor_scope_leave(&scope,a);
        if(!restored)return -1;
        contacts+=hit;
        if(hit)contact_mask|=AVP_MP_ACTOR_BIT(p);
    }
    if(v->acid_cue && v->acid_contact_masks){
        const AvpMpActorMask newcomers=contact_mask&~v->acid_contact_masks[index];
        v->acid_contact_masks[index]=contact_mask;
        for(unsigned p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(newcomers&(AVP_MP_ACTOR_BIT(p))){
            const AvpMpActorNativeState *a=v->actors[p];
            v->acid_cue(v->user,p,(unsigned)cur_level,
                        (int32_t)a->player.x_pos,(int32_t)a->player.y_pos);
        }
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

int avp_mp_amp_step_targets_with_health(
    AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS],
    AvpMpActorMask mask,AvpMpAmpChooseTarget choose,void *user,
    AvpMpActorMask acid_contact_masks[AVP_NUM_AMPS],AvpMpAmpPlayerAcidCue cue,
    AvpMpAmpEnemySoundScope enemy_sound,AvpMpAmpActorDamage on_damage)
{
    AvpMpAmpTargetVisit v={0};
    unsigned i;
    if(!actors || !choose || !mask || (mask&~(AVP_MP_ACTOR_VALID_MASK)) ||
       !avp_mp_native_world_scope_active() || avp_mp_actor_scope_active())return 0;
    /* Same PC-only emulated video-beam cadence as pc_main.c's UpdateAMPs
     * path; install it ONCE while the destination world's original AMP
     * globals are bound, never on a particular player or the host world.
     * Original single-player UpdateAMPs remains untouched. */
    avp_amp_set_beam_counter(avp_mp_amp_host_beam_value(frame_counter));
    /* An AMP slot reused for another creature/puddle starts with a clean
     * entry history. The live world's original AMP mode owns validity. */
    if(acid_contact_masks){
        for(i=0;i<AVP_NUM_AMPS;i++)
            if(!avp_amp_is_acid_puddle(&amp_data[i]))acid_contact_masks[i]=0u;
    }
    v.actors=actors;v.mask=mask;v.choose=choose;v.user=user;
    v.acid_contact_masks=acid_contact_masks;
    v.acid_cue=cue;
    v.enemy_sound=enemy_sound;
    v.on_damage=on_damage;
    for(i=0;i<AVP_LOCAL_MP_MAX_ACTORS;i++)if(mask&(AVP_MP_ACTOR_BIT(i))){
        if(!actors[i] || !avp_mp_actor_native_valid(actors[i]))return 0;
        v.candidates[i]=actors[i];
    }
    return avp_mp_amp_native_update_scoped(&v,visit_enter,visit_leave,
                                            visit_acid,cue!=NULL);
}
int avp_mp_amp_step_targets_spatial(
    AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS],
    AvpMpActorMask mask,AvpMpAmpChooseTarget choose,void *user,
    AvpMpActorMask acid_contact_masks[AVP_NUM_AMPS],AvpMpAmpPlayerAcidCue cue,
    AvpMpAmpEnemySoundScope enemy_sound)
{
    return avp_mp_amp_step_targets_with_health(actors,mask,choose,user,
        acid_contact_masks,cue,enemy_sound,NULL);
}
/* Preserve the previous private-acid API and legacy observer entrypoint
 * for callers not running the full positional F2 host. */
int avp_mp_amp_step_targets_local_acid(
    AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS],
    AvpMpActorMask mask,AvpMpAmpChooseTarget choose,void *user,
    AvpMpActorMask acid_contact_masks[AVP_NUM_AMPS],AvpMpAmpPlayerAcidCue cue)
{
    return avp_mp_amp_step_targets_spatial(actors,mask,choose,user,
                                           acid_contact_masks,cue,NULL);
}
/* Preserve the existing observer-only API for legacy/regression clients.
 * The real local-multiplayer host supplies persistent per-world masks and
 * a per-actor cue callback through the opt-in entry point above. */
int avp_mp_amp_step_targets(AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS],
                            AvpMpActorMask mask,AvpMpAmpChooseTarget choose,void *user)
{
    return avp_mp_amp_step_targets_local_acid(actors,mask,choose,user,NULL,NULL);
}
