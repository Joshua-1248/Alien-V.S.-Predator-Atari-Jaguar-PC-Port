/* RE26/S65: source-driven, one-window live native movement/display smoke.
 * No fabricated enemy, physics, map or player initialization. Clones exactly
 * the original actor state after the retail startup has initialized it.
 * Incomplete systems are intentionally NOT represented as playable co-op.
 */
#include "local_mp_walktest.h"
#include "local_mp_native_backend.h"
#include "local_mp_transition.h"
#include "local_mp_chamber.h"
#include "levels.h"
#include "pc/pc_platform.h"
#include "pc/pc_runtime.h"
#include "collision.h"
#include "joypad.h"
#include "maze.h"
#include "player.h"
#include "doors.h"
#include "computer.h"
#include "amp.h"
#include "hud.h"
#include "mazescrn.h"
#include "music.h"
#include "avp_runtime.h"
#include "jaguar/host/fullsynth_host.h"
#include "dev_cheats.h"
#include "../cheats/health_policy.h"
#include "../cheats/damage_gate.h"
#include "pc/pc_original_cheats.h"
#include "local_mp_respawn_policy.h"
#include "savegame.h"
#include "eeprom.h"
#include "local_mp_life.h"
#include "local_mp_deathmatch.h"
#include "local_mp_kill_notice.h"
#include "local_mp_dm_loot.h"
#include "local_mp_death_camera.h"
#include "local_mp_spycam.h"
#include "local_mp_corpses.h"
#include "local_mp_sprite_pose.h"
#include "avp_sfx_ids.h"
#include "avp_runtime.h"
#include "weapons.h"
#include "collectables.h"
#include "pc/pc_marine_start.h"
#include "pc/pc_quick_turn.h"
#include "pc/pc_input_routing.h"
#include "pc/pc_live_heading.h"
#include "jaguar/avp_gpu/presentation_interp.h"
#include "pc/pc_audio.h"
#include "local_mp_ambient_policy.h"
#include "local_mp_pause.h"
#include "local_mp_bot.h"
#include "local_mp_move_guard.h"
#include "local_mp_bot_levels.h"
#include "local_mp_bot_awareness.h"
#include "local_mp_bot_targeting.h"
#include "local_mp_cheat_targets.h"
#include "local_mp_tracker_peers.h"
#include "local_mp_enhancement_buffer.h"
#include <math.h>
#include "avp_resources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AvpMpDmBotTravelState {
    unsigned level,previous_level,target_level,valid_hop,avoid_valid;
    AvpMpBotLevelPlan hop,avoid;
    uint64_t entered_tick,last_contact_tick,retry_tick,plan_tick;
    uint64_t last_progress_tick,best_distance;
} AvpMpDmBotTravelState;

typedef struct AvpMpWalkHost {
    AvpPcVideo *video;
    AvpMpInputConfig profiles;
    AvpMpNativeBackendHost host;
    AvpMpSession session;
    AvpMpActorNativeState initial_actor;
    AvpMpActorNativeState species_start[3]; /* original per-species initializers */
    uint8_t species_start_ready[3];
    AvpMpNativeWorld initial_world;
    AvpMpNativeWorld persistent_world[AVP_MP_RETAIL_LEVELS];
    uint8_t persistent_ready[AVP_MP_RETAIL_LEVELS];
    /* Alien campaign's authored Level 15 end Queen must not start merely
     * because a dead Predator respawns at their own starting ship. */
    uint8_t deferred_alien_end_queen;
    const AvpRomImage *rom;
    AvpMpTransitionRequest entering[AVP_LOCAL_MP_MAX_ACTORS];
    int entering_reset[AVP_LOCAL_MP_MAX_ACTORS];
    int8_t lift_owner[AVP_MP_RETAIL_LEVELS];
    AvpMpRetailMaps retail_maps;
    AvpMpRetailMap diagnostic_map;
    AvpMpRetailMap *map;
    int retail_catalog_ready;
    unsigned level;
    unsigned species;
    unsigned players; /* human renderers P1..P4 */
    unsigned actors; /* human players + standalone bot actors */
    uint8_t random_species[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpBotState bots[AVP_LOCAL_MP_MAX_ACTORS];
    /* R300: one shared campaign emergency; no solo bot can trigger repeated
     * launch or choose a fresh leader every tick. */
    unsigned evacuation_active;
    unsigned evacuation_leader; /* AVP_LOCAL_MP_MAX_ACTORS when humans lead */
    uint64_t evacuation_start_tick;
    AvpMpHudWorldState shared_countdown; /* Same original countdown on ALL levels. */
    unsigned shared_countdown_valid;
    AvpMpBotPickupMemory pickup_memory[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpBotDoorClaims shared_door_claims;
    /* One stable destination and Use owner per original lift shaft. */
    AvpMpBotLiftClaims shared_lift_claims;
    unsigned last_bot_world_level[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned last_bot_goal_lift[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned lift_arrival_pending[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned lift_arrival_x[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned lift_arrival_y[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned lift_egress_valid[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned lift_egress_level[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned lift_egress_x[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned lift_egress_y[AVP_LOCAL_MP_MAX_ACTORS];
    struct {unsigned level,x,y,destination,side,follow_human;uint64_t tick;} lift_intent[AVP_LOCAL_MP_MAX_ACTORS];
    uint64_t lift_board_tick; /* Current original bot-input tick, for intent expiry. */
    AvpMpBotContact contacts[AVP_LOCAL_MP_MAX_ACTORS][AVP_NUM_AMPS+AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpBotTrail amp_trails[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    AvpMpBotTrail player_trails[AVP_LOCAL_MP_MAX_ACTORS];
    struct {
        uint16_t visits[AVP_MP_BOT_MAP_CELLS];
        uint32_t x,y;
        uint64_t best_goal_distance;
        unsigned best_route_cost,last_route_progress_cell;
        unsigned level,valid;
        uint64_t expires,last_progress_tick;
    } patrol[AVP_LOCAL_MP_MAX_ACTORS];
    /* Immutable, read-only retail RDB per-level panel identifiers. */
    AvpDoorPanelIds bot_panel_ids[AVP_MP_RETAIL_LEVELS];
    uint8_t bot_panel_list[AVP_MP_RETAIL_LEVELS][AVP_MAX_PANELS];
    uint8_t bot_panels_ready[AVP_MP_RETAIL_LEVELS];
    struct {
        AvpMpBotLevelPlan hop;
        uint64_t tick;
        unsigned from_level,goal_level,gx,gy,species,goal_known;
        unsigned valid;
    } bot_level_cache[AVP_LOCAL_MP_MAX_ACTORS];
    /* DM exploration is a route commitment, not a teleport or an enemy
     * position lookup. Each bot remembers its destination and a single
     * failed source-authored doorway to avoid repeated portal ping-pong. */
    AvpMpDmBotTravelState dm_bot_travel[AVP_LOCAL_MP_MAX_ACTORS];
    /* Security Keycards persist per actor, not as a single consumed AMP.
     * Reset only the dying player's record upon a successful respawn. */
    u16 collected_keycards[AVP_LOCAL_MP_MAX_ACTORS];
    /* Current-world item stays alive for all players; ownership is per life
     * and per physical AMP slot. An AMP reused for a different item must
     * invalidate stale claims without resetting other worlds. */
    AvpMpActorMask collected_items[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    uint64_t collected_identity[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    /* Deathmatch corpse attempts persist through respawns. New AMP slot
     * generations invalidate stale claims; other pickups still reset per life. */
    uint64_t dm_body_identity[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    AvpMpActorMask dm_body_claimed[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    unsigned collecting_player;
    uint8_t random_level_entry[AVP_LOCAL_MP_MAX_ACTORS];
    u32 spawn_x[AVP_LOCAL_MP_MAX_ACTORS];
    u32 spawn_y[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpSharedArrivalGrace shared_arrival; /* Existing P1-P4 arrival policy. */
    AvpMpPeerGrace peer_spawn_grace; /* All 20 actors. */
    AvpMpEnhancementBuffer host_input_buffer; /* Opt-in P2-P4 sub-tick pulses. */
    u32 last_pad;
    float p1_forward, p1_strafe, p1_turn;
    float p1_aim_sensitivity, p1_aim_speed;
    int p1_analog_enabled, p1_aim_enabled, p1_enhanced_aim, p1_mouse_aim;
    int p1_keyboard_strafe;
    unsigned pending_actions;
    unsigned terminal_pending_mask;
    u8 terminal_pending_panel[AVP_LOCAL_MP_MAX_ACTORS];
    u16 terminal_pending_offset[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned pending_cheat_actions[AVP_LOCAL_MP_MAX_ACTORS];
    uint8_t orig_god_last[AVP_LOCAL_MP_MAX_PLAYERS];
    int p1_weapon_delta;
    int weapon_cycle[AVP_LOCAL_MP_MAX_ACTORS];
    uint8_t tracker_cheat_borrowed[AVP_LOCAL_MP_MAX_ACTORS];
    uint8_t sprite_fire_down[AVP_LOCAL_MP_MAX_ACTORS];
    uint8_t sprite_fire_active[AVP_LOCAL_MP_MAX_ACTORS];
    uint8_t sprite_fire_weapon[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpMarineCrouchAnim sprite_marine_crouch[AVP_LOCAL_MP_MAX_ACTORS];
    uint64_t sprite_attack_tick[AVP_LOCAL_MP_MAX_ACTORS];
    uint64_t sprite_alien_repeat_tick[AVP_LOCAL_MP_MAX_ACTORS]; /* AMP handalien 5-tick recovery */
    uint8_t sprite_roar_active[AVP_LOCAL_MP_MAX_ACTORS];
    uint64_t sprite_roar_tick[AVP_LOCAL_MP_MAX_ACTORS];
    /* Visual-only per-actor AMP pacing. Never touches native weapon, pain,
     * Predator cloak, or movement simulation state. */
    uint8_t sprite_motion_active[AVP_LOCAL_MP_MAX_ACTORS];
    uint64_t sprite_motion_tick[AVP_LOCAL_MP_MAX_ACTORS];
    uint8_t sprite_cloak_valid[AVP_LOCAL_MP_MAX_ACTORS];
    uint8_t sprite_cloak_target[AVP_LOCAL_MP_MAX_ACTORS];
    uint8_t sprite_cloak_transition[AVP_LOCAL_MP_MAX_ACTORS];
    uint64_t sprite_cloak_tick[AVP_LOCAL_MP_MAX_ACTORS];
    uint8_t sprite_pain_valid[AVP_LOCAL_MP_MAX_ACTORS];
    int16_t sprite_pain_last_energy[AVP_LOCAL_MP_MAX_ACTORS];
    uint64_t sprite_pain_tick[AVP_LOCAL_MP_MAX_ACTORS];
    uint32_t map_zoom_held[AVP_LOCAL_MP_MAX_ACTORS];
    int map_zoom_steps[AVP_LOCAL_MP_MAX_ACTORS];
    AvpPcQuickTurn quick_turn[AVP_LOCAL_MP_MAX_ACTORS];
    uint32_t quick_held[AVP_LOCAL_MP_MAX_ACTORS];
    uint32_t prior_weapon_buttons[AVP_LOCAL_MP_MAX_ACTORS];
    u32 prev_x[AVP_LOCAL_MP_MAX_ACTORS];
    u32 prev_y[AVP_LOCAL_MP_MAX_ACTORS];
    u32 prev_angle[AVP_LOCAL_MP_MAX_ACTORS];
    /* Independent host-clock smooth look per player, authoritative physics
     * remains inside original 10-Hz PLAYER.S MovePlayer. */
    AvpPcLiveHeading live_heading[AVP_LOCAL_MP_MAX_ACTORS];
    u32 live_last_game_angle[AVP_LOCAL_MP_MAX_ACTORS];
    double presentation_alpha;
    AvpMpLife player_life[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpLife vent_fade[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpLifeFrame vent_frame[AVP_LOCAL_MP_MAX_PLAYERS];
    AvpMpDeathmatch dm;
    unsigned dm_shooter;
    unsigned dm_amp_target; /* actor currently scoped for native AMP AI */
    /* A native AMP slot can be reused: validate its true world-specific
     * generation before crediting projectile hits to a player. */
    uint8_t dm_projectile_owner[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    /* Snapshot the attacker cloak state at projectile creation, not impact.
     * Inherited by explosions and guarded by the existing slot generation. */
    uint8_t dm_projectile_predator_uncloaked[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    uint32_t dm_projectile_gen[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    uint8_t dm_death_credited[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpKillNotice dm_notices[AVP_LOCAL_MP_MAX_PLAYERS];
    AvpMpDeathCamera death_camera[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpCorpses corpses;
    unsigned long completed_respawns;
    int map_leased;
    int combat_test;
    unsigned long weapon_updates;
    unsigned long amp_target_requests;
    AvpMpWeaponAudioState weapon_voice[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned current_ambient_level; /* shared original DSP voice zero, not per actor */
    AvpMpTransitionQueue transitions;
    unsigned exit_kind[AVP_LOCAL_MP_MAX_ACTORS];
    int8_t amp_locked_player[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    u32 amp_locked_generation[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
} AvpMpWalkHost;

/* Make each participant's original Jaguar character from the original
 * PLAYER.S and MAZESCRN.S initializers, NOT from a campaign-species clone.
 * The original initializer touches door/AMP-hosted globals, so prepare it
 * against a private copy of the already initialized campaign world. Restore
 * the host actor/world when finished: the campaign map/AI remains unchanged.
 * Only requested species are prepared, and the campaign actor is used as-is.
 */
static int walk_prepare_species_start(AvpMpWalkHost *t,unsigned species)
{
    AvpMpNativeWorld temporary;
    AvpMpActorNativeState actor;
    AvpMpNativeScope scope={0};
    int actor_active=0,world_active=0,ok=0;
    if(!t || species>AVP_MP_PREDATOR)return 0;
    if(t->species_start_ready[species])return 1;
    if(species==t->species){
        t->species_start[species]=t->initial_actor;
        t->species_start_ready[species]=1;
        return 1;
    }
    temporary=t->initial_world;
    actor=t->initial_actor;
    if(!avp_mp_native_world_enter(&scope,&temporary))goto done;
    world_active=1;
    if(!avp_mp_native_actor_enter(&scope,&actor))goto done;
    actor_active=1;
    /* Original InitPlayer -> InitScrOverlays order. There is no savegame
     * for a new F2 co-op session. This uses the source health, movement,
     * access, HUD, cocoon, Predator equipment and weapon scripts. */
    if(savegame)goto done;
    player_type=(s16)(species<<2);
    InitPlayer();
    InitScrOverlays();
    ok=1;
done:
    if(actor_active && !avp_mp_native_actor_leave(&scope,&actor))ok=0;
    if(world_active && !avp_mp_native_world_leave(&scope,&temporary))ok=0;
    if(!ok || !avp_mp_actor_native_valid(&actor) ||
       actor.player.player_type!=(s16)(species<<2))return 0;
    t->species_start[species]=actor;
    t->species_start_ready[species]=1;
    return 1;
}

/* SafePos checks nearby walls, but is not itself a guarantee that a
 * position lies INSIDE the destination map. Validate source map bounds and
 * authored usable floor before accepting a co-op spawn/join. */
static int walk_spawn_on_retail_map(const AvpMpRetailMap *map,u32 x,u32 y)
{
    if(!map || !map->bytes || map->size<8u || !map->width || !map->height ||
       map->size<8u+(size_t)map->width*map->height*8u)return 0;
    /* A valid center alone does not guarantee the actual 40-pixel halfwidth
     * fits on authored floor. This guards DM spawns, campaign initial poses,
     * co-op arrivals and same-level respawns without moving anyone across a
     * wall or choosing new, unverified coordinates. */
    return avp_mp_move_guard_floor(map->bytes+8u,map->width,map->height,
                                   (s32)x,(s32)y,40u<<9);
}
/* Deathmatch uses the original rand_set() cell sampling (avp_random()&63),
 * but the retail AMP randomizer can populate sealed/off-map rooms. Players
 * need the stronger guarantee that their spawn belongs to a reachable
 * component of the CURRENT authored level for their species/clearance.
 * SafePos and all door rules remain the original engine; no teleport through
 * locked/sealed geometry and no change to co-op start/respawn policy. */
static int walk_dm_random_spawn(AvpMpWalkHost *t,const AvpMpNativeWorld *world,
    unsigned p,const AvpMpActorNativeState *actor,u32 *out_x,u32 *out_y)
{
    AvpMpBotMap map;
    uint16_t components[AVP_MP_BOT_MAP_CELLS];
    u32 origin_x,origin_y,origin_angle;
    unsigned ax,ay;
    u32 fallback_x=0u,fallback_y=0u;
    int fallback=0;
    if(!t||!world||!world->map||!actor||!out_x||!out_y)return 0;
    map=(AvpMpBotMap){world->map->bytes+8u,world->map->width,
        world->map->height,world->doors.wall_flags,world->doors.current_meta,
        actor->player.player_type==PT_HUMAN?99u:actor->access_level,NULL,NULL,0u};
    if(map.width>64u||map.height>64u||!map.width||!map.height||
       !avp_mp_bot_components(&map,components))return 0;
    /* A dead player may be in a detached/invalid floor island. Anchoring
     * the next random spawn to that last position perpetuates the error.
     * Always use the original authored start pose for THIS level, never
     * the player's last position (or a previous level's start pose). */
    if(t->session.setup.custom_session_id==AVP_MP_CUSTOM_LEVEL){
        /* Custom session 16 borrows a retail level ONLY for its artwork.
         * Its authored playable geometry is independent of the retail
         * start pose, which can be inside the VOID (DM1) or a wall. Use the
         * already validated custom-map entry from the native host actor.
         * This keeps the connectivity/island and SafePos rules below intact
         * without reusing a retail campaign coordinate in a custom level. */
        origin_x=t->initial_actor.player.x_pos;
        origin_y=t->initial_actor.player.y_pos;
        origin_angle=t->initial_actor.player.centre_angle;
    }else if(!avp_levels_start_pose(world->map->level,
                                    &origin_x,&origin_y,&origin_angle))
        return 0;
    if(!walk_spawn_on_retail_map(world->map,origin_x,origin_y) ||
       SafePos(80,(s32)origin_x,(s32)origin_y,40)!=AVP_COLL_SAFE)
        return 0; /* Invalid source/custom spawn; never put actor in void. */
    ax=origin_x>>16;ay=origin_y>>16;
    if(ax>=map.width||ay>=map.height ||
       !components[ay*64u+ax])return 0;
    /* Original random AMP spawn RNG / 64x64 map-cell sampling, except
     * players must be on actual authored floor and not embedded in props.
     * Avoid spawn overlap when possible, but never fail solely from density. */
    for(unsigned try_no=0;try_no<4096u;try_no++){
        unsigned x=(unsigned)(avp_random()&63u);
        unsigned y=(unsigned)(avp_random()&63u);
        u32 sx=(x<<16)|0x8000u,sy=(y<<16)|0x8000u;
        int distant=1;
        if(!avp_mp_dm_spawn_origin_region(components,map.width,map.height,
                ax,ay,x,y)||
           !walk_spawn_on_retail_map(world->map,sx,sy)||
           !avp_mp_dm_spawn_interior(map.cells,map.width,map.height,
               map.wall_flags,x,y)||
           SafePos(80,(s32)sx,(s32)sy,40)!=AVP_COLL_SAFE ||
           AMPCollisions((s32)sx,(s32)sy))continue;
        for(unsigned other=0;other<t->actors;other++)if(other!=p){
            const AvpMpActorNativeState *r=(const AvpMpActorNativeState *)t->session.player[other];
            const AvpMpActiveWorld *same=avp_mp_session_player_world(&t->session,other);
            if(r && same && same->level==world->map->level &&
               (uint64_t)llabs((int64_t)sx-(int64_t)r->player.x_pos)<(2u<<16) &&
               (uint64_t)llabs((int64_t)sy-(int64_t)r->player.y_pos)<(2u<<16))
                distant=0;
        }
        if(distant){*out_x=sx;*out_y=sy;return 1;}
        if(!fallback){fallback=1;fallback_x=sx;fallback_y=sy;}
    }
    if(fallback){*out_x=fallback_x;*out_y=fallback_y;return 1;}
    /* In extremely dense/irregular maps, locate a valid reachable floor
     * deterministically before failing rather than putting someone in void. */
    for(unsigned y=0;y<map.height;y++)for(unsigned x=0;x<map.width;x++){
        u32 sx=(x<<16)|0x8000u,sy=(y<<16)|0x8000u;
        if(avp_mp_dm_spawn_origin_region(components,map.width,map.height,
               ax,ay,x,y) &&
           walk_spawn_on_retail_map(world->map,sx,sy) &&
           avp_mp_dm_spawn_interior(map.cells,map.width,map.height,
               map.wall_flags,x,y) &&
           SafePos(80,(s32)sx,(s32)sy,40)==AVP_COLL_SAFE &&
           !AMPCollisions((s32)sx,(s32)sy)){
            *out_x=sx;*out_y=sy;return 1;
        }
    }
    return 0;
}
static int walk_create_actor(void *user,unsigned p,unsigned species,
                             AvpMpActorNativeState *out)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)user;
    if(!out||p>=t->actors||species>AVP_MP_RANDOM)return 0;
    if(species==AVP_MP_RANDOM){
        t->random_species[p]=1u;
        species=(unsigned)(avp_random()%3u);
    }
    if(!t->species_start_ready[species])return 0;
    t->session.setup.species[p]=species;
    *out=t->species_start[species];
    /* The original Marine-start grant is applied to each native actor,
     * including P1, AFTER restoring the saved campaign bootstrap snapshot.
     * Keep the duplicate player/weapon availability fields aligned. */
    if(species==AVP_MP_MARINE && avp_pc_video_marine_start_shotgun(t->video)){
        if(!avp_pc_marine_start_shotgun_apply(1,0,PT_HUMAN,
                &out->player.cur_weps,&out->player.new_wepno,
                out->weapon.ammo_info))return 0;
        out->weapon.cur_weps=out->player.cur_weps;
        out->weapon.new_wepno=out->player.new_wepno;
    }
    if(t->dm.enabled && species==AVP_MP_PREDATOR)
        avp_mp_dm_predator_weapons(&out->player.cur_weps,&out->weapon.cur_weps);
    if(p==0u && t->map &&
       (!walk_spawn_on_retail_map(t->map,out->player.x_pos,out->player.y_pos) ||
        SafePos(80,(s32)out->player.x_pos,(s32)out->player.y_pos,40)!=AVP_COLL_SAFE)){
        uint64_t best=UINT64_MAX;
        u32 chosen_x=0u,chosen_y=0u;
        int found=0;
        const int64_t initial_x=(int64_t)out->player.x_pos;
        const int64_t initial_y=(int64_t)out->player.y_pos;
        for(unsigned y=1;y+1u<maze_height;y++)
        for(unsigned x=1;x+1u<maze_width;x++){
            u32 sx=((u32)x<<16)|0x8000u,sy=((u32)y<<16)|0x8000u;
            int64_t dx=(int64_t)sx-initial_x,dy=(int64_t)sy-initial_y;
            uint64_t distance=(uint64_t)(dx*dx+dy*dy);
            if(distance>=best || !walk_spawn_on_retail_map(t->map,sx,sy) ||
               SafePos(80,(s32)sx,(s32)sy,40)!=AVP_COLL_SAFE)continue;
            best=distance;chosen_x=sx;chosen_y=sy;found=1;
        }
        if(!found)return 0; /* Never spawn in the void. */
        out->player.x_pos=chosen_x;out->player.y_pos=chosen_y;
        out->player.x_vel=0;out->player.y_vel=0;
    }
    if(p>0u){
        /* All players enter the SAME verified campaign starting position.
         * The original single-player engine has no actor-vs-actor player
         * collision, so stacking does not trap anybody. Forcing P2-P4 onto
         * nearby map cells could put P3/P4 behind a wall or outside the
         * authored playable area. The initial P1 pose was validated above. */
        out->player.x_pos=t->spawn_x[0];
        out->player.y_pos=t->spawn_y[0];
        out->player.x_vel=out->player.y_vel=0;
        if(t->map && (!walk_spawn_on_retail_map(t->map,out->player.x_pos,out->player.y_pos) ||
            SafePos(80,(s32)out->player.x_pos,(s32)out->player.y_pos,40)!=AVP_COLL_SAFE))
            return 0;
    }
    if(t->dm.enabled){
        u32 sx,sy;
        if(!walk_dm_random_spawn(t,&t->initial_world,p,out,&sx,&sy))return 0;
        out->player.x_pos=sx;out->player.y_pos=sy;
        out->player.x_vel=out->player.y_vel=0;
    }
    if(t->dm.enabled && species==AVP_MP_MARINE)out->access_level=99u;
    if(!avp_mp_actor_native_valid(out))return 0;
    t->spawn_x[p]=out->player.x_pos;
    t->spawn_y[p]=out->player.y_pos;
    return 1;
}
/* Prepare an unvisited destination by running the original retail AMP,
 * door, panel and collision initialization while the original host globals
 * are temporarily scoped. The currently populated level is NEVER reset.
 * All committed worlds are retained, including levels with zero players. */
static int walk_create_level(void *user,unsigned campaign,unsigned level,
                             AvpMpNativeWorld *out)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)user;
    AvpMpRetailMap *m;
    AvpMpNativeWorld temporary;
    AvpMpActorNativeState staged_actor;
    AvpMpNativeScope scope={0};
    AvpDoorPanelIds ids;
    u32 saved_panel_pos=panel_pos;
    s16 saved_new_level=new_level;
    s32 saved_new_x=new_x,saved_new_y=new_y;
    int started=0,actor_started=0,ok=0;
    int deferred_queen=0;
    if(!out || campaign!=t->species || level<1u ||
       level>AVP_MP_RETAIL_LEVELS)return 0;
    if(t->persistent_ready[level-1u]){
        *out=t->persistent_world[level-1u];
        return avp_mp_native_world_valid(out);
    }
    if(level==t->level){
        *out=t->initial_world;
        return avp_mp_native_world_valid(out);
    }
    {u32 start_x,start_y,start_angle;
     if(!t->retail_catalog_ready || !t->rom || !t->rom->exact_retail_sha256 ||
        !avp_levels_start_pose(level,&start_x,&start_y,&start_angle))return 0;}
    m=avp_mp_retail_maps_acquire(&t->retail_maps,campaign,level);
    if(!m)return 0;
    temporary=t->initial_world;
    if(!avp_mp_native_world_enter(&scope,&temporary))goto done;
    started=1;
    staged_actor=t->initial_actor;
    if(!avp_mp_native_actor_enter(&scope,&staged_actor))goto done;
    actor_started=1;
    avp_collision_bind_maze(m->bytes+8u,(u16)m->width,(u16)m->height);
    avp_doors_bind_maze(m->bytes+8u,(u16)m->width,(u16)m->height);
    maze_width=(u16)m->width;maze_height=(u16)m->height;
    cur_level=(s16)level;
    if(!avp_resources_bind_retail_panel_info(t->rom,level,&ids))goto done;
    ResetAccess();
    ResetDoors();
    /* Preserve the original authored AMP/door initialization. Only suppress
     * an Alien campaign end Queen when this FIRST level-15 visit is caused
     * solely by a species-specific respawn, not by a real passage/airlock. */
    if(campaign==AVP_MP_ALIEN && level==15u &&
       !(levels_visit&(1u<<(level-1u)))){
        for(unsigned p=0;p<t->actors;p++)
            if(t->entering[p].valid && t->entering_reset[p] &&
               t->entering[p].target_level==(s16)level){
                deferred_queen=1;break;
            }
    }
    avp_amp_mp_defer_end_queen(deferred_queen);
    restore_level();
    avp_amp_mp_defer_end_queen(0);
    if(!avp_mp_native_actor_leave(&scope,&staged_actor)){actor_started=0;goto done;}
    actor_started=0;
    if(!avp_mp_native_world_leave(&scope,&temporary)){started=0;goto done;}
    started=0;
    temporary.map=m;
    temporary.doors.maze=m->bytes+8u;
    if(!avp_mp_native_world_valid(&temporary))goto done;
    if(t->dm.enabled && !t->dm.enemies)
        (void)avp_mp_dm_remove_enemies(&temporary.amp);
    t->persistent_world[level-1u]=temporary;
    t->persistent_ready[level-1u]=1;
    if(deferred_queen){
        t->deferred_alien_end_queen=1;
        fprintf(stderr,"MP Alien Level 15 end Queen deferred: species respawn is not campaign travel.\n");
    }
    *out=temporary;
    ok=1;
done:
    if(actor_started)(void)avp_mp_native_actor_leave(&scope,&staged_actor);
    if(started)(void)avp_mp_native_world_leave(&scope,&temporary);
    if(!ok && m && t->retail_maps.original[level-1u] &&
       m->size==t->retail_maps.original_size[level-1u])
        memcpy(m->bytes,t->retail_maps.original[level-1u],m->size);
    panel_pos=saved_panel_pos;
    new_level=saved_new_level;new_x=saved_new_x;new_y=saved_new_y;
    if(!ok)(void)avp_mp_retail_maps_release(&t->retail_maps,campaign,level,m);
    return ok;
}
static void walk_destroy_level(void *u,AvpMpNativeWorld *w)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    if(t&&w&&w->map&&w->map->level>=1 &&
       w->map->level<=AVP_MP_RETAIL_LEVELS){
        t->persistent_world[w->map->level-1u]=*w;
        t->persistent_ready[w->map->level-1u]=1;
    }
}
/* Once an actual (non-death) level transition reaches the deferred ending
 * room, install the ORIGINAL AMP.S encounter on the world snapshot. Do this
 * only after the movement transaction commits: failed batch arrivals must
 * never mutate an AMP world or consume an objective. */
static int walk_arm_alien_end_queen(AvpMpWalkHost *t,unsigned level)
{
    AvpMpNativeWorld *world=NULL;
    AvpMpNativeScope scope={0};
    AvpMpActorNativeState campaign_actor;
    int actor_entered=0,ok=1;
    if(!t || !t->deferred_alien_end_queen || level!=15u ||
       t->species!=AVP_MP_ALIEN)return 1;
    for(unsigned i=0;i<AVP_LOCAL_MP_MAX_ACTORS;i++)
        if(t->session.worlds[i].context && t->session.worlds[i].level==level){
            world=(AvpMpNativeWorld *)t->session.worlds[i].context;break;
        }
    if(!world || !avp_mp_native_world_enter(&scope,world))return 0;
    campaign_actor=t->initial_actor;
    if(avp_mp_native_actor_enter(&scope,&campaign_actor)){
        actor_entered=1;
        make_end_queen(); /* unchanged retail Queen constructor */
    }else ok=0;
    if(actor_entered && !avp_mp_native_actor_leave(&scope,&campaign_actor))ok=0;
    if(!avp_mp_native_world_leave(&scope,world))ok=0;
    if(!ok)return 0;
    t->deferred_alien_end_queen=0;
    if(t->persistent_ready[14])t->persistent_world[14]=*world;
    fprintf(stderr,"MP Alien Level 15 original end Queen armed by real level transition.\n");
    return 1;
}

static int walk_random_level_spawn(AvpMpWalkHost *t,unsigned p);
static int walk_join(void *u,AvpMpNativeWorld *w,unsigned p,AvpMpActorNativeState *a)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    AvpMpNativeScope scope={0};
    AvpMpActorNativeState staged;
    u32 x,y,angle;
    int found=0;
    if(!t||!w||!a||p>=t->actors||!avp_mp_actor_native_valid(a))return 0;
    if(!t->entering[p].valid)return 1; /* Initial campaign join. */
    if(t->entering[p].target_level!=(int16_t)w->map->level)return 0;
    /* Original Predator cannot enter narrow retail ducts. Prevent every
     * fresh-actor join from restarting a Predator inside a vent. Regular
     * scripted transitions are unchanged and use their own door rules. */
    if(t->entering_reset[p] && t->session.setup.custom_session_id!=AVP_MP_CUSTOM_LEVEL &&
       !avp_mp_spawn_level_allowed(t->session.setup.species[p],w->map->level))
        return 0;
    if(!avp_levels_start_pose(w->map->level,&x,&y,&angle))return 0;
    if(t->entering[p].new_x>=0){
        x=(u32)t->entering[p].new_x;y=(u32)t->entering[p].new_y;
        angle=a->player.centre_angle;
    }
    if(!walk_spawn_on_retail_map(w->map,x,y)){
        /* An authored transfer position outside the level cannot be used.
           Revert to the destination's original entry and search safely. */
        if(!avp_levels_start_pose(w->map->level,&x,&y,&angle))return 0;
    }
    /* Validate against the original destination collision and floor map.
     * Do NOT reject a valid entry merely because another co-op player is
     * standing there. Vents, airlocks and cramped lifts have a single authored
     * arrival point, and their players are not solid against one another. */
    if(!avp_mp_native_world_enter(&scope,w))return 0;
    if(walk_spawn_on_retail_map(w->map,x,y) &&
       SafePos(80,(s32)x,(s32)y,40)==AVP_COLL_SAFE)found=1;
    if(!found){
        uint64_t best=UINT64_MAX;
        u32 selected_x=0u,selected_y=0u;
        /* If a transfer entry is blocked/outside, choose the nearest
         * original authored interior floor rather than moving a passenger
         * out of bounds. Existing passengers are not obstacles. */
        for(unsigned yy=1;yy+1u<maze_height;yy++)
        for(unsigned xx=1;xx+1u<maze_width;xx++){
            u32 sx=(xx<<16)|0x8000u,sy=(yy<<16)|0x8000u;
            int64_t dx=(int64_t)sx-(int64_t)x,dy=(int64_t)sy-(int64_t)y;
            uint64_t distance=(uint64_t)(dx*dx+dy*dy);
            if(distance>=best || !walk_spawn_on_retail_map(w->map,sx,sy) ||
                SafePos(80,(s32)sx,(s32)sy,40)!=AVP_COLL_SAFE)continue;
            selected_x=sx;selected_y=sy;best=distance;
        }
        if(best!=UINT64_MAX){x=selected_x;y=selected_y;found=1;}
    }
    if(found && t->entering_reset[p] &&
       (t->dm.enabled || t->random_level_entry[p])){
        u32 sx,sy;
        if(!walk_dm_random_spawn(t,w,p,a,&sx,&sy))
            found=0;
        else {x=sx;y=sy;}
    }
    if(!avp_mp_native_world_leave(&scope,w))return 0;
    if(!found)return 0;
    staged=*a;
    if(t->entering_reset[p]){
        if(!avp_mp_respawn_restart(&staged,
                    &t->species_start[t->session.setup.species[p]],x,y,
                    avp_pc_video_marine_start_shotgun(t->video)))return 0;
        if(t->dm.enabled && staged.player.player_type==PT_HUMAN)
            staged.access_level=99u;
        if(t->dm.enabled && staged.player.player_type==PT_PREDATOR)
            avp_mp_dm_predator_weapons(&staged.player.cur_weps,&staged.weapon.cur_weps);
    }
    staged.player.x_pos=x;staged.player.y_pos=y;
    staged.player.centre_angle=angle;
    staged.player.x_vel=staged.player.y_vel=0;
    staged.player.new_pos=0;
    if(!avp_mp_actor_native_valid(&staged))return 0;
    *a=staged;
    return 1;
}
/* Batch joins preserve genuine within-chamber offsets where each resulting
 * position is source-valid. If a translated offset would put a passenger
 * through a wall or outside the map, that passenger shares a VALID group
 * arrival point instead. No group passenger is rejected for overlapping an
 * existing player or another member of this batch. */
/* Execute LEVELS.S's exact entry-cell hostile cleanup for committed
 * co-op arrivals. Unlike a single-player LoadLevel, the destination may be
 * an already-live world; do not clear other map cells or touch protected
 * actors, corpse templates, grids, or the original AMP allocation list.
 * This must follow the successful group transaction (never a staged join).
 */
static int walk_clear_arrival_enemies(AvpMpWalkHost *t,AvpMpActorMask mask)
{
    if(!t)return 0;
    for(unsigned p=0;p<t->actors;p++)if(mask&AVP_MP_ACTOR_BIT(p)){
        const AvpMpActiveWorld *active=avp_mp_session_player_world(&t->session,p);
        const AvpMpActorNativeState *actor=(const AvpMpActorNativeState *)t->session.player[p];
        AvpMpNativeWorld *world=active?(AvpMpNativeWorld *)active->context:NULL;
        AvpMpNativeScope scope={0};
        if(!world||!actor||!avp_mp_native_world_enter(&scope,world))return 0;
        avp_levels_kill_bastards_at(actor->player.x_pos,actor->player.y_pos);
        if(!avp_mp_native_world_leave(&scope,world))return 0;
    }
    return 1;
}
static int walk_join_group(void *u,AvpMpNativeWorld *w,AvpMpActorMask mask,
                           AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS])
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    u32 anchor_x=0u,anchor_y=0u;
    int anchor_found=0;
    AvpMpActorMask fallback_mask=0u;
    for(unsigned p=0;p<t->actors;p++)if(mask&(AVP_MP_ACTOR_BIT(p))){
        u32 requested_x=(u32)t->entering[p].new_x;
        u32 requested_y=(u32)t->entering[p].new_y;
        int requested=t->entering[p].valid && t->entering[p].new_x>=0 &&
                      t->entering[p].new_y>=0;
        if(!actors[p]||!walk_join(u,w,p,actors[p]))return 0;
        /* A normal valid translated pose is kept exactly. A position that
         * walk_join had to repair should instead stack with the group. */
        if(requested && (actors[p]->player.x_pos!=requested_x ||
                         actors[p]->player.y_pos!=requested_y))
            fallback_mask|=AVP_MP_ACTOR_BIT(p);
        else if(!anchor_found){
            anchor_x=actors[p]->player.x_pos;
            anchor_y=actors[p]->player.y_pos;
            anchor_found=1;
        }
    }
    if(!anchor_found){
        /* All requested positions needed repair: share the first repaired
         * in-bounds floor cell, never scatter the passengers. */
        for(unsigned p=0;p<t->actors;p++)if(mask&(AVP_MP_ACTOR_BIT(p))){
            anchor_x=actors[p]->player.x_pos;anchor_y=actors[p]->player.y_pos;
            anchor_found=1;break;
        }
    }
    if(!anchor_found)return 0;
    for(unsigned p=0;p<t->actors;p++)if(fallback_mask&(AVP_MP_ACTOR_BIT(p))){
        actors[p]->player.x_pos=anchor_x;
        actors[p]->player.y_pos=anchor_y;
        actors[p]->player.x_vel=actors[p]->player.y_vel=0;
        actors[p]->player.new_pos=0;
        if(!avp_mp_actor_native_valid(actors[p]))return 0;
    }
    return 1;
}
/* The co-op host tracks pairs recently placed on the same valid floor
 * point. Original P1-P4 shared-arrival accounting is retained alongside
 * the 16-actor PC-only peer-collision grace for overlapping arrivals. */
static void walk_shared_arrival_refresh(AvpMpWalkHost *t,AvpMpActorMask arriving_mask)
{
    uint8_t present[4]={0};
    unsigned world_level[4]={0};
    uint32_t x[4]={0},y[4]={0};
    if(!t)return;
    /* Shared-arrival collision grace is defined for P1-P4 only. Headless
     * standalone bot actors must not index these four-viewpoint arrays. */
    for(unsigned p=0;p<t->players && p<AVP_LOCAL_MP_MAX_PLAYERS;p++){
        const AvpMpActiveWorld *world=avp_mp_session_player_world(&t->session,p);
        const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
        if(world&&a){
            present[p]=1;world_level[p]=world->level;
            x[p]=a->player.x_pos;y[p]=a->player.y_pos;
        }
    }
    avp_mp_shared_arrival_update(&t->shared_arrival,present,world_level,x,y,arriving_mask);
    /* All 20 actors, including headless bots, share the same original
     * same-world / PLAYER_WIDTH rule. The reusable helper also handles
     * grace expiry when either actor moves away or changes worlds. */
    {
        uintptr_t worlds[AVP_MP_PEER_COUNT]={0};
        uint32_t xs[AVP_MP_PEER_COUNT]={0},ys[AVP_MP_PEER_COUNT]={0};
        AvpMpActorMask mask=0u;
        for(unsigned p=0;p<t->actors;p++){
            const AvpMpActiveWorld *world=avp_mp_session_player_world(&t->session,p);
            const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
            if(world&&a){
                worlds[p]=(uintptr_t)(const void *)world;
                xs[p]=a->player.x_pos;ys[p]=a->player.y_pos;
                mask|=AVP_MP_ACTOR_BIT(p);
            }
        }
        avp_mp_peer_grace_update(&t->peer_spawn_grace,mask,worlds,xs,ys,arriving_mask);
    }
}
/* R467: a bot's accepted pressure request is not a license to transfer
 * after a later actor reopens a door during the same shared-world tick.
 * Re-read the captured native slots and actual map immediately at commit.
 * Resolve the authored destination and chamber; unknown state fails closed. */
static int walk_bot_airlock_commit_ready(const AvpMpNativeWorld *world,
    const AvpMpActorNativeState *a,const AvpMpTransitionRequest *request)
{
    uint32_t active[2];unsigned count=0;
    const AvpDoorLevelMeta *meta;
    const AvpMpRetailMap *map=world?world->map:NULL;
    if(!map||!map->bytes||!a||!request||!world->doors.valid||
       world->doors.maze!=map->bytes+8u||world->doors.width!=map->width||
       world->doors.height!=map->height||world->doors.level!=(int)map->level||
       !world->doors.level_meta||!map->level||
       map->level>world->doors.level_meta_count||
       map->width!=64u||map->height!=64u||map->size<8u+64u*64u*8u)
        return 0;
    meta=&world->doors.level_meta[map->level-1u];
    for(unsigned d=0;d<2u;d++)if(world->doors.door[d].active){
        if(world->doors.door[d].offset>=64u*64u*8u)return 0;
        active[count++]=(uint32_t)world->doors.door[d].offset;
    }
    for(size_t i=0;i<meta->exit_count;i++){
        const AvpDoorExitEntry *e=&meta->exits[i];
        AvpMpAirlockRoom room;
        if(e->level!=(unsigned)request->target_level||
           e->x!=((uint32_t)request->new_x>>16)||
           e->y!=((uint32_t)request->new_y>>16)||
           !avp_mp_airlock_room(map,meta,e->panel_offset,&room)||
           !avp_mp_chamber_contains(map,(room.exit_x<<16)|0x8000u,
               (room.exit_y<<16)|0x8000u,a->player.x_pos,a->player.y_pos))continue;
        return avp_mp_airlock_action(&room,map->bytes+8u,64u*64u*8u,
            active,count)==AVP_MP_AIRLOCK_PRESSURE;
    }
    return 0;
}
/* Derive passengers only from the source map's authored closed chamber,
 * never distance or a visible open door. Keep the initiator's destination
 * as the destination-room anchor and preserve every 16.16 relative offset. */
static unsigned walk_room_passengers(AvpMpWalkHost *t,unsigned leader,
                                     const AvpMpTransitionRequest *request)
{
    const AvpMpActiveWorld *origin=avp_mp_session_player_world(&t->session,leader);
    const AvpMpNativeWorld *world=origin?(const AvpMpNativeWorld *)origin->context:NULL;
    const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[leader];
    AvpMpActorMask mask=AVP_MP_ACTOR_BIT(leader);
    AvpMpTransitionRequest staged[AVP_LOCAL_MP_MAX_ACTORS];
    memset(staged,0,sizeof(staged));
    /* A same-level authored warp uses walk_join(), not the multi-world
     * transaction. It must not silently consume a bystander's pending exit
     * without repositioning that passenger. Only cross-level transfers
     * enter the shared-chamber grouping path. */
    if(!world||!world->map||!a||request->use_cocoon||
       (unsigned)request->target_level==origin->level||
       request->new_x<0||request->new_y<0||
       (t->exit_kind[leader]!=AVP_DOORS_EXIT_LIFT &&
        t->exit_kind[leader]!=AVP_DOORS_EXIT_AIRLOCK))return mask;
    for(unsigned p=0;p<t->actors;p++)if(p!=leader){
        const AvpMpActiveWorld *other=avp_mp_session_player_world(&t->session,p);
        const AvpMpActorNativeState *b=(const AvpMpActorNativeState *)t->session.player[p];
        AvpMpTransitionRequest existing;
        int32_t tx,ty;
        if(other!=origin||!b||b->player.player_energy<=0||b->player.game_over||
           avp_mp_life_busy(&t->player_life[p])||
           !avp_mp_chamber_contains(world->map,a->player.x_pos,a->player.y_pos,
                                   b->player.x_pos,b->player.y_pos))continue;
        /* Both players can press the SAME original lift/airlock on one
         * world tick. R13 excluded everyone with a pending request, turning
         * that shared-chamber action into two unrelated moves. The second
         * request may only join if the destination AND source exit kind
         * agree, and it is not a separate respawn/cocoon exit. */
        if(avp_mp_transition_peek(&t->transitions,p,&existing) &&
           (t->entering_reset[p] ||
            t->exit_kind[p]!=t->exit_kind[leader] ||
            !avp_mp_transition_same_destination(request,&existing)))continue;
        /* Preserve the relative position where safe. An overflowed offset
         * shares the leader destination without splitting the passengers. */
        if(!avp_mp_chamber_translate_or_stack(request->new_x,request->new_y,
              a->player.x_pos,a->player.y_pos,b->player.x_pos,b->player.y_pos,
              &tx,&ty))return 0;
        staged[p]=*request;
        staged[p].new_x=tx;
        staged[p].new_y=ty;
        mask|=AVP_MP_ACTOR_BIT(p);
    }
    /* Publish passenger staging only once ALL valid offsets are known. */
    for(unsigned p=0;p<t->actors;p++)if(p!=leader && (mask&(AVP_MP_ACTOR_BIT(p))))
        t->entering[p]=staged[p];
    return mask;
}
/* Release the specific original DSP level-effect voice, never the shared
 * ambience or another player's weapon voice. DoEffect may return -1 when the
 * original 8-voice allocator cannot accept the cue: that is NOT a handle. */
static void walk_unloop_exit_handle(u32 handle)
{
    const AvpRuntimeOps *ops;
    if(!handle || handle==(u32)-1)return;
    ops=avp_runtime_ops();
    if(ops->file_event)ops->file_event(ops->user,0x554e4c50u,handle,0,0);
    else StopLoop(handle);
}
/* MAZE.S::PlayAvP::load_complete emits UNLP for levfx_ID after an
 * airlock/lift transition. The multiplayer path never enters PlayAvP's
 * load_complete label. Also drop the old world snapshot's handle: the same
 * world can remain occupied after a group transition. */
static void walk_finish_exit_effect(AvpMpWalkHost *t,unsigned old_level,
                                    u32 handle)
{
    if(!t||old_level<1u||old_level>AVP_MP_RETAIL_LEVELS)return;
    walk_unloop_exit_handle(handle);
    for(unsigned i=0;i<AVP_LOCAL_MP_MAX_ACTORS;i++){
        AvpMpActiveWorld *active=&t->session.worlds[i];
        AvpMpNativeWorld *native=(AvpMpNativeWorld *)active->context;
        if(native && active->level==old_level &&
           native->doors.level_effect_id==handle)
            native->doors.level_effect_id=0;
    }
    if(t->persistent_ready[old_level-1u] &&
       t->persistent_world[old_level-1u].doors.level_effect_id==handle)
        t->persistent_world[old_level-1u].doors.level_effect_id=0;
}
/* Voice zero is globally shared, even with four independent original
 * worlds. Retune it only when the co-op party's level membership changes:
 * one level => that original level's RDB.DTA ambient descriptor;
 * split party => original Sub-Level 2/3/4 ambience (level 2 descriptor).
 * Never restart it per actor, per tick, or on an unchanged membership.
 * This intentionally leaves soundtrack/weapon/alarm ownership untouched. */
/* Keep source locations and actual human listeners in ONE PC stereo mix.
 * The original player slots P1-P4 still own the only local viewports; bots
 * (including AI-controlled P1-P4) never borrow unrelated input or views. */
/* A living, dying, or dead actor with an owned native world is a valid
 * camera subject. The original subject's death camera and FadeDown overlay
 * are rendered by walk_draw_view; changing the subject on death would hide
 * that black fade. Only a deliberate Spy Next/Previous press changes views.
 * World/actor loss is handled by the existing session compose fallback. */
static AvpMpActorMask walk_spy_eligible_mask(const AvpMpWalkHost *t,unsigned viewer)
{
    AvpMpActorMask eligible=AVP_MP_ACTOR_BIT(viewer);
    for(unsigned q=0;q<t->actors;q++){
        if(q==viewer || !t->session.player[q] ||
           !avp_mp_session_player_world(&t->session,q))continue;
        eligible|=AVP_MP_ACTOR_BIT(q);
    }
    return eligible;
}
static void walk_refresh_audio_listeners(AvpMpWalkHost *t)
{
    AvpMpSpatialListener listeners[4]={{0}};
    AvpMpActorMask bot_mask=0u;
    unsigned views=0u,spy_active=0u;
    if(!t)return;
    for(unsigned p=0;p<t->actors && p<AVP_LOCAL_MP_MAX_ACTORS;p++)
        if(t->session.setup.bot_enabled[p])bot_mask|=AVP_MP_ACTOR_BIT(p);
    for(unsigned p=0;p<t->players;p++){
        const unsigned subject=t->session.spy_target[p]<t->actors ?
                                t->session.spy_target[p]:p;
        const AvpMpActiveWorld *w=avp_mp_session_player_world(&t->session,subject);
        const AvpMpActorNativeState *a=
            (const AvpMpActorNativeState *)t->session.player[subject];
        if(!w||!a||!w->level||w->level>15u)continue;
        /* The audio listener follows the *displayed* subject, including
         * their original level, location and facing. Input and game state
         * remain attached to the viewer's own actor. */
        listeners[p].active=avp_mp_spatial_view_listener(p,t->players,w->level);
        listeners[p].subject_plus_one=subject+1u;
        listeners[p].level=w->level;
        listeners[p].x=(s32)a->player.x_pos;
        listeners[p].y=(s32)a->player.y_pos;
        listeners[p].heading=a->player.centre_angle;
        views+=listeners[p].active;
        if(subject!=p)spy_active=1u;
    }
    /* While spying, sounds from the viewer's own character are spatial as
     * well, rather than remaining centered despite the remote camera. The
     * observed player's own effects gain the direct-feedback path below. */
    if(spy_active)bot_mask=AVP_MP_ACTOR_BIT(t->actors)-UINT64_C(1);
    /* Preserve the original solo-bot spectator fallback for an unbound
     * scene, without stealing the other on-screen listeners. */
    if(!views && t->players)listeners[0].active=1u;
    avp_jaguar_fullsynth_mp_set_listeners(listeners,bot_mask);
}
static int walk_sync_ambient(AvpMpWalkHost *t)
{
    unsigned levels[AVP_LOCAL_MP_MAX_ACTORS]={0};
    unsigned target;
    if(!t||!t->players)return 0;
    for(unsigned p=0;p<t->players;p++){
        const unsigned subject=t->session.spy_target[p]<t->actors ?
                               t->session.spy_target[p]:p;
        const AvpMpActiveWorld *w=avp_mp_session_player_world(&t->session,subject);
        if(!w)return 0;
        levels[p]=w->level;
    }
    /* Voice zero is one shared original ambience. Choose it from the
     * displayed viewpoints rather than characters the viewers have left
     * behind. Mixed-level split screens retain the established level-2
     * shared-ambience policy; normal unspied co-op is unchanged. */
    target=avp_mp_ambient_target(levels,t->players);
    if(!target)return 0;
    if(target!=t->current_ambient_level){
        avp_pc_audio_start_ambient(target);
        fprintf(stderr,"MP shared ambience switched to retail level %u (%s party).\n",
                target,levels[0]==levels[t->players-1]?
                (target==levels[0]?"together":"split"):"split");
        t->current_ambient_level=target;
    }
    return 1;
}
static void walk_leave(void *u,AvpMpNativeWorld *w,unsigned p,AvpMpActorNativeState *a)
{(void)u;(void)w;(void)p;(void)a;}
static int walk_prepare_p1(void *u,AvpMpActorNativeState *actor,
                           const AvpMpPlayerCommand *command)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    u32 old,pad=t->last_pad;
    if(t->session.setup.bot_enabled[0])
        return avp_mp_native_input_prepare(actor,command,&t->profiles.player[0]);
    if(!avp_mp_actor_native_valid(actor))return 0;
    old=actor->player.joy_cur;
    actor->player.joy_edge=(~(old^pad))|pad;
    actor->player.joy_cur=pad;
    /* Reuse the same ORIGINAL PC gamepad/mouse axis normalization that
     * pc_main.c uses. Previously the F2 path only passed digital Jaguar
     * buttons and silently threw away the stick's forward/turn axes. */
    actor->player.host_analog_motion=t->p1_analog_enabled;
    actor->player.host_analog_forward=t->p1_forward;
    actor->player.host_analog_strafe=t->p1_strafe;
    actor->player.host_keyboard_strafe_during_turn=t->p1_keyboard_strafe;
    actor->player.host_analog_aim=t->p1_aim_enabled;
    actor->player.host_aim_turn=t->p1_turn;
    actor->player.host_aim_sensitivity=t->p1_aim_sensitivity;
    actor->player.host_aim_speed=t->p1_aim_speed;
    /* Player 1 already has its own original PC gamepad turning controls.
     * Don't inherit a secondary-player digital tuning factor. */
    actor->player.host_digital_turn_sensitivity=1.0f;
    actor->player.host_digital_turn_speed=1.0f;
    actor->player.host_enhanced_analog_aim=t->p1_enhanced_aim;
    actor->player.host_mouse_aim=t->p1_mouse_aim;
    actor->player.host_live_heading_enabled=0;
    return 1;
}
/* Preserve claims for a particular original AMP identity. A host AMP slot
 * can be recycled: distinguish different item types/positions so a later
 * pickup in that slot is not incorrectly treated as already collected. */
static uint64_t walk_pickup_identity(const AvpAmp *amp)
{
    uint64_t hash=UINT64_C(1469598103934665603);
    if(!amp)return 0;
    /* Keep each source field distinct. Direct bitwise XOR between a cell's
     * high position bits and astype could cancel on a reused AMP slot. */
    hash=(hash^(uint32_t)amp->xpos)*UINT64_C(1099511628211);
    hash=(hash^(uint32_t)amp->ypos)*UINT64_C(1099511628211);
    hash=(hash^(uint16_t)amp->astype)*UINT64_C(1099511628211);
    return hash;
}
static void walk_dm_award_body_loot(void)
{
    AvpMpDmLoot loot={0};
    uint16_t chance,ammo_select,tracker_roll;
    if(player_type!=PT_HUMAN ||player_energy<=0)return;
    /* Keep the existing ordered weapon/ammo draws and their exact odds.
     * An independent third draw gives an untracked Marine a 1/16 chance
     * at the motion tracker, even when all owned ammo pools are full.
     * The corpse's once-per-Marine claim is handled by the caller. */
    chance=avp_random();
    ammo_select=avp_random();
    tracker_roll=avp_random();
    if(avp_mp_dm_corpse_loot(cur_weps,ammo_info,chance,ammo_select,&loot))
        (void)CollectIt(loot.type,loot.amount);
    if(avp_mp_dm_tracker_bonus(tracker_roll,show_mt!=0))
        (void)CollectIt(COLL_MTRACKER,0);
}
static void walk_dm_other_bodies(AvpMpWalkHost *t,unsigned p)
{
    unsigned level;
    if(!t ||!t->dm.enabled||player_type!=PT_HUMAN||player_energy<=0)return;
    level=(unsigned)cur_level;
    if(level<1u||level>AVP_MP_RETAIL_LEVELS)return;
    /* Real dead player visual actors have no AMP/grid slot; scavenge their
     * retained death-pose record without turning it into a collision item. */
    if(avp_mp_corpse_claim_at(&t->corpses,level,p,x_pos,y_pos)){
        walk_dm_award_body_loot();
        if(getenv("AVP_BOT_PLAYTEST_TRACE"))
            fprintf(stderr,"BODYLOOT tick=%llu p=%u level=%u xy=%08x/%08x owned=%u ammo=%u,%u,%u,%u\n",
                (unsigned long long)t->session.tick,p+1u,level,x_pos,y_pos,
                (unsigned)cur_weps,(unsigned)ammo_info[0].cur,(unsigned)ammo_info[1].cur,
                (unsigned)ammo_info[2].cur,(unsigned)ammo_info[3].cur);
    }
    /* Fallen native enemy and authored AMP corpses retain their original
     * collection rules. Only an AC_HUMAN player corpse grants random gear. */
}
static int walk_mp_item_available(AvpMpWalkHost *t,unsigned p,
            unsigned level,unsigned slot,const AvpAmp *amp)
{
    AvpMpActorMask bit;
    uint64_t identity;
    if(!t||p>=t->actors||p>=AVP_LOCAL_MP_MAX_ACTORS ||
       level<1u||level>AVP_MP_RETAIL_LEVELS ||slot>=AVP_NUM_AMPS ||!amp)
        return 0;
    bit=AVP_MP_ACTOR_BIT(p);
    identity=walk_pickup_identity(amp);
    if(t->collected_identity[level-1u][slot]!=identity)return 1;
    return !(t->collected_items[level-1u][slot]&bit);
}
static int walk_claim_mp_item(void *u,unsigned level,unsigned slot,s16 type,int commit)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    unsigned p;
    AvpMpActorMask bit;
    uint64_t identity;
    const AvpMpActiveWorld *current;
    const AvpAmp *amp;
    if(!t ||level<1u||level>AVP_MP_RETAIL_LEVELS||slot>=AVP_NUM_AMPS ||
       type<COLL_S1 ||type>COLL_MEDAID)return 0;
    p=t->collecting_player;
    if(p>=t->actors ||p>=AVP_LOCAL_MP_MAX_ACTORS)return 0;
    current=avp_mp_session_player_world(&t->session,p);
    if(!current||current->level!=level)return 0;
    /* This hook executes WITHIN native actor/world scope, so amp_data is
     * the authoritative world state, unlike the out-of-scope snapshot. */
    amp=&amp_data[slot];
    identity=walk_pickup_identity(amp);
    if(t->collected_identity[level-1u][slot]!=identity){
        /* A new item replaced a prior AMP slot. Reset per-player claims on
         * first commit, not during an eligibility query. */
        if(commit){
            t->collected_identity[level-1u][slot]=identity;
            t->collected_items[level-1u][slot]=0u;
        }
    }
    bit=AVP_MP_ACTOR_BIT(p);
    if(t->collected_identity[level-1u][slot]==identity &&
       (t->collected_items[level-1u][slot]&bit))return 0;
    if(commit)t->collected_items[level-1u][slot]|=bit;
    return 1;
}
static void walk_reset_mp_item_claims(AvpMpWalkHost *t,unsigned p)
{
    unsigned level,slot;
    const AvpMpActorMask keep=~(AVP_MP_ACTOR_BIT(p));
    if(!t||p>=AVP_LOCAL_MP_MAX_ACTORS)return;
    for(level=0;level<AVP_MP_RETAIL_LEVELS;level++)
        for(slot=0;slot<AVP_NUM_AMPS;slot++)
            t->collected_items[level][slot]&=keep;
}

/* A duct passenger is outside both levels during their PERSONAL fade.
 * The avatar returns only after the destination fade-in fully completes.
 * Keep the original 60 Hz fade/view rendering; this flag affects world
 * targeting, collision, animation and damage, not the player's camera. */
static int walk_vent_world_present(const AvpMpWalkHost *t,unsigned p)
{
    return t && p<t->actors && !avp_mp_life_busy(&t->vent_fade[p]);
}
static int walk_amp_world_present(void *user,unsigned p)
{
    return walk_vent_world_present((const AvpMpWalkHost *)user,p);
}
static int walk_is_duct_transfer(unsigned from,const AvpMpTransitionRequest *r);
static int walk_bot_shared_lift_pair(const AvpMpWalkHost *t,unsigned p,unsigned q);
static int walk_actor(void *u,unsigned p)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    AvpMpWeaponAudioState old_host;
    int audio_active=t->combat_test;
    s16 previous_level=new_level;
    s32 previous_x=new_x,previous_y=new_y;
    u32 prior_exit_effect=levfx_ID;
    int result=1;
    AvpMpPeerCollisionView peer_view={0};
    /* Each dead player owns a 60-Hz fade. Neither their original movement
     * nor weapons advance while waiting for their own respawn. Their renderer
     * AMP snapshot is refreshed separately before each world update. */
    if(avp_mp_life_busy(&t->player_life[p]) ||
       !walk_vent_world_present(t,p))return 1;
    if(p<t->players && !avp_pc_video_local_mp_hud_message_route(t->video,(int)p))return 0;
    /* LEVELS.S has one exit request per Jaguar player; clear it BEFORE
     * each actor and retain any new exit with its actual owner. */
    new_level=0;
    avp_doors_host_clear_exit_kind();
    if(audio_active && (p>=t->actors ||
        !avp_music_mp_weapon_enter(&old_host,&t->weapon_voice[p]))){
        new_level=previous_level;new_x=previous_x;new_y=previous_y;
        if(p<t->players)(void)avp_pc_video_local_mp_hud_message_route(t->video,-1);
        return 0;
    }
    if(audio_active)avp_jaguar_fullsynth_mp_set_weapon_owner(p);
    avp_jaguar_fullsynth_mp_set_sound_source(p,(unsigned)cur_level,x_pos,y_pos);
    /* MAZE.S::NextFrame publishes per-actor original fixed-point heading
     * before PLAYER.S::MovePlayer. Without it forward/strafe velocity stays
     * zero even though P1/P2 input is being correctly delivered. */
    /* Reuse the original PC 200 ms 180-degree interpolated quick turn.
     * Each participant owns a timeline, and the bound original PLAYER.S
     * receives only this participant's heading. The action never changes
     * the shared-world clock or the other participants' headings. */
    if(t->quick_turn[p].active){
        uint64_t now=avp_pc_now_ns();
        avp_player_host_quick_turn_heading(1,
            avp_pc_quick_turn_angle(&t->quick_turn[p],now));
        if(avp_pc_quick_turn_complete(&t->quick_turn[p],now))
            avp_pc_quick_turn_cancel(&t->quick_turn[p]);
    }else avp_player_host_quick_turn_heading(0,0);
    sin_ang=sin_d0((u16)(centre_angle>>16));
    cos_ang=cos_d0((u16)(centre_angle>>16));
    /* Store this actor's PREVIOUS position and original weapon/AMP/door
     * presentation state before the single 10 Hz simulation step. */
    t->prev_x[p]=x_pos;
    t->prev_y[p]=y_pos;
    t->prev_angle[p]=centre_angle;
    if(p<t->players && !avp_pc_video_local_mp_snapshot_actor(t->video,p,
                 avp_collision_maze_data(),maze_width,maze_height))result=0;
    /* Save the real preceding weapon pose for every spyable subject,
     * including headless bots. It is presentation data, not game state. */
    if(!avp_pc_video_local_mp_snapshot_spy_weapon(t->video,p))result=0;
    {
        const AvpMpInputConfig *cfg=avp_pc_video_local_mp_profiles(t->video);
        const AvpMpInputProfile *pref=cfg&&p<AVP_LOCAL_MP_MAX_PLAYERS?
            &cfg->player[p]:NULL; /* Standalone bots never borrow P2 cheats. */
        const int primary=(p==0u);
        avp_dev_cheats_tracker_borrowed_restore(t->tracker_cheat_borrowed[p]);
        avp_dev_cheats_set_health_mode(p<t->players?avp_pc_video_new_health_mode(t->video,p):AVP_NEW_HEALTH_OFF);
        avp_dev_cheats_set_infinite_ammo(primary?avp_pc_video_infinite_ammo(t->video):
                 (pref?pref->cheat_infinite_ammo:0));
        /* Bots must never inherit a human noclip toggle. In particular,
         * enabling noclip to recover from an invalid position can let them
         * roam farther into exterior void. Keep native collision active. */
        avp_pc_runtime_set_noclip(t->session.setup.bot_enabled[p]?0:
            (primary?avp_pc_video_noclip(t->video):
                     (pref?pref->cheat_noclip:0)));
        /* Original SafePos checks walls/bounds but not the floor selector.
         * Refuse non-floor destinations for bots ONLY at the host movement
         * callback, while leaving source PLAYER.S physics untouched. */
        avp_pc_runtime_set_mp_bot_floor_guard(t->session.setup.bot_enabled[p]);
        avp_pc_runtime_set_mp_player_floor_guard(!t->session.setup.bot_enabled[p]);
        /* Deathmatch Fast Movement is a 2x floor, not a stacking factor.
         * Never apply a human-only cheat to a bot. */
        const int cheat_fast=p<t->players && !t->session.setup.bot_enabled[p] ?
            avp_pc_video_double_movement(t->video,p):0;
        avp_player_host_movement_speed(cheat_fast,t->dm.enabled && t->dm.fast_movement);
        avp_pc_runtime_set_mp_fast_sweep_guard(
            (t->dm.enabled && t->dm.fast_movement) || cheat_fast);
        /* Player collisions apply to humans AND native bots in the same
         * physical world, with spawn/transport grace only for overlapping
         * pairs.  The original SafePos/MovePlayer physics is unchanged. */
        if(t->session.setup.player_collisions){
            const AvpMpActiveWorld *own=avp_mp_session_player_world(&t->session,p);
            peer_view.count=t->actors;
            peer_view.from_x=(int32_t)x_pos;
            peer_view.from_y=(int32_t)y_pos;
            for(unsigned q=0;q<t->actors;q++)if(q!=p){
                const AvpMpActiveWorld *other=avp_mp_session_player_world(&t->session,q);
                const AvpMpActorNativeState *actor=(const AvpMpActorNativeState *)t->session.player[q];
                if(own&&own==other&&actor&&actor->player.player_energy>0 &&
                   !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q) &&
                   !avp_mp_peer_grace_exempt(&t->peer_spawn_grace,p,q) &&
                   !walk_bot_shared_lift_pair(t,p,q)){
                    peer_view.active[q]=1;
                    peer_view.x[q]=actor->player.x_pos;
                    peer_view.y[q]=actor->player.y_pos;
                }
            }
            avp_pc_runtime_set_mp_peer_collision_view(&peer_view);
        }
        avp_player_set_pain_sounds(primary?avp_pc_video_player_pain_sounds_enabled(t->video):
                 (pref?pref->audio_player_pain:1));
        avp_player_set_facehug_pain_sounds(primary?avp_pc_video_facehug_pain_sounds_enabled(t->video):
                 (pref?pref->audio_facehug_pain:1));
        avp_mazescrn_set_facehug_shake_pain_sounds(primary?avp_pc_video_facehug_shake_sounds_enabled(t->video):
                 (pref?pref->audio_facehug_shake:1));
        avp_dev_cheats_set_tracker(primary?avp_pc_video_tracker_cheat(t->video):
                 (pref?pref->cheat_tracker:0));
    }
    if(t->weapon_cycle[p] && player_type!=PT_ALIEN)
        avp_weapons_cycle(t->weapon_cycle[p],4);
    {
        const unsigned cheats=t->pending_cheat_actions[p] |
            (p==0u?t->pending_actions:0u);
        if(cheats&AVP_PC_ACTION_RESTORE_HEALTH)avp_dev_cheats_restore_health();
        if(cheats&AVP_PC_ACTION_RESTORE_AMMO)avp_dev_cheats_restore_ammo();
        if(cheats&AVP_PC_ACTION_GIVE_WEAPONS)avp_dev_cheats_give_weapons();
        if(cheats&AVP_PC_ACTION_SECURITY_UP)avp_dev_cheats_adjust_security(1);
        if(cheats&AVP_PC_ACTION_SECURITY_DOWN)avp_dev_cheats_adjust_security(-1);
        if(p<t->players)avp_pc_original_cheat_apply(cheats);
    }
    /* R208 single-player already bridges the analog forward/back axis to
     * the Jaguar FIRE_B+JOY_UP/DOWN lift chord. The multiplayer route used
     * proportional movement but omitted that source-authored elevator input.
     * Apply exactly the same host adapter to THIS bound actor (P1..P4).
     * No change to original digital lift selection or world door state. */
    {
        const AvpMpActorNativeState *bound=
            (const AvpMpActorNativeState *)t->session.player[p];
        if(bound && bound->player.host_analog_motion &&
           !(joy_cur & (1u<<FIRE_B))){
            joy_cur=avp_pc_lift_analog_pad(joy_cur,1,
                avp_doors_player_facing_lift(),
                bound->player.host_analog_forward);
        }
    }
    if(p<t->players){
        const int now=avp_pc_video_original_god_player(t->video,p);
        /* Retail PC adapter: per-actor mode, not the old shared P1-only flag. */
        avp_pc_video_original_gps_actor(t->video,p,(unsigned)cur_level,x_pos,y_pos);
        avp_pc_video_original_actor_status(t->video,p,(unsigned)player_type,
            (unsigned)cur_weps,show_mt!=0);
        if(now)cheat=-1;
        else if(t->orig_god_last[p])cheat=0;
        if(now!=(int)t->orig_god_last[p]){
            const AvpRuntimeOps *ops=avp_runtime_ops();
            /* Message callbacks run while THIS actor's original HUD scope
             * is bound, never on P1's global message queue by accident. */
            if(ops && ops->set_message)
                ops->set_message(ops->user,now?5u:6u);
        }
        t->orig_god_last[p]=(uint8_t)now;
    }
    joy_cur=avp_pc_original_cheat_alien_pad(joy_cur);
    avp_player_clear_last_pain_sfx();
    t->collecting_player=p;
    avp_collect_set_mp_item_claim(walk_claim_mp_item,t);
    /* Source-authored corpse loot (including white coats/medaid) must use
     * Collectables() unchanged. Deathmatch randomized gear belongs ONLY
     * to fallen Marine player actors, handled after the native tick. */
    avp_collect_set_mp_body_loot(NULL,NULL);
    /* Enhanced stick heading is sampled at host cadence, then consumed ONCE
     * by original MovePlayer; no extra physics or world tick is created. */
    avp_player_host_live_heading(t->live_heading[p].active &&
        !t->quick_turn[p].active,
        avp_pc_live_heading_word(&t->live_heading[p]));
    avp_doors_mp_restrict_interactions(t->dm.enabled ||
                                      t->session.setup.custom_session_id==AVP_MP_CUSTOM_LEVEL,
                                      t->dm.level_travel &&
                                      t->session.setup.custom_session_id!=AVP_MP_CUSTOM_LEVEL);
    avp_dev_cheats_before_actor();
    avp_doors_mp_bot_airlock_interlock(t->session.setup.bot_enabled[p]);
    avp_mp_player_native_step_actor();
    avp_doors_mp_bot_airlock_interlock(0);
    avp_dev_cheats_health_tick();
    avp_player_host_double_movement(0); /* Never leak actor P1-P4 mode. */
    avp_pc_runtime_set_mp_peer_collision_view(NULL);
    avp_pc_runtime_set_mp_bot_floor_guard(0);
    avp_pc_runtime_set_mp_player_floor_guard(0);
    avp_pc_runtime_set_mp_fast_sweep_guard(0);
    avp_doors_mp_restrict_interactions(0,1);
    avp_player_host_live_heading(0,0);
    avp_collect_set_mp_body_loot(NULL,NULL);
    avp_collect_set_mp_item_claim(NULL,NULL);
    walk_dm_other_bodies(t,p);
    {
        /* Native PLAYER.S Fire and MAZESCRN.S weapon actions drive Marine
         * visual loops/shotgun starts. Other species keep their original
         * sprite timelines; no combat timings are modified here. */
        unsigned species=(unsigned)player_type>>2;
        unsigned weapon=(unsigned)(cur_wepno>=0?cur_wepno:0);
        int pressed=wep_fire!=0;
        AvpMpSpritePose pose;
        if(species==0u){
            /* The original weapon script runs in walk_post_actor(), AFTER
             * this player-movement callback. A held button is NOT proof that
             * a shot has fired: let the post-weapon hook update Marine poses
             * from the completed native action/ammunition transaction. */
        }else{
            /* Preserve original Alien/Predator visual attack sequencing,
             * including Alien's five-tick between-combos recovery. */
            if(t->sprite_fire_active[p] &&
               !avp_mp_sprite_attack_pose(species,t->sprite_fire_weapon[p],
                     (unsigned)(t->session.tick-t->sprite_attack_tick[p])).active){
                t->sprite_fire_active[p]=0;
                if(species==1u)t->sprite_alien_repeat_tick[p]=t->session.tick+5u;
            }
            if(avp_mp_sprite_attack_rearm(species,pressed,t->sprite_fire_down[p],
                  t->sprite_fire_active[p],t->session.tick,
                  t->sprite_alien_repeat_tick[p])){
                pose=avp_mp_sprite_attack_pose(species,weapon,0);
                if(pose.active){
                    t->sprite_attack_tick[p]=t->session.tick;
                    t->sprite_fire_weapon[p]=(uint8_t)weapon;
                    t->sprite_fire_active[p]=1;
                }
            }
        }
        t->sprite_fire_down[p]=(uint8_t)pressed;
        /* AMP.S's Predator roar is AS_FIGHT2 with AVP_FX_EDITR, which is
         * also an original player Predator pain cue. Never roar for the
         * other three Predator pain variants. */
        if(species==2u && avp_player_last_pain_sfx()==AVP_FX_EDITR){
            t->sprite_roar_tick[p]=t->session.tick;
            t->sprite_roar_active[p]=1;
        }
        if(t->sprite_roar_active[p] &&
           !avp_mp_sprite_roar_pose((unsigned)(t->session.tick-t->sprite_roar_tick[p])).active)
            t->sprite_roar_active[p]=0;
    }
    /* Original single-player changes levels immediately after operating an
     * airlock. Multiplayer can run a second actor in that SAME original
     * door world before the queued transfer commits. If both players use the
     * airlock, DOORS.S replaces levfx_ID with the newer DoEffect handle.
     * Without releasing the superseded handle, the earlier pressure-loop
     * stays in the global DSP mixer forever and the normal post-transfer
     * StopLoop can only see the last handle. No audio is changed when the
     * handle is unchanged (ordinary single-player never enters this path). */
    if(prior_exit_effect!=levfx_ID)
        walk_unloop_exit_handle(prior_exit_effect);
    /* The F12 infinite-health and ammo cheats must apply to the currently
     * bound actor, not once to an arbitrary process-global Player 1. */
    avp_dev_cheats_tick();
    /* The lightweight no-AMP walktest still needs the original HUD
     * calculations; normal combat mode does these after the world step. */
    if(!t->combat_test){
        UpdtHUDMsg();
        avp_hud_set_low_health_energy_flash(
            p==0u?avp_pc_video_low_health_energy_flash(t->video):
            (p<AVP_MP_INPUT_PLAYERS?
                t->profiles.player[p].low_health_energy_flash:1));
        UpdtHUD();
    }
    {uint16_t lift_x,lift_y;
     if(cur_level>=1 && cur_level<=15 &&
        avp_mp_doors_native_pending_lift(&lift_x,&lift_y) &&
        ((u16)(x_pos>>16))==lift_x && ((u16)(y_pos>>16))==lift_y &&
        (t->lift_owner[(unsigned)cur_level-1u]<0 ||
         !avp_mp_session_player_world(&t->session,
              (unsigned)t->lift_owner[(unsigned)cur_level-1u]) ||
         avp_mp_session_player_world(&t->session,
              (unsigned)t->lift_owner[(unsigned)cur_level-1u])->level!=(unsigned)cur_level ||
         ((const AvpMpActorNativeState *)t->session.player[
              (unsigned)t->lift_owner[(unsigned)cur_level-1u]])->player.x_pos>>16!=lift_x ||
         ((const AvpMpActorNativeState *)t->session.player[
              (unsigned)t->lift_owner[(unsigned)cur_level-1u]])->player.y_pos>>16!=lift_y))
        t->lift_owner[(unsigned)cur_level-1u]=(int8_t)p;}
    /* Actors after P4 are native headless bots, NOT human HUD/map viewports.
     * Sending them through the P1-P4-only automap API faults the first
     * shared-world tick before any weapon updates can run. */
    if(p<t->players && !avp_pc_video_local_mp_automap_observe(t->video,p,
           avp_collision_maze_data(),maze_width,maze_height,
           x_pos,y_pos,centre_angle,(unsigned)cur_level))result=0;
    if(player_energy<=0 && !(p<t->players && avp_pc_video_original_god_player(t->video,p)) &&
       !avp_mp_life_busy(&t->player_life[p])){
        /* Record non-PvP deaths too (acid, enemies, hazards), but do not
         * double-count a death already credited to a PvP shooter. */
        if(t->dm.enabled && !t->dm_death_credited[p]){
            avp_mp_dm_kill(&t->dm,AVP_LOCAL_MP_MAX_ACTORS,p);
            t->dm_death_credited[p]=1u;
        }
        /* This F2 path does not call MAZE.S::Process_EndGame. Deliver its
         * original one-shot species death sound with THIS player's native
         * globals/HUD route bound; never use the campaign species. */
        const AvpRuntimeOps *ops=avp_runtime_ops();
        unsigned species=(unsigned)player_type>>2;
        unsigned sfx=avp_mp_death_sfx(species);
        if(!sfx || !avp_mp_corpse_add(&t->corpses,(unsigned)cur_level,
                                      species,x_pos,y_pos,(u16)(centre_angle>>16),
                                      t->session.tick))result=0;
        if(audio_active){
            /* This actor is still bound to its own original weapon stream
             * and reserved sound voice. Stop its continuous firing effect
             * before the one-shot species death sound. Never KillAll():
             * other players, music, and ambient voices must keep playing. */
            avp_mazescrn_mp_stop_weapon_audio();
            if(t->weapon_voice[p].looping && t->weapon_voice[p].handle)
                KillSfx(t->weapon_voice[p].handle);
        }
        if(ops){
            /* Deathmatch respawns are not Jaguar campaign GAME OVERs.
             * The PC-only kill notice is drawn per viewport after fades. */
            if(!t->dm.enabled && ops->set_message)ops->set_message(ops->user,1u);
            if(sfx && ops->play_sfx)ops->play_sfx(ops->user,sfx);
        }
        t->sprite_fire_active[p]=0;
        t->sprite_fire_down[p]=0;
        t->sprite_marine_crouch[p].phase=AVP_MP_MARINE_CROUCH_IDLE;
        t->sprite_alien_repeat_tick[p]=0;
        t->sprite_roar_active[p]=0;
        /* Reset visual timeline only on death, never at each player tick. */
        t->sprite_motion_active[p]=0;
        t->sprite_cloak_valid[p]=0;
        t->sprite_pain_valid[p]=0;
        t->sprite_pain_tick[p]=0;
        avp_pc_quick_turn_cancel(&t->quick_turn[p]);
        /* Freeze only THIS player's camera at the actual death pose, not
         * at the previous interpolated host frame. Retain all world ticks. */
        avp_mp_death_camera_begin(&t->death_camera[p],x_pos,y_pos,centre_angle);
        t->prev_x[p]=x_pos;t->prev_y[p]=y_pos;
        t->prev_angle[p]=centre_angle;
        avp_pc_live_heading_reset(&t->live_heading[p]);
        /* Original Jaguar player movement is no longer stepped while dying,
         * but cached velocity could otherwise reappear in HUD presentation. */
        ResetMove();player_vel=0;
        avp_mp_life_begin(&t->player_life[p]);
        fprintf(stderr,"MP P%u %s death: original sfx=%x, original corpse on level %u.\n",
            p+1u,avp_local_mp_species_name(species),sfx,(unsigned)cur_level);
    }
    /* COMPUTER.S has one global comp_panel, but any of four Marine actors
     * can operate a terminal before this shared world tick completes.
     * Latch the actual owner + authored panel offset and clear the shared
     * event so the next actor cannot overwrite the first request. */
    if(comp_panel && player_type==PT_HUMAN && !t->dm.enabled){
        t->terminal_pending_mask|=(1u<<p);
        t->terminal_pending_panel[p]=comp_panel;
        t->terminal_pending_offset[p]=comp_offset;
        comp_panel=0;
    }
    if(new_level){
        t->exit_kind[p]=avp_doors_host_exit_kind();
        if(!avp_mp_transition_queue(&t->transitions,p,new_level,
                                     new_x,new_y,use_cocoon))result=0;
        else if(!use_cocoon){
            const AvpMpTransitionRequest *r=&t->transitions.player[p];
            if(walk_is_duct_transfer((unsigned)cur_level,r))
                avp_mp_life_begin(&t->vent_fade[p]);
        }
    }
    /* Original per-actor exit must never leak to the next actor, nor to
     * whatever player was bound before the shared-world transaction. */
    new_level=previous_level;new_x=previous_x;new_y=previous_y;
    if(audio_active){
        avp_jaguar_fullsynth_mp_set_weapon_owner(0u);
        if(!avp_music_mp_weapon_leave(&old_host,&t->weapon_voice[p]))result=0;
    }
    avp_jaguar_fullsynth_mp_clear_sound_source();
    t->tracker_cheat_borrowed[p]=(uint8_t)avp_dev_cheats_tracker_borrowed();
    avp_pc_runtime_set_mp_bot_floor_guard(0);
    avp_pc_runtime_set_mp_player_floor_guard(0);
    avp_pc_runtime_set_mp_fast_sweep_guard(0);
    avp_player_host_double_movement(0);
    avp_pc_runtime_set_noclip(avp_pc_video_noclip(t->video));
    avp_player_set_pain_sounds(avp_pc_video_player_pain_sounds_enabled(t->video));
    avp_player_set_facehug_pain_sounds(avp_pc_video_facehug_pain_sounds_enabled(t->video));
    avp_mazescrn_set_facehug_shake_pain_sounds(avp_pc_video_facehug_shake_sounds_enabled(t->video));
    if(p<t->players && !avp_pc_video_local_mp_hud_message_route(t->video,-1))result=0;
    avp_new_damage_gate_set(0);
    return result;
}

/* Source AMP.S `amp_in_range` uses a square: x/y difference, each axis,
 * never a Euclidean radius. For this same-species experimental combat mode,
 * choose the closest living original actor by that source-shaped Chebyshev
 * distance, breaking ties by player slot. This is NEW multiplayer policy,
 * not a claim that retail Jaguar has multiplayer targeting rules. */
/* Source-derived LineOfSight (0 means unobstructed); the nearest-player
 * and target-lock policy is co-op-specific, not a Jaguar retail rule. Keep
 * a living acquired target while visible. Reacquire nearest visible actor,
 * or nearest living actor if none is visible, and reset on AMP slot reuse. */
static int walk_choose_target(void *u,const AvpAmp *a,unsigned amp_index,
                              AvpMpActorMask mask,const AvpMpActorNativeState *const actors[AVP_LOCAL_MP_MAX_ACTORS])
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    unsigned best=~0u, visible=~0u;
    uint64_t best_dist=UINT64_MAX,visible_dist=UINT64_MAX;
    u32 generation;
    AvpXY from;
    if(!a||!t||amp_index>=AVP_NUM_AMPS)return -1;
    t->dm_amp_target=AVP_LOCAL_MP_MAX_ACTORS;
    ++t->amp_target_requests;
    generation=avp_amp_slot_generation(amp_index);
    {unsigned level=(cur_level>=1 && cur_level<=15)?(unsigned)cur_level-1u:0u;
     if(t->amp_locked_generation[level][amp_index]!=generation){
        t->amp_locked_generation[level][amp_index]=generation;
        t->amp_locked_player[level][amp_index]=-1;
     }}
    from.x=a->xpos;from.y=a->ypos;
    for(unsigned i=0;i<AVP_LOCAL_MP_MAX_ACTORS;i++)if(mask&(AVP_MP_ACTOR_BIT(i))){
        const AvpMpActorNativeState *p=actors[i];
        uint64_t dx,dy,d;
        int clear;
        AvpXY to;
        if(!p||p->player.player_energy<=0||p->player.game_over)continue;
        {
            int64_t x=(int64_t)a->xpos-(int64_t)(s32)p->player.x_pos;
            int64_t y=(int64_t)a->ypos-(int64_t)(s32)p->player.y_pos;
            dx=(uint64_t)(x<0?-x:x);
            dy=(uint64_t)(y<0?-y:y);
        }
        d=dx>dy?dx:dy;
        to.x=(s32)p->player.x_pos;to.y=(s32)p->player.y_pos;
        clear=LineOfSight(&from,&to)==0;
        if((int)i==t->amp_locked_player[(cur_level>=1&&cur_level<=15)?(unsigned)cur_level-1u:0u][amp_index] && clear){
            /* R114: retained locks MUST pass through the same per-target
             * cheat binding as newly chosen targets. Returning here reused
             * the previous AMP's flags and could protect the wrong player. */
            best=i;
            visible=i;
            break;
        }
        if(d<best_dist){best_dist=d;best=i;}
        if(clear && d<visible_dist){visible_dist=d;visible=i;}
    }
    best=visible!=~0u?visible:best;
    if(best!=~0u){
        const AvpMpTargetCheatFlags flags=avp_mp_target_cheats(&t->profiles,best,
            avp_pc_video_attacks_disabled(t->video),
            avp_pc_video_facehug_attach_disabled(t->video));
        /* The original AMP mode runs with one actor scoped at a time.
         * Always bind only this target's flags, including retained locks.
         * Bots cannot inherit P2's cheat flags. The AMP scoped visitor
         * restores the prior world flags after each enemy update. */
        avp_amp_set_attacks_disabled(flags.no_enemy_attack);
        avp_amp_set_facehug_attach_disabled(flags.no_facehug);
    }else{
        /* No target: never carry the previous actor's protection into the
         * next AMP update (especially a later bot or unprotected human). */
        avp_amp_set_attacks_disabled(0);
        avp_amp_set_facehug_attach_disabled(0);
    }
    t->amp_locked_player[(cur_level>=1&&cur_level<=15)?(unsigned)cur_level-1u:0u][amp_index]=best==~0u?-1:(int8_t)best;
    if(best!=~0u)t->dm_amp_target=best;
    return best==~0u?-1:(int)best;
}
static int walk_update_view_contacts(AvpMpWalkHost *t,unsigned p,
                                     unsigned viewer,const AvpMpNativeWorld *world);

/* R64: all Marine third-person attack timing is derived AFTER the original
 * UpdtScrOverlays weapon-script tick. PLAYER.S movement (walk_actor) merely
 * records Fire input; it cannot know whether MAZESCRN actually fired yet.
 * This hook is per scoped player and never advances another gameplay tick. */
static void walk_marine_pose_post_weapon(AvpMpWalkHost *t,unsigned p,
    const AvpMpWeaponEngineState *before,const AvpMpWeaponEngineState *after)
{
    const unsigned weapon=(unsigned)(after->cur_wepno>=0?after->cur_wepno:0);
    const unsigned pressed=wep_fire!=0;
    const unsigned ready=avp_mp_sprite_marine_weapon_ready(weapon,
        after->new_wepno,after->wep_desel,after->mp_equip_phase);
    const unsigned interrupted=avp_mp_sprite_marine_fire_interrupted(
        player_energy,player_dead,pain) || !ready;
    const unsigned fired=avp_mp_sprite_marine_shot_event(weapon,pressed,
        ready, before->cur_wepno==1 && after->cur_wepno==1,
        before->ammo_info[0].cur,after->ammo_info[0].cur);
    avp_mp_sprite_marine_crouch_update(&t->sprite_marine_crouch[p],
        weapon,pressed,interrupted,t->session.tick);
    if(interrupted || t->sprite_fire_weapon[p]!=weapon)
        t->sprite_fire_active[p]=0;
    if(!interrupted && avp_mp_sprite_marine_sustained_weapon(weapon)){
        if(!pressed)t->sprite_fire_active[p]=0;
        else if(!t->sprite_fire_active[p]){
            t->sprite_attack_tick[p]=t->session.tick;
            t->sprite_fire_weapon[p]=(uint8_t)weapon;
            t->sprite_fire_active[p]=1;
        }
    }else if(weapon==1u){
        /* Shotgun sprite begins with a REAL shot, and runs precisely three
         * source-derived frontal frames. It never loops from held Fire. */
        if(t->sprite_fire_active[p] &&
           !avp_mp_sprite_attack_pose_view(0u,1u,(unsigned)(
                t->session.tick-t->sprite_attack_tick[p]),1u,0u).active)
            t->sprite_fire_active[p]=0;
        if(!interrupted && fired){
            t->sprite_attack_tick[p]=t->session.tick;
            t->sprite_fire_weapon[p]=1u;
            t->sprite_fire_active[p]=1u;
        }
    }
    t->sprite_fire_down[p]=(uint8_t)pressed;
}

static int walk_post_actor(void *u,unsigned player)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    AvpMpWeaponAudioState old_host;
    int ok;
    if(player>=t->actors || avp_mp_life_busy(&t->player_life[player]) || !walk_vent_world_present(t,player))return 1;
    if(player<t->players && !avp_pc_video_local_mp_hud_message_route(t->video,(int)player))return 0;
    if(!avp_music_mp_weapon_enter(&old_host,&t->weapon_voice[player])){
        if(player<t->players)(void)avp_pc_video_local_mp_hud_message_route(t->video,-1);
        return 0;
    }
    avp_dev_cheats_set_health_mode(player<t->players?avp_pc_video_new_health_mode(t->video,player):AVP_NEW_HEALTH_OFF);
    avp_jaguar_fullsynth_mp_set_weapon_owner(player);
    avp_jaguar_fullsynth_mp_set_sound_source(player,(unsigned)cur_level,x_pos,y_pos);
    /* The original scanner reads its nearest contact at HUD update time,
     * before the next render. Refresh this actor's real same-level roster
     * here so player motion also drives the normal 10-Hz tracker click. */
    {const AvpMpActiveWorld *active=avp_mp_session_player_world(&t->session,player);
     if(!active || (player<t->players && !walk_update_view_contacts(t,player,player,
                   (const AvpMpNativeWorld *)active->context))){
         avp_jaguar_fullsynth_mp_clear_sound_source();
         avp_jaguar_fullsynth_mp_set_weapon_owner(0u);
         (void)avp_music_mp_weapon_leave(&old_host,&t->weapon_voice[player]);
         (void)avp_pc_video_local_mp_hud_message_route(t->video,-1);
         return 0;
     }}
    /* One original weapon-script update per actor; shared music, ambience,
     * global SFX pool, and alarm are never duplicated per player. */
    /* The active scope here is the shooter, not the last AMP target.
     * PvP shot victims must update their saved snapshots. */
    t->dm_amp_target=AVP_LOCAL_MP_MAX_ACTORS;
    t->dm_shooter=player;
    /* R300: world-scoped HUD.S clock must follow one SESSION timebase.
     * Separate level instances can have different historical frame_counter
     * values; all Marines still see the SAME 120-second alarm countdown. */
    if(t->evacuation_active && !t->dm.enabled)
        avp_hud_set_clock((u32)(t->session.tick+1u),10u);
    avp_mazescrn_mp_fast_equip(t->dm.enabled && t->dm.fast_equip);
    avp_weapons_mp_predator_deathmatch(t->dm.enabled && player_type==PT_PREDATOR);
    /* F2 multiplayer runs outside pc_main.c's single-player settings loop.
     * Refresh this PC enhancement for each bound native actor immediately
     * before its original MAZESCRN weapon tick.  Otherwise the global
     * range flag is never set in an F2 session, despite the menu showing On.
     * The original script still handles damage, hit geometry, wall clipping,
     * ammunition and weapon/species restrictions without modification. */
    avp_mazescrn_set_unlimited_shooting_range(
        avp_pc_video_unlimited_shooting_range(t->video));
    avp_mazescrn_set_alien_cocoon_infinite_ammo_assist(
        avp_pc_video_alien_cocoon_infinite_ammo_assist(t->video) &&
        avp_dev_cheats_infinite_ammo());
    {
        AvpMpWeaponEngineState before,after;
        const int watch_marine=player_type==PT_HUMAN &&
            avp_mp_weapon_native_capture(&before);
        UpdtScrOverlays();
        if(watch_marine && avp_mp_weapon_native_capture(&after))
            walk_marine_pose_post_weapon(t,player,&before,&after);
    }
    avp_weapons_mp_predator_deathmatch(0);
    avp_mazescrn_mp_fast_equip(0);
    t->dm_shooter=AVP_LOCAL_MP_MAX_ACTORS;
    PostFrame();
    avp_dev_cheats_health_tick();
    UpdtHUDMsg();
    /* The original HP2 calculation now receives *this listener's* actual
     * voice PCM samples: own sounds always, others only if same original
     * level and inside the co-op visual meter's proximity range. No change
     * to host audio output or Jaguar weapon/AI behavior. */
    {s32 l=0,r=0;
        avp_jaguar_fullsynth_mp_meter_near(player,(unsigned)cur_level,x_pos,y_pos,&l,&r);
        avp_hud_set_pred_meter_samples(l,r);
        /* Native HUD world is shared, but each actor's Energy preference is not.
         * Restore the proper value immediately before the scoped HUD update. */
        avp_hud_set_low_health_energy_flash(
            player==0u?avp_pc_video_low_health_energy_flash(t->video):
            (player<AVP_MP_INPUT_PLAYERS?
                t->profiles.player[player].low_health_energy_flash:1));
        UpdtHUD();
        if(player<t->players && !avp_pc_video_local_mp_waveform_step(t->video,player,l,r)){
            avp_jaguar_fullsynth_mp_clear_sound_source();
            (void)avp_pc_video_local_mp_hud_message_route(t->video,-1);
            return 0;
        }
    }
    avp_jaguar_fullsynth_mp_clear_sound_source();
    avp_jaguar_fullsynth_mp_set_weapon_owner(0u);
    ok=avp_music_mp_weapon_leave(&old_host,&t->weapon_voice[player]);
    avp_new_damage_gate_set(0);
    ++t->weapon_updates;
    if(player<t->players && !avp_pc_video_local_mp_hud_message_route(t->video,-1))ok=0;
    return ok;
}
/* Run while the original level is scoped, immediately BEFORE the shared
 * world's AMP update. Alive actors already snapshot in walk_actor; dying
 * actors skip that callback, but their fixed-camera view still shows moving
 * enemies. Refresh only their renderer history, without stepping movement,
 * weapons, death fade, or world simulation again. */
static int walk_snapshot_dying_views(void *u,AvpMpActorMask member_mask)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    const u8 *maze=avp_collision_maze_data();
    if(!t || !maze || !maze_width || !maze_height)return 0;
    for(unsigned p=0;p<t->players;p++)if((member_mask&(AVP_MP_ACTOR_BIT(p))) &&
                                           avp_mp_life_busy(&t->player_life[p])){
        AvpMpActorNativeState *a=(AvpMpActorNativeState *)t->session.player[p];
        AvpMpActorNativeScope scope={0};
        int snap,restored;
        if(!a || !avp_mp_actor_scope_enter(&scope,a))return 0;
        snap=avp_pc_video_local_mp_snapshot_actor(t->video,p,
                                                  maze,maze_width,maze_height);
        restored=avp_mp_actor_scope_leave(&scope,a);
        if(!snap || !restored)return 0;
    }
    return 1;
}
/* This debug command belongs to the currently selected PLAYER ONE level
 * (F12 Debug has no per-player selector). Run it exactly once in the bound
 * source world, preserving all player/weapon/campaign state and ALL other
 * loaded levels, including their saved actors and open doors. */
static int walk_debug_refresh_bound_level(AvpMpWalkHost *t)
{
    const unsigned level=(cur_level>=1 && (unsigned)cur_level<=AVP_MP_RETAIL_LEVELS)?
        (unsigned)cur_level:0u;
    const AvpMpActiveWorld *p1;
    AvpMpRetailMap *map;
    const u8 *pristine;
    size_t size;
    if(!t || !t->retail_catalog_ready || !level ||
       t->species>=AVP_MP_RETAIL_CAMPAIGNS)return 0;
    p1=avp_mp_session_player_world(&t->session,0u);
    if(!p1 || p1->level!=level)return 0;
    map=&t->retail_maps.saved[t->species][level-1u];
    pristine=t->retail_maps.original[level-1u];
    size=t->retail_maps.original_size[level-1u];
    if(!pristine || !map->bytes || size!=map->size ||
       map->bytes+8u!=avp_collision_maze_data())return 0;
    memcpy(map->bytes,pristine,size);
    avp_collision_bind_maze(map->bytes+8u,(u16)map->width,(u16)map->height);
    avp_doors_bind_maze(map->bytes+8u,(u16)map->width,(u16)map->height);
    SaveDoors(); /* Fresh door positions replace this level's old save. */
    ResetDoors();
    ResetAccess();
    avp_amp_dev_freshen_current_level();
    restore_level(); /* Authored placements + newly randomized positions. */
    if(t->dm.enabled && !t->dm.enemies)
        (void)avp_amp_dev_clear_enemies();
    memset(t->collected_items[level-1u],0,sizeof(t->collected_items[0]));
    memset(t->collected_identity[level-1u],0,sizeof(t->collected_identity[0]));
    memset(t->dm_body_identity[level-1u],0,sizeof(t->dm_body_identity[0]));
    memset(t->dm_body_claimed[level-1u],0,sizeof(t->dm_body_claimed[0]));
    memset(t->amp_trails[level-1u],0,sizeof(t->amp_trails[0]));
    fprintf(stderr,"F12 Debug: refreshed current multiplayer level %u.\n",level);
    return 1;
}

/* The original AMP.S ACIDX is one shared effect per puddle. Multiplayer
 * contact is evaluated for each living actor; route each NEW contact cue
 * through that actor's source identity rather than playing it globally.
 * The hosted DSP keeps the same original descriptor and synthesis. */
/* The original AMP loop owns every enemy's precise XY.  Apply the
 * source tag only for the duration of THAT enemy mode, so an Alien on
 * Sub-Level 3 cannot be heard by a Sub-Level 4 listener.  This also tags
 * direct DoEffect voices from enemy Marine guns, not only amp_sfx calls. */
static void walk_refill_protected_actor(void *u,unsigned player,int entering)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    AvpNewHealthMode mode=t && player<t->players?
        avp_pc_video_new_health_mode(t->video,player):AVP_NEW_HEALTH_OFF;
    if(entering)avp_new_damage_gate_set(mode==AVP_NEW_HEALTH_INVINCIBLE);
    else{
        avp_new_health_apply(mode,&player_energy,&old_energy,max_energy);
        avp_new_damage_gate_set(0);
    }
}
static void walk_enemy_sound_scope(void *u,const AvpAmp *a,unsigned level,
                                    int entering)
{
    (void)u;
    if(entering && a && a>=amp_data && a<amp_data+AVP_NUM_AMPS)
        avp_jaguar_fullsynth_mp_set_world_sound_source(level,a->xpos,a->ypos,
                                                        (unsigned)(a-amp_data));
    else avp_jaguar_fullsynth_mp_clear_sound_source();
}
static void walk_player_acid_cue(void *u,unsigned player,unsigned level,
                                  int32_t x,int32_t y)
{
    const AvpMpWalkHost *t=(const AvpMpWalkHost *)u;
    const AvpRuntimeOps *ops=avp_runtime_ops();
    if(!t || player>=t->actors || !ops || !ops->play_sfx)return;
    avp_jaguar_fullsynth_mp_set_sound_source(player,level,x,y);
    avp_jaguar_fullsynth_mp_set_personal_cue(1);
    ops->play_sfx(ops->user,AVP_FX_ACIDX);
    avp_jaguar_fullsynth_mp_set_personal_cue(0);
    avp_jaguar_fullsynth_mp_clear_sound_source();
}
/* World-local physical presence, not view count or player collision toggle.
 * Duct/death fades have no physical avatar, and other levels must not
 * influence the door state of this bound level. */
static int walk_door_actor_present(const AvpMpWalkHost *t,unsigned q,unsigned level)
{
    const AvpMpActiveWorld *w=avp_mp_session_player_world(&t->session,q);
    const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[q];
    return w && w->level==level && a && a->player.player_energy>0 &&
        !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q);
}
static unsigned walk_door_occupants(const AvpMpWalkHost *t,unsigned level,
                                   AvpMpDoorOccupant *out)
{
    unsigned count=0;
    for(unsigned q=0;q<t->actors;q++)if(walk_door_actor_present(t,q,level)){
        const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[q];
        out[count++]=(AvpMpDoorOccupant){a->player.x_pos,a->player.y_pos};
    }
    return count;
}

static int walk_world(void *u)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    if(t->pending_actions&AVP_PC_ACTION_REFRESH_LEVEL)
        (void)walk_debug_refresh_bound_level(t);
    /* Global kill-all/type cheats modify the shared original AMP world,
     * not the host/P1 world's stale unbound AMP array. Run once here, with
     * the actual world context bound, not once for each player. */
    if(t->pending_actions&AVP_PC_ACTION_KILL_SELECTED)
        (void)avp_amp_dev_kill_enemies(avp_pc_video_kill_selected_type(t->video));
    if(t->pending_actions&AVP_PC_ACTION_KILL_ALL)
        (void)avp_amp_dev_kill_enemies(-1);
    /* World action, but only for the F12 Activate For player's actual
     * current level. One shared AMP world may contain several players.
     * Execute once in this bound world, never in every loaded level. */
    if(cur_level>=1 && (unsigned)(unsigned)cur_level<=AVP_MP_RETAIL_LEVELS){
        unsigned applied=0u;
        for(unsigned p=0;p<t->players;p++){
            const unsigned actions=(p==0u?t->pending_actions:t->pending_cheat_actions[p]) & ~applied;
            const AvpMpActiveWorld *where=avp_mp_session_player_world(&t->session,p);
            if(!where || where->level!=(unsigned)cur_level)continue;
            if(actions&AVP_PC_ACTION_CLEAR_ENEMIES){
                (void)avp_amp_dev_clear_enemies();
                applied|=AVP_PC_ACTION_CLEAR_ENEMIES;
            }
            if(actions&AVP_PC_ACTION_CLEAR_HAZARDS){
                (void)avp_amp_dev_clear_hazards();
                applied|=AVP_PC_ACTION_CLEAR_HAZARDS;
            }
        }
    }
    {uint16_t lx,ly;
     const unsigned level=(cur_level>=1&&cur_level<=15)?(unsigned)cur_level:0u;
     int8_t owner=level?t->lift_owner[level-1u]:-1;
     u32 host_x=x_pos,host_y=y_pos;
     int16_t prior_new_level=new_level;
     int32_t prior_new_x=new_x,prior_new_y=new_y;
     const AvpMpActiveWorld *member=NULL;
     const AvpMpActorNativeState *passenger=NULL;
     int lift_present=0;
     if(level && avp_mp_doors_native_pending_lift(&lx,&ly)){
        int valid=0;
        if(owner>=0 && (unsigned)owner<t->actors){
            const AvpMpActiveWorld *assigned=avp_mp_session_player_world(&t->session,(unsigned)owner);
            const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[(unsigned)owner];
            valid=assigned && a && walk_door_actor_present(t,(unsigned)owner,level) &&
                (u16)(a->player.x_pos>>16)==lx &&
                (u16)(a->player.y_pos>>16)==ly && a->player.player_energy>0;
        }
        if(!valid){
            owner=-1;
            for(unsigned q=0;q<t->actors;q++){
                const AvpMpActiveWorld *candidate=avp_mp_session_player_world(&t->session,q);
                const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[q];
                if(candidate && a && walk_door_actor_present(t,q,level) &&
                   (u16)(a->player.x_pos>>16)==lx && (u16)(a->player.y_pos>>16)==ly){
                    owner=(int8_t)q;break;
                }
            }
            t->lift_owner[level-1u]=owner;
        }
     }
     if(level && avp_mp_doors_native_pending_lift(&lx,&ly) &&
        owner>=0 && (unsigned)owner<t->actors){
        member=avp_mp_session_player_world(&t->session,(unsigned)owner);
        passenger=(const AvpMpActorNativeState *)t->session.player[(unsigned)owner];
        if(member && passenger && walk_door_actor_present(t,(unsigned)owner,level) &&
           (u16)(passenger->player.x_pos>>16)==lx &&
           (u16)(passenger->player.y_pos>>16)==ly){
            x_pos=passenger->player.x_pos;
            y_pos=passenger->player.y_pos;
            lift_present=1;
        }
     }
     avp_doors_host_clear_exit_kind();
     /* The delayed original lift effect is emitted by world DoorStep, not
      * by an actor update. Attribute its sound to the authored lift cell
      * in THIS bound multiplayer level. SP never enters this host path. */
     {uint16_t sound_x=0u,sound_y=0u;
      const int sound_lift=level && avp_mp_doors_native_pending_lift(&sound_x,&sound_y);
      if(sound_lift)avp_jaguar_fullsynth_mp_set_world_sound_source(level,
          (s32)((uint32_t)sound_x<<16 | 0x8000u),
          (s32)((uint32_t)sound_y<<16 | 0x8000u),AVP_NUM_AMPS+level);
      AvpMpDoorOccupant occupants[AVP_LOCAL_MP_MAX_ACTORS];
      const unsigned count=walk_door_occupants(t,level,occupants);
      const unsigned reversed=avp_mp_doors_native_step_occupied(occupants,count,lift_present);
      if(reversed && getenv("AVP_BOT_PLAYTEST_TRACE"))
          fprintf(stderr,"MP door occupancy: level=%u tick=%llu count=%u reversed=%u\n",
                  level,(unsigned long long)t->session.tick,count,reversed);
      if(sound_lift)avp_jaguar_fullsynth_mp_clear_sound_source();
     }
     if(new_level && new_level!=prior_new_level){
        if(!passenger || !member || member->level!=level ||
           x_pos!=passenger->player.x_pos ||
           y_pos!=passenger->player.y_pos ||
           !avp_mp_transition_queue(&t->transitions,(unsigned)owner,
                            new_level,new_x,new_y,0)){
            fprintf(stderr,"MP delayed lift exit unowned on level %u; keeping world active.\n",level);
            x_pos=host_x;y_pos=host_y;
            new_level=prior_new_level;new_x=prior_new_x;new_y=prior_new_y;
            /* A previous user of the lift may have already transferred;
             * never turn that stale owner into a fatal four-player crash. */
            if(level)t->lift_owner[level-1u]=-1;
            return 1;
        }
        t->exit_kind[(unsigned)owner]=avp_doors_host_exit_kind();
        fprintf(stderr,"MP delayed lift exit: P%u level %u -> %d.\n",
                (unsigned)owner+1u,level,new_level);
     }
     x_pos=host_x;y_pos=host_y;
     new_level=prior_new_level;new_x=prior_new_x;new_y=prior_new_y;
     if(level && !avp_mp_doors_native_pending_lift(&lx,&ly))t->lift_owner[level-1u]=-1;
    }
    return 1;
}
static void walk_live_look_poll(AvpMpWalkHost *t,uint64_t now,unsigned sim_hz)
{
    if(!t)return;
    for(unsigned p=0;p<t->players;p++){
        const AvpMpActorNativeState *a=
            (const AvpMpActorNativeState *)t->session.player[p];
        const AvpMpInputProfile *profile=&t->profiles.player[p];
        float axis=0.0f;
        int enabled;
        if(!a || t->session.setup.bot_enabled[p] ||
           avp_mp_life_busy(&t->player_life[p]) ||
           !walk_vent_world_present(t,p) ||
           t->quick_turn[p].active || a->weapon.hugkill){
            avp_pc_live_heading_reset(&t->live_heading[p]);
            continue;
        }
        enabled=p==0u ? avp_pc_video_live_stick_turn(t->video,&axis) :
            avp_pc_video_local_mp_live_turn(t->video,p,&axis);
        if(!enabled){
            avp_pc_live_heading_reset(&t->live_heading[p]);
            continue;
        }
        if(t->live_heading[p].active &&
           a->player.centre_angle!=t->live_last_game_angle[p])
            avp_pc_live_heading_reset(&t->live_heading[p]);
        (void)avp_pc_live_heading_poll(&t->live_heading[p],
            a->player.centre_angle,now,axis,
            p==0u?avp_pc_video_turn_sensitivity(t->video):profile->gamepad_turn_sensitivity,
            p==0u?avp_pc_video_turn_speed(t->video):profile->gamepad_turn_speed,
            a->player.max_avel,sim_hz);
        t->live_last_game_angle[p]=a->player.centre_angle;
    }
}
/* Populate the actual player's same-level scanner contacts before
 * HUD.S::TracTest() runs at 10 Hz, and again at presentation time. Use the
 * existing map marker roster so that neither dead/corpse players nor actors
 * in another original Jaguar level appear as moving motion contacts. */
static int walk_update_view_contacts(AvpMpWalkHost *t,unsigned p,
                                     unsigned viewer,const AvpMpNativeWorld *world)
{
    AvpGpuMapTeammate dots[AVP_GPU_MAP_TEAMMATES];
    AvpGpuMapTeammate presentation[AVP_GPU_MAP_TEAMMATES];
    unsigned count=0;
    int smooth;
    if(!t||!world||p>=t->actors||viewer>=t->players)return 0;
    smooth=avp_pc_video_smooth_presentation(t->video) && t->session.tick!=0;
    for(unsigned q=0;q<t->actors;q++){
        const AvpMpActorNativeState *other;
        const AvpMpActiveWorld *pw;
        pw=avp_mp_session_player_world(&t->session,q);
        other=(const AvpMpActorNativeState *)t->session.player[q];
        if(!other || !avp_mp_tracker_peer_eligible(p,q,world,
                pw?pw->context:NULL,
                avp_mp_life_busy(&t->player_life[q]) || !walk_vent_world_present(t,q),
                other->player.player_energy,other->player.game_over))continue;
        if(count>=AVP_GPU_MAP_TEAMMATES)break;
        dots[count].x=other->player.x_pos;
        dots[count].y=other->player.y_pos;
        presentation[count]=dots[count];
        if(smooth){
            const AvpPresentationPose prev={
                t->prev_x[q],t->prev_y[q],t->prev_angle[q]};
            const AvpPresentationPose curr={
                other->player.x_pos,other->player.y_pos,
                other->player.centre_angle};
            AvpPresentationPose shown;
            /* Match other-player world sprites: interpolate normal 10-Hz
             * movement, but NEVER smear a teleported/respawned contact
             * across the map or over an unrelated level. The eligibility
             * check above already rejects dead/different-level players. */
            if(avp_presentation_interpolate_pose(&prev,&curr,
                    t->presentation_alpha,&shown)){
                presentation[count].x=shown.x;
                presentation[count].y=shown.y;
            }
        }
        ++count;
    }
    return avp_pc_video_local_mp_map_teammates(t->video,viewer,dots,
                             presentation,count,t->map_zoom_steps[p]);
}

/* Same directional definition as the source AAF selector. The special
 * stationary/angled Marine pose must be determined for EACH viewer: a Marine
 * can be facing P1 but sideways to P2 in the same native simulation tick. */
static int walk_marine_attack_front(const AvpGpuOtherPlayer *other,
                                    u32 viewer_x,u32 viewer_y)
{
    const double pi=3.14159265358979323846;
    double dx=(double)other->x/65536.0-(double)viewer_x/65536.0;
    double dy=(double)other->y/65536.0-(double)viewer_y/65536.0;
    double face=-(double)other->angle*(2.0*pi/65536.0);
    double rel=atan2(dy,dx)-face;
    while(rel<-pi)rel+=2.0*pi;
    while(rel>pi)rel-=2.0*pi;
    return fabs(rel)>3.0*pi/4.0;
}

static int walk_draw_view(void *u,AvpMpNativeWorld *world,unsigned p,
                          unsigned viewer,AvpMpActorNativeState *actor,
                          AvpLocalMpSourceFrame *out)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    const AvpMpRetailMap *m=world->map;
    u32 rx=x_pos, ry=y_pos, ra=centre_angle;
    (void)actor;
    if(avp_pc_video_smooth_presentation(t->video) && t->session.tick){
        const AvpPresentationPose pp={t->prev_x[p],t->prev_y[p],t->prev_angle[p]};
        const AvpPresentationPose cp={x_pos,y_pos,centre_angle};
        AvpPresentationPose rp;
        /* Match single-player's guard: never interpolate a scripted warp,
         * a level transfer or a discontinuous facing change. */
        const int enhanced=actor && actor->player.host_enhanced_analog_aim;
        if((enhanced ? avp_presentation_interpolate_pose_continuous(&pp,&cp,
                     t->presentation_alpha,&rp) :
                      avp_presentation_interpolate_pose(&pp,&cp,
                     t->presentation_alpha,&rp))){
            rx=rp.x;ry=rp.y;ra=rp.angle;
        }
    }
    if(avp_pc_video_smooth_presentation(t->video) &&
       t->live_heading[p].active &&
       !t->quick_turn[p].active && !avp_mp_life_busy(&t->player_life[p]))
        ra=avp_pc_live_heading_word(&t->live_heading[p]);
    if(t->quick_turn[p].active && !avp_mp_life_busy(&t->player_life[p]))
        ra=avp_pc_quick_turn_angle(&t->quick_turn[p],avp_pc_now_ns());
    /* Explicitly override smooth presentation after interpolation. The
     * dying player's fixed viewpoint can still see moving enemies/players
     * and the active death fade, without drifting or turning. */
    avp_mp_death_camera_apply(&t->death_camera[p],&rx,&ry,&ra);
    /* Other participants use the *original* creature AAF and source sprite
     * projection; no flat host overlays. The view must never see itself or
     * anyone in a different world, or a participant mid death/respawn fade.
     * Prepare all pose data before entering the renderer; this is visual
     * only and never writes the original enemy allocator/grid. */
    {
        AvpGpuOtherPlayer others[AVP_GPU_OTHER_PLAYERS_MAX];
        unsigned visible=0;
        /* Dead players have left visual-only original AAF corpse poses in
         * the death world. A later respawn or same-room transfer must not
         * teleport or erase their body. */
        visible=avp_mp_corpses_for_level(&t->corpses,world->map->level,
                    t->session.tick,others,AVP_GPU_OTHER_PLAYERS_MAX);
        for(unsigned q=0;q<t->actors;q++){
            const AvpMpActorNativeState *b;
            const AvpMpActiveWorld *pw;
            s16 type;
            int moving;
            if(q==p || avp_mp_life_busy(&t->player_life[q]) ||
               !walk_vent_world_present(t,q))continue;
            pw=avp_mp_session_player_world(&t->session,q);
            if(!pw || pw->context!=world)continue;
            b=(const AvpMpActorNativeState *)t->session.player[q];
            if(!b || b->player.game_over)continue;
            type=b->player.player_type;
            if(type!=PT_HUMAN && type!=PT_ALIEN && type!=PT_PREDATOR)continue;
            if(visible>=AVP_GPU_OTHER_PLAYERS_MAX)return 0;
            others[visible].x=(s32)b->player.x_pos;
            others[visible].y=(s32)b->player.y_pos;
            others[visible].angle=(u16)(b->player.centre_angle>>16);
            others[visible].creature=type==PT_HUMAN?AC_HUMAN:
                        type==PT_ALIEN?AC_ALIEN:AC_PREDATOR;
            {
                moving=(b->player.x_pos!=t->prev_x[q] ||
                            b->player.y_pos!=t->prev_y[q]);
                unsigned species=(unsigned)type>>2;
                AvpMpSpritePose pose;
                if(moving && !t->sprite_motion_active[q]){
                    t->sprite_motion_tick[q]=t->session.tick;
                    t->sprite_motion_active[q]=1;
                }else if(!moving)t->sprite_motion_active[q]=0;
                pose=avp_mp_sprite_motion_pose(species,(unsigned)moving,
                    (unsigned)(t->session.tick-t->sprite_motion_tick[q]));
                others[visible].animation=pose.sequence;
                others[visible].frame=pose.frame;
                /* The original Predator player invisibility state is much
                 * shorter than the enemy AMP.S visual cloak sheet. Track the
                 * visible sheet independently at source AMP frame cadence. */
                if(type==PT_PREDATOR){
                    unsigned cloaked=(unsigned)(b->player.invis_act!=0);
                    if(!t->sprite_cloak_valid[q]){
                        t->sprite_cloak_valid[q]=1;
                        t->sprite_cloak_target[q]=(uint8_t)cloaked;
                        t->sprite_cloak_transition[q]=(uint8_t)cloaked;
                        t->sprite_cloak_tick[q]=t->session.tick;
                        if(cloaked && b->player.invis_stat==2 &&
                           b->player.predo_count==0)
                            t->sprite_cloak_tick[q]=t->session.tick-17u;
                    }else if(t->sprite_cloak_target[q]!=cloaked){
                        t->sprite_cloak_target[q]=(uint8_t)cloaked;
                        t->sprite_cloak_transition[q]=1;
                        t->sprite_cloak_tick[q]=t->session.tick;
                    }
                }
                /* All three third-person avatars use their original AMP
                 * enemy recoil cadence (two native ticks). The player's
                 * longer first-person PLAYER.S pain/fade stays unchanged. */
                avp_mp_sprite_enemy_recoil_observe(&t->sprite_pain_valid[q],
                        &t->sprite_pain_last_energy[q],&t->sprite_pain_tick[q],
                        b->player.player_energy,t->session.tick);
                if((pose=avp_mp_sprite_pending_death_pose(
                            b->player.player_energy)).active){
                    others[visible].animation=pose.sequence;
                    others[visible].frame=pose.frame;
                }else if(type==PT_PREDATOR &&
                    (t->sprite_cloak_target[q] || t->sprite_cloak_transition[q]) &&
                    (pose=avp_mp_sprite_predator_cloak_pose(
                        t->sprite_cloak_target[q],
                        (unsigned)(t->session.tick-t->sprite_cloak_tick[q]))).active){
                    others[visible].animation=pose.sequence;
                    others[visible].frame=pose.frame;
                }else if(type==PT_PREDATOR && t->sprite_roar_active[q] &&
                         (pose=avp_mp_sprite_roar_pose((unsigned)(
                                    t->session.tick-t->sprite_roar_tick[q]))).active){
                    others[visible].animation=pose.sequence;
                    others[visible].frame=pose.frame;
                }else if(t->sprite_pain_valid[q] &&
                         (pose=avp_mp_sprite_enemy_recoil_pose((unsigned)(
                                    t->session.tick-t->sprite_pain_tick[q]))).active){
                    others[visible].animation=pose.sequence;
                    others[visible].frame=pose.frame;
                }else if(type==PT_HUMAN &&
                    avp_mp_sprite_marine_crouch_weapon(t->sprite_fire_weapon[q]) &&
                    walk_marine_attack_front(&others[visible],rx,ry) &&
                    (pose=avp_mp_sprite_marine_crouch_pose_present(
                        &t->sprite_marine_crouch[q],t->session.tick,
                        t->presentation_alpha,t->sprite_fire_weapon[q])).active){
                    /* Release animation remains visible even after native
                     * Fire is up. A non-front observer keeps R61's original
                     * side/back behavior; no extra firing is performed. */
                    others[visible].animation=pose.sequence;
                    others[visible].frame=pose.frame;
                }else if(t->sprite_fire_active[q] &&
                    /* The frontend has already claimed Pulse/Smart frames:
                     * never pre-display the generic crouch sprite, which
                     * otherwise precedes/duplicates the wind-up. */
                    !avp_mp_sprite_marine_front_owns_attack(species,
                        t->sprite_fire_weapon[q],
                        type==PT_HUMAN &&
                        walk_marine_attack_front(&others[visible],rx,ry)) &&
                    (pose=avp_mp_sprite_attack_pose_view(species,
                        t->sprite_fire_weapon[q],(unsigned)(
                            t->session.tick-t->sprite_attack_tick[q]),
                        type==PT_HUMAN &&
                            walk_marine_attack_front(&others[visible],rx,ry),
                        type==PT_HUMAN &&
                            avp_mp_sprite_marine_sustained_weapon(
                                t->sprite_fire_weapon[q]))).active){
                    others[visible].animation=pose.sequence;
                    others[visible].frame=pose.frame;
                }else if(!moving && type==PT_PREDATOR){
                    pose=avp_mp_sprite_stand_pose(species,
                                (unsigned)b->weapon.cur_wepno);
                    if(pose.active){
                        others[visible].animation=pose.sequence;
                        others[visible].frame=pose.frame;
                    }
                }

            }
            if(avp_pc_video_smooth_presentation(t->video) && t->session.tick){
                const AvpPresentationPose pp={t->prev_x[q],t->prev_y[q],t->prev_angle[q]};
                const AvpPresentationPose cp={b->player.x_pos,b->player.y_pos,
                                             b->player.centre_angle};
                AvpPresentationPose rp;
                const int enhanced=b->player.host_enhanced_analog_aim;
                if((enhanced ? avp_presentation_interpolate_pose_continuous(&pp,&cp,
                              t->presentation_alpha,&rp) :
                               avp_presentation_interpolate_pose(&pp,&cp,
                              t->presentation_alpha,&rp))){
                    others[visible].x=(s32)rp.x;
                    others[visible].y=(s32)rp.y;
                    others[visible].angle=(u16)(rp.angle>>16);
                }
            }
            if(avp_pc_video_smooth_presentation(t->video) &&
               t->live_heading[q].active && !t->quick_turn[q].active)
                others[visible].angle=(u16)(avp_pc_live_heading_word(
                    &t->live_heading[q])>>16);
            if(t->quick_turn[q].active)
                others[visible].angle=(u16)(avp_pc_quick_turn_angle(
                    &t->quick_turn[q],avp_pc_now_ns())>>16);
            if(type==PT_HUMAN && !moving &&
               (others[visible].animation==AS_FIGHT1 ||
                others[visible].animation==AS_FIGHT2) &&
               !walk_marine_attack_front(&others[visible],rx,ry))
                others[visible].frame=0;
            ++visible;
        }
        if(!avp_pc_video_local_mp_set_other_players(t->video,viewer,others,visible))return 0;
    }
    if(!walk_update_view_contacts(t,p,viewer,world))return 0;
    avp_pc_video_local_mp_spy_status(t->video,viewer,p,t->dm.enabled,
                       t->dm.kills[p],t->dm.deaths[p]);
    if(!avp_pc_video_local_mp_render_actor(t->video,viewer,p,m->bytes+8u,
          (u16)m->width,(u16)m->height,rx,ry,ra,m->level,
          t->session.tick,out))return 0;
    if(viewer==p && viewer<t->players &&
       !avp_mp_life_frame_preserve(&t->vent_frame[viewer],&t->vent_fade[p],
            (uint32_t *)(uintptr_t)out->pixels,out->width,out->height,out->pitch))
        return 0;
    avp_mp_life_apply(&t->player_life[p],(uint32_t *)(uintptr_t)out->pixels,
                      out->width,out->height,out->pitch);
    avp_mp_life_apply(&t->vent_fade[p],(uint32_t *)(uintptr_t)out->pixels,
                      out->width,out->height,out->pitch);
    /* Render the PC kill confirmation AFTER death/transition fades, so the
     * victim can read KILLED BY even during the black death screen. */
    if(t->dm.enabled && viewer<t->players){
        unsigned other=0u;
        unsigned kind=avp_mp_kill_notice_visible_for_viewer(
            t->dm_notices,t->players,viewer,t->session.tick,&other);
        const AvpMpActorNativeState *viewer_actor=
            (const AvpMpActorNativeState *)t->session.player[viewer];
        if(kind && !avp_pc_video_local_mp_draw_kill_notice(t->video,viewer,kind,other,
            viewer_actor?viewer_actor->player.player_type:actor->player.player_type))
            return 0;
    }
    return 1;
}
static int walk_draw(void *u,AvpMpNativeWorld *world,unsigned p,
                     AvpMpActorNativeState *actor,AvpLocalMpSourceFrame *out)
{return walk_draw_view(u,world,p,p,actor,out);}
static int walk_draw_spy(void *u,AvpMpNativeWorld *world,unsigned p,
                         unsigned viewer,AvpMpActorNativeState *actor,
                         AvpLocalMpSourceFrame *out)
{return walk_draw_view(u,world,p,viewer,actor,out);}
static void walk_cleanup(AvpMpWalkHost *t)
{
    for(unsigned p=0;p<AVP_LOCAL_MP_MAX_PLAYERS;p++)
        avp_mp_life_frame_release(&t->vent_frame[p]);
    /* A game-over, an aborted transfer, or closing the co-op window does
     * not enter PlayAvP::load_complete. Release any still-owned airlock/lift
     * loops while the original per-world handles remain available. This is
     * handle-specific; never call KillAll/KillAmbi on normal co-op teardown. */
    for(unsigned i=0;i<AVP_LOCAL_MP_MAX_ACTORS;i++){
        const AvpMpActiveWorld *active=&t->session.worlds[i];
        const AvpMpNativeWorld *native=(const AvpMpNativeWorld *)active->context;
        if(native)walk_unloop_exit_handle(native->doors.level_effect_id);
    }
    for(unsigned i=0;i<AVP_MP_RETAIL_LEVELS;i++)
        if(t->persistent_ready[i])
            walk_unloop_exit_handle(t->persistent_world[i].doors.level_effect_id);
    avp_player_set_mp_shot_hook(NULL,NULL);
    avp_amp_set_mp_projectile_hooks(NULL,NULL,NULL,NULL,NULL);
    avp_amp_mp_set_campaign_type(-1);
    avp_amp_mp_defer_end_queen(0);
    avp_jaguar_fullsynth_mp_set_listeners(NULL,0u);
    avp_mp_session_stop(&t->session);
    if(t->retail_catalog_ready){
        for(unsigned i=0;i<AVP_MP_RETAIL_LEVELS;i++)
            if(t->persistent_ready[i])
                (void)avp_mp_retail_maps_release(&t->retail_maps,t->species,
                          i+1u,t->persistent_world[i].map);
        if(t->map_leased && !t->persistent_ready[t->level-1u])
            (void)avp_mp_retail_maps_release(&t->retail_maps,t->species,
                                             t->level,t->map);
    }
    t->map_leased=0;
    if(t->retail_catalog_ready){
        (void)avp_mp_retail_maps_destroy(&t->retail_maps);
        t->retail_catalog_ready=0;
    }
    free(t->diagnostic_map.bytes);
    t->diagnostic_map.bytes=NULL;
    t->map=NULL;
}
static int walk_dm_shot(void *user,s32 origin_x,s32 origin_y,
    s32 boxx,s32 boxy,s16 cs,s16 sn,u16 width,u32 closest,s16 damage);
static void walk_dm_projectile_spawn(void *user,const AvpAmp *projectile);
static int walk_dm_projectile_hit(void *user,const AvpAmp *projectile);
static void walk_dm_explosion_spawn(void *user,const AvpAmp *bolt,const AvpAmp *explosion_amp);
static int walk_dm_explosion_damage(void *user,const AvpAmp *explosion_amp);
static int walk_init(AvpMpWalkHost *t,AvpPcVideo *v,unsigned level,
                     const AvpMazeCandidate *retail_level,
                     const AvpMazeCandidate *all_mazes,unsigned maze_count,
                     const AvpRomImage *rom,int combat_test,unsigned players,
                     const AvpLocalMpSetup *requested)
{
    u8 *source=avp_collision_maze_data();
    size_t size;
    AvpLocalMpSetup setup;
    AvpMpWorldBackend ops;
    const AvpMpInputConfig *controls=avp_pc_video_local_mp_profiles(v);
    if(!t||!v||!controls||!source||!maze_width||maze_width>64u||
       !maze_height||maze_height>64u||level<1u||level>15u||
       players<1u||players>AVP_LOCAL_MP_MAX_PLAYERS)return 0;
    avp_amp_set_enemy_number_multiplier(avp_pc_video_enemy_number_multiplier(v));
    avp_amp_set_vent_facehuggers(avp_pc_video_vent_facehuggers(v));
    avp_pc_video_apply_enemy_stats(v);
    t->video=v;t->rom=rom;t->level=level;t->combat_test=combat_test!=0;t->players=players;
    t->species=(unsigned)player_type>>2;
    if(t->species>AVP_MP_PREDATOR)return 0;
    t->profiles=*controls;
    memset(t->amp_locked_player,-1,sizeof(t->amp_locked_player));
    memset(t->lift_owner,-1,sizeof(t->lift_owner));
    if(!avp_mp_actor_native_capture(&t->initial_actor))return 0;
    {AvpMpWeaponAudioState original;
     if(!avp_music_mp_weapon_capture(&original))return 0;
     for(unsigned p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)t->weapon_voice[p]=original;}

    size=8u+(size_t)maze_width*maze_height*8u;
    /* The F2 retail session now owns all fifteen ORIGINAL decoded levels
     * through the existing persistent campaign map store. Preparing the
     * catalog does NOT activate cross-level travel: native AMP/door/HUD
     * initialization and GPU/audio ownership must also be transactional. */
    if(retail_level){
        AvpMpRetailMapSource sources[AVP_MP_RETAIL_LEVELS]={{0}};
        unsigned seen=0;
        if(!all_mazes||!maze_count||maze_count>64u ||
           retail_level->file_index!=86u+level ||
           retail_level->decoded_size!=size ||
           retail_level->width!=maze_width ||
           retail_level->height!=maze_height || !retail_level->decoded)return 0;
        for(unsigned i=0;i<maze_count;i++){
            const AvpMazeCandidate *m=&all_mazes[i];
            if(m->file_index<AVP_MP_RETAIL_MAZE_FIRST_FILE ||
               m->file_index>=AVP_MP_RETAIL_MAZE_FIRST_FILE+AVP_MP_RETAIL_LEVELS)
                continue;
            if(seen>=AVP_MP_RETAIL_LEVELS)return 0;
            sources[seen].file_index=m->file_index;
            sources[seen].decoded=m->decoded;
            sources[seen].decoded_size=m->decoded_size;
            sources[seen].width=m->width;
            sources[seen].height=m->height;
            ++seen;
        }
        if(seen!=AVP_MP_RETAIL_LEVELS ||
           !avp_mp_retail_maps_init(&t->retail_maps,sources,seen))return 0;
        t->retail_catalog_ready=1;
        /* Parse authored level-specific panel IDs without rebinding DOORS.S
         * or disturbing the currently active game's globals. */
        if(rom&&rom->exact_retail_sha256){
            for(unsigned i=0;i<AVP_MP_RETAIL_LEVELS;i++)
                t->bot_panels_ready[i]=(uint8_t)
                    avp_resources_retail_panel_info(rom,i+1u,
                         &t->bot_panel_ids[i],t->bot_panel_list[i],NULL);
        }
        t->map=avp_mp_retail_maps_acquire(&t->retail_maps,t->species,level);
        if(!t->map)return 0;
        t->map_leased=1;
        if(t->map->size!=size ||
           memcmp(t->map->bytes,retail_level->decoded,size)!=0)return 0;
    }else{
        /* Diagnostic geometry may still be used without distributing ROM
         * bytes; keep its old source-specific header allocation. */
        t->map=&t->diagnostic_map;
        t->map->bytes=(uint8_t *)calloc(1,size);
        if(!t->map->bytes)return 0;
        t->map->bytes[0]=(uint8_t)(maze_width>>8);
        t->map->bytes[1]=(uint8_t)maze_width;
        t->map->bytes[2]=(uint8_t)(maze_height>>8);
        t->map->bytes[3]=(uint8_t)maze_height;
        memcpy(t->map->bytes+8u,source,size-8u);
        t->map->size=size;t->map->width=maze_width;
        t->map->height=maze_height;t->map->level=level;
        t->map->leases=1;t->map_leased=1;
    }
    if(!avp_mp_native_capture_active_world(&t->initial_world,t->map))
        return 0;
    if(t->retail_catalog_ready){
        t->persistent_world[level-1u]=t->initial_world;
        t->persistent_ready[level-1u]=1;
    }
    t->host.user=t;
    t->host.profiles=&t->profiles;
    t->host.create_actor=walk_create_actor;
    t->host.create_level=walk_create_level;
    t->host.destroy_level=walk_destroy_level;
    t->host.join_level=walk_join;
    t->host.join_group=walk_join_group;
    t->host.leave_level=walk_leave;
    t->host.prepare_player1=walk_prepare_p1;
    t->host.advance_actor=walk_actor;
    t->host.snapshot_world_presentation=walk_snapshot_dying_views;
    t->host.advance_level=walk_world;
    if(t->combat_test){
        t->host.choose_amp_target=walk_choose_target;
        t->host.actor_world_present=walk_amp_world_present;
        t->host.on_acid_contact=walk_player_acid_cue;
        t->host.on_enemy_sound_scope=walk_enemy_sound_scope;
        t->host.on_actor_damage=walk_refill_protected_actor;
        t->host.advance_actor_post_world=walk_post_actor;
    }
    t->host.draw_actor=walk_draw;
    t->host.draw_spy_actor=walk_draw_spy;
    avp_local_mp_defaults(&setup);
    setup.enabled=1;setup.players=t->players;setup.campaign=t->species;
    {const AvpLocalMpSetup *menu=avp_pc_video_local_mp_setup(v);
     if(menu){
         avp_local_mp_copy_session_options(&setup,menu);
         for(unsigned p=0;p<t->players;p++)
             setup.bot_enabled[p]=menu->bot_enabled[p];
     }}
    if(requested){
        AvpLocalMpSetup selected=*requested;
        avp_local_mp_validate(&selected);
        if(selected.players!=t->players || selected.campaign!=t->species)return 0;
        avp_local_mp_copy_session_options(&setup,&selected);
        setup.custom_session_id=selected.custom_session_id;
        for(unsigned p=0;p<t->players;p++){
            setup.species[p]=selected.species[p];
            setup.bot_enabled[p]=selected.bot_enabled[p];
        }
    }else for(unsigned p=0;p<t->players;p++){
        setup.species[p]=t->species;
        /* Non-menu diagnostic sessions never silently enable bots. */
    }
    avp_local_mp_validate(&setup);
    t->actors=avp_local_mp_total_actors(&setup);
    if(t->actors>AVP_LOCAL_MP_MAX_ACTORS)return 0;
    for(unsigned p=0;p<t->actors;p++){
        setup.level[p]=level;
        if(setup.species[p]==AVP_MP_RANDOM){
            for(unsigned species=0;species<3u;species++)
                if(!walk_prepare_species_start(t,species))return 0;
        }else if(!walk_prepare_species_start(t,setup.species[p]))return 0;
    }
    avp_mp_dm_init(&t->dm,&setup); /* set BEFORE create_actor callbacks */
    if(t->dm.enabled && !t->dm.enemies){
        unsigned removed=avp_mp_dm_remove_enemies(&t->initial_world.amp);
        if(t->retail_catalog_ready){
            t->persistent_world[level-1u]=t->initial_world;
            t->persistent_ready[level-1u]=1;
        }
        fprintf(stderr,"Deathmatch initial NPC population suppressed: %u hostiles.\n",removed);
    }
    avp_pc_video_local_mp_runtime_setup(v,&setup);
    ops=avp_mp_native_backend_ops();
    if(!avp_mp_session_start(&t->session,&setup,&ops,&t->host))return 0;
    if(!walk_clear_arrival_enemies(t,
        (AVP_MP_ACTOR_BIT(t->actors)-UINT64_C(1))))return 0;
    /* First-spawn randomization is independent from death/respawn. Apply
     * per actor after the original first world exists, through the normal
     * transactional destination factory, before the first rendered frame. */
    if(setup.spawn_in_random_level && setup.custom_session_id!=AVP_MP_CUSTOM_LEVEL){
        for(unsigned p=0;p<t->actors;p++)
            if(!walk_random_level_spawn(t,p))return 0;
        walk_shared_arrival_refresh(t,AVP_MP_ACTOR_BIT(t->actors)-UINT64_C(1));
    }
    walk_refresh_audio_listeners(t);
    t->dm_shooter=AVP_LOCAL_MP_MAX_ACTORS;
    t->dm_amp_target=AVP_LOCAL_MP_MAX_ACTORS;
    if(t->dm.enabled){
        avp_player_set_mp_shot_hook(walk_dm_shot,t);
        avp_amp_set_mp_projectile_hooks(walk_dm_projectile_spawn,
            walk_dm_projectile_hit,walk_dm_explosion_spawn,
            walk_dm_explosion_damage,t);
    }
    if(t->dm.enabled && t->dm.fast_movement)for(unsigned p=0;p<t->actors;p++){
        AvpMpActorNativeState *a=(AvpMpActorNativeState *)t->session.player[p];
        if(a)avp_mp_dm_speed(1,&a->player.max_speed,&a->player.acc_shift);
    }
    avp_amp_mp_set_campaign_type((s16)(t->species<<2));
    avp_amp_mp_set_predator_honor_coop(t->species==AVP_MP_PREDATOR && !t->dm.enabled);
    t->current_ambient_level=level; /* host already started the original level cue */
    for(unsigned p=0;p<t->players;p++){
        const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
        if(!a || !avp_pc_video_local_mp_snapshot_actor(v,p,source,maze_width,maze_height))
            return 0;
        t->prev_x[p]=a->player.x_pos;
        t->prev_y[p]=a->player.y_pos;
        t->prev_angle[p]=a->player.centre_angle;
        t->live_last_game_angle[p]=a->player.centre_angle;
    }
    walk_shared_arrival_refresh(t,AVP_MP_ACTOR_BIT(t->actors)-UINT64_C(1));
    t->presentation_alpha=1.0;
    return 1;
}

/* The current retail experiment owns only its initially loaded level.
 * Restore the original fresh-start actor state without ever reinitializing
 * doors or AMPs in a world occupied by the other players.  The snapshot was
 * captured AFTER the authentic InitPlayer/InitScrOverlays sequence; no
 * replacement gameplay initialization is invented here. */
/* AI observes the actual original AMP/world snapshots. Never create a
 * "bot physics" path: every decision is translated to the same native
 * command buttons used by human participants. The retail ROM supplies all
 * level connections, enemies, access metadata and item types. */
static uint64_t walk_bot_distance(u32 ax,u32 ay,u32 bx,u32 by)
{
    int64_t dx=(int64_t)ax-(int64_t)bx,dy=(int64_t)ay-(int64_t)by;
    return (uint64_t)(dx*dx+dy*dy);
}
static int walk_bot_amp_active(const AvpAmp *a)
{ return a && (a->mode||a->host_static) && !(a->flags&(1u<<AMP_DEAD)); }
static int walk_bot_hostile(const AvpAmp *a)
{
    if(!walk_bot_amp_active(a) || a->energy<=0 ||
       !(a->flags&(1u<<AMP_KILLABLE)))return 0;
    return a->creature==AC_ALIEN || a->creature==AC_ACRAWL ||
           a->creature==AC_FACEHUG || a->creature==AC_EGG ||
           a->creature==AC_HUMAN || a->creature==AC_PREDATOR ||
           a->creature==AC_AQUEEN;
}
/* A native Marine can cocoon only after the original STUN death phase and
 * physical approach. The bot must NEVER fabricate a new cocoon. */
static int walk_bot_cocoon_candidate(const AvpAmp *a)
{
    return a && a->mode && a->creature==AC_HUMAN && a->energy<=0 &&
           (a->flags&(1u<<AMP_STUN));
}
static void walk_bot_mark_acid(const AvpMpNativeWorld *w,uint8_t risk[4096],
                               AvpMpBotObservation *view)
{
    memset(risk,0,4096u);
    view->acid_count=0u;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        const AvpAmp *a=&w->amp.actors[i];
        unsigned x,y;
        /* The dying Alien remains AC_ALIEN while its original mode performs
         * contact acid damage. Do not confuse presentation blood sprites
         * with the source damage-causing acid puddle. */
        if(!avp_amp_is_acid_puddle(a))continue;
        x=(u32)a->xpos>>16;y=(u32)a->ypos>>16;
        if(x>=64u||y>=64u)continue;
        if(view->acid_count<AVP_MP_BOT_MAX_ACID){
            AvpMpBotAcid *hz=&view->acid[view->acid_count++];
            hz->x=(u32)a->xpos;hz->y=(u32)a->ypos;
        }
        avp_mp_bot_acid_risk_mark(risk,(u32)a->xpos,(u32)a->ypos);
    }
}
/* Original COLLIDE.S::coll_widths: registered AMP collision halfwidths.
 * Mirror the source values ONLY for read-only bot steering; original
 * AMPCollisions remains authoritative for actual player movement. */
static uint32_t walk_bot_amp_halfwidth(unsigned creature)
{
    static const int32_t width[40]={
        (50<<9)*85/100,(50<<9)*85/100,(50<<9)*85/100,
        (40<<9)*85/100,(40<<9)*85/100,50<<9,
        0,0,0,0,0,
        (40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,
        (40<<9)*85/100,(40<<9)*85/100,
        (10<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,
        (35<<9)*85/100,(40<<9)*85/100,(35<<9)*85/100,
        (40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,
        (40<<9)*85/100,(40<<9)*85/100,
        0,0,0,0,0,(50<<9)*85/100,(40<<9)*85/100,
        (40<<9)*85/100,(80<<9)*85/100,(40<<9)*85/100,(80<<9)*85/100
    };
    return creature<40u ? (uint32_t)width[creature] : 0u;
}
static void walk_bot_mark_props(const AvpMpNativeWorld *world,
                               AvpMpBotObservation *view,
                               uint8_t prop_risk[4096])
{
    uint8_t observed[AVP_NUM_AMPS]={0};
    memset(prop_risk,0,4096u);
    view->prop_count=0u;
    /* COLLIDE.S consults registered grid IDs, NOT every rendered sprite.
     * An inactive/collected or stale grid ID cannot block the player. */
    for(unsigned y=0;y<64u;y++)for(unsigned x=0;x<64u;x++){
        unsigned owner=world->amp.collmap[y*64u+x];
        const AvpAmp *a;
        uint32_t half;
        if(!owner||owner>AVP_NUM_AMPS||observed[owner-1u])continue;
        a=&world->amp.actors[owner-1u];
        if((!a->mode&&!a->host_static)||((u32)a->xpos>>16)!=x||
           ((u32)a->ypos>>16)!=y)continue;
        half=walk_bot_amp_halfwidth(a->creature);
        if(!half)continue;
        observed[owner-1u]=1u;
        prop_risk[y*64u+x]=1u;
        if(view->prop_count<AVP_MP_BOT_MAX_NEAR_PROPS &&
           walk_bot_distance(view->x,view->y,
                   (u32)a->xpos,(u32)a->ypos)<
                   ((uint64_t)3u<<16)*((uint64_t)3u<<16)){
            AvpMpBotProp *p=&view->props[view->prop_count++];
            p->x=(u32)a->xpos;p->y=(u32)a->ypos;p->halfwidth=half;
        }
    }
}
static int walk_bot_source_los(AvpMpNativeWorld *world,
                                u32 x,u32 y,u32 tx,u32 ty)
{
    AvpMpNativeScope scope={0};
    AvpXY from={(s32)x,(s32)y},to={(s32)tx,(s32)ty};
    int clear=0;
    if(!avp_mp_native_world_enter(&scope,world))return 0;
    clear=LineOfSight(&from,&to)==0;
    if(!avp_mp_native_world_leave(&scope,world))return 0;
    return clear;
}
/* Original sight can be clear while a source-collision furniture AMP is
 * directly between the Marine and an Alien. This probe is ONLY a movement
 * heuristic; original TestSpark / FireDistance still decide actual hits.
 * It never treats a hostile AMP as invisible furniture or peeks beyond a
 * currently known last-seen/tracker contact. */
static int walk_bot_prop_line_clear(const AvpMpNativeWorld *world,
    u32 x,u32 y,u32 tx,u32 ty,unsigned target_slot)
{
    const double ax=(double)x/65536.0,ay=(double)y/65536.0;
    const double dx=(double)((int64_t)tx-(int64_t)x)/65536.0;
    const double dy=(double)((int64_t)ty-(int64_t)y)/65536.0;
    const double length=sqrt(dx*dx+dy*dy);
    const unsigned samples=(unsigned)(length*8.0)+1u;
    if(!world || !samples)return 0;
    for(unsigned i=1u;i<samples;i++){
        const double t=(double)i/(double)samples;
        const double cx=ax+dx*t,cy=ay+dy*t;
        const unsigned bx=(unsigned)cx,by=(unsigned)cy;
        unsigned owner;
        const AvpAmp *obstacle;
        const uint32_t half_limit=(uint32_t)(0.125*65536.0);
        uint32_t half;
        if(cx<0.0||cy<0.0||bx>=64u||by>=64u)continue;
        owner=world->amp.collmap[by*64u+bx];
        if(!owner||owner>AVP_NUM_AMPS||owner-1u==target_slot)continue;
        obstacle=&world->amp.actors[owner-1u];
        if((!obstacle->mode&&!obstacle->host_static) ||
           walk_bot_hostile(obstacle))continue;
        half=walk_bot_amp_halfwidth(obstacle->creature);
        if(!half)continue;
        /* Do not veto the muzzle merely because it starts inside an
         * obstacle's large original collision square. */
        if(t*length<0.25 || (1.0-t)*length<0.28)continue;
        if(fabs(cx-(double)obstacle->xpos/65536.0)<
               (double)(half+half_limit)/65536.0 &&
           fabs(cy-(double)obstacle->ypos/65536.0)<
               (double)(half+half_limit)/65536.0)return 0;
    }
    return 1;
}
typedef struct WalkBotLaneProbe {
    AvpMpNativeWorld *world;
    unsigned target_slot;
} WalkBotLaneProbe;
static int walk_bot_firing_lane_probe(void *ctx,u32 x,u32 y,u32 tx,u32 ty)
{
    const WalkBotLaneProbe *q=(const WalkBotLaneProbe *)ctx;
    return q && walk_bot_source_los(q->world,x,y,tx,ty) &&
           walk_bot_prop_line_clear(q->world,x,y,tx,ty,q->target_slot);
}
static int walk_bot_trail_los(void *ctx,u32 x,u32 y,u32 tx,u32 ty)
{ return walk_bot_source_los((AvpMpNativeWorld *)ctx,x,y,tx,ty); }
/* Incremental map exploration is scoped to this bot, not the positions of
 * hidden enemies. Valid original routes are mandatory for patrol targets. */
static void walk_bot_patrol(AvpMpWalkHost *t,unsigned p,
                             AvpMpBotObservation *v,unsigned level)
{
    unsigned x=v->x>>16,y=v->y>>16,tx,ty;
    uint64_t dx,dy;
    if(!v->map.cells||x>=v->map.width||y>=v->map.height)return;
    if(t->patrol[p].level!=level){
        memset(&t->patrol[p],0,sizeof(t->patrol[p]));
        t->patrol[p].level=level;
    }
    if(v->tick%6u==0u && t->patrol[p].visits[y*64u+x]!=65535u)
        ++t->patrol[p].visits[y*64u+x];
    dx=(uint64_t)llabs((long long)(int64_t)v->x-((long long)t->patrol[p].x));
    dy=(uint64_t)llabs((long long)(int64_t)v->y-((long long)t->patrol[p].y));
    if(t->patrol[p].valid && t->bots[p].acid_exit_count && v->map.risk){
        AvpMpBotObservation previous=*v;
        previous.goal=AVP_MP_BOT_PATROL;previous.has_target=1u;
        previous.hostile_target=0u;
        previous.target_x=t->patrol[p].x;previous.target_y=t->patrol[p].y;
        if(avp_mp_bot_acid_goal_unsafe(&t->bots[p],&previous) ||
           avp_mp_bot_acid_goal_blocked(&previous)){
            t->patrol[p].valid=0u;
            t->bots[p].nav_retarget_patrol=0u;
        }
    }
    if(t->patrol[p].valid){
        if(t->bots[p].nav_retarget_patrol){
            const unsigned gx=t->patrol[p].x>>16,gy=t->patrol[p].y>>16;
            if(gx<64u && gy<64u)t->patrol[p].visits[gy*64u+gx]=65535u;
            t->patrol[p].valid=0u;
            t->bots[p].nav_retarget_patrol=0u;
        }
    }
    if(t->patrol[p].valid){
        const unsigned gx=t->patrol[p].x>>16,gy=t->patrol[p].y>>16;
        const unsigned gi=gy*64u+gx;
        const uint64_t remaining=dx>dy?dx:dy;
        /* Movement alone is not progress: bots may strafe in circles or
         * oscillate across the same blocked wall forever. Only a >=0.25
         * tile reduction in the farthest goal axis resets the watchdog. */
        if(remaining+16384u<=t->patrol[p].best_goal_distance){
            t->patrol[p].best_goal_distance=remaining;
            t->patrol[p].last_progress_tick=v->tick;
        }
        /* A legal Jaguar corridor can initially go AWAY from its final
         * destination (a U-shaped detour). The R10 Euclidean-only watchdog
         * called that a stall despite actual path progress. Recheck the
         * original door/acid/prop cost field at most once every 4 bot ticks,
         * and credit progress only after entering another floor cell.
         * Merely rotating in place or waiting for a hazard to disappear
         * cannot reset the stall timer. */
        if(v->map.cells && (v->tick&3u)==0u &&
           y*64u+x!=t->patrol[p].last_route_progress_cell){
            AvpMpBotRoute remaining_route={0};
            if(avp_mp_bot_route(&v->map,x,y,gx,gy,&remaining_route) &&
               remaining_route.cost<t->patrol[p].best_route_cost &&
               t->patrol[p].best_route_cost-remaining_route.cost>=10u){
                t->patrol[p].best_route_cost=remaining_route.cost;
                t->patrol[p].last_route_progress_cell=y*64u+x;
                t->patrol[p].last_progress_tick=v->tick;
            }
        }
        if(dx<45875u && dy<45875u){ /* actually at destination, <=0.70 tile */
            if(gi<AVP_MP_BOT_MAP_CELLS && t->patrol[p].visits[gi]<60000u)
                t->patrol[p].visits[gi]+=500u;
            t->patrol[p].valid=0u;
        }else if(v->tick>=t->patrol[p].expires ||
                 v->tick-t->patrol[p].last_progress_tick>=
                     (level==4u?130u:75u)){
            /* Reject the repeatedly blocked destination until this level
             * is reset; don't choose it again after every 19-tick recovery. */
            if(gi<AVP_MP_BOT_MAP_CELLS)t->patrol[p].visits[gi]=65535u;
            t->patrol[p].valid=0u;
        }
    }
    if(!t->patrol[p].valid){
        uint16_t personal[AVP_MP_BOT_MAP_CELLS];
        memcpy(personal,t->patrol[p].visits,sizeof personal);
        /* Existing per-bot visits are independent. Also reserve other
         * live bot patrol destinations (not opponents' hidden locations),
         * so similarly spawned bots don't all choose the same route. */
        for(unsigned q=0;q<t->actors;q++)if(q!=p &&
           t->session.setup.bot_enabled[q] && t->patrol[q].valid &&
           t->patrol[q].level==level){
            int px=(int)(t->patrol[q].x>>16),py=(int)(t->patrol[q].y>>16);
            for(int yy=py-2;yy<=py+2;yy++)for(int xx=px-2;xx<=px+2;xx++)
                if(xx>=0&&yy>=0&&xx<64&&yy<64)
                    personal[(unsigned)yy*64u+(unsigned)xx]=65535u;
        }
        if(avp_mp_bot_patrol_pick(&v->map,x,y,personal,
         (uint32_t)(p+1u)*0x9e3779b9u+(uint32_t)(v->tick/180u),&tx,&ty)){
        t->patrol[p].x=(tx<<16)|0x8000u;
        t->patrol[p].y=(ty<<16)|0x8000u;
        /* The Training Maze (Sub-Level 4) has long, winding hallways and
         * dead ends. Give the path enough wall-clock time to physically
         * traverse its Dijkstra distance rather than resetting every 18s.
         * Keep a separate progress watchdog for real stalls. */
        {AvpMpBotRoute planned={0};
         uint64_t budget=level==4u?1500u:600u;
         if(avp_mp_bot_route(&v->map,x,y,tx,ty,&planned)){
             const uint64_t travel=(uint64_t)planned.cost*(level==4u?8u:5u);
             if(travel>budget)budget=travel;
         }
         if(budget>(level==4u?4200u:2400u))budget=level==4u?4200u:2400u;
         t->patrol[p].expires=v->tick+budget;}
        t->patrol[p].last_progress_tick=v->tick;
        {
            uint64_t gx=(uint64_t)llabs((long long)(int64_t)v->x-(long long)t->patrol[p].x);
            uint64_t gy=(uint64_t)llabs((long long)(int64_t)v->y-(long long)t->patrol[p].y);
            t->patrol[p].best_goal_distance=gx>gy?gx:gy;
        }
        t->patrol[p].valid=1u;
        t->patrol[p].last_route_progress_cell=y*64u+x;
        t->patrol[p].best_route_cost=UINT32_MAX;
        {AvpMpBotRoute route={0};
         if(avp_mp_bot_route(&v->map,x,y,tx,ty,&route))
             t->patrol[p].best_route_cost=route.cost;}
        /* Unwind recovery commands from a PREVIOUS unreachable goal. */
        t->bots[p].immobile_ticks=0u;
        t->bots[p].evade_ticks=0u;
        t->bots[p].last_motion=0u;
        }
    }
    if(t->patrol[p].valid){
        v->has_target=1u;v->hostile_target=0u;
        v->goal=AVP_MP_BOT_PATROL;
        v->target_x=t->patrol[p].x;v->target_y=t->patrol[p].y;
    }
}
/* One observation pass per bot per native 10-Hz tick. An enemy's current
 * position is NEVER handed to the planner unless it was observed through
 * original LineOfSight (forward 140-degree field or a short-range proximity
 * alert for a living threat),
 * or is detected by an actually-owned Marine motion tracker in 15-m range.
 * Stale contacts only supply their LAST OBSERVED POSITION for four seconds.
 * This policy applies to both original AMP enemies and Deathmatch players. */
static void walk_bot_scan_contacts(AvpMpWalkHost *t,unsigned p,
    const AvpMpNativeWorld *world,const AvpMpActorNativeState *actor,
    const AvpMpBotObservation *v)
{
    AvpMpBotContact *contacts=t->contacts[p];
    const unsigned level=world->map->level;
    const int tracker=v->player_species==AVP_MP_MARINE &&
                      actor->hudview.show_mt!=0;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        const AvpAmp *a=&world->amp.actors[i];
        const int usable=walk_bot_hostile(a) || walk_bot_cocoon_candidate(a);
        const u32 x=(u32)a->xpos,y=(u32)a->ypos;
        const int radar=usable && tracker && (a->flags&(1u<<AMP_MOTION)) &&
            avp_bot_motion_range(v->x,v->y,x,y);
        /* R298: Source-LOS-gated proximity detection. A Facehugger can
         * sprint in from beside/behind a Marine before entering the old
         * 140-degree cone. Give active Facehuggers a four-tile warning,
         * adult/Queen melee threats three, active ranged enemies two.
         * Never disclose an egg, item, wall-hidden or closed-door actor.
         * Keep the forward cone's original eight-tile reach otherwise. */
        unsigned peripheral_tiles=0u;
        if(a->creature==AC_FACEHUG)peripheral_tiles=4u;
        else if(a->creature==AC_ALIEN || a->creature==AC_ACRAWL ||
                a->creature==AC_AQUEEN)peripheral_tiles=3u;
        else if(a->creature==AC_PREDATOR || a->creature==AC_HUMAN)
            peripheral_tiles=2u;
        const int sight=usable &&
            avp_bot_in_view_or_nearby(v->heading,v->x,v->y,x,y,
                                     peripheral_tiles) &&
            walk_bot_source_los((AvpMpNativeWorld *)world,v->x,v->y,x,y);
        AvpMpBotTrail *trail=&t->amp_trails[level-1u][i];
        if(!usable){
            memset(&contacts[i],0,sizeof contacts[i]);
            avp_bot_trail_clear(trail);continue;
        }
        avp_bot_trail_sample(trail,level,world->amp.slot_generation[i],v->tick,x,y);
        avp_bot_contact_update_budget(&contacts[i],level,
            world->amp.slot_generation[i],v->tick,x,y,sight,radar,
            v->behavior==AVP_MP_BOT_BERSERKER?120u:AVP_BOT_MEMORY_TICKS);
        if(!sight&&!radar){
            AvpMpBotTrailPoint crumb;
            if(avp_bot_trail_visible(trail,level,world->amp.slot_generation[i],
                  v->tick,v->heading,v->x,v->y,walk_bot_trail_los,
                  (void *)world,&crumb))
                avp_bot_contact_breadcrumb(&contacts[i],&crumb,v->tick);
        }
    }
    for(unsigned q=0;q<AVP_LOCAL_MP_MAX_ACTORS;q++){
        AvpMpBotContact *c=&contacts[AVP_NUM_AMPS+q];
        const AvpMpActorNativeState *enemy;
        u32 x,y;
        int radar,sight;
        if(!t->dm.enabled||q==p||q>=t->actors||
           avp_mp_life_busy(&t->player_life[q]) || !walk_vent_world_present(t,q) ||
           avp_mp_session_player_world(&t->session,q)!=
             avp_mp_session_player_world(&t->session,p) ||
           !(enemy=(const AvpMpActorNativeState *)t->session.player[q])||
           enemy->player.player_energy<=0 || enemy->player.game_over){
            memset(c,0,sizeof(*c));continue;
        }
        x=enemy->player.x_pos;y=enemy->player.y_pos;
        /* New local-multiplayer motion contacts obey the original scanner
         * range. No opponent position is learned through a wall beyond 15 m. */
        radar=tracker && avp_bot_motion_range(v->x,v->y,x,y);
        sight=!enemy->player.invisflag &&
            avp_bot_in_view(v->heading,v->x,v->y,x,y) &&
            walk_bot_source_los((AvpMpNativeWorld *)world,v->x,v->y,x,y);
        /* Invisible Predator players leave no retroactively discoverable
         * optical trail. Marine motion-tracker contacts remain independent. */
        if(enemy->player.invisflag)avp_bot_trail_clear(&t->player_trails[q]);
        else avp_bot_trail_sample(&t->player_trails[q],level,
                                 1u+t->dm.deaths[q],v->tick,x,y);
        avp_bot_contact_update_budget(c,level,1u+t->dm.deaths[q],
                                v->tick,x,y,sight,radar,
            v->behavior==AVP_MP_BOT_BERSERKER?120u:AVP_BOT_MEMORY_TICKS);
        if(!sight&&!radar && !enemy->player.invisflag){
            AvpMpBotTrailPoint crumb;
            if(avp_bot_trail_visible(&t->player_trails[q],level,
                  1u+t->dm.deaths[q],v->tick,v->heading,v->x,v->y,
                  walk_bot_trail_los,(void *)world,&crumb))
                avp_bot_contact_breadcrumb(c,&crumb,v->tick);
        }
    }
}
/* Next interlevel hop from the original LEVDOOR.S authored exit table, plus
 * source DOORS.S lift adjacency on levels 1..5 (for non-Aliens only). */
static unsigned walk_bot_next_level(const AvpDoorLevelMeta *metas,
                                   unsigned meta_count,unsigned current,
                                   unsigned destination,unsigned species,
                                   unsigned excluded_first_hops)
{
    unsigned queue[AVP_MAX_LEVEL],head=0,tail=0,prev[AVP_MAX_LEVEL+1u]={0};
    if(!metas||meta_count<AVP_MAX_LEVEL||!current||
       current>AVP_MAX_LEVEL||!destination||destination>AVP_MAX_LEVEL||
       current==destination)return 0;
    queue[tail++]=current;prev[current]=current;
    while(head<tail){
        unsigned cur=queue[head++];
        const AvpDoorLevelMeta *m=&metas[cur-1u];
        for(size_t i=0;i<m->exit_count;i++){
            unsigned to=m->exits[i].level;
            if(!to||to>AVP_MAX_LEVEL||prev[to] ||
               (cur==current && (excluded_first_hops&(1u<<to))) ||
               !avp_mp_bot_transition_allowed(species,cur,to,
                                              AVP_MP_BOT_TRANS_EXIT))continue;
            prev[to]=cur;queue[tail++]=to;
        }
        if(cur<=5u){
            for(int d=-1;d<=1;d+=2){
                unsigned to=(unsigned)((int)cur+d);
                if(to<1u||to>5u||prev[to] ||
                   (cur==current && (excluded_first_hops&(1u<<to))) ||
                   !avp_mp_bot_transition_allowed(species,cur,to,
                                                  AVP_MP_BOT_TRANS_LIFT))continue;
                prev[to]=cur;queue[tail++]=to;
            }
        }
    }
    if(!prev[destination])return 0;
    {unsigned step=destination;
     for(unsigned guard=0;guard<AVP_MAX_LEVEL;guard++){
         if(prev[step]==current)return step;
         if(prev[step]==step||!prev[step])break;
         step=prev[step];
     }}
    return 0;
}
/* Find a real door panel/lift in the actor's CURRENT maze. The panel side
 * and destination are never fabricated. Cheapest path wins, not nearest
 * Euclidean position through a wall. */
static int walk_bot_choose_exit(const AvpMpNativeWorld *w,
                                unsigned target_level,unsigned species,
                                unsigned from_x,unsigned from_y,
                                const AvpMpBotMap *map,
                                AvpMpBotObservation *v)
{
    unsigned chosen_cost=UINT32_MAX;
    if(!w||!map||!map->cells||!target_level)return 0;
    if(w->doors.current_meta){
        const AvpDoorLevelMeta *meta=w->doors.current_meta;
        for(size_t i=0;i<meta->exit_count;i++){
            const AvpDoorExitEntry *e=&meta->exits[i];
            unsigned index=e->panel_offset/8u,side=e->panel_offset%8u;
            unsigned x=index%map->width,y=index/map->width;
            AvpMpBotRoute route;
            unsigned panel=0u,duct_panel=0u;
            if(e->level!=target_level||side>=4u||x>=map->width||y>=map->height)
                continue;
            panel=(unsigned)(map->cells[(size_t)index*8u+side]&0x7fu);
            if(panel>=1u && panel<=AVP_MAX_PANELS){
                unsigned file=w->doors.panel_list[panel-1u];
                duct_panel=(w->doors.panels.wall2!=0xffu &&
                            file==w->doors.panels.wall2) ||
                           (w->doors.panels.airduct1!=0xffu &&
                            file==w->doors.panels.airduct1);
            }
            if(!avp_mp_bot_transition_allowed(species,w->map->level,target_level,
                    duct_panel?AVP_MP_BOT_TRANS_DUCT:AVP_MP_BOT_TRANS_EXIT) ||
               !avp_mp_bot_route(map,from_x,from_y,x,y,&route))continue;
            if(route.cost>=chosen_cost)continue;
            chosen_cost=route.cost;
            v->target_x=(x<<16)|0x8000u;v->target_y=(y<<16)|0x8000u;
            v->target_side=side;v->goal=AVP_MP_BOT_EXIT;
            v->current_level=w->map->level;v->destination_level=target_level;
            v->exit_is_duct=duct_panel;
        }
    }
    if(avp_mp_bot_transition_allowed(species,w->map->level,target_level,
                                     AVP_MP_BOT_TRANS_LIFT) &&
       w->map->level<=5u &&
       target_level>=1u && target_level<=5u &&
       (target_level+1u==w->map->level || target_level==w->map->level+1u)){
        for(unsigned y=0;y<map->height;y++)for(unsigned x=0;x<map->width;x++){
            const u8 *cell=map->cells+((size_t)y*map->width+x)*8u;
            for(unsigned side=0;side<4u;side++){
                unsigned clean=(unsigned)(cell[side]&0x7fu),file_id;
                AvpMpBotRoute route;
                if(clean==0u||clean>AVP_MAX_PANELS)continue;
                file_id=w->doors.panel_list[clean-1u];
                if(file_id!=w->doors.panels.lift_back &&
                   file_id!=w->doors.panels.lift_side)continue;
                /* DOORS.S::do_lifts rejects the actual lift at (9,21)
                 * in precisely these two directions. Never queue its Use
                 * command: another usable lift/airlock may be available. */
                if(avp_mp_bot_lift_jammed(w->map->level,x,y,
                        target_level>w->map->level?1:-1))continue;
                if(!avp_mp_bot_route(map,from_x,from_y,x,y,&route) ||
                   route.cost>=chosen_cost)continue;
                chosen_cost=route.cost;
                v->target_x=(x<<16)|0x8000u;v->target_y=(y<<16)|0x8000u;
                v->target_side=side;v->goal=AVP_MP_BOT_LIFT;
                v->current_level=w->map->level;v->destination_level=target_level;
                v->exit_is_duct=0u;
                v->lift_direction=target_level>w->map->level?1:-1;
            }
        }
    }
    if(chosen_cost==UINT32_MAX)return 0;
    v->has_target=1;v->hostile_target=0;
    return 1;
}
/* Build only read-only maps for the original retail world graph. Future
 * levels use the original cartridge maze; visited worlds use the saved,
 * mutable campaign copy, so open doors stay open across return visits. */
static int walk_bot_level_layouts(const AvpMpWalkHost *t,
                          const AvpMpNativeWorld *current,
                          unsigned access_level,
                          AvpMpBotLevelLayout layouts[AVP_MAX_LEVEL])
{
    const AvpDoorLevelMeta *metas;
    if(!t||!t->retail_catalog_ready||!current||
       !current->doors.level_meta||
       current->doors.level_meta_count<AVP_MAX_LEVEL)return 0;
    metas=current->doors.level_meta;
    memset(layouts,0,sizeof(AvpMpBotLevelLayout)*AVP_MAX_LEVEL);
    for(unsigned level=1u;level<=AVP_MAX_LEVEL;level++){
        const unsigned idx=level-1u;
        const AvpMpRetailMap *saved=&t->retail_maps.saved[t->species][idx];
        const AvpMpNativeWorld *loaded=NULL;
        const uint8_t *bytes;
        unsigned width,height;
        if(!t->bot_panels_ready[idx])return 0;
        for(unsigned j=0;j<AVP_LOCAL_MP_MAX_ACTORS;j++){
            const AvpMpActiveWorld *w=&t->session.worlds[j];
            if(w->context && w->level==level){
                loaded=(const AvpMpNativeWorld*)w->context;break;
            }
        }
        if(!loaded && t->persistent_ready[idx])
            loaded=&t->persistent_world[idx];
        if(loaded&&loaded->map){
            bytes=loaded->map->bytes;
            width=loaded->map->width;height=loaded->map->height;
        }else if(saved->bytes){
            bytes=saved->bytes;width=saved->width;height=saved->height;
        }else{
            bytes=t->retail_maps.original[idx];
            width=t->retail_maps.width[idx];
            height=t->retail_maps.height[idx];
        }
        if(!bytes||width==0u||height==0u||width>64u||height>64u)
            return 0;
        layouts[idx].map=(AvpMpBotMap){bytes+8u,width,height,
             loaded?loaded->doors.wall_flags:current->doors.wall_flags,
             &metas[idx],access_level,NULL,NULL,0u};
        layouts[idx].meta=&metas[idx];
        layouts[idx].panel_ids=t->bot_panel_ids[idx];
        memcpy(layouts[idx].panel_list,t->bot_panel_list[idx],
               AVP_MAX_PANELS);
    }
    return 1;
}
static int walk_bot_apply_level_hop(const AvpMpNativeWorld *world,
                                  const AvpMpBotMap *map,unsigned from_x,
                                  unsigned from_y,const AvpMpBotLevelPlan *hop,
                                  AvpMpBotObservation *view)
{
    AvpMpBotRoute route;
    if(!world||!world->map||!view||!hop ||
       hop->from_level!=world->map->level||
       !avp_mp_bot_route(map,from_x,from_y,hop->x,hop->y,&route))return 0;
    view->target_x=(hop->x<<16)|0x8000u;
    view->target_y=(hop->y<<16)|0x8000u;
    view->target_side=hop->side;
    view->goal=hop->kind==AVP_MP_BOT_TRANS_LIFT ?
               AVP_MP_BOT_LIFT:AVP_MP_BOT_EXIT;
    view->current_level=hop->from_level;
    view->destination_level=hop->to_level;
    view->exit_is_duct=hop->kind==AVP_MP_BOT_TRANS_DUCT;
    view->lift_direction=hop->kind==AVP_MP_BOT_TRANS_LIFT ?
                          (hop->to_level>hop->from_level?1:-1):0;
    view->has_target=1u;view->hostile_target=0u;
    return 1;
}
/* Authentic per-cell-connected level routing. Unlike a simple 15-level
 * graph, this distinguishes both sides of jammed storage doors. It may
 * choose level 3 -> airduct -> level 3 when the direct room route is blocked.
 * Each hop is still an ordinary original Use / lift input. */
static int walk_bot_choose_level_route(AvpMpWalkHost *t,unsigned player,
                                       const AvpMpNativeWorld *world,
                                       unsigned destination,unsigned species,
                                       unsigned from_x,unsigned from_y,
                                       const AvpMpBotMap *map,
                                       AvpMpBotObservation *view,
                                       unsigned gx,unsigned gy,int goal_known)
{
    AvpMpBotLevelLayout layouts[AVP_MAX_LEVEL];
    AvpMpBotLevelPlan hop={0};
    uint64_t tick=view->tick;
    if(!t||!world||!world->map||!map||!map->cells||
       player>=AVP_LOCAL_MP_MAX_ACTORS ||
       t->session.setup.custom_session_id==AVP_MP_CUSTOM_LEVEL ||
       (t->dm.enabled && !t->dm.level_travel))return 0;
    if(t->retail_catalog_ready){
        /* Cache only for a bounded eight original ticks. Revalidate that
         * the actual chosen portal remains reachable EVERY decision. */
        if(t->bot_level_cache[player].valid &&
           tick>=t->bot_level_cache[player].tick &&
           tick-t->bot_level_cache[player].tick<
               (world->map->level==4u?75u:35u) &&
           t->bot_level_cache[player].from_level==world->map->level &&
           t->bot_level_cache[player].goal_level==destination &&
           t->bot_level_cache[player].species==species &&
           t->bot_level_cache[player].goal_known==(unsigned)goal_known &&
           (!goal_known ||
            (t->bot_level_cache[player].gx==gx &&
             t->bot_level_cache[player].gy==gy))){
            hop=t->bot_level_cache[player].hop;
            if(walk_bot_apply_level_hop(world,map,from_x,from_y,&hop,view))
                return 1;
        }
        if(walk_bot_level_layouts(t,world,view->player_species==0u?
                              (unsigned)((const AvpMpActorNativeState *)
                                t->session.player[player])->access_level:10u,
                              layouts) &&
           avp_mp_bot_plan_levels(layouts,species,world->map->level,
               from_x,from_y,destination,gx,gy,goal_known,&hop) &&
           walk_bot_apply_level_hop(world,map,from_x,from_y,&hop,view)){
            t->bot_level_cache[player].hop=hop;
            t->bot_level_cache[player].tick=tick;
            t->bot_level_cache[player].from_level=world->map->level;
            t->bot_level_cache[player].goal_level=destination;
            t->bot_level_cache[player].species=species;
            t->bot_level_cache[player].gx=gx;
            t->bot_level_cache[player].gy=gy;
            t->bot_level_cache[player].goal_known=(unsigned)goal_known;
            t->bot_level_cache[player].valid=1u;
            return 1;
        }
        t->bot_level_cache[player].valid=0u;
        return 0; /* Do not fall back to topology-only fake shortcuts. */
    }
    /* Preserve synthetic no-ROM smoke setup (no authenticated maze graph). */
    {
        unsigned excluded=0u;
        if(!world->doors.level_meta||
           world->doors.level_meta_count<AVP_MAX_LEVEL)return 0;
        for(unsigned i=0;i<AVP_MAX_LEVEL;i++){
            unsigned next=walk_bot_next_level(world->doors.level_meta,
              world->doors.level_meta_count,world->map->level,destination,
              species,excluded);
            if(!next)break;
            if(walk_bot_choose_exit(world,next,species,from_x,from_y,map,view))
                return 1;
            excluded|=1u<<next;
        }
    }
    return 0;
}
/* Deathmatch bots otherwise run the SAME single-level patrol forever: the
 * campaign and co-op code are the only callers of the authored portal
 * graph. R125 supplies a goal when no known local target needs attention.
 * The goal is stable through intermediate ducts, and uses only original
 * DOORS.S exits/lifts, actual passage geometry and native Use commands. */
static int walk_bot_dm_travel(AvpMpWalkHost *t,unsigned p,
                              const AvpMpNativeWorld *world,unsigned species,
                              AvpMpBotObservation *v)
{
    AvpMpBotLevelLayout layouts[AVP_MAX_LEVEL];
    AvpMpBotLevelPlan chosen={0};
    unsigned level,from_x,from_y,destination=0u,ready=0u;
    uint64_t distance;
    /* Never change the "Players Can Move Between Levels" DM toggle or
     * offer a retail portal on a custom map without authored LEVDOOR. */
    if(!t || !t->dm.enabled || !t->dm.level_travel ||
       !t->retail_catalog_ready ||
       t->session.setup.custom_session_id==AVP_MP_CUSTOM_LEVEL ||
       !world || !world->map || !v || !v->map.cells ||
       !world->doors.level_meta ||
       world->doors.level_meta_count<AVP_MAX_LEVEL ||
       p>=AVP_LOCAL_MP_MAX_ACTORS)return 0;
    level=world->map->level;
    if(level<1u||level>AVP_MAX_LEVEL)return 0;
    from_x=v->x>>16;from_y=v->y>>16;
    if(from_x>=v->map.width || from_y>=v->map.height)return 0;
    {
        /* Alias only the one bot's policy state. The original actor and
         * world snapshots are never written by this helper. */
        AvpMpDmBotTravelState *r=&t->dm_bot_travel[p];
        if(r->level!=level){
            r->previous_level=r->level;
            r->level=level;r->entered_tick=v->tick;
            r->valid_hop=0u;r->avoid_valid=0u;r->retry_tick=0u;
            if(r->target_level==level)r->target_level=0u;
        }
        /* An observed enemy or badly needed health takes precedence over
         * roaming. A vent pickup is a short detour, not a permanent excuse
         * to circle inside an empty duct: after five seconds, seek a real
         * exit if available and keep the supply goal as fallback. */
        if(v->has_target &&
           (v->hostile_target || v->goal==AVP_MP_BOT_HEALTH ||
            v->goal==AVP_MP_BOT_COCOON ||
            !(level>=6u && level<=13u) ||
            v->tick<r->entered_tick+50u)){
            r->last_contact_tick=v->tick;
            return 0;
        }
        if(level>=6u&&level<=13u){
            /* No outside position is supplied. Even if a bot spawns in
             * a vent with no opponent, it can seek a genuine exit. */
            if(species==AVP_MP_PREDATOR)return 0; /* original duct rule */
        }else if(!r->target_level &&
                 (v->tick<r->entered_tick+120u ||
                  v->tick<r->last_contact_tick+60u))return 0;
        if(r->avoid_valid && v->tick>=r->last_progress_tick+180u)
            r->avoid_valid=0u; /* Retry a temporarily busy exit later. */
        if(v->tick<r->retry_tick)return 0;
        if(r->valid_hop && r->hop.from_level==level &&
           v->tick>=r->plan_tick && v->tick-r->plan_tick<(level==4u?100u:45u)){
            chosen=r->hop;ready=1u;
        }else{
            const unsigned access=species==AVP_MP_MARINE ?
                ((const AvpMpActorNativeState*)t->session.player[p])->access_level:10u;
            if(!walk_bot_level_layouts(t,world,access,layouts)){
                r->retry_tick=v->tick+30u;return 0;
            }
            destination=r->target_level;
            if(!destination && level>=6u&&level<=13u){
                ready=(unsigned)avp_mp_bot_plan_levels_avoiding(layouts,
                    species,level,from_x,from_y,0u,0u,0u,0,
                    r->avoid_valid?&r->avoid:NULL,&chosen);
                if(ready)r->target_level=chosen.goal_level;
            }else if(destination){
                ready=(unsigned)avp_mp_bot_plan_levels_avoiding(layouts,
                    species,level,from_x,from_y,destination,0u,0u,0,
                    r->avoid_valid?&r->avoid:NULL,&chosen);
            }else{
                /* If a surface becomes quiet, explore a different level;
                 * never use a roster's hidden coordinates to chase players.
                 * Each bot takes a different deterministic rotation. Prefer
                 * surface destinations to avoid aimless vent-to-vent loops. */
                const unsigned surface[7]={1u,2u,3u,4u,5u,14u,15u};
                const unsigned offset=(p*3u+(unsigned)(v->tick/160u))%7u;
                for(unsigned n=0;n<7u && !ready;n++){
                    unsigned to=surface[(n+offset)%7u];
                    if(to==level || to==r->previous_level)continue;
                    ready=(unsigned)avp_mp_bot_plan_levels_avoiding(layouts,
                        species,level,from_x,from_y,to,0u,0u,0,
                        r->avoid_valid?&r->avoid:NULL,&chosen);
                    if(ready)r->target_level=to;
                }
            }
            if(!ready){
                /* Clear an invalid target once; use local patrol until a
                 * fresh authored route appears, not a tight failed loop. */
                r->target_level=0u;r->valid_hop=0u;
                r->retry_tick=v->tick+45u;
                return 0;
            }
            if(!r->valid_hop || r->hop.from_level!=chosen.from_level ||
               r->hop.to_level!=chosen.to_level || r->hop.x!=chosen.x ||
               r->hop.y!=chosen.y || r->hop.side!=chosen.side){
                r->last_progress_tick=v->tick;
                r->best_distance=UINT64_MAX;
            }
            r->hop=chosen;r->valid_hop=1u;r->plan_tick=v->tick;
        }
        if(!walk_bot_apply_level_hop(world,&v->map,from_x,from_y,
                                     &chosen,v)){
            r->valid_hop=0u;r->retry_tick=v->tick+8u;
            return 0;
        }
        /* The chosen portal must get closer over time. If blocked by an
         * already active door or an inaccessible panel for long enough,
         * blacklist that FIRST portal and replan via another authored one.
         * Leave plenty of time for the original door animation to finish. */
        {
            const int64_t dx=(int64_t)v->x-((int64_t)chosen.x*65536+32768);
            const int64_t dy=(int64_t)v->y-((int64_t)chosen.y*65536+32768);
            distance=(uint64_t)(llabs(dx)+llabs(dy));
        }
        if(r->best_distance==UINT64_MAX || distance+8192u<r->best_distance){
            r->best_distance=distance;
            r->last_progress_tick=v->tick;
        }else if(v->tick>=r->last_progress_tick+100u){
            r->avoid=chosen;r->avoid_valid=1u;
            r->valid_hop=0u;
            r->retry_tick=v->tick+10u;
            r->last_progress_tick=v->tick;
            r->best_distance=UINT64_MAX;
        }
    }
    return 1;
}

/* Find a source-reachable tile outside the SMALL authored elevator chamber.
 * If it is a multi-cell room, don't just move the passenger to another
 * elevator tile and claim that the door has cleared. */
static int walk_bot_lift_disembark(const AvpMpNativeWorld *world,
                                  AvpMpBotObservation *v,unsigned lx,unsigned ly)
{
    uint32_t best=UINT32_MAX;
    unsigned goal_x=0u,goal_y=0u,found=0u;
    if(!world||!world->map||!v||!v->map.cells)return 0;
    for(int dy=-4;dy<=4;dy++)for(int dx=-4;dx<=4;dx++){
        int nx=(int)lx+dx,ny=(int)ly+dy;
        AvpMpBotRoute route={0};
        if(nx<0||ny<0||nx>=(int)v->map.width||ny>=(int)v->map.height ||
           (nx==(int)lx && ny==(int)ly) ||
           !avp_mp_bot_playable_position(v->map.cells,v->map.width,
               v->map.height,(uint32_t)nx<<16,(uint32_t)ny<<16))continue;
        if(avp_mp_chamber_contains(world->map,
               (lx<<16)|0x8000u,(ly<<16)|0x8000u,
               ((unsigned)nx<<16)|0x8000u,((unsigned)ny<<16)|0x8000u))
            continue;
        if(!avp_mp_bot_route(&v->map,v->x>>16,v->y>>16,
                             (unsigned)nx,(unsigned)ny,&route) ||
           route.cost>=best)continue;
        best=route.cost;goal_x=(unsigned)nx;goal_y=(unsigned)ny;found=1u;
    }
    if(!found && !avp_mp_bot_lift_egress(&v->map,v->x>>16,v->y>>16,
                                         &goal_x,&goal_y))return 0;
    v->goal=AVP_MP_BOT_PATROL;
    v->target_x=(goal_x<<16)|0x8000u;
    v->target_y=(goal_y<<16)|0x8000u;
    v->has_target=1u;v->hostile_target=0u;
    v->lift_wait=0u;
    return 1;
}

/* Co-op invitation uses only original authored lift panels and chamber
 * containment. Human players operate their own lift; a nearby bot boards
 * as a PASSENGER and never synthesizes the human's desired floor. The
 * caller retains all existing source DOORS.S restrictions. */
static unsigned walk_bot_lift_passenger_destination(unsigned level,
        unsigned x,unsigned y,unsigned species)
{
    for(int delta=1;delta>=-1;delta-=2){
        int next=(int)level+delta;
        if(next<1||next>5 ||
           !avp_mp_bot_transition_allowed(species,level,(unsigned)next,
                                          AVP_MP_BOT_TRANS_LIFT) ||
           avp_mp_bot_lift_jammed(level,x,y,delta))continue;
        return (unsigned)next;
    }
    return 0u;
}
static int walk_bot_lift_panel(const AvpMpNativeWorld *world,
                               unsigned x,unsigned y,unsigned *side)
{
    const AvpMpRetailMap *map=world?world->map:NULL;
    if(!map || x>=map->width||y>=map->height)
        return 0;
    const uint8_t *cell=map->bytes+8u+((size_t)y*map->width+x)*8u;
    unsigned authored=0u;
    for(unsigned s=0;s<4u;s++)if(cell[s]==64u)authored=1u; /* DOORS.S LIFTDOR */
    if(!authored)return 0;
    for(unsigned s=0;s<4u;s++){
        unsigned panel=cell[s]&0x7fu;
        if(panel && panel<=AVP_MAX_PANELS){
            unsigned file=world->doors.panel_list[panel-1u];
            if((world->doors.panels.lift_back!=0xffu &&
                file==world->doors.panels.lift_back) ||
               (world->doors.panels.lift_side!=0xffu &&
                file==world->doors.panels.lift_side)){
                *side=s;return 1;
            }
        }
    }
    return 0;
}
/* Same-world, read-only co-op lift rendezvous. Only live human occupants
 * already in an original closed elevator chamber invite bots. For a second
 * bot, an active bot lift claim can invite another bot to join that SAME
 * shaft, not create a second operator or destination. */
static void walk_bot_invite_lift(AvpMpWalkHost *t,unsigned p,
        const AvpMpNativeWorld *world,AvpMpBotObservation *v)
{
    unsigned chosen_x=0u,chosen_y=0u,chosen_side=0u,chosen_dest=0u;
    unsigned human=0u,found=0u;
    uint32_t best=UINT32_MAX;
    const AvpMpActiveWorld *here=avp_mp_session_player_world(&t->session,p);
    if(t->dm.enabled||!world||!world->map||!v||!v->map.cells||
       world->map->level<1u||world->map->level>5u||
       v->player_species==AVP_MP_ALIEN)return;
    for(unsigned y=0;y<world->map->height;y++)for(unsigned x=0;x<world->map->width;x++){
        unsigned side, destination=0u,is_human=0u;
        AvpMpBotRoute route={0};
        const uint64_t lx=((uint64_t)x<<16)|0x8000u;
        const uint64_t ly=((uint64_t)y<<16)|0x8000u;
        if(llabs((int64_t)v->x-(int64_t)lx)>(int64_t)(6u<<16) ||
           llabs((int64_t)v->y-(int64_t)ly)>(int64_t)(6u<<16) ||
           !walk_bot_lift_panel(world,x,y,&side))continue;
        /* Occupancy is evaluated from the actual native world and actor
         * positions, never from remembered contact coordinates. */
        for(unsigned q=0;q<t->players;q++)if(q!=p &&
            !t->session.setup.bot_enabled[q] &&
            avp_mp_session_player_world(&t->session,q)==here &&
            !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q)){
            const AvpMpActorNativeState *a=(const AvpMpActorNativeState*)t->session.player[q];
            if(a && a->player.player_energy>0 && !a->player.game_over &&
               avp_mp_chamber_contains(world->map,(uint32_t)lx,(uint32_t)ly,
                   a->player.x_pos,a->player.y_pos)){
                destination=walk_bot_lift_passenger_destination(
                    world->map->level,x,y,v->player_species);
                if(destination){is_human=1u;break;}
            }
        }
        if(!is_human){
            for(unsigned i=0;i<AVP_MP_BOT_LIFT_CLAIMS;i++){
                const AvpMpBotLiftClaim *c=&t->shared_lift_claims.entries[i];
                if(!c->active || c->level!=world->map->level||
                   c->x!=x||c->y!=y || c->leader==p ||
                   v->tick>=c->until ||
                   c->leader>=t->actors ||
                   !t->session.setup.bot_enabled[c->leader] ||
                   avp_mp_session_player_world(&t->session,c->leader)!=here ||
                   !t->lift_intent[c->leader].level ||
                   v->tick<t->lift_intent[c->leader].tick ||
                   v->tick-t->lift_intent[c->leader].tick>20u ||
                   !avp_mp_bot_transition_allowed(v->player_species,
                       world->map->level,c->destination,AVP_MP_BOT_TRANS_LIFT))
                    continue;
                destination=c->destination;break;
            }
        }
        {AvpMpBotObservation probe=*v;
         probe.current_level=world->map->level;
         if(!destination || !avp_mp_bot_lift_boarding_route(&probe,
                   x,y,destination,&route))continue;}
        /* Humans in the lift take precedence over other bot lift claims.
         * Source route cost picks the closest actually reachable shaft. */
        if(found && (is_human<human || (is_human==human && route.cost>=best)))
            continue;
        found=1u;human=is_human;best=route.cost;
        chosen_x=x;chosen_y=y;chosen_side=side;chosen_dest=destination;
    }
    if(found){
        v->goal=AVP_MP_BOT_LIFT;v->has_target=1u;v->hostile_target=0u;
        v->target_x=(chosen_x<<16)|0x8000u;
        v->target_y=(chosen_y<<16)|0x8000u;
        v->target_side=chosen_side;
        v->current_level=world->map->level;
        v->destination_level=chosen_dest;
        v->lift_direction=chosen_dest>world->map->level?1:-1;
        v->lift_follow_human=human;
        v->target_visible=0u;v->target_is_alien=0u;
    }
}
/* Temporarily ignore only peer BODY collision between co-op passengers
 * approaching the SAME enclosed elevator. This avoids bots forming a
 * blocking queue at a one-cell door. Walls, door geometry, all unrelated
 * player collisions, single player and deathmatch remain untouched. */
static int walk_bot_shared_lift_pair(const AvpMpWalkHost *t,unsigned p,unsigned q)
{
    if(!t||t->dm.enabled||p>=t->actors||q>=t->actors||p==q||
       (!t->session.setup.bot_enabled[p] && !t->session.setup.bot_enabled[q]))
        return 0;
    const AvpMpActiveWorld *here=avp_mp_session_player_world(&t->session,p);
    const AvpMpNativeWorld *world=here?(const AvpMpNativeWorld*)here->context:NULL;
    const AvpMpActorNativeState *a=(const AvpMpActorNativeState*)t->session.player[p];
    const AvpMpActorNativeState *b=(const AvpMpActorNativeState*)t->session.player[q];
    if(!world||!world->map||here!=avp_mp_session_player_world(&t->session,q)||
       here->level>5u||!a||!b)return 0;
    for(unsigned i=0;i<2u;i++){
        unsigned bot=i?q:p,other=i?p:q;
        unsigned x=t->lift_intent[bot].x,y=t->lift_intent[bot].y;
        if(!t->session.setup.bot_enabled[bot] ||
           t->lift_intent[bot].level!=here->level ||
           t->lift_board_tick<t->lift_intent[bot].tick ||
           t->lift_board_tick-t->lift_intent[bot].tick>2u)continue;
        const AvpMpActorNativeState *r=(const AvpMpActorNativeState*)t->session.player[other];
        if(t->session.setup.bot_enabled[other]){
            if(t->lift_intent[other].level!=here->level ||
               t->lift_intent[other].x!=x||t->lift_intent[other].y!=y ||
               t->lift_board_tick<t->lift_intent[other].tick ||
               t->lift_board_tick-t->lift_intent[other].tick>2u)continue;
        }else if(!avp_mp_chamber_contains(world->map,
               (x<<16)|0x8000u,(y<<16)|0x8000u,
               r->player.x_pos,r->player.y_pos))continue;
        if(llabs((int64_t)a->player.x_pos-(((int64_t)x<<16)|0x8000))>
                (int64_t)(2u<<16) ||
           llabs((int64_t)a->player.y_pos-(((int64_t)y<<16)|0x8000))>
                (int64_t)(2u<<16) ||
           llabs((int64_t)b->player.x_pos-(((int64_t)x<<16)|0x8000))>
                (int64_t)(2u<<16) ||
           llabs((int64_t)b->player.y_pos-(((int64_t)y<<16)|0x8000))>
                (int64_t)(2u<<16))continue;
        return 1;
    }
    return 0;
}

static void walk_bot_coordinate_lift(AvpMpWalkHost *t,unsigned p,
        const AvpMpNativeWorld *world,AvpMpBotObservation *v,uint64_t tick)
{
    unsigned x,y,sx,sy,leader,destination,unready=0u,waiting=0u;
    uint64_t created;
    if(!t||!world||!world->map||!v)return;
    if(t->last_bot_world_level[p] &&
       t->last_bot_world_level[p]!=world->map->level){
        t->lift_egress_valid[p]=0u;
    }
    if(t->last_bot_world_level[p] &&
       t->last_bot_world_level[p]!=world->map->level &&
       (t->last_bot_goal_lift[p] || t->exit_kind[p]==AVP_DOORS_EXIT_LIFT)){
        t->lift_arrival_pending[p]=1u;
        t->lift_arrival_x[p]=v->x>>16;
        t->lift_arrival_y[p]=v->y>>16;
    }
    if(t->lift_arrival_pending[p] &&
       (v->x>>16!=t->lift_arrival_x[p] ||
        v->y>>16!=t->lift_arrival_y[p]) &&
       !avp_mp_chamber_contains(world->map,
           (t->lift_arrival_x[p]<<16)|0x8000u,
           (t->lift_arrival_y[p]<<16)|0x8000u,v->x,v->y)){
        t->lift_arrival_pending[p]=0u;
        t->lift_egress_valid[p]=0u;
    }
    /* R288: every bot that ARRIVED on this floor must clear the chamber.
     * The former logic checked this only after selecting another lift goal,
     * then left the new lift owner inside forever while sending followers
     * out. It also skipped egress when supply/health/combat selected PATROL.
     * Do not choose another floor while still in the arrival chamber. */
    if(avp_mp_bot_lift_needs_disembark(t->lift_arrival_pending[p],
       avp_mp_chamber_contains(world->map,
           (t->lift_arrival_x[p]<<16)|0x8000u,
           (t->lift_arrival_y[p]<<16)|0x8000u,v->x,v->y)) &&
       !(v->hostile_target && v->target_visible) &&
       !(v->melee_visible && v->melee_threat!=AVP_MP_BOT_THREAT_NONE)){
        unsigned ex=t->lift_egress_x[p],ey=t->lift_egress_y[p];
        /* Continue to the same exit for as many ticks as are necessary;
         * replanning to a new exit inside the chamber causes oscillation. */
        if(t->lift_egress_valid[p] &&
           t->lift_egress_level[p]==world->map->level){
            v->goal=AVP_MP_BOT_PATROL;
            v->target_x=(ex<<16)|0x8000u;
            v->target_y=(ey<<16)|0x8000u;
            v->has_target=1u;v->hostile_target=0u;
        }else if(walk_bot_lift_disembark(world,v,
                  t->lift_arrival_x[p],t->lift_arrival_y[p])){
            t->lift_egress_valid[p]=1u;
            t->lift_egress_level[p]=world->map->level;
            t->lift_egress_x[p]=v->target_x>>16;
            t->lift_egress_y[p]=v->target_y>>16;
        }else{
            /* Never operate the lift again with an unresolved arrival;
             * a truly blocked exit must be solved by authentic doors or
             * another route, not by bouncing between two floors. */
            v->goal=AVP_MP_BOT_PATROL;
            v->has_target=0u;v->hostile_target=0u;
        }
        t->lift_intent[p].level=0u;
        for(unsigned i=0u;i<AVP_MP_BOT_LIFT_CLAIMS;i++)
            if(t->shared_lift_claims.entries[i].active &&
               t->shared_lift_claims.entries[i].leader==p)
                t->shared_lift_claims.entries[i].active=0u;
        return;
    }
    /* DM opponents are not passengers in a shared planned trip. Keep
     * native chamber egress, but never inherit an opponent's destination. */
    if(t->dm.enabled){
        t->lift_intent[p].level=0u;
        v->lift_wait=0u;v->lift_follow_human=0u;
        return;
    }
    /* A bot already boarded this genuine chamber must not abandon its
     * agreed lift direction just because the scanner or supply planner
     * produced another routine waypoint on the next original game tick. */
    if((v->goal!=AVP_MP_BOT_LIFT || !v->has_target ||
        (v->target_x>>16)!=t->lift_intent[p].x ||
        (v->target_y>>16)!=t->lift_intent[p].y ||
        v->destination_level!=t->lift_intent[p].destination) &&
       avp_mp_bot_lift_keep_trip(v,t->lift_intent[p].level,
           world->map->level,t->lift_intent[p].tick,
           avp_mp_chamber_contains(world->map,
               (t->lift_intent[p].x<<16)|0x8000u,
               (t->lift_intent[p].y<<16)|0x8000u,v->x,v->y))){
        unsigned side=0u;
        const unsigned lx=t->lift_intent[p].x,ly=t->lift_intent[p].y;
        const unsigned dst=t->lift_intent[p].destination;
        if(walk_bot_lift_panel(world,lx,ly,&side) &&
           side==t->lift_intent[p].side &&
           avp_mp_bot_transition_allowed(v->player_species,
              world->map->level,dst,AVP_MP_BOT_TRANS_LIFT) &&
           !avp_mp_bot_lift_jammed(world->map->level,lx,ly,
               dst>world->map->level?1:-1)){
            v->goal=AVP_MP_BOT_LIFT;v->has_target=1u;
            v->target_x=(lx<<16)|0x8000u;
            v->target_y=(ly<<16)|0x8000u;
            v->target_side=side;v->hostile_target=0u;
            v->destination_level=dst;
            v->lift_direction=dst>world->map->level?1:-1;
            v->lift_follow_human=t->lift_intent[p].follow_human;
        }
    }
    if(v->goal!=AVP_MP_BOT_LIFT || !v->has_target){
        /* The former owner changed tasks, died, or got distracted by a
         * real enemy. Other passengers may elect a new owner next tick. */
        t->lift_intent[p].level=0u;
        for(unsigned i=0u;i<AVP_MP_BOT_LIFT_CLAIMS;i++)
            if(t->shared_lift_claims.entries[i].active &&
               t->shared_lift_claims.entries[i].leader==p)
                t->shared_lift_claims.entries[i].active=0u;
        return;
    }
    x=v->target_x>>16;y=v->target_y>>16;
    sx=v->x>>16;sy=v->y>>16;
    /* Even bots six tiles away have declared their common boarding intent:
     * the owner can see them approaching rather than activating first. */
    if((sx>x?sx-x:x-sx)>6u || (sy>y?sy-y:y-sy)>6u){
        t->lift_intent[p].level=0u;
        return;
    }
    t->lift_intent[p].level=world->map->level;
    t->lift_intent[p].x=x;t->lift_intent[p].y=y;
    t->lift_intent[p].destination=v->destination_level;
    t->lift_intent[p].side=v->target_side;
    t->lift_intent[p].follow_human=v->lift_follow_human;
    t->lift_intent[p].tick=tick;
    if(v->lift_follow_human){
        /* A human passenger may have left before operating the lift. Do not
         * preserve a phantom human operator forever: if nobody human is
         * still in the chamber, elect one bot through the normal claims. */
        unsigned human_inside=0u;
        const AvpMpActiveWorld *here=avp_mp_session_player_world(&t->session,p);
        for(unsigned q=0u;q<t->players;q++)if(q!=p &&
            !t->session.setup.bot_enabled[q] &&
            avp_mp_session_player_world(&t->session,q)==here &&
            !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q)){
            const AvpMpActorNativeState *a=
                (const AvpMpActorNativeState *)t->session.player[q];
            if(a && a->player.player_energy>0 && !a->player.game_over &&
               avp_mp_chamber_contains(world->map,
                   (x<<16)|0x8000u,(y<<16)|0x8000u,
                   a->player.x_pos,a->player.y_pos)){
                human_inside=1u;break;
            }
        }
        if(!human_inside){
            v->lift_follow_human=0u;
            t->lift_intent[p].follow_human=0u;
        }
    }
    if(v->lift_follow_human){
        /* A human already in this chamber operates the source lift. Bots
         * only board; no additional conflicting keypad Use commands. */
        for(unsigned i=0u;i<AVP_MP_BOT_LIFT_CLAIMS;i++){
            AvpMpBotLiftClaim *c=&t->shared_lift_claims.entries[i];
            if(c->active && c->level==world->map->level &&
               c->x==x && c->y==y)c->active=0u;
        }
        v->lift_wait=1u;
        return;
    }
    /* An unrelated bot across the maze does not hold this lift hostage.
     * Once close enough to board, agree on one destination and one owner. */
    if((sx>x?sx-x:x-sx)>2u || (sy>y?sy-y:y-sy)>2u)return;
    /* Don't retain an owner whose last approach was abandoned. Other
     * bots should not wait 120 ticks for a dead/disconnected player. */
    for(unsigned i=0u;i<AVP_MP_BOT_LIFT_CLAIMS;i++){
        AvpMpBotLiftClaim *c=&t->shared_lift_claims.entries[i];
        if(c->active && c->level==world->map->level &&
           c->x==x && c->y==y && c->leader!=p){
            const unsigned owner=c->leader;
            const AvpMpActorNativeState *a=owner<t->actors?
                (const AvpMpActorNativeState*)t->session.player[owner]:NULL;
            const AvpMpActiveWorld *where=owner<t->actors?
                avp_mp_session_player_world(&t->session,owner):NULL;
            if(!a || !where || where->level!=world->map->level ||
               a->player.player_energy<=0 || a->player.game_over ||
               (tick>t->lift_intent[owner].tick+20u))c->active=0u;
        }
    }
    if(!avp_mp_bot_lift_agree(&t->shared_lift_claims,
        world->map->level,x,y,p,v->destination_level,tick,
        &leader,&destination,&created))return;
    v->destination_level=destination;
    v->lift_direction=destination>world->map->level?1:-1;
    v->lift_wait=leader!=p || tick<created+3u;
    t->lift_intent[p].level=world->map->level;
    t->lift_intent[p].x=x;t->lift_intent[p].y=y;
    t->lift_intent[p].tick=tick;
    for(unsigned q=0;q<t->actors;q++)if(q!=p &&
       t->session.setup.bot_enabled[q] &&
       avp_mp_session_player_world(&t->session,q) &&
       avp_mp_session_player_world(&t->session,q)->level==world->map->level){
        const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[q];
        unsigned qx,qy;
        if(!a||a->player.player_energy<=0||a->player.game_over)continue;
        qx=a->player.x_pos>>16;qy=a->player.y_pos>>16;
        if((qx==x && qy==y) ||
           (((qx>x?qx-x:x-qx)<=4u && (qy>y?qy-y:y-qy)<=4u) &&
            avp_mp_chamber_contains(world->map,
              (x<<16)|0x8000u,(y<<16)|0x8000u,
              a->player.x_pos,a->player.y_pos))){
            /* A passenger who still touches a door margin can reverse
             * the original closing animation. Wait for actual clearance. */
            if(llabs((int64_t)(a->player.x_pos&0xffffu)-32768)>7500 ||
               llabs((int64_t)(a->player.y_pos&0xffffu)-32768)>7500)
                unready++;
        }else if(t->lift_intent[q].level==world->map->level &&
                 t->lift_intent[q].x==x && t->lift_intent[q].y==y &&
                 tick>=t->lift_intent[q].tick &&
                 tick-t->lift_intent[q].tick<20u &&
                 (qx>x?qx-x:x-qx)<=6u && (qy>y?qy-y:y-qy)<=6u)
            waiting++;
    }
    if(leader==p && (unready || (tick<created+160u && waiting)))
        v->lift_wait=1u;
}

/* R296: original Predator/Alien ship airlocks contain two four-place
 * recessed side bays. Coordinate ordinary bot movement inputs ONLY: the
 * original DOORS.S door bytes, interlock and airlock transfer remain final
 * authority. Never synthesize an exit for a non-retail or unknown map. */
static int walk_bot_airlock_interrupt(const AvpMpBotObservation *v)
{
    if(!v || v->deathmatch)return 1;
    if(v->facehugged || v->other_player_alien_trapped ||
       v->other_player_swarmed || v->melee_visible ||
       (v->hostile_target && v->target_visible) ||
       v->retreat_to_teammate || v->retreat_to_refuge ||
       (v->health_low && (v->goal==AVP_MP_BOT_HEALTH ||
                          v->goal==AVP_MP_BOT_MEDICAL)) ||
       (v->marine_low_ammo && v->goal==AVP_MP_BOT_SUPPLY) ||
       v->goal==AVP_MP_BOT_LIFT || v->goal==AVP_MP_BOT_COCOON ||
       v->goal==AVP_MP_BOT_QUEEN_WAIT)return 1;
    return 0;
}
static int walk_bot_airlock_is_member(const AvpMpWalkHost *t,unsigned q,
    const AvpMpActiveWorld *here,const AvpMpAirlockRoom *room)
{
    const AvpMpActorNativeState *a;
    if(q>=t->actors || !t->session.setup.bot_enabled[q] ||
       avp_mp_session_player_world(&t->session,q)!=here ||
       avp_mp_life_busy(&t->player_life[q]) ||
       !walk_vent_world_present(t,q))return 0;
    a=(const AvpMpActorNativeState*)t->session.player[q];
    return a && a->player.player_energy>0 && !a->player.game_over &&
        t->bots[q].airlock_level==here->level &&
        t->bots[q].airlock_panel_offset==
            (room->exit_y*64u+room->exit_x)*8u+room->exit_side;
}
/* Predict assigned recesses in stable bot-index order, respecting any
 * human already standing in a recess. Beyond available seats, later bots
 * wait OUTSIDE for the next airlock cycle instead of blocking both doors. */
static unsigned walk_bot_airlock_seat(const AvpMpWalkHost *t,unsigned p,
     const AvpMpActiveWorld *here,const AvpMpNativeWorld *world,
     const AvpMpAirlockRoom *room)
{
    uint8_t claimed[AVP_MP_AIRLOCK_SEATS]={0};
    unsigned chosen=AVP_MP_AIRLOCK_SEATS;
    for(unsigned q=0;q<t->actors;q++)if(q!=p &&
       !t->session.setup.bot_enabled[q] &&
       avp_mp_session_player_world(&t->session,q)==here &&
       !avp_mp_life_busy(&t->player_life[q])){
        const AvpMpActorNativeState *a=
            (const AvpMpActorNativeState*)t->session.player[q];
        if(!a||a->player.player_energy<=0||a->player.game_over)continue;
        for(unsigned i=0;i<room->seat_count;i++){
            const int64_t dx=(int64_t)a->player.x_pos-
                ((int64_t)room->seats[i].x*65536+32768);
            const int64_t dy=(int64_t)a->player.y_pos-
                ((int64_t)room->seats[i].y*65536+32768);
            if(llabs(dx)<20000 && llabs(dy)<20000)claimed[i]=1u;
        }
    }
    for(unsigned q=0;q<t->actors;q++)if(
       walk_bot_airlock_is_member(t,q,here,room)){
        unsigned i=0u;
        while(i<room->seat_count && claimed[i])i++;
        if(q==p)chosen=i;
        if(i<room->seat_count)claimed[i]=1u;
    }
    (void)world;
    return chosen;
}
static void walk_bot_airlock_target(AvpMpBotObservation *v,unsigned goal,
                                   unsigned x,unsigned y,unsigned side)
{
    v->goal=(enum AvpMpBotGoal)goal;
    v->has_target=1u;v->hostile_target=0u;v->target_visible=0u;
    v->tracker_only_target=0u;v->target_is_alien=0u;
    v->target_x=(x<<16)|0x8000u;
    v->target_y=(y<<16)|0x8000u;
    v->target_side=side;
    v->airlock_face_side=side;
    v->airlock_pressure=0u;
}
static void walk_bot_coordinate_airlock(AvpMpWalkHost *t,unsigned p,
     const AvpMpNativeWorld *world,AvpMpBotObservation *v)
{
    const AvpMpActiveWorld *here;
    const AvpDoorLevelMeta *meta;
    AvpMpBotState *memory;
    AvpMpAirlockRoom room={0};
    unsigned exit_offset=0u,chosen=AVP_MP_AIRLOCK_SEATS;
    unsigned from_goal=0u,human_inside=0u,operator=t->actors;
    uint64_t best=UINT64_MAX;
    int inside,waiting=0;
    if(!t||!world||!world->map||!v || !v->map.cells ||
       (t->dm.enabled && !t->dm.level_travel) || !t->retail_catalog_ready ||
       t->session.setup.custom_session_id==AVP_MP_CUSTOM_LEVEL ||
       world->map->level>AVP_MAX_LEVEL || p>=t->actors ||
       !world->doors.level_meta ||
       world->doors.level_meta_count<AVP_MAX_LEVEL)return;
    /* Do not override a just-finished elevator trip while that bot is
     * still clearing the original lift doorway. */
    if(t->lift_arrival_pending[p])return;
    here=avp_mp_session_player_world(&t->session,p);
    meta=&world->doors.level_meta[world->map->level-1u];
    memory=&t->bots[p];
    /* Deathmatch never recruits opponents or waits for co-op seats. A
     * bot pursuing its own authored ship exit operates both door bytes
     * from inside, with the same original animation/pressure interlock. */
    if(t->dm.enabled){
        if(v->facehugged || (v->hostile_target && v->target_visible) ||
           !v->has_target || v->goal!=AVP_MP_BOT_EXIT || v->exit_is_duct)return;
        for(size_t i=0;i<meta->exit_count;i++){
            const AvpDoorExitEntry *e=&meta->exits[i];
            if(e->level!=v->destination_level ||
               e->panel_offset/8u%world->map->width!=(v->target_x>>16) ||
               e->panel_offset/8u/world->map->width!=(v->target_y>>16) ||
               e->panel_offset%8u!=v->target_side ||
               !avp_mp_airlock_room(world->map,meta,e->panel_offset,&room))continue;
            inside=avp_mp_chamber_contains(world->map,
                (room.exit_x<<16)|0x8000u,(room.exit_y<<16)|0x8000u,v->x,v->y);
            if(avp_mp_airlock_cycle_reverse_block(&memory->airlock_cycle,
                   world->map->level,room.destination,inside,0,0,v->tick)){
                const unsigned outside_y=room.entry_side==3u?room.entry_y+1u:room.entry_y-1u;
                walk_bot_airlock_target(v,AVP_MP_BOT_PATROL,room.entry_x,outside_y,room.entry_side);
                v->destination_level=0u;return;
            }
            if(!inside)return; /* native route takes bot into chamber */
            if(t->exit_kind[p]==AVP_DOORS_EXIT_AIRLOCK){
                AvpMpTransitionRequest pending;
                if(avp_mp_transition_peek(&t->transitions,p,&pending)){
                    walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_SEAT,
                        v->x>>16,v->y>>16,room.entry_side);return;
                }
            }
            switch(avp_mp_airlock_action(&room,world->map->bytes+8u,
                   (size_t)world->map->width*world->map->height*8u,
                   v->active_door_offsets,v->active_door_count)){
            case AVP_MP_AIRLOCK_CLOSE_ENTRY:
                walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_CLOSE,
                    room.entry_x,room.entry_y,room.entry_side);
                v->airlock_door_ready=1u;return;
            case AVP_MP_AIRLOCK_CLOSE_EXIT:
                walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_CLOSE,
                    room.exit_x,room.exit_y,room.exit_side);
                v->airlock_door_ready=1u;return;
            case AVP_MP_AIRLOCK_PRESSURE:
                v->airlock_pressure=1u;return;
            default:
                walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_SEAT,
                    v->x>>16,v->y>>16,room.entry_side);return;
            }
        }
        return;
    }
    if(walk_bot_airlock_interrupt(v))return;
    /* An existing, bounded boarding commitment survives routine follow,
     * patrol and source scanner replanning. Real danger still interrupts. */
    if(memory->airlock_level==world->map->level &&
       memory->airlock_panel_offset &&
       avp_mp_airlock_room(world->map,meta,memory->airlock_panel_offset,&room)){
        const int matching=v->goal==AVP_MP_BOT_EXIT && v->has_target &&
            !v->exit_is_duct && v->destination_level==room.destination &&
            (v->target_x>>16)==room.exit_x && (v->target_y>>16)==room.exit_y &&
            v->target_side==room.exit_side;
        const int in_room=avp_mp_chamber_contains(world->map,
            (room.exit_x<<16)|0x8000u,(room.exit_y<<16)|0x8000u,v->x,v->y);
        if(avp_mp_airlock_commitment_valid(v->tick,memory->airlock_last_intent,
                                         in_room,matching)){
            exit_offset=memory->airlock_panel_offset;
            from_goal=matching;
        }
    }
    /* Real exit goal (same level panel and ORIGINAL destination). */
    if(!exit_offset && v->goal==AVP_MP_BOT_EXIT && v->has_target &&
       !v->exit_is_duct && v->destination_level){
        for(size_t i=0;i<meta->exit_count;i++){
            const AvpDoorExitEntry *e=&meta->exits[i];
            if(e->level!=v->destination_level ||
               e->panel_offset/8u%world->map->width!=v->target_x>>16 ||
               e->panel_offset/8u/world->map->width!=v->target_y>>16 ||
               e->panel_offset%8u!=v->target_side)continue;
            if(avp_mp_airlock_room(world->map,meta,e->panel_offset,&room)){
                exit_offset=e->panel_offset;from_goal=1u;break;
            }
        }
    }
    /* An actual human inside a bounded chamber invites nearby bots,
     * even when that human hasn't activated the pressurization panel yet. */
    if(!exit_offset)for(size_t i=0;i<meta->exit_count;i++){
        const AvpDoorExitEntry *e=&meta->exits[i];
        AvpMpAirlockRoom candidate;
        AvpMpBotRoute route={0};
        uint64_t dist;
        int occupied=0;
        if(!avp_mp_airlock_room(world->map,meta,e->panel_offset,&candidate))continue;
        dist=walk_bot_distance(v->x,v->y,
            (candidate.exit_x<<16)|0x8000u,
            (candidate.exit_y<<16)|0x8000u);
        if(dist>(((uint64_t)8u<<16)*((uint64_t)8u<<16)) || dist>=best ||
           !avp_mp_bot_route(&v->map,v->x>>16,v->y>>16,
                    candidate.exit_x,candidate.exit_y,&route))continue;
        for(unsigned q=0;q<t->actors;q++)if(q!=p &&
           !t->session.setup.bot_enabled[q] &&
           avp_mp_session_player_world(&t->session,q)==here &&
           !avp_mp_life_busy(&t->player_life[q]) &&
           walk_vent_world_present(t,q)){
            const AvpMpActorNativeState *a=(const AvpMpActorNativeState*)
                                       t->session.player[q];
            if(a && a->player.player_energy>0 && !a->player.game_over &&
               avp_mp_chamber_contains(world->map,
                  (candidate.exit_x<<16)|0x8000u,
                  (candidate.exit_y<<16)|0x8000u,
                  a->player.x_pos,a->player.y_pos)){
                occupied=1;break;
            }
        }
        if(!occupied)for(unsigned q=0;q<t->actors;q++)if(q!=p &&
            walk_bot_airlock_is_member(t,q,here,&candidate)){
            const AvpMpActorNativeState *a=
                (const AvpMpActorNativeState*)t->session.player[q];
            if(a && avp_mp_chamber_contains(world->map,
                  (candidate.exit_x<<16)|0x8000u,
                  (candidate.exit_y<<16)|0x8000u,
                  a->player.x_pos,a->player.y_pos)){
                occupied=1;break;
            }
        }
        if(occupied){room=candidate;exit_offset=e->panel_offset;best=dist;}
    }
    if(!exit_offset){
        if(memory->airlock_level &&
           memory->airlock_level!=world->map->level)
            memory->airlock_level=memory->airlock_panel_offset=0u;
        return;
    }
    /* R299: After an ORIGINAL 1<->15 or 5<->14 ship transfer, the
     * destination airlock is an ARRIVAL, not a fresh departure request.
     * R296's physical-human-in-chamber invitation otherwise immediately
     * reverses the pressure cycle when the party arrives together. The
     * ordinary level planner can also choose the old level on its next
     * FOLLOW tick while a teammate lags behind. Both are suppressed until
     * the bot and a human have established themselves OUTSIDE this arrival
     * chamber. Native Jaguar doors, pressure effects and travel stay intact.
     */
    if(memory->airlock_cycle.arrived_level==world->map->level &&
       memory->airlock_cycle.arrived_from==room.destination){
        int bot_inside=avp_mp_chamber_contains(world->map,
            (room.exit_x<<16)|0x8000u,(room.exit_y<<16)|0x8000u,
            v->x,v->y);
        int any_human=0,disembarked=0;
        const AvpMpActorNativeState *outside_human=NULL;
        uint64_t outside_best=UINT64_MAX;
        for(unsigned q=0;q<t->actors;q++)if(q!=p &&
           !t->session.setup.bot_enabled[q] &&
           !avp_mp_life_busy(&t->player_life[q]) &&
           walk_vent_world_present(t,q)){
            const AvpMpActorNativeState *a=
                (const AvpMpActorNativeState*)t->session.player[q];
            const AvpMpActiveWorld *qworld=
                avp_mp_session_player_world(&t->session,q);
            uint64_t dist;
            if(!a||!qworld||a->player.player_energy<=0||
               a->player.game_over)continue;
            /* Only humans in this world can disembark from this chamber.
             * A bot arriving alone must be able to leave it. */
            if(qworld!=here)continue;
            any_human=1;
            if(avp_mp_chamber_contains(world->map,
                  (room.exit_x<<16)|0x8000u,(room.exit_y<<16)|0x8000u,
                  a->player.x_pos,a->player.y_pos))continue;
            disembarked=1;
            dist=walk_bot_distance(v->x,v->y,
                                  a->player.x_pos,a->player.y_pos);
            if(dist<outside_best){outside_best=dist;outside_human=a;}
        }
        if(avp_mp_airlock_cycle_reverse_block(&memory->airlock_cycle,
             world->map->level,room.destination,bot_inside,
             disembarked,any_human,v->tick)){
            if(outside_human){
                /* Walk OUT toward the teammate, not toward the return
                 * pressure panel. This also permits the rearm condition
                 * to become true without inventing a bot teleport. */
                v->goal=AVP_MP_BOT_FOLLOW;
                v->has_target=1u;v->hostile_target=0u;
                v->target_visible=0u;v->tracker_only_target=0u;
                v->target_x=outside_human->player.x_pos;
                v->target_y=outside_human->player.y_pos;
            }else if(bot_inside && !any_human){
                /* Solo arrival: walk through the normal entrance into the
                 * destination level. Never press the reverse pressure panel. */
                const unsigned outside_y=room.entry_side==3u?room.entry_y+1u:room.entry_y-1u;
                walk_bot_airlock_target(v,AVP_MP_BOT_PATROL,room.entry_x,outside_y,room.entry_side);
                v->destination_level=0u;
            }else if(bot_inside && room.seat_count){
                /* Hold at an existing recess instead of pressing again.
                 * Choose a nearby original seat and face inward. */
                uint64_t nearest=UINT64_MAX;
                unsigned nearest_seat=0u;
                for(unsigned i=0;i<room.seat_count;i++){
                    uint64_t d=walk_bot_distance(v->x,v->y,
                        (room.seats[i].x<<16)|0x8000u,
                        (room.seats[i].y<<16)|0x8000u);
                    if(d<nearest){nearest=d;nearest_seat=i;}
                }
                walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_SEAT,
                    room.seats[nearest_seat].x,room.seats[nearest_seat].y,
                    room.seats[nearest_seat].face_side);
            }else{
                /* Teammates still on the departure side have not issued
                 * another valid crossing just because FOLLOW replanned.
                 * Wait here without producing another panel Use. */
                v->goal=AVP_MP_BOT_PATROL;
                v->has_target=1u;v->hostile_target=0u;
                v->target_visible=0u;v->tracker_only_target=0u;
                v->target_x=v->x;v->target_y=v->y;
            }
            return;
        }
    }
    /* An acknowledged original pressure request is already underway.
     * Re-pressing the same panel before group commit can replace the native
     * effect and does not create a second legitimate transfer. */
    if(t->exit_kind[p]==AVP_DOORS_EXIT_AIRLOCK){
        AvpMpTransitionRequest pending;
        if(avp_mp_transition_peek(&t->transitions,p,&pending) &&
           (unsigned)pending.target_level==room.destination &&
           room.seat_count){
            const unsigned seat=p%room.seat_count;
            walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_SEAT,
                room.seats[seat].x,room.seats[seat].y,
                room.seats[seat].face_side);
            return;
        }
    }
    if(memory->airlock_level!=world->map->level ||
       memory->airlock_panel_offset!=exit_offset){
        memory->airlock_level=world->map->level;
        memory->airlock_panel_offset=exit_offset;
        memory->airlock_destination=room.destination;
        memory->airlock_since=v->tick;
    }
    if(from_goal || memory->airlock_last_intent==0u)
        memory->airlock_last_intent=v->tick;
    inside=avp_mp_chamber_contains(world->map,
        (room.exit_x<<16)|0x8000u,(room.exit_y<<16)|0x8000u,
        v->x,v->y);
    /* A chamber invitation persists while team members are truly present,
     * or while a real level-transition goal is still requested. */
    for(unsigned q=0;q<t->actors;q++)if(q!=p &&
       !t->session.setup.bot_enabled[q] &&
       avp_mp_session_player_world(&t->session,q)==here &&
       !avp_mp_life_busy(&t->player_life[q])){
        const AvpMpActorNativeState *a=(const AvpMpActorNativeState*)
                                    t->session.player[q];
        if(a && a->player.player_energy>0 && !a->player.game_over &&
           avp_mp_chamber_contains(world->map,
              (room.exit_x<<16)|0x8000u,
              (room.exit_y<<16)|0x8000u,
              a->player.x_pos,a->player.y_pos)){
            human_inside=1u;break;
        }
    }
    if(human_inside || inside)memory->airlock_last_intent=v->tick;
    if(!human_inside && !inside && !from_goal &&
       v->tick>memory->airlock_last_intent+90u){
        memory->airlock_level=memory->airlock_panel_offset=0u;
        return;
    }
    /* Choose only the lowest-index bot ALREADY inside the airlock as
     * temporary operator. Humans may use the original panel during a
     * grace period; the operator then assists if nobody presses it. */
    for(unsigned q=0;q<t->actors;q++)if(
        walk_bot_airlock_is_member(t,q,here,&room)){
        const AvpMpActorNativeState *a=(const AvpMpActorNativeState*)
                                           t->session.player[q];
        if(a && avp_mp_chamber_contains(world->map,
            (room.exit_x<<16)|0x8000u,(room.exit_y<<16)|0x8000u,
            a->player.x_pos,a->player.y_pos)){
            operator=q;break;
        }
    }
    chosen=walk_bot_airlock_seat(t,p,here,world,&room);
    v->current_level=world->map->level;
    v->destination_level=room.destination;
    v->exit_is_duct=0u;
    if(chosen>=room.seat_count){
        /* Too many passengers for the ACTUAL authored recesses. No
         * overlapping fictional standing spaces or blocker in doorway. */
        const unsigned outside_y=room.entry_side==3u?
               room.entry_y+2u:room.entry_y-2u;
        walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_SEAT,
            room.entry_x,outside_y,room.entry_side==3u?1u:3u);
        return;
    }
    if(!inside || operator!=p){
        walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_SEAT,
            room.seats[chosen].x,room.seats[chosen].y,
            room.seats[chosen].face_side);
        return;
    }
    /* Give other registered, physically nearby passengers time to board
     * and settle into their unique recesses. Never wait for an unrelated
     * bot across the level; a bounded deadline prevents deadlock. */
    if(v->tick<memory->airlock_since+160u){
        for(unsigned q=0;q<t->actors;q++)if(q!=p &&
           walk_bot_airlock_is_member(t,q,here,&room)){
            const AvpMpActorNativeState *a=(const AvpMpActorNativeState*)
                                           t->session.player[q];
            if(a && walk_bot_distance(v->x,v->y,
                     a->player.x_pos,a->player.y_pos)<
                 ((uint64_t)8u<<16)*((uint64_t)8u<<16)){
                if(!avp_mp_chamber_contains(world->map,
                        (room.exit_x<<16)|0x8000u,
                        (room.exit_y<<16)|0x8000u,
                        a->player.x_pos,a->player.y_pos))waiting++;
            }
        }
    }
    /* A human in the chamber may use the controls themselves. After a
     * short grace period, a seated bot can operate for the whole group;
     * never shut the door against a human still in the doorway. */
    if(human_inside){
        for(unsigned q=0;q<t->actors;q++)if(q!=p &&
           !t->session.setup.bot_enabled[q] &&
           avp_mp_session_player_world(&t->session,q)==here &&
           !avp_mp_life_busy(&t->player_life[q])){
            const AvpMpActorNativeState *a=
                (const AvpMpActorNativeState*)t->session.player[q];
            if(!a || a->player.player_energy<=0 || a->player.game_over ||
               !avp_mp_chamber_contains(world->map,
                    (room.exit_x<<16)|0x8000u,
                    (room.exit_y<<16)|0x8000u,
                    a->player.x_pos,a->player.y_pos))continue;
            if((a->player.x_pos>>16)==room.entry_x &&
                (a->player.y_pos>>16)==room.entry_y){
                const unsigned frac=a->player.y_pos&0xffffu;
                if((room.entry_side==3u && frac>0xb800u) ||
                   (room.entry_side==1u && frac<0x4800u))waiting++;
            }
            if((a->player.x_pos>>16)==room.exit_x &&
                (a->player.y_pos>>16)==room.exit_y){
                const unsigned frac=a->player.y_pos&0xffffu;
                if((room.exit_side==3u && frac>0xb800u) ||
                   (room.exit_side==1u && frac<0x4800u))waiting++;
            }
        }
    }
    if(waiting || (human_inside && v->tick<memory->airlock_since+80u)){
        walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_SEAT,
            room.seats[chosen].x,room.seats[chosen].y,
            room.seats[chosen].face_side);
        return;
    }
    /* Keep the native pressure request behind BOTH stationary closed bytes.
     * While either door is moving, stay in the chamber and wait; never use
     * the pressure panel on the first zero-byte opening tick. */
    switch(avp_mp_airlock_action(&room,world->map->bytes+8u,
            (size_t)world->map->width*world->map->height*8u,
            v->active_door_offsets,v->active_door_count)){
    case AVP_MP_AIRLOCK_CLOSE_ENTRY:
        walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_CLOSE,
            room.entry_x,room.entry_y,room.entry_side);
        v->airlock_door_ready=1u;
        return;
    case AVP_MP_AIRLOCK_CLOSE_EXIT:
        walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_CLOSE,
            room.exit_x,room.exit_y,room.exit_side);
        v->airlock_door_ready=1u;
        return;
    case AVP_MP_AIRLOCK_PRESSURE:
        walk_bot_airlock_target(v,AVP_MP_BOT_EXIT,
            room.exit_x,room.exit_y,room.exit_side);
        v->airlock_pressure=1u;
        return;
    default:
        /* Wait at the current interior position (not a door toggle).
         * A seat would make the operator walk away and back on every cycle. */
        walk_bot_airlock_target(v,AVP_MP_BOT_AIRLOCK_SEAT,
            v->x>>16,v->y>>16,room.entry_side);
        return;
    }
}

/* R281: only use AMPs that EXIST in a previously initialized retail
 * world (or a world currently occupied by another player). Do not invent
 * a supply pickup in an unvisited level or run ResetDoors()/restore_level()
 * merely to scout it. Source pickup claims and original access rules are
 * evaluated per player, just as for current-level ammunition. */
static const AvpMpNativeWorld *walk_bot_known_world(const AvpMpWalkHost *t,
                                                    unsigned level)
{
    if(!t||level<1u||level>AVP_MP_RETAIL_LEVELS)return NULL;
    for(unsigned i=0;i<AVP_LOCAL_MP_MAX_ACTORS;i++){
        const AvpMpActiveWorld *active=&t->session.worlds[i];
        if(active->context && active->level==level)
            return (const AvpMpNativeWorld *)active->context;
    }
    return t->persistent_ready[level-1u]?
        &t->persistent_world[level-1u]:NULL;
}
static int walk_bot_supply_useful(const AvpMpActorNativeState *a,
                                  const AvpMpBotObservation *v,
                                  unsigned level,const AvpAmp *item)
{
    s16 kind=COLL_NONE,amount=0;
    unsigned weapon=0u;
    if(!a||!v||!item ||
       !avp_collect_peek(level,(uint32_t)item->xpos>>16,
             (uint32_t)item->ypos>>16,item->astype,&kind,&amount))return 0;
    switch(kind){
    case COLL_SHOTGUN:case COLL_SHOTAMMO:weapon=1u;break;
    case COLL_PULSERIFLE:case COLL_PULSEAMMO:weapon=2u;break;
    case COLL_FLAMETHROWER:case COLL_FLAMEAMMO:weapon=3u;break;
    case COLL_SMARTGUN:case COLL_SMARTAMMO:weapon=4u;break;
    default:return 0;
    }
    /* Original CollectIt(COLL_*AMMO) stores reserve rounds even before the
     * weapon itself is acquired. Only native ammo capacity decides whether
     * this pickup is useful; a weapon item may also grant a new weapon. */
    const int gun=kind==COLL_SHOTGUN || kind==COLL_PULSERIFLE ||
                  kind==COLL_FLAMETHROWER || kind==COLL_SMARTGUN;
    if(v->marine_unarmed){
        if(gun && !(v->marine_owned&(1u<<weapon)))
            return amount>0 || v->marine_ammo[weapon-1u]>0;
        if(!(v->marine_owned&(1u<<weapon)) || amount<=0)return 0;
    }
    if(!(v->marine_owned&(1u<<weapon)) && gun)return 1;
    return v->marine_ammo[weapon-1u]<a->weapon.ammo_info[weapon-1u].max;
}
/* Validate remembered stock against actual native lifetime and per-player claims. */
static int walk_bot_resupply_item_valid(AvpMpWalkHost *t,unsigned p,
    const AvpMpActorNativeState *actor,const AvpMpBotObservation *v,
    const AvpMpNativeWorld *stock,unsigned level,unsigned slot)
{
    if(!stock || !stock->map || slot>=AVP_NUM_AMPS)return 0;
    const AvpAmp *amp=&stock->amp.actors[slot];
    return (amp->mode || amp->host_static) && !(amp->flags&(1u<<AMP_COLLECT)) &&
           walk_mp_item_available(t,p,level,slot,amp) &&
           !avp_mp_bot_pickup_avoided(&t->pickup_memory[p],level,slot,
               stock->amp.slot_generation[slot],v->tick) &&
           walk_bot_supply_useful(actor,v,level,amp);
}
static int walk_bot_resupply_held(AvpMpWalkHost *t,unsigned p,
    const AvpMpActorNativeState *actor,const AvpMpNativeWorld *world,
    const AvpMpBotObservation *v)
{
    AvpMpBotState *m=&t->bots[p];
    if(!m->resupply_valid)return 0;
    const AvpMpNativeWorld *stock=m->resupply_level==world->map->level?
        world:walk_bot_known_world(t,m->resupply_level);
    if(!walk_bot_resupply_item_valid(t,p,actor,v,stock,m->resupply_level,m->resupply_slot) ||
       stock->amp.slot_generation[m->resupply_slot]!=m->resupply_generation ||
       ((uint32_t)stock->amp.actors[m->resupply_slot].xpos>>16)!=m->resupply_x ||
       ((uint32_t)stock->amp.actors[m->resupply_slot].ypos>>16)!=m->resupply_y ||
       v->tick>=m->resupply_until){m->resupply_valid=0u;return 0;}
    /* Bounded renewal on arrival at the actual supply level only. */
    if(m->resupply_last_level!=world->map->level && m->resupply_level==world->map->level){
        m->resupply_last_level=world->map->level;
        m->resupply_until=v->tick+160u;
    }
    return 1;
}
static void walk_bot_supply_target(AvpMpBotObservation *v,const AvpAmp *amp,unsigned slot)
{
    v->goal=AVP_MP_BOT_SUPPLY;v->has_target=1u;v->hostile_target=0u;
    v->target_visible=v->tracker_only_target=0u;v->target_slot=slot;
    v->target_x=(u32)amp->xpos;v->target_y=(u32)amp->ypos;
}
/* Six graph probes per retry, interleaved across known levels. Within a
 * stable set of eligible stock, failed batches advance rather than starving
 * everything behind the first six AMP slots. Nearest numeric level is only
 * a probe-order hint; the real portal graph still authorizes every route. */
#define WALK_SUPPLY_PROBES 6u
struct WalkSupplyScan {
    uint16_t slots[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS+32u];
    unsigned count[AVP_MP_RETAIL_LEVELS];
};
struct WalkSupplyCandidate {unsigned level,slot;};
static unsigned walk_bot_supply_batch(const struct WalkSupplyScan *scan,
    unsigned current,unsigned *cursor,struct WalkSupplyCandidate out[WALK_SUPPLY_PROBES])
{
    unsigned order[AVP_MP_RETAIL_LEVELS],levels=0u,total=0u,max_count=0u;
    unsigned start,n=0u,ordinal=0u;
    for(unsigned l=1u;l<=AVP_MP_RETAIL_LEVELS;l++){
        const unsigned count=scan->count[l-1u];
        if(l==current || !count)continue;
        unsigned d=l>current?l-current:current-l,at=levels;
        while(at){
            const unsigned prior=order[at-1u];
            const unsigned pd=prior>current?prior-current:current-prior;
            if(pd<=d)break;
            order[at]=prior;at--;
        }
        order[at]=l;levels++;total+=count;
        if(count>max_count)max_count=count;
    }
    if(!total){*cursor=0u;return 0u;}
    start=*cursor%total;
    /* Circular interval of at most six entries. Two passes only when it
     * wraps; no repeated probe within one batch. */
    for(unsigned pass=0u;pass<2u && n<total && n<WALK_SUPPLY_PROBES;pass++){
        ordinal=0u;
        for(unsigned row=0u;row<max_count && n<total && n<WALK_SUPPLY_PROBES;row++)
            for(unsigned k=0u;k<levels && n<total && n<WALK_SUPPLY_PROBES;k++){
                const unsigned l=order[k];
                if(row>=scan->count[l-1u])continue;
                if((pass==0u && ordinal>=start) || (pass==1u && ordinal<start))
                    out[n++]=(struct WalkSupplyCandidate){l,scan->slots[l-1u][row]};
                ordinal++;
            }
    }
    *cursor=(start+n)%total;
    return n;
}
static int walk_bot_choose_known_resupply(AvpMpWalkHost *t,unsigned p,
              const AvpMpActorNativeState *actor,
              const AvpMpNativeWorld *world,AvpMpBotObservation *v)
{
    AvpMpBotState *memory=&t->bots[p];
    const unsigned current=world->map->level;
    const unsigned sx=v->x>>16,sy=v->y>>16;
    AvpMpBotObservation candidate;
    const AvpMpNativeWorld *stock;
    if(walk_bot_resupply_held(t,p,actor,world,v)){
        const unsigned level=memory->resupply_level;
        const unsigned slot=memory->resupply_slot;
        candidate=*v;
        if(level==current){
            AvpMpBotRoute route={0};
            if(avp_mp_bot_route(&v->map,sx,sy,memory->resupply_x,memory->resupply_y,&route) && route.valid){
                walk_bot_supply_target(v,&world->amp.actors[slot],slot);return 1;
            }
        }else if(walk_bot_choose_level_route(t,p,world,level,AVP_MP_MARINE,
             sx,sy,&v->map,&candidate,memory->resupply_x,memory->resupply_y,1)){
            *v=candidate;return 1;
        }
        memory->resupply_valid=0u;
    }
    if(v->tick<memory->resupply_retry_tick)return 0;
    /* Keep the existing bounded graph budget, but diversify the candidates
     * and advance through them after an unsuccessful scan. */
    struct WalkSupplyScan scan={0};
    struct WalkSupplyCandidate top[WALK_SUPPLY_PROBES];
    if(memory->resupply_scan_level!=current){
        memory->resupply_scan_level=current;memory->resupply_scan_cursor=0u;
    }
    for(unsigned level=1u;level<=AVP_MP_RETAIL_LEVELS;level++){
        if(level==current)continue;
        stock=walk_bot_known_world(t,level);
        if(!stock||!stock->map)continue;
        for(unsigned i=0;i<AVP_NUM_AMPS;i++){
            if(!walk_bot_resupply_item_valid(t,p,actor,v,stock,level,i))continue;
            scan.slots[level-1u][scan.count[level-1u]++]=(uint16_t)i;
        }
    }
    const unsigned count=walk_bot_supply_batch(&scan,current,
        &memory->resupply_scan_cursor,top);
    for(unsigned k=0;k<count;k++){
        const unsigned level=top[k].level,slot=top[k].slot;
        const AvpAmp *amp;
        stock=walk_bot_known_world(t,level);
        if(!stock)continue;
        amp=&stock->amp.actors[slot];
        candidate=*v;
        if(!walk_bot_choose_level_route(t,p,world,level,AVP_MP_MARINE,
                sx,sy,&v->map,&candidate,
                (uint32_t)amp->xpos>>16,(uint32_t)amp->ypos>>16,1))continue;
        memory->resupply_valid=1u;
        memory->resupply_scan_cursor=0u;
        memory->resupply_level=level;memory->resupply_slot=slot;
        memory->resupply_generation=stock->amp.slot_generation[slot];
        memory->resupply_x=(uint32_t)amp->xpos>>16;
        memory->resupply_y=(uint32_t)amp->ypos>>16;
        memory->resupply_last_level=current;
        memory->resupply_until=v->tick+160u;
        memory->resupply_retry_tick=v->tick+40u;
        *v=candidate;return 1;
    }
    memory->resupply_retry_tick=v->tick+40u;
    return 0;
}
/* RE44 R283: health pickups are source AMPs, not synthesized health.
 * Terminal coordinates are derived ONLY from bound original COMPUTER.S
 * panel offsets and revalidated against the original map byte. */
static int walk_bot_health_item_available(AvpMpWalkHost *t,unsigned p,
    const AvpMpNativeWorld *world,unsigned level,unsigned slot)
{
    const AvpAmp *a;
    if(!world||!world->map||slot>=AVP_NUM_AMPS)return 0;
    a=&world->amp.actors[slot];
    return (a->mode || a->host_static) && !(a->flags&(1u<<AMP_COLLECT)) &&
           (a->astype==11 || a->astype==21 || a->astype==40) &&
           walk_mp_item_available(t,p,level,slot,a);
}
static int walk_bot_medical_cell(const AvpMpNativeWorld *world,
                                 unsigned level,u16 offset,
                                 unsigned *x,unsigned *y,unsigned *side)
{
    unsigned cell;
    if(!world||!world->map||!x||!y||!side ||
       !avp_computer_is_medical_terminal(level,offset))return 0;
    if((unsigned)offset>=(unsigned)world->map->width*world->map->height*8u)
        return 0;
    *side=(unsigned)offset%8u;
    if(*side>=4u)return 0;
    cell=(unsigned)offset/8u;
    *x=cell%world->map->width;
    *y=cell/world->map->width;
    /* Must be the actual authored panel, not an invisible trigger.
     * The real DoorKeys() verifies original panel type on Use. */
    return world->map->bytes[8u+offset]!=0u &&
           world->map->bytes[8u+cell*8u+5u]!=0u;
}
/* Health uses the same bounded rotating remote search as ammunition.
 * Local, route-legal healing wins before remote graph probes. A medical
 * entry encodes its authored panel offset after the AMP slot range. */
static int walk_bot_healing_candidate(AvpMpWalkHost *t,unsigned p,
    const AvpMpActorNativeState *actor,const AvpMpBotObservation *v,
    const AvpMpNativeWorld *stock,unsigned level,unsigned id,
    unsigned *x,unsigned *y,unsigned *side,unsigned *generation)
{
    *side=*generation=0u;
    if(id>=AVP_NUM_AMPS){
        const unsigned offset=id-AVP_NUM_AMPS;
        return offset<=UINT16_MAX && v->player_species==AVP_MP_MARINE &&
            avp_computer_first_aid_ceiling(actor->access_level)>v->player_energy &&
            walk_bot_medical_cell(stock,level,(u16)offset,x,y,side);
    }
    if(!walk_bot_health_item_available(t,p,stock,level,id) ||
       avp_mp_bot_pickup_avoided(&t->pickup_memory[p],level,id,
            stock->amp.slot_generation[id],v->tick))return 0;
    *generation=stock->amp.slot_generation[id];
    *x=(uint32_t)stock->amp.actors[id].xpos>>16;
    *y=(uint32_t)stock->amp.actors[id].ypos>>16;
    return 1;
}
static void walk_bot_healing_target(AvpMpBotObservation *v,
    const AvpMpNativeWorld *stock,unsigned id,unsigned x,unsigned y,unsigned side)
{
    v->has_target=1u;v->hostile_target=0u;
    v->target_visible=v->tracker_only_target=0u;
    v->goal=id>=AVP_NUM_AMPS?AVP_MP_BOT_MEDICAL:AVP_MP_BOT_HEALTH;
    v->target_side=side;
    v->target_slot=id>=AVP_NUM_AMPS?id-AVP_NUM_AMPS:id;
    v->target_x=(x<<16)|0x8000u;v->target_y=(y<<16)|0x8000u;
    if(id<AVP_NUM_AMPS){
        v->target_x=(uint32_t)stock->amp.actors[id].xpos;
        v->target_y=(uint32_t)stock->amp.actors[id].ypos;
    }
}
static int walk_bot_choose_known_healing(AvpMpWalkHost *t,unsigned p,
    const AvpMpActorNativeState *actor,const AvpMpNativeWorld *world,
    AvpMpBotObservation *v)
{
    if(!t||!actor||!world||!world->map||!v||!v->map.cells ||
       p>=AVP_LOCAL_MP_MAX_ACTORS || t->dm.enabled || v->player_species==AVP_MP_ALIEN ||
       !avp_mp_bot_health_low(v->player_energy,v->max_energy))return 0;
    AvpMpBotState *memory=&t->bots[p];
    const unsigned current=world->map->level,sx=v->x>>16,sy=v->y>>16;
    const AvpMpNativeWorld *stock;
    AvpMpBotObservation candidate;
    unsigned x,y,side,generation;
    if(memory->healing_valid && v->tick<memory->healing_until){
        const unsigned level=memory->healing_level;
        const unsigned id=memory->healing_slot+(memory->healing_medical?AVP_NUM_AMPS:0u);
        stock=level==current?world:walk_bot_known_world(t,level);
        if((memory->healing_medical || memory->healing_slot<AVP_NUM_AMPS) &&
           walk_bot_healing_candidate(t,p,actor,v,stock,level,id,&x,&y,&side,&generation) &&
           generation==memory->healing_generation && x==memory->healing_x && y==memory->healing_y){
            candidate=*v;
            if(level==current){
                AvpMpBotRoute route={0};
                if(avp_mp_bot_route(&v->map,sx,sy,x,y,&route) && route.valid){
                    if(memory->healing_last_level!=current){
                        memory->healing_last_level=current;memory->healing_until=v->tick+150u;
                    }
                    walk_bot_healing_target(v,stock,id,x,y,side);return 1;
                }
            }else if(walk_bot_choose_level_route(t,p,world,level,v->player_species,
                      sx,sy,&v->map,&candidate,x,y,1)){
                *v=candidate;return 1;
            }
        }
    }
    if(memory->healing_valid)memory->healing_retry_tick=0u;
    memory->healing_valid=0u;
    if(memory->healing_scan_level!=current){
        memory->healing_scan_level=current;memory->healing_scan_cursor=0u;
        memory->healing_retry_tick=0u;
    }
    if(v->tick<memory->healing_retry_tick)return 0;
    struct WalkSupplyScan scan={0};
    struct WalkSupplyCandidate top[WALK_SUPPLY_PROBES];
    unsigned best_id=0u,best_x=0u,best_y=0u,best_side=0u,best_generation=0u,best_level=0u;
    uint64_t best_cost=UINT64_MAX;
    for(unsigned level=1u;level<=AVP_MP_RETAIL_LEVELS;level++){
        stock=level==current?world:walk_bot_known_world(t,level);
        if(!stock||!stock->map)continue;
        u16 offsets[32];size_t n=0u;
        if(v->player_species==AVP_MP_MARINE &&
           avp_computer_first_aid_ceiling(actor->access_level)>v->player_energy){
            n=avp_computer_medical_terminals(level,offsets,32u);if(n>32u)n=32u;
        }
        for(unsigned i=0;i<AVP_NUM_AMPS+n;i++){
            const unsigned id=i<AVP_NUM_AMPS?i:(unsigned)AVP_NUM_AMPS+offsets[i-AVP_NUM_AMPS];
            if(!walk_bot_healing_candidate(t,p,actor,v,stock,level,id,&x,&y,&side,&generation))continue;
            if(level!=current){
                if(id<=UINT16_MAX)scan.slots[level-1u][scan.count[level-1u]++]=(uint16_t)id;
                continue;
            }
            AvpMpBotRoute route={0};
            if(!avp_mp_bot_route(&v->map,sx,sy,x,y,&route) || !route.valid)continue;
            const uint64_t cost=(uint64_t)route.cost+(id>=AVP_NUM_AMPS?12u:0u);
            if(cost>=best_cost)continue;
            best_cost=cost;best_level=level;best_id=id;best_x=x;best_y=y;
            best_side=side;best_generation=generation;
        }
    }
    candidate=*v;
    if(best_level){
        walk_bot_healing_target(&candidate,world,best_id,best_x,best_y,best_side);
    }else{
        const unsigned count=walk_bot_supply_batch(&scan,current,&memory->healing_scan_cursor,top);
        for(unsigned i=0;i<count;i++){
            const unsigned level=top[i].level,id=top[i].slot;
            stock=walk_bot_known_world(t,level);
            if(!walk_bot_healing_candidate(t,p,actor,v,stock,level,id,&x,&y,&side,&generation))continue;
            candidate=*v;
            if(!walk_bot_choose_level_route(t,p,world,level,v->player_species,
                 sx,sy,&v->map,&candidate,x,y,1))continue;
            best_level=level;best_id=id;best_x=x;best_y=y;best_generation=generation;
            break;
        }
    }
    memory->healing_retry_tick=v->tick+35u;
    if(!best_level)return 0;
    memory->healing_valid=1u;memory->healing_level=best_level;
    memory->healing_x=best_x;memory->healing_y=best_y;
    memory->healing_medical=best_id>=AVP_NUM_AMPS;
    memory->healing_slot=memory->healing_medical?best_id-AVP_NUM_AMPS:best_id;
    memory->healing_generation=best_generation;memory->healing_last_level=current;
    memory->healing_until=v->tick+150u;memory->healing_scan_cursor=0u;
    *v=candidate;return 1;
}

/* Emergency fallback when the Marine has no usable rounds, cannot reach
 * current-level ammunition and no remembered cross-level supply route was
 * found. Select only an actual reachable authored exit/duct/lift. Commit to
 * it instead of bouncing between tracker locations each bot tick. This is
 * an ESCAPE from active danger, not a fake ammo source on an unseen level. */
static int walk_bot_emergency_exit(AvpMpWalkHost *t,unsigned p,
           const AvpMpNativeWorld *world,AvpMpBotObservation *v)
{
    AvpMpBotState *memory=&t->bots[p];
    const unsigned level=world->map->level;
    AvpMpBotObservation proposed;
    if(memory->emergency_exit_valid){
        if(v->tick<memory->emergency_exit_until &&
           memory->emergency_exit_destination!=level){
            proposed=*v;
            if(walk_bot_choose_level_route(t,p,world,
                  memory->emergency_exit_destination,AVP_MP_MARINE,
                  v->x>>16,v->y>>16,&v->map,&proposed,0u,0u,0) &&
               !avp_mp_bot_duct_reverse_block(&memory->duct_arrival,level,
                   proposed.destination_level,v->x,v->y,v->tick)){
                *v=proposed;return 1;
            }
        }
        memory->emergency_exit_valid=0u;
    }
    if(v->tick<memory->emergency_exit_retry_tick)return 0;
    uint64_t best=UINT64_MAX;
    unsigned chosen=0u;
    for(unsigned dest=1u;dest<=AVP_MAX_LEVEL;dest++){
        AvpMpBotRoute route={0};
        AvpMpBotObservation candidate=*v;
        if(dest==level ||
           avp_mp_bot_duct_reverse_block(&memory->duct_arrival,level,dest,v->x,v->y,v->tick) ||
           !walk_bot_choose_exit(world,dest,AVP_MP_MARINE,
                   v->x>>16,v->y>>16,&v->map,&candidate) ||
           !avp_mp_bot_route(&v->map,v->x>>16,v->y>>16,
                   candidate.target_x>>16,candidate.target_y>>16,&route))
            continue;
        /* The true source route cost, not raw distance to an illegal
         * panel, chooses an escape. The graph also verifies destination
         * reachability when committing the chosen candidate below. */
        if(route.cost>=best)continue;
        proposed=candidate;best=route.cost;chosen=dest;
    }
    if(chosen){
        AvpMpBotObservation graph=*v;
        if(walk_bot_choose_level_route(t,p,world,chosen,AVP_MP_MARINE,
                 v->x>>16,v->y>>16,&v->map,&graph,0u,0u,0) &&
           !avp_mp_bot_duct_reverse_block(&memory->duct_arrival,level,
               graph.destination_level,v->x,v->y,v->tick)){
            memory->emergency_exit_valid=1u;
            memory->emergency_exit_destination=chosen;
            memory->emergency_exit_until=v->tick+140u;
            memory->emergency_exit_retry_tick=v->tick+30u;
            *v=graph;return 1;
        }
    }
    memory->emergency_exit_retry_tick=v->tick+30u;
    return 0;
}
/* R493: player corpses are NOT AMPs. The native Deathmatch claim path
 * already grants their loot, but a bot must deliberately navigate to the
 * visible record. Keep identities in a separate pickup-watch namespace. */
#define WALK_BOT_CORPSE_SLOT_BASE (AVP_NUM_AMPS+AVP_LOCAL_MP_MAX_ACTORS+1u)
static int walk_bot_corpse_available(const AvpMpWalkHost *t,unsigned p,
    unsigned level,unsigned i)
{
    if(!t->dm.enabled || p>=AVP_LOCAL_MP_MAX_ACTORS || i>=AVP_MP_CORPSE_LIMIT)return 0;
    const AvpMpCorpse *c=&t->corpses.records[i];
    return c->occupied && c->creature==AC_HUMAN && c->level==level &&
        !(c->looted_mask&AVP_MP_ACTOR_BIT(p));
}
static int walk_bot_visible_corpse_supply(AvpMpWalkHost *t,unsigned p,
    const AvpMpActorNativeState *actor,const AvpMpNativeWorld *world,
    AvpMpBotObservation *v,uint64_t danger,uint32_t tx,uint32_t ty,
    uint64_t fixed_cost,AvpBotBreadcrumbLos los,void *los_context)
{
    if(!t->dm.enabled || !v->marine_unarmed || v->facehugged || !los)return 0;
    AvpMpDmLoot loot={0};
    /* Query the existing odds function with a hypothetical winning draw.
     * This consumes no random numbers and does not predict actual loot. */
    if(!avp_mp_dm_corpse_loot((uint8_t)v->marine_owned,actor->weapon.ammo_info,0u,0u,&loot))return 0;
    const unsigned level=world->map->level,sx=v->x>>16,sy=v->y>>16;
    const uint64_t near=(UINT64_C(3)<<16)*(UINT64_C(3)<<16);
    uint64_t best=fixed_cost;
    unsigned chosen=AVP_MP_CORPSE_LIMIT;
    const AvpMpBotPickupMemory *watch=&t->pickup_memory[p];
    for(unsigned i=0;i<AVP_MP_CORPSE_LIMIT;i++){
        const AvpMpCorpse *c=&t->corpses.records[i];
        AvpMpBotRoute route={0};
        if(!walk_bot_corpse_available(t,p,level,i) ||
           avp_mp_bot_pickup_avoided(&t->pickup_memory[p],level,
              WALK_BOT_CORPSE_SLOT_BASE+i,(unsigned)c->death_tick,v->tick) ||
           !avp_bot_in_view_or_nearby(v->heading,v->x,v->y,c->x,c->y,4u) ||
           !los(los_context,v->x,v->y,c->x,c->y))continue;
        if(!avp_mp_bot_route(&v->map,sx,sy,c->x>>16,c->y>>16,&route) || !route.valid)continue;
        if(danger<near && (walk_bot_distance(c->x,c->y,tx,ty)<danger ||
           ((route.next_x!=sx || route.next_y!=sy) &&
           walk_bot_distance((route.next_x<<16)|0x8000u,
                (route.next_y<<16)|0x8000u,tx,ty)<danger)))continue;
        /* Prefer certain authored supplies at comparable distances. */
        const uint64_t cost=(uint64_t)route.cost+10u;
        /* R495: keep the actual preceding supply objective while it still
         * passes every eligibility/visibility/route/danger check. The same
         * pickup watchdog then observes progress instead of resetting. */
        if(watch->active && watch->level==level &&
           watch->slot==WALK_BOT_CORPSE_SLOT_BASE+i &&
           watch->generation==(unsigned)c->death_tick &&
           watch->target_x==((c->x&0xffff0000u)|0x8000u) &&
           watch->target_y==((c->y&0xffff0000u)|0x8000u)){
            chosen=i;break;
        }
        if(cost>=best)continue;
        best=cost;chosen=i;
    }
    if(chosen==AVP_MP_CORPSE_LIMIT)return 0;
    const AvpMpCorpse *c=&t->corpses.records[chosen];
    v->goal=AVP_MP_BOT_SUPPLY;v->has_target=1u;v->hostile_target=0u;
    v->target_visible=v->tracker_only_target=0u;
    v->target_slot=WALK_BOT_CORPSE_SLOT_BASE+chosen;
    /* Collection is tile-based: target the corpse cell's safe centre. */
    v->target_x=(c->x&0xffff0000u)|0x8000u;
    v->target_y=(c->y&0xffff0000u)|0x8000u;
    return 1;
}
/* R472: Empty Marines must not lose supply/escape objectives to combat,
 * low-priority loot or squad regrouping. Use only real available items.
 * Preserve native exit/lift coordination by choosing before its planner. */
static void walk_bot_empty_marine(AvpMpWalkHost *t,unsigned p,
    const AvpMpActorNativeState *actor,const AvpMpNativeWorld *world,
    AvpMpBotObservation *v,AvpBotBreadcrumbLos corpse_los,void *los_context)
{
    if(!v->marine_unarmed || !v->map.cells || v->facehugged)return;
    AvpMpBotState *memory=&t->bots[p];
    const unsigned level=world->map->level;
    const unsigned sx=v->x>>16,sy=v->y>>16;
    unsigned best=AVP_NUM_AMPS;
    const int held=walk_bot_resupply_held(t,p,actor,world,v) && memory->resupply_level==level;
    const AvpMpBotPickupMemory *watch=&t->pickup_memory[p];
    int local_held=0;
    uint64_t best_cost=UINT64_MAX, danger=UINT64_MAX;
    uint32_t tx=0,ty=0;
    /* Current visible/tracker threats, including Deathmatch opponents.
     * No private enemy positions are consulted. */
    for(unsigned i=0;i<AVP_NUM_AMPS+ t->actors;i++){
        if(i<AVP_NUM_AMPS){if(!walk_bot_hostile(&world->amp.actors[i]))continue;}
        else{
            unsigned q=i-AVP_NUM_AMPS;
            const AvpMpActorNativeState *enemy=(const AvpMpActorNativeState *)t->session.player[q];
            if(!t->dm.enabled || q==p || !enemy || enemy->player.player_energy<=0 ||
               enemy->player.game_over || avp_mp_life_busy(&t->player_life[q]) ||
               !walk_vent_world_present(t,q) ||
               avp_mp_session_player_world(&t->session,q)!=
               avp_mp_session_player_world(&t->session,p))continue;
        }
        const AvpMpBotContact *c=&t->contacts[p][i];
        if(!c->sight && !c->tracker)continue;
        uint64_t d=walk_bot_distance(v->x,v->y,c->x,c->y);
        if(d<danger){danger=d;tx=c->x;ty=c->y;}
    }
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        const AvpAmp *amp=&world->amp.actors[i];
        AvpMpBotRoute route={0};
        if((!amp->mode && !amp->host_static) || (amp->flags&(1u<<AMP_COLLECT)) ||
           !walk_mp_item_available(t,p,level,i,amp) ||
           avp_mp_bot_pickup_avoided(&t->pickup_memory[p],level,i,
               world->amp.slot_generation[i],v->tick) ||
           !walk_bot_supply_useful(actor,v,level,amp))continue;
        if(!avp_mp_bot_route(&v->map,sx,sy,(u32)amp->xpos>>16,
               (u32)amp->ypos>>16,&route) || !route.valid)continue;
        uint64_t cost=route.cost;
        /* Avoid supply routes that immediately lead into a nearby threat.
         * Use the first genuine route step, not the final item's angle. */
        if(danger<((uint64_t)3u<<16)*((uint64_t)3u<<16) &&
           walk_bot_distance((route.next_x<<16)|0x8000u,
                (route.next_y<<16)|0x8000u,tx,ty)<danger &&
           (route.next_x!=sx || route.next_y!=sy))continue;
        if(held && i==memory->resupply_slot){best=i;break;}
        /* Only the final preceding objective may claim this priority. No
         * speculative scan may renew it, and a reused AMP cannot inherit it.
         * Existing cross-level arrival commitment has priority. */
        if(t->dm.enabled && !held && watch->active && watch->level==level &&
           watch->slot==i && watch->generation==world->amp.slot_generation[i] &&
           watch->target_x==(u32)amp->xpos && watch->target_y==(u32)amp->ypos){
            best=i;local_held=1;break;
        }
        if(cost<best_cost){best=i;best_cost=cost;}
    }
    memory->travel_valid=memory->tracker_seek_valid=memory->cover_flank_valid=0u;
    v->cover_flank_valid=v->retreat_to_teammate=v->retreat_to_refuge=0u;
    v->other_player_alien_trapped=v->other_player_swarmed=0u;
    if(held && best!=memory->resupply_slot)memory->resupply_valid=0u;
    if(!local_held && !(held && best==memory->resupply_slot) &&
       walk_bot_visible_corpse_supply(t,p,actor,world,v,danger,tx,ty,best_cost,
                                    corpse_los,los_context))return;
    if(best<AVP_NUM_AMPS){
        const AvpAmp *amp=&world->amp.actors[best];
        v->goal=AVP_MP_BOT_SUPPLY;v->has_target=1u;v->hostile_target=0u;
        v->target_visible=v->tracker_only_target=0u;v->target_slot=best;
        v->target_x=(u32)amp->xpos;v->target_y=(u32)amp->ypos;
        return;
    }
    if((!t->dm.enabled || t->dm.level_travel) &&
       walk_bot_choose_known_resupply(t,p,actor,world,v))return;
    if(danger!=UINT64_MAX){
        if((!t->dm.enabled || t->dm.level_travel) &&
           walk_bot_emergency_exit(t,p,world,v))return;
        unsigned gx,gy;
        if(memory->refuge_valid && memory->refuge_level==level &&
           v->tick<memory->refuge_until &&
           (sx!=memory->refuge_x || sy!=memory->refuge_y)){
            AvpMpBotRoute route={0};
            if(!avp_mp_bot_route(&v->map,sx,sy,memory->refuge_x,memory->refuge_y,&route) ||
               walk_bot_distance((route.next_x<<16)|0x8000u,
                   (route.next_y<<16)|0x8000u,tx,ty)<danger)memory->refuge_valid=0u;
        }else memory->refuge_valid=0u;
        if(!memory->refuge_valid && avp_mp_bot_pick_refuge(&v->map,sx,sy,tx>>16,ty>>16,&gx,&gy)){
            memory->refuge_valid=1u;memory->refuge_level=level;
            memory->refuge_x=gx;memory->refuge_y=gy;memory->refuge_until=v->tick+32u;
        }
        if(memory->refuge_valid){
            v->goal=AVP_MP_BOT_PATROL;v->has_target=1u;v->hostile_target=0u;
            v->target_visible=v->tracker_only_target=0u;
            v->target_x=(memory->refuge_x<<16)|0x8000u;
            v->target_y=(memory->refuge_y<<16)|0x8000u;return;
        }
        /* No safe route: invoke the planner's direct retreat guard. */
        v->goal=AVP_MP_BOT_HOSTILE;v->has_target=v->hostile_target=1u;
        v->target_x=tx;v->target_y=ty;v->target_visible=0u;
        v->tracker_only_target=0u;v->target_slot=AVP_NUM_AMPS+AVP_LOCAL_MP_MAX_ACTORS;
        return;
    }
    /* No immediate threat: keep searching real supplies, cards and exits.
     * Never keep a stale hostile investigation as an empty Marine. */
    /* R495: the generic early pickup scan accepts cards and incompatible
     * reserves, while the empty-Marine scan requires a usable supply. Do
     * not restore a rejected generic pickup during the remote scan's retry
     * interval. Explore or pursue a legitimate travel goal instead. */
    if(t->dm.enabled && v->goal==AVP_MP_BOT_SUPPLY){
        v->goal=AVP_MP_BOT_PATROL;v->has_target=v->hostile_target=0u;
        v->target_visible=v->tracker_only_target=0u;
        v->target_slot=AVP_NUM_AMPS+AVP_LOCAL_MP_MAX_ACTORS;
        walk_bot_patrol(t,p,v,level);
    }else if(v->hostile_target || v->goal==AVP_MP_BOT_INVESTIGATE || v->goal==AVP_MP_BOT_FOLLOW){
        v->goal=AVP_MP_BOT_PATROL;v->has_target=v->hostile_target=0u;
        v->target_visible=v->tracker_only_target=0u;
    }
}
/* Authored Level-5 escape pod: the original panel resides in cell (17,53).
 * The actual reachable chamber is east of the panel at x=19..23,
 * y=51..54; adjacent floor-looking tiles are disconnected by walls.
 * Seat candidates are the floor cells immediately inside its NORTH or EAST
 * walls. Face inward, away from the wall behind the actor. Each bot claims
 * a stable different place (independent of changing human positions). The
 * collision/route verification below is authoritative; no cell is fabricated.
 * For more bots than distinct valid seats, use remaining authentic floor
 * cells within the chamber instead of piling on the launch panel. */
static int walk_bot_escape_seat(const AvpMpBotObservation *v,
    unsigned bot_order,unsigned *seat_x,unsigned *seat_y,unsigned *face)
{
    static const struct {uint8_t x,y,dir;} seats[]={
        /* All first ten places are adjacent to an authentic solid wall,
         * and source-path-reachable from the real control at (17,53).
         * 0=west,1=north,2=east,3=south: look AWAY from wall behind. */
        {19,52,3},{19,54,1},{20,52,3},{20,54,1},
        {22,52,3},{22,54,1},{23,52,0},{23,54,0},
        {21,53,0},{21,54,1},
        {19,53,2},{20,53,3} /* overflow places inside authentic capsule */
    };
    const unsigned n=(unsigned)(sizeof seats/sizeof seats[0]);
    if(!v || !v->map.cells || !seat_x || !seat_y || !face ||
       v->observed_world_level!=5u)return 0;
    for(unsigned k=0;k<n;k++){
        const unsigned index=(bot_order+k)%n;
        const unsigned x=seats[index].x,y=seats[index].y;
        AvpMpBotRoute route={0};
        if(!avp_mp_bot_playable_position(v->map.cells,v->map.width,
                v->map.height,(x<<16)|0x8000u,(y<<16)|0x8000u) ||
           !avp_mp_bot_route(&v->map,v->x>>16,v->y>>16,x,y,&route))continue;
        *seat_x=x;*seat_y=y;*face=seats[index].dir;
        return 1;
    }
    return 0;
}

/* Elect exactly one living bot in a bots-only session. The random draw is
 * made ONCE at the start or after losing the leader, never in the hot route
 * loop; the original campaign RNG remains the source of randomness. */
static unsigned walk_bot_evacuation_leader(AvpMpWalkHost *t)
{
    unsigned candidates[AVP_LOCAL_MP_MAX_ACTORS],n=0u;
    if(!t)return AVP_LOCAL_MP_MAX_ACTORS;
    for(unsigned p=0;p<t->actors;p++){
        const AvpMpActorNativeState *a=
            (const AvpMpActorNativeState *)t->session.player[p];
        if(a && !t->session.setup.bot_enabled[p] &&
           !avp_mp_life_busy(&t->player_life[p]) &&
           a->player.player_energy>0 && !a->player.game_over)
            return AVP_LOCAL_MP_MAX_ACTORS; /* humans remain in command */
        if(a && t->session.setup.bot_enabled[p] &&
           !avp_mp_life_busy(&t->player_life[p]) &&
           a->player.player_energy>0 && !a->player.game_over)
            candidates[n++]=p;
    }
    if(t->evacuation_leader<t->actors)
        for(unsigned i=0;i<n;i++)if(candidates[i]==t->evacuation_leader)
            return t->evacuation_leader;
    return n?candidates[(unsigned)avp_random()%n]:AVP_LOCAL_MP_MAX_ACTORS;
}

/* The capsule's original boarding chamber occupies x=19..23/y=51..54,
 * beyond the authentic launch panel at (17,53). Do not grant escape for
 * bots still in another world, in the access corridor, or respawning. */
static int walk_bot_escape_all_boarded(const AvpMpWalkHost *t,unsigned leader)
{
    if(!t || leader>=t->actors)return 0;
    for(unsigned p=0;p<t->actors;p++)if(p!=leader){
        const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
        const AvpMpActiveWorld *w=avp_mp_session_player_world(&t->session,p);
        unsigned x,y;
        if(!a || !t->session.setup.bot_enabled[p] ||
           avp_mp_life_busy(&t->player_life[p]) ||
           a->player.player_energy<=0 || a->player.game_over)continue;
        x=a->player.x_pos>>16;y=a->player.y_pos>>16;
        if(!w || w->level!=5u || x<19u || x>23u || y<51u || y>54u)
            return 0;
    }
    return 1; /* A lone surviving bot can launch once at the real panel. */
}

/* An armed self-destruct is the same global campaign event for every Marine,
 * even if they are currently on different levels. Reaching Level 5 must use
 * the original authored route of DOORS.S panels, lifts and vents. */
static int walk_bot_evacuation_goal(AvpMpWalkHost *t,unsigned p,
    const AvpMpNativeWorld *world,unsigned species,AvpMpBotObservation *v)
{
    unsigned x=0u,y=0u,face=0u,ordinal=0u;
    if(!t || !v || !v->map.cells || !world || !world->map ||
       !t->evacuation_active || t->dm.enabled || species!=AVP_MP_MARINE)
        return 0;
    v->evacuation_active=1u;
    if(world->map->level!=5u){
        if(!walk_bot_choose_level_route(t,p,world,5u,AVP_MP_MARINE,
             v->x>>16,v->y>>16,&v->map,v,20u,53u,1))return 0;
        v->target_visible=0u;v->tracker_only_target=0u;
        return 1;
    }
    /* In a bots-only game the elected leader is responsible for REAL
     * launch-panel Use, but only AFTER every other living bot has boarded.
     * The bot does not call the pod script from a distance or invent launch. */
    if(p==t->evacuation_leader &&
       walk_bot_escape_all_boarded(t,p) &&
       avp_mp_bot_playable_position(v->map.cells,v->map.width,
          v->map.height,(17u<<16)|0x8000u,(53u<<16)|0x8000u)){
        AvpMpBotRoute route={0};
        if(avp_mp_bot_route(&v->map,v->x>>16,v->y>>16,17u,53u,&route)){
            v->goal=AVP_MP_BOT_ESCAPE_LAUNCH;
            v->has_target=1u;v->hostile_target=0u;
            v->target_visible=0u;v->tracker_only_target=0u;
            v->target_x=(17u<<16)|0x8000u;
            v->target_y=(53u<<16)|0x8000u;
            v->target_side=0u; /* original panel offset 27272: west face */
            return 1;
        }
    }
    for(unsigned q=0;q<p;q++)if(t->session.setup.bot_enabled[q])ordinal++;
    if(!walk_bot_escape_seat(v,ordinal,&x,&y,&face))return 0;
    v->goal=AVP_MP_BOT_ESCAPE_SEAT;
    v->has_target=1u;v->hostile_target=0u;
    v->target_visible=0u;v->tracker_only_target=0u;
    v->target_x=(x<<16)|0x8000u;v->target_y=(y<<16)|0x8000u;
    v->airlock_face_side=face;
    return 1;
}

static void walk_bot_commands(AvpMpWalkHost *t,
                              AvpMpPlayerCommand cmds[AVP_LOCAL_MP_MAX_ACTORS],
                              uint64_t tick)
{
    /* Native HP is read each bot-input tick; reserve damage among bots
     * choosing the same enemy so their combined shots are not all granted
     * against the same last few health points. */
    unsigned choke_reserved[AVP_NUM_AMPS]={0};
    t->lift_board_tick=tick;
    for(unsigned p=0;p<t->actors;p++)if(t->session.setup.bot_enabled[p]){
        const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
        const AvpMpActiveWorld *here=avp_mp_session_player_world(&t->session,p);
        AvpMpNativeWorld *world=here?(AvpMpNativeWorld *)here->context:NULL;
        AvpMpBotObservation view={0};
        uint8_t acid_risk[4096],prop_risk[4096];
        uint64_t best=UINT64_MAX;
        unsigned species;
        int marine_empty=0,healing_travel=0;
        unsigned acid_chokes=0u,acid_retarget=0u;
        uint16_t marine_caps[4]={0};
        memset(&cmds[p],0,sizeof(cmds[p]));
        if(!a||!world||avp_mp_life_busy(&t->player_life[p])||
           !walk_vent_world_present(t,p)||
           a->player.player_energy<=0 || a->player.game_over){
            /* Damage memory belongs to one life, not a previous corpse.
             * Keep route, AI profile and source respawn semantics intact. */
            t->bots[p].physical_valid=0u;
            t->bots[p].physical_stall=0u;
            if(a && (a->player.player_energy<=0 || a->player.game_over)){
                memset(&t->bots[p].duct_arrival,0,sizeof t->bots[p].duct_arrival);
                t->bots[p].resupply_valid=0u;t->bots[p].resupply_retry_tick=0u;
                if(t->dm.enabled)t->pickup_memory[p].active=0u;
                t->bots[p].resupply_scan_level=t->bots[p].resupply_scan_cursor=0u;
                t->bots[p].healing_valid=0u;t->bots[p].healing_retry_tick=0u;
                t->bots[p].healing_scan_level=t->bots[p].healing_scan_cursor=0u;
            }
            t->bots[p].damage_observed=0u;
            t->bots[p].damage_react_until=0u;
            t->lift_intent[p].level=0u;
            t->lift_arrival_pending[p]=0u;
            t->lift_egress_valid[p]=0u;
            for(unsigned i=0u;i<AVP_MP_BOT_LIFT_CLAIMS;i++)
                if(t->shared_lift_claims.entries[i].active &&
                   t->shared_lift_claims.entries[i].leader==p)
                    t->shared_lift_claims.entries[i].active=0u;
            continue;
        }
        species=(unsigned)a->player.player_type>>2;
        view.x=a->player.x_pos;view.y=a->player.y_pos;
        view.behavior=p<t->players?AVP_MP_BOT_BALANCED:
            t->session.setup.bot_behavior[p-t->players];
        view.heading=a->player.centre_angle;view.player_index=p;view.tick=tick;
        view.player_collision_disabled=!t->session.setup.player_collisions;
        view.observed_world_level=here->level;
        view.door_claims=&t->shared_door_claims;
        /* Read the ORIGINAL currently animated doors, not only their maze
         * opening bytes. A door may be active with byte zero for one tick
         * after the first bot's Use, and another Use would reverse it. */
        if(world->doors.valid && world->map &&
           world->doors.maze==world->map->bytes+8u){
            for(unsigned d=0;d<2u;d++)if(world->doors.door[d].active &&
                 world->doors.door[d].offset<
                     (size_t)world->map->width*world->map->height*8u){
                view.active_door_offsets[view.active_door_count++]=
                    (uint32_t)world->doors.door[d].offset;
            }
        }
        view.player_species=species;
        view.deathmatch=t->dm.enabled;
        view.current_weapon=(unsigned)(a->weapon.cur_wepno>0 ? a->weapon.cur_wepno : 0);
        if(species==AVP_MP_PREDATOR)view.predator_owned=a->weapon.cur_weps;
        if(species==AVP_MP_MARINE){
            view.marine_inventory_valid=1u;
            view.marine_owned=a->weapon.cur_weps;
            marine_empty=1;
            for(unsigned i=0;i<4u;i++){
                view.marine_ammo[i]=a->weapon.ammo_info[i].cur;
                marine_caps[i]=a->weapon.ammo_info[i].max;
                if((view.marine_owned&(1u<<(i+1u))) && view.marine_ammo[i])
                    marine_empty=0;
            }
        }
        view.marine_unarmed=species==AVP_MP_MARINE && marine_empty;
        view.marine_low_ammo=species==AVP_MP_MARINE && !t->dm.enabled &&
            avp_mp_bot_marine_needs_resupply(view.marine_owned,
                                             view.marine_ammo,marine_caps);
        if(!view.marine_low_ammo && !view.marine_unarmed)
            t->bots[p].resupply_valid=0u;
        view.player_energy=a->player.player_energy;
        view.max_energy=a->player.max_energy;
        view.medpak=a->weapon.medpak;
        view.health_low=!t->dm.enabled && species!=AVP_MP_ALIEN &&
            avp_mp_bot_health_low(view.player_energy,view.max_energy);
        if(!view.health_low)t->bots[p].healing_valid=0u;
        view.health_critical=view.health_low &&
            avp_mp_bot_health_critical(view.player_energy,view.max_energy);
        view.invis_act=a->player.invis_act;
        view.invis_stat=a->player.invis_stat;
        view.invisflag=a->player.invisflag;
        /* Player-specific original weapon overlay state: one bot cannot
         * dislodge a Facehugger that latched onto a different player. */
        view.facehugged=a->weapon.hugkill!=0;
        view.facehug_direction=a->weapon.unstick_dir;
        /* The --diagnostic map is synthetic, not an authenticated retail
         * maze. Keep the S95 direct-command smoke behavior there; only
         * solve source doors/hazards on real ROM-backed maps. */
        if(world->map && t->retail_catalog_ready){
            /* Alien players are acid-immune. Do not spend route-planning
               cost or steering time on their own species' puddles. */
            if(species==1u){memset(acid_risk,0,sizeof acid_risk);view.acid_count=0u;}
            else walk_bot_mark_acid(world,acid_risk,&view);
            walk_bot_mark_props(world,&view,prop_risk);
            view.map=(AvpMpBotMap){
                world->map->bytes+8u,world->map->width,world->map->height,
                world->doors.wall_flags,world->doors.current_meta,
                a->access_level,acid_risk,prop_risk,0u};
            acid_chokes=avp_mp_bot_acid_route_memory(&t->bots[p],&view,acid_risk);
        }
        /* Player collision exemptions match the SAME per-actor original
         * SafePos peer view. Do not steer around teammates if collisions
         * are disabled, on another level, dead or spawn-grace exempt. */
        if(t->session.setup.player_collisions && view.map.cells){
            uint64_t nearest=UINT64_MAX;
            for(unsigned q=0;q<t->actors;q++)if(q!=p &&
                avp_mp_session_player_world(&t->session,q)==here &&
                !avp_mp_peer_grace_exempt(&t->peer_spawn_grace,p,q) &&
                !walk_bot_shared_lift_pair(t,p,q) &&
                !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q)){
                const AvpMpActorNativeState *peer=
                    (const AvpMpActorNativeState *)t->session.player[q];
                if(!peer || peer->player.player_energy<=0)continue;
                const uint64_t distance=walk_bot_distance(view.x,view.y,
                    peer->player.x_pos,peer->player.y_pos);
                if(distance>=((uint64_t)3u<<16)*((uint64_t)3u<<16))continue;
                if(view.prop_count<AVP_MP_BOT_MAX_NEAR_PROPS){
                    AvpMpBotProp *block=&view.props[view.prop_count++];
                    block->x=peer->player.x_pos;
                    block->y=peer->player.y_pos;
                    block->halfwidth=AVP_MP_PEER_CLEARANCE;
                }
                if(distance<nearest){
                    nearest=distance;
                    view.nearby_peer_valid=1u;
                    view.nearby_peer_index=q;
                    view.nearby_peer_x=peer->player.x_pos;
                    view.nearby_peer_y=peer->player.y_pos;
                    view.nearby_peer_heading=peer->player.centre_angle;
                }
            }
        }
        /* Persist abandoned pickups across tick-level goal reselection.
         * A corpse on the opposite side of a wall is not a reachable item
         * simply because its maze CELL is connected. */
        {
            AvpMpBotPickupMemory *memory=&t->pickup_memory[p];
            uint32_t cost=UINT32_MAX;
            const unsigned cell=(view.y>>16)*64u+(view.x>>16);
            if(memory->active && memory->level==world->map->level &&
               ((memory->slot<AVP_NUM_AMPS &&
                world->amp.slot_generation[memory->slot]==memory->generation) ||
                (memory->slot>=WALK_BOT_CORPSE_SLOT_BASE &&
                 walk_bot_corpse_available(t,p,world->map->level,
                     memory->slot-WALK_BOT_CORPSE_SLOT_BASE) &&
                 (unsigned)t->corpses.records[memory->slot-WALK_BOT_CORPSE_SLOT_BASE].death_tick==memory->generation)) &&
               view.map.cells && (tick&3u)==0u &&
               cell!=memory->last_route_cell){
                AvpMpBotRoute path={0};
                if(avp_mp_bot_route(&view.map,view.x>>16,view.y>>16,
                     memory->target_x>>16,memory->target_y>>16,&path))
                    cost=path.cost;
            }
            avp_mp_bot_pickup_observe(memory,world->map->level,tick,
                                     view.x,view.y,cost,cell);
        }
        walk_bot_scan_contacts(t,p,world,a,&view);
        /* Cloak policy may use only OBSERVED or remembered Marine AMPs,
         * not a census of the entire level. Currently seen/scanned nearby
         * hostiles prevent cloaking during a fight. */
        for(unsigned i=0;t->combat_test && i<AVP_NUM_AMPS;i++){
            const AvpAmp *amp=&world->amp.actors[i];
            uint64_t distance;
            const AvpMpBotContact *contact=&t->contacts[p][i];
            if(!walk_bot_hostile(amp) || !contact->known)continue;
            if(amp->creature==AC_HUMAN)view.marines_on_level++;
            if(!contact->sight && (!contact->tracker ||
               (species==AVP_MP_MARINE && !marine_empty)))continue;
            distance=walk_bot_distance(view.x,view.y,contact->x,contact->y);
            if(distance<=((uint64_t)6u<<16)*((uint64_t)6u<<16))
                view.combat_enemies_nearby++;
        }
        /* Deathmatch players are legitimate hostile contacts too. Their
         * coordinates influence cloaking ONLY after optical detection or
         * another original scanner contact (no all-map opponent census). */
        if(t->dm.enabled)for(unsigned q=0;q<t->actors;q++)if(q!=p &&
            avp_mp_session_player_world(&t->session,q)==here &&
            !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q)){
            const AvpMpActorNativeState *opponent=(const AvpMpActorNativeState *)t->session.player[q];
            const AvpMpBotContact *contact=&t->contacts[p][AVP_NUM_AMPS+q];
            if(!opponent || opponent->player.player_energy<=0)continue;
            /* DM roster membership can govern whether to cloak even if
             * the opposing player is out of sight. Never provide their
             * position or a target unless the real contact was observed. */
            view.dm_opponents_on_level++;
            if(!contact->known)continue;
            if(opponent->player.player_type==PT_HUMAN)view.marines_on_level++;
            if((contact->sight||(contact->tracker &&
                 (species!=AVP_MP_MARINE || marine_empty))) &&
               walk_bot_distance(view.x,view.y,contact->x,contact->y)<=
                   ((uint64_t)6u<<16)*((uint64_t)6u<<16))
                view.combat_enemies_nearby++;
        }
        /* Friendly human Marines are teammates, not hostile Marines.
           Their presence must not make a Predator cloak on an empty map. */
        /* Low-health Alien actively pursues ONLY native Marine AMPs for
         * cocooning. Keep the same valid target through claw/tail/claw and
         * the close-range stun-death phase; changing target every tick can
         * interrupt the sequence. Completed cocoons cease to be Marines. */
        if(t->combat_test && species==AVP_MP_ALIEN &&
           view.max_energy>0 && view.player_energy*100<view.max_energy*65){
            unsigned chosen=AVP_NUM_AMPS;
            uint64_t best_cocoon=UINT64_MAX;
            AvpMpBotState *bot=&t->bots[p];
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *amp=&world->amp.actors[i];
                uint64_t distance;
                AvpMpBotRoute path;
                const AvpMpBotContact *contact=&t->contacts[p][i];
                if(amp->creature!=AC_HUMAN || !contact->known ||
                   (!contact->sight && !contact->tracker) ||
                   !(walk_bot_hostile(amp)||walk_bot_cocoon_candidate(amp)))continue;
                if(view.map.cells &&
                   !avp_mp_bot_route(&view.map,view.x>>16,view.y>>16,
                                     contact->x>>16,contact->y>>16,&path))
                    continue;
                distance=walk_bot_distance(view.x,view.y,contact->x,contact->y);
                if(bot->cocoon_locked && bot->cocoon_target_slot==i){
                    chosen=i;break;
                }
                if(distance<best_cocoon){best_cocoon=distance;chosen=i;}
            }
            if(chosen<AVP_NUM_AMPS){
                const AvpAmp *amp=&world->amp.actors[chosen];
                if(!bot->cocoon_locked || bot->cocoon_target_slot!=chosen){
                    bot->cocoon_phase=0u;
                    bot->cocoon_phase_end=tick+14u;
                }
                bot->cocoon_locked=1u;bot->cocoon_target_slot=chosen;
                view.has_target=1u;view.goal=AVP_MP_BOT_COCOON;
                view.target_is_marine=1u;
                view.target_is_stunned=!!(amp->flags&(1u<<AMP_STUN));
                view.target_energy_positive=amp->energy>0;
                view.hostile_target=amp->energy>0;
                view.target_slot=chosen;
                view.target_x=t->contacts[p][chosen].x;
                view.target_y=t->contacts[p][chosen].y;
            }else{bot->cocoon_locked=0u;bot->cocoon_phase=0u;}
        }else if(species==AVP_MP_ALIEN){
            t->bots[p].cocoon_locked=0u;t->bots[p].cocoon_phase=0u;
        }
        /* Health search for Marines and Predators at <=60% health; normal
         * pickups MUST be uncollected and still active in the source world.
         * Alien survival never chases a health item it cannot use. */
        if(!view.has_target && species!=AVP_MP_ALIEN &&
           view.health_low &&
           (species!=AVP_MP_PREDATOR || view.medpak<=0)){
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *amp=&world->amp.actors[i];
                uint64_t distance;
                AvpMpBotRoute route;
                unsigned gx=(u32)amp->xpos>>16,gy=(u32)amp->ypos>>16;
                if(!amp->mode && !amp->host_static)continue;
                if(amp->flags&(1u<<AMP_COLLECT) ||
                   !walk_mp_item_available(t,p,world->map->level,i,amp) ||
                   avp_mp_bot_pickup_avoided(&t->pickup_memory[p],
                       world->map->level,i,world->amp.slot_generation[i],tick))continue;
                if(amp->astype!=11 && amp->astype!=21 && amp->astype!=40)continue;
                /* A nearby health pack behind a locked original door is
                 * not a useful target. Prefer a reachable source path. */
                if(view.map.cells){
                    if(!avp_mp_bot_route(&view.map,view.x>>16,view.y>>16,
                                         gx,gy,&route))continue;
                    distance=route.cost;
                }else distance=walk_bot_distance(view.x,view.y,
                                                   (u32)amp->xpos,(u32)amp->ypos);
                if(distance>=best)continue;
                best=distance;view.has_target=1;view.hostile_target=0;
                view.goal=AVP_MP_BOT_HEALTH;view.target_slot=i;
                view.target_x=(u32)amp->xpos;view.target_y=(u32)amp->ypos;
            }
        }
        /* Currently sighted enemies should interrupt routine health/supply
         * searches throughout the original ranged engagement envelope,
         * not only within the old three-tile panic radius. Scanner-only
         * contacts do not interrupt supply or health searches. */
        if((species!=AVP_MP_MARINE || !marine_empty) &&
           view.goal!=AVP_MP_BOT_COCOON && t->combat_test){
            uint64_t nearest=UINT64_MAX;
            unsigned selected=0u,selected_sight=0u;
            const AvpAmp *threat=NULL;
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *amp=&world->amp.actors[i];
                uint64_t distance;
                const AvpMpBotContact *contact=&t->contacts[p][i];
                if(!walk_bot_hostile(amp) || !contact->known)continue;
                if(species==AVP_MP_PREDATOR &&
                   amp->creature==AC_EGG && avp_mp_bot_egg_avoided(&t->bots[p],
                       world->map->level,i,world->amp.slot_generation[i],tick))continue;
                if(contact->tracker && !contact->sight &&
                   (tick<t->bots[p].tracker_cooldown_until ||
                    avp_mp_bot_tracker_contact_suppressed(&t->bots[p],i,
                       world->map->level,contact->serial,
                       contact->x,contact->y,tick)))continue;
                distance=walk_bot_distance(view.x,view.y,contact->x,contact->y);
                if(!avp_mp_bot_immediate_contact_prefer(species,
                     contact->sight,contact->tracker,distance,
                     selected,selected_sight,nearest))continue;
                nearest=distance;selected=1u;
                selected_sight=contact->sight;threat=amp;
            }
            /* R473: only optical confirmation interrupts supplies. The
             * ordinary pursuit scan below handles reachable scanner clues. */
            /* Above the last quarter, defend normally. Below it, reserve
             * rounds for close, actually visible emergencies and leave a
             * route to real supplies; don't chase a scanner blip to death. */
            if(threat && avp_mp_bot_contact_interrupts_supply(selected_sight,nearest) &&
               (!view.marine_low_ammo ||
                (selected_sight && nearest<
                   ((uint64_t)2u<<16)*((uint64_t)2u<<16)))){
                const unsigned idx=(unsigned)(threat-world->amp.actors);
                const AvpMpBotContact *contact=&t->contacts[p][idx];
                view.has_target=view.hostile_target=1u;
                view.goal=AVP_MP_BOT_HOSTILE;
                view.target_x=contact->x;view.target_y=contact->y;
                view.target_is_marine=threat->creature==AC_HUMAN;
                view.target_is_alien=threat->creature==AC_ALIEN ||
                                     threat->creature==AC_ACRAWL;
                view.target_energy_positive=threat->energy>0;
                view.target_slot=idx;
            }
        }
        /* Marine scrounging uses the SAME read-only original pickup table
           for standalone Shotgun Shells (def 25), Pulse Rifle Rounds (16/24),
           Flamethrower Gas (15), Smart Gun Ammo (12/23), as well as authored
           corpses including green-uniform pulse-ammo bodies. Even an incomplete magazine can benefit from one pickup:
           consider it when below its original max, never fabricate ammo or
           collect an already-claimed body. Combat still overrides detours. */
        if(!view.has_target && species==AVP_MP_MARINE && view.map.cells &&
           view.marine_low_ammo && !view.marine_unarmed &&
           t->bots[p].resupply_level==world->map->level &&
           walk_bot_resupply_held(t,p,a,world,&view)){
            AvpMpBotRoute route={0};
            const unsigned slot=t->bots[p].resupply_slot;
            const AvpAmp *amp=&world->amp.actors[slot];
            if(avp_mp_bot_route(&view.map,view.x>>16,view.y>>16,
                (u32)amp->xpos>>16,(u32)amp->ypos>>16,&route) && route.valid)
                walk_bot_supply_target(&view,amp,slot);
            else t->bots[p].resupply_valid=0u;
        }
        if(!view.has_target && species==AVP_MP_MARINE && view.map.cells){
            unsigned loaded_total=0u,ammo_cap=0u;
            int any_loaded=0;
            struct {const AvpAmp *amp;uint64_t estimate;} candidates[8]={{0}};
            unsigned count=0u;
            for(unsigned w=0;w<4u;w++)if(view.marine_owned&(1u<<(w+1u))){
                loaded_total+=view.marine_ammo[w];
                ammo_cap+=a->weapon.ammo_info[w].max;
                if(view.marine_ammo[w])any_loaded=1;
            }
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *amp=&world->amp.actors[i];
                s16 type=COLL_NONE,amount=0;
                unsigned weapon=0u,prio=3u;
                uint64_t dist,estimate;
                unsigned at;
                if((!amp->mode && !amp->host_static) ||
                   (amp->flags&(1u<<AMP_COLLECT)) ||
                   !walk_mp_item_available(t,p,world->map->level,i,amp) ||
                   avp_mp_bot_pickup_avoided(&t->pickup_memory[p],
                       world->map->level,i,world->amp.slot_generation[i],tick))continue;
                if(!avp_collect_peek(world->map->level,(u32)amp->xpos>>16,
                      (u32)amp->ypos>>16,amp->astype,&type,&amount))continue;
                if(type>=COLL_S1 && type<=COLL_S10){
                    if((unsigned)type<=a->access_level)continue;
                    prio=0u;
                }else if(type==COLL_MTRACKER){
                    prio=3u;
                }else{
                    switch(type){
                    case COLL_SHOTGUN:case COLL_SHOTAMMO:weapon=1u;break;
                    case COLL_PULSERIFLE:case COLL_PULSEAMMO:weapon=2u;break;
                    case COLL_FLAMETHROWER:case COLL_FLAMEAMMO:weapon=3u;break;
                    case COLL_SMARTGUN:case COLL_SMARTAMMO:weapon=4u;break;
                    default:continue;
                    }
                    const unsigned cur=view.marine_ammo[weapon-1u];
                    const unsigned max=a->weapon.ammo_info[weapon-1u].max;
                    const unsigned missing=max>cur?max-cur:0u;
                    const int weapon_item=type==COLL_SHOTGUN || type==COLL_PULSERIFLE ||
                        type==COLL_FLAMETHROWER || type==COLL_SMARTGUN;
                    if(weapon_item && !(view.marine_owned&(1u<<weapon)))prio=1u;
                    else if(missing==0u)continue;
                    else if(!any_loaded)prio=0u;
                    /* An empty reserve is worth fetching even if the bot
                     * has not found its weapon yet. In the starting Brig,
                     * authentic green-uniform corpses carry pulse rounds. */
                    else if(cur==0u)prio=1u;
                    /* Never reject an EMPTY weapon's compatible ammo
                     * merely because other guns happen to be well stocked.
                     * Immediate visually confirmed combat was resolved
                     * before this passive scavenge step. */
                    else if(cur>0u && ammo_cap &&
                            loaded_total*4u>ammo_cap &&
                            view.combat_enemies_nearby)continue;
                    else prio=2u;
                }
                dist=walk_bot_distance(view.x,view.y,(u32)amp->xpos,(u32)amp->ypos);
                estimate=dist/((uint64_t)65536u*65536u)+prio*90u;
                /* Bound Dijkstra work to the eight most promising source
                   pickups on this tick; retain nearest candidate per rank. */
                for(at=0;at<count && candidates[at].estimate<=estimate;at++);
                if(at>=8u)continue;
                if(count<8u)count++;
                for(unsigned j=count-1u;j>at;j--)candidates[j]=candidates[j-1u];
                candidates[at].amp=amp;candidates[at].estimate=estimate;
            }
            best=UINT64_MAX;
            for(unsigned i=0;i<count;i++){
                const AvpAmp *amp=candidates[i].amp;
                AvpMpBotRoute route;
                if(!avp_mp_bot_route(&view.map,view.x>>16,view.y>>16,
                    (u32)amp->xpos>>16,(u32)amp->ypos>>16,&route))continue;
                uint64_t cost=(uint64_t)route.cost+candidates[i].estimate/2u;
                /* Fight immediate threats while armed. Desperate, empty-gun
                   Marines may run toward farther supply caches instead. */
                if(any_loaded && !view.marine_low_ammo &&
                   view.combat_enemies_nearby && cost>25u)continue;
                if(cost>=best)continue;
                best=cost;view.has_target=1;view.hostile_target=0;
                view.goal=AVP_MP_BOT_SUPPLY;
                view.target_slot=(unsigned)(amp-world->amp.actors);
                view.target_x=(u32)amp->xpos;view.target_y=(u32)amp->ypos;
            }
        }
        if(!view.has_target && !marine_empty && !view.marine_low_ammo){
            /* Evaluate at most eight nearest source enemies with the real
             * grid planner. An AMP behind a locked wall must not suppress a
             * reachable enemy. Limit Dijkstra calls at the 10 Hz tick. */
            struct {const AvpAmp *amp;const AvpMpBotContact *contact;
                    uint64_t dist;} nearby[8]={0};
            unsigned count=0;
            best=UINT64_MAX;
            for(unsigned i=0;t->combat_test && i<AVP_NUM_AMPS;i++){
                const AvpAmp *amp=&world->amp.actors[i];
                uint64_t distance;
                unsigned at;
                const AvpMpBotContact *contact=&t->contacts[p][i];
                if(!walk_bot_hostile(amp)||!contact->known)continue;
                if(species==AVP_MP_PREDATOR &&
                   amp->creature==AC_EGG && avp_mp_bot_egg_avoided(&t->bots[p],
                       world->map->level,i,world->amp.slot_generation[i],tick))continue;
                if(contact->tracker && !contact->sight &&
                   (tick<t->bots[p].tracker_cooldown_until ||
                    avp_mp_bot_tracker_contact_suppressed(&t->bots[p],i,
                       world->map->level,contact->serial,
                       contact->x,contact->y,tick)))continue;
                distance=walk_bot_distance(view.x,view.y,contact->x,contact->y);
                if(species==AVP_MP_ALIEN && amp->creature==AC_HUMAN &&
                   view.max_energy>0 && view.player_energy*100<view.max_energy*45)
                    distance/=4u;
                for(at=0;at<count && nearby[at].dist<=distance;at++);
                if(at>=8u)continue;
                if(count<8u)count++;
                for(unsigned j=count-1u;j>at;j--)nearby[j]=nearby[j-1u];
                nearby[at].amp=amp;nearby[at].contact=contact;
                nearby[at].dist=distance;
            }
            for(unsigned i=0;i<count;i++){
                const AvpAmp *amp=nearby[i].amp;
                const AvpMpBotContact *contact=nearby[i].contact;
                uint64_t distance=nearby[i].dist;
                AvpMpBotRoute route;
                if(view.map.cells){
                    if(!avp_mp_bot_route(&view.map,view.x>>16,view.y>>16,
                         contact->x>>16,contact->y>>16,&route)){
                        /* A currently seen AMP can be shot even if its cell
                         * cannot be navigated into, never through a wall. */
                        if(!contact->sight){
                            /* Hidden contacts must have a local route. */
                            continue;
                        }else distance=2u+(distance>>32);
                    }else distance=(uint64_t)route.cost+1u;
                    if(species==AVP_MP_ALIEN && amp->creature==AC_HUMAN &&
                       view.max_energy>0 && view.player_energy*100<view.max_energy*45)
                        distance/=4u;
                }
                if(distance>=best)continue;
                best=distance;view.has_target=1;view.hostile_target=1;
                view.goal=AVP_MP_BOT_HOSTILE;
                view.target_x=contact->x;view.target_y=contact->y;
                view.target_is_marine=amp->creature==AC_HUMAN;
                view.target_is_alien=amp->creature==AC_ALIEN || amp->creature==AC_ACRAWL;
                view.target_is_stunned=!!(amp->flags&(1u<<AMP_STUN));
                view.target_energy_positive=amp->energy>0;
                view.target_slot=(unsigned)(amp-world->amp.actors);
            }
        }
        /* Tactical barrel shooting: only if an actual source DRUM can
         * damage the current enemy, with the bot and all friends outside
         * the original 1-cell explosion range (extra margin). */
        if(view.hostile_target && species!=AVP_MP_ALIEN && best>0){
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *amp=&world->amp.actors[i];
                int safe=1;
                if(!walk_bot_amp_active(amp)||amp->creature!=AC_DRUM||
                   amp->energy<=0||!(amp->flags&(1u<<AMP_KILLABLE)))continue;
                if(walk_bot_distance((u32)amp->xpos,(u32)amp->ypos,
                                     view.target_x,view.target_y)>(uint64_t)(1.0*65536.0*1.0*65536.0))continue;
                if(walk_bot_distance(view.x,view.y,(u32)amp->xpos,(u32)amp->ypos)<
                   (uint64_t)(2.5*65536.0*2.5*65536.0))continue;
                for(unsigned q=0;q<t->actors;q++)if(q!=p &&
                    avp_mp_session_player_world(&t->session,q)==here){
                    const AvpMpActorNativeState *other=(const AvpMpActorNativeState *)t->session.player[q];
                    if(other && walk_bot_distance(other->player.x_pos,other->player.y_pos,
                                                 (u32)amp->xpos,(u32)amp->ypos)<
                        (uint64_t)(2.5*65536.0*2.5*65536.0))safe=0;
                }
                if(!safe)continue;
                view.target_x=(u32)amp->xpos;view.target_y=(u32)amp->ypos;
                view.target_is_alien=0u; /* A live barrel is not an Alien. */
                /* Barrel is environmental, not an unseen enemy contact.
                 * A shot still needs an original clear, forward visual ray. */
                view.target_slot=AVP_NUM_AMPS+AVP_LOCAL_MP_MAX_ACTORS;
                view.target_visible=avp_bot_in_view(view.heading,view.x,view.y,
                    view.target_x,view.target_y) &&
                    walk_bot_source_los(world,view.x,view.y,
                        view.target_x,view.target_y);
                break;
            }
        }
        /* Alien campaign AI has its OWN objectives; Marine walkthrough
         * tactics do not imply that Alien bots should scavenge or follow
         * the Marine route. An immediate nearby attack and a low-health
         * cocoon take precedence. Never cross a forbidden lift. */
        if(!t->dm.enabled && species==AVP_MP_ALIEN &&
           t->species==AVP_MP_ALIEN && view.goal!=AVP_MP_BOT_COCOON &&
           world->map && view.map.cells){
            uint64_t threat_distance=view.hostile_target ?
                walk_bot_distance(view.x,view.y,view.target_x,view.target_y) : UINT64_MAX;
            if(threat_distance>((uint64_t)3u<<16)*((uint64_t)3u<<16)){
                if(here->level!=15u){
                    unsigned dest=avp_mp_bot_alien_campaign_target_level(here->level);
                    if(dest){
                        AvpMpBotObservation mission=view;
                        if(walk_bot_choose_level_route(t,p,world,dest,species,
                                 view.x>>16,view.y>>16,&view.map,&mission,
                                 0u,0u,0))
                            view=mission;
                    }
                }else{
                    /* Fight the actual generators, not the shield sprites.
                     * After ngens reaches zero, face the real Queen actor
                     * while retail AMP.S advances the ending. */
                    const AvpAmp *target=NULL,*queen=NULL;
                    uint64_t closest=UINT64_MAX;
                    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                        const AvpAmp *amp=&world->amp.actors[i];
                        AvpMpBotRoute route;
                        uint64_t d;
                        if(amp->creature==AC_AQUEEN && amp->mode)
                            queen=amp;
                        if(amp->creature!=AC_GEN || !amp->mode ||
                           amp->energy<=0 || !(amp->flags&(1u<<AMP_KILLABLE)))continue;
                        if(!avp_mp_bot_route(&view.map,view.x>>16,view.y>>16,
                            (u32)amp->xpos>>16,(u32)amp->ypos>>16,&route))continue;
                        d=(uint64_t)route.cost;
                        if(d<closest){closest=d;target=amp;}
                    }
                    if(target){
                        view.has_target=1u;view.hostile_target=1u;
                        view.goal=AVP_MP_BOT_HOSTILE;
                        view.target_x=(u32)target->xpos;
                        view.target_y=(u32)target->ypos;
                        view.target_is_marine=0u;view.target_is_alien=0u;
                        view.target_is_stunned=0u;
                        view.target_energy_positive=1u;
                        /* An authored mission generator is not a sensed
                         * enemy. Its objective position is known, but the
                         * bot can fire only on a visible original LOS. */
                        view.target_slot=AVP_NUM_AMPS+AVP_LOCAL_MP_MAX_ACTORS;
                        view.target_visible=avp_bot_in_view(view.heading,view.x,view.y,
                            view.target_x,view.target_y) &&
                            walk_bot_source_los(world,view.x,view.y,
                                view.target_x,view.target_y);
                    }else if(!world->amp.ngens && queen){
                        view.has_target=1u;view.hostile_target=0u;
                        view.goal=AVP_MP_BOT_QUEEN_WAIT;
                        view.target_x=(u32)queen->xpos;
                        view.target_y=(u32)queen->ypos;
                    }
                }
            }
        }
        if(!view.has_target && !t->dm.enabled){best=UINT64_MAX;
            /* Prefer a human leader. With all four AI slots enabled,
             * P1 provides a stable leader for P2-P4 rather than having
             * each bot chase another bot in a circular dependency. */
            for(unsigned q=0;q<t->actors;q++)if(q!=p &&
                (!t->session.setup.bot_enabled[q] || (p!=0u && q==0u)) &&
                !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q) &&
                avp_mp_session_player_world(&t->session,q)==here){
                const AvpMpActorNativeState *other=(const AvpMpActorNativeState *)t->session.player[q];
                if(other && other->player.player_energy>0 && !other->player.game_over){
                    uint64_t distance=walk_bot_distance(view.x,view.y,
                        other->player.x_pos,other->player.y_pos);
                    if(distance>=best ||
                       !avp_mp_bot_follow_target_possible(species,here->level,
                           here->level,&view.map,view.x>>16,view.y>>16,
                           (u32)other->player.x_pos>>16,
                           (u32)other->player.y_pos>>16))continue;
                    best=distance;view.has_target=1;
                    view.goal=AVP_MP_BOT_FOLLOW;
                    view.target_x=other->player.x_pos;view.target_y=other->player.y_pos;
                }
            }
        }
        /* Follow human players across real authored level links. Do not
         * teleport, force shared world state, or offer lifts to Aliens. */
        if(!view.has_target && !t->dm.enabled && world->doors.level_meta &&
           world->doors.level_meta_count>=AVP_MAX_LEVEL){
            best=UINT64_MAX;
            for(unsigned q=0;q<t->actors;q++)if(q!=p &&
                (!t->session.setup.bot_enabled[q] || (p!=0u && q==0u)) &&
                !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q)){
                const AvpMpActiveWorld *otherworld=
                    avp_mp_session_player_world(&t->session,q);
                const AvpMpActorNativeState *other=(const AvpMpActorNativeState *)t->session.player[q];
                if(!otherworld||!other||
                   other->player.player_energy<=0||other->player.game_over)
                    continue;
                /* If a teammate is behind a JAMMED door on our SAME
                 * level, the direct follow route failed above. Plan a
                 * detour via an original duct instead of idling. */
                {AvpMpBotObservation candidate=view;
                 uint64_t distance=walk_bot_distance(view.x,view.y,
                              other->player.x_pos,other->player.y_pos);
                 if(distance<best && walk_bot_choose_level_route(t,p,world,
                            otherworld->level,species,view.x>>16,view.y>>16,
                            &view.map,&candidate,
                            (u32)other->player.x_pos>>16,
                            (u32)other->player.y_pos>>16,1)){
                     best=distance;view=candidate;
                 }}
            }
        }
        /* Nearest live melee threat is separate from the chosen combat or
         * health target. Use source AMP species and active/dead checks; do
         * not dodge an enemy behind an opaque authored wall or closed door. */
        if(t->combat_test){
            uint64_t nearest=UINT64_MAX;
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *amp=&world->amp.actors[i];
                unsigned threat=AVP_MP_BOT_THREAT_NONE;
                uint64_t d;
                const AvpMpBotContact *contact=&t->contacts[p][i];
                if(!walk_bot_hostile(amp)||!contact->known ||
                   !contact->sight)continue;
                switch(amp->creature){
                case AC_ALIEN:case AC_ACRAWL:threat=AVP_MP_BOT_THREAT_ALIEN;break;
                case AC_PREDATOR:threat=AVP_MP_BOT_THREAT_PREDATOR;break;
                case AC_AQUEEN:threat=AVP_MP_BOT_THREAT_QUEEN;break;
                case AC_FACEHUG:threat=AVP_MP_BOT_THREAT_FACEHUG;break;
                default:break;
                }
                if(!threat)continue;
                d=walk_bot_distance(view.x,view.y,contact->x,contact->y);
                /* At most ~4 map cells. Source melee strike reach is
                 * shorter; this margin gives the bot room to respond. */
                if(d>=((uint64_t)4u<<16)*((uint64_t)4u<<16) || d>=nearest)
                    continue;
                nearest=d;view.melee_threat=threat;
                view.melee_x=contact->x;view.melee_y=contact->y;
            }
            if(view.melee_threat)for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpMpBotContact *contact=&t->contacts[p][i];
                if(contact->sight && contact->x==view.melee_x &&
                   contact->y==view.melee_y){view.melee_visible=1u;break;}
            }
        }
        if(view.hostile_target && t->combat_test &&
           view.target_slot<AVP_NUM_AMPS){
            const AvpMpBotContact *contact=&t->contacts[p][view.target_slot];
            view.target_visible=contact->sight &&
                  contact->x==view.target_x && contact->y==view.target_y;
        }
        /* Deathmatch opponents compete with source AMP enemies. R8 used to
         * CLEAR the selected AMP target here, so bots ignored ALL native
         * enemies whenever Deathmatch was enabled. Keep whichever acquired
         * threat is nearer, giving current visible contacts precedence over
         * stale breadcrumb/last-seen intel. */
        if(t->dm.enabled){
            uint64_t nearest=view.hostile_target ?
                walk_bot_distance(view.x,view.y,view.target_x,view.target_y):
                UINT64_MAX;
            unsigned best_visible=view.hostile_target && view.target_visible;
            for(unsigned q=0;q<t->actors;q++)if(q!=p &&
                !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q) &&
                avp_mp_session_player_world(&t->session,q)==here){
                const AvpMpActorNativeState *enemy=(const AvpMpActorNativeState *)t->session.player[q];
                uint64_t d;
                const AvpMpBotContact *contact=&t->contacts[p][AVP_NUM_AMPS+q];
                if(!enemy || enemy->player.player_energy<=0 || !contact->known)continue;
                if(contact->tracker && !contact->sight){
                    AvpMpBotRoute radar_route={0};
                    if(tick<t->bots[p].tracker_cooldown_until ||
                       avp_mp_bot_tracker_contact_suppressed(&t->bots[p],AVP_NUM_AMPS+q,
                           world->map->level,contact->serial,contact->x,contact->y,tick) ||
                       (view.has_target && !view.hostile_target) ||
                       (view.map.cells && !avp_mp_bot_route(&view.map,view.x>>16,view.y>>16,
                           contact->x>>16,contact->y>>16,&radar_route)))continue;
                }
                d=walk_bot_distance(view.x,view.y,contact->x,contact->y);
                /* Visible contacts defeat stale targets; do not interrupt
                 * combat with an AMP just because a player is known. */
                if(!avp_mp_bot_dm_prefer_player(view.hostile_target,
                      best_visible,nearest,contact->sight,d,view.has_target))
                    continue;
                nearest=d;best_visible=contact->sight;
                view.has_target=view.hostile_target=1u;
                view.goal=AVP_MP_BOT_HOSTILE;view.target_x=contact->x;
                view.target_y=contact->y;
                view.target_is_marine=enemy->player.player_type==PT_HUMAN;
                view.target_is_alien=enemy->player.player_type==PT_ALIEN;
                view.target_energy_positive=1u;
                view.target_slot=AVP_NUM_AMPS+q;
                view.target_visible=contact->sight;
            }
        }
        /* A contact's old square is a clue, not an instruction to stare at
         * empty floor forever. Once reached and checked for a full second,
         * investigate NEW sighted breadcrumbs or resume map exploration. */
        if(view.hostile_target && view.goal==AVP_MP_BOT_HOSTILE &&
           !view.target_visible &&
           view.target_slot<AVP_NUM_AMPS+AVP_LOCAL_MP_MAX_ACTORS){
            AvpMpBotContact *c=&t->contacts[p][view.target_slot];
            if(c->known && !c->tracker && view.tick>=c->last_seen_tick+10u &&
               walk_bot_distance(view.x,view.y,view.target_x,view.target_y)<
                   ((uint64_t)32768u*32768u)){
                avp_bot_contact_investigated(c);
                view.has_target=view.hostile_target=0u;
            }
        }
        /* Deathmatch travel is a real goal, not a low-priority random
         * patrol destination. A known enemy or needed supply wins first. */
        if(!view.has_target)
            (void)walk_bot_dm_travel(t,p,world,species,&view);
        if(!view.has_target)walk_bot_patrol(t,p,&view,world->map->level);
        else if(view.map.cells && view.tick%6u==0u){
            unsigned cx=view.x>>16,cy=view.y>>16;
            /* Memory is accumulated while pursuing fights and supplies too:
             * switching back to exploration never forgets visited corridors. */
            if(t->patrol[p].level==world->map->level && cx<64u && cy<64u &&
               t->patrol[p].visits[cy*64u+cx]<65535u)
                ++t->patrol[p].visits[cy*64u+cx];
        }
        /* RE44 R275: an optically visible original Alien physically pinning
         * another living player takes precedence over ordinary nearest-AMP
         * targeting. An unrelated closer enemy must not make the bot ignore
         * a teammate who has no legal escape. Uses this bot's actual
         * optical contacts and native actors only; no hidden target cheat. */
        if(!t->dm.enabled && t->combat_test && view.map.cells &&
           species!=AVP_MP_ALIEN){
            uint64_t nearest=UINT64_MAX;
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *alien=&world->amp.actors[i];
                const AvpMpBotContact *contact=&t->contacts[p][i];
                if(!walk_bot_hostile(alien) ||
                   (alien->creature!=AC_ALIEN && alien->creature!=AC_ACRAWL) ||
                   !contact->sight ||
                   contact->x!=(uint32_t)alien->xpos ||
                   contact->y!=(uint32_t)alien->ypos)continue;
                const uint64_t distance=walk_bot_distance(view.x,view.y,
                    contact->x,contact->y);
                if(distance>=nearest)continue;
                for(unsigned q=0;q<t->actors;q++)if(q!=p &&
                    avp_mp_session_player_world(&t->session,q)==here &&
                    !avp_mp_life_busy(&t->player_life[q]) &&
                    walk_vent_world_present(t,q)){
                    const AvpMpActorNativeState *victim=
                        (const AvpMpActorNativeState *)t->session.player[q];
                    if(!victim || victim->player.player_energy<=0 ||
                       victim->player.game_over ||
                       victim->player.player_type==PT_ALIEN)continue;
                    if(!avp_mp_bot_alien_traps_player(&view.map,
                        (uint32_t)alien->xpos,(uint32_t)alien->ypos,
                        victim->player.x_pos,victim->player.y_pos))continue;
                    nearest=distance;
                    view.has_target=view.hostile_target=view.target_visible=1u;
                    view.goal=AVP_MP_BOT_HOSTILE;
                    view.target_x=contact->x;view.target_y=contact->y;
                    view.target_is_alien=1u;
                    view.target_is_marine=view.target_is_stunned=0u;
                    view.target_energy_positive=alien->energy>0;
                    view.target_slot=i;
                    /* Preserve actual witnessed rescue priority when the
                     * close-threat selector considers another source AMP. */
                    view.other_player_alien_trapped=1u;
                    break;
                }
            }
        }
        /* R277: multiple close original Aliens are an immediate threat,
         * not separate harmless encounters. Count ONLY this bot's current
         * optical observations; ignore scanner-only blips and breadcrumbs.
         * A visible Alien within melee approach distance must be faced
         * ahead of an unrelated remembered enemy or routine pickup. */
        if(t->combat_test && species==AVP_MP_MARINE){
            uint64_t nearest=UINT64_MAX;
            unsigned close_slot=AVP_NUM_AMPS;
            unsigned rescue_selected=view.other_player_alien_trapped;
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *alien=&world->amp.actors[i];
                const AvpMpBotContact *seen=&t->contacts[p][i];
                if(!walk_bot_hostile(alien) ||
                   (alien->creature!=AC_ALIEN && alien->creature!=AC_ACRAWL) ||
                   !seen->sight)continue;
                const uint64_t distance=walk_bot_distance(view.x,view.y,
                    seen->x,seen->y);
                if(distance<((uint64_t)3u<<16)*((uint64_t)3u<<16))
                    view.close_alien_contacts++;
                if(seen->sight && distance<nearest &&
                   distance<((uint64_t)2u<<16)*((uint64_t)2u<<16)){
                    nearest=distance;close_slot=i;
                }
            }
            if(close_slot<AVP_NUM_AMPS && !rescue_selected &&
               (!view.hostile_target || !view.target_visible ||
                walk_bot_distance(view.x,view.y,view.target_x,view.target_y)>nearest)){
                const AvpAmp *alien=&world->amp.actors[close_slot];
                const AvpMpBotContact *seen=&t->contacts[p][close_slot];
                view.has_target=view.hostile_target=view.target_visible=1u;
                view.goal=AVP_MP_BOT_HOSTILE;
                view.target_x=seen->x;view.target_y=seen->y;
                view.target_is_alien=1u;view.target_is_marine=0u;
                view.target_is_stunned=0u;
                view.target_energy_positive=alien->energy>0;
                view.target_slot=close_slot;
            }
            /* Fight AND withdraw toward the closest real teammate when
             * outnumbered, critically injured, or struck near an Alien.
             * Prefer the companion farther from the observed main threat;
             * never chase a player through a locked door, on another
             * level, inside a vent transition, or already dead. */
            const int recently_hit=t->bots[p].damage_observed &&
                (view.player_energy<t->bots[p].last_observed_energy ||
                 tick<t->bots[p].damage_react_until);
            const int critical=view.max_energy>0 &&
                (int64_t)view.player_energy*100<(int64_t)view.max_energy*45;
            if(!t->dm.enabled && view.map.cells &&
               view.hostile_target && view.target_is_alien &&
               view.target_visible && (view.close_alien_contacts>=2u ||
                  (view.close_alien_contacts && (critical || recently_hit)))){
                uint64_t best_cost=UINT64_MAX;
                const uint64_t threat_distance=walk_bot_distance(view.x,view.y,
                    view.target_x,view.target_y);
                for(unsigned q=0;q<t->actors;q++)if(q!=p &&
                    avp_mp_session_player_world(&t->session,q)==here &&
                    !avp_mp_life_busy(&t->player_life[q]) &&
                    walk_vent_world_present(t,q)){
                    const AvpMpActorNativeState *friend=
                        (const AvpMpActorNativeState *)t->session.player[q];
                    AvpMpBotRoute refuge={0};
                    if(!friend || friend->player.player_energy<=0 ||
                       friend->player.game_over ||
                       friend->player.player_type==PT_ALIEN)continue;
                    const uint64_t separation=walk_bot_distance(view.x,view.y,
                        friend->player.x_pos,friend->player.y_pos);
                    if(separation<((uint64_t)1u<<16)*((uint64_t)1u<<16))continue;
                    const uint64_t ally_clearance=walk_bot_distance(
                        friend->player.x_pos,friend->player.y_pos,
                        view.target_x,view.target_y);
                    /* A teammate right beside the enemy isn't refuge. */
                    if(ally_clearance<=threat_distance+
                        ((uint64_t)32768u*32768u))continue;
                    if(!avp_mp_bot_route(&view.map,view.x>>16,view.y>>16,
                        friend->player.x_pos>>16,friend->player.y_pos>>16,
                        &refuge) || !refuge.valid ||
                        refuge.requires_door_use || refuge.cost>=best_cost)
                        continue;
                    best_cost=refuge.cost;
                    view.retreat_to_teammate=1u;
                    view.retreat_teammate_x=friend->player.x_pos;
                    view.retreat_teammate_y=friend->player.y_pos;
                }
            }
        }
        /* RE44 R288: a single-Alien "trapped player" probe can miss an
         * actual sandwich: each Alien leaves one mathematically clear escape
         * button, but that escape runs directly into the OTHER Alien.
         * Recognize only TWO currently sighted original
         * Aliens around a living same-world teammate; their locations must
         * already belong to this bot's observation (never omniscient AMP
         * positions). A visibly threatened bot also deserves aid, but
         * favor a HUMAN teammate when several players are endangered.
         * This is not a change to native collision, attacks or acid. */
        if(!t->dm.enabled && t->combat_test &&
           species==AVP_MP_MARINE && !(view.other_player_alien_trapped || view.other_player_swarmed) &&
           !(view.close_alien_contacts>=2u && view.target_visible)){
            unsigned choice=AVP_NUM_AMPS;
            for(unsigned pass=0u;pass<2u && choice>=AVP_NUM_AMPS;pass++)
            for(unsigned q=0u;q<t->actors;q++)if(q!=p &&
                (t->session.setup.bot_enabled[q]!=0u)==(pass!=0u) &&
                avp_mp_session_player_world(&t->session,q)==here &&
                !avp_mp_life_busy(&t->player_life[q]) &&
                walk_vent_world_present(t,q)){
                const AvpMpActorNativeState *friend=(const AvpMpActorNativeState *)
                    t->session.player[q];
                unsigned threats=0u,nearest=AVP_NUM_AMPS;
                uint64_t nearest_d=UINT64_MAX;
                if(!friend || friend->player.player_energy<=0 ||
                   friend->player.game_over ||
                   friend->player.player_type==PT_ALIEN)continue;
                for(unsigned i=0u;i<AVP_NUM_AMPS;i++){
                    const AvpAmp *alien=&world->amp.actors[i];
                    const AvpMpBotContact *seen=&t->contacts[p][i];
                    const uint64_t range=((uint64_t)98304u*98304u);
                    if(!walk_bot_hostile(alien) ||
                       (alien->creature!=AC_ALIEN &&
                        alien->creature!=AC_ACRAWL) ||
                       !seen->sight ||
                       walk_bot_distance(friend->player.x_pos,
                           friend->player.y_pos,seen->x,seen->y)>range)
                        continue;
                    threats++;
                    /* Prefer the nearest confirmed threat. */
                    const uint64_t d=walk_bot_distance(view.x,view.y,
                        seen->x,seen->y);
                    if(nearest>=AVP_NUM_AMPS ||
                       (seen->sight && !t->contacts[p][nearest].sight) ||
                       (seen->sight==t->contacts[p][nearest].sight &&
                        d<nearest_d)){
                        nearest=i;nearest_d=d;
                    }
                }
                if(threats>=2u && nearest<AVP_NUM_AMPS){
                    choice=nearest;
                    break;
                }
            }
            if(choice<AVP_NUM_AMPS){
                const AvpAmp *alien=&world->amp.actors[choice];
                const AvpMpBotContact *seen=&t->contacts[p][choice];
                view.other_player_swarmed=1u;
                view.has_target=view.hostile_target=1u;
                view.goal=AVP_MP_BOT_HOSTILE;
                view.target_slot=choice;
                view.target_x=seen->x;view.target_y=seen->y;
                view.target_visible=seen->sight!=0u;
                view.target_is_alien=1u;
                view.target_is_marine=view.target_is_stunned=0u;
                view.target_energy_positive=alien->energy>0;
            }
        }
        /* R278: defend living same-world companions from visually observed
         * stationary eggs, fast Facehuggers and ranged Predators. The original
         * egg has NO motion flag, so an egg behind a wall is not a scanner
         * contact. This priority is based on this bot's actual optical LOS,
         * not invisible enemy positions, and cannot override the R275
         * pinned-player rescue or R277 immediate multi-Alien self-defense. */
        if(!t->dm.enabled && t->combat_test &&
           species==AVP_MP_MARINE && !marine_empty &&
           !(view.other_player_alien_trapped || view.other_player_swarmed) &&
           !(view.close_alien_contacts>=2u && view.target_visible)){
            unsigned chosen=AVP_NUM_AMPS,priority=0u;
            uint64_t closest=UINT64_MAX;
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *enemy=&world->amp.actors[i];
                const AvpMpBotContact *seen=&t->contacts[p][i];
                uint64_t me_d2;
                if(!walk_bot_hostile(enemy) || !seen->sight)continue;
                me_d2=walk_bot_distance(view.x,view.y,seen->x,seen->y);
                for(unsigned q=0;q<t->actors;q++)if(q!=p &&
                    avp_mp_session_player_world(&t->session,q)==here &&
                    !avp_mp_life_busy(&t->player_life[q]) &&
                    walk_vent_world_present(t,q)){
                    const AvpMpActorNativeState *friend=
                        (const AvpMpActorNativeState *)t->session.player[q];
                    unsigned urgency;
                    if(!friend || friend->player.player_energy<=0 ||
                       friend->player.game_over ||
                       friend->player.player_type==PT_ALIEN)continue;
                    urgency=avp_mp_bot_guard_urgency(enemy->creature,
                        walk_bot_distance(friend->player.x_pos,
                            friend->player.y_pos,seen->x,seen->y),me_d2);
                    if(!urgency)continue;
                    /* Preserve current equally urgent target to prevent
                     * near-equal hostiles swapping on every scanner tick. */
                    if(urgency>priority ||
                       (urgency==priority && i==view.target_slot) ||
                       (urgency==priority && me_d2<closest &&
                        chosen!=view.target_slot)){
                        priority=urgency;closest=me_d2;chosen=i;
                    }
                }
            }
            if(chosen<AVP_NUM_AMPS){
                const AvpAmp *enemy=&world->amp.actors[chosen];
                const AvpMpBotContact *seen=&t->contacts[p][chosen];
                /* Immediate melee danger to THIS bot always wins over
                 * guarding a more distant teammate. */
                if(!(view.melee_visible &&
                    walk_bot_distance(view.x,view.y,view.melee_x,view.melee_y)<
                      (uint64_t)65536u*65536u)){
                    view.has_target=view.hostile_target=view.target_visible=1u;
                    view.goal=AVP_MP_BOT_HOSTILE;
                    view.target_x=seen->x;view.target_y=seen->y;
                    view.target_is_marine=enemy->creature==AC_HUMAN;
                    view.target_is_alien=enemy->creature==AC_ALIEN ||
                                         enemy->creature==AC_ACRAWL;
                    view.target_is_stunned=0u;
                    view.target_energy_positive=enemy->energy>0;
                    view.target_slot=chosen;
                    /* Do not abandon a real retreat route selected while
                     * escaping several Aliens for an unrelated guard target. */
                }
            }
        }
        /* The real current facing of an OPTICALLY SIGHTED opponent can
         * justify a Guerilla flank. Do not disclose a heading for old
         * breadcrumbs, motion-tracker contacts or other unseen actors. */
        if(view.behavior==AVP_MP_BOT_GUERILLA && view.hostile_target &&
           view.target_visible){
            if(view.target_slot<AVP_NUM_AMPS){
                const unsigned i=view.target_slot;
                const AvpAmp *target=&world->amp.actors[i];
                const AvpMpBotContact *seen=&t->contacts[p][i];
                if(seen->sight && seen->x==view.target_x &&
                   seen->y==view.target_y &&
                   seen->x==(u32)target->xpos && seen->y==(u32)target->ypos){
                    view.target_heading=(uint32_t)target->angle<<16;
                    view.target_heading_valid=1u;
                }
            }else if(view.target_slot<AVP_NUM_AMPS+t->actors){
                const unsigned q=view.target_slot-AVP_NUM_AMPS;
                const AvpMpActorNativeState *target=
                    (const AvpMpActorNativeState *)t->session.player[q];
                const AvpMpBotContact *seen=&t->contacts[p][view.target_slot];
                if(target && seen->sight && seen->x==view.target_x &&
                   seen->y==view.target_y &&
                   seen->x==target->player.x_pos &&
                   seen->y==target->player.y_pos){
                    view.target_heading=target->player.centre_angle;
                    view.target_heading_valid=1u;
                }
            }
        }
        /* R275: source enemy energy is read only for the FINAL, optically
         * visible target, never for stale breadcrumbs or tracker contacts. */
        if(view.hostile_target && view.target_visible &&
           view.target_slot<AVP_NUM_AMPS){
            const AvpAmp *chosen=&world->amp.actors[view.target_slot];
            if(walk_bot_hostile(chosen) &&
               (uint32_t)chosen->xpos==view.target_x &&
               (uint32_t)chosen->ypos==view.target_y){
                view.target_health=(int)chosen->energy;
                view.target_health_valid=1u;
                /* The source eggwait mode is the only reliable unopened
                 * state. Shotgun-only bots must not provoke it with an
                 * early hit, which skips its normal Facehugger delay. */
                if(chosen->creature==AC_EGG){
                    view.target_is_egg=1u;
                    view.target_egg_unopened=(chosen->mode==eggwait);
                }
            }
        }
        /* R283: at low health, prioritize a legitimate health pickup or
         * medical terminal even when an old tracker goal exists. Continue
         * immediate close-range fighting/rescue; do not run into an Alien's
         * melee reach just to use a panel. */
        if(!t->dm.enabled && view.health_low && view.map.cells &&
           species!=AVP_MP_ALIEN && !(view.other_player_alien_trapped || view.other_player_swarmed) &&
           (view.goal!=AVP_MP_BOT_HEALTH ||
            (t->bots[p].healing_valid && t->bots[p].healing_level==world->map->level)) &&
           view.goal!=AVP_MP_BOT_MEDICAL){
            const uint64_t danger=view.hostile_target && view.target_visible?
                 walk_bot_distance(view.x,view.y,view.target_x,view.target_y):
                 UINT64_MAX;
            const int close_facehugger=view.hostile_target &&
                view.target_visible && view.target_slot<AVP_NUM_AMPS &&
                world->amp.actors[view.target_slot].creature==AC_FACEHUG &&
                danger<=((uint64_t)4u<<16)*((uint64_t)4u<<16) &&
                !view.marine_unarmed;
            if(!close_facehugger &&
               danger>=((uint64_t)2u<<16)*((uint64_t)2u<<16)){
                AvpMpBotObservation to_health=view;
                if(walk_bot_choose_known_healing(t,p,a,world,&to_health)){
                    view=to_health;
                    healing_travel=view.goal==AVP_MP_BOT_EXIT || view.goal==AVP_MP_BOT_LIFT;
                    t->bots[p].tracker_seek_valid=0u;
                    t->bots[p].cover_flank_valid=0u;
                }
            }
        }
        /* Low ammunition is an EARLY survival signal, not merely an all-
         * weapons-empty state. Don't walk up to another hidden tracker enemy
         * when there is a reachable source ammo cache on a known level.
         * Prioritize current-level supply and urgent teammate rescue first.
         * For other-level pickups, the actual authored portal graph picks
         * the necessary duct/lift/airlock. Never travel by teleport. */
        if(!t->dm.enabled && species==AVP_MP_MARINE &&
           view.marine_low_ammo && view.map.cells && !healing_travel &&
           !(view.other_player_alien_trapped || view.other_player_swarmed) &&
           view.goal!=AVP_MP_BOT_SUPPLY && view.goal!=AVP_MP_BOT_HEALTH &&
           view.goal!=AVP_MP_BOT_MEDICAL){
            const uint64_t d2=view.hostile_target?
                walk_bot_distance(view.x,view.y,view.target_x,view.target_y):
                UINT64_MAX;
            const int close_facehugger=view.hostile_target &&
                view.target_visible && view.target_slot<AVP_NUM_AMPS &&
                world->amp.actors[view.target_slot].creature==AC_FACEHUG &&
                d2<=((uint64_t)4u<<16)*((uint64_t)4u<<16);
            const int melee_emergency=view.target_visible && !marine_empty &&
                (d2<((uint64_t)2u<<16)*((uint64_t)2u<<16) ||
                 close_facehugger);
            if(!melee_emergency){
                AvpMpBotObservation supply=view;
                if(walk_bot_choose_known_resupply(t,p,a,world,&supply)){
                    view=supply;
                    t->bots[p].tracker_seek_valid=0u;
                    t->bots[p].cover_flank_valid=0u;
                }
            }
        }
        /* If no accessible ammunition has been located, attempt a REAL
         * map route away from the nearest visually known/own-tracker threat.
         * Commit to the escape cell to avoid 10-Hz forward/back oscillation.
         * With no legal path, preserve safe standoff while other players
         * may rescue this Marine; never order a fake passage through props. */
        if(!t->dm.enabled && species==AVP_MP_MARINE &&
           view.marine_low_ammo && view.map.cells &&
           view.goal!=AVP_MP_BOT_SUPPLY && view.goal!=AVP_MP_BOT_HEALTH &&
           view.goal!=AVP_MP_BOT_MEDICAL &&
           view.goal!=AVP_MP_BOT_EXIT && view.goal!=AVP_MP_BOT_LIFT &&
           !view.retreat_to_teammate && !(view.other_player_alien_trapped || view.other_player_swarmed)){
            uint64_t distance=UINT64_MAX;
            unsigned threat_x=0u,threat_y=0u,seen=0u;
            AvpMpBotState *memory=&t->bots[p];
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *amp=&world->amp.actors[i];
                const AvpMpBotContact *c=&t->contacts[p][i];
                uint64_t d;
                if(!walk_bot_hostile(amp)||(!c->sight &&
                   (!c->tracker || !marine_empty)))continue;
                d=walk_bot_distance(view.x,view.y,c->x,c->y);
                if(d>=((uint64_t)6u<<16)*((uint64_t)6u<<16)||d>=distance)
                    continue;
                distance=d;seen=1u;threat_x=c->x>>16;threat_y=c->y>>16;
            }
            if(seen){
                if(memory->refuge_valid &&
                   (memory->refuge_level!=world->map->level ||
                    tick>=memory->refuge_until ||
                    ((view.x>>16)==memory->refuge_x &&
                     (view.y>>16)==memory->refuge_y)))
                    memory->refuge_valid=0u;
                if(!memory->refuge_valid){
                    unsigned gx,gy;
                    if(avp_mp_bot_pick_refuge(&view.map,view.x>>16,
                            view.y>>16,threat_x,threat_y,&gx,&gy)){
                        memory->refuge_valid=1u;
                        memory->refuge_level=world->map->level;
                        memory->refuge_x=gx;memory->refuge_y=gy;
                        memory->refuge_until=tick+32u;
                    }
                }
                if(memory->refuge_valid){
                    if(view.hostile_target && view.target_visible &&
                       !marine_empty){
                        view.retreat_to_refuge=1u;
                        view.retreat_refuge_x=
                           (memory->refuge_x<<16)|0x8000u;
                        view.retreat_refuge_y=
                           (memory->refuge_y<<16)|0x8000u;
                    }else{
                        view.goal=AVP_MP_BOT_PATROL;
                        view.has_target=1u;view.hostile_target=0u;
                        view.target_x=(memory->refuge_x<<16)|0x8000u;
                        view.target_y=(memory->refuge_y<<16)|0x8000u;
                        t->bots[p].tracker_seek_valid=0u;
                    }
                }
            }
        }
        /* Injured Marines should seek cover even when ammunition remains.
         * A critical injury overrides Berserker's normal rush behavior.
         * Reuse the source-legal escape waypoint, not fake invulnerability. */
        if(!t->dm.enabled && species==AVP_MP_MARINE &&
           view.health_critical && view.map.cells &&
           !(view.other_player_alien_trapped || view.other_player_swarmed) &&
           view.goal!=AVP_MP_BOT_HEALTH && view.goal!=AVP_MP_BOT_MEDICAL &&
           view.goal!=AVP_MP_BOT_EXIT && view.goal!=AVP_MP_BOT_LIFT &&
           !view.retreat_to_refuge){
            uint64_t nearest=UINT64_MAX;
            unsigned tx=0u,ty=0u;
            for(unsigned i=0u;i<AVP_NUM_AMPS;i++){
                const AvpMpBotContact *c=&t->contacts[p][i];
                uint64_t d;
                if(!walk_bot_hostile(&world->amp.actors[i]) ||
                   (!c->sight && (!c->tracker || !marine_empty)))continue;
                d=walk_bot_distance(view.x,view.y,c->x,c->y);
                if(d<nearest){nearest=d;tx=c->x>>16;ty=c->y>>16;}
            }
            if(nearest<((uint64_t)7u<<16)*((uint64_t)7u<<16)){
                unsigned rx,ry;
                if(avp_mp_bot_pick_refuge(&view.map,view.x>>16,view.y>>16,
                        tx,ty,&rx,&ry)){
                    if(view.hostile_target && view.target_visible){
                        view.retreat_to_refuge=1u;
                        view.retreat_refuge_x=(rx<<16)|0x8000u;
                        view.retreat_refuge_y=(ry<<16)|0x8000u;
                    }else{
                        view.goal=AVP_MP_BOT_PATROL;
                        view.has_target=1u;view.hostile_target=0u;
                        view.target_x=(rx<<16)|0x8000u;
                        view.target_y=(ry<<16)|0x8000u;
                    }
                }
            }
        }
        /* At critical health, a Marine with no known accessible recovery
         * point can flee through an AUTHORED vent/lift/airlock even when
         * it still has ammunition. Do not overwrite the current healing
         * destination, an urgent rescue, or a legitimate fighting escape. */
        if(!t->dm.enabled && species==AVP_MP_MARINE &&
           view.health_critical && view.combat_enemies_nearby &&
           view.map.cells && !(view.other_player_alien_trapped || view.other_player_swarmed) &&
           view.goal!=AVP_MP_BOT_HEALTH &&
           view.goal!=AVP_MP_BOT_MEDICAL &&
           view.goal!=AVP_MP_BOT_EXIT && view.goal!=AVP_MP_BOT_LIFT &&
           !t->bots[p].healing_valid){
            AvpMpBotObservation escape=view;
            if(walk_bot_emergency_exit(t,p,world,&escape)){
                view=escape;
                view.retreat_to_refuge=0u;
                t->bots[p].tracker_seek_valid=0u;
            }
        }
        /* With no rounds left, a safe room alone may merely delay death.
         * If there is no known supply route, try an actual original vent or
         * lift out of this threatened level. Do NOT do this in a quiet room,
         * while a reachable pickup exists, or during player rescue. */
        if(!t->dm.enabled && species==AVP_MP_MARINE &&
           view.marine_unarmed && view.map.cells &&
           view.combat_enemies_nearby &&
           !(view.other_player_alien_trapped || view.other_player_swarmed) &&
           view.goal!=AVP_MP_BOT_SUPPLY && view.goal!=AVP_MP_BOT_HEALTH &&
           view.goal!=AVP_MP_BOT_EXIT && view.goal!=AVP_MP_BOT_LIFT){
            AvpMpBotObservation escaping=view;
            if(walk_bot_emergency_exit(t,p,world,&escaping)){
                view=escaping;
                view.retreat_to_refuge=0u;
                view.retreat_to_teammate=0u;
                t->bots[p].tracker_seek_valid=0u;
            }
        }
        /* R473: scanner-only contacts never initiate interlevel travel.
         * A hidden contact behind an isolated region is not a mission goal.
         * Local search uses reachable cells; authored exits still serve real
         * teammate, supply, evacuation and other navigation goals. */
        /* Co-op boarding may interrupt routine travel or unseen remembered
         * contacts, but NEVER a current optical fight or a seen melee threat.
         * R275's unconditional invitation erased an already-selected
         * source Alien (including a pinned-player rescue), suppressing FIRE
         * even though the bot had line of sight. Actual elevator mechanics,
         * shared claims and native collision remain unchanged. */
        if(!t->dm.enabled && avp_mp_bot_lift_invitation_allowed(&view) &&
           /* Critical recovery travel must not be replaced by an unrelated
            * co-op elevator invitation at the next 10-Hz bot tick. */
           !(view.health_low &&
             (view.goal==AVP_MP_BOT_HEALTH ||
              view.goal==AVP_MP_BOT_MEDICAL ||
              view.goal==AVP_MP_BOT_EXIT || view.goal==AVP_MP_BOT_LIFT)) &&
           !(view.marine_low_ammo &&
             (view.goal==AVP_MP_BOT_EXIT || view.goal==AVP_MP_BOT_LIFT)))
            walk_bot_invite_lift(t,p,world,&view);
        if(view.marine_unarmed && !t->evacuation_active)
            walk_bot_empty_marine(t,p,a,world,&view,walk_bot_trail_los,world);
        walk_bot_coordinate_lift(t,p,world,&view,tick);
        /* R278 scanner-only tracking must not rewrite the movement goal
         * with a new hidden target coordinate each 10-Hz tick. Retain the
         * previous LIVE scanner contact and its waypoint for a short search
         * commitment. A visible fight, player rescue or elevator trip
         * immediately cancels the commitment. */
        {
            AvpMpBotState *memory=&t->bots[p];
            if(!view.marine_unarmed && view.goal==AVP_MP_BOT_HOSTILE && view.hostile_target &&
               !view.target_visible && !(view.other_player_alien_trapped || view.other_player_swarmed) &&
               !view.melee_visible && memory->tracker_seek_valid &&
               tick<memory->tracker_seek_until &&
               memory->tracker_seek_level==world->map->level &&
               memory->tracker_seek_slot<AVP_NUM_AMPS){
                const unsigned i=memory->tracker_seek_slot;
                const AvpMpBotContact *previous=&t->contacts[p][i];
                const AvpAmp *enemy=&world->amp.actors[i];
                if(previous->known && previous->tracker && !previous->sight &&
                   previous->serial==memory->tracker_seek_serial &&
                   !avp_mp_bot_tracker_contact_suppressed(memory,i,
                       world->map->level,previous->serial,
                       previous->x,previous->y,tick) &&
                   walk_bot_hostile(enemy)){
                    view.has_target=view.hostile_target=1u;
                    view.goal=AVP_MP_BOT_HOSTILE;
                    view.target_slot=i;
                    view.target_x=previous->x;view.target_y=previous->y;
                    view.target_is_alien=enemy->creature==AC_ALIEN ||
                                         enemy->creature==AC_ACRAWL;
                    view.target_is_marine=enemy->creature==AC_HUMAN;
                    view.target_is_stunned=0u;
                    view.target_energy_positive=enemy->energy>0;
                }
            }
            view.tracker_only_target=0u;
            if(view.goal==AVP_MP_BOT_HOSTILE && view.hostile_target &&
               !view.target_visible && view.target_slot<AVP_NUM_AMPS+AVP_LOCAL_MP_MAX_ACTORS){
                const AvpMpBotContact *seen=&t->contacts[p][view.target_slot];
                if(seen->tracker && !seen->sight){
                    view.tracker_only_target=1u;
                    avp_mp_bot_tracker_hold(memory,&view,seen->serial);
                    if(view.has_target && view.tracker_only_target && t->dm.enabled){
                        view.goal=AVP_MP_BOT_INVESTIGATE;
                        view.hostile_target=0u;
                    }
                    if(!view.has_target){
                        /* Abandoning a blocked radar objective must also
                         * cancel recovery spins from that old wall push.
                         * A FOLLOW goal should move on the very next tick. */
                        memory->evade_ticks=0u;
                        memory->immobile_ticks=0u;
                        memory->last_motion=0u;
                    }
                }
            }
            if(!view.tracker_only_target)
                avp_mp_bot_tracker_hold(memory,&view,0u);
            if(!view.has_target && !t->dm.enabled && !view.marine_unarmed){
                /* After discarding a stalled RADAR-ONLY contact, do not
                 * stand in place or wander past the co-op team. Return
                 * to ordinary same-level human following when reachable. */
                uint64_t follow_dist=UINT64_MAX;
                for(unsigned q=0;q<t->actors;q++)if(q!=p &&
                    !t->session.setup.bot_enabled[q] &&
                    !avp_mp_life_busy(&t->player_life[q]) &&
                    walk_vent_world_present(t,q) &&
                    avp_mp_session_player_world(&t->session,q)==here){
                    const AvpMpActorNativeState *leader=
                        (const AvpMpActorNativeState *)t->session.player[q];
                    uint64_t distance;
                    if(!leader || leader->player.player_energy<=0 ||
                       leader->player.game_over)continue;
                    distance=walk_bot_distance(view.x,view.y,
                        leader->player.x_pos,leader->player.y_pos);
                    if(distance>=follow_dist ||
                       !avp_mp_bot_follow_target_possible(species,here->level,
                         here->level,&view.map,view.x>>16,view.y>>16,
                         (u32)leader->player.x_pos>>16,
                         (u32)leader->player.y_pos>>16))continue;
                    follow_dist=distance;
                    view.has_target=1u;
                    view.goal=AVP_MP_BOT_FOLLOW;
                    view.hostile_target=0u;
                    view.target_x=leader->player.x_pos;
                    view.target_y=leader->player.y_pos;
                }
            }
            if(!view.has_target)
                walk_bot_patrol(t,p,&view,world->map->level);
        }
        /* Do not shoot a living enemy Alien if its ORIGINAL 96<<9
         * death spray could reach another non-Alien player. Read every
         * living actor in this same world (including bots), independent
         * of LOS, collision settings or friend steering. Do not use stale
         * contact positions or apply this to PvP Alien avatars. */
        if(view.hostile_target && view.target_is_alien &&
           view.target_slot<AVP_NUM_AMPS){
            const AvpAmp *target=&world->amp.actors[view.target_slot];
            if(walk_bot_hostile(target) &&
               (target->creature==AC_ALIEN || target->creature==AC_ACRAWL)){
                for(unsigned q=0;q<t->actors;q++)if(q!=p &&
                    avp_mp_session_player_world(&t->session,q)==here &&
                    !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q)){
                    const AvpMpActorNativeState *other=
                        (const AvpMpActorNativeState *)t->session.player[q];
                    if(!other || other->player.player_energy<=0 ||
                       other->player.game_over ||
                       other->player.player_type==PT_ALIEN)continue;
                    if(avp_mp_bot_death_spray_near_player(
                           (uint32_t)target->xpos,(uint32_t)target->ypos,
                           other->player.x_pos,other->player.y_pos)){
                        view.other_player_splash_unsafe=1u;
                        if(avp_mp_bot_alien_traps_player(&view.map,
                            (uint32_t)target->xpos,(uint32_t)target->ypos,
                            other->player.x_pos,other->player.y_pos))
                            view.other_player_alien_trapped=1u;
                        /* Inspect all endangered players; a second one
                         * might be pinned even if the first is mobile. */
                    }
                }
            }
        }
        /* R275: Allow *nonlethal* weakening before luring a source Alien
         * away from a one-cell corridor. Never permit this for projectiles:
         * their delayed/explosive damage cannot be predicted here. Use the
         * original loaded Marine hitscan definitions (shotgun 10, pulse 5,
         * smartgun 60) rather than guessed weapon power. The two-shot headroom
         * reserves for an already animating shot; same-tick bot reservations
         * prevent a pack of bots from all consuming the same HP margin.
         * No source weapon/damage/acid or human input is modified. */
        unsigned choke_wanted=0u;
        unsigned choke_slot=AVP_NUM_AMPS;
        if(!t->dm.enabled && view.goal==AVP_MP_BOT_HOSTILE &&
           view.target_visible && view.target_is_alien &&
           view.target_slot<AVP_NUM_AMPS && view.map.cells &&
           view.player_species==AVP_MP_MARINE &&
           !avp_mp_bot_alien_death_passable(&view.map,
               view.target_x,view.target_y) &&
           (view.current_weapon==1u || view.current_weapon==2u ||
            view.current_weapon==4u) &&
           (view.marine_owned&(1u<<view.current_weapon)) &&
           view.marine_ammo[view.current_weapon-1u]){
            const AvpAmp *alien=&world->amp.actors[view.target_slot];
            const AvpWeaponDef *weapon=avp_weapon_def(PT_HUMAN,
                                                  (s16)view.current_weapon);
            if(walk_bot_hostile(alien) &&
               (alien->creature==AC_ALIEN || alien->creature==AC_ACRAWL) &&
               weapon && weapon->damage>0){
                const unsigned damage=(unsigned)weapon->damage;
                /* Strict > leaves at least one real HP after the expected
                 * shot and two further scheduled/straggler shots. */
                if(avp_mp_bot_weakening_allowed(alien->energy,damage,
                    choke_reserved[view.target_slot])){
                    view.nonlethal_choke_fire=1u;
                    choke_wanted=damage;
                    choke_slot=view.target_slot;
                }
            }
        }
        /* R277: Tables can hide an observed Alien's firing lane even
         * though its map CELL is reachable. Pick an actual floor cell with
         * original source LOS and an unobstructed collmap-prop line, then
         * navigate there without discarding the hostile contact or aim.
         * Only observed/tracked enemy coordinates are passed to this probe.
         * Cache per-target waypoints to avoid 10-Hz path oscillation. */
        if(view.player_species==AVP_MP_MARINE && !view.marine_unarmed &&
           view.goal==AVP_MP_BOT_HOSTILE && view.hostile_target &&
           view.target_is_alien && view.target_slot<AVP_NUM_AMPS &&
           view.map.cells && !view.retreat_to_teammate &&
           !view.close_alien_contacts){
            AvpMpBotState *memory=&t->bots[p];
            WalkBotLaneProbe probe={world,view.target_slot};
            const int clear=view.target_visible &&
                walk_bot_firing_lane_probe(&probe,view.x,view.y,
                                           view.target_x,view.target_y);
            if(clear){
                memory->cover_flank_valid=0u;
                memory->cover_retry_tick=0u;
            }else{
                if(memory->cover_flank_valid &&
                   (memory->cover_flank_slot!=view.target_slot ||
                    memory->cover_flank_level!=world->map->level ||
                    ((view.x>>16)==memory->cover_flank_x &&
                     (view.y>>16)==memory->cover_flank_y &&
                     llabs((int64_t)view.x-(((int64_t)memory->cover_flank_x<<16)+0x8000))<11000 &&
                     llabs((int64_t)view.y-(((int64_t)memory->cover_flank_y<<16)+0x8000))<11000 &&
                     tick+12u>=memory->cover_flank_until) ||
                    tick>=memory->cover_flank_until ||
                    (memory->immobile_ticks>=5u &&
                     tick+10u>=memory->cover_flank_until)))
                    memory->cover_flank_valid=0u;
                if(!memory->cover_flank_valid && tick>=memory->cover_retry_tick){
                    unsigned fx,fy;
                    if(avp_mp_bot_pick_firing_lane(&view.map,view.x,view.y,
                            view.target_x,view.target_y,
                            walk_bot_firing_lane_probe,&probe,&fx,&fy)){
                        memory->cover_flank_valid=1u;
                        memory->cover_flank_slot=view.target_slot;
                        memory->cover_flank_level=world->map->level;
                        memory->cover_flank_x=fx;
                        memory->cover_flank_y=fy;
                        memory->cover_flank_until=tick+35u;
                    }
                    /* A target behind a wall cannot force a full search
                     * every original tick, including with twelve bots. */
                    memory->cover_retry_tick=tick+15u;
                }
                if(memory->cover_flank_valid){
                    view.cover_flank_valid=1u;
                    view.cover_flank_x=memory->cover_flank_x;
                    view.cover_flank_y=memory->cover_flank_y;
                }
            }
        }else t->bots[p].cover_flank_valid=0u;
        /* A Marine's original motion tracker informs WHERE to investigate,
         * never WHO is in clear sight. No firing, melee targeting, damage
         * estimates or combat stance based solely on a scanner blip or old
         * optical memory. Revalidate original LOS/FOV every observation;
         * visually confirmed targets remain HOSTILE without interruption.
         * Keep R290's held waypoints and unreachable-contact watchdog. */
        avp_mp_bot_coop_contact_as_memory(&view);
        /* R295: co-op formation is evaluated after rescue, cover, scanner,
         * native elevator and survival decisions. Prefer a living human
         * leader, otherwise the lowest-index living bot. Only real player
         * state and original reachable routes are used. A failed direct
         * route may use a real authored vent/lift hop, never a teleport.
         * Optional distant loot and unseen hostiles no longer outrank the
         * squad. Nearby ammo rooms remain valid short excursions. */
        if(!t->dm.enabled && view.map.cells){
            unsigned anchor=t->actors;
            unsigned anchor_is_human=0u;
            for(unsigned q=0u;q<t->actors;q++)if(q!=p &&
                !avp_mp_life_busy(&t->player_life[q]) &&
                walk_vent_world_present(t,q)){
                const AvpMpActorNativeState *peer=
                    (const AvpMpActorNativeState *)t->session.player[q];
                if(!peer || peer->player.player_energy<=0 ||
                   peer->player.game_over)continue;
                if(!t->session.setup.bot_enabled[q]){
                    anchor=q;anchor_is_human=1u;break;
                }
                if(!anchor_is_human && anchor==t->actors)anchor=q;
            }
            if(!anchor_is_human){
                unsigned chosen=walk_bot_evacuation_leader(t);
                t->evacuation_leader=chosen;
                if(chosen<t->actors)anchor=chosen!=p?chosen:t->actors;
            }
            if(anchor<t->actors){
                const AvpMpActorNativeState *leader=
                    (const AvpMpActorNativeState *)t->session.player[anchor];
                const AvpMpActiveWorld *leader_world=
                    avp_mp_session_player_world(&t->session,anchor);
                const uint64_t separation=walk_bot_distance(view.x,view.y,
                    leader->player.x_pos,leader->player.y_pos);
                const uint64_t objective=view.has_target?
                    walk_bot_distance(view.target_x,view.target_y,
                        leader->player.x_pos,leader->player.y_pos):0u;
                unsigned empty_supply=0u;
                if(view.goal==AVP_MP_BOT_SUPPLY &&
                   view.target_slot<AVP_NUM_AMPS){
                    const AvpAmp *stock=&world->amp.actors[view.target_slot];
                    s16 kind=COLL_NONE,amount=0;
                    unsigned weapon=0u;
                    if(avp_collect_peek(world->map->level,
                         (u32)stock->xpos>>16,(u32)stock->ypos>>16,
                         stock->astype,&kind,&amount)){
                        switch(kind){
                        case COLL_SHOTAMMO:weapon=1u;break;
                        case COLL_PULSEAMMO:weapon=2u;break;
                        case COLL_FLAMEAMMO:weapon=3u;break;
                        case COLL_SMARTAMMO:weapon=4u;break;
                        default:break;
                        }
                        /* The original Marine can collect rounds into a
                         * completely empty reserve without owning its gun.
                         * An actual authored cache like the Brig pulse-ammo
                         * corpse is a needed supply, not optional wandering. */
                        if(weapon && !view.marine_ammo[weapon-1u])
                            empty_supply=1u;
                    }
                }
                if(leader_world &&
                   avp_mp_bot_team_regroup_priority(&view,separation,
                                                     objective,empty_supply)){
                    AvpMpBotObservation together=view;
                    int reachable=0;
                    if(leader_world==here){
                        if(avp_mp_bot_follow_target_possible(species,here->level,
                              here->level,&view.map,view.x>>16,view.y>>16,
                              leader->player.x_pos>>16,leader->player.y_pos>>16)){
                            together.goal=AVP_MP_BOT_FOLLOW;
                            together.has_target=1u;
                            together.hostile_target=0u;
                            together.target_x=leader->player.x_pos;
                            together.target_y=leader->player.y_pos;
                            reachable=1;
                        }
                    }else if(world->doors.level_meta &&
                             world->doors.level_meta_count>=AVP_MAX_LEVEL){
                        reachable=walk_bot_choose_level_route(t,p,world,
                            leader_world->level,species,
                            view.x>>16,view.y>>16,&view.map,&together,
                            leader->player.x_pos>>16,
                            leader->player.y_pos>>16,1);
                    }
                    if(reachable){
                        view=together;
                        t->bots[p].tracker_seek_valid=0u;
                    }
                }
            }
        }
        if(t->evacuation_active && species==AVP_MP_MARINE && !t->dm.enabled){
            const uint64_t close=(uint64_t)(3u<<16)*(uint64_t)(3u<<16);
            const unsigned urgent_hostile=view.goal==AVP_MP_BOT_HOSTILE &&
                view.hostile_target && view.target_visible &&
                walk_bot_distance(view.x,view.y,
                                  view.target_x,view.target_y)<=close;
            /* Evacuate first; turn and fight only verified enemies directly
             * blocking this bot's departure. No radar-only hostile fire. */
            view.evacuation_active=1u;
            if(!urgent_hostile) {
                if(walk_bot_evacuation_goal(t,p,world,species,&view)){
                    t->bots[p].tracker_seek_valid=0u;
                    t->bots[p].cover_flank_valid=0u;
                }
            }
        }
        if(!t->evacuation_active || view.goal!=AVP_MP_BOT_ESCAPE_SEAT)
            walk_bot_coordinate_airlock(t,p,world,&view);
        /* R497: a tactically blocked travel route is not a generic patrol
         * retry. Preserve normal panel/arrival/airlock handling. */
        if(!t->lift_arrival_pending[p] && avp_mp_bot_tactical_travel(&t->bots[p],&view)){
            t->bots[p].travel_valid=t->bots[p].resupply_valid=0u;
            t->bots[p].tracker_seek_valid=0u;
        }
        /* R484: a short source-reachable patrol retry after persistent
         * inactivity. Do not replace combat, airlock or lift egress goals. */
        if(avp_mp_bot_objective_recoverable(&view) && !t->lift_arrival_pending[p]){
            AvpMpBotState *memory=&t->bots[p];
            if(memory->physical_valid && memory->physical_level==world->map->level &&
               tick==memory->physical_tick+1u &&
               (memory->objective_stall>=20u || avp_mp_bot_loop_ready(memory,&view)) &&
               tick>=memory->objective_recovery_until){
                if(avp_mp_bot_loop_ready(memory,&view)){
                    if(memory->loop_recoveries<UINT32_MAX)memory->loop_recoveries++;
                    memory->loop_valid=memory->loop_ready=0u;
                }
                memory->objective_recovery_until=tick+30u;
                memory->objective_stall=0u;
                memory->nav_retarget_patrol=1u;
                memory->travel_valid=0u;
                memory->resupply_valid=0u;
                memory->healing_valid=0u;
                memory->tracker_seek_valid=0u;
            }
            if(memory->physical_level==world->map->level &&
               tick==memory->physical_tick+1u && tick<memory->objective_recovery_until){
                AvpMpBotObservation retry=view;
                retry.has_target=retry.hostile_target=retry.target_visible=0u;
                retry.cover_flank_valid=retry.retreat_to_refuge=retry.retreat_to_teammate=0u;
                walk_bot_patrol(t,p,&retry,world->map->level);
                if(retry.has_target)view=retry;
            }
        }
        /* R487: an objective behind a remembered choke is not a movement
         * retry. The early risk grid filters candidate scans; this final
         * check catches cached objectives or explicit portal overrides. */
        if(!t->lift_arrival_pending[p] &&
           (avp_mp_bot_acid_goal_unsafe(&t->bots[p],&view) ||
            (acid_chokes && avp_mp_bot_acid_goal_blocked(&view)))){
            AvpMpBotObservation retry=view;
            AvpMpBotState *memory=&t->bots[p];
            acid_retarget=1u;
            memory->travel_valid=memory->resupply_valid=memory->healing_valid=0u;
            memory->tracker_seek_valid=0u;
            /* Do not permanently blacklist a destination for temporary acid. */
            if(t->patrol[p].valid){
                AvpMpBotObservation previous=view;
                previous.goal=AVP_MP_BOT_PATROL;
                previous.target_x=t->patrol[p].x;previous.target_y=t->patrol[p].y;
                if(avp_mp_bot_acid_goal_unsafe(memory,&previous) ||
                   avp_mp_bot_acid_goal_blocked(&previous))t->patrol[p].valid=0u;
            }
            retry.has_target=retry.hostile_target=retry.target_visible=0u;
            retry.cover_flank_valid=retry.retreat_to_refuge=retry.retreat_to_teammate=0u;
            walk_bot_patrol(t,p,&retry,world->map->level);
            if(!retry.has_target){
                /* No safe-side patrol floor: hold here and rescan rather
                 * than issue arbitrary motion toward the protected choke. */
                retry.has_target=1u;retry.goal=AVP_MP_BOT_PATROL;
                retry.target_x=retry.x;retry.target_y=retry.y;
            }
            view=retry;
        }
        /* Commit only final pickup targets, not discarded candidate AMPs.
         * Other goals never inherit a stale pickup failure timer. */
        if(view.has_target &&
           (view.goal==AVP_MP_BOT_SUPPLY || view.goal==AVP_MP_BOT_HEALTH) &&
           view.target_slot<AVP_NUM_AMPS &&
           (uint32_t)world->amp.actors[view.target_slot].xpos==view.target_x &&
           (uint32_t)world->amp.actors[view.target_slot].ypos==view.target_y &&
           walk_mp_item_available(t,p,world->map->level,view.target_slot,
                                  &world->amp.actors[view.target_slot])){
            AvpMpBotRoute path={0};
            uint32_t cost=UINT32_MAX;
            if(view.map.cells && avp_mp_bot_route(&view.map,
                view.x>>16,view.y>>16,view.target_x>>16,view.target_y>>16,&path))
                cost=path.cost;
            avp_mp_bot_pickup_track(&t->pickup_memory[p],world->map->level,
                view.target_slot,world->amp.slot_generation[view.target_slot],
                tick,view.x,view.y,view.target_x,view.target_y,cost,
                (view.y>>16)*64u+(view.x>>16));
        }else if(view.has_target && view.goal==AVP_MP_BOT_SUPPLY &&
                view.target_slot>=WALK_BOT_CORPSE_SLOT_BASE &&
                walk_bot_corpse_available(t,p,world->map->level,
                    view.target_slot-WALK_BOT_CORPSE_SLOT_BASE)){
            const AvpMpCorpse *c=&t->corpses.records[view.target_slot-WALK_BOT_CORPSE_SLOT_BASE];
            AvpMpBotRoute path={0};
            uint32_t cost=UINT32_MAX;
            if(avp_mp_bot_route(&view.map,view.x>>16,view.y>>16,
                view.target_x>>16,view.target_y>>16,&path))cost=path.cost;
            avp_mp_bot_pickup_track(&t->pickup_memory[p],world->map->level,
                view.target_slot,(unsigned)c->death_tick,tick,view.x,view.y,
                view.target_x,view.target_y,cost,(view.y>>16)*64u+(view.x>>16));
        }else t->pickup_memory[p].active=0u;
        /* Only final, currently optically observed egg receives watchdog
         * updates. A reused AMP slot must never inherit the old cooldown. */
        avp_mp_bot_egg_watch(&t->bots[p],&view,
            view.target_slot<AVP_NUM_AMPS?world->amp.slot_generation[view.target_slot]:0u);
        avp_mp_bot_step_precise(&t->bots[p],&view,a->player.max_avel,&cmds[p]);
        /* PRIVATE R292 field-test-only diagnostics, NOT a release change. */
        if(getenv("AVP_BOT_PLAYTEST_TRACE") && (tick%5u==0u || getenv("AVP_BOT_TRACE_EVERY_TICK")))
            fprintf(stderr,"BOTTRACE tick=%llu p=%u level=%u xy=%08x/%08x hp=%d goal=%u has=%u hostile=%u visible=%u radar=%u target=%u txy=%08x/%08x acts=%08x stuck=%u acid=%u lowammo=%u unarmed=%u ammo=%u,%u,%u,%u follower=%u physical_stall=%u analog=%u/%g/%g doors=%u/%u/%u dest=%u duct=%u idle=%u route=%u wait=%u hug=%u liftwait=%u recovery=%u acid_contact=%u acid_exits=%u acid_returns_blocked=%u acid_chokes=%u acid_retarget=%u loop_ready=%u loop_recoveries=%u peer_melee_align=%u peer_unblock=%u species=%u heading=%08x peer=%u/%u pxy=%08x/%08x props=%u tactical_idle=%u tactical_retargets=%u tactical_active=%u melee=%u/%u mxy=%08x/%08x cloak=%u/%u/%u decloak_wait=%u vent_cross=%u\n",
                (unsigned long long)tick,p+1u,world->map->level,
                view.x,view.y,view.player_energy,(unsigned)view.goal,
                view.has_target,view.hostile_target,view.target_visible,
                view.tracker_only_target,view.target_slot,view.target_x,view.target_y,
                (unsigned)cmds[p].actions,t->bots[p].immobile_ticks,
                view.acid_count,view.marine_low_ammo,view.marine_unarmed,
                view.marine_ammo[0],view.marine_ammo[1],view.marine_ammo[2],view.marine_ammo[3],
                t->bots[p].follow_backoff_active,t->bots[p].physical_stall,
                cmds[p].analog_axis_mask,(double)cmds[p].analog_forward,(double)cmds[p].analog_strafe,
                view.active_door_count,view.active_door_offsets[0],view.active_door_offsets[1],
                view.destination_level,view.exit_is_duct,t->bots[p].objective_stall,
                t->bots[p].objective_route,t->bots[p].objective_wait,view.facehugged,
                view.lift_wait,(unsigned)(tick<t->bots[p].objective_recovery_until),
                t->bots[p].acid_contact_count,t->bots[p].acid_exit_count,t->bots[p].acid_return_blocks,acid_chokes,acid_retarget,
                t->bots[p].loop_ready,t->bots[p].loop_recoveries,t->bots[p].peer_melee_align,t->bots[p].peer_unblock,
                view.player_species,view.heading,view.nearby_peer_valid,view.nearby_peer_index,
                view.nearby_peer_x,view.nearby_peer_y,view.prop_count,
                t->bots[p].tactical_idle,t->bots[p].tactical_retargets,
                (unsigned)(tick<t->bots[p].tactical_until),
                view.melee_threat,view.melee_visible,view.melee_x,view.melee_y,
                view.invis_act,view.invis_stat,view.invisflag,
                t->bots[p].predator_decloak_wait,(unsigned)(tick<t->bots[p].vent_cross_until));
        /* R500: retain nearby geometry for reproducible stalled-bot replays.
         * Opt-in only; no game-state writes and no extra route search. */
        if(getenv("AVP_BOT_TRACE_GEOMETRY") && tick%10u==0u &&
           view.marine_unarmed && view.melee_visible &&
           (!cmds[p].actions || tick<t->bots[p].tactical_until)){
            fprintf(stderr,"BOTGEOM tick=%llu p=%u level=%u xy=%08x/%08x acid=",
                (unsigned long long)tick,p+1u,world->map->level,view.x,view.y);
            for(unsigned i=0;i<view.acid_count;i++){
                if(llabs((long long)view.acid[i].x-view.x)>3LL*65536 ||
                   llabs((long long)view.acid[i].y-view.y)>3LL*65536)continue;
                fprintf(stderr,"%08x/%08x,",view.acid[i].x,view.acid[i].y);
            }
            fprintf(stderr," props=");
            for(unsigned i=0;i<view.prop_count;i++)fprintf(stderr,"%08x/%08x/%u,",
                view.props[i].x,view.props[i].y,view.props[i].halfwidth);
            fprintf(stderr," doors=");
            for(unsigned i=0;i<view.active_door_count && i<2u;i++){
                unsigned off=view.active_door_offsets[i];
                if(view.map.cells && off<view.map.width*view.map.height*8u)
                    fprintf(stderr,"%u/%u,",off,view.map.cells[off]);
            }
            fprintf(stderr,"\n");
        }
        if(choke_slot<AVP_NUM_AMPS &&
           (cmds[p].actions&(1u<<AVP_MP_FIRE)))
            choke_reserved[choke_slot]+=choke_wanted;
        t->last_bot_world_level[p]=world->map->level;
        t->last_bot_goal_lift[p]=view.goal==AVP_MP_BOT_LIFT;
    }
}

/* One Deathmatch damage path for native hits and native projectiles.
 * During AMP update, a single target actor may be scoped into original
 * PLAYER.S globals: write that target to player_energy so the subsequent
 * actor scope capture cannot overwrite its PvP damage. Everyone else is
 * updated in their saved individual snapshot. */
static void walk_dm_apply_hit(AvpMpWalkHost *t,unsigned shooter,
                              unsigned target,int damage,
                              unsigned attacker_predator_uncloaked)
{
    AvpMpActorNativeState *victim;
    int prior_energy,new_energy;
    AvpNewHealthMode mode;
    int scoped;
    if(!t||!t->dm.enabled||t->dm.finished||shooter>=t->actors||
       target>=t->actors||damage<=0||t->dm_death_credited[target]||
       (avp_mp_life_busy(&t->player_life[target]) ||
        !walk_vent_world_present(t,target)))return;
    victim=(AvpMpActorNativeState *)t->session.player[target];
    if(!victim)return;
    scoped=avp_mp_actor_scope_active() && t->dm_amp_target==target;
    prior_energy=scoped?player_energy:victim->player.player_energy;
    if(prior_energy<=0)return;
    mode=target<t->players?avp_pc_video_new_health_mode(t->video,target):AVP_NEW_HEALTH_OFF;
    if(mode==AVP_NEW_HEALTH_INVINCIBLE)return;
    /* Never modify original AMP/enemy hits. Both optional modifiers are
     * applied only after we have selected another living player avatar. */
    damage=avp_mp_dm_predator_attacker_damage(
        t->dm.predator_uncloaked_damage,
        shooter!=target && attacker_predator_uncloaked,damage);
    damage=avp_mp_dm_victim_damage(t->dm.cloak_damage,
        victim->player.player_type,
        (unsigned)(scoped?invisflag:victim->player.invisflag),damage);
    /* Both independent 4x modifiers may be active; avoid signed overflow. */
    {int64_t remaining=(int64_t)prior_energy-(int64_t)damage;
     new_energy=remaining<-32768?-32768:(int)remaining;}
    if(scoped)player_energy=(s16)new_energy;
    else victim->player.player_energy=(s16)new_energy;
    if(mode==AVP_NEW_HEALTH_INFINITE){
        if(scoped)avp_new_health_apply(mode,&player_energy,&old_energy,max_energy);
        else avp_new_health_apply(mode,&victim->player.player_energy,&victim->player.old_energy,victim->player.max_energy);
        return;
    }
    if(new_energy<=0 && !(target<t->players && avp_pc_video_original_god_player(t->video,target))){
        avp_mp_dm_kill(&t->dm,shooter,target);
        t->dm_death_credited[target]=1u;
        /* Scored PvP event, once per death: show each human participant
         * their own message. Bot IDs are retained up to player #36. */
        avp_mp_kill_notice_record(t->dm_notices,t->players,t->actors,
                                  shooter,target,t->session.tick);
        fprintf(stderr,"Deathmatch: P%u eliminated P%u, score=%u (P%u deaths=%u).\n",
            shooter+1u,target+1u,t->dm.kills[shooter],target+1u,
            t->dm.deaths[target]);
    }
}
static int walk_dm_projectile_slot(const AvpAmp *a,unsigned *level,unsigned *slot)
{
    uintptr_t base=(uintptr_t)(const void *)amp_data;
    uintptr_t addr=(uintptr_t)(const void *)a;
    uintptr_t off;
    if(!a || cur_level<1||(unsigned)cur_level>AVP_MP_RETAIL_LEVELS ||
       addr<base || addr-base>=sizeof(AvpAmp)*AVP_NUM_AMPS)return 0;
    off=addr-base;
    if(off%sizeof(AvpAmp))return 0;
    *level=(unsigned)cur_level-1u;
    *slot=(unsigned)(off/sizeof(AvpAmp));
    return 1;
}
static int walk_dm_projectile_owner(AvpMpWalkHost *t,const AvpAmp *a)
{
    unsigned level,slot;
    if(!t||!t->dm.enabled||!walk_dm_projectile_slot(a,&level,&slot) ||
       t->dm_projectile_gen[level][slot]!=avp_amp_slot_generation(slot))return -1;
    return t->dm_projectile_owner[level][slot] ?
        (int)t->dm_projectile_owner[level][slot]-1:-1;
}
static void walk_dm_register_projectile(AvpMpWalkHost *t,const AvpAmp *a,
                                        unsigned owner,unsigned predator_uncloaked)
{
    unsigned level,slot;
    if(!t||owner>=t->actors||!walk_dm_projectile_slot(a,&level,&slot))return;
    t->dm_projectile_owner[level][slot]=(uint8_t)(owner+1u);
    t->dm_projectile_predator_uncloaked[level][slot]=(uint8_t)!!predator_uncloaked;
    t->dm_projectile_gen[level][slot]=avp_amp_slot_generation(slot);
}
static void walk_dm_projectile_spawn(void *user,const AvpAmp *a)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)user;
    if(t && a && t->dm_shooter<t->actors &&
       (a->flags&(1u<<AMP_PLAYER)))
        walk_dm_register_projectile(t,a,t->dm_shooter,
            player_type==PT_PREDATOR && !invisflag);
}
static void walk_dm_explosion_spawn(void *user,const AvpAmp *bolt,const AvpAmp *explosion_amp)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)user;
    int owner=walk_dm_projectile_owner(t,bolt);
    if(owner>=0){
        unsigned level,slot,uncloaked=0u;
        if(walk_dm_projectile_slot(bolt,&level,&slot))
            uncloaked=t->dm_projectile_predator_uncloaked[level][slot];
        walk_dm_register_projectile(t,explosion_amp,(unsigned)owner,uncloaked);
    }
}
static int walk_dm_projectile_hit(void *user,const AvpAmp *a)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)user;
    int owner=walk_dm_projectile_owner(t,a);
    unsigned best=AVP_LOCAL_MP_MAX_ACTORS;
    uint64_t best_distance=UINT64_MAX;
    int damage;
    if(!t||owner<0||t->dm.finished ||
       !(damage=avp_mp_dm_projectile_damage(a->creature)))return 0;
    /* Projectiles stay in their native world even if their shooter exits
     * that level before impact. Source world/slot/generation is authoritative. */
    for(unsigned q=0;q<t->actors;q++)if(q!=(unsigned)owner &&
        avp_mp_session_player_world(&t->session,q) &&
        avp_mp_session_player_world(&t->session,q)->level==(unsigned)cur_level &&
        !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q)){
        const AvpMpActorNativeState *victim=(const AvpMpActorNativeState *)t->session.player[q];
        uint64_t dx,dy,dist;
        if(!victim||victim->player.player_energy<=0||
           t->dm_death_credited[q])continue;
        dx=(uint64_t)llabs((long long)a->xpos-(long long)(s32)victim->player.x_pos);
        dy=(uint64_t)llabs((long long)a->ypos-(long long)(s32)victim->player.y_pos);
        if(dx>(40u<<9)||dy>(40u<<9))continue;
        dist=dx>dy?dx:dy;
        if(dist>=best_distance)continue;
        best=q;best_distance=dist;
    }
    if(best==AVP_LOCAL_MP_MAX_ACTORS)return 0;
    {unsigned level,slot,uncloaked=0u;
     if(walk_dm_projectile_slot(a,&level,&slot))
         uncloaked=t->dm_projectile_predator_uncloaked[level][slot];
     walk_dm_apply_hit(t,(unsigned)owner,best,damage,uncloaked);}
    return 1;
}
static int walk_dm_explosion_damage(void *user,const AvpAmp *a)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)user;
    int owner=walk_dm_projectile_owner(t,a);
    if(!t||owner<0)return 0;
    for(unsigned q=0;q<t->actors;q++)if(
        avp_mp_session_player_world(&t->session,q) &&
        avp_mp_session_player_world(&t->session,q)->level==(unsigned)cur_level &&
        !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q)){
        const AvpMpActorNativeState *v=(const AvpMpActorNativeState *)t->session.player[q];
        AvpXY centre={a->xpos,a->ypos},to;
        if(!v||v->player.player_energy<=0||t->dm_death_credited[q])continue;
        if(!avp_mp_dm_explosion_candidate((s32)v->player.x_pos-a->xpos,
                (s32)v->player.y_pos-a->ypos,(u32)a->timer))continue;
        to.x=(s32)v->player.x_pos;to.y=(s32)v->player.y_pos;
        if(LineOfSight(&to,&centre))continue;
        {unsigned level,slot,uncloaked=0u;
         if(walk_dm_projectile_slot(a,&level,&slot))
             uncloaked=t->dm_projectile_predator_uncloaked[level][slot];
         walk_dm_apply_hit(t,(unsigned)owner,q,200,uncloaked);}
    }
    return 1; /* apply native AreaAmpDamage; skip original current-player damage */
}

/* TestSpark owns original random spread, FireDistance and closest AMP.
 * The PC-local hook tests other living player avatars against those SAME
 * native shot parameters. No shots are synthesized and co-op never registers
 * this hook. Its victim's native player snapshot takes damage; the victim's
 * next original Pain()/death tick owns the normal sounds and fade. */
static int walk_dm_shot(void *user,s32 origin_x,s32 origin_y,
    s32 boxx,s32 boxy,s16 cs,s16 sn,u16 width,u32 closest,s16 damage)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)user;
    const unsigned shooter=t?t->dm_shooter:AVP_LOCAL_MP_MAX_ACTORS;
    const AvpMpActiveWorld *here;
    unsigned selected=AVP_LOCAL_MP_MAX_ACTORS;
    uint32_t nearest=closest;
    if(!t || !t->dm.enabled || t->dm.finished || shooter>=t->actors ||
       damage<=0)return 0;
    here=avp_mp_session_player_world(&t->session,shooter);
    if(!here)return 0;
    for(unsigned q=0;q<t->actors;q++)if(q!=shooter &&
        avp_mp_session_player_world(&t->session,q)==here &&
        !avp_mp_life_busy(&t->player_life[q]) && walk_vent_world_present(t,q)){
        AvpMpActorNativeState *a=(AvpMpActorNativeState *)t->session.player[q];
        AvpXY from,to;
        uint32_t depth=0;
        if(!a || a->player.player_energy<=0 || a->player.game_over)continue;
        to.x=(s32)a->player.x_pos;to.y=(s32)a->player.y_pos;
        if(!avp_mp_dm_shot_candidate(to.x-origin_x,to.y-origin_y,
             boxx,boxy,cs,sn,width,nearest,&depth))continue;
        from.x=origin_x;from.y=origin_y;
        if(LineOfSight(&from,&to))continue;
        nearest=depth;selected=q;
    }
    if(selected==AVP_LOCAL_MP_MAX_ACTORS)return 0;
    /* The native TestSpark hook returns NULL to process_shot for player
     * contacts, so the original enemy-only melee sound path would otherwise
     * be skipped. Reuse its EXACT melee contact descriptor and TFX voice
     * dispatch, once and only once for an actual PvP contact. */
    {
        unsigned contact_sfx=avp_mazescrn_melee_contact_sfx(player_type,cur_wepno);
        if(contact_sfx)(void)TFX(contact_sfx);
    }
    /* Marine hitscan bullets use the identical two-frame native spark
     * creator as Marine vs AMP hits. Its original ABLOOD/PBLOOD/MBLOOD
     * creature sprites are yellow, green and red respectively. Spawn
     * inside the currently bound authoritative world, not a screen overlay.
     * PLAYER.S offsets the spark to 7/8 along the hit segment. */
    {
        const AvpMpActorNativeState *hit=(const AvpMpActorNativeState *)
            t->session.player[selected];
        int blood=hit?avp_mp_dm_bullet_impact_creature(player_type,cur_wepno,
            hit->player.player_type):-1;
        if(blood>=0 && hit){
            AvpAmp victim={0};
            s32 dx=(s32)hit->player.x_pos-origin_x;
            s32 dy=(s32)hit->player.y_pos-origin_y;
            victim.creature=(u16)blood;
            dx-=dx>>3;dy-=dy>>3;
            make_spark_hit_at(origin_x+dx,origin_y+dy,&victim);
        }
    }
    walk_dm_apply_hit(t,shooter,selected,
        avp_mp_dm_direct_damage(player_type,cur_wepno,damage),
        player_type==PT_PREDATOR && !invisflag);
    return 1;
}

/* Run the original synchronous COMPUTER.S code with exactly the activating
 * player's native PLAYER/HUD/weapon state and that player's original DOORS/
 * AMP level bound. All other players and worlds are paused until it closes.
 * Do not manufacture aid, clearance, destruct/launch or terminal text. */
static int last_campaign_pod_launched;
int avp_mp_walktest_last_campaign_launch(void)
{return last_campaign_pod_launched;}

static int walk_run_terminal(AvpMpWalkHost *t,unsigned p)
{
    const AvpMpActiveWorld *active;
    AvpMpNativeWorld *world;
    AvpMpActorNativeState *actor;
    AvpMpNativeScope scope={0};
    int world_entered=0,actor_entered=0,ok=1,launched=0,destructed=0;
    int terminal_ui_unavailable=0;
    const AvpRuntimeOps *runtime=avp_runtime_ops();
    unsigned offset;
    if(!t||p>=t->actors||!(t->terminal_pending_mask&(1u<<p)))return 0;
    offset=t->terminal_pending_offset[p];
    t->terminal_pending_mask&=~(1u<<p);
    if(t->dm.enabled)return 1; /* Never enter a terminal UI in Deathmatch. */
    active=avp_mp_session_player_world(&t->session,p);
    actor=(AvpMpActorNativeState *)t->session.player[p];
    if(!active || !active->context || !actor ||
       actor->player.player_type!=PT_HUMAN)return 0;
    /* R283: AI can select the original medical terminal's FIRST AID
     * operation after physically using its source-authored panel. The
     * synchronous original menu cannot be driven by a bot; bind this actor
     * and world then invoke the exact COMPUTER.S health operation. Never
     * use this path for nonmedical panels or story terminals. */
    if(t->session.setup.bot_enabled[p]){
        int healed=0;
        /* Bot-only campaign: no human can operate the capsule launch.
         * Only the elected leader, physically at an AUTHENTIC Level-5
         * terminal face and with the crew already aboard, may dispatch
         * COMPUTER.S's own armed pod launch. Other bots never issue it. */
        if(t->evacuation_active && p==t->evacuation_leader &&
           active->level==5u && (offset==27272u ||
              offset==27273u || offset==27275u) &&
           walk_bot_escape_all_boarded(t,p) &&
           ((unsigned)actor->player.x_pos>>16)==17u &&
           ((unsigned)actor->player.y_pos>>16)==53u){
            int bot_launch=0;
            world=(AvpMpNativeWorld *)active->context;
            if(!avp_mp_native_world_enter(&scope,world))return 0;
            if(!avp_mp_native_actor_enter(&scope,actor)){
                (void)avp_mp_native_world_leave(&scope,world);return 0;
            }
            /* Calls the original COMPUTER.S script action, with the
             * real shared destruct_flag from the bound Marine actor. */
            bot_launch=avp_computer_launch_pod();
            if(!avp_mp_native_actor_leave(&scope,actor) ||
               !avp_mp_native_world_leave(&scope,world))return 0;
            if(bot_launch){
                last_campaign_pod_launched=1;
                for(unsigned q=0;q<t->actors;q++){
                    AvpMpActorNativeState *other=
                        (AvpMpActorNativeState *)t->session.player[q];
                    if(other)other->player.launch_flag=1;
                }
                fprintf(stderr,"MP bots-only pod launched by bot leader P%u using original COMPUTER.S.\n",p+1u);
                return 2;
            }
            return 1;
        }
        if(!avp_computer_is_medical_terminal(active->level,(u16)offset) ||
           actor->access_level<4u || actor->access_level>10u ||
           avp_computer_first_aid_ceiling(actor->access_level)<=
             actor->player.player_energy)return 1;
        world=(AvpMpNativeWorld *)active->context;
        if(!avp_mp_native_world_enter(&scope,world))return 0;
        if(!avp_mp_native_actor_enter(&scope,actor)){
            (void)avp_mp_native_world_leave(&scope,world);return 0;
        }
        /* Pure original COMPUTER.S effect, no artificial health write. */
        healed=avp_computer_first_aid()==1;
        if(!avp_mp_native_actor_leave(&scope,actor) ||
           !avp_mp_native_world_leave(&scope,world))return 0;
        if(healed)fprintf(stderr,
           "MP P%u bot used original medical first aid on level %u.\n",
           p+1u,active->level);
        return 1;
    }
    world=(AvpMpNativeWorld *)active->context;
    if(!avp_mp_native_world_enter(&scope,world))return 0;
    world_entered=1;
    if(!avp_mp_native_actor_enter(&scope,actor)){ok=0;goto done;}
    actor_entered=1;
    comp_panel=t->terminal_pending_panel[p];
    comp_offset=(u16)offset;
    /* The source uses the current Marine's personal access/health but the
     * shared campaign self-destruct flag. All actors are synchronized below
     * only for the genuine global completion/destruct state. */
    if(!avp_pc_video_local_mp_hud_message_route(t->video,(int)p)){ok=0;goto done;}
    for(unsigned q=0;q<t->actors;q++)
        avp_pc_live_heading_reset(&t->live_heading[q]);
    if(!avp_pc_video_local_mp_terminal_begin(t->video,p,t->players)){
        /* A failed host presentation allocation/mode must not terminate an
         * otherwise healthy co-op campaign. No COMPUTER.S action has begun;
         * unwind the native scopes and allow another Use attempt later. */
        fprintf(stderr,"MP P%u terminal presentation unavailable; returning to game.\n",p+1u);
        terminal_ui_unavailable=1;goto done;
    }
    avp_pc_runtime_set_mp_terminal((int)p,t->players);
    if(runtime && runtime->play_sfx)runtime->play_sfx(runtime->user,AVP_FX_COMPE);
    /* COMPUTER.S itself manages original CLUT, text rendering, menus,
     * countdown arming, first aid and the escape-pod launch event. */
    Computer();
    if(runtime && runtime->play_sfx)runtime->play_sfx(runtime->user,AVP_FX_COMPD);
    destructed=destruct_flag!=0;
    launched=launch_flag!=0;
    /* Arm the ORIGINAL HUD.S countdown against the global MP tick clock,
     * not against whichever player later happens to open a HUD viewport.
     * This is performed once for the authenticated source COMPUTER.S arm. */
    if(destructed && !t->evacuation_active && t->species==AVP_MP_MARINE){
        avp_hud_set_clock((u32)t->session.tick,10u);
        counting=-1;
        InitCountdown();
        (void)avp_mp_hudworld_native_capture(&t->shared_countdown);
        t->shared_countdown_valid=1u;
    }
    fprintf(stderr,"MP original terminal: P%u level %u panel %u offset %u (destruct=%d launch=%d).\n",
            p+1u,active->level,(unsigned)comp_panel,offset,destructed,launched);
done:
    avp_pc_runtime_set_mp_terminal(-1,0);
    avp_pc_video_local_mp_terminal_end(t->video);
    if(!avp_pc_video_local_mp_hud_message_route(t->video,-1))ok=0;
    if(actor_entered && !avp_mp_native_actor_leave(&scope,actor))ok=0;
    if(world_entered && !avp_mp_native_world_leave(&scope,world))ok=0;
    /* The Jaguar destruction sequence is one campaign condition. Our actor
     * scope stores it per actor for co-op isolation; publish the script's
     * real result to ALL players, not just the computer operator. */
    if(ok && launched)last_campaign_pod_launched=1;
    if(ok && destructed && !t->evacuation_active &&
       t->species==AVP_MP_MARINE){
        t->evacuation_active=1u;
        t->evacuation_start_tick=t->session.tick;
        t->evacuation_leader=walk_bot_evacuation_leader(t);
        fprintf(stderr,"MP self-destruct armed: emergency evacuation to Level 5; bot leader=%u.\n",
                t->evacuation_leader< t->actors?t->evacuation_leader+1u:0u);
    }
    if(ok && (destructed||launched))for(unsigned q=0;q<t->actors;q++){
        AvpMpActorNativeState *a=(AvpMpActorNativeState *)t->session.player[q];
        if(a){if(destructed)a->player.destruct_flag=1;
              if(launched)a->player.launch_flag=1;}
    }
    avp_pc_video_reset_presentation(t->video);
    if(terminal_ui_unavailable && ok)return 1;
    return ok? (launched?2:1):0;
}

/* Select one of the fifteen source-authored level worlds, without bias to
 * campaign start, and join at a source-valid randomized floor coordinate.
 * Each failed transaction leaves the source world/actor untouched. This is
 * PC-only spawning; no Jaguar door, player or map logic is modified. */
static int walk_random_level_spawn(AvpMpWalkHost *t,unsigned p)
{
    unsigned candidates[AVP_MP_RETAIL_LEVELS];
    if(!t || p>=t->actors || !t->retail_catalog_ready ||
       t->session.setup.custom_session_id==AVP_MP_CUSTOM_LEVEL)return 0;
    for(unsigned i=0;i<AVP_MP_RETAIL_LEVELS;i++)candidates[i]=i+1u;
    for(unsigned i=AVP_MP_RETAIL_LEVELS-1u;i>0;i--){
        unsigned j=(unsigned)(avp_random()%(i+1u));
        unsigned tmp=candidates[i];candidates[i]=candidates[j];candidates[j]=tmp;
    }
    for(unsigned i=0;i<AVP_MP_RETAIL_LEVELS;i++){
        const unsigned level=candidates[i];
        /* Do not choose duct levels for Predator first-spawn OR respawn.
         * Keep the original shuffled candidate order for all species. */
        if(!avp_mp_spawn_level_allowed(t->session.setup.species[p],level))
            continue;
        const AvpMpActiveWorld *current=avp_mp_session_player_world(&t->session,p);
        int ok=0;
        if(!current)return 0;
        memset(&t->entering[p],0,sizeof t->entering[p]);
        t->entering[p].valid=1;
        t->entering[p].target_level=(s16)level;
        t->entering[p].new_x=t->entering[p].new_y=-1;
        t->entering_reset[p]=1;
        t->random_level_entry[p]=1;
        if(current->level==level){
            AvpMpActorNativeState *actor=(AvpMpActorNativeState *)t->session.player[p];
            if(actor)ok=walk_join(t,(AvpMpNativeWorld *)current->context,p,actor);
        }else ok=avp_mp_session_move(&t->session,p,level);
        t->random_level_entry[p]=0;
        t->entering_reset[p]=0;
        memset(&t->entering[p],0,sizeof t->entering[p]);
        if(ok){
            AvpMpActorNativeState *actor=(AvpMpActorNativeState *)t->session.player[p];
            if(!walk_clear_arrival_enemies(t,AVP_MP_ACTOR_BIT(p)))return 0;
            if(actor){t->spawn_x[p]=actor->player.x_pos;t->spawn_y[p]=actor->player.y_pos;}
            return 1;
        }
    }
    return 0;
}
static int walk_respawn(AvpMpWalkHost *t,unsigned p)
{
    AvpMpActorNativeState *a=(AvpMpActorNativeState *)t->session.player[p];
    AvpMpRespawnPlan plan;
    if(!a || !avp_mp_respawn_plan(a,
            avp_pc_video_marine_start_shotgun(t->video),
            t->session.setup.campaign,
            t->session.setup.respawn_campaign_start,&plan) ||
       plan.kind==AVP_MP_RESPAWN_NONE)return 0;
    /* Select exactly once per death, before the original species initializer
     * runs. A new species must not reuse an old Alien cocoon destination,
     * and the existing campaign-start option remains authoritative in co-op. */
    if(t->random_species[p] && t->session.setup.random_species_on_respawn){
        static const unsigned species_level[3]={3u,14u,15u};
        const unsigned next=(unsigned)(avp_random()%3u);
        if(!t->species_start_ready[next])return 0;
        t->session.setup.species[p]=next;
        plan.kind=AVP_MP_RESPAWN_SPECIES_START;
        plan.new_x=plan.new_y=-1;
        if(!t->session.setup.respawn_campaign_start && !t->dm.enabled)
            plan.target_level=species_level[next];
    }
    /* Independent random-level respawn takes priority over the normal
     * co-op campaign-start choice and DM same-world rule. A selected level
     * may be a duct or ship; only legal floor/engine-safe poses are accepted. */
    /* DM and random-first-level sessions otherwise restart on their CURRENT
     * level. If a Predator dies while already in a duct, move the respawn to
     * a legal surface level instead of looping into that same duct. */
    const AvpMpActiveWorld *death_world=avp_mp_session_player_world(&t->session,p);
    const int predator_duct_restart=death_world &&
        (t->dm.enabled || t->session.setup.spawn_in_random_level) &&
        !avp_mp_spawn_level_allowed(t->session.setup.species[p],death_world->level);
    if((t->session.setup.respawn_on_random_level || predator_duct_restart) &&
       t->session.setup.custom_session_id!=AVP_MP_CUSTOM_LEVEL){
        if(!walk_random_level_spawn(t,p))return 0;
        a=(AvpMpActorNativeState *)t->session.player[p];
        t->prev_x[p]=a->player.x_pos;t->prev_y[p]=a->player.y_pos;
        t->prev_angle[p]=a->player.centre_angle;
        if(t->dm.enabled && t->dm.fast_movement)
            avp_mp_dm_speed(1,&a->player.max_speed,&a->player.acc_shift);
        t->dm_death_credited[p]=0u;
        avp_pc_live_heading_reset(&t->live_heading[p]);
        t->live_last_game_angle[p]=a->player.centre_angle;
        avp_mp_death_camera_end(&t->death_camera[p]);
        walk_shared_arrival_refresh(t,AVP_MP_ACTOR_BIT(p));
        t->collected_keycards[p]=0;
        walk_reset_mp_item_claims(t,p);
        memset(t->contacts[p],0,sizeof t->contacts[p]);
        ++t->completed_respawns;
        return 1;
    }
    if(t->dm.enabled || t->session.setup.spawn_in_random_level){
        const AvpMpActiveWorld *current=avp_mp_session_player_world(&t->session,p);
        if(!current)return 0;
        plan.kind=AVP_MP_RESPAWN_SPECIES_START;
        plan.target_level=current->level;
        plan.new_x=plan.new_y=-1;
    }
    {const AvpMpActiveWorld *current=avp_mp_session_player_world(&t->session,p);
     if(!current)return 0;
     if(plan.target_level!=current->level){
        /* Cross-world respawn first performs the same transactional move
         * as a real exit; no other world loses its doors/enemies. The
         * destination join reconstructs the original fresh actor loadout
         * with the supplied species-start and honor/shotgun policy. */
        if(!t->retail_catalog_ready)return 0;
        if(plan.kind==AVP_MP_RESPAWN_COCOON){
            /* Cocoon consumption mutates the original HUD inventory. Until
             * the detached transaction can roll back its side effects, do
             * not consume a life if a different world must be loaded. */
            return 0;
        }
        memset(&t->entering[p],0,sizeof(t->entering[p]));
        t->entering[p].valid=1;
        t->entering[p].target_level=(s16)plan.target_level;
        t->entering[p].new_x=plan.new_x;
        t->entering[p].new_y=plan.new_y;
        t->entering_reset[p]=1;
        {int moved=avp_mp_session_move(&t->session,p,plan.target_level);
         memset(&t->entering[p],0,sizeof(t->entering[p]));
         t->entering_reset[p]=0;
         if(!moved)return 0;}
        a=(AvpMpActorNativeState *)t->session.player[p];
        if(!walk_clear_arrival_enemies(t,AVP_MP_ACTOR_BIT(p)))return 0;
        t->prev_x[p]=a->player.x_pos;t->prev_y[p]=a->player.y_pos;
        t->prev_angle[p]=a->player.centre_angle;
        /* The death fade is allowed to hold the camera only until the
         * transactional destination join has completed. Previously this
         * cross-world return skipped the release, permanently freezing view. */
        avp_mp_death_camera_end(&t->death_camera[p]);
        walk_shared_arrival_refresh(t,AVP_MP_ACTOR_BIT(p));
        t->collected_keycards[p]=0;
        walk_reset_mp_item_claims(t,p);
        memset(t->contacts[p],0,sizeof t->contacts[p]);
        ++t->completed_respawns;
        fprintf(stderr,"MP P%u cross-level respawn: level %u, camera released.\n",
                p+1u,plan.target_level);
        return 1;
     }}
    if(plan.kind==AVP_MP_RESPAWN_COCOON){
        AvpMpNativeScope scope={0};
        AvpMpNativeWorld *world=NULL;
        s16 saved_new_level=new_level;
        s32 saved_new_x=new_x,saved_new_y=new_y;
        for(unsigned w=0;w<AVP_LOCAL_MP_MAX_ACTORS;w++)
            if(t->session.worlds[w].context &&
               (t->session.worlds[w].member_mask&(AVP_MP_ACTOR_BIT(p)))) {
                world=(AvpMpNativeWorld *)t->session.worlds[w].context;break;
            }
        if(!world || !avp_mp_native_world_enter(&scope,world))return 0;
        if(!avp_mp_native_actor_enter(&scope,a)){
            (void)avp_mp_native_world_leave(&scope,world);return 0;
        }
        UseCocoon();  /* Original HUD.S removes the ready cocoon. */
        x_pos=(u32)plan.new_x;y_pos=(u32)plan.new_y;
        x_vel=y_vel=0;player_dead=game_over=key_lock=0;
        new_level=saved_new_level;new_x=saved_new_x;new_y=saved_new_y;
        {int actor_ok=avp_mp_native_actor_leave(&scope,a);
         int world_ok=avp_mp_native_world_leave(&scope,world);
         if(!actor_ok||!world_ok)return 0;}
    }else{
        /* A mixed-species player can die on their species-start level
         * AFTER entering it from a different campaign start. The original
         * per-session spawn_x/y are coordinates in the campaign's first
         * world and must never be reused in this destination map. Resolve
         * this same-world restart against the original LEVELS.S start pose
         * with the same SafePos/other-player clearance as a normal join. */
        const AvpMpActiveWorld *current=avp_mp_session_player_world(&t->session,p);
        AvpMpNativeWorld *destination=current?(AvpMpNativeWorld *)current->context:NULL;
        int ok;
        if(!destination)return 0;
        memset(&t->entering[p],0,sizeof(t->entering[p]));
        t->entering[p].valid=1;
        t->entering[p].target_level=(s16)plan.target_level;
        t->entering[p].new_x=-1;t->entering[p].new_y=-1;
        t->entering_reset[p]=1;
        ok=walk_join(t,destination,p,a);
        memset(&t->entering[p],0,sizeof(t->entering[p]));
        t->entering_reset[p]=0;
        if(!ok)return 0;
    }
    if(!walk_clear_arrival_enemies(t,AVP_MP_ACTOR_BIT(p)))return 0;
    t->prev_x[p]=a->player.x_pos;t->prev_y[p]=a->player.y_pos;
    t->prev_angle[p]=a->player.centre_angle;
    if(t->dm.enabled && t->dm.fast_movement)
        avp_mp_dm_speed(1,&a->player.max_speed,&a->player.acc_shift);
    t->dm_death_credited[p]=0u;
    avp_pc_live_heading_reset(&t->live_heading[p]);
    t->live_last_game_angle[p]=a->player.centre_angle;
    /* Release the frozen camera only after the destination join succeeds.
     * If it fails, the dead player remains safely frozen and black. */
    avp_mp_death_camera_end(&t->death_camera[p]);
    walk_shared_arrival_refresh(t,AVP_MP_ACTOR_BIT(p));
    t->collected_keycards[p]=0;
    walk_reset_mp_item_claims(t,p);
    memset(t->contacts[p],0,sizeof t->contacts[p]);
    ++t->completed_respawns;
    {const AvpMpActiveWorld *current=avp_mp_session_player_world(&t->session,p);
     fprintf(stderr,"MP P%u respawn complete at %08x/%08x on original level %u.\n",
            p+1u,(unsigned)a->player.x_pos,(unsigned)a->player.y_pos,
            current?current->level:0u);}
    return 1;
}

/* MAZE.S::PlayAvP fades only duct changes; the local co-op compositor
 * must perform that 64 VBlank fade per passenger, not globally. */
static void walk_dm_publish(AvpMpWalkHost *t)
{
    char status[512];
    size_t used=0;
    unsigned seconds;
    if(!t || !t->dm.enabled)return;
    seconds=avp_mp_dm_seconds_remaining(&t->dm);
    if(t->dm.finished){
        if(t->dm.winner==AVP_LOCAL_MP_MAX_ACTORS)
            used=(size_t)snprintf(status,sizeof(status),"DEATHMATCH OVER - TIE");
        else used=(size_t)snprintf(status,sizeof(status),"DEATHMATCH OVER - PLAYER %u WINS",
                          t->dm.winner+1u);
    }else if(t->dm.time_limit_minutes){
        used=(size_t)snprintf(status,sizeof(status),"DEATHMATCH - %u:%02u remaining",
                              seconds/60u,seconds%60u);
    }else used=(size_t)snprintf(status,sizeof(status),"DEATHMATCH - Unlimited Time");
    if(used>=sizeof(status))used=sizeof(status)-1u;
    avp_pc_video_local_mp_deathmatch_status(t->video,status);
    /* R204: export every player score as data. The X11/GDI presentation
     * arranges these into as many visible rows as the window permits. */
    avp_pc_video_local_mp_deathmatch_scores(t->video,t->actors,
        t->dm.kills,t->dm.deaths,t->dm.elapsed_ticks);
}

static int walk_is_duct_transfer(unsigned from,const AvpMpTransitionRequest *r)
{
    if(!r || !r->valid || r->use_cocoon || from<1u || from>15u)return 0;
    return (from>=AVP_FIRST_DUCT && from<=AVP_LAST_DUCT) ||
           ((unsigned)r->target_level>=AVP_FIRST_DUCT &&
            (unsigned)r->target_level<=AVP_LAST_DUCT);
}

int avp_mp_walktest_run(AvpPcVideo *video,unsigned level,
                        const AvpMazeCandidate *retail_level,
                        const AvpMazeCandidate *all_mazes,unsigned maze_count,
                        const AvpRomImage *rom,int combat_test,unsigned players,
                        const AvpLocalMpSetup *requested)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)calloc(1,sizeof(*t));
    uint64_t next_tick, next_display, next_render;
    int quit=0,failed=0,completed=0,return_to_title=0,restart_from_f2=0;
    int start_custom_from_cheat=0;
    int f2_menu_audio_suspended=0;
    uint64_t dm_finished_at=0;
    int victory_ending=0;
    unsigned victory_countdown=0;
    unsigned long ticks=0;
    const uint64_t sim_period=100000000ull;
    unsigned render_hz=60u;
    uint64_t render_period=1000000000ull/60ull;
    AvpMpPause pause_state={0};
    last_campaign_pod_launched=0;
    if(!t)return 1;
    t->evacuation_leader=AVP_LOCAL_MP_MAX_ACTORS;
    if(!avp_pc_video_local_mp_walktest_mode(video,1)||
       !walk_init(t,video,level,retail_level,all_mazes,maze_count,
                  rom,combat_test,players,requested)){
        fprintf(stderr,"MP movement test cannot bind the current original world/actor state.\n");
        walk_cleanup(t);avp_pc_video_local_mp_walktest_mode(video,0);
        free(t);return 1;
    }
    avp_hud_set_coop_countdown(1u);
    if(t->dm.enabled){
        fprintf(stderr,"Deathmatch mode: level=%u, NPC enemies=%s, fast-equip=%s, score=%s (%u), time=%s (%u min), 2x movement=%s.\n",
            t->dm.level,t->dm.enemies?"on":"off",t->dm.fast_equip?"on":"off",
            t->dm.score_limit?"limited":"unlimited",t->dm.score_limit,
            t->dm.time_limit_minutes?"limited":"unlimited",t->dm.time_limit_minutes,
            t->dm.fast_movement?"on":"off");
        walk_dm_publish(t);
    }
    fprintf(stderr,"RE27 native %s: %u players, original %s campaign level %u, one window.\n",
                   t->combat_test?"combat":"movement/view",t->players,
                   avp_local_mp_campaign_name(t->species),level);
    for(unsigned p=0;p<t->players;p++)
        fprintf(stderr,"MP P%u species: %s (type %d; max energy %d; weapons %d).\n",
                p+1u,avp_local_mp_species_name(t->session.setup.species[p]),
                (int)((AvpMpActorNativeState *)t->session.player[p])->player.player_type,
                (int)((AvpMpActorNativeState *)t->session.player[p])->player.max_energy,
                (int)((AvpMpActorNativeState *)t->session.player[p])->weapon.num_weps);
    next_tick=avp_pc_now_ns()+100000000ull;
    next_display=avp_pc_now_ns()+(1000000000ull/60ull);
    /* Jaguar MazeList/fade keeps its 60 Hz clock. Raster views may use
     * optional 120 Hz host presentation without advancing game state. */
    render_hz=avp_pc_video_presentation_hz(video);
    render_period=avp_presentation_host_frame_period(render_hz);
    next_render=next_display;
    /* The host pad/mouse binder is sampled outside an actor scope. It must
     * use Player 1's character controls (Alien turn/attack and Predator
     * equipment mappings), even in a different-species campaign. Original
     * scoped actor visits save/restore this unbound host value. */
    const s16 host_campaign_type=player_type;
    player_type=((const AvpMpActorNativeState *)t->session.player[0])->player.player_type;
    while(!quit && !failed){
        AvpMpPlayerCommand cmd[AVP_LOCAL_MP_MAX_ACTORS]={{0}};
        uint32_t pad=UINT32_MAX;
        uint64_t now;
        avp_pc_video_poll(video,&pad,&quit);
        /* Do not route manual spy controls through the 10-Hz Jaguar input
         * scheduler. Consume all ordered key/button press events on every
         * host poll, including multiple quick taps between game ticks. */
        avp_pc_video_local_mp_collect_spy_input(video,t->players);
        {int changed=0;
         for(unsigned p=0;p<t->players;p++){
             int direction;
             while((direction=avp_pc_video_local_mp_take_spy_press(video,p))!=0){
                 const unsigned old=t->session.spy_target[p];
                 const unsigned subject=avp_mp_spycam_cycle(p,old,t->actors,
                           walk_spy_eligible_mask(t,p),direction);
                 t->session.spy_target[p]=subject;
                 if(subject!=old){
                     changed=1;
                     fprintf(stderr,"MP P%u spycam: observing P%u (%s).\n",
                         p+1u,subject+1u,subject==p?"self":"remote");
                 }
             }
         }
         if(changed)walk_refresh_audio_listeners(t);
        }
        /* Capture short P2-P4 inputs at host poll cadence, not just the
         * original 10 Hz tick. The normal 10 Hz command path below is
         * completely unchanged when both enhancement switches are OFF.
         * The high-rate reader consumes mouse impulses, so the buffer also
         * carries those forward exactly once. */
        if(!quit && avp_pc_video_local_mp_bots_may_think(video) &&
           (avp_pc_video_buffer_short_presses(video)||
            avp_pc_video_buffer_short_analog(video))){
            AvpMpPlayerCommand sampled[AVP_LOCAL_MP_MAX_PLAYERS]={{0}};
            if(avp_pc_video_local_mp_take_commands(video,t->players,sampled))
                avp_mp_enhancement_buffer_poll(&t->host_input_buffer,sampled,
                    t->players,avp_pc_video_buffer_short_presses(video),
                    avp_pc_video_buffer_short_analog(video));
            else avp_mp_enhancement_buffer_reset(&t->host_input_buffer);
        }else avp_mp_enhancement_buffer_reset(&t->host_input_buffer);
        if(avp_pc_video_take_restart_title(video)){
            /* Do not process a further world, AI, input or presentation tick.
             * walk_cleanup below releases all rooms, audio and actors. */
            return_to_title=1;
            break;
        }
        if(quit)break;
        now=avp_pc_now_ns();
        {const unsigned selected=avp_pc_video_presentation_hz(video);
         if(selected!=render_hz){
             render_hz=selected;
             render_period=avp_presentation_host_frame_period(render_hz);
             next_render=now; /* Apply F12 change without stale 60 Hz phase. */
         }
        }
        if(t->dm.enabled){
            walk_dm_publish(t);
            if(t->dm.finished){
                if(!dm_finished_at){
                    dm_finished_at=now;
                    fprintf(stderr,"Deathmatch complete after %llu ticks. Winner: %s.\n",
                        (unsigned long long)t->dm.elapsed_ticks,
                        t->dm.winner==AVP_LOCAL_MP_MAX_ACTORS?"tie":"player (see scoreboard)");
                    avp_jaguar_fullsynth_mp_set_paused(1);
                }
                if(now-dm_finished_at>=4000000000ull){return_to_title=1;break;}
                if(now>=next_render){
                    avp_pc_video_local_mp_render_alpha(video,1.0);
                    if(!avp_pc_video_present_local_mp(video,&t->session))failed=1;
                    next_render=avp_presentation_next_display_deadline(
                        next_render,now,render_period);
                }
                next_tick=now+sim_period;
                /* Preserve the original VBL phase without executing frozen
                 * Jaguar world or fade logic. */
                next_display=avp_presentation_next_display_deadline(
                    next_display,now,1000000000ull/60ull);
                avp_pc_sleep_ns(2000000ull);
                continue;
            }
        }
        /* Start Game from the live F2 screen replaces this session. The
         * existing cleanup releases worlds, actors and audio before main
         * initializes a fresh retail campaign using the edited setup. */
        if(avp_pc_video_local_mp_launch_pending(video)){
            restart_from_f2=1;
            break;
        }
        /* F2 is a real modal screen: freeze the 10-Hz game, 60-Hz fade,
         * bot commands and pause edges while editing it. Always poll OS
         * events and present the completed co-op views below the overlay. */
        if(avp_pc_video_local_mp_menu_open(video)){
            /* A modal menu must not preserve an unfinished host-clock turn
             * that could be displayed or replayed after a long pause. */
            for(unsigned p=0;p<t->players;p++)
                avp_pc_live_heading_reset(&t->live_heading[p]);
            if(!f2_menu_audio_suspended){
                avp_jaguar_fullsynth_mp_set_paused(1);
                f2_menu_audio_suspended=1;
            }
            if(now>=next_render){
                avp_pc_video_local_mp_render_alpha(video,1.0);
                if(!avp_pc_video_present_local_mp(video,&t->session))failed=1;
                next_render=avp_presentation_next_display_deadline(
                    next_render,now,render_period);
            }
            next_tick=now+sim_period;
            /* Preserve the original VBL phase without executing frozen
             * Jaguar world or fade logic. */
            next_display=avp_presentation_next_display_deadline(
                next_display,now,1000000000ull/60ull);
            avp_pc_sleep_ns(2000000ull);
            continue;
        }
        if(f2_menu_audio_suspended){
            /* Restore the audio state owned by the existing co-op Pause
             * toggle. Never unpause a session still paused by a player. */
            avp_jaguar_fullsynth_mp_set_paused(pause_state.paused);
            f2_menu_audio_suspended=0;
            next_tick=now+sim_period;
        }
        /* Bot ownership is the only F2 setting that alters an existing
         * session. Campaign, player count and character selections take
         * effect on Start Game, not by modifying live world state. */
        {const AvpLocalMpSetup *menu=avp_pc_video_local_mp_setup(video);
         if(menu)for(unsigned p=0;p<t->players;p++){
             unsigned requested=!!menu->bot_enabled[p];
             if((unsigned)t->session.setup.bot_enabled[p]!=requested){
                 t->session.setup.bot_enabled[p]=requested;
                 memset(&t->bots[p],0,sizeof(t->bots[p]));
                 fprintf(stderr,"MP P%u AI control %s live.\n",p+1u,
                         requested?"enabled":"disabled");
             }
         }}
        if(pause_state.paused){
            for(unsigned p=0;p<t->players;p++)
                avp_pc_live_heading_reset(&t->live_heading[p]);
            /* The entire multiworld session is frozen. Poll control input
             * independently of simulation, including P2-P4's Pause binds.
             * Do not run MazeList, enemy ticks, deaths, transitions or HUD
             * timers. F12/OS events continue through avp_pc_video_poll. */
            if(now>=next_tick){
                uint32_t held=((pad&(1u<<PAUSE))==0u)?1u:0u;
                int valid=avp_pc_video_local_mp_take_commands(video,t->players,cmd);
                if(valid)for(unsigned p=1;p<t->players;p++)
                    if(cmd[p].actions&(1u<<AVP_MP_PAUSE))held|=1u<<p;
                if(avp_mp_pause_step(&pause_state,held) && !pause_state.paused){
                    avp_jaguar_fullsynth_mp_set_paused(0);
                    avp_pc_video_local_mp_set_paused(video,0);
                    fprintf(stderr,"MP session resumed at tick %lu.\n",ticks);
                }
                next_tick=now+sim_period;
            }
            if(now>=next_render){
                avp_pc_video_local_mp_render_alpha(video,1.0);
                if(!avp_pc_video_present_local_mp(video,&t->session))failed=1;
                next_render=avp_presentation_next_display_deadline(
                    next_render,now,render_period);
            }
            /* Preserve the original VBL phase without executing frozen
             * Jaguar world or fade logic. */
            next_display=avp_presentation_next_display_deadline(
                next_display,now,1000000000ull/60ull);
            avp_pc_sleep_ns(2000000ull);
            continue;
        }
        /* Per-player stick polling occurs between authoritative sim ticks. */
        walk_live_look_poll(t,now,10u);
        /* MAZESCRN.S::MazeList is normally driven by the original Jaguar
         * 60 Hz display clock. It advances the authored fade-in before the
         * rasterizer applies the game's black-at-0x80 output rule. The
         * previous walktest skipped this original display path completely,
         * leaving the retail level permanently black. This limited smoke
         * has exactly one shared world; no multi-world clock is implied. */
        if(now>=next_display){
            MazeList();
            if(victory_ending && !victory_countdown && faded){
                completed=1;quit=1;
            }
            for(unsigned p=0;p<t->actors;p++)
                (void)avp_mp_life_display_step(&t->vent_fade[p]);
            for(unsigned p=0;p<t->actors && !victory_ending;p++){
                if(avp_mp_life_display_step(&t->player_life[p])){
                    if(walk_respawn(t,p)){
                        /* A new life has no outstanding DM travel plan. */
                        memset(&t->dm_bot_travel[p],0,sizeof t->dm_bot_travel[p]);
                        avp_mp_life_resumed(&t->player_life[p]);
                    }
                    else avp_mp_life_wait(&t->player_life[p]);
                }
            }
            next_display=avp_presentation_next_display_deadline(
                next_display,now,1000000000ull/60ull);
        }
        if(victory_ending){
            /* After the original game_over from the shared AMP world,
             * complete the source end_count (10 simulation ticks) and
             * original 64-VBL FadeDown before tearing down any world.
             * No player/AI ticks may run during the ending. */
            if(now>=next_tick){
                if(victory_countdown)--victory_countdown;
                next_tick=now+sim_period;
            }
            if(!victory_countdown && faded){completed=1;quit=1;}
        }else if(now>=next_tick){
            /* The original host's simulation-pad/analog bridge is part of
             * the game input path, not an optional single-player extra. */
            t->last_pad=avp_pc_video_simulation_pad(video,pad);
            /* F12 player settings are live; command mapping and native scope
             * must see the SAME saved profile on this 10-Hz tick. */
            {const AvpMpInputConfig *current=avp_pc_video_local_mp_profiles(video);
             if(current)t->profiles=*current;}
            t->p1_analog_enabled=avp_pc_video_analog_enabled(video);
            avp_pc_video_analog_motion(video,&t->p1_forward,&t->p1_strafe);
            t->p1_keyboard_strafe=avp_pc_video_keyboard_strafe_during_turn(video);
            t->p1_turn=avp_pc_video_modern_turn(video);
            t->p1_aim_enabled=avp_pc_video_aim_enabled(video);
            t->p1_aim_sensitivity=avp_pc_video_aim_sensitivity(video);
            t->p1_aim_speed=avp_pc_video_aim_speed(video);
            t->p1_enhanced_aim=avp_pc_video_enhanced_analog_enabled(video);
            t->p1_mouse_aim=avp_pc_video_mouse_aim_active(video);
            t->pending_actions=avp_pc_video_take_actions(video);
            if(t->pending_actions&AVP_PC_ACTION_CUSTOM_MAP_START){
                start_custom_from_cheat=1;break;
            }
            for(unsigned p=0;p<t->players;p++)
                t->pending_cheat_actions[p]=avp_pc_video_take_local_mp_cheat_actions(video,p);
            t->p1_weapon_delta=avp_pc_video_take_weapon_delta(video);
            avp_pc_runtime_set_noclip(avp_pc_video_noclip(video));
            avp_dev_cheats_set_health_mode(AVP_NEW_HEALTH_OFF);
            avp_dev_cheats_set_infinite_ammo(avp_pc_video_infinite_ammo(video));
            avp_amp_set_enemies_frozen(avp_pc_video_freeze_enemies(video));
            avp_amp_set_attacks_disabled(avp_pc_video_attacks_disabled(video));
            avp_doors_predator_jammed_attack_enabled(
                avp_pc_video_predator_attack_opens_jammed_doors(video));
            avp_player_set_center_aim_options(
                avp_pc_video_player_melee_center_aim(video),
                avp_pc_video_player_all_center_aim(video));
            avp_amp_set_predator_cloak_honor_options(
                avp_pc_video_predator_cloaked_no_honor_loss(video),
                avp_pc_video_predator_cloaked_half_honor(video));
            avp_amp_set_marine_pulse_los_hits(avp_pc_video_marine_pulse_los_hits(video));
            avp_amp_set_persistent_enemy_health(avp_pc_video_persistent_enemy_health(video));
            avp_amp_set_persistent_hazard_health(avp_pc_video_persistent_hazard_health(video));
            avp_amp_set_generator_blast_radius(avp_pc_video_generator_blast_radius(video));
            avp_amp_set_crawl_death_interrupt(avp_pc_video_crawl_death_interrupt(video));
            avp_amp_set_enemy_number_multiplier(avp_pc_video_enemy_number_multiplier(video));
            avp_amp_set_vent_facehuggers(avp_pc_video_vent_facehuggers(video));
            avp_pc_video_apply_enemy_stats(video);
            avp_amp_set_marine_fire_interrupt(avp_pc_video_marine_fire_interrupt(video));
            avp_amp_set_normalize_diagonal_contact_range(avp_pc_video_normalize_diagonal_contact_range(video));
            avp_amp_set_normalize_diagonal_alien_death_spray_range(
                avp_pc_video_normalize_diagonal_alien_death_spray_range(video));
            avp_amp_set_normalize_diagonal_enemy_movement_speed(
                avp_pc_video_normalize_diagonal_enemy_movement_speed(video));
            avp_amp_set_enemy_activation_distance(avp_pc_video_enemy_activation_distance(video));
            avp_amp_set_facehug_attach_disabled(avp_pc_video_facehug_attach_disabled(video));
            /* Secondary player uses the exact previously saved P2 controls.
             * Paused menus or lost focus MUST NOT replay stale input. */
            int input_accepted=avp_pc_video_local_mp_take_commands(video,t->players,cmd);
            if(input_accepted && (avp_pc_video_buffer_short_presses(video)||
                                   avp_pc_video_buffer_short_analog(video)))
                avp_mp_enhancement_buffer_take(&t->host_input_buffer,cmd,
                    t->players,avp_pc_video_buffer_short_presses(video),
                    avp_pc_video_buffer_short_analog(video));
            else avp_mp_enhancement_buffer_reset(&t->host_input_buffer);
            if(!input_accepted){
                /* A rejected multiplayer frame must release *all* players.
                 * P1 uses extra legacy host analogue/turn/weapon fields that
                 * are not part of cmd[]. Releasing only its Jaguar pad left
                 * stick movement/turning active behind F12, F2 or FocusOut.
                 * Never change the legacy single-player sampling path. */
                t->last_pad=UINT32_MAX;
                t->p1_forward=t->p1_strafe=t->p1_turn=0.0f;
                t->p1_analog_enabled=t->p1_aim_enabled=0;
                t->p1_enhanced_aim=t->p1_mouse_aim=0;
                t->p1_keyboard_strafe=0;
                t->p1_weapon_delta=0;
                memset(t->prior_weapon_buttons,0,sizeof(t->prior_weapon_buttons));
                memset(cmd,0,sizeof(cmd));
            }
            /* Use rising edges from ALL participants, but toggle one shared
             * scheduler pause. P1's native Pause bit comes from its existing
             * keyboard/gamepad input, not a second controller poll. */
            {
                uint32_t held=((t->last_pad&(1u<<PAUSE))==0u)?1u:0u;
                for(unsigned p=1;p<t->players;p++)
                    if(cmd[p].actions&(1u<<AVP_MP_PAUSE))held|=1u<<p;
                if(avp_mp_pause_step(&pause_state,held) && pause_state.paused){
                    for(unsigned p=0;p<t->players;p++)
                        avp_pc_live_heading_reset(&t->live_heading[p]);
                    avp_jaguar_fullsynth_mp_set_paused(1);
                    avp_pc_video_local_mp_set_paused(video,1);
                    fprintf(stderr,"MP session paused by player input at tick %lu.\n",ticks);
                    /* Show the last completed co-op scene, not an advanced
                     * interpolation, while the original player/world snapshots
                     * remain untouched. */
                    avp_pc_video_local_mp_render_alpha(video,1.0);
                    if(!avp_pc_video_present_local_mp(video,&t->session))failed=1;
                    next_tick=now+sim_period;
                    next_render=avp_presentation_next_display_deadline(
                        next_render,now,render_period);
                    avp_pc_sleep_ns(2000000ull);
                    continue;
                }
            }
            /* AI overrides human action samples for selected participant
             * slots only, preserving original P1/P2-P4 input paths for all
             * human participants. Pause remains under local human/menu control. */
            /* Focus loss suppresses HUMAN samples, never an autonomous bot.
             * Preserve the existing menu/pause guard independently of focus. */
            if(avp_pc_video_local_mp_bots_may_think(video))
                walk_bot_commands(t,cmd,ticks);
            if(t->session.setup.bot_enabled[0]){
                t->last_pad=UINT32_MAX;
                t->p1_forward=t->p1_strafe=t->p1_turn=0.0f;
                t->p1_analog_enabled=t->p1_aim_enabled=0;
                t->p1_enhanced_aim=t->p1_mouse_aim=0;
                t->p1_keyboard_strafe=0;
                t->p1_weapon_delta=0;
                t->pending_actions&=(AVP_PC_ACTION_CLEAR_ENEMIES|AVP_PC_ACTION_CLEAR_HAZARDS|
                    AVP_PC_ACTION_REFRESH_LEVEL);
            }
            /* Original PC quick-turn binding for P1, dedicated keyboard,
             * mouse or axis-as-button binding for the other participants. */
            {unsigned n=avp_pc_video_take_quick_turn(video);
             if(n && !t->session.setup.bot_enabled[0]){
                 const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[0];
                 if(a&&!avp_mp_life_busy(&t->player_life[0]))
                     avp_pc_quick_turn_enqueue(&t->quick_turn[0],
                        a->player.centre_angle,now,n);
             }}
            for(unsigned p=1;p<t->players;p++){
                uint32_t held=cmd[p].actions&(1u<<AVP_MP_QUICK_TURN);
                if(held&&!t->quick_held[p]&&!avp_mp_life_busy(&t->player_life[p])){
                    const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
                    if(a)avp_pc_quick_turn_enqueue(&t->quick_turn[p],
                         a->player.centre_angle,now,1u);
                }
                t->quick_held[p]=held;
            }
            /* Spycam switches on the host input/presentation clock above,
             * not on this 10-Hz native game tick. Native player controls
             * remain owned by the input player. */
            /* PC-only per-viewport map magnification. Independent rising
             * edges, separate from the original map toggle/retail raster. */
            for(unsigned p=0;p<t->players;p++){
                uint32_t held=cmd[p].actions &
                    ((1u<<AVP_MP_MAP_ZOOM_IN)|(1u<<AVP_MP_MAP_ZOOM_OUT));
                uint32_t edge=held&~t->map_zoom_held[p];
                if(edge&(1u<<AVP_MP_MAP_ZOOM_IN)){
                    if(t->map_zoom_steps[p]<8)t->map_zoom_steps[p]++;
                }
                if(edge&(1u<<AVP_MP_MAP_ZOOM_OUT)){
                    if(t->map_zoom_steps[p]>-8)t->map_zoom_steps[p]--;
                }
                t->map_zoom_held[p]=held;
            }
            /* PC weapon cycle is host-only, not a Jaguar pad bit. Store
             * each player's press edge and apply it with THEIR original
             * weapon globals bound, before their simulation step. */
            /* P1 normally owns the legacy wheel/key delta. When P1 is
             * AI-controlled, its NEXT/PREV command instead needs the same
             * native weapon edge bridge as every other bot. Headless bot
             * slots P5-P20 are simulation actors, not viewports; R22 only
             * processed p<t->players here, so their weapon requests never
             * reached avp_weapons_cycle() despite passing the AI unit tests. */
            for(unsigned p=0;p<t->actors;p++){
                if(p==0u && !t->session.setup.bot_enabled[p]){
                    t->weapon_cycle[p]=t->p1_weapon_delta;
                    t->prior_weapon_buttons[p]=0u;
                }else{
                    t->weapon_cycle[p]=avp_mp_input_weapon_cycle_edge(
                        cmd[p].actions,&t->prior_weapon_buttons[p]);
                }
            }
            if(ticks%10u==0u){
                unsigned any=0;
                for(unsigned p=0;p<t->players;p++)any|=cmd[p].actions;
                if(any){
                    fprintf(stderr,"MP key samples:");
                    for(unsigned p=0;p<t->players;p++)
                        fprintf(stderr," P%u=%08x",p+1u,cmd[p].actions);
                    fprintf(stderr," pad=%08x\n",(unsigned)t->last_pad);
                }
            }
            /* Each original level has an independent door/AMP world, but
             * destruction of Golgotha is one campaign-wide timer. Seed
             * every occupied world with the SAME previous completed HUD
             * tick before its normal native simulation. */
            if(t->shared_countdown_valid)for(unsigned w=0;w<AVP_LOCAL_MP_MAX_ACTORS;w++){
                AvpMpActiveWorld *active=&t->session.worlds[w];
                AvpMpNativeWorld *native=(AvpMpNativeWorld *)active->context;
                if(native && active->member_mask)
                    native->hud_world=t->shared_countdown;
            }
            if(!avp_mp_session_tick(&t->session,cmd))failed=1;
            /* All original world tick paths advanced to the SAME session
             * time. Snapshot one resulting countdown to seed the next tick.
             * A later level transition must not restart its 2-minute clock. */
            if(!failed && t->shared_countdown_valid)
                for(unsigned w=0;w<AVP_LOCAL_MP_MAX_ACTORS;w++){
                    const AvpMpActiveWorld *active=&t->session.worlds[w];
                    const AvpMpNativeWorld *native=(const AvpMpNativeWorld *)active->context;
                    if(native && active->member_mask &&
                       avp_mp_hudworld_native_valid(&native->hud_world)){
                        t->shared_countdown=native->hud_world;
                        break;
                    }
                }
            if(!failed){
                /* Do not switch the camera when the subject dies. Follow
                 * their original death fade and subsequent respawn. */
                walk_refresh_audio_listeners(t);
                avp_mp_dm_tick(&t->dm);
            }
            for(unsigned p=0;!failed && p<t->players;p++){
                const AvpMpActorNativeState *a=
                    (const AvpMpActorNativeState *)t->session.player[p];
                if(!a)continue;
                /* Original script, mouse, or digital yaw always wins. */
                if(t->live_heading[p].active && a->player.centre_angle!=
                       avp_pc_live_heading_word(&t->live_heading[p]))
                    avp_pc_live_heading_reset(&t->live_heading[p]);
                t->live_last_game_angle[p]=a->player.centre_angle;
            }
            /* The Queen ending sets game_over inside the original AMP.S
             * state machine, using the selected actor's scoped globals.
             * Unlike single-player, this host has no PlayAvP outer loop;
             * recognize the saved actor event before the next world tick. */
            if(!failed)for(unsigned p=0;p<t->players;p++){
                const AvpMpActorNativeState *a=
                    (const AvpMpActorNativeState *)t->session.player[p];
                if(a && a->player.game_over && !t->dm.enabled){
                    victory_ending=1;victory_countdown=10u;
                    FadeDown();
                    fprintf(stderr,"MP original campaign game_over: P%u, species %u; ending fade begun.\n",
                            p+1u,t->species);
                    break;
                }
            }
            t->pending_actions=0u;
            if(victory_ending){
                /* Original PlayAvP leaves the gameplay loop once its ending
                 * sequence begins. Do not dispatch stale exit requests or
                 * mutate worlds after the Queen has set game_over. */
                ++ticks;
                next_tick+=sim_period;
                continue;
            }
            if(!failed&&!walk_sync_ambient(t))failed=1;
            /* The old walktest silently read ONE global new_level. Four
             * independently latched original exit requests are now visible.
             * STOP here: the current source factory only owns this one
             * retail level, and a wrong-level loader would corrupt worlds. */
            /* Terminal requests are PLAYER-owned, not shared DOORS.S events.
             * Original COMPUTER.S is modal and suspends the entire co-op
             * simulation while the operator browses. Process simultaneous
             * actors deterministically in player order. */
            if(!failed && t->terminal_pending_mask){
                for(unsigned p=0;p<t->actors;p++)if(t->terminal_pending_mask&(1u<<p)){
                    int status=walk_run_terminal(t,p);
                    if(!status){failed=1;break;}
                    if(status==2){
                        victory_ending=1;victory_countdown=10u;
                        FadeDown();
                        /* No additional computer should run after the pod
                         * launch: the original campaign has ended. */
                        t->terminal_pending_mask=0;
                        break;
                    }
                }
                /* Wall-clock time spent reading a modal terminal must NOT
                 * produce a burst of catch-up game ticks on return. */
                {uint64_t reset_now=avp_pc_now_ns();
                 next_tick=reset_now+sim_period;
                 next_display=reset_now+(1000000000ull/60ull);
                 next_render=next_display;}
            }
            if(new_level){
                /* Timed lift doors can request a world-level exit after the
                 * actor callback: there is NO assigned player here yet.
                 * Fail rather than quietly sending the wrong participant. */
                fprintf(stderr,"MP unowned world-level exit to %d: cannot assign lift passenger safely.\n",(int)new_level);
                failed=1;
            }
            for(unsigned p=0;p<t->actors;p++){
                AvpMpTransitionRequest request;
                const AvpMpActiveWorld *old_world;
                unsigned old_level;
                u32 old_level_effect=0;
                int moved=0;
                if(!avp_mp_transition_peek(&t->transitions,p,&request))continue;
                old_world=avp_mp_session_player_world(&t->session,p);
                if(!old_world){failed=1;break;}
                old_level=old_world->level;
                if(walk_is_duct_transfer(old_level,&request)){
                    AvpMpLife *fade=&t->vent_fade[p];
                    if(fade->phase==AVP_MP_LIFE_ALIVE ||
                       (fade->phase==AVP_MP_LIFE_FADE_OUT && fade->fade==0u)){
                        const AvpRuntimeOps *runtime=avp_runtime_ops();
                        avp_mp_life_begin(fade);
                        if(runtime && runtime->play_sfx){
                            const AvpMpActorNativeState *a=
                                (const AvpMpActorNativeState *)t->session.player[p];
                            if(a)avp_jaguar_fullsynth_mp_set_sound_source(p,old_level,
                                        (s32)a->player.x_pos,(s32)a->player.y_pos);
                            runtime->play_sfx(runtime->user,AVP_FX_DUCT1);
                            if(a)avp_jaguar_fullsynth_mp_clear_sound_source();
                        }
                        fprintf(stderr,"MP P%u original duct transfer fade-out: %u -> %u.\n",
                            p+1u,old_level,(unsigned)request.target_level);
                    }
                    if(fade->phase==AVP_MP_LIFE_FADE_OUT)continue;
                    if(fade->phase!=AVP_MP_LIFE_WAIT_LEVEL)continue;
                }
                {
                    const AvpMpNativeWorld *native=(const AvpMpNativeWorld *)old_world->context;
                    if(native)old_level_effect=native->doors.level_effect_id;
                }
                t->entering[p]=request;
                t->entering_reset[p]=0;
                /* Only real original airlock/lift exits pick up other players.
                 * Ducts, cocoon escapes and other transitions remain solo. */
                const int bot_interlock=t->session.setup.bot_enabled[p] &&
                    t->exit_kind[p]==AVP_DOORS_EXIT_AIRLOCK;
                unsigned passengers=(!bot_interlock ||
                    walk_bot_airlock_commit_ready(
                        (const AvpMpNativeWorld *)old_world->context,
                        (const AvpMpActorNativeState *)t->session.player[p],&request))?
                    walk_room_passengers(t,p,&request):0u;
                /* Existing refusal path consumes the stale request and
                 * releases its pressure cue. Next tick the bot replans
                 * the closing sequence instead of teleporting. */
                if(!passengers){
                    moved=0;
                }else if(passengers!=(AVP_MP_ACTOR_BIT(p)) &&
                   old_world->level!=(unsigned)request.target_level){
                    moved=avp_mp_session_move_group(&t->session,passengers,
                                                    (unsigned)request.target_level);
                }else if(old_world->level==(unsigned)request.target_level){
                    /* A same-level duct/cocoon return is a position change,
                     * not a new world allocation. Keep shared state intact. */
                    AvpMpNativeWorld *ctx=(AvpMpNativeWorld *)old_world->context;
                    moved=walk_join(t,ctx,p,(AvpMpActorNativeState *)t->session.player[p]);
                }else moved=avp_mp_session_move(&t->session,p,(unsigned)request.target_level);
                for(unsigned q=0;q<t->actors;q++)if(q==p || (passengers&(AVP_MP_ACTOR_BIT(q))))
                    memset(&t->entering[q],0,sizeof(t->entering[q]));
                if(moved && !walk_clear_arrival_enemies(t,passengers)){
                    fprintf(stderr,"MP committed destination AMP entry cleanup failed.\n");
                    failed=1;break;
                }
                if(moved && old_level!=(unsigned)request.target_level){
                    for(unsigned q=0u;q<t->actors;q++)
                        if((passengers&AVP_MP_ACTOR_BIT(q)) && t->session.setup.bot_enabled[q]){
                            const AvpMpActorNativeState *passenger=t->session.player[q];
                            if(passenger)avp_mp_bot_duct_arrived(&t->bots[q].duct_arrival,
                                old_level,(unsigned)request.target_level,
                                passenger->player.x_pos,passenger->player.y_pos,t->session.tick);
                        }
                }
                if(moved && old_level!=(unsigned)request.target_level &&
                   t->exit_kind[p]==AVP_DOORS_EXIT_AIRLOCK){
                    /* R299: an entire co-op transfer is one transaction.
                     * Latch EVERY transported bot (including passengers
                     * who never pressed Use) against immediately operating
                     * the reverse portal on the destination side. */
                    for(unsigned q=0u;q<t->actors;q++)
                        if((passengers&AVP_MP_ACTOR_BIT(q)) &&
                           t->session.setup.bot_enabled[q]){
                            AvpMpBotState *memory=&t->bots[q];
                            avp_mp_airlock_cycle_arrived(&memory->airlock_cycle,
                                old_level,(unsigned)request.target_level,
                                t->session.tick);
                            memory->airlock_level=0u;
                            memory->airlock_panel_offset=0u;
                            memory->airlock_destination=0u;
                            memory->airlock_since=0u;
                            memory->airlock_last_intent=0u;
                            t->bot_level_cache[q].valid=0u;
                        }
                }
                if(moved && old_level!=(unsigned)request.target_level &&
                   t->exit_kind[p]==AVP_DOORS_EXIT_LIFT){
                    /* The source transition clears exit_kind for every
                     * passenger before the next bot observation. A bot
                     * carried as a passenger may never have set its own
                     * last_bot_goal_lift, so latch each transferred bot's
                     * destination arrival NOW, from its actual new-world
                     * position. Do not infer it later from a new goal. */
                    for(unsigned q=0u;q<t->actors;q++)
                        if((passengers&AVP_MP_ACTOR_BIT(q)) &&
                           t->session.setup.bot_enabled[q]){
                            const AvpMpActorNativeState *passenger=
                                (const AvpMpActorNativeState*)t->session.player[q];
                            if(!passenger)continue;
                            t->lift_arrival_pending[q]=1u;
                            t->lift_arrival_x[q]=passenger->player.x_pos>>16;
                            t->lift_arrival_y[q]=passenger->player.y_pos>>16;
                            t->lift_egress_valid[q]=0u;
                        }
                }
                if(moved)walk_shared_arrival_refresh(t,passengers);
                if(moved && !walk_arm_alien_end_queen(t,(unsigned)request.target_level)){
                    fprintf(stderr,"MP deferred campaign objective arm failed; preserving fatal error.\n");
                    failed=1;break;
                }
                if(moved){
                    if(t->vent_fade[p].phase==AVP_MP_LIFE_WAIT_LEVEL)
                        avp_mp_life_resumed(&t->vent_fade[p]);
                    /* The original single-player level entry always ends
                     * the transfer cue. Do it only after group commit. */
                    walk_finish_exit_effect(t,old_level,old_level_effect);
                    /* A grouped lift/airlock may contain independently
                     * queued original requests from two players in the same
                     * tick. Commit each only AFTER all passengers arrived,
                     * or the second actor would immediately exit AGAIN using
                     * the stale request on the next host tick. Passengers
                     * who did not press Use have no queue entry to consume. */
                    for(unsigned q=0;q<t->actors;q++)if(passengers&(AVP_MP_ACTOR_BIT(q))){
                        AvpMpTransitionRequest consumed;
                        (void)avp_mp_transition_commit(&t->transitions,q,&consumed);
                    }
                    for(unsigned q=0;q<t->actors;q++)if(passengers&(AVP_MP_ACTOR_BIT(q))){
                        avp_pc_quick_turn_cancel(&t->quick_turn[q]);
                        t->exit_kind[q]=AVP_DOORS_EXIT_NONE;
                        const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[q];
                        t->prev_x[q]=a->player.x_pos;t->prev_y[q]=a->player.y_pos;
                        t->prev_angle[q]=a->player.centre_angle;
                        avp_pc_live_heading_reset(&t->live_heading[q]);
                        t->live_last_game_angle[q]=a->player.centre_angle;
                    }
                    {const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
                     t->prev_x[p]=a->player.x_pos;t->prev_y[p]=a->player.y_pos;
                     t->prev_angle[p]=a->player.centre_angle;
                     avp_pc_live_heading_reset(&t->live_heading[p]);
                     t->live_last_game_angle[p]=a->player.centre_angle;}
                    fprintf(stderr,"MP P%u original retail transfer complete: %u -> %u with %u room passenger(s) (other worlds retained).\n",
                            p+1u,old_level,(unsigned)request.target_level,
                            (unsigned)__builtin_popcount(passengers)-1u);
                }else{
                    if(t->vent_fade[p].phase==AVP_MP_LIFE_WAIT_LEVEL)
                        avp_mp_life_resumed(&t->vent_fade[p]);
                    /* A failed group arrival must not leave a looping
                     * pressure/elevator cue indefinitely, either. */
                    walk_finish_exit_effect(t,old_level,old_level_effect);
                    fprintf(stderr,"MP P%u original retail exit to %u refused; prior world intact.\n",
                            p+1u,(unsigned)request.target_level);
                    {AvpMpTransitionRequest ignored;
                     (void)avp_mp_transition_commit(&t->transitions,p,&ignored);}
                    t->exit_kind[p]=AVP_DOORS_EXIT_NONE;
                    /* Rejection is not a fatal session error: a destination
                     * elevator may already contain another player's actor.
                     * Original world state and both player memberships stay
                     * intact, so another activation can safely retry. */
                    continue;
                }
            }
            if(!failed&&!walk_sync_ambient(t))failed=1;
            for(unsigned p=0;p<t->actors;p++){
                const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
                if(!a)failed=1;
                /* Death/game-over are player-owned lifecycle transitions,
                 * not an entire co-op session failure. */
            }
            walk_shared_arrival_refresh(t,0);
            ++ticks;
            next_tick+=sim_period;
            if(now>=next_tick && now-next_tick>sim_period)next_tick=now+sim_period;
            if(getenv("AVP_BOT_PLAYTEST_TRACE") && ticks%5u==0u){
                for(unsigned bp=0u;bp<t->actors;bp++){
                    const AvpMpActorNativeState *ba=(const AvpMpActorNativeState *)t->session.player[bp];
                    const AvpMpActiveWorld *bw=avp_mp_session_player_world(&t->session,bp);
                    if(ba)fprintf(stderr,"BOTSTATE tick=%llu p=%u bot=%u level=%u xy=%08x/%08x hp=%d heading=%08x weapon=%d ammo=%u,%u,%u,%u\n",
                        (unsigned long long)ticks,bp+1u,
                        t->session.setup.bot_enabled[bp],bw?bw->level:0u,
                        ba->player.x_pos,ba->player.y_pos,ba->player.player_energy,
                        ba->player.centre_angle,ba->weapon.cur_wepno,
                        ba->weapon.ammo_info[0].cur,ba->weapon.ammo_info[1].cur,
                        ba->weapon.ammo_info[2].cur,ba->weapon.ammo_info[3].cur);
                }
            }
            if(ticks==1u || ticks%20u==0u){
                fprintf(stderr,"MP original tick %lu:",ticks);
                for(unsigned p=0;p<t->players;p++){
                    const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
                    fprintf(stderr," P%u %08x/%08x facing %08x",p+1u,
                        a?a->player.x_pos:0,a?a->player.y_pos:0,a?a->player.centre_angle:0);
                }
                fputc('\n',stderr);
                if(t->combat_test){
                    /* Preserve the exact S67/S68 two-player diagnostic
                     * contract consumed by existing retail regressions. */
                    if(t->players==2u){
                        const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[0];
                        const AvpMpActorNativeState *b=(const AvpMpActorNativeState *)t->session.player[1];
                        fprintf(stderr,"MP original combat %lu: P1 hp=%d weapon=%d ammo0=%u fire=%u P2 hp=%d weapon=%d ammo0=%u fire=%u original AMP modes=%lu\n",
                            ticks,a->player.player_energy,a->weapon.cur_wepno,a->weapon.ammo_info[0].cur,(unsigned)a->player.wep_fire,
                            b->player.player_energy,b->weapon.cur_wepno,b->weapon.ammo_info[0].cur,(unsigned)b->player.wep_fire,
                            t->amp_target_requests);
                    }else{
                        fprintf(stderr,"MP original combat %lu:",ticks);
                        for(unsigned p=0;p<t->players;p++){
                            const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
                            if(a)fprintf(stderr," P%u hp=%d weapon=%d ammo0=%u fire=%u",p+1u,
                                a->player.player_energy,a->weapon.cur_wepno,
                                a->weapon.ammo_info[0].cur,(unsigned)a->player.wep_fire);
                        }
                        fprintf(stderr," original AMP modes=%lu weapon updates=%lu",
                            t->amp_target_requests,t->weapon_updates);
                        for(unsigned p=0;p<t->players;p++)
                            fprintf(stderr," audioP%u=%llu",p+1u,
                                (unsigned long long)avp_jaguar_fullsynth_mp_weapon_start_count(p));
                        fputc('\n',stderr);
                    }
                }
            }
        }
        /* Independent PC-only 60/120 Hz rasterization. Jaguar VBL stays
         * at 60 Hz above; never redraw game ticks in this branch. */
        if(now>=next_render && !failed){
            const uint64_t render_now=avp_pc_now_ns();
            const double since=render_now>next_tick-sim_period?
                (double)(render_now-(next_tick-sim_period))/(double)sim_period:0.0;
            t->presentation_alpha=since>1.0?1.0:since;
            /* Preserve a dying subject's original 60-Hz fade. */
            avp_pc_video_local_mp_render_alpha(video,t->presentation_alpha);
            if(!avp_pc_video_present_local_mp(video,&t->session))failed=1;
            next_render=avp_presentation_next_display_deadline(
                next_render,avp_pc_now_ns(),render_period);
        }
        {const uint64_t after=avp_pc_now_ns();
         const uint64_t budget=avp_presentation_sleep_budget(after,
             next_tick,next_display,next_render,2000000ull);
         if(budget)avp_pc_sleep_ns(budget);
        }
    }
    if(t->combat_test){
        fprintf(stderr,"MP audio original weapon effect starts:");
        for(unsigned p=0;p<t->actors;p++)
            fprintf(stderr," P%u=%llu",p+1u,
                 (unsigned long long)avp_jaguar_fullsynth_mp_weapon_start_count(p));
        fputc('\n',stderr);
    }
    fprintf(stderr,"RE27 original session ended after %lu ticks (fault=%d, victory=%d); weapon updates=%lu AMP target visits=%lu.\n",
            ticks,failed,completed,t->weapon_updates,t->amp_target_requests);
    fprintf(stderr,"MP completed individual respawns: %lu.\n",t->completed_respawns);
    /* On close, restart, or fatal exit release the shared mixer even when
     * the last action was a pause. Never leak a muted audio engine into title. */
    avp_jaguar_fullsynth_mp_set_paused(0);
    avp_pc_video_local_mp_set_paused(video,0);
    avp_pc_video_local_mp_deathmatch_status(video,NULL);
    walk_cleanup(t);
    avp_hud_set_coop_countdown(0u);
    player_type=host_campaign_type;
    avp_pc_video_local_mp_walktest_mode(video,0);
    free(t);
    return failed?AVP_MP_RUN_ERROR:(start_custom_from_cheat?AVP_MP_RUN_CUSTOM_MAP:
           (restart_from_f2?AVP_MP_RUN_RESTART:
           (return_to_title?AVP_MP_RUN_TITLE:
           (completed?AVP_MP_RUN_COMPLETED:AVP_MP_RUN_EXIT))));
}
