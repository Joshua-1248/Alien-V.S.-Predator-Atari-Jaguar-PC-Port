#ifndef AVP_LOCAL_MP_DEATHMATCH_H
#define AVP_LOCAL_MP_DEATHMATCH_H
#include <stdint.h>
#include "local_mp.h"
/* Deathmatch rules are PC-local. 0 score/time means unlimited. Simulation
 * clock advances only on original 10-Hz game ticks, not wall-clock pauses. */
typedef struct AvpMpDeathmatch {
    unsigned enabled,players,score_limit,time_limit_minutes,fast_movement;
    unsigned fast_equip,enemies,level,level_travel,cloak_damage;
    uint64_t elapsed_ticks;
    unsigned kills[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned deaths[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned finished,winner; /* winner 0..3; 4 means tie */
} AvpMpDeathmatch;
void avp_mp_dm_init(AvpMpDeathmatch *m,const AvpLocalMpSetup *setup);
void avp_mp_dm_tick(AvpMpDeathmatch *m);
void avp_mp_dm_kill(AvpMpDeathmatch *m,unsigned killer,unsigned victim);
unsigned avp_mp_dm_seconds_remaining(const AvpMpDeathmatch *m);
/* Called once on native actor initialization, never per movement frame. */
void avp_mp_dm_speed(unsigned enabled,uint32_t *max_speed,int16_t *acc_shift);
/* Filter only living hostile NPC records; preserve props, pickups, corpses,
 * player-owned projectiles and the original AMP allocator's slot identities.
 * Invoked ONLY when Deathmatch Enemy AI is disabled. */
struct AvpMpAmpNativeState;
unsigned avp_mp_dm_remove_enemies(struct AvpMpAmpNativeState *amp);
/* Conservative spawnable interior test: floor and walls enclosing any
 * adjacent empty space. This is in addition to native SafePos and reachability. */
int avp_mp_dm_spawn_interior(const uint8_t *cells,unsigned width,unsigned height,
    const uint8_t wall_flags[256],unsigned x,unsigned y);
/* Restrict random spawns to the component containing the ORIGINAL level
 * start pose, never to the disconnected room where a player last died.
 * Components use the existing PC bot's source-authored door/access rules. */
int avp_mp_dm_spawn_origin_region(const uint16_t components[4096],
    unsigned width,unsigned height,unsigned origin_x,unsigned origin_y,
    unsigned candidate_x,unsigned candidate_y);
/* Original PLAYER.S TestSpark coarse box, front half-plane, closest-z and
 * width check, reused ONLY for candidate player avatars in Deathmatch. */
/* PC-local PvP damage only; the original weapon table and enemy damage
 * remain unchanged. `source_damage` already includes native charge decay. */
int avp_mp_dm_direct_damage(int16_t species,int16_t weapon,int source_damage);
/* Deathmatch-only victim vulnerability; the original AMP damage is unchanged. */
int avp_mp_dm_victim_damage(unsigned modifier,int16_t species,
                            unsigned invisflag,int damage);
/* Projectile/splash damage against player avatars, never against AMPs. */
int avp_mp_dm_projectile_damage(unsigned projectile_creature);
/* Native creature type for original species-colored blood spark;
 * -1 means no puff (including all flamethrower/projectile weapons). */
int avp_mp_dm_bullet_impact_creature(int16_t shooter_species,
    int16_t weapon,int16_t victim_species);
int avp_mp_dm_explosion_candidate(int32_t dx,int32_t dy,uint32_t radius);
int avp_mp_dm_shot_candidate(int32_t dx,int32_t dy,int32_t boxx,int32_t boxy,
                             int16_t cs,int16_t sn,uint16_t width,
                             uint32_t nearest,uint32_t *depth);
#endif
