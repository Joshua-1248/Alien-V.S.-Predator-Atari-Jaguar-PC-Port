#ifndef AVP_LOCAL_MP_BOT_H
#define AVP_LOCAL_MP_BOT_H
#include "local_mp_worlds.h"
#include <stddef.h>
struct AvpDoorLevelMeta;
#include <stdint.h>

/* PC-only command planner. World state, collision, door animation, level
 * transitions, attacks, damage and Predator honour are ALWAYS original code. */
#define AVP_MP_BOT_MAP_MAX 64u
#define AVP_MP_BOT_MAP_CELLS (AVP_MP_BOT_MAP_MAX*AVP_MP_BOT_MAP_MAX)

/* Labels for original AMP enemy families, not invented source attacks. */
enum AvpMpBotMeleeThreat {
    AVP_MP_BOT_THREAT_NONE=0, AVP_MP_BOT_THREAT_ALIEN,
    AVP_MP_BOT_THREAT_PREDATOR, AVP_MP_BOT_THREAT_QUEEN,
    AVP_MP_BOT_THREAT_FACEHUG
};

/* The retail DOORS.S rules: Marine operates ducts and lifts, Alien ducts
 * but no lifts, Predator lifts but no ducts. Level 6..13 are duct maps.
 * Values describe the real level-connection mechanism, not a new move. */
enum AvpMpBotTransitionKind {
    AVP_MP_BOT_TRANS_EXIT=0, AVP_MP_BOT_TRANS_DUCT=1,
    AVP_MP_BOT_TRANS_LIFT=2
};
int avp_mp_bot_transition_allowed(unsigned species,unsigned from_level,
                                   unsigned to_level,
                                   enum AvpMpBotTransitionKind kind);
/* Retail DOORS.S::do_lifts rejects this specific lift/direction pair.
 * A jam is static authored geometry, not a temporary retry condition. */
int avp_mp_bot_lift_jammed(unsigned level,unsigned x,unsigned y,int direction);

enum AvpMpBotGoal {
    AVP_MP_BOT_PATROL=0, AVP_MP_BOT_HOSTILE, AVP_MP_BOT_FOLLOW,
    AVP_MP_BOT_HEALTH, AVP_MP_BOT_COCOON, AVP_MP_BOT_EXIT, AVP_MP_BOT_LIFT,
    AVP_MP_BOT_SUPPLY, AVP_MP_BOT_QUEEN_WAIT
};

/* Original AMP.S::amp_acid uses axis-aligned +/- (30<<9) contact. Keep
 * damage hazard coordinates separately from the coarse grid cost field. */
#define AVP_MP_BOT_MAX_ACID 300u
typedef struct AvpMpBotAcid { uint32_t x,y; } AvpMpBotAcid;

/* Read-only local view of original COLLIDE.S::AMPCollisions blockers. */
#define AVP_MP_BOT_MAX_NEAR_PROPS 64u
typedef struct AvpMpBotProp { uint32_t x,y,halfwidth; } AvpMpBotProp;

typedef struct AvpMpBotMap {
    const uint8_t *cells; /* original eight-byte maze cells, header skipped */
    unsigned width,height;
    const uint8_t *wall_flags; /* captured original DOORS.S fullbits[256] */
    const struct AvpDoorLevelMeta *door_meta; /* authored access/airlock records */
    unsigned access_level;
    const uint8_t *risk; /* 64x64, 0 safe / 1 nearby acid / 2 acid cell */
    const uint8_t *prop_risk; /* same grid; source collmap occupancy, route cost */
} AvpMpBotMap;

/* Shared PC-only door arbitration across bots in a map, not original door state. */
#define AVP_MP_BOT_DOOR_CLAIMS 64u
typedef struct AvpMpBotDoorClaim {
    const uint8_t *cells;
    uint32_t offset;
    uint64_t until;
} AvpMpBotDoorClaim;
typedef struct AvpMpBotDoorClaims {
    AvpMpBotDoorClaim entries[AVP_MP_BOT_DOOR_CLAIMS];
    unsigned next_evict;
} AvpMpBotDoorClaims;
int avp_mp_bot_door_claim(AvpMpBotDoorClaims *claims,
    const uint8_t *cells,uint32_t offset,uint64_t tick);

typedef struct AvpMpBotState {
    uint32_t previous_x,previous_y;
    unsigned immobile_ticks,evade_ticks,initialized;
    uint16_t door_cell,door_next;
    unsigned door_attempts,door_cooldown;
    unsigned facehug_phase; /* 0 first release, 1 press, 2 release */
    unsigned last_motion, recovery_side, recovery_count; /* observed stall response */
    unsigned acid_commit_ticks; /* bounded centerline crossing after an acid-lane stall */
    unsigned cocoon_locked,cocoon_target_slot,cocoon_phase;
    uint64_t cocoon_phase_end,combat_hold_until;
    uint64_t predator_rotation_last_tick,alien_attack_until;
    unsigned predator_weapon_target,alien_attack_choice;
    /* 16 unique four-weapon orders per bot, seeded once per life. */
    unsigned predator_pattern_ready,predator_pattern_start;
    unsigned predator_pattern_stride,predator_pattern_order,predator_pattern_step;
    unsigned predator_idle_ticks,predator_switch_pending; /* 150 idle 10-Hz ticks; paused in combat */
    uint64_t last_door_use;
    uint64_t last_exit_use;
    uint64_t lure_end_tick,lure_cooldown_tick;
    unsigned lure_target_slot;
    unsigned guerilla_flank_side,guerilla_flank_ready;
    uint64_t guerilla_flank_until;
} AvpMpBotState;

typedef struct AvpMpBotObservation {
    unsigned behavior; /* Balanced=0, Berserker=1, Guerilla=2 */
    unsigned deathmatch; /* Enable random Predator weapon and Alien attack selection */
    uint32_t x,y,heading; /* 16.16 XY; original heading high 16 */
    uint32_t target_x,target_y;
    unsigned has_target,hostile_target;
    unsigned player_index;
    uint64_t tick;
    enum AvpMpBotGoal goal;
    unsigned target_side; /* original DOORS.S 0 LEFT,1 TOP,2 RIGHT,3 BOTTOM */
    int lift_direction; /* -1 Up, +1 Down; never synthesize an invalid lift */
    unsigned current_level,destination_level,exit_is_duct;
    unsigned target_visible; /* authoritative original LineOfSight == 0 */
    unsigned target_is_marine,target_is_stunned; /* source AMP creature/flags */
    unsigned target_is_alien; /* only Alien AMP enemies form acid on death */
    unsigned target_energy_positive,target_slot;
    /* Only from a CURRENTLY SIGHTED AMP/player. Never reveal the heading
     * of a tracker-only or remembered contact to Guerilla bots. */
    uint32_t target_heading; /* source AMP angle / player centre_angle, 16.16 */
    unsigned target_heading_valid;
    unsigned marines_on_level,combat_enemies_nearby;
    unsigned dm_opponents_on_level; /* presence only; no position disclosure */
    uint32_t melee_x,melee_y;
    unsigned melee_threat,melee_visible; /* nearest visible source melee AMP */
    unsigned player_species; /* 0 Marine, 1 Alien, 2 Predator */
    unsigned current_weapon; /* original cur_wepno, used for Predator melee selection */
    uint8_t predator_owned; /* retail weapon owned bits 1..4, per actor */
    unsigned marine_inventory_valid; /* absent only from legacy bot smoke views */
    uint8_t marine_owned; /* original per-actor cur_weps, bits 1..4 */
    uint16_t marine_ammo[4]; /* per-actor original ammo_info[].cur, no fabricated rounds */
    int player_energy,max_energy,medpak;
    unsigned invis_act,invis_stat,invisflag;
    unsigned facehugged; /* original MAZESCRN.S hugkill for this actor */
    int facehug_direction; /* original unstick_dir: >=0 LEFT, <0 RIGHT */
    unsigned acid_count;
    AvpMpBotAcid acid[AVP_MP_BOT_MAX_ACID];
    unsigned prop_count;
    AvpMpBotProp props[AVP_MP_BOT_MAX_NEAR_PROPS];
    AvpMpBotMap map;
    AvpMpBotDoorClaims *door_claims; /* Shared only among bots in current host. */
    /* Read-only original DOORS.S active slots from this same world.  An
     * in-flight door may still have opening byte 0 before its first native
     * door tick; a second bot must not press Use and reverse it. */
    unsigned active_door_count;
    uint32_t active_door_offsets[2]; /* Relative to map.cells (header skipped). */
} AvpMpBotObservation;

typedef struct AvpMpBotRoute {
    unsigned valid,requires_door_use,door_is_open;
    unsigned door_value; /* Raw source opening byte: nonzero = moving/open. */
    uint32_t door_offset; /* Eight-byte original cell boundary offset. */
    unsigned next_x,next_y;
    /* A second authored path cell for optional forward-looking steering.
     * Immediate hops can be cardinal or fully open, verified diagonals;
     * the original Jaguar movement and collision remain authoritative. */
    unsigned lookahead_valid,lookahead_x,lookahead_y;
    uint32_t cost;
} AvpMpBotRoute;

/* Can the original acid contact square be bypassed on a legal
 * single-cell-wide approach, including a vent? No fictional 3x3 clearance. */
int avp_mp_bot_alien_death_passable(const AvpMpBotMap *map,
                                   uint32_t x,uint32_t y);
/* Pick an actual source-reachable patrol destination for no-contact bots.
 * Visit weights are bot-owned memory, not a privileged target lookup. */
int avp_mp_bot_patrol_pick(const AvpMpBotMap *map,unsigned x,unsigned y,
    const uint16_t visits[AVP_MP_BOT_MAP_CELLS],uint32_t seed,
    unsigned *target_x,unsigned *target_y);
/* Player-AI decisions only. Retail score and attack implementation remain intact. */
unsigned avp_mp_bot_predator_preferred_weapon(const AvpMpBotObservation *view);
unsigned avp_mp_bot_random_predator_weapon(unsigned owned,unsigned previous,
                                           uint32_t seed);
/* 16 distinct full 1..4 Predator weapon orders; 0 = invalid request. */
unsigned avp_mp_bot_predator_order_weapon(unsigned order,unsigned step);
unsigned avp_mp_bot_alien_campaign_target_level(unsigned current_level);

/* Exact AMP.S 30<<9 Chebyshev contact, marked into intersected grid
 * squares for coarse routing. Final steering still uses actor coordinates. */
void avp_mp_bot_acid_risk_mark(uint8_t risk[AVP_MP_BOT_MAP_CELLS],
                                uint32_t px,uint32_t py);
/* Eight-direction grid search considers BOTH original wall faces, keyed-door
 * restrictions and acid/prop hazards. Diagonals require an open 2x2 square;
 * this only selects player commands, never moves a player or opens a door. */
int avp_mp_bot_route(const AvpMpBotMap *map,unsigned start_x,unsigned start_y,
                     unsigned goal_x,unsigned goal_y,AvpMpBotRoute *out);
/* Label original reachable floor components; locked/jammed doors separate
 * regions, but correctly open doors remain traversable. 0 = non-floor. */
unsigned avp_mp_bot_components(const AvpMpBotMap *map,uint16_t out[AVP_MP_BOT_MAP_CELLS]);
/* Reject an unreachable teammate before selecting a FOLLOW goal. For
 * different levels this is only a species-level prefilter: the caller must
 * still find an actual source-authored connection and usable door/lift. */
int avp_mp_bot_follow_target_possible(unsigned species,unsigned from_level,
        unsigned teammate_level,const AvpMpBotMap *map,
        unsigned from_x,unsigned from_y,unsigned teammate_x,unsigned teammate_y);
int avp_mp_bot_outside_bounds(uint32_t x,uint32_t y,
                              unsigned width,unsigned height);
/* PC multiplayer bot-only movement guard: a collision-safe position also
 * needs an actual authored floor tile. Never replaces original SafePos. */
int avp_mp_bot_playable_position(const uint8_t *cells,unsigned width,
                                unsigned height,uint32_t x,uint32_t y);
void avp_mp_bot_step(AvpMpBotState *state,const AvpMpBotObservation *view,
                     AvpMpPlayerCommand *out);
#endif
