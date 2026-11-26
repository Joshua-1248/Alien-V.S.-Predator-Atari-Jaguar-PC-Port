#ifndef AVP_LOCAL_MP_BOT_H
#define AVP_LOCAL_MP_BOT_H
#include "local_mp_worlds.h"
#include "local_mp_chamber.h"
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
/* Read-only tactical budget; original weapon/damage code remains authoritative. */
int avp_mp_bot_weakening_allowed(int original_energy,unsigned weapon_damage,
                                 unsigned other_bot_reservations);
struct AvpMpBotObservation;
/* R282 co-op shotgun egg tactic: 0=ordinary attack, 1=approach without
 * shooting, 2=activated: fire normally, 3=insufficient shells: do not provoke. */
unsigned avp_mp_bot_shotgun_egg_policy(const struct AvpMpBotObservation *view);
struct AvpMpBotState;
struct AvpMpBotObservation;
struct AvpMpBotMap;
/* R278: prioritize a SIGHTED source creature near a real living teammate.
 * Never infer egg presence from the motion tracker or through walls. */
unsigned avp_mp_bot_guard_urgency(unsigned creature,uint64_t teammate_d2,
                                  uint64_t observer_d2);
/* Stabilize the waypoint used for a genuine scanner-only hostile contact.
 * Call only after host target selection; no Fire permission is granted. */
/* A scanned AMP in another disconnected region requires the authentic
 * LEVDOOR/lift graph rather than local fallback steering. */
int avp_mp_bot_tracker_needs_portal(const struct AvpMpBotMap *map,
    unsigned sx,unsigned sy,unsigned gx,unsigned gy);
/* Tighten PC co-op bot aiming to the source TestSpark weapon width;
 * real Jaguar spread/hit testing remains unchanged. */
int avp_mp_bot_marine_aim_ready(unsigned weapon,double error,
    double range,int emergency);
/* Bot-only health urgency; original player energy and health cap are unchanged. */
int avp_mp_bot_health_low(int energy,int cap);
int avp_mp_bot_health_critical(int energy,int cap);
int avp_mp_bot_marine_needs_resupply(unsigned owned,
    const uint16_t ammo[4],const uint16_t maxima[4]);
/* R295: high-priority co-op cohesion without abandoning authentic immediate
 * combat, rescue, health or ammo objectives. Distances are squared original
 * 16.16 actor XY coordinates, not fabricated navigation geometry. */
int avp_mp_bot_team_regroup_priority(const struct AvpMpBotObservation *view,
    uint64_t teammate_distance_sq,uint64_t objective_distance_sq,
    unsigned supply_fills_empty_reserve);
int avp_mp_bot_tracker_contact_suppressed(const struct AvpMpBotState *state,
    unsigned slot,unsigned level,unsigned generation,
    uint32_t x,uint32_t y,uint64_t tick);
void avp_mp_bot_tracker_hold(struct AvpMpBotState *state,
                             struct AvpMpBotObservation *view,
                             unsigned original_slot_generation);
/* Co-op Marine radar and last-seen contacts are navigation clues only.
 * Only a fresh, original optically verified contact is a combat target. */
void avp_mp_bot_coop_contact_as_memory(struct AvpMpBotObservation *view);

enum AvpMpBotGoal {
    AVP_MP_BOT_PATROL=0, AVP_MP_BOT_HOSTILE, AVP_MP_BOT_FOLLOW,
    AVP_MP_BOT_HEALTH, AVP_MP_BOT_COCOON, AVP_MP_BOT_EXIT, AVP_MP_BOT_LIFT,
    AVP_MP_BOT_SUPPLY, AVP_MP_BOT_QUEEN_WAIT, AVP_MP_BOT_MEDICAL,
    AVP_MP_BOT_INVESTIGATE, /* unseen, remembered contact: navigation, not engagement */
    AVP_MP_BOT_AIRLOCK_SEAT, AVP_MP_BOT_AIRLOCK_CLOSE,
    AVP_MP_BOT_ESCAPE_SEAT, /* R300: wait inside original Level-5 capsule */
    AVP_MP_BOT_ESCAPE_LAUNCH /* real Use against authored Level-5 pod panel */
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
    const uint8_t *risk; /* 64x64, 0 safe / 1 nearby acid / 2 acid cell / 3 remembered choke */
    const uint8_t *prop_risk; /* same grid; source collmap occupancy, route cost */
    unsigned avoid_cell_plus_one; /* per-bot temporary blocked waypoint, 0 none */
} AvpMpBotMap;
int avp_mp_bot_pick_refuge(const AvpMpBotMap *map,unsigned sx,unsigned sy,
    unsigned threat_x,unsigned threat_y,unsigned *rx,unsigned *ry);


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

/* Only ONE active destination/activation owner per original lift shaft.
 * This is bot input arbitration; original DOORS.S owns the actual lift. */
#define AVP_MP_BOT_LIFT_CLAIMS 32u
typedef struct AvpMpBotLiftClaim {
    unsigned active,level,x,y,leader,destination;
    uint64_t created,until;
} AvpMpBotLiftClaim;
typedef struct AvpMpBotLiftClaims {
    AvpMpBotLiftClaim entries[AVP_MP_BOT_LIFT_CLAIMS];
} AvpMpBotLiftClaims;
int avp_mp_bot_lift_agree(AvpMpBotLiftClaims *claims,unsigned level,
    unsigned x,unsigned y,unsigned actor,unsigned wanted,uint64_t tick,
    unsigned *leader,unsigned *destination,uint64_t *created);
/* RE44 R276: an optically visible, currently selected hostile (or nearby
 * observed melee threat) must be handled BEFORE co-op elevator boarding.
 * Tracker-only/old contacts do not hold the lift indefinitely. This gates
 * only the bot's optional lift invitation, not original lift mechanics. */
struct AvpMpBotObservation;
int avp_mp_bot_lift_invitation_allowed(const struct AvpMpBotObservation *view);
/* A boarded co-op bot keeps its original shaft/destination while another
 * routine rescans objectives. A real visible attack interrupts it; an arrival
 * on a new level requires disembarking regardless of newly chosen goals. */
int avp_mp_bot_lift_keep_trip(const struct AvpMpBotObservation *view,
    unsigned intent_level,unsigned level,uint64_t last_intent_tick,
    unsigned within_lift_chamber);
int avp_mp_bot_lift_needs_disembark(unsigned arrival_pending,
    unsigned inside_arrival_chamber);
/* Pick a genuine neighboring floor for disembarkation; never teleport. */
int avp_mp_bot_lift_egress(const AvpMpBotMap *map,unsigned x,unsigned y,
                            unsigned *goal_x,unsigned *goal_y);

/* PC-only per-bot pickup-target memory: a nearby corpse is not useful
 * if repeated original movement commands cannot get within pickup range.
 * The identity includes the native AMP slot generation (which can be reused). */
#define AVP_MP_BOT_PICKUP_AVOIDS 8u
typedef struct AvpMpBotPickupAvoidEntry {
    unsigned level,slot,generation;
    uint64_t until;
} AvpMpBotPickupAvoidEntry;
typedef struct AvpMpBotPickupMemory {
    AvpMpBotPickupAvoidEntry avoided[AVP_MP_BOT_PICKUP_AVOIDS];
    unsigned next_avoid,active,level,slot,generation;
    uint32_t target_x,target_y;
    uint64_t last_progress_tick,best_distance;
    uint32_t best_route_cost,last_route_cell;
} AvpMpBotPickupMemory;
int avp_mp_bot_pickup_avoided(const AvpMpBotPickupMemory *mem,
    unsigned level,unsigned slot,unsigned generation,uint64_t tick);
void avp_mp_bot_pickup_track(AvpMpBotPickupMemory *mem,
    unsigned level,unsigned slot,unsigned generation,uint64_t tick,
    uint32_t x,uint32_t y,uint32_t tx,uint32_t ty,uint32_t route_cost,
    uint32_t cell);
void avp_mp_bot_pickup_observe(AvpMpBotPickupMemory *mem,
    unsigned level,uint64_t tick,uint32_t x,uint32_t y,
    uint32_t route_cost,uint32_t cell);

/* Arrival guard applies only to speculative emergency escapes. */
typedef struct AvpMpBotDuctMemory {
    unsigned from,level;
    uint32_t x,y;
    uint64_t tick;
} AvpMpBotDuctMemory;
void avp_mp_bot_duct_arrived(AvpMpBotDuctMemory *m,unsigned from,unsigned level,
    uint32_t x,uint32_t y,uint64_t tick);
int avp_mp_bot_duct_reverse_block(const AvpMpBotDuctMemory *m,unsigned level,
    unsigned dest,uint32_t x,uint32_t y,uint64_t tick);

typedef struct AvpMpBotState {
    AvpMpBotDuctMemory duct_arrival;
    unsigned physical_valid,physical_level,physical_requested,physical_stall;
    uint32_t physical_x,physical_y;
    uint64_t physical_tick;
    unsigned objective_stall,objective_requested,objective_route,objective_wait;
    unsigned peer_unblock,peer_unblock_level,peer_unblock_index,peer_unblock_center;
    uint32_t peer_unblock_x,peer_unblock_y,peer_unblock_cx,peer_unblock_cy;
    uint64_t peer_unblock_until; /* R494: bounded passive peer-clearance escape */
    unsigned peer_melee_align; /* R492: short peer-combat alignment command */
    uint64_t objective_recovery_until;
    /* R497: measured tactical travel stall, independent of damage. */
    unsigned tactical_blocked, tactical_idle, tactical_level, tactical_retargets;
    uint32_t tactical_x,tactical_y;
    uint64_t tactical_until;
    /* R501: progress towards safety, not merely successful movement. */
    uint32_t tactical_threat_x,tactical_threat_y;
    double tactical_best_separation,tactical_progress_path;
    unsigned tactical_progress_ticks,tactical_progress_valid;
    /* R489: bounded local oscillation, not a collision or idle timer. */
    uint64_t loop_start_tick,loop_path,loop_start_distance;
    uint32_t loop_x,loop_y,loop_target_x,loop_target_y;
    unsigned loop_valid,loop_goal,loop_level,loop_ready,loop_recoveries;

    uint32_t previous_x,previous_y;
    unsigned immobile_ticks,evade_ticks,initialized;
    uint16_t door_cell,door_next;
    unsigned door_attempts,door_cooldown;
    unsigned door_wait_valid,door_wait_level,door_wait_best;
    uint64_t door_wait_tick;
    unsigned facehug_phase; /* 0 first release, 1 press, 2 release */
    unsigned last_motion, recovery_side, recovery_count; /* observed stall response */
    /* R486: actual puddle exits, independent of goal and body heading. */
    struct { uint32_t x,y; uint64_t tick; } acid_exits[16];
    unsigned acid_exit_count,acid_return_blocks,acid_contact_count;
    unsigned acid_commit_ticks; /* bounded centerline crossing after an acid-lane stall */
    /* R272: retain a collision-legal lane direction near acid across tactical
     * replans. This applies to navigation inputs only, not attack/damage. */
    unsigned acid_lane_motion,acid_lane_cell,acid_lane_goal;
    uint64_t acid_lane_until;
    /* R284: real XY/net progress near an acid corridor, independent of
     * tracker target jitter and the collision-only immobile counter. */
    uint32_t acid_progress_x,acid_progress_y;
    unsigned acid_progress_cell,acid_progress_level,acid_progress_valid;
    uint64_t acid_progress_tick;
    unsigned cocoon_locked,cocoon_target_slot,cocoon_phase;
    uint64_t cocoon_phase_end,combat_hold_until;
    uint64_t predator_decloak_tick;
    unsigned predator_decloak_wait;
    uint64_t vent_goal_since,vent_cross_until;
    unsigned vent_goal_valid,vent_goal_level,vent_goal_cell,vent_goal_kind;
    unsigned vent_cross_count,vent_cross_index;
    struct { double x,y; } vent_cross_path[6];
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
    /* R130: observed native health only. A hit causes a short evasive
     * reaction, NOT knowledge of the shooter or a new damage system. */
    int last_observed_energy;
    unsigned damage_observed,damage_world_level,damage_side;
    uint64_t damage_observed_tick,damage_react_until;
    /* PC-only memory for an original collision-blocked route edge.
     * Retain across short-lived patrol/corpse objective changes. */
    unsigned nav_bad_next_plus_one,nav_recent_cell[32],nav_recent_count;
    /* A 32-cell recent route trace catches loops around entire maze blocks,
     * not only the original four-cell A/B oscillation. */
    uint64_t nav_bad_until,nav_anchor_tick;
    uint32_t nav_anchor_x,nav_anchor_y;
    unsigned nav_goal_x,nav_goal_y,nav_anchor_valid;
    unsigned nav_anchor_level,nav_anchor_next,nav_bad_level;
    unsigned nav_anchor_cell;
    uint64_t nav_anchor_seen;
    unsigned nav_retarget_patrol;
    unsigned nav_bounce_last,nav_bounce_other,nav_bounce_count,nav_bounce_level;
    uint64_t nav_bounce_seen,nav_bounce_tick;
    /* Same-world player collision avoidance: one bot yields to another
     * instead of mutually pushing into the same narrow passage. */
    uint64_t peer_yield_until;
    /* RE44 R289: personal-space backoff while following an actual player.
     * Hysteresis avoids a tick-by-tick forward/backward oscillation. */
    unsigned follow_backoff_active;
    /* R284: a head-on peer in an acid-lined one-cell lane needs a REAL
     * passing pocket, not a 10-tick backward/forward ping-pong. */
    unsigned peer_pass_valid,peer_pass_x,peer_pass_y,peer_pass_index,peer_pass_level;
    uint64_t peer_pass_until,peer_pass_arrived,peer_pass_retry_tick;
    unsigned guerilla_flank_side,guerilla_flank_ready;
    uint64_t guerilla_flank_until;
    /* R277: hold a reachable original-map firing lane, not the enemy's
     * occupied cell; periodically reconsider if the remembered target moves. */
    unsigned cover_flank_valid,cover_flank_slot,cover_flank_level;
    unsigned cover_flank_x,cover_flank_y;
    uint64_t cover_flank_until,cover_retry_tick;
    /* R278: keep a scanner-only search commitment across the motion
     * tracker's 10-Hz position updates. This is bot memory, not hidden
     * world knowledge; sight or imminent danger cancels the commitment. */
    unsigned tracker_seek_valid,tracker_seek_slot,tracker_seek_level,tracker_seek_serial;
    uint32_t tracker_seek_x,tracker_seek_y;
    uint64_t tracker_seek_until;
    /* R473: bound a continuous scanner investigation, even if blips jitter. */
    uint64_t tracker_investigate_since,tracker_cooldown_until;
    unsigned tracker_investigate_active;
    unsigned tracker_seek_fail_slot,tracker_seek_fail_level;
    uint64_t tracker_seek_fail_until;
    /* RE44 R290: scanner pursuit must demonstrate actual movement toward
     * its investigation waypoint. A contact may remain detectable while a
     * bot is stuck looking at a wall. Never renew that failed objective on
     * every 10-Hz tick; try normal teammate following/navigation instead. */
    unsigned tracker_stall_valid,tracker_stall_slot,tracker_stall_level;
    unsigned tracker_stall_serial;
    uint32_t tracker_stall_goal_x,tracker_stall_goal_y;
    double tracker_stall_best_distance;
    uint64_t tracker_stall_since;
    unsigned tracker_seek_fail_serial;
    uint32_t tracker_seek_fail_x,tracker_seek_fail_y;
    uint32_t tracker_seek_fail_bot_x,tracker_seek_fail_bot_y;
    /* R281: stable, original-map-verified ammunition destination.  Never
     * implies a free pickup, teleport, or an unobserved enemy coordinate. */
    unsigned resupply_valid,resupply_level,resupply_x,resupply_y;
    unsigned resupply_slot,resupply_generation,resupply_last_level;
    unsigned resupply_scan_level,resupply_scan_cursor;
    uint64_t resupply_until,resupply_retry_tick;
    /* R283: keep a REAL known health pickup/medical panel destination
     * across vent and elevator transitions; avoid goal oscillation. */
    unsigned healing_valid,healing_level,healing_x,healing_y;
    unsigned healing_slot,healing_generation,healing_medical;
    unsigned healing_last_level,healing_scan_level,healing_scan_cursor;
    uint64_t healing_until,healing_retry_tick;
    unsigned refuge_valid,refuge_level,refuge_x,refuge_y;
    uint64_t refuge_until;
    unsigned airlock_level,airlock_panel_offset,airlock_destination;
    uint64_t airlock_since,airlock_last_intent;
    AvpMpAirlockCycle airlock_cycle; /* R299: suppress accidental reverse transfer. */
    unsigned emergency_exit_valid,emergency_exit_destination;
    uint64_t emergency_exit_until,emergency_exit_retry_tick;
    /* Independent travel objective while a verified hostile owns aim.
     * No enemy coordinates or persistent world pointers are stored here. */
    unsigned travel_valid,travel_level,travel_goal;
    uint32_t travel_x,travel_y;
    uint64_t travel_tick;
    unsigned travel_progress_valid,travel_progress_next,travel_progress_stalls;
    uint64_t travel_progress_tick;
    double travel_progress_best;
    /* Local stationary-egg progress, separate from mobile enemies/radar. */
    unsigned egg_watch_valid,egg_watch_level,egg_watch_slot,egg_watch_generation;
    int egg_watch_health;
    double egg_watch_distance;
    uint64_t egg_watch_tick;
    unsigned egg_avoid_level,egg_avoid_slot,egg_avoid_generation;
    uint64_t egg_avoid_until;
} AvpMpBotState;

typedef struct AvpMpBotObservation {
    unsigned behavior; /* Balanced=0, Berserker=1, Guerilla=2 */
    unsigned deathmatch; /* Enable random Predator weapon and Alien attack selection */
    uint32_t x,y,heading; /* 16.16 XY; original heading high 16 */
    uint32_t target_x,target_y;
    unsigned has_target,hostile_target;
    unsigned player_index;
    uint64_t tick;
    unsigned observed_world_level; /* actual bound native world, including patrol views */
    enum AvpMpBotGoal goal;
    unsigned target_side; /* original DOORS.S 0 LEFT,1 TOP,2 RIGHT,3 BOTTOM */
    int lift_direction; /* -1 Up, +1 Down; never synthesize an invalid lift */
    unsigned current_level,destination_level,exit_is_duct;
    unsigned lift_wait; /* Shared bot lift owner not ready, or peer disembarking. */
    unsigned lift_follow_human; /* Co-op passenger: never press the lift panel. */
    unsigned airlock_face_side; /* Seat: away from closest wall; door: panel side. */
    unsigned airlock_door_ready; /* Host checked original door open/stationary. */
    unsigned airlock_pressure; /* Source ship exit: center before Use. */
    unsigned evacuation_active; /* Armed Marine self-destruct: bot combat urgency only. */
    unsigned target_visible; /* authoritative original LineOfSight == 0 */
    unsigned tracker_only_target; /* currently owned scanner contact, no optical sight */
    unsigned target_is_marine,target_is_stunned; /* source AMP creature/flags */
    unsigned target_is_alien; /* only Alien AMP enemies form acid on death */
    /* R282: sighted original AC_EGG; unopened means its native AMP mode
     * is still eggwait.  Never infer this state from a motion blip. */
    unsigned target_is_egg,target_egg_unopened;
    /* A living, acid-vulnerable OTHER player is inside the original
     * death-spray square plus a small movement margin, same world only. */
    unsigned other_player_splash_unsafe;
    /* R275: host-authorized PC bot-only nonlethal shot at a choke Alien.
     * Only after reading the live AMP energy and original weapon damage. */
    unsigned nonlethal_choke_fire;
    unsigned other_player_alien_trapped;
    unsigned other_player_swarmed; /* R288: >=2 optically/tracker-observed Aliens pinning a living teammate */
    /* Sighted native source AMP energy, with explicit validity. */
    int target_health;
    unsigned target_health_valid;
    unsigned target_energy_positive,target_slot;
    /* Only from a CURRENTLY SIGHTED AMP/player. Never reveal the heading
     * of a tracker-only or remembered contact to Guerilla bots. */
    uint32_t target_heading; /* source AMP angle / player centre_angle, 16.16 */
    unsigned target_heading_valid;
    unsigned marines_on_level,combat_enemies_nearby;
    /* R277: only source-observed optical/tracker contacts within striking
     * approach range; no invisible-enemy position disclosure. A Marine under
     * multiple Alien attacks may defend itself rather than waiting to die. */
    unsigned close_alien_contacts;
    /* Same-world living friendly actor; host selects only a collision-legal,
     * original-map-reachable refuge. Keep the hostile target for aiming. */
    unsigned retreat_to_teammate;
    uint32_t retreat_teammate_x,retreat_teammate_y;
    /* Host-validated original LOS and physical-prop-clear firing position.
     * Only a travel waypoint; never promotes an unseen contact to a hit. */
    unsigned cover_flank_valid,cover_flank_x,cover_flank_y;
    unsigned dm_opponents_on_level; /* presence only; no position disclosure */
    uint32_t melee_x,melee_y;
    unsigned melee_threat,melee_visible; /* nearest visible source melee AMP */
    unsigned player_species; /* 0 Marine, 1 Alien, 2 Predator */
    unsigned current_weapon; /* original cur_wepno, used for Predator melee selection */
    uint8_t predator_owned; /* retail weapon owned bits 1..4, per actor */
    unsigned marine_inventory_valid; /* absent only from legacy bot smoke views */
    uint8_t marine_owned; /* original per-actor cur_weps, bits 1..4 */
    uint16_t marine_ammo[4]; /* per-actor original ammo_info[].cur, no fabricated rounds */
    unsigned marine_unarmed; /* all owned guns empty: escape, do not engage */
    unsigned marine_low_ammo; /* ALL owned pools <= 25%: proactively seek supplies */
    unsigned retreat_to_refuge; /* source-legal escape waypoint, not a foe */
    uint32_t retreat_refuge_x,retreat_refuge_y;
    int player_energy,max_energy,medpak;
    unsigned health_low,health_critical; /* PC-only tactical policy */
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
    /* Optional nearest living peer, only when PC Player Collisions is ON;
     * sampled from this ACTOR'S original native world, never other levels. */
    /* Explicit runtime switch: the absence of a nearby peer is not enough
     * to invalidate a yielding maneuver cached before collisions changed.
     * Default zero preserves the ON case for older isolated test callers. */
    unsigned player_collision_disabled;
    unsigned nearby_peer_valid,nearby_peer_index;
    uint32_t nearby_peer_x,nearby_peer_y,nearby_peer_heading;
    unsigned active_door_count;
    uint32_t active_door_offsets[2]; /* Relative to map.cells (header skipped). */
} AvpMpBotObservation;
/* R487: refresh exact exit history and add temporary route exclusions for
 * remembered, currently live acid chokes. Caller owns the private risk grid. */
unsigned avp_mp_bot_acid_route_memory(AvpMpBotState *state,
    const AvpMpBotObservation *view,uint8_t risk[AVP_MP_BOT_MAP_CELLS]);
int avp_mp_bot_acid_goal_blocked(const AvpMpBotObservation *view);
int avp_mp_bot_acid_goal_unsafe(const AvpMpBotState *state,const AvpMpBotObservation *view);

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

/* Source AMP.S::amp_begin_death() spray: 96<<9 on BOTH axes. A
 * modest 16<<9 movement cushion prevents firing at the exact boundary. */
int avp_mp_bot_death_spray_near_player(uint32_t alien_x,uint32_t alien_y,
                                      uint32_t player_x,uint32_t player_y);
/* Can the original acid contact square be bypassed on a legal
 * single-cell-wide approach, including a vent? No fictional 3x3 clearance. */
int avp_mp_bot_alien_death_passable(const AvpMpBotMap *map,
                                   uint32_t x,uint32_t y);
/* True only for an immediately endangered non-Alien player with no legal
 * local motion increasing their distance from this original enemy Alien. */
int avp_mp_bot_alien_traps_player(const AvpMpBotMap *map,
    uint32_t alien_x,uint32_t alien_y,uint32_t player_x,uint32_t player_y);
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
/* Probe is called on candidate player positions by the host (retail
 * LineOfSight plus source collmap prop clearance). The target is an ALREADY
 * observed contact; no private AMP location is discovered by this search. */
typedef int (*AvpMpBotFiringLaneProbe)(void *ctx,uint32_t x,uint32_t y,
                                       uint32_t tx,uint32_t ty);
int avp_mp_bot_pick_firing_lane(const AvpMpBotMap *map,
    uint32_t x,uint32_t y,uint32_t known_target_x,uint32_t known_target_y,
    AvpMpBotFiringLaneProbe lane,void *ctx,
    unsigned *lane_x,unsigned *lane_y);
/* PC co-op only: original lift access, adjacent/jam restrictions, six-cell
 * approach envelope and physically reachable original map path. Callers
 * must independently prove a human is INSIDE the authored lift chamber
 * or that another bot owns the SAME authenticated lift shaft. */
int avp_mp_bot_lift_boarding_route(const AvpMpBotObservation *view,
    unsigned lift_x,unsigned lift_y,unsigned destination,AvpMpBotRoute *out);
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
int avp_mp_bot_egg_avoided(const AvpMpBotState *state,unsigned level,
    unsigned slot,unsigned generation,uint64_t tick);
void avp_mp_bot_egg_watch(AvpMpBotState *state,const AvpMpBotObservation *view,
    unsigned generation);
void avp_mp_bot_step(AvpMpBotState *state,const AvpMpBotObservation *view,
                     AvpMpPlayerCommand *out);
int avp_mp_bot_tactical_travel(AvpMpBotState *state, AvpMpBotObservation *view);
int avp_mp_bot_objective_recoverable(const AvpMpBotObservation *view);
void avp_mp_bot_loop_observe(AvpMpBotState *state,const AvpMpBotObservation *view);
int avp_mp_bot_loop_ready(const AvpMpBotState *state,const AvpMpBotObservation *view);
void avp_mp_bot_step_precise(AvpMpBotState *state,const AvpMpBotObservation *view,
                             int maximum_turn,AvpMpPlayerCommand *out);
#endif
