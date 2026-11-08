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
#include "pc/pc_original_cheats.h"
#include "local_mp_respawn_policy.h"
#include "savegame.h"
#include "eeprom.h"
#include "local_mp_life.h"
#include "local_mp_deathmatch.h"
#include "local_mp_kill_notice.h"
#include "local_mp_dm_loot.h"
#include "local_mp_death_camera.h"
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
#include "local_mp_tracker_peers.h"
#include "local_mp_enhancement_buffer.h"
#include <math.h>
#include "avp_resources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    /* Security Keycards persist per actor, not as a single consumed AMP.
     * Reset only the dying player's record upon a successful respawn. */
    u16 collected_keycards[AVP_LOCAL_MP_MAX_ACTORS];
    /* Current-world item stays alive for all players; ownership is per life
     * and per physical AMP slot. An AMP reused for a different item must
     * invalidate stale claims without resetting other worlds. */
    uint16_t collected_items[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    uint64_t collected_identity[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    /* Deathmatch corpse attempts persist through respawns. New AMP slot
     * generations invalidate stale claims; other pickups still reset per life. */
    uint64_t dm_body_identity[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    uint16_t dm_body_claimed[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
    unsigned collecting_player;
    u32 spawn_x[AVP_LOCAL_MP_MAX_ACTORS];
    u32 spawn_y[AVP_LOCAL_MP_MAX_ACTORS];
    AvpMpSharedArrivalGrace shared_arrival; /* PC-only pair grace */
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
    AvpMpDeathmatch dm;
    unsigned dm_shooter;
    unsigned dm_amp_target; /* actor currently scoped for native AMP AI */
    /* A native AMP slot can be reused: validate its true world-specific
     * generation before crediting projectile hits to a player. */
    uint8_t dm_projectile_owner[AVP_MP_RETAIL_LEVELS][AVP_NUM_AMPS];
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
        actor->player.player_type==PT_HUMAN?99u:actor->access_level,NULL,NULL};
    if(map.width>64u||map.height>64u||!map.width||!map.height||
       !avp_mp_bot_components(&map,components))return 0;
    /* A dead player may be in a detached/invalid floor island. Anchoring
     * the next random spawn to that last position perpetuates the error.
     * Always use the original authored start pose for THIS level, never
     * the player's last position (or a previous level's start pose). */
    if(!avp_levels_start_pose(world->map->level,
                              &origin_x,&origin_y,&origin_angle) ||
       !walk_spawn_on_retail_map(world->map,origin_x,origin_y) ||
       SafePos(80,(s32)origin_x,(s32)origin_y,40)!=AVP_COLL_SAFE)
        return 0; /* Fail closed instead of selecting an arbitrary island. */
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
    if(found && t->dm.enabled && t->entering_reset[p]){
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
static int walk_join_group(void *u,AvpMpNativeWorld *w,unsigned mask,
                           AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS])
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    u32 anchor_x=0u,anchor_y=0u;
    int anchor_found=0;
    unsigned fallback_mask=0u;
    for(unsigned p=0;p<t->actors;p++)if(mask&(1u<<p)){
        u32 requested_x=(u32)t->entering[p].new_x;
        u32 requested_y=(u32)t->entering[p].new_y;
        int requested=t->entering[p].valid && t->entering[p].new_x>=0 &&
                      t->entering[p].new_y>=0;
        if(!actors[p]||!walk_join(u,w,p,actors[p]))return 0;
        /* A normal valid translated pose is kept exactly. A position that
         * walk_join had to repair should instead stack with the group. */
        if(requested && (actors[p]->player.x_pos!=requested_x ||
                         actors[p]->player.y_pos!=requested_y))
            fallback_mask|=1u<<p;
        else if(!anchor_found){
            anchor_x=actors[p]->player.x_pos;
            anchor_y=actors[p]->player.y_pos;
            anchor_found=1;
        }
    }
    if(!anchor_found){
        /* All requested positions needed repair: share the first repaired
         * in-bounds floor cell, never scatter the passengers. */
        for(unsigned p=0;p<t->actors;p++)if(mask&(1u<<p)){
            anchor_x=actors[p]->player.x_pos;anchor_y=actors[p]->player.y_pos;
            anchor_found=1;break;
        }
    }
    if(!anchor_found)return 0;
    for(unsigned p=0;p<t->actors;p++)if(fallback_mask&(1u<<p)){
        actors[p]->player.x_pos=anchor_x;
        actors[p]->player.y_pos=anchor_y;
        actors[p]->player.x_vel=actors[p]->player.y_vel=0;
        actors[p]->player.new_pos=0;
        if(!avp_mp_actor_native_valid(actors[p]))return 0;
    }
    return 1;
}
/* The co-op host tracks pairs that have just been placed on the same
 * valid floor point. There is currently no inter-player solid collision;
 * retaining these pair masks makes the landing contract explicit for the
 * future optional player collision setting without changing retail physics. */
static void walk_shared_arrival_refresh(AvpMpWalkHost *t,unsigned arriving_mask)
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
    unsigned mask=1u<<leader;
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
        mask|=1u<<p;
    }
    /* Publish passenger staging only once ALL valid offsets are known. */
    for(unsigned p=0;p<t->actors;p++)if(p!=leader && (mask&(1u<<p)))
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
static void walk_refresh_audio_listeners(AvpMpWalkHost *t)
{
    AvpMpSpatialListener listeners[4]={{0}};
    unsigned bot_mask=0u,views=0u;
    if(!t)return;
    for(unsigned p=0;p<t->actors && p<AVP_LOCAL_MP_MAX_ACTORS;p++)
        if(t->session.setup.bot_enabled[p])bot_mask|=1u<<p;
    for(unsigned p=0;p<t->players;p++){
        const AvpMpActiveWorld *w=avp_mp_session_player_world(&t->session,p);
        const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
        if(!w||!a||!w->level||w->level>15u)continue;
        /* The display is an audio listener regardless of who controls it.
         * Otherwise an AI-controlled P2/P3/P4 viewport only hears its own
         * positional sounds when P1 (or another human viewport) is nearby. */
        listeners[p].active=avp_mp_spatial_view_listener(p,t->players,w->level);
        listeners[p].level=w->level;
        listeners[p].x=(s32)a->player.x_pos;
        listeners[p].y=(s32)a->player.y_pos;
        listeners[p].heading=a->player.centre_angle;
        views+=listeners[p].active;
    }
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
    for(unsigned p=0;p<t->actors;p++){
        const AvpMpActiveWorld *w=avp_mp_session_player_world(&t->session,p);
        if(!w)return 0;
        levels[p]=w->level;
    }
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
    if(avp_mp_corpse_claim_at(&t->corpses,level,p,x_pos,y_pos))
        walk_dm_award_body_loot();
    /* Fallen native enemy and authored AMP corpses retain their original
     * collection rules. Only an AC_HUMAN player corpse grants random gear. */
}
static int walk_mp_item_available(AvpMpWalkHost *t,unsigned p,
            unsigned level,unsigned slot,const AvpAmp *amp)
{
    uint16_t bit;
    uint64_t identity;
    if(!t||p>=t->actors||p>=AVP_LOCAL_MP_MAX_ACTORS ||
       level<1u||level>AVP_MP_RETAIL_LEVELS ||slot>=AVP_NUM_AMPS ||!amp)
        return 0;
    bit=(uint16_t)(1u<<p);
    identity=walk_pickup_identity(amp);
    if(t->collected_identity[level-1u][slot]!=identity)return 1;
    return !(t->collected_items[level-1u][slot]&bit);
}
static int walk_claim_mp_item(void *u,unsigned level,unsigned slot,s16 type,int commit)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    unsigned p;
    uint16_t bit;
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
    bit=(uint16_t)(1u<<p);
    if(t->collected_identity[level-1u][slot]==identity &&
       (t->collected_items[level-1u][slot]&bit))return 0;
    if(commit)t->collected_items[level-1u][slot]|=bit;
    return 1;
}
static void walk_reset_mp_item_claims(AvpMpWalkHost *t,unsigned p)
{
    unsigned level,slot;
    const uint16_t keep=(uint16_t)~(1u<<p);
    if(!t||p>=AVP_LOCAL_MP_MAX_ACTORS)return;
    for(level=0;level<AVP_MP_RETAIL_LEVELS;level++)
        for(slot=0;slot<AVP_NUM_AMPS;slot++)
            t->collected_items[level][slot]&=keep;
}

static int walk_actor(void *u,unsigned p)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    AvpMpWeaponAudioState old_host;
    int audio_active=t->combat_test;
    s16 previous_level=new_level;
    s32 previous_x=new_x,previous_y=new_y;
    u32 prior_exit_effect=levfx_ID;
    int result=1;
    /* Each dead player owns a 60-Hz fade. Neither their original movement
     * nor weapons advance while waiting for their own respawn. Their renderer
     * AMP snapshot is refreshed separately before each world update. */
    if(avp_mp_life_busy(&t->player_life[p]) ||
       t->vent_fade[p].phase==AVP_MP_LIFE_FADE_OUT ||
       t->vent_fade[p].phase==AVP_MP_LIFE_WAIT_LEVEL)return 1;
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
    {
        const AvpMpInputConfig *cfg=avp_pc_video_local_mp_profiles(t->video);
        const AvpMpInputProfile *pref=cfg&&p<AVP_LOCAL_MP_MAX_PLAYERS?
            &cfg->player[p]:NULL; /* Standalone bots never borrow P2 cheats. */
        const int primary=(p==0u);
        avp_dev_cheats_tracker_borrowed_restore(t->tracker_cheat_borrowed[p]);
        avp_dev_cheats_set_infinite_health(primary?avp_pc_video_infinite_health(t->video):
                 (pref?pref->cheat_infinite_health:0));
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
        avp_pc_runtime_set_mp_fast_sweep_guard(t->dm.enabled && t->dm.fast_movement);
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
    avp_doors_mp_restrict_interactions(t->dm.enabled,
                                      t->dm.level_travel);
    avp_mp_player_native_step_actor();
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
    if(!t->combat_test){UpdtHUDMsg();UpdtHUD();}
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
        t->terminal_pending_mask|=1u<<p;
        t->terminal_pending_panel[p]=comp_panel;
        t->terminal_pending_offset[p]=comp_offset;
        comp_panel=0;
    }
    if(new_level){
        t->exit_kind[p]=avp_doors_host_exit_kind();
        if(!avp_mp_transition_queue(&t->transitions,p,new_level,
                                     new_x,new_y,use_cocoon))result=0;
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
    avp_pc_runtime_set_noclip(avp_pc_video_noclip(t->video));
    avp_player_set_pain_sounds(avp_pc_video_player_pain_sounds_enabled(t->video));
    avp_player_set_facehug_pain_sounds(avp_pc_video_facehug_pain_sounds_enabled(t->video));
    avp_mazescrn_set_facehug_shake_pain_sounds(avp_pc_video_facehug_shake_sounds_enabled(t->video));
    if(p<t->players && !avp_pc_video_local_mp_hud_message_route(t->video,-1))result=0;
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
                              unsigned mask,const AvpMpActorNativeState *const actors[AVP_LOCAL_MP_MAX_ACTORS])
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
    for(unsigned i=0;i<AVP_LOCAL_MP_MAX_ACTORS;i++)if(mask&(1u<<i)){
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
            t->dm_amp_target=i;
            return (int)i;
        }
        if(d<best_dist){best_dist=d;best=i;}
        if(clear && d<visible_dist){visible_dist=d;visible=i;}
    }
    best=visible!=~0u?visible:best;
    if(best!=~0u){
        const AvpMpInputProfile *pref=&t->profiles.player[best<AVP_LOCAL_MP_MAX_PLAYERS?best:1u];
        /* The original AMP mode runs with one actor scoped at a time.
         * Rebind only that target's co-op cheat flags, never another player. */
        avp_amp_set_attacks_disabled(best==0u?
            avp_pc_video_attacks_disabled(t->video):pref->cheat_no_enemy_attack);
        avp_amp_set_facehug_attach_disabled(best==0u?
            avp_pc_video_facehug_attach_disabled(t->video):pref->cheat_no_facehug);
    }
    t->amp_locked_player[(cur_level>=1&&cur_level<=15)?(unsigned)cur_level-1u:0u][amp_index]=best==~0u?-1:(int8_t)best;
    if(best!=~0u)t->dm_amp_target=best;
    return best==~0u?-1:(int)best;
}
static int walk_update_view_contacts(AvpMpWalkHost *t,unsigned p,
                                     const AvpMpNativeWorld *world);

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
    if(player>=t->actors || avp_mp_life_busy(&t->player_life[player]))return 1;
    if(player<t->players && !avp_pc_video_local_mp_hud_message_route(t->video,(int)player))return 0;
    if(!avp_music_mp_weapon_enter(&old_host,&t->weapon_voice[player])){
        if(player<t->players)(void)avp_pc_video_local_mp_hud_message_route(t->video,-1);
        return 0;
    }
    avp_jaguar_fullsynth_mp_set_weapon_owner(player);
    avp_jaguar_fullsynth_mp_set_sound_source(player,(unsigned)cur_level,x_pos,y_pos);
    /* The original scanner reads its nearest contact at HUD update time,
     * before the next render. Refresh this actor's real same-level roster
     * here so player motion also drives the normal 10-Hz tracker click. */
    {const AvpMpActiveWorld *active=avp_mp_session_player_world(&t->session,player);
     if(!active || (player<t->players && !walk_update_view_contacts(t,player,
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
    UpdtHUDMsg();
    /* The original HP2 calculation now receives *this listener's* actual
     * voice PCM samples: own sounds always, others only if same original
     * level and inside the co-op visual meter's proximity range. No change
     * to host audio output or Jaguar weapon/AI behavior. */
    {s32 l=0,r=0;
        avp_jaguar_fullsynth_mp_meter_near(player,(unsigned)cur_level,x_pos,y_pos,&l,&r);
        avp_hud_set_pred_meter_samples(l,r);
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
    ++t->weapon_updates;
    if(player<t->players && !avp_pc_video_local_mp_hud_message_route(t->video,-1))ok=0;
    return ok;
}
/* Run while the original level is scoped, immediately BEFORE the shared
 * world's AMP update. Alive actors already snapshot in walk_actor; dying
 * actors skip that callback, but their fixed-camera view still shows moving
 * enemies. Refresh only their renderer history, without stepping movement,
 * weapons, death fade, or world simulation again. */
static int walk_snapshot_dying_views(void *u,unsigned member_mask)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)u;
    const u8 *maze=avp_collision_maze_data();
    if(!t || !maze || !maze_width || !maze_height)return 0;
    for(unsigned p=0;p<t->players;p++)if((member_mask&(1u<<p)) &&
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
     if(level && avp_mp_doors_native_pending_lift(&lx,&ly)){
        int valid=0;
        if(owner>=0 && (unsigned)owner<t->actors){
            const AvpMpActiveWorld *assigned=avp_mp_session_player_world(&t->session,(unsigned)owner);
            const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[(unsigned)owner];
            valid=assigned && assigned->level==level && a &&
                (u16)(a->player.x_pos>>16)==lx &&
                (u16)(a->player.y_pos>>16)==ly && a->player.player_energy>0;
        }
        if(!valid){
            owner=-1;
            for(unsigned q=0;q<t->actors;q++){
                const AvpMpActiveWorld *candidate=avp_mp_session_player_world(&t->session,q);
                const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[q];
                if(candidate && candidate->level==level && a && a->player.player_energy>0 &&
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
        if(member && member->level==level && passenger &&
           (u16)(passenger->player.x_pos>>16)==lx &&
           (u16)(passenger->player.y_pos>>16)==ly){
            x_pos=passenger->player.x_pos;
            y_pos=passenger->player.y_pos;
        }
     }
     avp_doors_host_clear_exit_kind();
     avp_mp_player_native_step_world_doors();
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
           t->vent_fade[p].phase==AVP_MP_LIFE_FADE_OUT ||
           t->vent_fade[p].phase==AVP_MP_LIFE_WAIT_LEVEL ||
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
                                     const AvpMpNativeWorld *world)
{
    AvpGpuMapTeammate dots[AVP_GPU_MAP_TEAMMATES];
    AvpGpuMapTeammate presentation[AVP_GPU_MAP_TEAMMATES];
    unsigned count=0;
    int smooth;
    if(!t||!world||p>=t->players)return 0;
    smooth=avp_pc_video_smooth_presentation(t->video) && t->session.tick!=0;
    for(unsigned q=0;q<t->actors;q++){
        const AvpMpActorNativeState *other;
        const AvpMpActiveWorld *pw;
        pw=avp_mp_session_player_world(&t->session,q);
        other=(const AvpMpActorNativeState *)t->session.player[q];
        if(!other || !avp_mp_tracker_peer_eligible(p,q,world,
                pw?pw->context:NULL,avp_mp_life_busy(&t->player_life[q]),
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
    return avp_pc_video_local_mp_map_teammates(t->video,p,dots,
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

static int walk_draw(void *u,AvpMpNativeWorld *world,unsigned p,
                     AvpMpActorNativeState *actor,AvpLocalMpSourceFrame *out)
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
            if(q==p || avp_mp_life_busy(&t->player_life[q]))continue;
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
                /* Alien and Predator enemy recoil lasts two native ticks.
                 * Track third-person hits independently from the original
                 * PLAYER.S pain/fade clock (which remains unchanged). */
                if(type==PT_ALIEN || type==PT_PREDATOR)
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
                }else if((type==PT_ALIEN || type==PT_PREDATOR) &&
                         t->sprite_pain_valid[q] &&
                         (pose=avp_mp_sprite_enemy_recoil_pose((unsigned)(
                                    t->session.tick-t->sprite_pain_tick[q]))).active){
                    others[visible].animation=pose.sequence;
                    others[visible].frame=pose.frame;
                }else if(type==PT_HUMAN && b->player.pain>0){
                    others[visible].animation=AS_KNOCKBACK;
                    others[visible].frame=0;
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
        if(!avp_pc_video_local_mp_set_other_players(t->video,p,others,visible))return 0;
    }
    if(!walk_update_view_contacts(t,p,world))return 0;
    if(!avp_pc_video_local_mp_render_actor(t->video,p,m->bytes+8u,
          (u16)m->width,(u16)m->height,rx,ry,ra,m->level,
          t->session.tick,out))return 0;
    avp_mp_life_apply(&t->player_life[p],(uint32_t *)(uintptr_t)out->pixels,
                      out->width,out->height,out->pitch);
    avp_mp_life_apply(&t->vent_fade[p],(uint32_t *)(uintptr_t)out->pixels,
                      out->width,out->height,out->pitch);
    /* Render the PC kill confirmation AFTER death/transition fades, so the
     * victim can read KILLED BY even during the black death screen. */
    if(t->dm.enabled){
        unsigned other=0u;
        unsigned kind=avp_mp_kill_notice_visible(&t->dm_notices[p],
                                                 t->session.tick,&other);
        if(kind && !avp_pc_video_local_mp_draw_kill_notice(t->video,p,kind,other,
                                               actor->player.player_type))
            return 0;
    }
    return 1;
}
static void walk_cleanup(AvpMpWalkHost *t)
{
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
        t->host.advance_actor_post_world=walk_post_actor;
    }
    t->host.draw_actor=walk_draw;
    avp_local_mp_defaults(&setup);
    setup.enabled=1;setup.players=t->players;setup.campaign=t->species;
    {const AvpLocalMpSetup *menu=avp_pc_video_local_mp_setup(v);
     if(menu){
         setup.layout=menu->layout;
         setup.respawn_campaign_start=menu->respawn_campaign_start;
         setup.game_mode=menu->game_mode;
         setup.dm_score_limit=menu->dm_score_limit;
         setup.dm_time_limit=menu->dm_time_limit;
         setup.dm_fast_movement=menu->dm_fast_movement;
         setup.dm_fast_equip=menu->dm_fast_equip;
         setup.dm_enemies=menu->dm_enemies;
         setup.dm_level=menu->dm_level;
         setup.dm_level_travel=menu->dm_level_travel;
        setup.dm_cloak_damage=menu->dm_cloak_damage;
         setup.bot_count=menu->bot_count;
         setup.show_whole_map=menu->show_whole_map;
         setup.random_species_on_respawn=menu->random_species_on_respawn;
         memcpy(setup.bot_species,menu->bot_species,sizeof setup.bot_species);
         memcpy(setup.bot_behavior,menu->bot_behavior,sizeof setup.bot_behavior);
         for(unsigned p=0;p<t->players;p++)
             setup.bot_enabled[p]=menu->bot_enabled[p];
     }}
    if(requested){
        AvpLocalMpSetup selected=*requested;
        avp_local_mp_validate(&selected);
        if(selected.players!=t->players || selected.campaign!=t->species)return 0;
        setup.layout=selected.layout;
        setup.respawn_campaign_start=selected.respawn_campaign_start;
        setup.game_mode=selected.game_mode;
        setup.dm_score_limit=selected.dm_score_limit;
        setup.dm_time_limit=selected.dm_time_limit;
        setup.dm_fast_movement=selected.dm_fast_movement;
        setup.dm_fast_equip=selected.dm_fast_equip;
        setup.dm_enemies=selected.dm_enemies;
        setup.dm_level=selected.dm_level;
        setup.dm_level_travel=selected.dm_level_travel;
        setup.dm_cloak_damage=selected.dm_cloak_damage;
        setup.bot_count=selected.bot_count;
        setup.show_whole_map=selected.show_whole_map;
        setup.random_species_on_respawn=selected.random_species_on_respawn;
        memcpy(setup.bot_species,selected.bot_species,sizeof setup.bot_species);
        memcpy(setup.bot_behavior,selected.bot_behavior,sizeof setup.bot_behavior);
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
    walk_shared_arrival_refresh(t,(1u<<t->actors)-1u);
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
                 v->tick-t->patrol[p].last_progress_tick>=25u){
            /* Reject the repeatedly blocked destination until this level
             * is reset; don't choose it again after every 19-tick recovery. */
            if(gi<AVP_MP_BOT_MAP_CELLS)t->patrol[p].visits[gi]=65535u;
            t->patrol[p].valid=0u;
        }
    }
    if(!t->patrol[p].valid && avp_mp_bot_patrol_pick(&v->map,x,y,
         t->patrol[p].visits,(uint32_t)v->tick+(p+1u)*0x9e3779b9u,&tx,&ty)){
        t->patrol[p].x=(tx<<16)|0x8000u;
        t->patrol[p].y=(ty<<16)|0x8000u;
        t->patrol[p].expires=v->tick+180u;
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
    if(t->patrol[p].valid){
        v->has_target=1u;v->hostile_target=0u;
        v->goal=AVP_MP_BOT_PATROL;
        v->target_x=t->patrol[p].x;v->target_y=t->patrol[p].y;
    }
}
/* One observation pass per bot per native 10-Hz tick. An enemy's current
 * position is NEVER handed to the planner unless it was visually observed
 * (forward 140-degree field + original LineOfSight), or is detected by an
 * actually-owned Marine motion tracker inside original 15-m scanner range.
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
        const int sight=usable && avp_bot_in_view(v->heading,v->x,v->y,x,y) &&
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
           avp_mp_life_busy(&t->player_life[q]) ||
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
             &metas[idx],access_level,NULL,NULL};
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
       (t->dm.enabled && !t->dm.level_travel))return 0;
    if(t->retail_catalog_ready){
        /* Cache only for a bounded eight original ticks. Revalidate that
         * the actual chosen portal remains reachable EVERY decision. */
        if(t->bot_level_cache[player].valid &&
           tick>=t->bot_level_cache[player].tick &&
           tick-t->bot_level_cache[player].tick<8u &&
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
static void walk_bot_commands(AvpMpWalkHost *t,
                              AvpMpPlayerCommand cmds[AVP_LOCAL_MP_MAX_ACTORS],
                              uint64_t tick)
{
    for(unsigned p=0;p<t->actors;p++)if(t->session.setup.bot_enabled[p]){
        const AvpMpActorNativeState *a=(const AvpMpActorNativeState *)t->session.player[p];
        const AvpMpActiveWorld *here=avp_mp_session_player_world(&t->session,p);
        AvpMpNativeWorld *world=here?(AvpMpNativeWorld *)here->context:NULL;
        AvpMpBotObservation view={0};
        uint8_t acid_risk[4096],prop_risk[4096];
        uint64_t best=UINT64_MAX;
        unsigned species;
        int marine_empty=0;
        memset(&cmds[p],0,sizeof(cmds[p]));
        if(!a||!world||avp_mp_life_busy(&t->player_life[p])||
           a->player.player_energy<=0 || a->player.game_over)continue;
        species=(unsigned)a->player.player_type>>2;
        view.x=a->player.x_pos;view.y=a->player.y_pos;
        view.behavior=p<t->players?AVP_MP_BOT_BALANCED:
            t->session.setup.bot_behavior[p-t->players];
        view.heading=a->player.centre_angle;view.player_index=p;view.tick=tick;
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
                if((view.marine_owned&(1u<<(i+1u))) && view.marine_ammo[i])
                    marine_empty=0;
            }
        }
        view.player_energy=a->player.player_energy;
        view.max_energy=a->player.max_energy;
        view.medpak=a->weapon.medpak;
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
                a->access_level,acid_risk,prop_risk};
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
            if(!contact->sight && !contact->tracker)continue;
            distance=walk_bot_distance(view.x,view.y,contact->x,contact->y);
            if(distance<=((uint64_t)6u<<16)*((uint64_t)6u<<16))
                view.combat_enemies_nearby++;
        }
        /* Deathmatch players are legitimate hostile contacts too. Their
         * coordinates influence cloaking ONLY after optical detection or
         * another original scanner contact (no all-map opponent census). */
        if(t->dm.enabled)for(unsigned q=0;q<t->actors;q++)if(q!=p &&
            avp_mp_session_player_world(&t->session,q)==here &&
            !avp_mp_life_busy(&t->player_life[q])){
            const AvpMpActorNativeState *opponent=(const AvpMpActorNativeState *)t->session.player[q];
            const AvpMpBotContact *contact=&t->contacts[p][AVP_NUM_AMPS+q];
            if(!opponent || opponent->player.player_energy<=0)continue;
            /* DM roster membership can govern whether to cloak even if
             * the opposing player is out of sight. Never provide their
             * position or a target unless the real contact was observed. */
            view.dm_opponents_on_level++;
            if(!contact->known)continue;
            if(opponent->player.player_type==PT_HUMAN)view.marines_on_level++;
            if((contact->sight||contact->tracker) &&
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
        /* Health search for Marines and Predators at <=45% health; normal
         * pickups MUST be uncollected and still active in the source world.
         * Alien survival never chases a health item it cannot use. */
        if(!view.has_target && species!=AVP_MP_ALIEN &&
           view.max_energy>0 && view.player_energy*100<view.max_energy*45 &&
           (species!=AVP_MP_PREDATOR || view.medpak<=0)){
            for(unsigned i=0;i<AVP_NUM_AMPS;i++){
                const AvpAmp *amp=&world->amp.actors[i];
                uint64_t distance;
                AvpMpBotRoute route;
                unsigned gx=(u32)amp->xpos>>16,gy=(u32)amp->ypos>>16;
                if(!amp->mode && !amp->host_static)continue;
                if(amp->flags&(1u<<AMP_COLLECT) ||
                   !walk_mp_item_available(t,p,world->map->level,i,amp))continue;
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
                view.goal=AVP_MP_BOT_HEALTH;
                view.target_x=(u32)amp->xpos;view.target_y=(u32)amp->ypos;
            }
        }
        /* Currently sighted enemies should interrupt routine health/supply
         * searches throughout the original ranged engagement envelope,
         * not only within the old three-tile panic radius. Radar-only
         * contacts can be approached, but never shot through walls.
         * Favor visible combat over a closer tracker-only contact. */
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
                distance=walk_bot_distance(view.x,view.y,contact->x,contact->y);
                if(!avp_mp_bot_immediate_contact_prefer(species,
                     contact->sight,contact->tracker,distance,
                     selected,selected_sight,nearest))continue;
                nearest=distance;selected=1u;
                selected_sight=contact->sight;threat=amp;
            }
            if(threat){
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
        /* Marine scrounging uses the same read-only authored pickup lookup
           as Collectables(), including corpse loot and security keycards.
           Prefer keycards and missing guns, but leave nearly-full ammo
           pickups until at least the original amount can be used. With no
           loaded guns, seek supplies rather than waiting to die shooting. */
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
                   !walk_mp_item_available(t,p,world->map->level,i,amp))continue;
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
                    else if(!(view.marine_owned&(1u<<weapon)) || missing==0u ||
                            (missing<(unsigned)amount && cur*4u>=max*3u))continue;
                    else if(!any_loaded)prio=0u;
                    else if(ammo_cap && loaded_total*4u>ammo_cap &&
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
                if(any_loaded && view.combat_enemies_nearby && cost>25u)continue;
                if(cost>=best)continue;
                best=cost;view.has_target=1;view.hostile_target=0;
                view.goal=AVP_MP_BOT_SUPPLY;
                view.target_x=(u32)amp->xpos;view.target_y=(u32)amp->ypos;
            }
        }
        if(!view.has_target && !marine_empty){
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
                        if(!contact->sight)continue;
                        distance=2u+(distance>>32);
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
                !avp_mp_life_busy(&t->player_life[q]) &&
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
                !avp_mp_life_busy(&t->player_life[q])){
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
                   (!contact->sight && !contact->tracker))continue;
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
                !avp_mp_life_busy(&t->player_life[q]) &&
                avp_mp_session_player_world(&t->session,q)==here){
                const AvpMpActorNativeState *enemy=(const AvpMpActorNativeState *)t->session.player[q];
                uint64_t d;
                const AvpMpBotContact *contact=&t->contacts[p][AVP_NUM_AMPS+q];
                if(!enemy || enemy->player.player_energy<=0 || !contact->known)continue;
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
        if(!view.has_target)walk_bot_patrol(t,p,&view,world->map->level);
        else if(view.map.cells && view.tick%6u==0u){
            unsigned cx=view.x>>16,cy=view.y>>16;
            /* Memory is accumulated while pursuing fights and supplies too:
             * switching back to exploration never forgets visited corridors. */
            if(t->patrol[p].level==world->map->level && cx<64u && cy<64u &&
               t->patrol[p].visits[cy*64u+cx]<65535u)
                ++t->patrol[p].visits[cy*64u+cx];
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
        avp_mp_bot_step(&t->bots[p],&view,&cmds[p]);
    }
}

/* One Deathmatch damage path for native hits and native projectiles.
 * During AMP update, a single target actor may be scoped into original
 * PLAYER.S globals: write that target to player_energy so the subsequent
 * actor scope capture cannot overwrite its PvP damage. Everyone else is
 * updated in their saved individual snapshot. */
static void walk_dm_apply_hit(AvpMpWalkHost *t,unsigned shooter,
                              unsigned target,int damage)
{
    AvpMpActorNativeState *victim;
    int old_energy,new_energy;
    int scoped;
    if(!t||!t->dm.enabled||t->dm.finished||shooter>=t->actors||
       target>=t->actors||damage<=0||t->dm_death_credited[target]||
       avp_mp_life_busy(&t->player_life[target]))return;
    victim=(AvpMpActorNativeState *)t->session.player[target];
    if(!victim)return;
    scoped=avp_mp_actor_scope_active() && t->dm_amp_target==target;
    old_energy=scoped?player_energy:victim->player.player_energy;
    if(old_energy<=0)return;
    damage=avp_mp_dm_victim_damage(t->dm.cloak_damage,
        victim->player.player_type,
        (unsigned)(scoped?invisflag:victim->player.invisflag),damage);
    new_energy=old_energy-damage;
    if(new_energy<-32768)new_energy=-32768;
    if(scoped)player_energy=(s16)new_energy;
    else victim->player.player_energy=(s16)new_energy;
    if(new_energy<=0 && !(target<t->players && avp_pc_video_original_god_player(t->video,target))){
        avp_mp_dm_kill(&t->dm,shooter,target);
        t->dm_death_credited[target]=1u;
        /* Scored PvP event, once per death: show each human participant
         * their own message. Bot IDs are retained up to player #16. */
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
static void walk_dm_register_projectile(AvpMpWalkHost *t,const AvpAmp *a,unsigned owner)
{
    unsigned level,slot;
    if(!t||owner>=t->actors||!walk_dm_projectile_slot(a,&level,&slot))return;
    t->dm_projectile_owner[level][slot]=(uint8_t)(owner+1u);
    t->dm_projectile_gen[level][slot]=avp_amp_slot_generation(slot);
}
static void walk_dm_projectile_spawn(void *user,const AvpAmp *a)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)user;
    if(t && a && t->dm_shooter<t->actors &&
       (a->flags&(1u<<AMP_PLAYER)))
        walk_dm_register_projectile(t,a,t->dm_shooter);
}
static void walk_dm_explosion_spawn(void *user,const AvpAmp *bolt,const AvpAmp *explosion_amp)
{
    AvpMpWalkHost *t=(AvpMpWalkHost *)user;
    int owner=walk_dm_projectile_owner(t,bolt);
    if(owner>=0)walk_dm_register_projectile(t,explosion_amp,(unsigned)owner);
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
        !avp_mp_life_busy(&t->player_life[q])){
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
    walk_dm_apply_hit(t,(unsigned)owner,best,damage);
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
        !avp_mp_life_busy(&t->player_life[q])){
        const AvpMpActorNativeState *v=(const AvpMpActorNativeState *)t->session.player[q];
        AvpXY centre={a->xpos,a->ypos},to;
        if(!v||v->player.player_energy<=0||t->dm_death_credited[q])continue;
        if(!avp_mp_dm_explosion_candidate((s32)v->player.x_pos-a->xpos,
                (s32)v->player.y_pos-a->ypos,(u32)a->timer))continue;
        to.x=(s32)v->player.x_pos;to.y=(s32)v->player.y_pos;
        if(LineOfSight(&to,&centre))continue;
        walk_dm_apply_hit(t,(unsigned)owner,q,200);
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
        !avp_mp_life_busy(&t->player_life[q])){
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
        avp_mp_dm_direct_damage(player_type,cur_wepno,damage));
    return 1;
}

/* Run the original synchronous COMPUTER.S code with exactly the activating
 * player's native PLAYER/HUD/weapon state and that player's original DOORS/
 * AMP level bound. All other players and worlds are paused until it closes.
 * Do not manufacture aid, clearance, destruct/launch or terminal text. */
static int walk_run_terminal(AvpMpWalkHost *t,unsigned p)
{
    const AvpMpActiveWorld *active;
    AvpMpNativeWorld *world;
    AvpMpActorNativeState *actor;
    AvpMpNativeScope scope={0};
    int world_entered=0,actor_entered=0,ok=1,launched=0,destructed=0;
    const AvpRuntimeOps *runtime=avp_runtime_ops();
    unsigned offset;
    if(!t||p>=t->players||!(t->terminal_pending_mask&(1u<<p)))return 0;
    offset=t->terminal_pending_offset[p];
    t->terminal_pending_mask&=~(1u<<p);
    if(t->dm.enabled)return 1; /* Never enter a terminal UI in Deathmatch. */
    active=avp_mp_session_player_world(&t->session,p);
    actor=(AvpMpActorNativeState *)t->session.player[p];
    if(!active || !active->context || !actor ||
       actor->player.player_type!=PT_HUMAN)return 0;
    /* Bot navigation may press Use at a terminal while pathfinding. Until a
     * separate bot computer-menu planner exists, never enter an unbounded
     * synchronous UI that has no human input owner. The original terminal
     * and all world state remain untouched in that case. */
    if(t->session.setup.bot_enabled[p]){
        fprintf(stderr,"MP P%u AI terminal request deferred: no menu driver.\n",p+1u);
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
        ok=0;goto done;
    }
    avp_pc_runtime_set_mp_terminal((int)p,t->players);
    if(runtime && runtime->play_sfx)runtime->play_sfx(runtime->user,AVP_FX_COMPE);
    /* COMPUTER.S itself manages original CLUT, text rendering, menus,
     * countdown arming, first aid and the escape-pod launch event. */
    Computer();
    if(runtime && runtime->play_sfx)runtime->play_sfx(runtime->user,AVP_FX_COMPD);
    destructed=destruct_flag!=0;
    launched=launch_flag!=0;
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
    if(ok && (destructed||launched))for(unsigned q=0;q<t->actors;q++){
        AvpMpActorNativeState *a=(AvpMpActorNativeState *)t->session.player[q];
        if(a){if(destructed)a->player.destruct_flag=1;
              if(launched)a->player.launch_flag=1;}
    }
    avp_pc_video_reset_presentation(t->video);
    return ok? (launched?2:1):0;
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
    if(t->dm.enabled){
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
        t->prev_x[p]=a->player.x_pos;t->prev_y[p]=a->player.y_pos;
        t->prev_angle[p]=a->player.centre_angle;
        /* The death fade is allowed to hold the camera only until the
         * transactional destination join has completed. Previously this
         * cross-world return skipped the release, permanently freezing view. */
        avp_mp_death_camera_end(&t->death_camera[p]);
        walk_shared_arrival_refresh(t,1u<<p);
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
               (t->session.worlds[w].member_mask&(1u<<p))) {
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
    walk_shared_arrival_refresh(t,1u<<p);
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
    for(unsigned p=0;p<t->actors && used+18u<sizeof(status);p++){
        int n=snprintf(status+used,sizeof(status)-used," | P%u %u/%u",
                       p+1u,t->dm.kills[p],t->dm.deaths[p]);
        if(n<0)break;
        used+=(size_t)n;
    }
    avp_pc_video_local_mp_deathmatch_status(t->video,status);
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
    int f2_menu_audio_suspended=0;
    uint64_t dm_finished_at=0;
    int victory_ending=0;
    unsigned victory_countdown=0;
    unsigned long ticks=0;
    const uint64_t sim_period=100000000ull;
    unsigned render_hz=60u;
    uint64_t render_period=1000000000ull/60ull;
    AvpMpPause pause_state={0};
    if(!t)return 1;
    if(!avp_pc_video_local_mp_walktest_mode(video,1)||
       !walk_init(t,video,level,retail_level,all_mazes,maze_count,
                  rom,combat_test,players,requested)){
        fprintf(stderr,"MP movement test cannot bind the current original world/actor state.\n");
        walk_cleanup(t);avp_pc_video_local_mp_walktest_mode(video,0);
        free(t);return 1;
    }
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
                    if(walk_respawn(t,p))avp_mp_life_resumed(&t->player_life[p]);
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
            for(unsigned p=0;p<t->players;p++)
                t->pending_cheat_actions[p]=avp_pc_video_take_local_mp_cheat_actions(video,p);
            t->p1_weapon_delta=avp_pc_video_take_weapon_delta(video);
            avp_pc_runtime_set_noclip(avp_pc_video_noclip(video));
            avp_dev_cheats_set_infinite_health(avp_pc_video_infinite_health(video));
            avp_dev_cheats_set_infinite_ammo(avp_pc_video_infinite_ammo(video));
            avp_amp_set_enemies_frozen(avp_pc_video_freeze_enemies(video));
            avp_amp_set_attacks_disabled(avp_pc_video_attacks_disabled(video));
            avp_amp_set_marine_pulse_los_hits(avp_pc_video_marine_pulse_los_hits(video));
            avp_amp_set_marine_fire_interrupt(avp_pc_video_marine_fire_interrupt(video));
            avp_amp_set_normalize_diagonal_contact_range(avp_pc_video_normalize_diagonal_contact_range(video));
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
             * slots P5-P16 are simulation actors, not viewports; R22 only
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
            if(!avp_mp_session_tick(&t->session,cmd))failed=1;
            if(!failed){
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
                for(unsigned p=0;p<t->players;p++)if(t->terminal_pending_mask&(1u<<p)){
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
                    if(fade->phase==AVP_MP_LIFE_ALIVE){
                        const AvpRuntimeOps *runtime=avp_runtime_ops();
                        avp_mp_life_begin(fade);
                        if(runtime && runtime->play_sfx)
                            runtime->play_sfx(runtime->user,AVP_FX_DUCT1);
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
                unsigned passengers=walk_room_passengers(t,p,&request);
                if(!passengers){
                    moved=0;
                }else if(passengers!=(1u<<p) &&
                   old_world->level!=(unsigned)request.target_level){
                    moved=avp_mp_session_move_group(&t->session,passengers,
                                                    (unsigned)request.target_level);
                }else if(old_world->level==(unsigned)request.target_level){
                    /* A same-level duct/cocoon return is a position change,
                     * not a new world allocation. Keep shared state intact. */
                    AvpMpNativeWorld *ctx=(AvpMpNativeWorld *)old_world->context;
                    moved=walk_join(t,ctx,p,(AvpMpActorNativeState *)t->session.player[p]);
                }else moved=avp_mp_session_move(&t->session,p,(unsigned)request.target_level);
                for(unsigned q=0;q<t->actors;q++)if(q==p || (passengers&(1u<<q)))
                    memset(&t->entering[q],0,sizeof(t->entering[q]));
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
                    for(unsigned q=0;q<t->actors;q++)if(passengers&(1u<<q)){
                        AvpMpTransitionRequest consumed;
                        (void)avp_mp_transition_commit(&t->transitions,q,&consumed);
                    }
                    for(unsigned q=0;q<t->actors;q++)if(passengers&(1u<<q)){
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
    player_type=host_campaign_type;
    avp_pc_video_local_mp_walktest_mode(video,0);
    free(t);
    return failed?AVP_MP_RUN_ERROR:(restart_from_f2?AVP_MP_RUN_RESTART:
           (return_to_title?AVP_MP_RUN_TITLE:
           (completed?AVP_MP_RUN_COMPLETED:AVP_MP_RUN_EXIT)));
}
