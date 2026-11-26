#include "local_mp_bot.h"
#include "amp.h"
#include "local_mp_input.h"
#include "local_mp_move_guard.h"
#include "doors.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Coordinate and wall conventions come from original PLAYER.S/COLLIDE.S.
 * This planner supplies player input commands, never direct world displacement. */
#define BOT_PI 3.14159265358979323846
#define BOT_INF UINT32_MAX
#define BOT_WALL_SOLID AVP_WB_SOLID
#define BOT_CELL_BYTES 8u
#define BOT_RISK_NEAR 35u
#define BOT_RISK_ACID 275u
#define BOT_DOOR_PENALTY 24u
#define BOT_PROP_PENALTY 85u
#define BOT_ACID_CONTACT ((double)(30u << 9)/65536.0)
#define BOT_ACID_CLEARANCE (BOT_ACID_CONTACT + 0.016)
/* AMP.S::amp_begin_death applies -50 inside an INCLUSIVE square:
 * abs(dx) <= 96<<9 && abs(dy) <= 96<<9 in original 16.16 units.
 * User-requested bot minimum is 0.76 tile, NOT the earlier 1.0 tile:
 * ceil(0.76 * 65536) = 49808 (the first representable offset at or above 0.76).
 * Compare exact integer deltas to avoid a floating-point boundary slip. */
#define BOT_ALIEN_DEATH_STANDOFF_UNITS 49808u
#define BOT_PLAYER_WALL_MARGIN ((double)(40u << 9)/65536.0)
#define BOT_LANE_MARGIN 0.014
/* MAZESCRN.S melee definitions: Alien tail/swipe/bite and Predator
 * combi-stick/wrist blade ALL have distance 0x90.  fire_wep() converts the
 * original value to 16.16 map units via <<8.  These fractions are PC-only
 * bot positioning margins, not changes to the original weapon range. */
#define BOT_MELEE_REACH ((double)(0x90u << 8)/65536.0)
#define BOT_MELEE_HOLD_NEAR (BOT_MELEE_REACH*0.60)
#define BOT_MELEE_HOLD_FAR (BOT_MELEE_REACH*0.80)
/* MAZESCRN.S defines melee shot width as 20 original pixels for Alien
 * tail/swipe/bite and Predator combi-stick/wrist-blades.  PLAYER.S::TestSpark
 * first checks an axis-aligned shot box expanded by this weapon width, then
 * a perpendicular aim strip. This makes diagonal strikes possible outside
 * a plain circular distance test; it does NOT change the original hitbox. */
#define BOT_MELEE_SHOT_WIDTH ((double)(20u << 9)/65536.0)

/* Species restrictions are source-authored in DOORS.S::DoorKeys and
 * DOORS.S::do_lifts; no new physics or access privileges are granted. */
int avp_mp_bot_transition_allowed(unsigned species,unsigned from_level,
                                   unsigned to_level,
                                   enum AvpMpBotTransitionKind kind)
{
    if(species>2u || from_level<1u || from_level>15u ||
       to_level<1u || to_level>15u || kind>AVP_MP_BOT_TRANS_LIFT)
        return 0;
    if(species==1u && kind==AVP_MP_BOT_TRANS_LIFT)return 0;
    if(species==2u && (kind==AVP_MP_BOT_TRANS_DUCT ||
       (from_level>=6u && from_level<=13u) ||
       (to_level>=6u && to_level<=13u)))return 0;
    return 1;
}

/* Actual level arrivals, not chosen goals, arm a bounded reverse guard.
 * Minimum four seconds, then release after two tiles of egress; maximum
 * eighteen seconds even in a dead end. Other routes are never excluded. */
void avp_mp_bot_duct_arrived(AvpMpBotDuctMemory *m,unsigned from,unsigned level,
    uint32_t x,uint32_t y,uint64_t tick)
{
    if(!m)return;
    memset(m,0,sizeof *m);
    if(from!=level && from>=1u && from<=15u && level>=1u && level<=15u &&
       ((from>=6u && from<=13u)||(level>=6u && level<=13u)))
        *m=(AvpMpBotDuctMemory){from,level,x,y,tick};
}
int avp_mp_bot_duct_reverse_block(const AvpMpBotDuctMemory *m,unsigned level,
    unsigned dest,uint32_t x,uint32_t y,uint64_t tick)
{
    if(!m || !m->level || m->level!=level || m->from!=dest || tick<m->tick ||
       tick-m->tick>=180u)return 0;
    return tick-m->tick<40u ||
        (llabs((long long)x-m->x)<131072 && llabs((long long)y-m->y)<131072);
}

/* Preserve at least three source weapon hits' worth of health: the
 * intended shot plus a conservative two-shot animation/in-flight margin.
 * Same-tick commands from other bots consume additional reservations.
 * This only returns permission for bot inputs, not a damage modifier. */
int avp_mp_bot_weakening_allowed(int original_energy,unsigned weapon_damage,
                                 unsigned other_bot_reservations)
{
    return original_energy>0 && weapon_damage>0 &&
        (uint64_t)(unsigned)original_energy >
        (uint64_t)weapon_damage*3u+(uint64_t)other_bot_reservations;
}

/* The original AMP.S egg opens when a non-Alien approaches within
 * 128<<9 on both axes with optical LOS. An early weapon hit lowers energy
 * below ldir and sets AMP_EGGOPEN, bypassing the normal 20-tick hatch wait.
 * With only a Shotgun (10 damage, original egg health 20), a co-op Marine
 * should trigger native proximity activation FIRST, then use the original
 * shotgun FIRE sequence to destroy the activated egg in two hits.
 * This is a command-only tactic; it never edits the AMP or weapon state. */
unsigned avp_mp_bot_shotgun_egg_policy(const AvpMpBotObservation *v)
{
    if(!v || v->deathmatch || v->player_species!=0u ||
       !v->marine_inventory_valid || !v->hostile_target ||
       !v->target_visible || !v->target_is_egg ||
       v->goal!=AVP_MP_BOT_HOSTILE ||
       (v->marine_owned&0x1eu)!=(1u<<1))return 0u;
    if(v->target_egg_unopened)
        return v->marine_ammo[0]>=2u?1u:3u;
    return 2u;
}

/* R278 co-op defense: all distances are ORIGINAL 16.16 fixed-point
 * positions, squared in 64-bit arithmetic. Selection is permitted ONLY
 * after optical sight; a motion tracker does not detect stationary eggs.
 * A nearby Egg or Facehugger is a more immediate danger to a companion
 * than an unrelated distant roaming enemy. Predator discs are ranged, so
 * a visibly sighted Predator threatens companions farther away. */
unsigned avp_mp_bot_guard_urgency(unsigned creature,uint64_t teammate_d2,
                                  uint64_t observer_d2)
{
    const uint64_t unit=(uint64_t)65536u*65536u;
    if(observer_d2>unit*70u)return 0u; /* outside effective visual/ranged envelope */
    switch(creature){
    case AC_FACEHUG:return teammate_d2<=unit*16u?50u:0u; /* 4 cells */
    case AC_EGG:return teammate_d2<=unit*16u?45u:0u; /* 4 cells */
    case AC_PREDATOR:return teammate_d2<=unit*64u?40u:0u; /* disc/laser threat */
    case AC_ALIEN:case AC_ACRAWL:
        return teammate_d2<=unit*4u?35u:0u;
    default:return 0u;
    }
}

/* Source-cell reachability check defined after the native route planner. */
static int bot_tracker_reachable_waypoint(const AvpMpBotObservation *v,
                                          uint32_t *way_x,uint32_t *way_y);
static int nav_valid(const AvpMpBotMap *m);

/* A temporarily abandoned scanner-only contact stays abandoned until its
 * retry deadline, and until either the bot or the observed Alien actually
 * moves into a different region. This prevents re-locking every 8 seconds
 * while everyone remains in the same corner. Original visual contact
 * and a new AMP generation always override this scanner-only decision.
 * Sighted enemies NEVER take this path and remain urgent combat targets. */
int avp_mp_bot_tracker_contact_suppressed(const AvpMpBotState *s,
    unsigned slot,unsigned level,unsigned generation,
    uint32_t x,uint32_t y,uint64_t tick)
{
    if(!s || !s->tracker_seek_fail_until ||
       level!=s->tracker_seek_fail_level ||
       (slot==s->tracker_seek_fail_slot && s->tracker_seek_fail_serial &&
        generation!=s->tracker_seek_fail_serial))return 0;
    /* Multiple aliens in the SAME hidden pocket must not take turns
     * reinstalling an already-failed scanner goal. Different areas remain
     * valid investigation targets. */
    if(slot!=s->tracker_seek_fail_slot &&
       ((x>>16)!=(s->tracker_seek_fail_x>>16) ||
        (y>>16)!=(s->tracker_seek_fail_y>>16)))return 0;
    if(llabs((long long)(int64_t)x-(long long)(int64_t)s->tracker_seek_fail_x)>=65536 ||
       llabs((long long)(int64_t)y-(long long)(int64_t)s->tracker_seek_fail_y)>=65536)
        return 0;
    if(tick>=s->tracker_seek_fail_until &&
       (llabs((long long)(int64_t)s->previous_x-
              (long long)(int64_t)s->tracker_seek_fail_bot_x)>=65536 ||
        llabs((long long)(int64_t)s->previous_y-
              (long long)(int64_t)s->tracker_seek_fail_bot_y)>=65536))
        return 0;
    return 1;
}

void avp_mp_bot_tracker_hold(AvpMpBotState *s,AvpMpBotObservation *v,
                             unsigned generation)
{
    uint64_t d2;
    if(!s||!v)return;
    /* A currently forbidden radar-only goal must NOT be reacquired merely
     * because the scanner still reports the same living Alien. */
    if(v->tracker_only_target && !v->target_visible &&
       (v->tick<s->tracker_cooldown_until ||
        avp_mp_bot_tracker_contact_suppressed(s,v->target_slot,
           v->observed_world_level,generation,v->target_x,v->target_y,v->tick))){
        s->tracker_seek_valid=0u;
        s->tracker_stall_valid=0u;
        v->has_target=v->hostile_target=v->tracker_only_target=0u;
        return;
    }
    /* Visual confirmation, a visible melee attacker, and team rescue
     * must interrupt scanner investigation instantly. */
    if(!v->tracker_only_target || !v->has_target || !v->hostile_target ||
       v->target_visible || (v->melee_visible && v->melee_threat) ||
       v->other_player_alien_trapped || v->other_player_swarmed){
        s->tracker_seek_valid=0u;
        s->tracker_stall_valid=0u;
        s->tracker_investigate_active=0u;
        return;
    }
    if(!s->tracker_investigate_active || s->tracker_seek_level!=v->observed_world_level ||
       v->tick<s->tracker_investigate_since){
        s->tracker_investigate_active=1u;
        s->tracker_investigate_since=v->tick;
    }
    /* Bound continuous investigation even when the selected contact changes. */
    if(v->tick-s->tracker_investigate_since>=80u){
        s->tracker_investigate_active=s->tracker_seek_valid=s->tracker_stall_valid=0u;
        s->tracker_cooldown_until=v->tick+30u;
        v->has_target=v->hostile_target=v->tracker_only_target=0u;
        return;
    }
    d2=(uint64_t)((int64_t)v->target_x-(int64_t)v->x)*
                       ((int64_t)v->target_x-(int64_t)v->x)+
       (uint64_t)((int64_t)v->target_y-(int64_t)v->y)*
                       ((int64_t)v->target_y-(int64_t)v->y);
    if(!s->tracker_seek_valid || s->tracker_seek_slot!=v->target_slot ||
       s->tracker_seek_level!=v->observed_world_level ||
       s->tracker_seek_serial!=generation || v->tick>=s->tracker_seek_until ||
       /* At the old investigated waypoint, commit to the next scanner
        * sample instead of oscillating around the empty square. */
       (v->tick+16u>=s->tracker_seek_until &&
        (uint64_t)((int64_t)s->tracker_seek_x-(int64_t)v->x)*
                    ((int64_t)s->tracker_seek_x-(int64_t)v->x)+
         (uint64_t)((int64_t)s->tracker_seek_y-(int64_t)v->y)*
                    ((int64_t)s->tracker_seek_y-(int64_t)v->y)<
         (uint64_t)20000u*20000u && d2>(uint64_t)50000u*50000u)){
        uint32_t waypoint_x=v->target_x,waypoint_y=v->target_y;
        if(!bot_tracker_reachable_waypoint(v,&waypoint_x,&waypoint_y)){
            /* When a contact is entirely unreachable, investigate other
             * cells instead of pushing and reversing against a solid wall
             * indefinitely. Host will issue a normal patrol goal. */
            s->tracker_seek_valid=0u;
            s->tracker_seek_fail_slot=v->target_slot;
            s->tracker_seek_fail_level=v->observed_world_level;
            s->tracker_seek_fail_until=v->tick+80u;
            s->tracker_cooldown_until=v->tick+30u;
            s->tracker_investigate_active=0u;
            s->tracker_seek_fail_serial=generation;
            s->tracker_seek_fail_x=v->target_x;
            s->tracker_seek_fail_y=v->target_y;
            s->tracker_seek_fail_bot_x=v->x;
            s->tracker_seek_fail_bot_y=v->y;
            s->tracker_stall_valid=0u;
            v->has_target=v->hostile_target=v->tracker_only_target=0u;
            return;
        }
        s->tracker_seek_valid=1u;
        s->tracker_seek_slot=v->target_slot;
        s->tracker_seek_level=v->observed_world_level;
        s->tracker_seek_serial=generation;
        s->tracker_seek_x=waypoint_x;
        s->tracker_seek_y=waypoint_y;
        s->tracker_seek_until=v->tick+24u; /* 2.4 original seconds */
    }
    /* Raw tracker contact before we replace it with the held waypoint. */
    {
    const uint32_t live_contact_x=v->target_x,live_contact_y=v->target_y;
    v->target_x=s->tracker_seek_x;
    v->target_y=s->tracker_seek_y;
    /* Stationary scanner-target deadlock: the old code refreshed the
     * waypoint forever even while the Marine was staring at a wall.
     * Require measurable progress toward its map-legal search destination.
     * This watchdog watches bot position, not issued movement buttons. */
    {
        const double dx=((double)(int64_t)v->target_x-(double)(int64_t)v->x)/65536.0;
        const double dy=((double)(int64_t)v->target_y-(double)(int64_t)v->y)/65536.0;
        /* Route distance, not straight-line distance: a legitimate detour
         * can initially move away from a scanner contact. */
        AvpMpBotRoute path={0};
        const int routed=nav_valid(&v->map) &&
            avp_mp_bot_route(&v->map,v->x>>16,v->y>>16,
                v->target_x>>16,v->target_y>>16,&path);
        double distance=hypot(dx,dy);
        if(routed && (path.next_x!=(v->x>>16) || path.next_y!=(v->y>>16)))
            distance=(double)path.cost/10.0 +
                hypot((double)path.next_x+0.5-(double)v->x/65536.0,
                      (double)path.next_y+0.5-(double)v->y/65536.0);
        if(!s->tracker_stall_valid ||
           s->tracker_stall_slot!=v->target_slot ||
           s->tracker_stall_level!=v->observed_world_level ||
           s->tracker_stall_serial!=generation ||
           s->tracker_stall_goal_x!=v->target_x ||
           s->tracker_stall_goal_y!=v->target_y ||
           v->tick<s->tracker_stall_since){
            s->tracker_stall_valid=1u;
            s->tracker_stall_slot=v->target_slot;
            s->tracker_stall_level=v->observed_world_level;
            s->tracker_stall_serial=generation;
            s->tracker_stall_goal_x=v->target_x;
            s->tracker_stall_goal_y=v->target_y;
            s->tracker_stall_best_distance=distance;
            s->tracker_stall_since=v->tick;
        }else if(distance+0.25<s->tracker_stall_best_distance){
            s->tracker_stall_best_distance=distance;
            s->tracker_stall_since=v->tick;
        }else if(v->tick>=s->tracker_stall_since+18u){
            /* Do not break a legitimate source door-opening interaction;
             * the door must finish its animation before movement resumes. */
            const int waiting_door=routed && path.requires_door_use &&
                v->tick-s->tracker_investigate_since<40u;
            if(!waiting_door){
                s->tracker_seek_valid=0u;
                s->tracker_stall_valid=0u;
                s->tracker_seek_fail_slot=v->target_slot;
                s->tracker_seek_fail_level=v->observed_world_level;
                s->tracker_seek_fail_serial=generation;
                /* Remember the LIVE contact, not the investigation point. */
                s->tracker_seek_fail_x=live_contact_x;
                s->tracker_seek_fail_y=live_contact_y;
                s->tracker_seek_fail_bot_x=v->x;
                s->tracker_seek_fail_bot_y=v->y;
                s->tracker_seek_fail_until=v->tick+80u;
                s->tracker_cooldown_until=v->tick+30u;
                s->tracker_investigate_active=0u;
                v->has_target=v->hostile_target=v->tracker_only_target=0u;
            }else s->tracker_stall_since=v->tick;
        }
    }
    }
}

/* RE44 R291: a motion tracker yields whereabouts, NOT sight. R290's
 * host kept unseen scanner contacts as HOSTILE while holding a search
 * waypoint; this let combat movement and luring decisions interfere with
 * source-map investigation. Keep the cached location and actual route,
 * but do not claim an attackable enemy until the original forward visual
 * check AND LineOfSight succeed on a later observation. This also applies
 * to an enemy last seen optically and now out of view. Never mutate the
 * actual contact table here: the host owns age/generation/remembering. */
void avp_mp_bot_coop_contact_as_memory(AvpMpBotObservation *v)
{
    if(!v || v->deathmatch || v->player_species!=AVP_MP_MARINE ||
       !v->has_target || v->goal!=AVP_MP_BOT_HOSTILE ||
       v->target_visible || v->target_slot>=AVP_NUM_AMPS)return;
    v->goal=AVP_MP_BOT_INVESTIGATE;
    v->hostile_target=0u;
    v->target_heading_valid=0u;
    v->target_health_valid=0u;
    v->nonlethal_choke_fire=0u;
    v->target_is_egg=0u;
    v->target_egg_unopened=0u;
}

/* When an already-visible enemy threatens the bot or a teammate, keep the
 * combat target. The original R275 host selected these optical contacts
 * before the optional lift invitation, but the invitation overwrote them.
 * Only observed threats block boarding: remembered/tracker-only contacts
 * should not strand a party outside a legitimate lift. */
int avp_mp_bot_lift_invitation_allowed(const AvpMpBotObservation *v)
{
    return v && !(v->hostile_target && v->target_visible) &&
           !(v->melee_threat!=AVP_MP_BOT_THREAT_NONE && v->melee_visible);
}

int avp_mp_bot_lift_keep_trip(const AvpMpBotObservation *v,
    unsigned intent_level,unsigned level,uint64_t last_intent_tick,
    unsigned within_lift_chamber)
{
    return v && !v->deathmatch && intent_level==level &&
           level>=1u && level<=5u && within_lift_chamber &&
           v->tick>=last_intent_tick && v->tick-last_intent_tick<=20u &&
           !((v->hostile_target && v->target_visible) ||
             (v->melee_visible && v->melee_threat!=AVP_MP_BOT_THREAT_NONE));
}
int avp_mp_bot_lift_needs_disembark(unsigned arrival_pending,
    unsigned inside_arrival_chamber)
{
    return arrival_pending && inside_arrival_chamber;
}

int avp_mp_bot_lift_jammed(unsigned level,unsigned x,unsigned y,int direction)
{
    return x==9u && y==21u &&
           ((level==3u && direction>0) || (level==4u && direction<0));
}

int avp_mp_bot_lift_agree(AvpMpBotLiftClaims *claims,unsigned level,
    unsigned x,unsigned y,unsigned actor,unsigned wanted,uint64_t tick,
    unsigned *leader,unsigned *destination,uint64_t *created)
{
    unsigned slot=AVP_MP_BOT_LIFT_CLAIMS;
    if(!claims||!leader||!destination||!created ||
       level<1u||level>5u||wanted<1u||wanted>5u ||
       (wanted+1u!=level && wanted!=level+1u) ||
       avp_mp_bot_lift_jammed(level,x,y,wanted>level?1:-1))return 0;
    for(unsigned i=0;i<AVP_MP_BOT_LIFT_CLAIMS;i++){
        AvpMpBotLiftClaim *c=&claims->entries[i];
        if(c->active && tick<c->until && c->level==level &&
           c->x==x && c->y==y){
            if(c->leader==actor)c->until=tick+120u;
            *leader=c->leader;*destination=c->destination;
            *created=c->created;
            return 1;
        }
        if((!c->active||tick>=c->until) && slot==AVP_MP_BOT_LIFT_CLAIMS)
            slot=i;
    }
    if(slot==AVP_MP_BOT_LIFT_CLAIMS)slot=actor%AVP_MP_BOT_LIFT_CLAIMS;
    claims->entries[slot]=(AvpMpBotLiftClaim){1u,level,x,y,actor,wanted,
                                               tick,tick+120u};
    *leader=actor;*destination=wanted;*created=tick;
    return 1;
}

int avp_mp_bot_lift_boarding_route(const AvpMpBotObservation *v,
    unsigned lift_x,unsigned lift_y,unsigned destination,AvpMpBotRoute *out)
{
    const int64_t tx=((int64_t)lift_x<<16)|0x8000;
    const int64_t ty=((int64_t)lift_y<<16)|0x8000;
    if(!v||!out||!v->map.cells||v->current_level<1u||v->current_level>5u||
       (destination+1u!=v->current_level &&
        destination!=v->current_level+1u) ||
       destination<1u||destination>5u||
       !avp_mp_bot_transition_allowed(v->player_species,
           v->current_level,destination,AVP_MP_BOT_TRANS_LIFT) ||
       avp_mp_bot_lift_jammed(v->current_level,lift_x,lift_y,
                               destination>v->current_level?1:-1) ||
       llabs((int64_t)v->x-tx)>(int64_t)(6u<<16) ||
       llabs((int64_t)v->y-ty)>(int64_t)(6u<<16))return 0;
    return avp_mp_bot_route(&v->map,v->x>>16,v->y>>16,
                             lift_x,lift_y,out);
}

int avp_mp_bot_outside_bounds(uint32_t x,uint32_t y,
                              unsigned width,unsigned height)
{
    return width==0u || height==0u || (int32_t)x<0 || (int32_t)y<0 ||
           (x>>16)>=width || (y>>16)>=height;
}
int avp_mp_bot_playable_position(const uint8_t *cells,unsigned width,
                                unsigned height,uint32_t x,uint32_t y)
{
    unsigned cx,cy;
    if(!cells || !width || !height || width>64u || height>64u ||
       avp_mp_bot_outside_bounds(x,y,width,height))return 0;
    cx=x>>16;cy=y>>16;
    /* Retail maze records have eight bytes per cell; byte five is the
     * authored floor selector. No new geometry, invisible walls or nav map. */
    return cells[((size_t)cy*width+cx)*8u+5u]!=0u;
}
int avp_mp_bot_marine_aim_ready(unsigned weapon,double error,
    double range,int emergency)
{
    double width,limit;
    if(!isfinite(error)||!isfinite(range)||range<0.0)return 0;
    /* PLAYER.S TestSpark uses original fire_width*512 fixed-point units.
     * Gun definitions: shotgun 40, pulse rifle / smart gun 20. No change to
     * source random spread, projectile physics, damage or hit selection. */
    width=(weapon==1u?40.0:20.0)/128.0;
    if(emergency && range<1.4)return fabs(error)<BOT_PI/6.0;
    limit=atan2(width*0.85,range>0.1?range:0.1);
    /* The original Marine D-pad first rotates 0x300/65536 per press
     * (~4.22 degrees), then accelerates. A three-degree tolerance allows
     * convergence without accepting the former ~16-degree firing cone.
     * The bot-specific turn threshold below MUST match this minimum or a
     * 3-8-degree error could leave it permanently staring, unable to fire. */
    if(limit<BOT_PI/60.0)limit=BOT_PI/60.0;
    if(limit>BOT_PI/8.0)limit=BOT_PI/8.0;
    return fabs(error)<limit;
}
int avp_mp_bot_health_low(int energy,int cap)
{
    return energy>0 && cap>0 && (int64_t)energy*100<=(int64_t)cap*60;
}
int avp_mp_bot_health_critical(int energy,int cap)
{
    return energy>0 && cap>0 && (int64_t)energy*100<=(int64_t)cap*30;
}
int avp_mp_bot_marine_needs_resupply(unsigned owned,
    const uint16_t ammo[4],const uint16_t maxima[4])
{
    unsigned active=0u;
    if(!ammo||!maxima)return 0;
    for(unsigned i=0;i<4u;i++)if(owned&(1u<<(i+1u))){
        active++;
        /* A useful loaded gun postpones panic. Check all actually owned
         * pools, not the currently selected weapon or irrelevant slots.
         * On the final quarter of every magazine, start planning escape
         * while some ammunition remains. The native caps do not change. */
        if(ammo[i] && (uint32_t)ammo[i]*4u>(uint32_t)maxima[i])
            return 0;
    }
    return active>0u;
}
/* R295: a bot should not drift across the map after a scanner contact or
 * optional supply. This policy is invoked ONLY with a genuine living co-op
 * teammate, once the original observation/goal has been selected. It never
 * replaces a visible fight, victim rescue, survival objective or a committed
 * original lift/duct trip. A short local ammo-room detour remains possible:
 * supplies within six cells of the teammate can be collected even if the
 * bot temporarily passes beyond the normal four-cell formation radius. */
int avp_mp_bot_team_regroup_priority(const AvpMpBotObservation *v,
    uint64_t teammate_distance_sq,uint64_t objective_distance_sq,
    unsigned supply_fills_empty_reserve)
{
    const uint64_t regroup_radius=(uint64_t)(4u<<16)*(4u<<16);
    const uint64_t local_supply_radius=(uint64_t)(6u<<16)*(6u<<16);
    if(!v || v->deathmatch || v->facehugged || v->marine_unarmed ||
       v->other_player_alien_trapped || v->other_player_swarmed ||
       v->melee_threat || v->retreat_to_teammate || v->retreat_to_refuge ||
       (v->hostile_target && v->target_visible) ||
       v->goal==AVP_MP_BOT_COCOON || v->goal==AVP_MP_BOT_QUEEN_WAIT ||
       v->goal==AVP_MP_BOT_LIFT || v->goal==AVP_MP_BOT_EXIT)return 0;
    if(v->health_low &&
       (v->goal==AVP_MP_BOT_HEALTH || v->goal==AVP_MP_BOT_MEDICAL))return 0;
    if(v->goal==AVP_MP_BOT_SUPPLY){
        if(v->marine_unarmed || v->marine_low_ammo ||
           supply_fills_empty_reserve)return 0;
        if(teammate_distance_sq<=local_supply_radius &&
           objective_distance_sq<=local_supply_radius)return 0;
    }
    /* A distant remembered/scanner-only enemy must not pull the whole
     * squad apart. An already-nearby teammate's position is a useful
     * stable anchor even before the bot itself leaves formation. */
    return teammate_distance_sq>regroup_radius ||
        (v->has_target && v->goal!=AVP_MP_BOT_FOLLOW &&
         objective_distance_sq>local_supply_radius);
}

static double angle_diff(uint32_t heading,double dx,double dy)
{
    const double aim=atan2(-dy,dx);
    const double current=(double)heading*(2.0*BOT_PI/4294967296.0);
    double difference=aim-current;
    while(difference>BOT_PI)difference-=2.0*BOT_PI;
    while(difference< -BOT_PI)difference+=2.0*BOT_PI;
    return difference;
}
/* PC bot decision only: approximate the original PLAYER.S::TestSpark
 * finite-distance shot box and perpendicular width for native melee weapons.
 * All real hits still go through the unchanged Jaguar TestSpark routine.
 * A simple Euclidean range of 0x90<<8 rejected valid oblique strikes even
 * when the target was straight along the weapon ray; enemies using the
 * original square contact range could then hit a motion-blocked bot freely.
 * Never issue speculative attacks into the back half-space or off the ray. */
static int bot_melee_source_shot_box(const AvpMpBotObservation *v,
                                     double tx,double ty)
{
    const double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    const double fx=cos(a),fy=-sin(a);
    const double side=fabs(tx*fy-ty*fx);
    const double forward=tx*fx+ty*fy;
    return forward>=0.0 &&
           fabs(tx)<fabs(fx*BOT_MELEE_REACH)+BOT_MELEE_SHOT_WIDTH &&
           fabs(ty)<fabs(fy*BOT_MELEE_REACH)+BOT_MELEE_SHOT_WIDTH &&
           /* Source truncates the xoff signed fixed-point word to pixels.
            * A sub-pixel remainder is permitted at the outer width edge. */
           side<BOT_MELEE_SHOT_WIDTH+(1.0/128.0);
}
static int nav_valid(const AvpMpBotMap *m)
{
    return m && m->cells && m->wall_flags && m->width>=1u &&
           m->height>=1u && m->width<=64u && m->height<=64u;
}
static unsigned nav_index(unsigned x,unsigned y){return y*64u+x;}
/* At most one bot sends Use for a particular original door byte in a
 * cooldown window. The source door animation remains authoritative. */
int avp_mp_bot_door_claim(AvpMpBotDoorClaims *claims,
    const uint8_t *cells,uint32_t offset,uint64_t tick)
{
    unsigned choice=AVP_MP_BOT_DOOR_CLAIMS;
    if(!claims||!cells)return 0;
    for(unsigned i=0;i<AVP_MP_BOT_DOOR_CLAIMS;i++){
        AvpMpBotDoorClaim *c=&claims->entries[i];
        if(c->cells==cells && c->offset==offset){
            if(tick<c->until)return 0;
            c->until=tick+30u;return 1;
        }
        if(choice==AVP_MP_BOT_DOOR_CLAIMS && (!c->cells || tick>=c->until))choice=i;
    }
    if(choice==AVP_MP_BOT_DOOR_CLAIMS){
        choice=claims->next_evict++%AVP_MP_BOT_DOOR_CLAIMS;
    }
    claims->entries[choice]=(AvpMpBotDoorClaim){cells,offset,tick+30u};
    return 1;
}
/* Only the original game's active door slots can prove a door is
 * currently moving.  The byte's 0 value is ambiguous immediately after
 * start_normal_or_double() and just before its final native DoorStep().
 * This guard affects bot input only; humans and source door physics are
 * unchanged. */
static int bot_door_animating(const AvpMpBotObservation *v,uint32_t offset)
{
    if(!v)return 0;
    for(unsigned i=0;i<v->active_door_count && i<2u;i++)
        if(v->active_door_offsets[i]==offset)return 1;
    return 0;
}
static unsigned nav_x(unsigned n){return n%64u;}
static unsigned nav_y(unsigned n){return n/64u;}
static const uint8_t *nav_cell(const AvpMpBotMap *m,unsigned x,unsigned y)
{ return m->cells+((size_t)y*m->width+x)*BOT_CELL_BYTES; }

/* A collectible's route may look valid at GRID resolution while the
 * original actor repeatedly collides with furniture or a wall. Record
 * actual progress and temporarily stop selecting this exact AMP identity.
 * Hostile pursuit, exit routing, movement and source pickup rules remain
 * independent of this optional bot-only watchdog. */
static uint64_t bot_pickup_remaining(uint32_t x,uint32_t y,
                                     uint32_t tx,uint32_t ty)
{
    const uint64_t dx=(uint64_t)llabs((long long)(int64_t)x-(long long)(int64_t)tx);
    const uint64_t dy=(uint64_t)llabs((long long)(int64_t)y-(long long)(int64_t)ty);
    return dx>dy?dx:dy;
}
int avp_mp_bot_pickup_avoided(const AvpMpBotPickupMemory *mem,
    unsigned level,unsigned slot,unsigned generation,uint64_t tick)
{
    if(!mem)return 0;
    for(unsigned i=0;i<AVP_MP_BOT_PICKUP_AVOIDS;i++){
        const AvpMpBotPickupAvoidEntry *b=&mem->avoided[i];
        if(b->until>tick && b->level==level && b->slot==slot &&
           b->generation==generation)return 1;
    }
    return 0;
}
void avp_mp_bot_pickup_track(AvpMpBotPickupMemory *mem,
    unsigned level,unsigned slot,unsigned generation,uint64_t tick,
    uint32_t x,uint32_t y,uint32_t tx,uint32_t ty,uint32_t route_cost,
    uint32_t cell)
{
    if(!mem)return;
    if(!mem->active || mem->level!=level || mem->slot!=slot ||
       mem->generation!=generation || mem->target_x!=tx || mem->target_y!=ty){
        mem->active=1u;mem->level=level;mem->slot=slot;
        mem->generation=generation;mem->target_x=tx;mem->target_y=ty;
        mem->best_distance=bot_pickup_remaining(x,y,tx,ty);
        mem->best_route_cost=route_cost;
        mem->last_route_cell=cell;
        mem->last_progress_tick=tick;
    }
}
void avp_mp_bot_pickup_observe(AvpMpBotPickupMemory *mem,
    unsigned level,uint64_t tick,uint32_t x,uint32_t y,
    uint32_t route_cost,uint32_t cell)
{
    uint64_t remaining;
    if(!mem||!mem->active)return;
    if(level!=mem->level){mem->active=0u;return;}
    remaining=bot_pickup_remaining(x,y,mem->target_x,mem->target_y);
    /* Real progress is a >=1/8 tile reduction in farthest-axis distance,
     * or a shorter authored A* route after actually changing cells.
     * Pure turning, wall pushing and same-cell oscillations earn no credit. */
    if(remaining+8192u<=mem->best_distance ||
       (cell!=mem->last_route_cell && route_cost!=UINT32_MAX &&
        route_cost+10u<=mem->best_route_cost)){
        if(remaining<mem->best_distance)mem->best_distance=remaining;
        if(route_cost<mem->best_route_cost)mem->best_route_cost=route_cost;
        mem->last_route_cell=cell;
        mem->last_progress_tick=tick;
    }
    if(tick>=mem->last_progress_tick &&
       tick-mem->last_progress_tick>=40u){
        AvpMpBotPickupAvoidEntry *b=
            &mem->avoided[mem->next_avoid++%AVP_MP_BOT_PICKUP_AVOIDS];
        *b=(AvpMpBotPickupAvoidEntry){mem->level,mem->slot,
                                    mem->generation,tick+300u};
        mem->active=0u;
    }
}

/* The original doors store their opening byte in this cell's LEFT/TOP
 * neighbour boundary: RIGHT uses next cell's LEFT, BOTTOM uses next TOP.
 * Reject inaccessible authored doors unless their actual byte is open.
 * An AVP_ACC_EXIT isn't a normal room passage: handled via exit planning. */
static int nav_edge(const AvpMpBotMap *m,unsigned x,unsigned y,unsigned side,
                    unsigned *nx,unsigned *ny,unsigned *door,unsigned *open)
{
    static const int sx[4]={-1,0,1,0},sy[4]={0,-1,0,1};
    static const unsigned opposite[4]={2,3,0,1};
    const uint8_t *c,*n;
    unsigned f1,f2,offset=0,has_door=0;
    int xx=(int)x+sx[side],yy=(int)y+sy[side];
    if(xx<0||yy<0||xx>=(int)m->width||yy>=(int)m->height)return 0;
    c=nav_cell(m,x,y);n=nav_cell(m,(unsigned)xx,(unsigned)yy);
    /* Only a bot-planning detour, never an original geometry change. */
    if(m->avoid_cell_plus_one==nav_index((unsigned)xx,(unsigned)yy)+1u)return 0;
    /* The original placement grid treats floor byte five==0 as unusable. */
    if(!c[5]||!n[5])return 0;
    f1=m->wall_flags[c[side]];f2=m->wall_flags[n[opposite[side]]];
    if((f1|f2)&BOT_WALL_SOLID)return 0;
    if((f1|f2)&AVP_WB_DOOR){
        const AvpDoorLevelMeta *meta=m->door_meta;
        unsigned i;
        has_door=1;
        if(side==0)offset=((size_t)y*m->width+x)*8u+6u;
        else if(side==1)offset=((size_t)y*m->width+x)*8u+7u;
        else if(side==2)offset=((size_t)yy*m->width+(unsigned)xx)*8u+6u;
        else offset=((size_t)yy*m->width+(unsigned)xx)*8u+7u;
        /* An edge with a broken original door reference is not passable. */
        if(offset>=m->width*m->height*8u)return 0;
        if(meta && m->cells[offset]==0u)for(i=0;i<meta->access_count;i++){
            const AvpDoorAccessEntry *a=&meta->access[i];
            if(a->door_offset!=offset)continue;
            /* Source DOORS.S::access_ok explicitly ALLOWS the Level-5
             * AVP_ACC_ESCAPE door (its negative code only prints the escape
             * notice). R299's generic negative-code rejection stranded the
             * whole real capsule component outside the bot route graph. */
            if((a->code<0 && a->code!=AVP_ACC_ESCAPE) ||
               (a->code>0 && (unsigned)a->code>m->access_level))
                return 0;
        }
    }
    *nx=(unsigned)xx;*ny=(unsigned)yy;
    *door=has_door;*open=has_door?m->cells[offset]:255u;
    return 1;
}
/* Eight-neighbour PC-only navigation: a diagonal edge is legal only
 * through the ENTIRE open 2x2 square. Both cardinal routes around the
 * corner must exist, without doors, props or marked acid in that square.
 * This cannot invent a shortcut across solid panels, locked doors, narrow
 * props or an acid lane. Species-immunity is passed in by the caller. */
static int nav_diagonal(const AvpMpBotMap *m,unsigned x,unsigned y,
                        int dx,int dy,int ignore_acid)
{
    unsigned hx,hy,vx,vy,nx,ny,door,open;
    const unsigned horizontal=dx<0?0u:2u,vertical=dy<0?1u:3u;
    const int tx=(int)x+dx,ty=(int)y+dy;
    if((dx!=1 && dx!=-1)||(dy!=1 && dy!=-1) ||
       tx<0||ty<0||tx>=(int)m->width||ty>=(int)m->height)return 0;
    if(m->avoid_cell_plus_one==nav_index((unsigned)tx,(unsigned)ty)+1u)
        return 0;
    if(!nav_edge(m,x,y,horizontal,&hx,&hy,&door,&open)||door)return 0;
    if(!nav_edge(m,x,y,vertical,&vx,&vy,&door,&open)||door)return 0;
    if(!nav_edge(m,hx,hy,vertical,&nx,&ny,&door,&open)||door ||
       nx!=(unsigned)tx||ny!=(unsigned)ty)return 0;
    if(!nav_edge(m,vx,vy,horizontal,&nx,&ny,&door,&open)||door ||
       nx!=(unsigned)tx||ny!=(unsigned)ty)return 0;
    if(m->risk && !ignore_acid &&
       (m->risk[nav_index(x,y)] || m->risk[nav_index(hx,hy)] ||
        m->risk[nav_index(vx,vy)] ||
        m->risk[nav_index((unsigned)tx,(unsigned)ty)]))return 0;
    if(m->prop_risk &&
       (m->prop_risk[nav_index(x,y)] || m->prop_risk[nav_index(hx,hy)] ||
        m->prop_risk[nav_index(vx,vy)] ||
        m->prop_risk[nav_index((unsigned)tx,(unsigned)ty)]))return 0;
    return 1;
}
unsigned avp_mp_bot_components(const AvpMpBotMap *m,
                                uint16_t out[AVP_MP_BOT_MAP_CELLS])
{
    uint16_t queue[AVP_MP_BOT_MAP_CELLS];
    unsigned labels=0u;
    if(!out)return 0u;
    memset(out,0,AVP_MP_BOT_MAP_CELLS*sizeof *out);
    if(!nav_valid(m))return 0u;
    for(unsigned y=0;y<m->height;y++)for(unsigned x=0;x<m->width;x++){
        unsigned head=0u,tail=0u,origin=nav_index(x,y);
        if(out[origin]||!nav_cell(m,x,y)[5] || (m->risk && m->risk[origin]==3u))continue;
        if(labels>=UINT16_MAX-1u)return 0u;
        ++labels;out[origin]=(uint16_t)labels;
        queue[tail++]=(uint16_t)origin;
        while(head<tail){
            const unsigned cell=queue[head++];
            const unsigned cx=nav_x(cell),cy=nav_y(cell);
            for(unsigned side=0;side<4u;side++){
                unsigned nx,ny,door,open,n;
                if(!nav_edge(m,cx,cy,side,&nx,&ny,&door,&open))continue;
                n=nav_index(nx,ny);
                if(out[n] || (m->risk && m->risk[n]==3u))continue;
                out[n]=(uint16_t)labels;
                queue[tail++]=(uint16_t)n;
            }
        }
    }
    return labels;
}

/* The original Alien death spray is a 96<<9 axis-aligned square,
 * not the smaller persistent amp_acid contact square (30<<9). Keep
 * this PC-only tactical margin OUT of the original game's damage rules. */
int avp_mp_bot_death_spray_near_player(uint32_t alien_x,uint32_t alien_y,
                                      uint32_t player_x,uint32_t player_y)
{
    const int64_t radius=(int64_t)(96u+16u)<<9;
    const int64_t dx=(int64_t)(int32_t)alien_x-(int64_t)(int32_t)player_x;
    const int64_t dy=(int64_t)(int32_t)alien_y-(int64_t)(int32_t)player_y;
    return llabs(dx)<=radius && llabs(dy)<=radius;
}

/* A source Alien death creates an actual +/-30<<9 acid contact square.
 * Instead of treating the entire 3x3 neighborhood as unsafe (which falsely
 * rejects narrow Jaguar vents), inspect the original four wall/door edges
 * and sample potential player-center lanes inside the target cell. The
 * player must retain the original 40<<9 wall margin. No safe centre line
 * means an acid splash can block a one-cell passage. All movement remains
 * subject to the original SafePos check. This is a tactical prediction, not
 * a change to the original acid geometry or death spawn. */
int avp_mp_bot_alien_death_passable(const AvpMpBotMap *m,
                                    uint32_t x,uint32_t y)
{
    const unsigned cx=x>>16,cy=y>>16;
    unsigned connected=0u;
    unsigned nx,ny,door,open;
    const double hazard_x=(double)(int32_t)x/65536.0;
    const double hazard_y=(double)(int32_t)y/65536.0;
    const double margin=BOT_PLAYER_WALL_MARGIN+0.001;
    if(!nav_valid(m)||cx>=m->width||cy>=m->height)return 1;
    for(unsigned side=0;side<4u;side++)
        if(nav_edge(m,cx,cy,side,&nx,&ny,&door,&open))
            connected|=1u<<side;
    if(!connected)return 1; /* No traversable exit is threatened. */
    /* A genuinely open neighboring 2x2 square supplies a legal detour
     * around the future puddle. Narrow ducts lack this extra space, so
     * don't demand that the entire 3x3 neighborhood be open. */
    for(int dx=-1;dx<=1;dx+=2)for(int dy=-1;dy<=1;dy+=2)
        if(nav_diagonal(m,cx,cy,dx,dy,1))return 1;
    /* Check each source-legal cardinal passage orientation. A one-way
     * branch is still checked; no artificial 3x3 clear room is required. */
    for(unsigned side=0;side<4u;side++)if(connected&(1u<<side)){
        const int vertical=(side==1u || side==3u);
        /* A through-passage requires both ends when available. A dead-end
         * can instead use its single entry, but still needs a safe lane. */
        unsigned other=vertical?(side==1u?3u:1u):(side==0u?2u:0u);
        if((connected & (1u<<other)) ||
           (connected & ~(1u<<side))==0u){
            for(unsigned n=0;n<=8u;n++){
                const double lateral=(double)(vertical?cx:cy)+
                    margin+(1.0-2.0*margin)*(double)n/8.0;
                const double offset=fabs(lateral-(vertical?hazard_x:hazard_y));
                if(offset>BOT_ACID_CONTACT+0.001)return 1;
            }
        }
    }
    /* For a corner, evaluate a two-stage path within the same cell. It
     * can make the turn only if its two intermediate center points and
     * the corner point are outside the exact acid-contact square. */
    if(connected && !(connected&(connected-1u)))return 0;
    for(unsigned a=0;a<4u;a++)if(connected&(1u<<a))
      for(unsigned b=a+1u;b<4u;b++)if((connected&(1u<<b)) && ((a+b)&1u)!=0u){
        double px=(double)cx+0.5,py=(double)cy+0.5;
        if(a==0u||b==0u)px=(double)cx+margin;
        if(a==2u||b==2u)px=(double)cx+1.0-margin;
        if(a==1u||b==1u)py=(double)cy+margin;
        if(a==3u||b==3u)py=(double)cy+1.0-margin;
        if(fabs(px-hazard_x)>BOT_ACID_CONTACT+0.001 ||
           fabs(py-hazard_y)>BOT_ACID_CONTACT+0.001)return 1;
      }
    return 0;
}
/* Indexed min-heap with decrease-key: no dynamic allocation or unbounded
 * queue, at most 4096 cells. For 64x64 Jaguar maps this is inexpensive at
 * the unchanged 10 Hz simulation rate. */
typedef struct NavHeap {
    uint16_t cell[AVP_MP_BOT_MAP_CELLS];
    uint16_t position[AVP_MP_BOT_MAP_CELLS]; /* 0 absent, otherwise index+1 */
    unsigned count;
    const uint32_t *distance;
} NavHeap;
static void heap_swap(NavHeap *h,unsigned i,unsigned j)
{
    uint16_t a=h->cell[i],b=h->cell[j];
    h->cell[i]=b;h->cell[j]=a;
    h->position[a]=(uint16_t)(j+1u);h->position[b]=(uint16_t)(i+1u);
}
static void heap_offer(NavHeap *h,uint16_t cell)
{
    unsigned k;
    if(h->position[cell])k=h->position[cell]-1u;
    else{
        if(h->count>=AVP_MP_BOT_MAP_CELLS)return;
        k=h->count++;h->cell[k]=cell;h->position[cell]=(uint16_t)(k+1u);
    }
    while(k){unsigned p=(k-1u)/2u;
        if(h->distance[h->cell[p]]<=h->distance[h->cell[k]])break;
        heap_swap(h,k,p);k=p;
    }
}
static uint16_t heap_take(NavHeap *h)
{
    uint16_t result=h->cell[0];
    h->position[result]=0u;
    --h->count;
    if(h->count){
        unsigned k=0;h->cell[0]=h->cell[h->count];h->position[h->cell[0]]=1u;
        for(;;){unsigned l=k*2u+1u,r=l+1u,b=k;
            if(l<h->count&&h->distance[h->cell[l]]<h->distance[h->cell[b]])b=l;
            if(r<h->count&&h->distance[h->cell[r]]<h->distance[h->cell[b]])b=r;
            if(b==k)break;
            heap_swap(h,k,b);
            k=b;
        }
    }
    return result;
}
static int bot_route_pass(const AvpMpBotMap *m,unsigned sx,unsigned sy,
                          unsigned gx,unsigned gy,AvpMpBotRoute *out,
                          int avoid_props,int *crosses_prop)
{
    uint32_t distance[AVP_MP_BOT_MAP_CELLS];
    uint16_t parent[AVP_MP_BOT_MAP_CELLS];
    uint8_t visited[AVP_MP_BOT_MAP_CELLS];
    NavHeap heap={0};
    unsigned start,goal;
    if(crosses_prop)*crosses_prop=0;
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    if(!nav_valid(m)||sx>=m->width||sy>=m->height||
       gx>=m->width||gy>=m->height)return 0;
    start=nav_index(sx,sy);goal=nav_index(gx,gy);
    if(m->risk && m->risk[goal]==3u && start!=goal)return 0;
    if(start==goal){out->valid=1;out->next_x=gx;out->next_y=gy;return 1;}
    for(unsigned n=0;n<AVP_MP_BOT_MAP_CELLS;n++){
        distance[n]=BOT_INF;parent[n]=UINT16_MAX;visited[n]=0;
    }
    heap.distance=distance;
    distance[start]=0;heap_offer(&heap,(uint16_t)start);
    while(heap.count){
        unsigned u=heap_take(&heap),x=nav_x(u),y=nav_y(u);
        if(visited[u])continue;
        visited[u]=1;
        if(u==goal)break;
        for(unsigned side=0;side<4u;side++){
            unsigned nx,ny,door,open,v,penalty=10u;
            if(!nav_edge(m,x,y,side,&nx,&ny,&door,&open))continue;
            v=nav_index(nx,ny);
            if(visited[v] || (m->risk && m->risk[v]==3u))continue;
            /* Prefer a real authored detour around a collmap prop. If the
             * only path crosses the prop cell, the second route pass retains
             * the old soft cost and lets original SafePos decide movement. */
            if(avoid_props && m->prop_risk && m->prop_risk[v])continue;
            if(door)penalty+=BOT_DOOR_PENALTY;
            if(m->risk){
                if(m->risk[v]==1u)penalty+=BOT_RISK_NEAR;
                else if(m->risk[v]>=2u)penalty+=BOT_RISK_ACID;
            }
            /* Source AMP props are costly, never fictional solid walls:
             * some have enough floor clearance to walk past on one side. */
            if(m->prop_risk && m->prop_risk[v])
                penalty+=BOT_PROP_PENALTY;
            if(distance[u]!=BOT_INF && distance[u]+penalty<distance[v]){
                distance[v]=distance[u]+penalty;parent[v]=(uint16_t)u;
                heap_offer(&heap,(uint16_t)v);
            }
        }
        /* True diagonal path edges, not merely diagonal steering toward a
         * cardinal waypoint. sqrt(2) grid cost ~= 14 vs cardinal 10. */
        for(int dy=-1;dy<=1;dy+=2)for(int dx=-1;dx<=1;dx+=2){
            unsigned nx,ny,v,penalty=14u;
            if(!nav_diagonal(m,x,y,dx,dy,0))continue;
            nx=(unsigned)((int)x+dx);ny=(unsigned)((int)y+dy);
            v=nav_index(nx,ny);
            if(visited[v])continue;
            if(distance[u]!=BOT_INF && distance[u]+penalty<distance[v]){
                distance[v]=distance[u]+penalty;parent[v]=(uint16_t)u;
                heap_offer(&heap,(uint16_t)v);
            }
        }
    }
    if(distance[goal]==BOT_INF)return 0;
    /* Only do a second graph search when the ordinary shortest path
     * actually crosses an original collmap prop. Almost all routes have no
     * such obstruction, so 20-actor matches keep R127 route-planning cost. */
    if(crosses_prop && m->prop_risk){
        unsigned node=goal;
        for(unsigned steps=0;steps<AVP_MP_BOT_MAP_CELLS && node!=start;steps++){
            if(m->prop_risk[node])*crosses_prop=1;
            if(parent[node]==UINT16_MAX)break;
            node=parent[node];
        }
    }
    {
        unsigned next=goal,prev,second=UINT16_MAX;
        for(unsigned steps=0;steps<AVP_MP_BOT_MAP_CELLS;steps++){
            prev=parent[next];
            if(prev==start)break;
            if(prev==UINT16_MAX)return 0;
            second=next;next=prev;
        }
        if(parent[next]!=start)return 0;
        out->valid=1;out->next_x=nav_x(next);out->next_y=nav_y(next);
        if(second!=UINT16_MAX){
            out->lookahead_valid=1u;
            out->lookahead_x=nav_x(second);
            out->lookahead_y=nav_y(second);
        }
        out->cost=distance[goal];
        /* Diagonal edges cross no doors and are already verified open. */
        out->door_is_open=1u;
        for(unsigned dir=0;dir<4u;dir++){
            unsigned nx,ny,door,open;
            if(nav_edge(m,sx,sy,dir,&nx,&ny,&door,&open)&&
               nx==out->next_x && ny==out->next_y){
                out->requires_door_use=door&&open<40u;
                out->door_is_open=!door||open>=40u;
                out->door_value=open;
                if(door){
                    if(dir==0u)out->door_offset=(sy*m->width+sx)*8u+6u;
                    else if(dir==1u)out->door_offset=(sy*m->width+sx)*8u+7u;
                    else if(dir==2u)out->door_offset=(ny*m->width+nx)*8u+6u;
                    else out->door_offset=(ny*m->width+nx)*8u+7u;
                }
                break;
            }
        }
    }
    return 1;
}
int avp_mp_bot_tracker_needs_portal(const AvpMpBotMap *map,
    unsigned sx,unsigned sy,unsigned gx,unsigned gy)
{
    AvpMpBotRoute local={0};
    if(!nav_valid(map)||sx>=map->width||sy>=map->height||
       gx>=map->width||gy>=map->height)return 0;
    return !avp_mp_bot_route(map,sx,sy,gx,gy,&local);
}

int avp_mp_bot_route(const AvpMpBotMap *m,unsigned sx,unsigned sy,
                     unsigned gx,unsigned gy,AvpMpBotRoute *out)
{
    AvpMpBotRoute clear_route={0};
    int crosses_prop=0;
    if(!bot_route_pass(m,sx,sy,gx,gy,out,0,&crosses_prop))return 0;
    /* Only an actually obstructed baseline path warrants the second search.
     * If the authored geometry offers no prop-free route, preserve the
     * original costed path and original MovePlayer/SafePos authority. */
    if(crosses_prop && bot_route_pass(m,sx,sy,gx,gy,&clear_route,1,NULL))
        *out=clear_route;
    return 1;
}
/* An owned scanner knows a moving contact's approximate coordinate,
 * but this does not make a blocked or isolated target cell accessible.
 * Choose an ORIGINAL route-legal neighbor within three cells. The host
 * commits to the result for 24 ticks; no source geometry or collision is
 * modified and the bot still must SEE its enemy before firing. */
static int bot_tracker_reachable_waypoint(const AvpMpBotObservation *v,
                                          uint32_t *way_x,uint32_t *way_y)
{
    unsigned sx,sy,tx,ty;
    uint64_t best=UINT64_MAX;
    int best_x=-1,best_y=-1;
    AvpMpBotRoute path={0};
    if(!v||!way_x||!way_y)return 0;
    if(!nav_valid(&v->map))return 1; /* diagnostic legacy observation */
    sx=v->x>>16;sy=v->y>>16;
    tx=v->target_x>>16;ty=v->target_y>>16;
    if(sx>=v->map.width||sy>=v->map.height)return 0;
    if(tx<v->map.width && ty<v->map.height &&
       avp_mp_bot_route(&v->map,sx,sy,tx,ty,&path))return 1;
    for(int radius=1;radius<=3;radius++){
        for(int dy=-radius;dy<=radius;dy++)for(int dx=-radius;dx<=radius;dx++){
            const int cx=(int)tx+dx,cy=(int)ty+dy;
            uint64_t merit;
            if(abs(dx)!=radius && abs(dy)!=radius)continue;
            if(cx<0||cy<0||cx>=(int)v->map.width||cy>=(int)v->map.height)
                continue;
            if(!nav_cell(&v->map,(unsigned)cx,(unsigned)cy)[5])continue;
            if(v->map.prop_risk &&
               v->map.prop_risk[nav_index((unsigned)cx,(unsigned)cy)])continue;
            if(v->map.risk &&
               v->map.risk[nav_index((unsigned)cx,(unsigned)cy)]>=2u)continue;
            if(!avp_mp_bot_route(&v->map,sx,sy,
                 (unsigned)cx,(unsigned)cy,&path)||!path.valid ||
                 path.requires_door_use)continue;
            merit=(uint64_t)(dx*dx+dy*dy)*200u+(uint64_t)path.cost;
            if(merit<best){best=merit;best_x=cx;best_y=cy;}
        }
        if(best!=UINT64_MAX)break;
    }
    if(best_x<0)return 0;
    *way_x=((uint32_t)best_x<<16)|0x8000u;
    *way_y=((uint32_t)best_y<<16)|0x8000u;
    return 1;
}

/* Search only nearby authored floor cells reachable by normal player
 * controls. An observed Alien hidden by tables should make the bot move to
 * a source-clear firing lane, not stand in front of a solid prop forever.
 * This is never an omniscient enemy search: the caller supplies only an
 * already-known target coordinate, and an authoritative LOS/prop probe.
 * The host caches the resulting waypoint to avoid oscillating every tick. */
int avp_mp_bot_pick_firing_lane(const AvpMpBotMap *m,
    uint32_t x,uint32_t y,uint32_t tx,uint32_t ty,
    AvpMpBotFiringLaneProbe lane,void *ctx,
    unsigned *lane_x,unsigned *lane_y)
{
    unsigned sx=x>>16,sy=y>>16;
    uint64_t best=UINT64_MAX;
    unsigned bx=0u,by=0u;
    if(!lane||!lane_x||!lane_y||!nav_valid(m)||
       sx>=m->width||sy>=m->height)return 0;
    /* Prefer a modest lateral change before a seven-cell detour. A goal
     * inside an actual registered prop is never intentionally selected. */
    for(int radius=1;radius<=7;radius++){
        for(int dy=-radius;dy<=radius;dy++)for(int dx=-radius;dx<=radius;dx++){
            const int cx=(int)sx+dx,cy=(int)sy+dy;
            uint64_t metric;
            AvpMpBotRoute path={0};
            int64_t ex,ey;
            if(abs(dx)!=radius && abs(dy)!=radius)continue;
            if(cx<0||cy<0||cx>=(int)m->width||cy>=(int)m->height)continue;
            if(!nav_cell(m,(unsigned)cx,(unsigned)cy)[5])continue;
            if(m->prop_risk && m->prop_risk[nav_index((unsigned)cx,(unsigned)cy)])continue;
            if(m->risk && m->risk[nav_index((unsigned)cx,(unsigned)cy)]>=2u)continue;
            ex=((int64_t)cx<<16)+0x8000-(int64_t)tx;
            ey=((int64_t)cy<<16)+0x8000-(int64_t)ty;
            /* A Marine must not deliberately move onto the Alien's
             * original splash/contact square merely to get a shot. */
            if(llabs(ex)<(int64_t)(0.80*65536.0) &&
               llabs(ey)<(int64_t)(0.80*65536.0))continue;
            if(!lane(ctx,((uint32_t)cx<<16)|0x8000u,
                         ((uint32_t)cy<<16)|0x8000u,tx,ty))continue;
            /* Reject blocked original doors and inaccessible destinations;
             * the search never shortcuts through furniture or walls. */
            if(!avp_mp_bot_route(m,sx,sy,(unsigned)cx,(unsigned)cy,&path) ||
               !path.valid || path.requires_door_use)continue;
            metric=(uint64_t)path.cost*8u+
                   (uint64_t)(llabs(ex)+llabs(ey))/32768u;
            if(metric<best){best=metric;bx=(unsigned)cx;by=(unsigned)cy;}
        }
        /* Once a short route is found, resist choosing a distant but
         * cosmetically straighter route on the next update. */
        if(best!=UINT64_MAX && radius>=3)break;
    }
    if(best==UINT64_MAX)return 0;
    *lane_x=bx;*lane_y=by;
    return 1;
}
/* A Predator cannot pursue a teammate into any of the duct worlds
 * (levels 6..13). Do not hold onto a stale follow goal or keep pressing a
 * vent panel: drop this objective and select other work until the teammate
 * returns to a reachable area. If the teammate is in the same world but
 * behind an untraversable boundary, don't stand facing the wall either.
 * Real level-to-level connectivity is verified by walk_bot_choose_level_route.
 * No permanent blacklist: this decision is evaluated anew each bot tick. */
int avp_mp_bot_follow_target_possible(unsigned species,unsigned from_level,
        unsigned teammate_level,const AvpMpBotMap *map,
        unsigned from_x,unsigned from_y,unsigned teammate_x,unsigned teammate_y)
{
    AvpMpBotRoute route;
    if(species>2u || from_level<1u || from_level>15u ||
       teammate_level<1u || teammate_level>15u)return 0;
    if(from_level!=teammate_level)
        return avp_mp_bot_transition_allowed(species,from_level,
                             teammate_level,AVP_MP_BOT_TRANS_EXIT);
    if(nav_valid(map))
        return avp_mp_bot_route(map,from_x,from_y,
                                teammate_x,teammate_y,&route);
    return 1; /* synthetic/diagnostic view with no authenticated map */
}

int avp_mp_bot_lift_egress(const AvpMpBotMap *map,unsigned x,unsigned y,
                            unsigned *goal_x,unsigned *goal_y)
{
    /* The actual native elevator may span several connected cells.
     * A legal floor neighbor provides a real route away from the panel.
     * Never make the bot walk through an invented open wall. */
    if(!nav_valid(map)||!goal_x||!goal_y||x>=map->width||y>=map->height)
        return 0;
    for(unsigned side=0u;side<4u;side++){
        unsigned nx,ny,door,open;
        AvpMpBotRoute path={0};
        if(!nav_edge(map,x,y,side,&nx,&ny,&door,&open) ||
           !avp_mp_bot_route(map,x,y,nx,ny,&path))continue;
        *goal_x=nx;*goal_y=ny;return 1;
    }
    return 0;
}

/* The original AMPCollisions tests axis-aligned x/y halfwidth squares,
 * not circles. Read-only samples keep player movement authoritative; the
 * bot simply refuses directions leading into a registered solid AMP. */
static int bot_prop_hit(const AvpMpBotObservation *v,double x,double y)
{
    for(unsigned i=0;i<v->prop_count;i++){
        const AvpMpBotProp *p=&v->props[i];
        double px=(double)p->x/65536.0,py=(double)p->y/65536.0;
        double r=(double)p->halfwidth/65536.0;
        if(fabs(px-x)<r && fabs(py-y)<r)return 1;
    }
    return 0;
}
static int bot_prop_motion_clear(const AvpMpBotObservation *v,double ax,double ay,
                                  double bx,double by)
{
    int starting=bot_prop_hit(v,ax,ay);
    /* Test multiple points, but allow an actor already overlapping a
     * prop to escape instead of freezing inside a collision square. */
    for(unsigned i=1;i<=6u;i++){
        double x=ax+(bx-ax)*(double)i/6.0,y=ay+(by-ay)*(double)i/6.0;
        if(bot_prop_hit(v,x,y)){
            if(!starting)return 0;
            if(i==6u)return 0;
        }
    }
    return 1;
}
/* R478: conservatively test the swept rectangle against observed solid
 * squares. Preserve close/head-on negotiation and fail closed when the
 * host prop list is saturated or the nearest peer is not registered. */
static int bot_peer_precision_clear(const AvpMpBotObservation *v,
                                    double dx,double dy,double distance)
{
    if(!v->nearby_peer_valid || v->player_collision_disabled)return 1;
    if(!nav_valid(&v->map) || v->prop_count>=AVP_MP_BOT_MAX_NEAR_PROPS)return 0;
    const double x=v->x/65536.0,y=v->y/65536.0;
    const double ex=x+dx*distance,ey=y+dy*distance;
    const double px=v->nearby_peer_x/65536.0,py=v->nearby_peer_y/65536.0;
    const double peer_r=AVP_MP_PEER_CLEARANCE/65536.0;
    unsigned registered=0;
    if(hypot(px-x,py-y)<1.3 || (px-x)*dx+(py-y)*dy>0.0)return 0;
    for(unsigned i=0;i<v->prop_count;i++){
        const AvpMpBotProp *p=&v->props[i];
        const double cx=p->x/65536.0,cy=p->y/65536.0;
        double r=p->halfwidth/65536.0;
        if(p->x==v->nearby_peer_x && p->y==v->nearby_peer_y){
            registered=1;r=fmax(r,peer_r);
        }
        r+=0.016;
        if(cx>=fmin(x,ex)-r && cx<=fmax(x,ex)+r &&
           cy>=fmin(y,ey)-r && cy<=fmax(y,ey)+r)return 0;
    }
    return registered!=0;
}
/* Precise original acid contact square, sampled along a prospective command.
 * Whole maze tiles remain a coarse path COST, not the actual damage hitbox. */
static unsigned bot_acid_hits_margin(const AvpMpBotObservation *v,
                                     double ax,double ay,double bx,double by,
                                     double clearance)
{
    unsigned hits=0;
    for(unsigned j=0;j<=6u;j++){
        double x=ax+(bx-ax)*(double)j/6.0,y=ay+(by-ay)*(double)j/6.0;
        for(unsigned i=0;i<v->acid_count;i++){
            double px=(double)v->acid[i].x/65536.0;
            double py=(double)v->acid[i].y/65536.0;
            if(fabs(x-px)<=clearance &&
               fabs(y-py)<=clearance){++hits;break;}
        }
    }
    return hits;
}
static unsigned bot_acid_hits(const AvpMpBotObservation *v,double ax,double ay,
                              double bx,double by)
{
    return bot_acid_hits_margin(v,ax,ay,bx,by,BOT_ACID_CLEARANCE);
}
/* Original AMP.S amp_in_range(30) tests |dx|<=30<<9 AND |dy|<=30<<9.
 * Original PLAYER.S uses SafePos(80, ..., 40), whose wall margin is
 * 40<<9 on the same axes. A 1x1 cell is not a solid 1x1 actor footprint.
 * Distinguish the read-only *damage* footprint from the wall clearance.
 * The renderer's acid graphic is not the damage geometry. */
void avp_mp_bot_acid_risk_mark(uint8_t risk[AVP_MP_BOT_MAP_CELLS],
                                uint32_t px,uint32_t py)
{
    const int64_t r=(int64_t)(30u<<9);
    const int64_t x0=(int64_t)(int32_t)px-r,x1=(int64_t)(int32_t)px+r;
    const int64_t y0=(int64_t)(int32_t)py-r,y1=(int64_t)(int32_t)py+r;
    /* A contact square narrower than one cell can intersect no more than
     * two cells on either axis. No arbitrary 3x3 hazard inflation. */
    if(!risk || x1<0 || y1<0 || x0>=64LL*65536 || y0>=64LL*65536)return;
    for(unsigned cy=(unsigned)((y0<0?0:y0)>>16);
        cy<=(unsigned)((y1>=64LL*65536?64LL*65536-1:y1)>>16);cy++)
    for(unsigned cx=(unsigned)((x0<0?0:x0)>>16);
        cx<=(unsigned)((x1>=64LL*65536?64LL*65536-1:x1)>>16);cx++){
        const int64_t mx=((int64_t)cx<<16)+32768;
        const int64_t my=((int64_t)cy<<16)+32768;
        const unsigned strength=(llabs(mx-(int64_t)(int32_t)px)<=r &&
                                  llabs(my-(int64_t)(int32_t)py)<=r)?2u:1u;
        const unsigned id=nav_index(cx,cy);
        if(risk[id]<strength)risk[id]=(uint8_t)strength;
    }
}

/* Read-only local counterpart to the original SafePos width=40 wall test.
 * Check the entire proposed lane, not just the cell CENTER. An open area
 * can safely use the 0.18 / 0.82 fringe; a wall-bounded corridor cannot.
 * MovePlayer/SafePos remain fully authoritative on executed commands. */
/* Match the corner protrusion and partial-wall tests of original SafePos.
 * A traversable tile edge is not necessarily wide enough at its endpoint. */
static int bot_corner_solid(const AvpMpBotMap *m,unsigned x,unsigned y,
                            unsigned side,int from_left)
{
    const unsigned wall=nav_cell(m,x,y)[side],f=m->wall_flags[wall];
    return (f&(AVP_WB_SOLID|AVP_WB_2SIDES)) ||
        ((f&AVP_WB_LEFTONLY) && (from_left?!(wall&128u):(wall&128u)!=0));
}
static int bot_partial_wall_clear(const AvpMpBotMap *m,unsigned x,unsigned y,
                                  unsigned side,unsigned coord)
{
    const unsigned wall=nav_cell(m,x,y)[side],f=m->wall_flags[wall];
    unsigned pos=coord>>9;
    if(wall&128u)pos^=127u;
    if(!wall)return 1;
    if((f&AVP_WB_SOLID) || ((f&AVP_WB_LEFTONLY)&&pos<14u) ||
       ((f&AVP_WB_2SIDES)&&(pos<14u||pos>113u)))return 0;
    if(f&AVP_WB_DOOR){
        unsigned nx,ny,door,open;
        if(!nav_edge(m,x,y,side,&nx,&ny,&door,&open))return 0;
        if(f&AVP_WB_TBDOOR){
            if(f&AVP_WB_EDGEDOOR)return 80u<open;
            return 80>64-(int)open && 80<64+(int)open;
        }
        if(f&AVP_WB_EDGEDOOR)return (int)pos>144-(int)open;
        return (int)pos>80-(int)open && (int)pos<48+(int)open;
    }
    return 1;
}
static int bot_lane_native_wall_clear(const AvpMpBotMap *m,double x,double y)
{
    unsigned sx,sy;
    double fx,fy;
    if(!nav_valid(m))return 1;
    if(x<0.0||y<0.0||x>=(double)m->width||y>=(double)m->height)return 0;
    sx=(unsigned)x;sy=(unsigned)y;fx=x-(double)sx;fy=y-(double)sy;
    if(!nav_cell(m,sx,sy)[5])return 0;
    /* SafePos checks the adjacent cell's wall near each inside corner,
     * even when both edges of this tile are nominally traversable. */
    const unsigned ux=(unsigned)(fx*65536.0),uy=(unsigned)(fy*65536.0);
    const int l=ux<20480u,r=ux>45055u,t=uy<20480u,b=uy>45055u;
    const unsigned above=sy?sy-1u:sy,below=sy+1u<m->height?sy+1u:sy;
    if(l && ((t&&bot_corner_solid(m,sx,above,0,1)) ||
             (b&&bot_corner_solid(m,sx,below,0,0)) ||
             !bot_partial_wall_clear(m,sx,sy,0,65535u-uy)))return 0;
    if(t && ((l&&sx&&bot_corner_solid(m,sx-1u,sy,1,0)) ||
             (r&&sx+1u<m->width&&bot_corner_solid(m,sx+1u,sy,1,1)) ||
             !bot_partial_wall_clear(m,sx,sy,1,ux)))return 0;
    if(r && ((t&&bot_corner_solid(m,sx,above,2,0)) ||
             (b&&bot_corner_solid(m,sx,below,2,1)) ||
             !bot_partial_wall_clear(m,sx,sy,2,uy)))return 0;
    if(b && ((l&&sx&&bot_corner_solid(m,sx-1u,sy,3,1)) ||
             (r&&sx+1u<m->width&&bot_corner_solid(m,sx+1u,sy,3,0)) ||
             !bot_partial_wall_clear(m,sx,sy,3,65535u-ux)))return 0;

    return 1;
}
static int bot_lane_wall_clear(const AvpMpBotMap *m,double x,double y)
{
    unsigned sx,sy,nx,ny,door,open;
    double fx,fy;
    if(!nav_valid(m))return 1;
    if(!bot_lane_native_wall_clear(m,x,y))return 0;
    sx=(unsigned)x;sy=(unsigned)y;fx=x-sx;fy=y-sy;
    if(fx<BOT_PLAYER_WALL_MARGIN &&
       (!nav_edge(m,sx,sy,0u,&nx,&ny,&door,&open)||(door&&open<40u)))return 0;
    if(fx>1.0-BOT_PLAYER_WALL_MARGIN &&
       (!nav_edge(m,sx,sy,2u,&nx,&ny,&door,&open)||(door&&open<40u)))return 0;
    if(fy<BOT_PLAYER_WALL_MARGIN &&
       (!nav_edge(m,sx,sy,1u,&nx,&ny,&door,&open)||(door&&open<40u)))return 0;
    if(fy>1.0-BOT_PLAYER_WALL_MARGIN &&
       (!nav_edge(m,sx,sy,3u,&nx,&ny,&door,&open)||(door&&open<40u)))return 0;
    return 1;
}
/* R293: a source actor can already be inside the *conservative bot*
 * wall-margin band (e.g. y=2.197 in a one-cell hallway). The old probe
 * rejected ALL four original movement inputs until the player was back
 * within the band -- a state it could never reach. Only authorize a short
 * command that decreases the existing wall-margin intrusion, never one
 * that creates a new intrusion or crosses a closed native map edge.
 * Original SafePos still owns the actual collision decision. */
static int bot_wall_margin_recovery(const AvpMpBotMap *m,
    double ax,double ay,double bx,double by)
{
    const double a[2]={ax,ay},b[2]={bx,by};
    double total_improved=0.0;
    if(!nav_valid(m) || ax<0.0 || ay<0.0 || bx<0.0 || by<0.0 ||
       ax>=(double)m->width || ay>=(double)m->height ||
       bx>=(double)m->width || by>=(double)m->height ||
       (unsigned)ax!=(unsigned)bx || (unsigned)ay!=(unsigned)by)return 0;
    /* Recovery may relax the conservative planner margin, never the
     * native corner/partial-wall check. */
    if(!bot_lane_native_wall_clear(m,bx,by))return 0;
    for(unsigned axis=0u;axis<2u;axis++){
        const unsigned sx=(unsigned)ax,sy=(unsigned)ay;
        const double fa=a[axis]-floor(a[axis]);
        const double fb=b[axis]-floor(b[axis]);
        for(unsigned side=0u;side<2u;side++){
            const unsigned direction=axis==0u?(side?2u:0u):
                                                  (side?3u:1u);
            unsigned nx,ny,door,open;
            double before,after;
            if(nav_edge(m,sx,sy,direction,&nx,&ny,&door,&open) &&
               (!door || open>=40u))continue;
            before=side?(fa-(1.0-BOT_PLAYER_WALL_MARGIN)):
                             (BOT_PLAYER_WALL_MARGIN-fa);
            after=side?(fb-(1.0-BOT_PLAYER_WALL_MARGIN)):
                            (BOT_PLAYER_WALL_MARGIN-fb);
            if(before<0.0)before=0.0;
            if(after<0.0)after=0.0;
            if(after>before+0.0001)return 0;
            total_improved+=before-after;
        }
    }
    /* A sideways recovery must have a meaningful component away from the
     * nearby wall; the previous forward wall-push cannot masquerade as
     * recovery because of a tiny heading-induced vertical component. */
    return total_improved>0.030;
}
/* Short motion can end on a legal tile but cross an inside corner before
 * its endpoint. Test the intermediate positions, including each crossed
 * tile boundary, before allowing the precision controller to follow it. */
static int bot_short_lane_clear(const AvpMpBotMap *m,double ax,double ay,double bx,double by)
{
    const int from_clear=bot_lane_wall_clear(m,ax,ay);
    if(!bot_lane_wall_clear(m,bx,by) &&
       !bot_wall_margin_recovery(m,ax,ay,bx,by))return 0;
    for(unsigned i=1;i<=16u;i++){
        const double t=(double)i/16.0,x=ax+(bx-ax)*t,y=ay+(by-ay)*t;
        if(!bot_lane_native_wall_clear(m,x,y) ||
           (from_clear && !bot_lane_wall_clear(m,x,y)))return 0;
    }
    for(unsigned axis=0;axis<2u;axis++){
        const double a=axis?ay:ax,b=axis?by:bx;
        if(floor(a)==floor(b))continue;
        const double edge=b>a?floor(b):floor(a);
        const double t=(edge-a)/(b-a);
        /* Check both sides of the edge, not just the cell chosen by floor. */
        for(int sign=-1;sign<=1;sign+=2){
            const double u=fmax(0.0,fmin(1.0,t+sign*0.0001));
            if(!bot_lane_wall_clear(m,ax+(bx-ax)*u,ay+(by-ay)*u))return 0;
        }
    }
    return 1;
}
static int bot_lane_segment_clear(const AvpMpBotObservation *v,
                                  double ax,double ay,double bx,double by)
{
    for(unsigned n=0;n<=12u;n++){
        const double t=(double)n/12.0;
        const double x=ax+(bx-ax)*t,y=ay+(by-ay)*t;
        if(!bot_lane_wall_clear(&v->map,x,y) || bot_prop_hit(v,x,y))return 0;
    }
    return 1;
}
/* Plan a TWO-LEG local line: shift into a safe lane in the current cell,
 * then advance to the next route cell. Search precise damage boundaries in
 * addition to old 0.18/0.82 waypoints. This uses original actor centers,
 * collision margin, source acid contact and real wall/prop geometry. */
static unsigned bot_best_acid_lane(const AvpMpBotObservation *v,
                                   const AvpMpBotRoute *route,
                                   double *selected)
{
    const double ax=(double)(int32_t)v->x/65536.0;
    const double ay=(double)(int32_t)v->y/65536.0;
    const int along_x=route->next_x!=(v->x>>16);
    const double longitudinal=along_x?(double)route->next_x+0.5:
                                      (double)route->next_y+0.5;
    const double current=along_x?ay:ax;
    const double floorcell=floor(current);
    double best_lane=current;
    unsigned best_hits=~0u;
    double best_cost=1e30;
    /* Local candidates start with center/current lane, then approach both
     * sides of EACH relevant exact acid square. Only candidates on this
     * floor cell and physically reachable at original player width count. */
    for(unsigned n=0;n<4u+v->acid_count*2u;n++){
        double lane;
        double bx,by,cx,cy;
        unsigned hits;
        double cost;
        if(n==0u)lane=current;
        else if(n==1u)lane=floorcell+0.18;
        else if(n==2u)lane=floorcell+0.82;
        else if(n==3u)lane=floorcell+0.50;
        else{
            const unsigned id=(n-4u)/2u;
            const double px=(double)(int32_t)v->acid[id].x/65536.0;
            const double py=(double)(int32_t)v->acid[id].y/65536.0;
            const double near=along_x?px:py;
            if(fabs(near-longitudinal)>1.70 &&
               fabs(near-(along_x?ax:ay))>1.70)continue;
            lane=(along_x?py:px)+((n&1u)?1.0:-1.0)*
                    (BOT_ACID_CLEARANCE+BOT_LANE_MARGIN);
        }
        if(lane<floorcell+0.035 || lane>floorcell+0.965)continue;
        bx=along_x?ax:lane;by=along_x?lane:ay;
        cx=along_x?longitudinal:lane;
        cy=along_x?lane:longitudinal;
        if(!bot_lane_segment_clear(v,ax,ay,bx,by) ||
           !bot_lane_segment_clear(v,bx,by,cx,cy))continue;
        hits=bot_acid_hits(v,ax,ay,bx,by)+bot_acid_hits(v,bx,by,cx,cy);
        cost=fabs(lane-current);
        if(hits<best_hits || (hits==best_hits && cost<best_cost)){
            best_hits=hits;best_cost=cost;best_lane=lane;
        }
    }
    *selected=best_lane;
    return best_hits;
}
/* If both sides of the route's next cardinal step have no real floor
 * exit, there is no alternate tile to bypass a centered acid square. A
 * single-cell corridor may have several contiguous acid contacts. Neither
 * strafing toward a blocked fringe nor backing away from another bot can
 * create a safe passage there. This only controls bot input decisions;
 * original walls, damage and collision remain authoritative. */
static int bot_route_acid_choke(const AvpMpBotObservation *v,
                                const AvpMpBotRoute *route)
{
    const AvpMpBotMap *m=&v->map;
    const unsigned sx=v->x>>16,sy=v->y>>16;
    unsigned sides[2],nx,ny,door,open;
    double ax,ay,bx,by,lane;
    if(!route || !route->valid || !v->acid_count ||
       v->player_species==1u || !nav_valid(m) ||
       sx>=m->width || sy>=m->height ||
       route->next_x>=m->width || route->next_y>=m->height ||
       route->requires_door_use || !route->door_is_open)return 0;
    if((route->next_x==sx)==(route->next_y==sy))return 0;
    if(abs((int)route->next_x-(int)sx)+
       abs((int)route->next_y-(int)sy)!=1)return 0;
    ax=(double)(int32_t)v->x/65536.0;
    ay=(double)(int32_t)v->y/65536.0;
    bx=(double)route->next_x+0.5;
    by=(double)route->next_y+0.5;
    if(!bot_acid_hits(v,ax,ay,bx,by))return 0;
    /* A physically legal ZERO-contact lane must still be used, even in a
     * single-width hallway with an off-center puddle. */
    if(bot_best_acid_lane(v,route,&lane)==0u)return 0;
    sides[0]=(route->next_x!=sx)?1u:0u;
    sides[1]=(route->next_x!=sx)?3u:2u;
    for(unsigned i=0;i<2u;i++){
        /* A detour only exists if this direction can reach a neighboring
         * floor from the current OR next tile. Neither is true for a
         * one-cell vent or the corridor in the reported screenshot. */
        if(nav_edge(m,sx,sy,sides[i],&nx,&ny,&door,&open) &&
           (!door || open>=40u))return 0;
        if(nav_edge(m,route->next_x,route->next_y,sides[i],
                    &nx,&ny,&door,&open) && (!door || open>=40u))return 0;
    }
    return 1;
}
/* The planner's acid is a COST, not a physical obstacle. Commit through
 * unavoidable contact. A reachable lane with less exposure takes priority
 * over forward-tanking even when the route uses an acid-center grid cell. */
static int bot_route_acid_crossing(const AvpMpBotObservation *v,
                                    const AvpMpBotRoute *route)
{
    const AvpMpBotMap *m=&v->map;
    const unsigned sx=v->x>>16,sy=v->y>>16;
    double lane;
    unsigned best,current;
    if(!route || !route->valid || v->player_species==1u ||
       !nav_valid(m) || !v->acid_count ||
       sx>=m->width || sy>=m->height ||
       route->next_x>=m->width || route->next_y>=m->height)return 1;
    if(route->next_x!=sx && route->next_y!=sy)return 1;
    if(bot_route_acid_choke(v,route))return 1;
    best=bot_best_acid_lane(v,route,&lane);
    current=bot_acid_hits(v,(double)(int32_t)v->x/65536.0,
         (double)(int32_t)v->y/65536.0,
         (double)route->next_x+0.5,(double)route->next_y+0.5);
    return best>=current;
}
static void bot_acid_waypoint(const AvpMpBotObservation *v,
                              const AvpMpBotRoute *route,double *wx,double *wy)
{
    const double ax=(double)(int32_t)v->x/65536.0;
    const double ay=(double)(int32_t)v->y/65536.0;
    const int along_x=route->next_x!=(v->x>>16);
    double lane;
    if(v->player_species==1u || !v->acid_count ||
       (route->next_x!=(v->x>>16) && route->next_y!=(v->y>>16)))return;
    if(bot_best_acid_lane(v,route,&lane)==~0u)return;
    /* Test the current lateral position against the *whole forward leg*.
     * The old 0.055 waypoint tolerance prematurely accepted a lane that
     * was still inside the original 30<<9 contact square; conversely a
     * fixed tight tolerance would make 0.07-tile player steps bounce
     * left/right around the exact candidate lane. Once current forward
     * motion is genuinely contact-free, advance without recentering. */
    /* R288: the extra tactical safety margin is NOT the original acid
     * damage square. If the route's next full leg is outside the original
     * 30<<9 contact square, follow it even when the larger preference
     * margin suggests an unreachable lateral correction. A full original
     * sidestep can jump across a very narrow remaining gap and hit the
     * adjacent wall; that used to freeze the bot for 20+ ticks. */
    if(!bot_acid_hits_margin(v,ax,ay,
          along_x?(double)route->next_x+0.5:ax,
          along_x?ay:(double)route->next_y+0.5,
          BOT_ACID_CONTACT)){
        *wx=along_x?(double)route->next_x+0.5:ax;
        *wy=along_x?ay:(double)route->next_y+0.5;
    }else if(fabs(lane-(along_x?ay:ax))>0.012){
        *wx=along_x?ax:lane;
        *wy=along_x?lane:ay;
    }else{
        *wx=along_x?(double)route->next_x+0.5:lane;
        *wy=along_x?lane:(double)route->next_y+0.5;
    }
}
/* If a prop blocks the central approach, steer to the free side of the
 * same cell before continuing. Choose stable side based on current lane;
 * this prevents reversing left/right every tick around a table/barrel. */
static void bot_prop_waypoint(const AvpMpBotObservation *v,
                              const AvpMpBotRoute *route,double *wx,double *wy)
{
    const double ax=(double)v->x/65536.0,ay=(double)v->y/65536.0;
    const int along_x=route->next_x!=(v->x>>16);
    const int advance=along_x ? (route->next_x>(v->x>>16)?1:-1):
                                (route->next_y>(v->y>>16)?1:-1);
    if(route->next_x!=(v->x>>16) && route->next_y!=(v->y>>16))return;
    for(unsigned i=0;i<v->prop_count;i++){
        const AvpMpBotProp *p=&v->props[i];
        double px=(double)p->x/65536.0,py=(double)p->y/65536.0;
        double pr=(double)p->halfwidth/65536.0;
        double ahead=along_x?advance*(px-ax):advance*(py-ay);
        double here=along_x?ay:ax,side=along_x?py:px;
        double candidates[2],best=1e30,selected=here;
        double cell=(double)(int)floor(side);
        if(ahead< -0.12 || ahead>1.2 || fabs(side-here)>pr+0.05)continue;
        candidates[0]=side-pr-0.03;
        candidates[1]=side+pr+0.03;
        for(unsigned j=0;j<2u;j++){
            double lane=candidates[j];
            double sx=along_x?ax:lane,sy=along_x?lane:ay;
            double score;
            /* Keep the candidate on the current floor cell. We do not
             * assume wall/actor widths; original MovePlayer enforces both. */
            if(lane<cell+0.08 || lane>cell+0.92)continue;
            /* A lane may lie inside the tile yet intersect a solid wall.
             * Check the ORIGINAL 40<<9 clearance along the actual shift;
             * don't aim at an impossible fringe beside a wide table. */
            if(!bot_lane_segment_clear(v,ax,ay,sx,sy))continue;
            score=fabs(lane-here);
            score+=80.0*(double)bot_acid_hits(v,ax,ay,sx,sy);
            if(score<best){best=score;selected=lane;}
        }
        if(best==1e30)continue;
        if(fabs(selected-here)>0.04){
            *wx=along_x?ax:selected;
            *wy=along_x?selected:ay;
        }else{
            *wx=along_x?(double)route->next_x+0.5:selected;
            *wy=along_x?selected:(double)route->next_y+0.5;
        }
        return;
    }
}
static int bot_props_close(const AvpMpBotObservation *v)
{
    const double x=(double)(int32_t)v->x/65536.0;
    const double y=(double)(int32_t)v->y/65536.0;
    for(unsigned i=0;i<v->prop_count;i++){
        const AvpMpBotProp *p=&v->props[i];
        const double margin=(double)p->halfwidth/65536.0+1.10;
        if(fabs((double)p->x/65536.0-x)<margin &&
           fabs((double)p->y/65536.0-y)<margin)return 1;
    }
    return 0;
}
static int bot_hazards_close(const AvpMpBotObservation *v)
{
    const double x=(double)(int32_t)v->x/65536.0;
    const double y=(double)(int32_t)v->y/65536.0;
    for(unsigned i=0;i<v->acid_count;i++)
        if(fabs((double)v->acid[i].x/65536.0-x)<1.25 &&
           fabs((double)v->acid[i].y/65536.0-y)<1.25)return 1;
    return bot_props_close(v);
}
/* Test a short potential movement against original wall/door faces before
 * side-stepping or retreating. This is a COMMAND filter only: real player
 * collision, actor width and acid contact remain the Jaguar engine's job. */
static int bot_step_clear_probe(const AvpMpBotObservation *v,double dx,double dy,
                          const AvpMpBotRoute *route,double probe)
{
    unsigned sx,sy,gx,gy,door,open,tx,ty;
    double x,y;
    const AvpMpBotMap *m=&v->map;
    if(!nav_valid(m))return 1; /* diagnostic map has no retail geometry */
    sx=v->x>>16;sy=v->y>>16;
    if(sx>=m->width||sy>=m->height)return 0;
    /* R288: the long 0.68-cell speculative probe blocked real, legal
     * movements BEFORE acid entered the old 1.25-cell proximity window.
     * E.g. approaching a puddle 1.30 cells away in a one-cell vent,
     * a necessary sideways motion from y=.34 was falsely tested at
     * y=1.02 and discarded as a collision. Its original MovePlayer tick
     * is much shorter than that prediction. Where a real acid hazard is
     * on/near the chosen route, probe a nearby 0.12-cell increment and
     * recheck original geometry on the next tick. A route remains only
     * an input suggestion: source SafePos/AMPCollisions are authoritative.
     * This also applies to independent defensive moves near the puddle. */
    if(v->acid_count){
        const double ax=(double)(int32_t)v->x/65536.0;
        const double ay=(double)(int32_t)v->y/65536.0;
        if(route && route->valid && probe>0.12)probe=0.12;
        else for(unsigned i=0u;i<v->acid_count;i++){
            if(fabs((double)(int32_t)v->acid[i].x/65536.0-ax)<2.0 &&
               fabs((double)(int32_t)v->acid[i].y/65536.0-ay)<2.0){
                if(probe>0.12)probe=0.12;
                break;
            }
        }
    }
    if(bot_hazards_close(v) && route && probe>0.12)probe=0.12;
    x=(double)v->x/65536.0+dx*probe;
    y=(double)v->y/65536.0+dy*probe;
    if(x<0.0||y<0.0||x>=(double)m->width||y>=(double)m->height)return 0;
    /* In a one-cell corridor the actor's original SafePos width forbids
     * sidestepping to the 0.78 fringe. Selecting such a direction to leave
     * acid only causes perpetual wall collisions and spinning. */
    if(v->acid_count){
        /* Digital movement retains its original one-tick wall probe.
         * Precise lane commands separately check their chosen short endpoint. */
        const double ax=(double)v->x/65536.0,ay=(double)v->y/65536.0;
        const double step=fmin(probe,0.07),bx=ax+dx*step,by=ay+dy*step;
        if(!bot_lane_wall_clear(m,bx,by) &&
           !bot_wall_margin_recovery(m,ax,ay,bx,by))return 0;
    }
    if(v->prop_count && !bot_prop_motion_clear(v,(double)v->x/65536.0,
             (double)v->y/65536.0,x,y))return 0;
    gx=(unsigned)x;gy=(unsigned)y;
    /* Never stray from the chosen path. With a genuine diagonal hop,
     * physical movement may enter one of its two side cells FIRST before
     * crossing the other axis. Accept those ONLY when the entire square is
     * verified open, then let the original MovePlayer/SafePos decide. */
    if(route && route->valid && (gx!=sx || gy!=sy) &&
       (gx!=route->next_x || gy!=route->next_y)){
        const int rx=(int)route->next_x-(int)sx;
        const int ry=(int)route->next_y-(int)sy;
        if((rx!=1 && rx!=-1)||(ry!=1 && ry!=-1) ||
           !nav_diagonal(m,sx,sy,rx,ry,v->player_species==1u) ||
           !((gx==route->next_x && gy==sy) ||
             (gx==sx && gy==route->next_y)))return 0;
    }
    /* The route planner already prices acid according to the entire map.
     * Once a safe/best-available route has been chosen, do not veto its
     * actual commands merely because a puddle's contact box spills into
     * an adjacent cell. That disagreement left bots standing in acid, or
     * hesitating outside it, forever.  Walls, doors and solid props remain
     * checked above and below, and native MovePlayer still applies damage. */
    if(v->player_species!=1u && !(route && route->valid)){
        if(v->acid_count){
            unsigned before=bot_acid_hits(v,(double)v->x/65536.0,
                                           (double)v->y/65536.0,
                                           (double)v->x/65536.0,
                                           (double)v->y/65536.0);
            unsigned exposure=bot_acid_hits(v,(double)v->x/65536.0,
                                             (double)v->y/65536.0,x,y);
            if(exposure>before)return 0;
        }else if(m->risk && m->risk[nav_index(gx,gy)]>=2u &&
                 m->risk[nav_index(sx,sy)]<2u)return 0;
    }
    if(gx!=sx){
        if(!nav_edge(m,sx,sy,gx<sx?0u:2u,&tx,&ty,&door,&open) ||
           (door&&open<40u))return 0;
    }
    if(gy!=sy){
        if(!nav_edge(m,sx,sy,gy<sy?1u:3u,&tx,&ty,&door,&open) ||
           (door&&open<40u))return 0;
    }
    /* Diagonal corner traversals are rejected unless both adjacent
     * cells also expose the opposite half of the crossing. */
    if(gx!=sx && gy!=sy){
        if(!nav_edge(m,gx,sy,gy<sy?1u:3u,&tx,&ty,&door,&open) ||
           (door&&open<40u))return 0;
        if(!nav_edge(m,sx,gy,gx<sx?0u:2u,&tx,&ty,&door,&open) ||
           (door&&open<40u))return 0;
    }
    return 1;
}
static int bot_step_clear(const AvpMpBotObservation *v,double dx,double dy,
                          const AvpMpBotRoute *route)
{
    return bot_step_clear_probe(v,dx,dy,route,0.68);
}
/* Aim slightly beyond the immediate source-legal next node when TWO
 * consecutive cardinal steps form a safe open lane. Diagonal hops already
 * have a direct heading, so this advance bend helper leaves them alone.
 * Do not shortcut doors, acid lanes, prop avoidance or a closed corner. */
static void bot_route_steer_point(const AvpMpBotObservation *v,
                                  const AvpMpBotRoute *route,
                                  double *wx,double *wy)
{
    const AvpMpBotMap *m=&v->map;
    unsigned sx=v->x>>16,sy=v->y>>16,nx,ny,door,open;
    int step1x,step1y,step2x,step2y;
    double candidate_x,candidate_y;
    const double ax=(double)(int32_t)v->x/65536.0;
    const double ay=(double)(int32_t)v->y/65536.0;
    *wx=(double)route->next_x+0.5;
    *wy=(double)route->next_y+0.5;
    if((route->next_x!=sx && route->next_y!=sy) ||
       !route->lookahead_valid || route->requires_door_use ||
       !route->door_is_open || bot_hazards_close(v) || !nav_valid(m))return;
    step1x=(int)route->next_x-(int)sx;
    step1y=(int)route->next_y-(int)sy;
    step2x=(int)route->lookahead_x-(int)route->next_x;
    step2y=(int)route->lookahead_y-(int)route->next_y;
    if(abs(step1x)+abs(step1y)!=1 ||
       abs(step2x)+abs(step2y)!=1)return;
    if(m->risk &&
       (m->risk[nav_index(route->next_x,route->next_y)] ||
        m->risk[nav_index(route->lookahead_x,route->lookahead_y)]))return;
    if(m->prop_risk &&
       (m->prop_risk[nav_index(route->next_x,route->next_y)] ||
        m->prop_risk[nav_index(route->lookahead_x,route->lookahead_y)]))return;
    /* Verify the next A* edge as an already-open original passage. */
    nx=ny=UINT32_MAX;
    for(unsigned side=0;side<4u;side++){
        if(!nav_edge(m,route->next_x,route->next_y,side,
                     &nx,&ny,&door,&open))continue;
        if(nx==route->lookahead_x && ny==route->lookahead_y)break;
    }
    if(nx!=route->lookahead_x || ny!=route->lookahead_y ||
       (door && open<40u))return;
    if(step1x!=step2x || step1y!=step2y){
        /* A bend can be rounded only when BOTH sides of the open 2x2
         * corner are traversable. Otherwise face the first node normally. */
        unsigned cx,cy,dir;
        if(step2x==0 && step2y==0)return;
        dir=step2x<0?0u:step2x>0?2u:step2y<0?1u:3u;
        if(!nav_edge(m,sx,sy,dir,&cx,&cy,&door,&open) ||
           (door && open<40u))return;
        if(m->risk && m->risk[nav_index(cx,cy)])return;
        if(m->prop_risk && m->prop_risk[nav_index(cx,cy)])return;
        dir=step1x<0?0u:step1x>0?2u:step1y<0?1u:3u;
        if(!nav_edge(m,cx,cy,dir,&nx,&ny,&door,&open) ||
           nx!=route->lookahead_x || ny!=route->lookahead_y ||
           (door && open<40u))return;
    }
    /* Keep the aim near the first path cell, not at a remote goal. The
     * line test checks the original wall margin and solid prop bounds. */
    candidate_x=*wx+0.45*(step2x);
    candidate_y=*wy+0.45*(step2y);
    if(!bot_lane_segment_clear(v,ax,ay,candidate_x,candidate_y))return;
    *wx=candidate_x;*wy=candidate_y;
}

/* A stable open corridor is a DIRECTION, not a succession of cell-center
 * targets. A centered bot should hold its heading instead of alternating
 * left and right as the next waypoint passes from one side of its nose to
 * the other. Route motion can still strafe toward the lane center, and real
 * bends/obstacles keep the original precise waypoint steering. */
static int bot_straight_route_heading(const AvpMpBotObservation *v,
                                      const AvpMpBotRoute *route,
                                      double *dx,double *dy)
{
    const AvpMpBotMap *m=&v->map;
    const unsigned sx=v->x>>16,sy=v->y>>16;
    const int step_x=(int)route->next_x-(int)sx;
    const int step_y=(int)route->next_y-(int)sy;
    const int look_x=(int)route->lookahead_x-(int)route->next_x;
    const int look_y=(int)route->lookahead_y-(int)route->next_y;
    const double px=(double)(int32_t)v->x/65536.0;
    const double py=(double)(int32_t)v->y/65536.0;
    double lateral;
    if(!route->valid || route->requires_door_use || !route->door_is_open ||
       bot_hazards_close(v) || !nav_valid(m) ||
       sx>=m->width || sy>=m->height ||
       route->next_x>=m->width || route->next_y>=m->height ||
       (step_x==0 && step_y==0) ||
       abs(step_x)>1 || abs(step_y)>1)return 0;
    /* Don't lock the heading across a bend. When a straight segment ends
     * at the destination, preserve its stable course through the last cell. */
    if(route->lookahead_valid &&
       (route->lookahead_x>=m->width || route->lookahead_y>=m->height ||
        step_x!=look_x || step_y!=look_y))return 0;
    if(m->risk &&
       (m->risk[nav_index(route->next_x,route->next_y)] ||
        (route->lookahead_valid &&
         m->risk[nav_index(route->lookahead_x,route->lookahead_y)])))return 0;
    if(m->prop_risk &&
       (m->prop_risk[nav_index(route->next_x,route->next_y)] ||
        (route->lookahead_valid &&
         m->prop_risk[nav_index(route->lookahead_x,route->lookahead_y)])))return 0;
    /* Sideways displacement from the segment's centerline, in map tiles.
     * At larger offsets, aim toward the actual waypoint to recenter. */
    lateral=fabs((px-((double)sx+0.5))*(double)step_y-
                 (py-((double)sy+0.5))*(double)step_x);
    if(step_x && step_y)lateral*=0.70710678118654752440;
    if(lateral>0.28)return 0;
    *dx=(double)step_x;
    *dy=(double)step_y;
    return 1;
}

/* Heading zero faces +X. Left/right are the original host sidestep axes,
 * NOT a fabricated world displacement or a new speed multiplier. */
static int bot_safe_action(const AvpMpBotObservation *v,unsigned action,
                           const AvpMpBotRoute *route)
{
    double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    double fx=cos(a),fy=-sin(a),dx=0,dy=0;
    switch(action){
    case AVP_MP_FORWARD:dx=fx;dy=fy;break;
    case AVP_MP_BACKWARD:dx=-fx;dy=-fy;break;
    case AVP_MP_STRAFE_LEFT:dx=fy;dy=-fx;break;
    case AVP_MP_STRAFE_RIGHT:dx=-fy;dy=fx;break;
    default:return 0;
    }
    return bot_step_clear(v,dx,dy,route);
}
/* Near a lingering acid puddle the tactical layer may choose a backward
 * step on one tick and the opposite forward step on the next. That always
 * counts as movement, so the collision-stall detector never fires and the
 * bot paces indefinitely. Keep one collision-legal lane direction until
 * it has crossed the current tile, the goal changes, or actual trouble
 * demands a different motion. No teleport, new damage rule, or hidden
 * movement vector: this only chooses among the four original key inputs. */
static void bot_acid_lane_hysteresis(AvpMpBotState *state,
    const AvpMpBotObservation *v,unsigned goal_x,unsigned goal_y,
    int urgent,unsigned *actions)
{
    const unsigned mask=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
    const unsigned move=*actions&mask;
    const unsigned tile=((v->y>>16)<<8)|(v->x>>16);
    const unsigned goal=(goal_y<<8)|goal_x;
    unsigned old=state->acid_lane_motion;
    unsigned opposite=old==(1u<<AVP_MP_FORWARD)?(1u<<AVP_MP_BACKWARD):
        old==(1u<<AVP_MP_BACKWARD)?(1u<<AVP_MP_FORWARD):
        old==(1u<<AVP_MP_STRAFE_LEFT)?(1u<<AVP_MP_STRAFE_RIGHT):
        old==(1u<<AVP_MP_STRAFE_RIGHT)?(1u<<AVP_MP_STRAFE_LEFT):0u;
    if(v->player_species==1u || !v->acid_count || !bot_hazards_close(v) ||
       urgent || !move || (move&(move-1u)) ||
       bot_acid_hits(v,(double)v->x/65536.0,(double)v->y/65536.0,
                        (double)v->x/65536.0,(double)v->y/65536.0)){
        /* Actual acid contact is handled by bot_escape_current_acid.
         * Never interfere with its emergency exit direction. */
        state->acid_lane_motion=0u;return;
    }
    if(tile!=state->acid_lane_cell){
        /* Changing a tracker/supply goal inside the SAME physical acid
         * bottleneck is not movement progress. Retain the collision-legal
         * direction until the bot actually reaches another cell. */
        state->acid_lane_motion=0u;old=0u;
    }
    if(old && move==opposite && v->tick<state->acid_lane_until &&
       !state->immobile_ticks){
        unsigned act=0u;
        double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
        const double fx=cos(a),fy=-sin(a),x=(double)v->x/65536.0,
                     y=(double)v->y/65536.0;
        double odx=0.0,ody=0.0,ndx=0.0,ndy=0.0;
        if(old==(1u<<AVP_MP_FORWARD)){act=AVP_MP_FORWARD;odx=fx;ody=fy;}
        else if(old==(1u<<AVP_MP_BACKWARD)){act=AVP_MP_BACKWARD;odx=-fx;ody=-fy;}
        else if(old==(1u<<AVP_MP_STRAFE_LEFT)){act=AVP_MP_STRAFE_LEFT;odx=fy;ody=-fx;}
        else if(old==(1u<<AVP_MP_STRAFE_RIGHT)){act=AVP_MP_STRAFE_RIGHT;odx=-fy;ody=fx;}
        ndx=-odx;ndy=-ody;
        /* If reversing really does leave danger sooner, permit it. Also
         * permit reversal when the previous direction hit a solid wall. */
        if(bot_safe_action(v,act,NULL) &&
           bot_acid_hits(v,x,y,x+odx*0.28,y+ody*0.28)<=
           bot_acid_hits(v,x,y,x+ndx*0.28,y+ndy*0.28)){
            *actions=(*actions&~mask)|old;
            return;
        }
    }
    if(move!=old || v->tick>=state->acid_lane_until)
        state->acid_lane_until=v->tick+60u;
    state->acid_lane_motion=move;
    state->acid_lane_cell=tile;state->acid_lane_goal=goal;
}
/* R299: At an off-centre acid puddle next to a T/cross junction, a bot
 * can reach the edge of an authored turn while still outside the ORIGINAL
 * SafePos clearance of the *destination* cell.  E.g. x=3.249,y=2.016,
 * next (3,1), whose west edge is a wall: forward crosses at x<3.3125;
 * all goal-positive motions fail bot_safe_action.  The only legal step is
 * temporarily SIDEWAYS AND SLIGHTLY BACKWARD, which the greedy route dot
 * score discarded.  Recenter laterally on the *next* authored tile before
 * crossing.  This is an input choice, never a movement or wall override.
 * It is restricted to nearby acid and an actually OPEN cardinal route. */
static unsigned bot_acid_junction_recenter(const AvpMpBotObservation *v,
                                            const AvpMpBotRoute *route)
{
    static const unsigned keys[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                                   AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
    const AvpMpBotMap *m=&v->map;
    const unsigned sx=v->x>>16,sy=v->y>>16;
    unsigned nx,ny,door,open;
    unsigned direction;
    double x,y,near,low,high,delta,a,fx,fy,best=0.04;
    unsigned choice=AVP_MP_INPUT_ACTIONS;
    int along_x;
    if(v->player_species==AVP_MP_ALIEN || !v->acid_count ||
       !bot_hazards_close(v) || !route || !route->valid ||
       route->requires_door_use || !nav_valid(m) ||
       sx>=m->width || sy>=m->height ||
       route->next_x>=m->width || route->next_y>=m->height ||
       (abs((int)route->next_x-(int)sx)+
        abs((int)route->next_y-(int)sy))!=1)return choice;
    x=(double)(int32_t)v->x/65536.0;
    y=(double)(int32_t)v->y/65536.0;
    along_x=route->next_x!=sx;
    direction=along_x?(route->next_x<sx?0u:2u):
                      (route->next_y<sy?1u:3u);
    if(!nav_edge(m,sx,sy,direction,&nx,&ny,&door,&open) ||
       (door && open<40u))return choice;
    /* Correct only at the immediate edge. No off-route circling in the
     * middle of the tile, and never hold up an ordinary straight hallway. */
    if(along_x){
        if((direction==0u?x-(double)sx:
             (double)sx+1.0-x)>0.34)return choice;
    }else{
        if((direction==1u?y-(double)sy:
             (double)sy+1.0-y)>0.34)return choice;
    }
    near=along_x?y:x;
    low=(double)(along_x?route->next_y:route->next_x);
    high=low+1.0;
    /* A neighboring open tile has no solid player-margin restriction.
     * Only tighten an edge where the next cell's source-authored wall or
     * still-closed door actually exists. */
    if(!nav_edge(m,route->next_x,route->next_y,along_x?1u:0u,
                 &nx,&ny,&door,&open) || (door && open<40u))
        low+=BOT_PLAYER_WALL_MARGIN+0.035;
    if(!nav_edge(m,route->next_x,route->next_y,along_x?3u:2u,
                 &nx,&ny,&door,&open) || (door && open<40u))
        high-=BOT_PLAYER_WALL_MARGIN+0.035;
    if(low>=high)return choice;
    if(near<low)delta=low-near;
    else if(near>high)delta=high-near;
    else return choice;
    a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    fx=cos(a);fy=-sin(a);
    for(unsigned i=0u;i<4u;i++){
        const double dx=i==0u?fx:i==1u?-fx:i==2u?fy:-fy;
        const double dy=i==0u?fy:i==1u?-fy:i==2u?-fx:fx;
        const double improvement=along_x?delta*dy:delta*dx;
        if(improvement>best && bot_safe_action(v,keys[i],route)){
            best=improvement;
            choice=keys[i];
        }
    }
    return choice;
}

/* Choose an existing player motion key toward the next cardinal or
 * diagonal waypoint. A fixed movement-angle threshold can reject every
 * legal action during oblique acid navigation; instead check source geometry.
 * Original player physics still owns speed, strafe and collision. */
static unsigned bot_route_motion(const AvpMpBotObservation *v,
                                 const AvpMpBotRoute *route,
                                 int commit_crossing)
{
    static const unsigned actions[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                                      AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
    double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    double fx=cos(a),fy=-sin(a);
    double wx=(double)route->next_x+0.5,wy=(double)route->next_y+0.5;
    double vx,vy;
    /* R293: Recover from an EXISTING wall-margin overlap first. At the
     * recorded X=3.527,Y=2.197 the normal acid lane calculation rejects
     * all possible lanes because its very first sampled point is already
     * outside the conservative wall-safe band. Select an original legal
     * strafe/translation that improves that overlap before recomputing the
     * acid waypoint. Do not ask the bot to spin to fix lateral clearance. */
    if(v->acid_count && nav_valid(&v->map)){
        const double x=(double)v->x/65536.0,y=(double)v->y/65536.0;
        if(!bot_lane_wall_clear(&v->map,x,y)){
            unsigned best_action=AVP_MP_INPUT_ACTIONS;
            double best_improvement=0.0;
            const double cx=floor(x)+0.5,cy=floor(y)+0.5;
            static const unsigned keys[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
            const double mx[4]={fx,-fx,fy,-fy};
            const double my[4]={fy,-fy,-fx,fx};
            for(unsigned i=0u;i<4u;i++){
                const double nx=x+mx[i]*0.07,ny=y+my[i]*0.07;
                const double improvement=(cx-x)*mx[i]+(cy-y)*my[i];
                if(improvement>best_improvement &&
                   bot_wall_margin_recovery(&v->map,x,y,nx,ny) &&
                   bot_safe_action(v,keys[i],route)){
                    best_improvement=improvement;best_action=keys[i];
                }
            }
            if(best_action<AVP_MP_INPUT_ACTIONS)return best_action;
        }
    }
    /* Entering the next cell at a corner requires clearance on THAT
     * cell's side walls, not just a legal position on the current T.
     * This lateral step may have a negative dot toward the next waypoint;
     * leaving it to the greedy movement scorer causes permanent paralysis. */
    {
        const unsigned recenter=bot_acid_junction_recenter(v,route);
        if(recenter<AVP_MP_INPUT_ACTIONS)return recenter;
    }
    /* A small last lateral correction may be necessary to get wholly
     * outside the original acid contact square. The old 0.05 dot-product
     * deadband swallowed that legal move even with the route selected. */
    double best=(v->acid_count && !commit_crossing &&
                 !bot_route_acid_crossing(v,route))?0.005:0.05;
    /* After an observed acid-lane stall, take the known passable cardinal
     * crossing rather than choosing the same ineffective sidestep again. */
    if(!commit_crossing)bot_route_steer_point(v,route,&wx,&wy);
    if(!commit_crossing && !bot_route_acid_crossing(v,route))
        bot_acid_waypoint(v,route,&wx,&wy);
    bot_prop_waypoint(v,route,&wx,&wy);
    vx=wx-(double)v->x/65536.0;
    vy=wy-(double)v->y/65536.0;
    unsigned choice=AVP_MP_INPUT_ACTIONS;
    for(unsigned n=0;n<4u;n++){
        double dx=n==0u?fx:n==1u?-fx:n==2u?fy:-fy;
        double dy=n==0u?fy:n==1u?-fy:n==2u?-fx:fx;
        double score=vx*dx+vy*dy;
        /* The lane waypoint was already evaluated against every nearby
         * acid square, and Dijkstra chose the level-wide least-cost route.
         * A SECOND, per-command acid penalty makes forward progress lose
         * against doing nothing at precisely the chokepoints we must cross.
         * Do not re-price these source-authorized route commands. */
        if(score>best && bot_safe_action(v,actions[n],route)){
            best=score;choice=actions[n];
        }
    }
    if(choice==AVP_MP_INPUT_ACTIONS && v->player_species!=1u && v->acid_count){
        /* If every positive route motion touches acid, take the least
         * hazardous legal path action instead of freezing indefinitely. */
        double worst=-1e30;
        for(unsigned n=0;n<4u;n++){
            double dx=n==0u?fx:n==1u?-fx:n==2u?fy:-fy;
            double dy=n==0u?fy:n==1u?-fy:n==2u?-fx:fx;
            double dot=vx*dx+vy*dy;
            if(dot<=0.05 || !bot_safe_action(v,actions[n],route))continue;
            double risk=(double)bot_acid_hits(v,(double)v->x/65536.0,
                        (double)v->y/65536.0,
                        (double)v->x/65536.0+dx*0.28,
                        (double)v->y/65536.0+dy*0.28);
            double score=dot-risk*100.0;
            if(score>worst){worst=score;choice=actions[n];}
        }
    }
    /* R288: a tactical acid lane can be mathematically reachable but NOT
     * reachable by a full original input step. For example, the required
     * remaining sidestep is 0.03 cell, while the next physical 0.07-cell
     * input hits a wall (original SafePos margin 40<<9). If all route-
     * waypoint commands were rejected, follow the next AUTHORED route
     * cell instead of doing nothing until a stall timer eventually fires.
     * The original wall/door/prop checks still reject invalid movement;
     * original acid contact and damage remain unchanged. */
    if(choice==AVP_MP_INPUT_ACTIONS && v->acid_count && route->valid &&
       (route->next_x!=(v->x>>16) || route->next_y!=(v->y>>16))){
        const double gx=(double)route->next_x+0.5-(double)v->x/65536.0;
        const double gy=(double)route->next_y+0.5-(double)v->y/65536.0;
        double best_progress=0.05;
        for(unsigned n=0u;n<4u;n++){
            const double dx=n==0u?fx:n==1u?-fx:n==2u?fy:-fy;
            const double dy=n==0u?fy:n==1u?-fy:n==2u?-fx:fx;
            const double progress=gx*dx+gy*dy;
            if(progress>best_progress && bot_safe_action(v,actions[n],route)){
                best_progress=progress;choice=actions[n];
            }
        }
    }
    return choice;
}
/* A bot ALREADY standing in acid must attempt to leave, even if its
 * current combat tactic would otherwise hold position. If there is a route,
 * follow its next waypoint using ordinary player commands. Otherwise take
 * the first locally navigable motion which reduces actual source acid
 * contact. Moving through unavoidable acid is preferable to waiting for
 * death; this helper never changes collision or damage handling. */
static unsigned bot_escape_current_acid(const AvpMpBotObservation *v,
                                         const AvpMpBotRoute *route)
{
    static const unsigned actions[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                                      AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
    const double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    const double fx=cos(a),fy=-sin(a);
    double ax,ay,best=-1e30,route_x=0.0,route_y=0.0;
    unsigned choice=AVP_MP_INPUT_ACTIONS;
    if(v->player_species==1u || !v->acid_count)return choice;
    ax=(double)(int32_t)v->x/65536.0;
    ay=(double)(int32_t)v->y/65536.0;
    if(!bot_acid_hits(v,ax,ay,ax,ay))return choice;
    /* When the graph has no damage-free lane through a one-cell hallway,
     * retreating from the same puddle on every tick is a permanent A-B
     * oscillation. Commit to the authored route instead of stepping away
     * and re-approaching. Native SafePos and acid damage still apply. */
    if(route && route->valid &&
       (route->next_x!=(v->x>>16) || route->next_y!=(v->y>>16))){
        /* Once contact has occurred, the selected graph route must own
         * escape direction. A locally lower-exposure fringe can be tried
         * by the normal route motion, but it must NOT cause this override
         * to step backward while the next tick walks forward again. */
        const unsigned follow=bot_route_motion(v,route,
            bot_route_acid_crossing(v,route));
        if(follow<AVP_MP_INPUT_ACTIONS)return follow;
    }
    if(route && route->valid){
        route_x=(double)route->next_x+0.5-ax;
        route_y=(double)route->next_y+0.5-ay;
    }
    /* A tick's actual MovePlayer translation is far shorter than the
     * puddle's width. Assess whether it INCREASES the distance to an exit
     * from the exact source contact box; don't pick the first legal strafe
     * with unchanged exposure and then perform a 360-degree recovery spin. */
    for(unsigned n=0;n<4u;n++){
        const double dx=n==0u?fx:n==1u?-fx:n==2u?fy:-fy;
        const double dy=n==0u?fy:n==1u?-fy:n==2u?-fx:fx;
        const double bx=ax+dx*0.28,by=ay+dy*0.28;
        double nearest=1e30,initial=1e30,score;
        AvpMpBotObservation test=*v;
        test.acid_count=0u;test.map.risk=NULL;
        if(!bot_safe_action(&test,actions[n],NULL))continue;
        for(unsigned i=0;i<v->acid_count;i++){
            const double px=(double)(int32_t)v->acid[i].x/65536.0;
            const double py=(double)(int32_t)v->acid[i].y/65536.0;
            const double olddist=fmax(fabs(ax-px),fabs(ay-py));
            const double distance=fmax(fabs(bx-px),fabs(by-py));
            if(olddist<initial)initial=olddist;
            if(distance<nearest)nearest=distance;
        }
        /* Prefer leaving ALL contact squares, then increasing clearance;
         * break ties toward an already-authored route. No wall noclip. */
        score=(double)(7u-bot_acid_hits(v,bx,by,bx,by))*100.0+
              (nearest-initial)*80.0+
              (route_x*dx+route_y*dy)*0.1;
        if(score>best+1e-6){best=score;choice=actions[n];}
    }
    return choice;
}
/* This is a behavior decision, never a movement vector fed to the engine.
 * Stay facing the opponent while choosing the ordinary BACKWARD or strafe
 * button whose world direction moves away.  The original MovePlayer,
 * SafePos and AMP collision functions still process every command. */
static unsigned bot_melee_retreat(const AvpMpBotObservation *v,double tx,double ty)
{
    static const unsigned actions[4]={AVP_MP_BACKWARD,AVP_MP_STRAFE_LEFT,
                                       AVP_MP_STRAFE_RIGHT,AVP_MP_FORWARD};
    double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    double fx=cos(a),fy=-sin(a),best=-1.0;
    unsigned choice=AVP_MP_INPUT_ACTIONS;
    if(tx*tx+ty*ty<0.000001 && bot_safe_action(v,AVP_MP_BACKWARD,NULL))
        return AVP_MP_BACKWARD;
    for(unsigned n=0;n<4u;n++){
        double vx=n==0u?-fx:n==1u?fy:n==2u?-fy:fx;
        double vy=n==0u?-fy:n==1u?-fx:n==2u?fx:fy;
        double away=-(tx*vx+ty*vy);
        if(away>best && away>0.0 && bot_safe_action(v,actions[n],NULL)){
            best=away;choice=actions[n];
        }
    }
    /* If the only actual retreat direction is blocked (for example by
     * acid), sidestep along the opponent instead of freezing or advancing
     * further into their contact range. */
    if(choice==AVP_MP_INPUT_ACTIONS){
        unsigned side=((v->tick/12u+v->player_index)&1u)?
            AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
        if(bot_safe_action(v,side,NULL))return side;
        side=side==AVP_MP_STRAFE_LEFT?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
        if(bot_safe_action(v,side,NULL))return side;
    }
    return choice;
}
/* R117: Luring is useful only when an original, collision-tested player
 * motion can actually increase distance from the Alien. Unlike the melee
 * escape helper, never fall back to a parallel sidestep: that can leave a
 * Marine frozen in the choke while its gun is deliberately suppressed.
 * This chooses a BUTTON, not a new velocity or a change to retail physics. */
static unsigned bot_lure_retreat_motion(const AvpMpBotObservation *v,
                                        double tx,double ty)
{
    static const unsigned actions[4]={AVP_MP_BACKWARD,AVP_MP_STRAFE_LEFT,
                                       AVP_MP_STRAFE_RIGHT,AVP_MP_FORWARD};
    const double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    const double fx=cos(a),fy=-sin(a);
    double best=0.05;
    unsigned choice=AVP_MP_INPUT_ACTIONS;
    for(unsigned n=0;n<4u;n++){
        const double vx=n==0u?-fx:n==1u?fy:n==2u?-fy:fx;
        const double vy=n==0u?-fy:n==1u?-fx:n==2u?fx:fy;
        const double away=-(tx*vx+ty*vy);
        if(away>best && bot_safe_action(v,actions[n],NULL)){
            best=away;
            choice=actions[n];
        }
    }
    return choice;
}
/* Empty Marines may sidestep if every strictly-away axis is blocked,
 * but never choose a step that reduces separation just to move somewhere. */
static unsigned bot_empty_escape_motion(const AvpMpBotObservation *v,double tx,double ty)
{
    unsigned motion=bot_lure_retreat_motion(v,tx,ty);
    if(motion<AVP_MP_INPUT_ACTIONS)return motion;
    const unsigned actions[4]={AVP_MP_BACKWARD,AVP_MP_STRAFE_LEFT,
                               AVP_MP_STRAFE_RIGHT,AVP_MP_FORWARD};
    const double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    const double fx=cos(a),fy=-sin(a);
    for(unsigned n=0;n<4u;n++){
        const double vx=n==0u?-fx:n==1u?fy:n==2u?-fy:fx;
        const double vy=n==0u?-fy:n==1u?-fx:n==2u?fx:fy;
        if(tx*vx+ty*vy<=0.000001 && bot_safe_action(v,actions[n],NULL))
            return actions[n];
    }
    return AVP_MP_INPUT_ACTIONS;
}
/* A rescue exception must be earned by original collision-legal geometry,
 * not merely by another actor being somewhere inside the acid splash. Use
 * all four legal original movement buttons as escape probes; the temporary
 * view has no privileged movement, and the game still resolves real actors. */
int avp_mp_bot_alien_traps_player(const AvpMpBotMap *map,
    uint32_t alien_x,uint32_t alien_y,uint32_t player_x,uint32_t player_y)
{
    AvpMpBotObservation player={0};
    const double dx=(double)((int64_t)alien_x-(int64_t)player_x)/65536.0;
    const double dy=(double)((int64_t)alien_y-(int64_t)player_y)/65536.0;
    if(!nav_valid(map) || fabs(dx)>0.74 || fabs(dy)>0.74)return 0;
    player.map=*map;
    player.x=player_x;player.y=player_y;
    player.heading=0u;
    return bot_lure_retreat_motion(&player,dx,dy)==AVP_MP_INPUT_ACTIONS;
}

/* Blend an original forward/backward command with one original sidestep
   command while following the same route waypoint (cardinal or diagonal). This avoids the
   visual stop-turn-stop cadence at diagonal headings. The combined move
   must clear the SAME original wall/door/prop checks; native MovePlayer
   still owns the actual velocity and collision resolution. */
static unsigned bot_route_second_motion(const AvpMpBotObservation *v,
                                         const AvpMpBotRoute *route,
                                         unsigned primary)
{
    const double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    const double fx=cos(a),fy=-sin(a);
    double way_x=(double)route->next_x+0.5;
    double way_y=(double)route->next_y+0.5;
    double wx,wy,forward,sideways;
    bot_route_steer_point(v,route,&way_x,&way_y);
    if(!bot_route_acid_crossing(v,route))
        bot_acid_waypoint(v,route,&way_x,&way_y);
    bot_prop_waypoint(v,route,&way_x,&way_y);
    wx=way_x-(double)(int32_t)v->x/65536.0;
    wy=way_y-(double)(int32_t)v->y/65536.0;
    forward=fx*wx+fy*wy;
    sideways=-fy*wx+fx*wy;
    unsigned secondary=AVP_MP_INPUT_ACTIONS;
    double x1=0.0,y1=0.0,x2=0.0,y2=0.0;
    /* Preserve single-axis precision at real props and on avoidable acid
     * approaches. On a path that has ALREADY selected an acid-covered tile,
     * keep the ordinary full-rate forward+strafe command available instead
     * of making the player rotate/sidestep a little on every 10-Hz tick. */
    /* Do not disable two-axis commands just because acid is NEARBY.
     * The swept damage square below decides whether combining axes would
     * cut the corner of an otherwise clean lane. */
    if(primary==AVP_MP_FORWARD || primary==AVP_MP_BACKWARD){
        if(fabs(sideways)<0.22)return secondary;
        secondary=sideways>0.0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
    }else if(primary==AVP_MP_STRAFE_LEFT || primary==AVP_MP_STRAFE_RIGHT){
        if(fabs(forward)<0.22)return secondary;
        secondary=forward>0.0?AVP_MP_FORWARD:AVP_MP_BACKWARD;
    }else return secondary;
    switch(primary){
    case AVP_MP_FORWARD:x1=fx;y1=fy;break;
    case AVP_MP_BACKWARD:x1=-fx;y1=-fy;break;
    case AVP_MP_STRAFE_LEFT:x1=fy;y1=-fx;break;
    case AVP_MP_STRAFE_RIGHT:x1=-fy;y1=fx;break;
    default:break;
    }
    switch(secondary){
    case AVP_MP_FORWARD:x2=fx;y2=fy;break;
    case AVP_MP_BACKWARD:x2=-fx;y2=-fy;break;
    case AVP_MP_STRAFE_LEFT:x2=fy;y2=-fx;break;
    case AVP_MP_STRAFE_RIGHT:x2=-fy;y2=fx;break;
    default:break;
    }
    /* The sidestep by itself can enter a different cell, but its VECTOR
       SUM with forward motion can remain on the authored cardinal route. */
    if(!bot_step_clear(v,x1+x2,y1+y2,route))return AVP_MP_INPUT_ACTIONS;
    if(v->player_species!=1u && v->acid_count &&
       !bot_route_acid_crossing(v,route)){
        const double ax=(double)(int32_t)v->x/65536.0;
        const double ay=(double)(int32_t)v->y/65536.0;
        const unsigned straight=bot_acid_hits(v,ax,ay,
                                  ax+x1*0.28,ay+y1*0.28);
        const unsigned diagonal=bot_acid_hits(v,ax,ay,
                                  ax+(x1+x2)*0.28,ay+(y1+y2)*0.28);
        if(diagonal>straight)return AVP_MP_INPUT_ACTIONS;
    }
    return secondary;
}
static void emit_turn(double turn,AvpMpPlayerCommand *out);
/* R284: in a one-cell acid corridor two real players can face each
 * other and each block the other's source collision. One bot cannot
 * overcome that deadlock by continually holding Forward. The higher-
 * indexed bot gives the lower-index player the passage, but ONLY when
 * the actual route goes toward a peer who faces back toward us. Existing
 * same-direction acid queues must never be asked to backtrack. */
static int bot_acid_headon_peer(const AvpMpBotObservation *v,
                                const AvpMpBotRoute *route)
{
    const double ax=(double)(int32_t)v->x/65536.0;
    const double ay=(double)(int32_t)v->y/65536.0;
    const double px=(double)(int32_t)v->nearby_peer_x/65536.0-ax;
    const double py=(double)(int32_t)v->nearby_peer_y/65536.0-ay;
    const double fx=(double)route->next_x+0.5-ax;
    const double fy=(double)route->next_y+0.5-ay;
    const double a=(double)v->nearby_peer_heading*(2.0*BOT_PI/4294967296.0);
    const double distance2=px*px+py*py;
    if(!v->acid_count || !nav_valid(&v->map) || !route->valid ||
       v->nearby_peer_index>=v->player_index ||
       !v->nearby_peer_valid || distance2>7.5 || distance2<0.01 ||
       (route->next_x==(v->x>>16) && route->next_y==(v->y>>16)) ||
       !bot_route_acid_choke(v,route))return 0;
    /* The peer must be ahead of THIS route and facing toward the actor.
     * Teammates in an ordinary same-direction queue are not a deadlock. */
    return px*fx+py*fy>0.3 &&
           -(px*cos(a)-py*sin(a))>sqrt(distance2)*0.4;
}

/* Find a genuine side pocket at or behind our current lane, not a fake
 * pass-through near a solid wall. Probe at most 2x13 authored floor cells
 * along a backward ray from the head-on encounter. The normal full route
 * and its original doors/acid costs determine whether it is accessible. */
static int bot_acid_peer_bay(const AvpMpBotObservation *v,
                             const AvpMpBotRoute *route,
                             unsigned *out_x,unsigned *out_y)
{
    const AvpMpBotMap *m=&v->map;
    const int sx=(int)(v->x>>16),sy=(int)(v->y>>16);
    const int dx=(int)route->next_x-sx,dy=(int)route->next_y-sy;
    const double px=(double)(int32_t)v->nearby_peer_x/65536.0;
    const double py=(double)(int32_t)v->nearby_peer_y/65536.0;
    unsigned best=UINT32_MAX;
    int found=0;
    if(abs(dx)+abs(dy)!=1 || !nav_valid(m))return 0;
    for(int back=0;back<=12;back++)for(int side=-1;side<=1;side+=2){
        const int x=sx-dx*back-dy*side;
        const int y=sy-dy*back+dx*side;
        AvpMpBotRoute candidate={0};
        unsigned cost;
        if(x<0||y<0||x>=(int)m->width||y>=(int)m->height ||
           !nav_cell(m,(unsigned)x,(unsigned)y)[5] ||
           (m->risk && m->risk[nav_index((unsigned)x,(unsigned)y)]>=2u) ||
           (fabs((double)x+0.5-px)<1.0 && fabs((double)y+0.5-py)<1.0) ||
           !avp_mp_bot_route(m,(unsigned)sx,(unsigned)sy,
                             (unsigned)x,(unsigned)y,&candidate) ||
           !candidate.valid || candidate.requires_door_use)continue;
        /* Do not route through the peer to reach a supposed 'safe' bay. */
        if(((double)candidate.next_x+0.5-(double)v->x/65536.0)*
                      (px-(double)v->x/65536.0)+
           ((double)candidate.next_y+0.5-(double)v->y/65536.0)*
                      (py-(double)v->y/65536.0)>0.35)continue;
        cost=candidate.cost;
        if(!found || cost<best){found=1;best=cost;
            *out_x=(unsigned)x;*out_y=(unsigned)y;}
    }
    return found;
}

static int bot_acid_peer_yield(AvpMpBotState *state,
    const AvpMpBotObservation *v,const AvpMpBotRoute *route,
    int need_forward,AvpMpPlayerCommand *out)
{
    AvpMpBotRoute bay={0};
    const unsigned sx=v->x>>16,sy=v->y>>16;
    if(v->player_collision_disabled)return 0;
    if((v->hostile_target && v->target_visible) || v->melee_threat ||
       v->other_player_alien_trapped){
        state->peer_pass_valid=0u;return 0;
    }
    if(state->peer_pass_valid &&
       (state->peer_pass_level!=v->observed_world_level ||
        v->tick>=state->peer_pass_until))state->peer_pass_valid=0u;
    if(!state->peer_pass_valid && need_forward && route &&
       bot_acid_headon_peer(v,route)){
        unsigned bx,by;
        /* A genuinely sealed one-cell passage has no side bay. Cache that
         * failed search briefly instead of running 26 A* probes every tick
         * for the same physically unchanged peer confrontation. */
        if(state->peer_pass_index==v->nearby_peer_index &&
           v->tick<state->peer_pass_retry_tick)return 0;
        if(!bot_acid_peer_bay(v,route,&bx,&by)){
            state->peer_pass_index=v->nearby_peer_index;
            state->peer_pass_retry_tick=v->tick+30u;
            return 0;
        }
        state->peer_pass_valid=1u;
        state->peer_pass_x=bx;state->peer_pass_y=by;
        state->peer_pass_index=v->nearby_peer_index;
        state->peer_pass_level=v->observed_world_level;
        state->peer_pass_until=v->tick+105u;
        state->peer_pass_arrived=0u;
    }
    if(!state->peer_pass_valid)return 0;
    if(sx==state->peer_pass_x && sy==state->peer_pass_y){
        if(!state->peer_pass_arrived)state->peer_pass_arrived=v->tick+1u;
        /* Leave enough time for the priority bot to traverse the original
         * one-wide lane. Never bounce immediately back out of the pocket. */
        if(v->tick<state->peer_pass_arrived+40u){
            state->previous_x=v->x;state->previous_y=v->y;
            state->last_motion=0u;return 1;
        }
        state->peer_pass_valid=0u;return 0;
    }
    if(!avp_mp_bot_route(&v->map,sx,sy,
            state->peer_pass_x,state->peer_pass_y,&bay) || !bay.valid ||
       bay.requires_door_use){state->peer_pass_valid=0u;return 0;}
    {
        const double dx=(double)bay.next_x+0.5-(double)v->x/65536.0;
        const double dy=(double)bay.next_y+0.5-(double)v->y/65536.0;
        const unsigned move=bot_route_motion(v,&bay,0u);
        emit_turn(angle_diff(v->heading,dx,dy),out);
        if(move<AVP_MP_INPUT_ACTIONS){
            out->actions|=1u<<move;
            const unsigned second=bot_route_second_motion(v,&bay,move);
            if(second<AVP_MP_INPUT_ACTIONS)out->actions|=1u<<second;
        }
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=out->actions &
            ((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
             (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT));
    }
    return 1;
}

/* Original AMP.S::do_score awards melee 1.0, ranged 0.5; invisibility
 * turns these into -1.0 and -2.0 respectively. The *engine* computes the
 * honor; this is purely the bot's weapon preference. Original slots:
 * 1 combi-stick, 2 cannon, 3 smart disc, 4 wrist blades. */
unsigned avp_mp_bot_predator_preferred_weapon(const AvpMpBotObservation *v)
{
    unsigned melee;
    if(!v||v->player_species!=2u)return 0u;
    melee=(v->predator_owned&(1u<<1))?1u:4u;
    if(!(v->predator_owned&(1u<<melee)))return v->current_weapon;
    /* A live Alien's source death spray takes 50 health in a 0.75-tile
     * square. Prefer an ALREADY UNLOCKED ranged weapon against this target,
     * even when healthy; otherwise a melee kill necessarily splashes the
     * Predator. Switching never grants weapons or alters retail honor. */
    if(v->hostile_target && v->target_is_alien &&
       v->goal==AVP_MP_BOT_HOSTILE){
        if(v->predator_owned&(1u<<3))return 3u; /* Smart Disc */
        if(v->predator_owned&(1u<<2))return 2u; /* Cannon */
    }
    /* Against other enemies, retain the original bot honor preference:
     * weak or outnumbered Predators may choose the unlocked Smart Disc. */
    if(v->hostile_target && (v->predator_owned&(1u<<3)) &&
       ((v->max_energy>0 && v->player_energy*100<v->max_energy*45) ||
        v->combat_enemies_nearby>=3u))return 3u;
    return melee;
}

/* Seeded bot-owned choice, isolated from the Jaguar's gameplay RNG.
 * Choose any actually owned combat weapon, avoiding the previous choice
 * when there is more than one. This never grants inventory. */
unsigned avp_mp_bot_random_predator_weapon(unsigned owned,unsigned previous,
                                           uint32_t seed)
{
    unsigned candidates[4],count=0u,choice;
    for(unsigned w=1u;w<=4u;w++)if(owned&(1u<<w))candidates[count++]=w;
    if(!count)return 0u;
    seed^=seed>>16;seed*=0x7feb352du;
    seed^=seed>>15;seed*=0x846ca68bu;seed^=seed>>16;
    choice=seed%count;
    if(count>1u && candidates[choice]==previous)
        choice=(choice+1u)%count;
    return candidates[choice];
}
/* R22 Predator Deathmatch rotation: 16 distinct permutations of all four
 * source weapon slots. There are 24 mathematical possibilities; this formula
 * intentionally uses 16. A bot walks the 16 rows with a private random start
 * and an odd stride, so each row occurs exactly once before the 64-slot
 * formula repeats. Only the source's weapon buttons perform actual switches. */
static const uint8_t predator_weapon_orders[16][4]={
    {1,2,3,4},{1,3,4,2},{1,4,2,3},{1,4,3,2},
    {2,1,3,4},{2,3,4,1},{2,4,1,3},{2,4,3,1},
    {3,1,4,2},{3,2,1,4},{3,4,1,2},{3,4,2,1},
    {4,1,2,3},{4,2,3,1},{4,3,1,2},{4,3,2,1}
};
unsigned avp_mp_bot_predator_order_weapon(unsigned order,unsigned step)
{
    if(order>=16u||step>=4u)return 0u;
    return predator_weapon_orders[order][step];
}
static unsigned bot_predator_formula_weapon(const AvpMpBotState *state)
{
    unsigned order=(state->predator_pattern_start+
        state->predator_pattern_stride*state->predator_pattern_order)&15u;
    return avp_mp_bot_predator_order_weapon(order,state->predator_pattern_step);
}
static void bot_predator_formula_advance(AvpMpBotState *state)
{
    if(++state->predator_pattern_step==4u){
        state->predator_pattern_step=0u;
        state->predator_pattern_order=(state->predator_pattern_order+1u)&15u;
    }
    state->predator_weapon_target=bot_predator_formula_weapon(state);
}
/* Actual route hops use the original LEVDOOR.S and DOORS.S metadata.
 * 1 is the main entry to the Predator ship, 15 is its end-Queen world. */
unsigned avp_mp_bot_alien_campaign_target_level(unsigned current_level)
{
    if(current_level<1u||current_level>15u||current_level==15u)return 0u;
    return current_level==1u?15u:1u;
}

static unsigned bot_weapon_cycle_direction(unsigned owned,unsigned current,
                                            unsigned desired)
{
    unsigned forward=current,backward=current;
    if(current<1u||current>4u)return AVP_MP_NEXT_WEAPON;
    for(unsigned n=0;n<4u;n++){
        forward=forward==4u?1u:forward+1u;
        backward=backward==1u?4u:backward-1u;
        if((owned&(1u<<forward)) && forward==desired)
            return AVP_MP_NEXT_WEAPON;
        if((owned&(1u<<backward)) && backward==desired)
            return AVP_MP_PREV_WEAPON;
    }
    return AVP_MP_NEXT_WEAPON;
}

static int bot_melee_weapon(const AvpMpBotObservation *v)
{
    /* Alien A/B/C are all melee attacks.  Predator ranged slots 2 and 3
     * must NOT be forced into the melee positioning band. */
    return v->player_species==1u ||
           (v->player_species==2u &&
            (v->current_weapon==1u || v->current_weapon==4u));
}
/* Guerilla distance management is independent of the fire decision.
 * Choose ordinary player movement buttons, never synthetic velocity. Close
 * when outside the desired band, BACKPEDAL when inside it, and flank only
 * while inside the band or when the authored collision rejects a step.
 * Melee range uses the original source weapon reach; ranged distance bands
 * are movement preferences only, NEVER artificial maximum firing ranges. */
static int bot_target_back_turned(const AvpMpBotObservation *v,
                                  double tx,double ty)
{
    const double a=(double)v->target_heading*(2.0*BOT_PI/4294967296.0);
    const double dist=sqrt(tx*tx+ty*ty);
    if(!v->target_visible || !v->target_heading_valid || dist<0.01)
        return 0;
    /* tx/ty point from bot TO enemy. A positive projection onto the
     * enemy's forward axis means the bot is behind that enemy. */
    return (cos(a)*tx-sin(a)*ty)>dist*0.60;
}
static unsigned bot_guerilla_approach(const AvpMpBotObservation *v,
                                      double tx,double ty)
{
    static const unsigned actions[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                                      AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
    const double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    const double fx=cos(a),fy=-sin(a);
    unsigned choice=AVP_MP_INPUT_ACTIONS;
    double best=0.0;
    for(unsigned n=0;n<4u;n++){
        const double dx=n==0u?fx:n==1u?-fx:n==2u?fy:-fy;
        const double dy=n==0u?fy:n==1u?-fy:n==2u?-fx:fx;
        const double progress=tx*dx+ty*dy;
        if(progress>best && bot_safe_action(v,actions[n],NULL)){
            best=progress;choice=actions[n];
        }
    }
    return choice;
}
static unsigned bot_guerilla_motion(AvpMpBotState *state,
                                    const AvpMpBotObservation *v,
                                    double tx,double ty,int back_turned)
{
    const double dist2=tx*tx+ty*ty;
    const int melee=bot_melee_weapon(v);
    /* Movement preferences only. Even a distant visible target can be
     * shot with a ranged weapon: only source collision/hit rules decide. */
    const double near=melee?BOT_MELEE_HOLD_NEAR:3.0;
    const double far=melee?BOT_MELEE_HOLD_FAR:5.0;
    unsigned side,other,motion;
    if(!state->guerilla_flank_ready){
        state->guerilla_flank_ready=1u;
        state->guerilla_flank_side=v->player_index&1u;
        state->guerilla_flank_until=v->tick+28u;
    }else if(v->tick>=state->guerilla_flank_until){
        state->guerilla_flank_side^=1u;
        state->guerilla_flank_until=v->tick+28u;
    }
    side=state->guerilla_flank_side?
         AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
    other=side==AVP_MP_STRAFE_LEFT?
          AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
    /* Do not orbit indefinitely outside melee reach (or far beyond a
     * preferred ranged fighting distance). Keep closing with legal player
     * controls; the aim and Fire decision is made independently below. */
    if(dist2>far*far || (melee && back_turned && dist2>near*near)){
        motion=bot_guerilla_approach(v,tx,ty);
        if(motion<AVP_MP_INPUT_ACTIONS)return motion;
    }
    /* Backpedal while looking at the opponent; another safe translation
     * axis is chosen when the wall/acid/solid prop blocks backward movement. */
    if(dist2<near*near){
        motion=bot_melee_retreat(v,tx,ty);
        if(motion<AVP_MP_INPUT_ACTIONS)return motion;
    }
    if(bot_safe_action(v,side,NULL))return side;
    if(bot_safe_action(v,other,NULL)){
        state->guerilla_flank_side^=1u;
        state->guerilla_flank_until=v->tick+28u;
        return other;
    }
    /* Corridor fallback: select a safe opening without changing the
     * original movement/collision code or inadvertently cancelling Fire. */
    if(dist2>far*far){
        motion=bot_guerilla_approach(v,tx,ty);
        if(motion<AVP_MP_INPUT_ACTIONS)return motion;
    }
    if(dist2<near*near){
        motion=bot_melee_retreat(v,tx,ty);
        if(motion<AVP_MP_INPUT_ACTIONS)return motion;
    }
    if(bot_safe_action(v,AVP_MP_BACKWARD,NULL))return AVP_MP_BACKWARD;
    if(bot_safe_action(v,AVP_MP_FORWARD,NULL))return AVP_MP_FORWARD;
    return AVP_MP_INPUT_ACTIONS;
}
static void emit_turn(double turn,AvpMpPlayerCommand *out)
{
    /* Internal signed radians hint for the precise input adapter. */
    out->analog_turn=(float)turn;
    if(turn>BOT_PI/23.0)out->actions|=1u<<AVP_MP_TURN_LEFT;
    if(turn< -BOT_PI/23.0)out->actions|=1u<<AVP_MP_TURN_RIGHT;
}
/* R281: with a truly visible target and a loaded Marine gun, don't use
 * the patrol-turn deadband (7.8 degrees) for precise firearm aiming.
 * Only bot input is changed; source MovePlayer's 0x300 acceleration and
 * original gun scatter / TestSpark ray remain untouched. */
static void emit_marine_precision_turn(double turn,AvpMpPlayerCommand *out)
{
    /* Internal signed radians hint for the precise input adapter. */
    out->analog_turn=(float)turn;
    if(turn>BOT_PI/60.0)out->actions|=1u<<AVP_MP_TURN_LEFT;
    if(turn< -BOT_PI/60.0)out->actions|=1u<<AVP_MP_TURN_RIGHT;
}
/* R130: a real decrease in this actor's ORIGINAL health signals danger.
 * This does not disclose an attacker, direction, weapon or location. Damage
 * from acid still uses native rules and the existing immediate acid escape.
 * Reset on healing/respawn, level transfer and tick-clock restart so old
 * damage from another life never produces a false reaction. */
static void bot_observe_damage(AvpMpBotState *s,const AvpMpBotObservation *v)
{
    if(!s->damage_observed || v->tick<s->damage_observed_tick ||
       s->damage_world_level!=v->observed_world_level ||
       v->player_energy<=0 || v->player_energy>s->last_observed_energy){
        s->damage_react_until=0u;
        s->damage_side=v->player_index&1u;
    }else if(v->player_energy<s->last_observed_energy){
        /* 1.8 seconds to notice a regular hit; 2.6 when critically hurt.
         * Further genuine damage refreshes this brief response. */
        const uint64_t interval=(v->max_energy>0 &&
            (int64_t)v->player_energy*100 < (int64_t)v->max_energy*40)?26u:18u;
        s->damage_react_until=v->tick+interval;
        s->damage_side^=1u;
    }
    s->last_observed_energy=v->player_energy;
    s->damage_world_level=v->observed_world_level;
    s->damage_observed_tick=v->tick;
    s->damage_observed=1u;
}
/* Pick only ORIGINAL player movement inputs that clear the normal source
 * collision and prop/acid guards. A visible enemy is real information,
 * so a badly hurt ranged bot may withdraw; without visible contact this
 * simply dodges and scans rather than aiming at privileged coordinates. */
static unsigned bot_damage_evasion(const AvpMpBotState *s,
                                   const AvpMpBotObservation *v)
{
    const unsigned side=s->damage_side?AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
    const unsigned other=side==AVP_MP_STRAFE_LEFT?
        AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
    if(v->hostile_target && v->target_visible &&
       v->behavior!=1u && !bot_melee_weapon(v) &&
       v->max_energy>0 &&
       (int64_t)v->player_energy*100<(int64_t)v->max_energy*40){
        const double dx=((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        const double dy=((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        unsigned retreat=bot_melee_retreat(v,dx,dy);
        if(retreat<AVP_MP_INPUT_ACTIONS)return retreat;
    }
    if(bot_safe_action(v,side,NULL))return side;
    if(bot_safe_action(v,other,NULL))return other;
    if(bot_safe_action(v,AVP_MP_BACKWARD,NULL))return AVP_MP_BACKWARD;
    return AVP_MP_INPUT_ACTIONS; /* blocked: never noclip or invent velocity */
}
static void bot_damage_react(const AvpMpBotState *s,
                             const AvpMpBotObservation *v,
                             AvpMpPlayerCommand *out)
{
    const unsigned movement=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
    unsigned motion;
    if(v->tick>=s->damage_react_until || !s->damage_observed)return;
    motion=bot_damage_evasion(s,v);
    if(motion<AVP_MP_INPUT_ACTIONS)
        out->actions=(out->actions&~movement)|(1u<<motion);
    /* If the current target is genuinely visible, preserve aim and FIRE.
     * Otherwise look around in response to being hurt; NEVER turn toward
     * a remembered/tracker-only target's hidden coordinates. */
    if(!(v->hostile_target && v->target_visible)){
        out->actions &= ~((1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT));
        out->analog_turn=0.0f;
        if(v->melee_threat && v->melee_visible){
            emit_turn(angle_diff(v->heading,
                 ((int64_t)v->melee_x-(int64_t)v->x)/65536.0,
                 ((int64_t)v->melee_y-(int64_t)v->y)/65536.0),out);
        }else out->actions|=1u<<(s->damage_side?
                       AVP_MP_TURN_LEFT:AVP_MP_TURN_RIGHT);
    }
}
/* When a noclip/glitch position lies beyond the authored map or inside a
 * nonfloor cell, A* cannot start and the normal safety filter refuses all
 * steps. Return using player buttons toward the nearest authored floor cell.
 * Never edit coordinates or bypass the original MovePlayer/SafePos checks.
 * If the native collision refuses re-entry, this remains a best-effort
 * recovery rather than an unsafe teleport across an exterior wall. */
static int bot_recover_outside(AvpMpBotState *state,
                               const AvpMpBotObservation *v,
                               AvpMpPlayerCommand *out)
{
    const AvpMpBotMap *m=&v->map;
    const int32_t px=(int32_t)v->x,py=(int32_t)v->y;
    const double xx=(double)px/65536.0,yy=(double)py/65536.0;
    const int inside=!avp_mp_bot_outside_bounds(v->x,v->y,
                                                m->width,m->height);
    double dx=0.0,dy=0.0,closest=1e99;
    double angle,fx,fy,scores[4];
    unsigned picked=0u,alternate=2u;
    static const unsigned motions[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                                     AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
    if(!nav_valid(m))return 0;
    if(inside && nav_cell(m,(uint32_t)px>>16,(uint32_t)py>>16)[5])return 0;
    for(unsigned y=0;y<m->height;y++)for(unsigned x=0;x<m->width;x++){
        double vx,vy,d;
        if(!nav_cell(m,x,y)[5])continue;
        vx=(double)x+0.5-xx;vy=(double)y+0.5-yy;
        d=vx*vx+vy*vy;
        if(d<closest){closest=d;dx=vx;dy=vy;}
    }
    if(closest==1e99)return 1; /* Corrupt/no-floor map: do not improvise. */
    emit_turn(angle_diff(v->heading,dx,dy),out);
    angle=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    fx=cos(angle);fy=-sin(angle);
    scores[0]= dx*fx+dy*fy;
    scores[1]=-scores[0];
    scores[2]=dx*fy-dy*fx;
    scores[3]=-scores[2];
    for(unsigned a=0;a<4u;a++)if(scores[a]>scores[picked])picked=a;
    alternate=picked==0u?1u:0u;
    for(unsigned a=0;a<4u;a++)if(a!=picked && scores[a]>scores[alternate])
        alternate=a;
    if(state->last_motion &&
       llabs((long long)(int64_t)v->x-(long long)(int64_t)state->previous_x)<128 &&
       llabs((long long)(int64_t)v->y-(long long)(int64_t)state->previous_y)<128)
        state->immobile_ticks++;
    else state->immobile_ticks=0;
    /* Try a different real movement axis when the nearest approach is
     * repeatedly blocked; never synthesize collision-bypassing velocity. */
    if(state->immobile_ticks>=9u){
        state->immobile_ticks=0u;
        state->recovery_side^=1u;
        state->evade_ticks=12u;
    }
    if(state->evade_ticks){
        --state->evade_ticks;
        if(scores[alternate]>0.0)picked=alternate;
    }
    out->actions|=1u<<motions[picked];
    state->last_motion=1u<<motions[picked];
    state->previous_x=v->x;state->previous_y=v->y;
    return 1;
}
/* Bot exploration: choose unvisited, far-enough source-connected floors,
 * then VERIFY their routes through the normal original door/collision map.
 * Never teleport or invent a path through an inaccessible cell. */
int avp_mp_bot_patrol_pick(const AvpMpBotMap *map,unsigned x,unsigned y,
    const uint16_t visits[AVP_MP_BOT_MAP_CELLS],uint32_t seed,
    unsigned *target_x,unsigned *target_y)
{
    struct PatrolCandidate {unsigned x,y;uint64_t merit;} top[12]={{0}};
    uint16_t comps[AVP_MP_BOT_MAP_CELLS];
    unsigned count=0,component,minimum_dist=2u;
    if(!map||!visits||!target_x||!target_y||!nav_valid(map)||
       x>=map->width||y>=map->height)return 0;
    if(map->risk)for(unsigned i=0;i<AVP_MP_BOT_MAP_CELLS;i++)
        if(map->risk[i]==3u){minimum_dist=1u;break;}
    avp_mp_bot_components(map,comps);
    component=comps[y*AVP_MP_BOT_MAP_MAX+x];
    if(!component)return 0;
    for(unsigned cy=0;cy<map->height;cy++)for(unsigned cx=0;cx<map->width;cx++){
        unsigned idx=cy*AVP_MP_BOT_MAP_MAX+cx;
        unsigned dx=cx>x?cx-x:x-cx,dy=cy>y?cy-y:y-cy;
        unsigned dist=dx>dy?dx:dy,at;
        uint64_t merit;
        if(comps[idx]!=component||dist<minimum_dist||visits[idx]>=60000u)continue;
        /* Patrol goals are exact tile centres, never useful acid pickups.
         * A bypassable puddle can still make that centre uninhabitable. */
        if(map->risk && map->risk[idx]>=2u)continue;
        /* Rare visits dominate; distant cells encourage active exploration.
         * Seed ties differently per player/tick, with no foreign target data. */
        /* Each bot's stable seed materially changes route selection; the
         * old 8-bit tie breaker was swamped by the farthest-cell score and
         * sent every new bot to the same map corner. Preserve its PRIVATE
         * visit history as the primary avoid-repeat signal. */
        {
            uint32_t hash=(uint32_t)idx*2654435761u ^ seed*2246822519u;
            hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;
            /* Strongly prefer less-traveled authored cells over an
             * immediate journey back along the same two corridors.
             * Seed still breaks ties differently for individual bots. */
            merit=36000u/(1u+(uint64_t)visits[idx]*4u)+
                  (uint64_t)(dist>24u?24u:dist)*128u+
                  (uint64_t)(hash&0x0fffu);
        }
        for(at=0;at<count && top[at].merit>=merit;at++);
        if(at>=12u)continue;
        if(count<12u)count++;
        for(unsigned i=count-1u;i>at;i--)top[i]=top[i-1u];
        top[at]=(struct PatrolCandidate){cx,cy,merit};
    }
    for(unsigned i=0;i<count;i++){
        AvpMpBotRoute route={0};
        if(avp_mp_bot_route(map,x,y,top[i].x,top[i].y,&route)){
            *target_x=top[i].x;*target_y=top[i].y;return 1;
        }
    }
    return 0;
}
/* Select a real floor reachable by the original collision/navigation
 * graph and farther from the contact. No unseen actors or fake exits.
 * With no such escape cell, return false and preserve the current safe
 * standoff instead of ordering the bot through a solid wall or acid. */
int avp_mp_bot_pick_refuge(const AvpMpBotMap *map,unsigned sx,unsigned sy,
    unsigned threat_x,unsigned threat_y,unsigned *rx,unsigned *ry)
{
    static const int off[8][2]={{-1,0},{1,0},{0,-1},{0,1},
                                  {-1,-1},{1,-1},{-1,1},{1,1}};
    const int64_t ax=(int64_t)sx-(int64_t)threat_x;
    const int64_t ay=(int64_t)sy-(int64_t)threat_y;
    const int64_t start_d=ax*ax+ay*ay;
    int64_t best=INT64_MIN;
    int found=0;
    if(!map||!map->cells||!rx||!ry||sx>=map->width||sy>=map->height)
        return 0;
    for(unsigned radius=2u;radius<=6u;radius+=2u)
        for(unsigned d=0;d<8u;d++){
            int x=(int)sx+(int)radius*off[d][0];
            int y=(int)sy+(int)radius*off[d][1];
            AvpMpBotRoute route={0};
            int64_t dx,dy,dist,merit;
            if(x<0||y<0||x>=(int)map->width||y>=(int)map->height)
                continue;
            dx=(int64_t)x-(int64_t)threat_x;
            dy=(int64_t)y-(int64_t)threat_y;
            dist=dx*dx+dy*dy;
            if(dist<=start_d+2 ||
               !avp_mp_bot_route(map,sx,sy,(unsigned)x,(unsigned)y,&route) ||
               !route.valid || route.cost>24u)continue;
            /* Value separation but penalize long paths; don't try a long
             * circuit through the enemy just to gain one extra cell. */
            merit=dist*12-(int64_t)route.cost*5;
            if(!found || merit>best){
                found=1;best=merit;*rx=(unsigned)x;*ry=(unsigned)y;
            }
        }
    return found;
}
/* Select translation in world space, independently of the desired facing.
 * The native movement routine uses the pre-turn sin/cos basis this tick.
 * Only ordinary approaches use this path; doors, hazard tactics and peer
 * yielding retain their existing planner. Native collision is authoritative. */
static int bot_precise_travel(const AvpMpBotObservation *v,
                             const AvpMpBotRoute *route,double dx,double dy,
                             int allow_distant_acid,AvpMpPlayerCommand *out)
{
    const double len=hypot(dx,dy);
    if(len<0.00001 || v->melee_threat ||
       (v->acid_count && !allow_distant_acid) || bot_hazards_close(v))return 0;
    dx/=len;dy/=len;
    if(!bot_peer_precision_clear(v,dx,dy,0.68))return 0;
    if(!bot_step_clear_probe(v,dx,dy,route,fmin(len,0.68)))return 0;
    if(nav_valid(&v->map)){
        const double x=(double)v->x/65536.0,y=(double)v->y/65536.0;
        const double probe=fmin(len,0.12);
        if(!bot_lane_wall_clear(&v->map,x+dx*probe,y+dy*probe) &&
           !bot_wall_margin_recovery(&v->map,x,y,x+dx*probe,y+dy*probe))return 0;
    }
    const double angle=(double)v->heading*(2.0*BOT_PI/4294967296.0);
    const double fx=cos(angle),fy=-sin(angle);
    double f=dx*fx+dy*fy,r=-dx*fy+dy*fx;
    if(fabs(f)<0.000001)f=0;
    if(fabs(r)<0.000001)r=0;
    out->analog_forward=(float)f;out->analog_strafe=(float)r;
    if(f!=0)out->actions|=1u<<(f>0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
    if(r!=0)out->actions|=1u<<(r>0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT);
    return 1;
}
/* R477: The old acid route used full digital keys (and body-relative
 * hysteresis) even for a tiny lateral correction. At a corner that can
 * overshoot the safe lane and reverse it next tick. Use an authored cardinal
 * hop and proportional world-space movement; preserve low-exposure lanes,
 * but prefer passage over waiting forever when no lane avoids the acid.
 * Native MovePlayer/SafePos/AMP collision and acid damage remain in charge. */
static int bot_precise_local_route(const AvpMpBotObservation *v,
    const AvpMpBotRoute *route,int commit,AvpMpPlayerCommand *out)
{
    const unsigned sx=v->x>>16,sy=v->y>>16;
    const double x=v->x/65536.0,y=v->y/65536.0;
    const int along_x=route->next_x!=sx;
    const double cx=route->next_x+0.5,cy=route->next_y+0.5;
    double wx=cx,wy=cy;
    if(!route->valid || route->requires_door_use || !route->door_is_open ||
       !nav_valid(&v->map) || (abs((int)route->next_x-(int)sx)+
          abs((int)route->next_y-(int)sy))!=1)return 0;
    /* For acid retain the existing best-lane selection unless the watchdog
     * already requested a bounded crossing. Props retain their own lanes. */
    if(v->acid_count && !commit && !bot_route_acid_crossing(v,route))
        bot_acid_waypoint(v,route,&wx,&wy);
    bot_prop_waypoint(v,route,&wx,&wy);
    for(unsigned attempt=0;attempt<3;attempt++){
        double dx=wx-x,dy=wy-y,len=hypot(dx,dy);
        if(len>0.004){
            double ux=dx/len,uy=dy/len,probe=fmin(len,0.10);
            if(bot_peer_precision_clear(v,ux,uy,0.68) &&
               bot_step_clear_probe(v,ux,uy,route,probe) &&
               bot_short_lane_clear(&v->map,x,y,x+ux*probe,y+uy*probe)){
                const double a=v->heading*(2.0*BOT_PI/4294967296.0);
                const double fx=cos(a),fy=-sin(a);
                /* Short lateral corrections taper before reaching the lane,
                 * unlike full digital sidesteps. Cruise remains bounded. */
                const double throttle=fmin(0.65,len/0.30);
                const unsigned moves=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                    (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
                out->analog_forward=(float)((ux*fx+uy*fy)*throttle);
                out->analog_strafe=(float)((-ux*fy+uy*fx)*throttle);
                out->analog_axis_mask|=3u;
                out->actions&=~moves;
                if(fabs(out->analog_forward)>0.00001)
                    out->actions|=1u<<(out->analog_forward>0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
                if(fabs(out->analog_strafe)>0.00001)
                    out->actions|=1u<<(out->analog_strafe>0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT);
                return 1;
            }
        }
        /* First recenter parallel to the crossing edge, without walking
         * into the destination's side wall. If a tiny lane adjustment is
         * not feasible, try the authored cell center on unavoidable acid
         * or during the bounded recovery window. */
        if(attempt==0){
            /* A failed side lane is not a reason to push into the wall.
             * Recenter first; subsequent ticks reprice the acid. */
            wx=along_x?x:cx;wy=along_x?cy:y;
        }else{wx=cx;wy=cy;}
    }
    return 0;
}
/* Remember the last actual navigation objective; combat selection may
 * replace target_x/y with an enemy, but must not erase the travel route.
 * Never retain a scripted interaction, investigation or cocoon as travel. */
static void bot_remember_travel(AvpMpBotState *s,const AvpMpBotObservation *v)
{
    if(s->travel_valid && (s->travel_level!=v->observed_world_level ||
       v->tick<s->travel_tick || v->tick-s->travel_tick>120u ||
       v->player_energy<=0 || v->facehugged))s->travel_valid=0u;
    if(v->hostile_target)return;
    s->travel_valid=0u;s->travel_progress_valid=0u;
    if(!v->has_target || v->facehugged || v->player_energy<=0 ||
       !nav_valid(&v->map))return;
    switch(v->goal){
    case AVP_MP_BOT_PATROL: case AVP_MP_BOT_FOLLOW:
    case AVP_MP_BOT_EXIT: case AVP_MP_BOT_LIFT:
    case AVP_MP_BOT_SUPPLY: case AVP_MP_BOT_HEALTH: case AVP_MP_BOT_MEDICAL:
        break;
    default:return;
    }
    s->travel_valid=1u;s->travel_level=v->observed_world_level;
    s->travel_goal=v->goal;s->travel_x=v->target_x;s->travel_y=v->target_y;
    s->travel_tick=v->tick;
}
/* Recompute the remembered route against CURRENT geometry. Combat never
 * opens a door by itself, bypasses collision or operates a stale panel.
 * An unavailable route yields to the original combat planner. */
static int bot_combat_travel(AvpMpBotState *s,const AvpMpBotObservation *v,
                            AvpMpPlayerCommand *out)
{
    AvpMpBotRoute route={0};AvpMpBotMap map=v->map;
    double wx,wy,dx,dy;
    unsigned sx=v->x>>16,sy=v->y>>16;
    if(!s->travel_valid || !v->has_target || !v->hostile_target ||
       !v->target_visible || v->tracker_only_target ||
       v->goal!=AVP_MP_BOT_HOSTILE || !nav_valid(&map))return 0;
    dx=((int64_t)s->travel_x-(int64_t)v->x)/65536.0;
    dy=((int64_t)s->travel_y-(int64_t)v->y)/65536.0;
    if((sx==(s->travel_x>>16) && sy==(s->travel_y>>16)) ||
       (s->travel_goal==AVP_MP_BOT_FOLLOW && hypot(dx,dy)<=1.8) ||
       (s->travel_goal==AVP_MP_BOT_PATROL && fabs(dx)<=0.50 && fabs(dy)<=0.50)){
        s->travel_valid=0u;return 0;
    }
    if(v->player_species==1u)map.risk=NULL;
    if(s->nav_bad_until>v->tick && s->nav_bad_level==v->observed_world_level)
        map.avoid_cell_plus_one=s->nav_bad_next_plus_one;
    if(!avp_mp_bot_route(&map,sx,sy,s->travel_x>>16,s->travel_y>>16,&route) ||
       !route.valid || route.requires_door_use ||
       (route.next_x==sx && route.next_y==sy))return 0;
    if(map.risk && (map.risk[nav_index(sx,sy)] ||
       map.risk[nav_index(route.next_x,route.next_y)]))return 0;
    /* A successful INPUT command is not proof that native collision
     * allowed motion. Bound attempts at the same remembered route hop.
     * Count actual active planner attempts, not idle or paused ticks. */
    {
        const unsigned next=nav_index(route.next_x,route.next_y)+1u;
        const double remaining=hypot((double)route.next_x+0.5-v->x/65536.0,
                                     (double)route.next_y+0.5-v->y/65536.0);
        if(!s->travel_progress_valid || s->travel_progress_next!=next ||
           v->tick<s->travel_progress_tick ||
           remaining+0.125<=s->travel_progress_best){
            s->travel_progress_valid=1u;s->travel_progress_next=next;
            s->travel_progress_best=remaining;s->travel_progress_stalls=0u;
            s->travel_progress_tick=v->tick;
        }else if(v->tick!=s->travel_progress_tick){
            s->travel_progress_tick=v->tick;
            if(++s->travel_progress_stalls>=45u){
                s->nav_bad_next_plus_one=next;s->nav_bad_level=v->observed_world_level;
                s->nav_bad_until=v->tick+110u;
                s->travel_valid=0u;s->travel_progress_valid=0u;
                /* Continue ordinary combat (including aim/FIRE) without
                 * insisting on the stalled previous travel objective. */
                return 0;
            }
        }
    }
    bot_route_steer_point(v,&route,&wx,&wy);
    bot_prop_waypoint(v,&route,&wx,&wy);
    if(!bot_precise_travel(v,&route,wx-(double)v->x/65536.0,
                           wy-(double)v->y/65536.0,1,out))return 0;
    /* A continually used route remains live; only an abandoned memory
     * expires. Native collision watchdogs still control stalled travel. */
    s->travel_tick=v->tick;
    return 1;
}
/* A collision stall can still happen despite valid input (another actor,
 * animation or native SafePos). Track meaningful approach OR actual native
 * egg damage; tiny oscillations and holding FIRE without hits do not count.
 * Scope the cooldown to the observed actor generation and level. */
int avp_mp_bot_egg_avoided(const AvpMpBotState *s,unsigned level,
    unsigned slot,unsigned generation,uint64_t tick)
{
    return s && tick<s->egg_avoid_until && s->egg_avoid_level==level &&
           s->egg_avoid_slot==slot && s->egg_avoid_generation==generation;
}
void avp_mp_bot_egg_watch(AvpMpBotState *s,const AvpMpBotObservation *v,
    unsigned generation)
{
    if(!s || !v)return;
    const double d=hypot(((int64_t)v->target_x-v->x)/65536.0,
                         ((int64_t)v->target_y-v->y)/65536.0);
    if(v->player_species!=AVP_MP_PREDATOR ||
       !v->has_target || !v->hostile_target || !v->target_visible ||
       !v->target_is_egg || v->goal!=AVP_MP_BOT_HOSTILE ||
       !v->target_health_valid || v->target_health<=0 || d>2.0 ||
       v->facehugged || v->melee_threat ||
       v->retreat_to_refuge || v->retreat_to_teammate ||
       v->other_player_alien_trapped || v->other_player_swarmed){
        s->egg_watch_valid=0u;return;
    }
    if(!s->egg_watch_valid || s->egg_watch_level!=v->observed_world_level ||
       s->egg_watch_slot!=v->target_slot || s->egg_watch_generation!=generation ||
       v->tick<s->egg_watch_tick || v->target_health<s->egg_watch_health ||
       s->egg_watch_distance-d>=0.125){
        s->egg_watch_valid=1u;s->egg_watch_level=v->observed_world_level;
        s->egg_watch_slot=v->target_slot;s->egg_watch_generation=generation;
        s->egg_watch_health=v->target_health;s->egg_watch_distance=d;
        s->egg_watch_tick=v->tick;
    }else if(v->tick-s->egg_watch_tick>=40u){
        s->egg_avoid_level=v->observed_world_level;
        s->egg_avoid_slot=v->target_slot;s->egg_avoid_generation=generation;
        s->egg_avoid_until=v->tick+45u;s->egg_watch_valid=0u;
        s->evade_ticks=0u;s->immobile_ticks=0u;s->last_motion=0u;
    }
}
/* Stationary eggs do not need duelling/orbit tactics. Choose a legal
 * striking pose and hold it while the original weapon attacks. */
/* Allow the real weapon interpreter to act if the original fade state
 * fails to clear. Keep the normal aim/range/splash gates intact. */
static int bot_predator_fire_ready(const AvpMpBotState *s,const AvpMpBotObservation *v)
{
    return (!v->invis_act && !v->invis_stat && !v->invisflag) ||
           s->predator_decloak_wait>=8u;
}
static int bot_predator_egg_position(AvpMpBotState *s,
    const AvpMpBotObservation *v,AvpMpPlayerCommand *out,int precise)
{
    const double ax=v->x/65536.0,ay=v->y/65536.0;
    const double ex=v->target_x/65536.0,ey=v->target_y/65536.0;
    const double tx=ex-ax,ty=ey-ay,d=hypot(tx,ty);
    const double turn=angle_diff(v->heading,tx,ty);
    const int melee=bot_melee_weapon(v);
    double bx=ax,by=ay,best=1e30;
    if(!precise || v->player_species!=AVP_MP_PREDATOR ||
       !v->has_target || !v->hostile_target || !v->target_visible ||
       !v->target_is_egg || v->goal!=AVP_MP_BOT_HOSTILE || d>2.0 ||
       v->melee_threat || v->retreat_to_refuge ||
       v->retreat_to_teammate || v->other_player_alien_trapped ||
       v->other_player_swarmed || v->tick<s->damage_react_until ||
       v->cover_flank_valid || !nav_valid(&v->map))return 0;
    /* The host includes collision-enabled teammates in the same prop list.
     * A peer elsewhere in the 3-tile observation radius need not prevent
     * striking; a peer in front of the muzzle still gets the old policy. */
    if(v->nearby_peer_valid){
        const double px=v->nearby_peer_x/65536.0-ax;
        const double py=v->nearby_peer_y/65536.0-ay;
        const double along=(px*tx+py*ty)/fmax(d,0.00001);
        const double side=fabs(px*ty-py*tx)/fmax(d,0.00001);
        if(along>0.0 && along<d+0.25 && side<0.40)return 0;
        /* Don't bypass the conservative guard if the observation omitted
         * the peer collision square (e.g. saturated prop array). */
        unsigned peer_in_props=0u;
        for(unsigned i=0;i<v->prop_count;i++)
            if(v->props[i].x==v->nearby_peer_x && v->props[i].y==v->nearby_peer_y)
                peer_in_props=1u;
        if(!peer_in_props)return 0;
    }
    for(unsigned i=0;i<v->acid_count;i++)
        if(fabs(v->acid[i].x/65536.0-ax)<1.25 &&
           fabs(v->acid[i].y/65536.0-ay)<1.25)return 0;
    /* Check reach at intended facing, not the current turning angle. */
    const int in_reach=d>0.00001 &&
        fabs(tx)<fabs(tx/d*BOT_MELEE_REACH)+BOT_MELEE_SHOT_WIDTH-0.025 &&
        fabs(ty)<fabs(ty/d*BOT_MELEE_REACH)+BOT_MELEE_SHOT_WIDTH-0.025;
    if(melee && !in_reach){
        /* Search local attack poses and the complete approach segment. */
        for(unsigned r=0;r<3u;r++)for(unsigned i=0;i<32u;i++){
            const double a=(double)i*(2.0*BOT_PI/32.0);
            const double radius=0.48+0.06*r;
            const double x=ex+cos(a)*radius,y=ey+sin(a)*radius;
            const double len=hypot(x-ax,y-ay);
            if(len<0.00001 || len>=best || !bot_lane_wall_clear(&v->map,x,y) ||
               bot_prop_hit(v,x,y) || bot_acid_hits(v,ax,ay,x,y))continue;
            if(!bot_lane_segment_clear(v,ax,ay,x,y))continue;
            if(!bot_step_clear_probe(v,(x-ax)/len,(y-ay)/len,NULL,fmin(len,0.12)))continue;
            best=len;bx=x;by=y;
        }
        /* If already pressed into a conservative wall margin, a complete
         * segment will necessarily fail at sample zero. First recover toward
         * the current cell centre using a short, genuinely improving step. */
        if(best==1e30 && !bot_lane_wall_clear(&v->map,ax,ay)){
            const double dx=floor(ax)+0.5-ax,dy=floor(ay)+0.5-ay;
            const double len=hypot(dx,dy);
            if(len>0.00001){
                const double probe=fmin(len,0.12);
                const double x=ax+dx/len*probe,y=ay+dy/len*probe;
                if(bot_wall_margin_recovery(&v->map,ax,ay,x,y) &&
                   bot_step_clear_probe(v,dx/len,dy/len,NULL,probe) &&
                   !bot_acid_hits(v,ax,ay,x,y)){
                    best=probe;bx=x;by=y;
                }
            }
        }
        if(best==1e30){
            const unsigned ranged=(v->predator_owned&(1u<<3))?3u:
                                  (v->predator_owned&(1u<<2))?2u:0u;
            if(!ranged)return 0;
            if(v->tick%10u==0u)
                out->actions|=1u<<bot_weapon_cycle_direction(v->predator_owned,
                                                        v->current_weapon,ranged);
        }
    }
    emit_turn(turn,out);
    s->evade_ticks=0u;s->immobile_ticks=0u;
    s->previous_x=v->x;s->previous_y=v->y;s->last_motion=0u;
    /* Pass this checked short vector through the precise-input adapter
     * even though the egg itself is a nearby solid AMP. */
    out->analog_axis_mask=3u;
    if(melee && !in_reach && best<1e30){
        const double dx=(bx-ax)/best,dy=(by-ay)/best;
        const double a=(double)v->heading*(2.0*BOT_PI/4294967296.0);
        const double fx=cos(a),fy=-sin(a);
        const double throttle=fmin(0.65,fmax(0.20,best/0.50));
        out->analog_forward=(float)((dx*fx+dy*fy)*throttle);
        out->analog_strafe=(float)((-dx*fy+dy*fx)*throttle);
        if(fabs(out->analog_forward)>0.00001)
            out->actions|=1u<<(out->analog_forward>0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
        if(fabs(out->analog_strafe)>0.00001)
            out->actions|=1u<<(out->analog_strafe>0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT);
        s->last_motion=out->actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                                    (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT));
    }else if((!melee || (in_reach && bot_melee_source_shot_box(v,tx,ty))) &&
             fabs(turn)<0.10 && bot_predator_fire_ready(s,v))
        out->actions|=1u<<AVP_MP_FIRE;
    return 1;
}
static void bot_step_plan(AvpMpBotState *state,const AvpMpBotObservation *v,
                     AvpMpPlayerCommand *out,int precise)
{
    double dx,dy,turn;
    int need_forward,need_fire=0;
    unsigned tactical_strafe=0u,threat_movement=0u,melee_movement=0u;
    unsigned guerilla_movement=0u;
    unsigned splash_retreat_motion=0u;
    unsigned health_retreat_motion=AVP_MP_INPUT_ACTIONS;
    unsigned lure_retreat_motion=AVP_MP_INPUT_ACTIONS;
    unsigned teammate_retreat_motion=AVP_MP_INPUT_ACTIONS;
    unsigned teammate_retreat_second=AVP_MP_INPUT_ACTIONS;
    AvpMpBotRoute teammate_route={0};
    int melee_engage=0,lure_alien=0,alien_ranged_standoff=0;
    int alien_death_blocks_passage=0;
    int emergency_alien_kill=0,nonlethal_alien_shot=0;
    unsigned shotgun_egg_policy=0u;
    AvpMpBotRoute route={0};
    unsigned start_x,start_y,goal_x,goal_y;
    int map_route=0;
    if(!state||!v||!out)return;
    memset(out,0,sizeof(*out));
    if(!state->initialized){
        state->previous_x=v->x;state->previous_y=v->y;
        state->initialized=1;
    }
    /* Player Collision is a LIVE multiplayer option. An in-progress acid
     * passing-bay maneuver must not survive switching collisions OFF: the
     * other actor has ceased to be a native physical obstruction. */
    if(v->player_collision_disabled){
        state->peer_pass_valid=0u;
        state->peer_pass_arrived=0u;
        state->peer_pass_until=0u;
        state->peer_pass_retry_tick=0u;
        state->peer_yield_until=0u;
    }
    /* Clear any prior follow clearance after switching to combat, a vent,
     * elevator or a different objective, including early-return paths. */
    if(v->goal!=AVP_MP_BOT_FOLLOW || !v->has_target)
        state->follow_backoff_active=0u;
    bot_observe_damage(state,v);
    /* MAZESCRN.S::update_wep tests a fresh active-low JOY_LEFT/RIGHT
     * edge once per successful throw. Holding the direction does nothing.
     * The first neutral tick ensures a fresh edge even if the bot happened
     * to be sidestepping or turning when the Facehugger attached. Nine
     * presses, damage, recovery, and the Facehugger release all remain
     * exclusively in the unchanged original gameplay code. */
    if(v->facehugged){
        if(state->facehug_phase==1u){
            out->actions|=1u<<(v->facehug_direction<0 ?
                AVP_MP_STRAFE_RIGHT : AVP_MP_STRAFE_LEFT);
            state->facehug_phase=2u;
        }else state->facehug_phase=1u;
        state->previous_x=v->x;state->previous_y=v->y;
        state->immobile_ticks=0u;state->evade_ticks=0u;
        state->last_motion=0u;
        return;
    }
    state->facehug_phase=0u;
    if(v->player_species!=AVP_MP_PREDATOR)state->predator_decloak_wait=0u;
    shotgun_egg_policy=avp_mp_bot_shotgun_egg_policy(v);
    if(bot_recover_outside(state,v,out))return;
    /* An empty Marine cannot engage even if a late host objective or a
     * Deathmatch contact still says HOSTILE. Escape along a legal motion
     * axis instead of chasing or waiting to fire an empty gun. */
    if(v->player_species==AVP_MP_MARINE && v->marine_inventory_valid &&
       v->marine_unarmed && v->has_target &&
       (v->hostile_target || v->goal==AVP_MP_BOT_HOSTILE ||
        v->goal==AVP_MP_BOT_INVESTIGATE)){
        const double dx=((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        const double dy=((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        const unsigned motion=bot_empty_escape_motion(v,dx,dy);
        state->travel_valid=0u;
        emit_turn(angle_diff(v->heading,-dx,-dy),out);
        if(motion<AVP_MP_INPUT_ACTIONS)out->actions|=1u<<motion;
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=out->actions & ((1u<<AVP_MP_FORWARD)|
            (1u<<AVP_MP_BACKWARD)|(1u<<AVP_MP_STRAFE_LEFT)|
            (1u<<AVP_MP_STRAFE_RIGHT));
        return;
    }
    /* The source-owned Queen ending starts after the last generator dies.
     * Face her without issuing weapons or movement; the original AMP tick
     * will decide if/when the cutscene starts. */
    if(v->goal==AVP_MP_BOT_QUEEN_WAIT && v->has_target &&
       v->player_species==1u){
        emit_turn(angle_diff(v->heading,
            (double)((int64_t)v->target_x-(int64_t)v->x),
            (double)((int64_t)v->target_y-(int64_t)v->y)),out);
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=0u;state->immobile_ticks=0u;
        state->evade_ticks=0u;
        return;
    }
    /* Request an OPTION edge before attacking any observed hostile, not
     * just a six-tile contact. If the native fade is stuck, cease waiting
     * after eight observed ticks; the native weapon interpreter owns FIRE. */
    if(v->player_species==2u){
        const int visible=v->has_target && v->hostile_target && v->target_visible;
        const int near_fight=v->combat_enemies_nearby!=0u || visible;
        const int cloaked=v->invis_act || v->invis_stat || v->invisflag;
        int want_cloak;
        if(!visible || !cloaked)state->predator_decloak_wait=0u;
        else if(!state->predator_decloak_wait ||
                v->tick!=state->predator_decloak_tick){
            if(!state->predator_decloak_wait || v->tick!=state->predator_decloak_tick+1u)
                state->predator_decloak_wait=1u;
            else if(state->predator_decloak_wait<8u)state->predator_decloak_wait++;
        }
        state->predator_decloak_tick=v->tick;
        if(v->max_energy>0 && v->player_energy>0 &&
           v->player_energy*100 < v->max_energy*70 && v->medpak>0)
            out->actions|=1u<<AVP_MP_PRED_MEDKIT;
        if(near_fight)state->combat_hold_until=v->tick+12u;
        want_cloak=(v->marines_on_level>0u ||
                    (v->deathmatch && v->dm_opponents_on_level>0u)) &&
                   !near_fight && v->tick>=state->combat_hold_until;
        if(want_cloak && !cloaked)out->actions|=1u<<AVP_MP_PRED_CLOAK;
        if(!want_cloak && v->invis_act &&
           (!visible || (state->predator_decloak_wait<8u &&
                        (state->predator_decloak_wait&1u))))
            out->actions|=1u<<AVP_MP_PRED_CLOAK;
    }
    if(bot_predator_egg_position(state,v,out,precise))return;
    if(v->player_species!=1u || v->goal!=AVP_MP_BOT_COCOON ||
       !v->target_is_marine){
        state->cocoon_locked=0u;state->cocoon_phase=0u;
    }
    if(v->has_target){
        dx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        dy=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
    }else{
        double direction=(double)((v->tick/50u+v->player_index*3u)&7u)*(BOT_PI/4.0);
        dx=cos(direction);dy=-sin(direction);
    }
    /* RE44 R289: a FOLLOW target is a living teammate, not an obstacle.
     * When they walk up to the bot, move AWAY using authentic native player
     * controls, even if Player Collision is OFF. Keep a 1.0/1.55-cell
     * hysteresis band so a stationary player cannot make the bot jitter.
     * Do not steal movement from combat, real acid contact, or an active
     * corridor crossing. Following through vents/lifts uses distinct goals. */
    if(v->goal!=AVP_MP_BOT_FOLLOW || !v->has_target || v->hostile_target ||
       v->deathmatch || v->melee_threat || v->other_player_alien_trapped ||
       v->other_player_swarmed || state->acid_commit_ticks ||
       (v->player_species!=AVP_MP_ALIEN && v->acid_count &&
        bot_acid_hits(v,(double)v->x/65536.0,(double)v->y/65536.0,
                         (double)v->x/65536.0,(double)v->y/65536.0))){
        state->follow_backoff_active=0u;
    }else{
        const double dist2=dx*dx+dy*dy;
        if(dist2>=1.55*1.55)state->follow_backoff_active=0u;
        else if(dist2<=1.0*1.0)state->follow_backoff_active=1u;
        if(state->follow_backoff_active){
            const unsigned motion=bot_melee_retreat(v,dx,dy);
            if(motion<AVP_MP_INPUT_ACTIONS){
                /* Turn to keep watching the teammate while backing up;
                 * the current heading determines the legal translation. */
                emit_turn(angle_diff(v->heading,dx,dy),out);
                out->actions|=1u<<motion;
                state->previous_x=v->x;state->previous_y=v->y;
                state->last_motion=1u<<motion;
                return;
            }
        }
    }
    start_x=v->x>>16;start_y=v->y>>16;
    goal_x=v->target_x>>16;goal_y=v->target_y>>16;
    if(v->cover_flank_valid && v->has_target &&
       (v->goal==AVP_MP_BOT_HOSTILE || v->goal==AVP_MP_BOT_INVESTIGATE) &&
       !v->retreat_to_teammate &&
       !v->retreat_to_refuge){
        /* Navigation target only. Target position, optical visibility,
         * health and native FIRE rules remain those of the real enemy. */
        goal_x=v->cover_flank_x;goal_y=v->cover_flank_y;
    }
    /* Do not reach an invalid interaction cell and press Use forever.
     * Source DOORS.S rejects Aliens at lifts and Predators at ducts. */
    if(v->goal==AVP_MP_BOT_LIFT && v->player_species==1u)return;
    /* DOORS.S::do_lifts only jams one position/direction pair. Do not
     * blacklist the entire lift or either neighboring sub-level: its
     * other directions are still legitimate. Also reject a stale goal
     * before issuing Use if a caller ever bypasses the route selector. */
    if(v->goal==AVP_MP_BOT_LIFT &&
       avp_mp_bot_lift_jammed(v->current_level,
                             v->target_x>>16,v->target_y>>16,
                             v->lift_direction))return;
    if((v->goal==AVP_MP_BOT_EXIT || v->goal==AVP_MP_BOT_LIFT) &&
       v->current_level && v->destination_level &&
       !avp_mp_bot_transition_allowed(v->player_species,
                 v->current_level,v->destination_level,
                 v->goal==AVP_MP_BOT_LIFT?AVP_MP_BOT_TRANS_LIFT:
                 (v->exit_is_duct?AVP_MP_BOT_TRANS_DUCT:AVP_MP_BOT_TRANS_EXIT)))
        return;
    if(v->goal==AVP_MP_BOT_EXIT && v->exit_is_duct &&
       v->player_species==2u)return;
    /* Aliens take no acid damage; ignore the risk cost, not doors, props
       or wall collisions. This also covers independent test observations. */
    AvpMpBotMap species_map=v->map;
    if(v->player_species==1u)species_map.risk=NULL;
    if(state->nav_bad_until>v->tick && state->nav_bad_next_plus_one &&
       state->nav_bad_level==v->observed_world_level &&
       !(v->goal==AVP_MP_BOT_HOSTILE && v->target_visible) &&
       v->goal!=AVP_MP_BOT_COCOON &&
       /* Never avoid the actual authored panel destination. */
       state->nav_bad_next_plus_one!=nav_index(goal_x,goal_y)+1u){
        /* The original SafePos stopped the prior attempted cell crossing.
         * Try another ORIGINAL authored route for this bot only. If this
         * is a one-way corridor, fall back to the unmodified route and
         * let the native collision/recovery decide. */
        species_map.avoid_cell_plus_one=state->nav_bad_next_plus_one;
    }else species_map.avoid_cell_plus_one=0u;
    if(v->has_target && nav_valid(&species_map)){
        map_route=avp_mp_bot_route(&species_map,start_x,start_y,
                                  goal_x,goal_y,&route);
        if(!map_route && species_map.avoid_cell_plus_one){
            species_map.avoid_cell_plus_one=0u;
            map_route=avp_mp_bot_route(&species_map,start_x,start_y,
                                       goal_x,goal_y,&route);
        }
    }
    state->objective_route=map_route?1u:2u;
    if(map_route && route.requires_door_use && bot_door_animating(v,route.door_offset))
        state->objective_wait=1u;
    if(map_route){
        if(v->cover_flank_valid && start_x==goal_x && start_y==goal_y){
            /* Entering the selected cell is not the same as getting out
             * from behind a table. Reach the actual verified centre before
             * treating this known enemy as having an unobstructed lane. */
            dx=(double)goal_x+0.5-(double)v->x/65536.0;
            dy=(double)goal_y+0.5-(double)v->y/65536.0;
        }
        if(route.next_x!=start_x || route.next_y!=start_y){
            double wx,wy;
            bot_route_steer_point(v,&route,&wx,&wy);
            dx=wx-(double)v->x/65536.0;
            dy=wy-(double)v->y/65536.0;
            /* Only the TURN target is straightened; the route movement
             * still aims at the actual waypoint and obeys native collision. */
            (void)bot_straight_route_heading(v,&route,&dx,&dy);
        }
    }
    if(v->has_target && nav_valid(&v->map) && !map_route &&
       !(v->hostile_target && v->target_visible)){
        /* A disconnected or stale patrol target is not a wait condition.
         * Ask the host to choose another source-connected destination.
         * Leave lift/exit, supply and rescue ownership to their own logic. */
        if(v->goal==AVP_MP_BOT_PATROL)state->nav_retarget_patrol=1u;
        state->nav_anchor_valid=0u;
        /* A blocked waypoint must not make an injured bot completely
         * motionless. Reposition using safe native player commands while
         * scanning; never shoot at a hidden enemy or cross a solid wall. */
        bot_damage_react(state,v,out);
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=out->actions &
            ((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
             (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT));
        return;
    }
    /* R296 ship-airlock seating: side-bay actors face INWARD, away from
     * the wall behind their backs. Reaching the seat is not permission to
     * operate any panel. Never turn toward an imaginary enemy contact. */
    if(map_route && (v->goal==AVP_MP_BOT_AIRLOCK_SEAT ||
                 v->goal==AVP_MP_BOT_ESCAPE_SEAT) &&
       start_x==goal_x && start_y==goal_y && v->airlock_face_side<4u &&
       fabs((double)((int64_t)v->target_x-(int64_t)v->x)/65536.0)<0.135 &&
       fabs((double)((int64_t)v->target_y-(int64_t)v->y)/65536.0)<0.135){
        static const int xx[4]={-1,0,1,0},yy[4]={0,-1,0,1};
        emit_turn(angle_diff(v->heading,(double)xx[v->airlock_face_side],
                             (double)yy[v->airlock_face_side]),out);
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=0u;
        return;
    }
    /* Source DOORS.S requires facing the panel, not looking toward a remote
     * waypoint. At the target cell of an authored exit/lift, turn to panel. */
    if(map_route && start_x==goal_x && start_y==goal_y &&
       (v->goal==AVP_MP_BOT_LIFT||v->goal==AVP_MP_BOT_EXIT||
        v->goal==AVP_MP_BOT_MEDICAL ||
        v->goal==AVP_MP_BOT_ESCAPE_LAUNCH ||
        v->goal==AVP_MP_BOT_AIRLOCK_CLOSE)){
        static const int xdir[4]={-1,0,1,0},ydir[4]={0,-1,0,1};
        if(v->target_side>=4u)return;
        turn=angle_diff(v->heading,(double)xdir[v->target_side],
                        (double)ydir[v->target_side]);
        /* A grid-cell match alone is not safely INSIDE a lift or airlock. At
         * the doorway edge the original closing-door collision reverses
         * the door. Center the actor using the original movement buttons,
         * then stop translating BEFORE pressing the panel. */
        if(v->goal==AVP_MP_BOT_LIFT || v->goal==AVP_MP_BOT_AIRLOCK_CLOSE ||
           (v->goal==AVP_MP_BOT_EXIT && v->airlock_pressure)){
            const double cx=(double)goal_x+0.5-(double)v->x/65536.0;
            const double cy=(double)goal_y+0.5-(double)v->y/65536.0;
            if(fabs(cx)>0.115 || fabs(cy)>0.115){
                if(precise && (v->goal!=AVP_MP_BOT_LIFT || v->deathmatch) && state->physical_stall<10u){
                    const double len=hypot(cx,cy),probe=fmin(0.10,len);
                    const double x=(double)v->x/65536.0,y=(double)v->y/65536.0;
                    const double ux=cx/len,uy=cy/len;
                    if((v->goal!=AVP_MP_BOT_LIFT || bot_peer_precision_clear(v,ux,uy,0.68)) &&
                       bot_step_clear_probe(v,ux,uy,&route,probe) &&
                       bot_short_lane_clear(&v->map,x,y,x+ux*probe,y+uy*probe)){
                        const double a=v->heading*(2.0*BOT_PI/4294967296.0);
                        const double fx=cos(a),fy=-sin(a),throttle=fmin(0.55,len/0.60);
                        out->analog_forward=(float)((ux*fx+uy*fy)*throttle);
                        out->analog_strafe=(float)((-ux*fy+uy*fx)*throttle);
                        out->analog_axis_mask|=3u;
                        if(fabs(out->analog_forward)>0.00001)out->actions|=1u<<(out->analog_forward>0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
                        if(fabs(out->analog_strafe)>0.00001)out->actions|=1u<<(out->analog_strafe>0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT);
                        emit_turn(turn,out);
                        state->last_motion=out->actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|(1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT));
                        state->previous_x=v->x;state->previous_y=v->y;
                        return;
                    }
                }
                static const unsigned movement[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                    AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
                const double heading=(double)v->heading*(2.0*BOT_PI/4294967296.0);
                const double fx=cos(heading),fy=-sin(heading);
                const double mx[4]={fx,-fx,fy,-fy};
                const double my[4]={fy,-fy,-fx,fx};
                double best=0.04;
                unsigned chosen=AVP_MP_INPUT_ACTIONS;
                for(unsigned i=0u;i<4u;i++){
                    const double score=cx*mx[i]+cy*my[i];
                    if(state->physical_stall>=10u &&
                       (state->physical_stall/5u)%2u==0u &&
                       (state->last_motion&(1u<<movement[i])))continue;
                    if(score>best && (v->goal==AVP_MP_BOT_LIFT?
                        bot_safe_action(v,movement[i],&route):
                        bot_step_clear_probe(v,mx[i],my[i],&route,0.12))){
                        best=score;chosen=movement[i];
                    }
                }
                if(chosen<AVP_MP_INPUT_ACTIONS){
                    out->actions|=1u<<chosen;
                    state->last_motion=1u<<chosen;
                }else{
                    state->last_motion=0u;
                    if(state->physical_stall>=10u)emit_turn(angle_diff(v->heading,cx,cy),out);
                }
                state->previous_x=v->x;state->previous_y=v->y;
                return;
            }
        }
        /* Medical FIRST AID requires an actual panel-facing Use press.
         * The original DoorKeys and COMPUTER.S own the interaction. */
        if(v->goal==AVP_MP_BOT_MEDICAL ||
           v->goal==AVP_MP_BOT_ESCAPE_LAUNCH ||
           v->goal==AVP_MP_BOT_AIRLOCK_CLOSE){
            emit_turn(turn,out);
            if(fabs(turn)<BOT_PI/9.0 &&
               (v->goal!=AVP_MP_BOT_AIRLOCK_CLOSE ||
                v->airlock_door_ready) &&
               (state->last_exit_use==0u||v->tick-state->last_exit_use>=35u)){
                out->actions|=1u<<AVP_MP_USE;
                state->last_exit_use=v->tick+1u;
            }
            state->previous_x=v->x;state->previous_y=v->y;
            state->last_motion=0u;
            return;
        }
        emit_turn(turn,out);
        if(!v->lift_wait && !v->lift_follow_human && fabs(turn)<BOT_PI/9.0 &&
           (state->last_exit_use==0u||v->tick-state->last_exit_use>=35u)){
            out->actions|=1u<<AVP_MP_USE;
            if(v->goal==AVP_MP_BOT_LIFT)
                out->actions|=1u<<(v->lift_direction<0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
            state->last_exit_use=v->tick+1u;
        }
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=0u;
        return;
    }
    turn=angle_diff(v->heading,dx,dy);
    if(v->cover_flank_valid && v->has_target &&
       (v->goal==AVP_MP_BOT_HOSTILE || v->goal==AVP_MP_BOT_INVESTIGATE) &&
       !v->retreat_to_teammate)
        need_forward=start_x!=goal_x || start_y!=goal_y ||
            fabs((double)goal_x+0.5-(double)v->x/65536.0)>0.16 ||
            fabs((double)goal_y+0.5-(double)v->y/65536.0)>0.16;
    else if(!v->has_target)need_forward=1;
    else if(v->goal==AVP_MP_BOT_COCOON && v->target_is_marine){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        /* Live Marines: stop in actual melee reach for the claw/tail/claw
         * sequence. Stunned fallen Marines: physically approach the source
         * stun_death proximity square, then stay still until converted. */
        need_forward=!v->target_energy_positive ?
            (fabs(tx)>0.10 || fabs(ty)>0.10):
            (tx*tx+ty*ty>BOT_MELEE_HOLD_FAR*BOT_MELEE_HOLD_FAR);
    }
    else if(v->hostile_target){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        if(shotgun_egg_policy){
            /* Eggwait uses a one-cell axis-aligned native proximity test.
             * Stop well inside that range without entering the egg's
             * occupied center. Once the original egg mode starts opening,
             * hold position for two authentic shotgun hits. With fewer
             * than two shells, do not intentionally trigger an intact egg. */
            need_forward=shotgun_egg_policy==1u &&
                (fabs(tx)>0.80 || fabs(ty)>0.80);
        }else if(v->health_low && v->target_visible &&
                 v->player_species!=AVP_MP_ALIEN && !bot_melee_weapon(v)){
            /* At 60% or less, preserve firing distance instead of rushing
             * within the source melee contact square. Actual threats may
             * still force a native close-range defensive shot. */
            need_forward=0;
        }else if(v->behavior==1u && !v->marine_low_ammo && !v->health_low)
            /* Berserker must also yield when severely injured. */
            need_forward=!v->target_visible || tx*tx+ty*ty>0.36*0.36;
        else if(v->behavior==2u && v->player_species!=1u &&
                (v->player_species==0u ||
                 (v->predator_owned&((1u<<2)|(1u<<3)))!=0u))
            need_forward=!v->target_visible && tx*tx+ty*ty>4.5*4.5;
        else need_forward=!v->target_visible || tx*tx+ty*ty>1.6*1.6;
    }else if(v->goal==AVP_MP_BOT_COCOON){
        /* AMP.S::stun_death uses amp_in_range(20), which means the
         * player's XY must be within 20<<9 (=0.15625 cell) on BOTH axes.
         * Merely entering the same maze tile cannot create a cocoon. */
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        need_forward=fabs(tx)>0.10 || fabs(ty)>0.10;
    }else if(v->goal==AVP_MP_BOT_HEALTH ||
             v->goal==AVP_MP_BOT_AIRLOCK_SEAT ||
             v->goal==AVP_MP_BOT_ESCAPE_SEAT ||
             v->goal==AVP_MP_BOT_AIRLOCK_CLOSE ||
             v->goal==AVP_MP_BOT_SUPPLY ||
             v->goal==AVP_MP_BOT_MEDICAL ||
             v->goal==AVP_MP_BOT_ESCAPE_LAUNCH ||
             v->goal==AVP_MP_BOT_EXIT ||
             v->goal==AVP_MP_BOT_LIFT)
        need_forward=(start_x!=goal_x || start_y!=goal_y ||
             ((v->goal==AVP_MP_BOT_AIRLOCK_SEAT ||
               v->goal==AVP_MP_BOT_ESCAPE_SEAT) &&
              (fabs((double)((int64_t)v->target_x-(int64_t)v->x)/65536.0)>0.135 ||
               fabs((double)((int64_t)v->target_y-(int64_t)v->y)/65536.0)>0.135)));
    else{
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        /* R10 patrol controller only completes at <0.70 tiles along BOTH
         * axes. The old generic 1.8-tile stop radius stopped movement far
         * OUTSIDE that arrival threshold: stand still -> 25-tick timeout
         * -> new goal -> repeat. Patrol is travel, not combat positioning.
         * Continue to the actual destination until the host sees arrival. */
        if(v->goal==AVP_MP_BOT_PATROL)
            need_forward=fabs(tx)>0.50 || fabs(ty)>0.50;
        else need_forward=tx*tx+ty*ty>1.8*1.8;
    }
    /* Head-on player contention outranks routine follow/supply navigation
     * but never an actively visible hostile. Both bots continue to obey
     * the original native player collisions; one gets a real side bay. */
    if(!v->deathmatch && v->player_species!=AVP_MP_ALIEN &&
       bot_acid_peer_yield(state,v,map_route?&route:NULL,
                           need_forward,out))return;
    /* Stay OUTSIDE the ORIGINAL alien death-splash square while using a
     * ranged gun. Only a visible living target qualifies; do not turn the
     * ordinary floor acid into a wider barrier or flee every Alien when
     * pursuing an exit. An unlocked ranged Predator can retreat WHILE
     * switching away from melee. A melee-only Predator must still attack. */
    alien_ranged_standoff=v->behavior!=1u && v->hostile_target && v->target_is_alien &&
        v->target_visible && v->goal==AVP_MP_BOT_HOSTILE &&
        (v->player_species==0u ||
         (v->player_species==2u &&
          (v->predator_owned&((1u<<2)|(1u<<3)))!=0u));
    if(alien_ranged_standoff){
        const int64_t tx=(int64_t)v->target_x-(int64_t)v->x;
        const int64_t ty=(int64_t)v->target_y-(int64_t)v->y;
        /* Safe once EITHER axis reaches 0.76 tile. The original splash
         * is a square, not a circular Euclidean range. */
        if(llabs(tx)<BOT_ALIEN_DEATH_STANDOFF_UNITS &&
           llabs(ty)<BOT_ALIEN_DEATH_STANDOFF_UNITS){
            unsigned motion=bot_melee_retreat(v,(double)tx/65536.0,
                                               (double)ty/65536.0);
            need_forward=0;
            if(motion<AVP_MP_INPUT_ACTIONS)
                splash_retreat_motion=motion+1u;
        }
    }
    /* R283: wounded ranged bots disengage earlier, not merely at melee
     * reach. Prefer original collision-tested BACKWARD/STRAFE input while
     * retaining sight and weapons; no teleport, health regeneration, or
     * widened attack range. Do not abandon immediate co-op rescues. */
    if(v->health_low && v->hostile_target && v->target_visible &&
       v->goal==AVP_MP_BOT_HOSTILE &&
       v->player_species!=AVP_MP_ALIEN && !bot_melee_weapon(v) &&
       !v->other_player_alien_trapped && !v->other_player_swarmed){
        const double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        const double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        const double max_distance=v->health_critical?7.0:5.0;
        if(tx*tx+ty*ty<max_distance*max_distance)
            health_retreat_motion=bot_lure_retreat_motion(v,tx,ty);
    }
    /* Melee positioning is meaningful only for a visible, reachable enemy.
     * If too far, approach; if too close, retreat without turning the back
     * on the enemy; at good strike distance strafe/fire.  The attack itself
     * still goes through the original weapon animation and hit test. */
    melee_engage=v->hostile_target && v->target_visible &&
                 !alien_ranged_standoff &&
                 bot_melee_weapon(v) && v->goal!=AVP_MP_BOT_COCOON;
    if(melee_engage && v->behavior!=1u){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        double distance2=tx*tx+ty*ty;
        if(distance2<BOT_MELEE_HOLD_NEAR*BOT_MELEE_HOLD_NEAR){
            unsigned motion=bot_melee_retreat(v,tx,ty);
            need_forward=0;
            if(motion<AVP_MP_INPUT_ACTIONS)melee_movement=motion+1u;
        }else if(distance2<=BOT_MELEE_HOLD_FAR*BOT_MELEE_HOLD_FAR){
            unsigned preferred=((v->tick/12u+v->player_index)&1u)?
                AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
            unsigned alternate=preferred==AVP_MP_STRAFE_LEFT?
                AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
            need_forward=0;
            if(bot_safe_action(v,preferred,NULL))melee_movement=preferred+1u;
            else if(bot_safe_action(v,alternate,NULL))melee_movement=alternate+1u;
        }else need_forward=1;
    }
    /* R274: A lethal shot on a living source Alien in a single-width
     * corridor can permanently seal the only route with its original acid
     * contact square. Luring is movement strategy, NOT an authorization to
     * kill when a timer expires or a retreat is blocked. Check the actual
     * source map and Alien position on EVERY combat tick, independently of
     * health, difficulty/behavior profile, or the lure cooldown. This
     * cannot affect native acid damage, enemy AI or player weapons. */
    alien_death_blocks_passage=!v->evacuation_active &&
       v->hostile_target && v->target_is_alien &&
       v->target_slot<AVP_NUM_AMPS && v->goal==AVP_MP_BOT_HOSTILE &&
       v->player_species!=1u && nav_valid(&v->map) &&
       !avp_mp_bot_alien_death_passable(&v->map,v->target_x,v->target_y);
    /* R275: luring cannot expire while an Alien is still standing in a
     * corridor where its original death acid would seal the only passage.
     * R274 vetoed the shot but kept R117's 150-tick retreat / 300-tick
     * cooldown, so the shooter resumed approaching its still-unsafe
     * target. Persist the observed target until it moves to a safe lane.
     * A never-engaged distant Alien is still approached to gain its notice;
     * after retreat begins, do not advance straight back into the choke.
     * The bot may only request source collision-tested movement buttons. */
    if(!alien_death_blocks_passage)
        state->lure_target_slot=0u;
    if(alien_death_blocks_passage && v->target_visible){
        const double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        const double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        const double distance2=tx*tx+ty*ty;
        if(distance2<=6.0*6.0 ||
           state->lure_target_slot==v->target_slot+1u){
            state->lure_target_slot=v->target_slot+1u;
            /* If the retreat is physically blocked, WAIT or evade; never
             * make forward pursuit the default while its death is unsafe.
             * At very close range the splash/melee escape overrides below
             * retain their original higher survival priority. */
            need_forward=0;
            if(distance2<=6.0*6.0){
                lure_retreat_motion=bot_lure_retreat_motion(v,tx,ty);
                lure_alien=lure_retreat_motion<AVP_MP_INPUT_ACTIONS;
            }
        }
    }
    /* R275: the host checks exact live AMP energy, source direct-hit damage,
     * and same-tick bot reservations before authorizing nonlethal Fire.
     * Respect that single shared decision: rechecking health here would
     * inadvertently bypass reservations made by another bot this tick. */
    nonlethal_alien_shot=alien_death_blocks_passage &&
                         v->nonlethal_choke_fire && v->target_visible;
    /* Self preservation and a trapped teammate override BOTH prohibitions.
     * Emergency close contact alone is not a license for a distant fatal
     * shot. Check collision-tested retreat for the *current* source target.
     * The ordinary aim/LOS/weapon rules remain authoritative. */
    if(v->target_is_alien && v->target_slot<AVP_NUM_AMPS &&
       v->hostile_target && v->target_visible){
        const double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        const double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        const double distance2=tx*tx+ty*ty;
        const int unable_to_retreat=
             bot_lure_retreat_motion(v,tx,ty)==AVP_MP_INPUT_ACTIONS ||
             (state->immobile_ticks>=4u && state->last_motion &&
              v->melee_visible && v->melee_threat==AVP_MP_BOT_THREAT_ALIEN &&
              llabs((long long)(int64_t)v->x-(long long)(int64_t)state->previous_x)<128 &&
              llabs((long long)(int64_t)v->y-(long long)(int64_t)state->previous_y)<128);
        const int self_pinned=fabs(tx)<0.90 && fabs(ty)<0.90 &&
             unable_to_retreat;
        /* R277: several actually DETECTED nearby Aliens are already a
         * lethal emergency before the single-target 0.9-tile pin threshold.
         * The old veto let an armed Marine be torn apart merely because a
         * mathematically legal sidestep existed. Do not exempt a lone,
         * distant corridor Alien: normal chip-and-lure safety still applies. */
        const int overwhelmed=v->close_alien_contacts>=2u &&
             distance2<=2.5*2.5;
        const int under_attack=v->tick<state->damage_react_until &&
             distance2<=1.65*1.65 && v->close_alien_contacts!=0u;
        emergency_alien_kill=self_pinned || overwhelmed || under_attack ||
                             v->other_player_alien_trapped ||
                             v->other_player_swarmed;
        if(emergency_alien_kill){
            lure_alien=0;
            /* R288: R275 cleared need_forward before it discovered that
             * another player was trapped. Merely releasing the FIRE veto
             * left the rescuer standing at the acid bottleneck, even with
             * an original legal route to the two Aliens pinning that player.
             * Restore actual route pursuit while outside melee/splash range;
             * a source-authorized MovePlayer step still decides collision. */
            if((v->other_player_alien_trapped || v->other_player_swarmed) &&
               !self_pinned && map_route &&
               (route.next_x!=start_x || route.next_y!=start_y) &&
               distance2>2.25*2.25)need_forward=1;
            if(self_pinned)need_forward=0;
        }
    }
    /* If damaged, the existing real-hit evasion can override the lure's
     * chosen motion. The hard unsafe-Alien FIRE veto remains independent. */
    if(lure_alien && v->tick<state->damage_react_until)lure_alien=0;
    if(v->hostile_target){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        double rawturn=angle_diff(v->heading,tx,ty);
        int melee=bot_melee_weapon(v);
        /* Guerilla ranged FIRE has no tactical distance/lure gate. LOS and
         * aim still matter; actual damage, ammo, equip/cloak state and hit
         * geometry remain the untouched original weapon implementation. */
        if(!v->marine_unarmed &&
           (!lure_alien || nonlethal_alien_shot || emergency_alien_kill) &&
           v->target_visible &&
           (melee ? bot_melee_source_shot_box(v,tx,ty) :
                    (v->behavior==2u ||
                     tx*tx+ty*ty<=
                        ((v->player_species==0u &&
                          (v->current_weapon==1u || v->current_weapon==2u ||
                           v->current_weapon==4u)) ? 8.25*8.25 : 6.0*6.0))) &&
           ((v->player_species==0u && !v->deathmatch && !melee &&
             (v->current_weapon==1u || v->current_weapon==2u ||
              v->current_weapon==4u)) ?
             avp_mp_bot_marine_aim_ready(v->current_weapon,rawturn,
                 sqrt(tx*tx+ty*ty),emergency_alien_kill) :
             fabs(rawturn)<(tx*tx+ty*ty<2.25 ? BOT_PI/6.0 : BOT_PI/11.0)) &&
           (v->player_species!=2u ||
            bot_predator_fire_ready(state,v)))
            need_fire=1;
        /* A bot must not be the cause of a death spray hitting a living
         * teammate. The observation covers ALL same-world human/bot actors
         * regardless of player-collision setting and excludes acid-immune
         * Alien players. Even Berserker/Guerilla or a pinned bot must wait
         * until everyone leaves the source spray range. The ordinary
         * danger/retreat movement remains active while the gun is held. */
        /* Never shoot a dormant shotgun-only egg. The real AMP switches
         * off eggwait when proximity has triggered its opening sequence;
         * only then may the bot fire. No special egg damage is invented. */
        if(shotgun_egg_policy==1u || shotgun_egg_policy==3u)
            need_fire=0;
        if(v->target_is_alien && v->target_slot<AVP_NUM_AMPS &&
           v->other_player_splash_unsafe && !v->evacuation_active &&
           !emergency_alien_kill && !nonlethal_alien_shot)need_fire=0;
        /* No damage from a bot's weapon may create an impassable one-way
         * acid barrier simply because the lure window expired, the bot
         * was injured, or it cannot retreat. All Marine/Predator behavior
         * profiles share this hard veto; aiming/movement are unchanged. */
        if(alien_death_blocks_passage &&
           !emergency_alien_kill && !nonlethal_alien_shot)need_fire=0;
    }
    /* R277: Retreat toward an actually reachable SAME-WORLD teammate while
     * keeping the sighted Alien in the original aiming/fire logic. A local
     * sidestep never navigated toward help. Use the already-verified floor,
     * access, door, acid and prop route planner; no teleports or extra speed.
     * Do not begin a closed-door interaction while under immediate attack. */
    if(v->player_species==0u &&
       ((v->retreat_to_teammate && v->hostile_target && v->target_visible) ||
        v->retreat_to_refuge) && nav_valid(&species_map) &&
       avp_mp_bot_route(&species_map,start_x,start_y,
           (v->retreat_to_refuge?v->retreat_refuge_x:
                                  v->retreat_teammate_x)>>16,
           (v->retreat_to_refuge?v->retreat_refuge_y:
                                  v->retreat_teammate_y)>>16,
           &teammate_route) && teammate_route.valid &&
       !teammate_route.requires_door_use &&
       (teammate_route.next_x!=start_x || teammate_route.next_y!=start_y)){
        teammate_retreat_motion=bot_route_motion(v,&teammate_route,0u);
        if(teammate_retreat_motion<AVP_MP_INPUT_ACTIONS)
            teammate_retreat_second=bot_route_second_motion(v,
                &teammate_route,teammate_retreat_motion);
    }
    /* Melee evasions take priority over ordinary approach only when a
     * living source Alien/Predator/Queen/Facehugger is actually in sight.
     * The margin is larger than the original sub-cell strike distance so
     * movement can start before the next 10-Hz combat tick. */
    if((v->behavior!=1u || v->marine_unarmed || v->marine_low_ammo || v->health_low) &&
       v->melee_threat && v->melee_visible &&
       !(v->goal==AVP_MP_BOT_COCOON && v->target_is_marine) &&
       !(melee_engage &&
         llabs((long long)(int64_t)v->target_x-(long long)(int64_t)v->melee_x)<8192 &&
         llabs((long long)(int64_t)v->target_y-(long long)(int64_t)v->melee_y)<8192)){
        /* Do not evade the SAME enemy we are deliberately dueling at melee
         * reach.  Other nearby threats still override melee positioning. */
        double tx=(double)((int64_t)v->melee_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->melee_y-(int64_t)v->y)/65536.0;
        double distance2=tx*tx+ty*ty;
        double limit=v->melee_threat==AVP_MP_BOT_THREAT_QUEEN?3.0:
                     v->melee_threat==AVP_MP_BOT_THREAT_FACEHUG?2.6:2.25;
        if(distance2<limit*limit &&
           v->goal!=AVP_MP_BOT_EXIT && v->goal!=AVP_MP_BOT_LIFT){
            /* Aliens need melee reach to stun/cocoon; strafe rather than
             * permanently fleeing their own intended melee encounter. */
            double retreat=angle_diff(v->heading,-tx,-ty);
            unsigned candidates[4];
            if(v->player_species==1u){
                candidates[0]=(v->tick/8u+v->player_index)%2u?
                    AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
                candidates[1]=candidates[0]==AVP_MP_STRAFE_LEFT?
                    AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
                candidates[2]=AVP_MP_BACKWARD;candidates[3]=AVP_MP_FORWARD;
            }else if(fabs(retreat)<BOT_PI/4.0){
                candidates[0]=AVP_MP_FORWARD;
                candidates[1]=retreat>0?AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
                candidates[2]=candidates[1]==AVP_MP_STRAFE_LEFT?
                    AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
                candidates[3]=AVP_MP_BACKWARD;
            }else if(fabs(retreat)>3.0*BOT_PI/4.0){
                candidates[0]=AVP_MP_BACKWARD;
                candidates[1]=retreat>0?AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
                candidates[2]=candidates[1]==AVP_MP_STRAFE_LEFT?
                    AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
                candidates[3]=AVP_MP_FORWARD;
            }else{
                candidates[0]=retreat>0?AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
                candidates[1]=AVP_MP_BACKWARD;
                candidates[2]=AVP_MP_FORWARD;
                candidates[3]=candidates[0]==AVP_MP_STRAFE_LEFT?
                    AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
            }
            if(v->player_species==AVP_MP_MARINE && v->marine_unarmed){
                const unsigned escape=bot_empty_escape_motion(v,tx,ty);
                if(escape<AVP_MP_INPUT_ACTIONS)threat_movement=escape+1u;
            }else for(unsigned n=0;n<4u;n++)if(bot_safe_action(v,candidates[n],NULL)){
                threat_movement=candidates[n]+1u;break;
            }
            /* R275: the target itself is the incoming melee Alien. If
             * its death would seal this passage, never let the generic
             * threat fallback advance INTO it when no retreat exists.
             * Unrelated melee threats keep their usual evasion behavior. */
            if(alien_death_blocks_passage &&
               state->lure_target_slot==v->target_slot+1u &&
               v->melee_threat==AVP_MP_BOT_THREAT_ALIEN &&
               llabs((long long)(int64_t)v->melee_x-(long long)(int64_t)v->target_x)<16384 &&
               llabs((long long)(int64_t)v->melee_y-(long long)(int64_t)v->target_y)<16384){
                threat_movement=lure_retreat_motion<AVP_MP_INPUT_ACTIONS?
                    lure_retreat_motion+1u:0u;
            }
        }
    }
    /* While fighting at a distance, sidestep and shoot instead of always
     * walking straight into the enemy. If a wall/acid makes one flank
     * unsafe, attempt the other; never assert two opposite sidestep bits. */
    if(!shotgun_egg_policy && v->behavior!=1u && !threat_movement &&
       !melee_engage && v->goal!=AVP_MP_BOT_COCOON &&
       v->hostile_target && v->target_visible && !v->cover_flank_valid &&
       v->goal==AVP_MP_BOT_HOSTILE){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        double distance2=tx*tx+ty*ty;
        double range=v->player_species==1u?2.0:6.0;
        if(distance2<range*range){
            unsigned preferred=((v->tick/12u+v->player_index)&1u)?
                AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
            unsigned alternate=preferred==AVP_MP_STRAFE_LEFT?
                AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
            if(bot_safe_action(v,preferred,NULL))tactical_strafe=preferred+1u;
            else if(bot_safe_action(v,alternate,NULL))tactical_strafe=alternate+1u;
        }
    }
    /* Guerilla closes when too far away, backpedals when too close, and
     * flanks inside the preferred band. These movement decisions NEVER
     * suppress Fire while a ranged target is visible and aimed at. Melee
     * retains its exact reach gate; emergency movement still takes priority. */
    /* R275: when a source Alien's death is still fatal to the passage,
     * a physically pinned Guerilla must not fall through to its ordinary
     * FORWARD combat-approach fallback after the lure cannot retreat. */
    if(!shotgun_egg_policy && v->behavior==2u &&
       v->hostile_target && v->target_visible &&
       !v->cover_flank_valid &&
       v->goal==AVP_MP_BOT_HOSTILE && !lure_alien &&
       !(alien_death_blocks_passage &&
         state->lure_target_slot==v->target_slot+1u)){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        const int back=bot_target_back_turned(v,tx,ty);
        unsigned motion=bot_guerilla_motion(state,v,tx,ty,back);
        if(motion<AVP_MP_INPUT_ACTIONS)guerilla_movement=motion+1u;
        tactical_strafe=0u; /* one coherent tactical motion, not opposites */
        /* Do not let the old ranged-only stop radius stall a melee bot
         * when a target gives it a clear rear approach. */
        if(back && bot_melee_weapon(v) &&
           tx*tx+ty*ty>BOT_MELEE_HOLD_FAR*BOT_MELEE_HOLD_FAR)
            need_forward=1;
        else if(!bot_melee_weapon(v))need_forward=0;
    }
    /* A blocked corridor requires steering via ORIGINAL MovePlayer. For a
     * closed, accessible authored door, press Use once then let its original
     * animation advance before trying the boundary again. */
    if(map_route && route.requires_door_use &&
       !(v->hostile_target && v->target_visible) &&
       (route.next_x!=start_x||route.next_y!=start_y)){
        uint16_t cell=(uint16_t)nav_index(start_x,start_y),
                 next=(uint16_t)nav_index(route.next_x,route.next_y);
        emit_turn(turn,out);
        if(!state->door_wait_valid || state->door_wait_level!=v->observed_world_level ||
           state->door_cell!=cell || state->door_next!=next || v->tick<state->door_wait_tick){
            if(state->door_wait_level!=v->observed_world_level ||
               state->door_cell!=cell || state->door_next!=next ||
               v->tick<state->door_wait_tick){
                state->door_attempts=0;state->door_cooldown=0;
            }
            state->door_cell=cell;state->door_next=next;
            state->door_wait_valid=1u;
            state->door_wait_level=v->observed_world_level;
            state->door_wait_best=route.door_value;
            state->door_wait_tick=v->tick;
        }else if(route.door_value>state->door_wait_best){
            /* Only actual opening progress earns time. Repeated USE,
             * animation flags and reopening to the same byte do not. */
            state->door_wait_best=route.door_value;
            state->door_wait_tick=v->tick;
        }else if(v->tick-state->door_wait_tick>=60u &&
                 v->goal!=AVP_MP_BOT_AIRLOCK_SEAT && v->goal!=AVP_MP_BOT_AIRLOCK_CLOSE &&
                 v->goal!=AVP_MP_BOT_ESCAPE_SEAT && v->goal!=AVP_MP_BOT_ESCAPE_LAUNCH){
            state->nav_bad_next_plus_one=(unsigned)next+1u;
            state->nav_bad_level=v->observed_world_level;
            state->nav_bad_until=v->tick+110u;
            if(v->goal==AVP_MP_BOT_PATROL)state->nav_retarget_patrol=1u;
            state->door_wait_tick=v->tick;
        }
        if(fabs(turn)<BOT_PI/9.0 && route.door_value==0u &&
           !bot_door_animating(v,route.door_offset) &&
           (!state->door_cooldown || v->tick>=state->door_cooldown) &&
           (!v->door_claims || avp_mp_bot_door_claim(v->door_claims,
                 v->map.cells,route.door_offset,v->tick))){
            out->actions|=1u<<AVP_MP_USE;
            state->last_door_use=v->tick;
            state->door_cooldown=(unsigned)(v->tick+26u);
            if(state->door_attempts<100u)state->door_attempts++;
        }
        state->immobile_ticks=0;
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=0u;
        return;
    }
    state->door_wait_valid=0u;
    /* Watch ORIGINAL position changes, not chosen movement commands only.
     * At a tight corner a bot may only turn or try a command that is
     * conservatively rejected; neither can excuse indefinite wall facing.
     * Track intermediate travel cells, not actual panel interaction. */
    if(map_route && need_forward &&
       (v->goal==AVP_MP_BOT_PATROL || v->goal==AVP_MP_BOT_INVESTIGATE ||
        v->goal==AVP_MP_BOT_SUPPLY ||
        v->goal==AVP_MP_BOT_HEALTH || v->goal==AVP_MP_BOT_MEDICAL ||
        v->goal==AVP_MP_BOT_FOLLOW ||
        v->goal==AVP_MP_BOT_EXIT || v->goal==AVP_MP_BOT_LIFT ||
        (v->goal==AVP_MP_BOT_HOSTILE && !v->target_visible)) &&
       !route.requires_door_use && (route.next_x!=start_x || route.next_y!=start_y)){
        const unsigned dest=nav_index(route.next_x,route.next_y)+1u;
        const unsigned cell=nav_index(start_x,start_y)+1u;
        /* Goal changes are NOT route loops. A legitimate dead-end return
         * may revisit a junction once; only an exact repeated corridor
         * sequence on the SAME goal is evidence of A-B-C-D-A-B-C-D. */
        /* Retargeting is not physical progress. Keep the local timeout
         * across changing objectives/hops in the same cell; only the
         * repeated-cell history is goal-specific. */
        if(!state->nav_anchor_valid || state->nav_anchor_level!=v->observed_world_level ||
           v->tick<state->nav_anchor_tick || v->tick-state->nav_anchor_seen>1u){
            state->nav_anchor_x=v->x;state->nav_anchor_y=v->y;
            state->nav_anchor_tick=v->tick;
            state->nav_anchor_valid=1u;
            state->nav_anchor_level=v->observed_world_level;
            state->nav_anchor_cell=cell;
            state->nav_anchor_next=0u;
            state->nav_recent_count=0u;
        }
        if(state->nav_goal_x!=goal_x || state->nav_goal_y!=goal_y){
            state->nav_goal_x=goal_x;state->nav_goal_y=goal_y;
            state->nav_recent_count=0u;
        }
        /* A repeated A/B boundary bounce is not progress even when the
         * host alternates pickup/patrol destinations. Track it independently
         * from the goal-specific long-loop history. */
        if(state->nav_bounce_level!=v->observed_world_level ||
           v->tick<state->nav_bounce_seen || v->tick-state->nav_bounce_seen>1u){
            state->nav_bounce_last=state->nav_bounce_other=state->nav_bounce_count=0u;
        }
        state->nav_bounce_level=v->observed_world_level;
        state->nav_bounce_seen=v->tick;
        if(cell!=state->nav_bounce_last){
            if(cell==state->nav_bounce_other && v->tick-state->nav_bounce_tick<=12u){
                state->nav_bounce_count++;
            }else state->nav_bounce_count=0u;
            state->nav_bounce_other=state->nav_bounce_last;
            state->nav_bounce_last=cell;
            state->nav_bounce_tick=v->tick;
            if(state->nav_bounce_count>=6u && v->goal!=AVP_MP_BOT_FOLLOW){
                state->nav_bad_next_plus_one=dest;
                state->nav_bad_level=v->observed_world_level;
                state->nav_bad_until=v->tick+110u;
                if(v->goal==AVP_MP_BOT_PATROL)state->nav_retarget_patrol=1u;
                state->nav_bounce_count=0u;
                if(v->player_species!=AVP_MP_ALIEN && v->acid_count && bot_hazards_close(v))
                    state->acid_commit_ticks=46u;
            }
        }
        state->nav_anchor_seen=v->tick;
        if(!state->nav_recent_count || state->nav_recent_cell[0]!=cell){
            unsigned repeat=0u;
            for(unsigned i=31u;i>0u;i--)
                state->nav_recent_cell[i]=state->nav_recent_cell[i-1u];
            state->nav_recent_cell[0]=cell;
            if(state->nav_recent_count<32u)state->nav_recent_count++;
            /* Two identical sequences of 2..12 *distinct moves*, such as
             * a six-cell circuit of the Training Maze, are not progress. */
            for(unsigned length=2u;length<=12u &&
                2u*length<=state->nav_recent_count;length++){
                unsigned equal=1u;
                for(unsigned k=0;k<length;k++)
                    if(state->nav_recent_cell[k]!=
                       state->nav_recent_cell[k+length]){equal=0u;break;}
                if(equal){repeat=1u;break;}
            }
            if(repeat){
                state->nav_bad_next_plus_one=dest;
                state->nav_bad_level=v->observed_world_level;
                state->nav_bad_until=v->tick+(v->observed_world_level==4u?180u:110u);
                if(v->goal==AVP_MP_BOT_PATROL)state->nav_retarget_patrol=1u;
                state->nav_recent_count=0u;
            }
        }
        {
            /* Re-evaluate the same physical anchor against the CURRENT
             * hop. A new destination from a moving teammate or alternating
             * A* tie does not buy time unless we actually approach it. */
            const double nx=(double)route.next_x+0.5,ny=(double)route.next_y+0.5;
            const double remaining=hypot(nx-v->x/65536.0,ny-v->y/65536.0);
            const double anchored=hypot(nx-state->nav_anchor_x/65536.0,
                                        ny-state->nav_anchor_y/65536.0);
            state->nav_anchor_next=dest;
            if(state->nav_anchor_cell!=cell || remaining+0.125<=anchored){
                state->nav_anchor_cell=cell;
                state->nav_anchor_x=v->x;state->nav_anchor_y=v->y;
                state->nav_anchor_tick=v->tick;
            }else if(v->tick-state->nav_anchor_tick>=
                     (v->observed_world_level==4u?75u:45u)){
                state->nav_bad_next_plus_one=dest;
                state->nav_bad_level=v->observed_world_level;
                state->nav_bad_until=v->tick+110u;
                state->nav_anchor_tick=v->tick;
                state->nav_anchor_x=v->x;state->nav_anchor_y=v->y;
                if(v->goal==AVP_MP_BOT_PATROL)state->nav_retarget_patrol=1u;
            }

        }
    }else state->nav_anchor_valid=0u;
    /* R293: A legal navigation step near acid owns steering until an
     * actual visible/melee danger interrupts it. A previous generic wall
     * recovery can otherwise keep issuing TURN_RIGHT for 19 ticks after
     * the route has become reachable -- the reported 360-degree spin.
     * Clearing the old recovery does not waive SafePos, doors or acid. */
    const int acid_route_steady=map_route && need_forward &&
        v->player_species!=AVP_MP_ALIEN && v->acid_count &&
        bot_hazards_close(v) && !route.requires_door_use &&
        (route.next_x!=start_x || route.next_y!=start_y) &&
        !(v->hostile_target && v->target_visible) && !v->melee_threat &&
        !splash_retreat_motion && !threat_movement;
    if(acid_route_steady)state->evade_ticks=0u;
    /* Track actual progress toward the current physical corridor waypoint,
     * NOT a changing follow target and NOT a little sideways bounce. Both
     * pressing a movement key that SafePos rejects and issuing NO movement
     * at all count toward this timer. */
    if(map_route && need_forward && v->player_species!=1u &&
       v->acid_count && bot_hazards_close(v) &&
       !route.requires_door_use &&
       (route.next_x!=start_x || route.next_y!=start_y) &&
       (!(v->hostile_target && v->target_visible) ||
        v->other_player_alien_trapped || v->other_player_swarmed) &&
       !v->melee_threat){
        const unsigned cell=(start_y<<8)|start_x;
        const double dest_x=(double)route.next_x+0.5;
        const double dest_y=(double)route.next_y+0.5;
        const double current_d=hypot(dest_x-(double)v->x/65536.0,
                                     dest_y-(double)v->y/65536.0);
        const double anchor_d=hypot(dest_x-
                         (double)state->acid_progress_x/65536.0,
                         dest_y-(double)state->acid_progress_y/65536.0);
        /* Do not reset on a grid boundary, on a lateral wiggle, or when
         * a moving human changes the overall FOLLOW goal. Only actual
         * progress toward the selected waypoint earns a new timeout. */
        if(!state->acid_progress_valid ||
           state->acid_progress_level!=v->observed_world_level ||
           anchor_d-current_d>=0.24){
            state->acid_progress_valid=1u;
            state->acid_progress_level=v->observed_world_level;
            state->acid_progress_cell=cell;
            state->acid_progress_x=v->x;
            state->acid_progress_y=v->y;
            state->acid_progress_tick=v->tick;
        }else if(v->tick>=state->acid_progress_tick+20u){
            state->acid_commit_ticks=46u;
            state->evade_ticks=0u;
            state->acid_progress_tick=v->tick;
        }
    }else state->acid_progress_valid=0u;
    /* Stuck means the LAST command actually tried to translate but the
     * original engine returned essentially the same coordinates. Do not
     * mistake stationary aiming, cocooning or a closed door for collision. */
    if(state->last_motion && !state->evade_ticks &&
       llabs((long long)(int64_t)v->x-(long long)(int64_t)state->previous_x)<128 &&
       llabs((long long)(int64_t)v->y-(long long)(int64_t)state->previous_y)<128){
        if(state->immobile_ticks<1000u)state->immobile_ticks++;
    }else if(!state->evade_ticks)state->immobile_ticks=0;
    /* A harmless sidestep may be selected repeatedly beside an acid
     * puddle even if the original collision prevents actual translation.
     * After four no-progress ticks, commit to the planner's cardinal next
     * cell for a limited period. Acid still hurts; movement never noclips. */
    if(map_route && need_forward && v->player_species!=1u &&
       v->acid_count && state->immobile_ticks>=4u &&
       !route.requires_door_use &&
       (route.next_x!=start_x || route.next_y!=start_y)){
        /* Never require the *failing* micro-lane probe to authorize stall
         * recovery. A theoretically safer sidestep can be blocked by the
         * real player/peer collision resolution outside this probe. */
        state->acid_commit_ticks=45u;
        state->evade_ticks=0u;
        state->immobile_ticks=0u;
    }
    if(state->acid_commit_ticks)state->acid_commit_ticks--;
    /* Do not spin in place for 19 frames while standing in actual alien
     * acid. The escape choice below uses native movement and chooses the
     * direction of increasing clearance instead of cycling turn recovery. */
    if(v->player_species!=1u && v->acid_count &&
       bot_acid_hits(v,(double)v->x/65536.0,(double)v->y/65536.0,
                       (double)v->x/65536.0,(double)v->y/65536.0)){
        state->evade_ticks=0u;
        if(state->immobile_ticks>9u){
            state->acid_commit_ticks=24u;
            state->immobile_ticks=0u;
        }
    }else if(state->immobile_ticks>9u){
        /* Near acid, blindly rotating for 19 ticks never improves the
         * route and can repeat indefinitely. Preserve heading and use
         * legal route or local escape inputs instead. */
        if(v->player_species!=AVP_MP_ALIEN && v->acid_count &&
           bot_hazards_close(v)){
            state->evade_ticks=0u;
            if(acid_route_steady)state->acid_commit_ticks=45u;
        }else{
            state->evade_ticks=19u;
            state->recovery_side^=1u;
            state->recovery_count++;
        }
        state->immobile_ticks=0u;
    }
    /* Even a recovery started just BEFORE entering the acid area must
     * not keep rotating after the bot reaches that area. */
    if(v->player_species!=AVP_MP_ALIEN && v->acid_count &&
       bot_hazards_close(v) && !v->melee_threat &&
       !(v->hostile_target && v->target_visible))state->evade_ticks=0u;
    state->previous_x=v->x;state->previous_y=v->y;
    /* Peer-yield was returning BACKWARD before acid routing even ran.
     * Three bots following the same sealed one-cell passage therefore
     * paced in opposite directions forever. In a true acid choke, follow
     * the chosen authored route and let original player collision queue
     * them behind the leading bot; no fictional sideways escape exists. */
    if(!v->player_collision_disabled && !state->acid_commit_ticks &&
       !bot_route_acid_choke(v,map_route?&route:NULL) &&
       v->nearby_peer_valid && v->nearby_peer_index<v->player_index &&
       !v->hostile_target &&
       (v->goal==AVP_MP_BOT_PATROL || v->goal==AVP_MP_BOT_SUPPLY ||
        v->goal==AVP_MP_BOT_HEALTH || v->goal==AVP_MP_BOT_MEDICAL ||
        v->goal==AVP_MP_BOT_FOLLOW)){
        const double px=((int64_t)v->nearby_peer_x-(int64_t)v->x)/65536.0;
        const double py=((int64_t)v->nearby_peer_y-(int64_t)v->y)/65536.0;
        const double distance2=px*px+py*py;
        /* A lower-index peer BEHIND this bot is not a route blocker. In
         * an acid corridor, backing into that teammate reproduces the
         * old oscillation even though both are heading the same way. The
         * physical route, not player index alone, determines right of way. */
        const double toward_x=map_route?
            (double)route.next_x+0.5-(double)(int32_t)v->x/65536.0:
            (double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        const double toward_y=map_route?
            (double)route.next_y+0.5-(double)(int32_t)v->y/65536.0:
            (double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        const int peer_ahead=px*toward_x+py*toward_y>0.0;
        if(peer_ahead && distance2<1.3*1.3)
            state->peer_yield_until=v->tick+10u;
        /* Hysteresis: do not reverse course as soon as the other bot
         * moves just outside the collision square. Release after actual
         * clearance, and never let yielding override combat or exit use. */
        if(peer_ahead && distance2<1.8*1.8 &&
           v->tick<state->peer_yield_until){
            const unsigned motion=bot_melee_retreat(v,px,py);
            if(motion<AVP_MP_INPUT_ACTIONS){
                emit_turn(turn,out);
                out->actions|=1u<<motion;
                state->last_motion=1u<<motion;
                state->peer_yield_until=v->tick+10u;
                return;
            }
        }
    }
    if(state->evade_ticks){
        --state->evade_ticks;
        /* When a target is already visible, do not blindly rotate away
         * for 19 recovery ticks; preserve original-aim attack cadence. */
        if(v->hostile_target && v->target_visible)
            emit_turn(angle_diff(v->heading,
                (double)((int64_t)v->target_x-(int64_t)v->x),
                (double)((int64_t)v->target_y-(int64_t)v->y)),out);
        else out->actions|=1u<<AVP_MP_TURN_RIGHT;
        /* Retreat away from the object first, then translate sideways,
         * and finally test forward progress. Flip flank on each failure. */
        {unsigned motion=AVP_MP_INPUT_ACTIONS;
         unsigned flank=state->recovery_side?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
         unsigned opposite=flank==AVP_MP_STRAFE_RIGHT?
             AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
         if(state->evade_ticks>=14u &&
            bot_safe_action(v,AVP_MP_BACKWARD,NULL))motion=AVP_MP_BACKWARD;
         else if(state->evade_ticks>=6u && bot_safe_action(v,flank,NULL))
            motion=flank;
         else if(state->evade_ticks>=6u && bot_safe_action(v,opposite,NULL))
            motion=opposite;
         else if(bot_safe_action(v,AVP_MP_FORWARD,map_route?&route:NULL))
            motion=AVP_MP_FORWARD;
         else if(bot_safe_action(v,flank,NULL))motion=flank;
         else if(bot_safe_action(v,opposite,NULL))motion=opposite;
         if(motion<AVP_MP_INPUT_ACTIONS)out->actions|=1u<<motion;
        }
    }else{
        /* Face a visible enemy while issuing movement via independent
         * original sidestep controls. Route waypoints still guide pursuit
         * when the source LOS reports a blocked shot. */
        if(v->hostile_target && v->target_visible){
            const double sight_turn=angle_diff(v->heading,
                (double)((int64_t)v->target_x-(int64_t)v->x),
                (double)((int64_t)v->target_y-(int64_t)v->y));
            if(v->player_species==0u && !v->deathmatch &&
               !v->marine_unarmed && v->marine_inventory_valid &&
               v->current_weapon>=1u && v->current_weapon<=4u &&
               v->marine_ammo[v->current_weapon-1u])
                emit_marine_precision_turn(sight_turn,out);
            else emit_turn(sight_turn,out);
        }else emit_turn(turn,out);
        if(splash_retreat_motion){
            /* Retreat takes priority over routine melee-evasion strafes,
             * which can carry a shooter INTO the 50-damage kill square.
             * Keep the independent face-enemy aim and Fire commands. */
            out->actions|=1u<<(splash_retreat_motion-1u);
        }else if(teammate_retreat_motion<AVP_MP_INPUT_ACTIONS){
            /* Route toward support is an emergency movement objective,
             * not a new combat target. Continue aiming/firing at the Alien.
             * Native collision remains responsible for real movement. */
            out->actions|=1u<<teammate_retreat_motion;
            if(teammate_retreat_second<AVP_MP_INPUT_ACTIONS)
                out->actions|=1u<<teammate_retreat_second;
        }else if(threat_movement){
            out->actions|=1u<<(threat_movement-1u);
        }else if(health_retreat_motion<AVP_MP_INPUT_ACTIONS){
            /* Back away from the source contact without losing aim/FIRE. */
            out->actions|=1u<<health_retreat_motion;
        }else if(lure_alien){
            /* Retreat along a safe original movement axis while retaining
             * independent aim/turn commands. If blocked, never suppress Fire. */
            out->actions|=1u<<lure_retreat_motion;
        }else if(precise && !state->acid_commit_ticks &&
                 v->tick>=state->damage_react_until && !v->health_low &&
                 !v->marine_unarmed && !v->marine_low_ammo &&
                 !v->retreat_to_refuge && !v->retreat_to_teammate &&
                 !v->other_player_alien_trapped && !v->other_player_swarmed &&
                 !alien_death_blocks_passage && !v->cover_flank_valid &&
                 !shotgun_egg_policy &&
                 (!bot_melee_weapon(v) || (need_fire && !melee_movement)) &&
                 bot_combat_travel(state,v,out)){
            /* Keep the selected enemy aim and native Fire decision;
             * only translation follows the previously selected route. */
        }else if(melee_movement){
            out->actions|=1u<<(melee_movement-1u);
        }else if(guerilla_movement){
            out->actions|=1u<<(guerilla_movement-1u);
        }else if(map_route && need_forward && !tactical_strafe &&
                 (route.next_x!=start_x || route.next_y!=start_y)){
            double wx,wy;
            bot_route_steer_point(v,&route,&wx,&wy);
            bot_prop_waypoint(v,&route,&wx,&wy);
            if(precise && !state->acid_commit_ticks &&
               bot_precise_travel(v,&route,wx-(double)v->x/65536.0,
                                 wy-(double)v->y/65536.0,0,out)){
                /* Exact waypoint direction, even when turning away from it. */
            }else{
            unsigned motion=bot_route_motion(v,&route,
                                               state->acid_commit_ticks!=0u);
            if(motion<AVP_MP_INPUT_ACTIONS){
                unsigned second=bot_route_second_motion(v,&route,motion);
                out->actions|=1u<<motion;
                if(second<AVP_MP_INPUT_ACTIONS){
                    out->actions|=1u<<second;
                    /* An unobstructed two-axis route can use the actual
                     * waypoint ratio instead of two fixed button strengths. */
                    if(!bot_hazards_close(v) && !state->acid_commit_ticks){
                        double wx,wy;
                        bot_route_steer_point(v,&route,&wx,&wy);
                        const double aa=(double)v->heading*(2.0*BOT_PI/4294967296.0);
                        const double fx=cos(aa),fy=-sin(aa);
                        wx-=(double)v->x/65536.0;wy-=(double)v->y/65536.0;
                        const double f=wx*fx+wy*fy,r=-wx*fy+wy*fx;
                        const double len=hypot(wx,wy);
                        if(len>0.0 && bot_step_clear(v,wx/len,wy/len,&route)){
                            out->analog_forward=(float)f;
                            out->analog_strafe=(float)r;
                        }
                    }
                }
            }
            }
        }else{
            if(tactical_strafe)out->actions|=1u<<(tactical_strafe-1u);
            if(precise && need_forward && !tactical_strafe && !state->acid_commit_ticks &&
               bot_precise_travel(v,map_route?&route:NULL,dx,dy,0,out)){
                /* Facing and direction of translation are independent. */
            }else{
            if(need_forward && fabs(turn)<BOT_PI*0.53 &&
               bot_safe_action(v,AVP_MP_FORWARD,map_route?&route:NULL))
                out->actions|=1u<<AVP_MP_FORWARD;
            /* Route movement need not rotate 90 degrees first. If the
             * waypoint lies alongside the player, translate sideways. */
            if(need_forward && !tactical_strafe &&
               !(out->actions&(1u<<AVP_MP_FORWARD)) &&
               fabs(turn)>=BOT_PI*0.30 && fabs(turn)<=BOT_PI*0.70){
                unsigned side=turn>0.0?AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
                if(bot_safe_action(v,side,map_route?&route:NULL))out->actions|=1u<<side;
            }
            }
        }
    }
    /* A hit overrides passive patrol/pickup motion, not real attack/fire
     * inputs. The existing native-Alien death splash and immediate melee
     * evasions outrank this response; acid escape outranks it below. */
    if(!splash_retreat_motion && !threat_movement &&
       teammate_retreat_motion==AVP_MP_INPUT_ACTIONS &&
       !acid_route_steady)
        bot_damage_react(state,v,out);
    /* A real-source no-progress observation overrules optional sidestep,
     * peer-yield and recovery motion during a bounded corridor crossing.
     * Visible hostile/melee defense still has priority. This does not
     * bypass original collision: only the same native MovePlayer keys. */
    if(state->acid_commit_ticks && map_route && need_forward &&
       v->player_species!=AVP_MP_ALIEN && v->acid_count &&
       (!(v->hostile_target && v->target_visible) ||
        v->other_player_alien_trapped || v->other_player_swarmed) &&
       !v->melee_threat && !route.requires_door_use &&
       (route.next_x!=start_x || route.next_y!=start_y)){
        const unsigned motion=bot_route_motion(v,&route,1);
        if(motion<AVP_MP_INPUT_ACTIONS){
            const unsigned move_mask=(1u<<AVP_MP_FORWARD)|
                (1u<<AVP_MP_BACKWARD)|(1u<<AVP_MP_STRAFE_LEFT)|
                (1u<<AVP_MP_STRAFE_RIGHT);
            out->actions=(out->actions&~move_mask)|(1u<<motion);
            const unsigned second=bot_route_second_motion(v,&route,motion);
            if(second<AVP_MP_INPUT_ACTIONS)out->actions|=1u<<second;
        }
    }
    /* Immediate survival override: remaining motionless while inside a
     * native acid contact square is not a valid combat or navigation goal.
     * Choose a source-legal action to leave even if regular tactical motion
     * chose to wait, lure, or strafe against the route. Leave aiming/Fire
     * untouched so the bot can fight while escaping. */
    if(v->player_species!=1u && v->acid_count){
        unsigned escape=bot_escape_current_acid(v,map_route?&route:NULL);
        if(escape<AVP_MP_INPUT_ACTIONS){
            const unsigned move_mask=(1u<<AVP_MP_FORWARD)|
                (1u<<AVP_MP_BACKWARD)|(1u<<AVP_MP_STRAFE_LEFT)|
                (1u<<AVP_MP_STRAFE_RIGHT);
            unsigned second=AVP_MP_INPUT_ACTIONS;
            /* The acid emergency override previously stripped a valid
             * companion motion EVERY tick while standing in the puddle.
             * Use the same collision-tested combined Jaguar commands as
             * the route follower so an oblique crossing keeps moving. */
            if(map_route && bot_route_acid_crossing(v,&route))
                second=bot_route_second_motion(v,&route,escape);
            out->actions=(out->actions&~move_mask)|(1u<<escape);
            if(second<AVP_MP_INPUT_ACTIONS)out->actions|=1u<<second;
        }
    }
    /* Marine rounds never regenerate. No rounds in the currently equipped
       owned gun => do not hammer FIRE; cycle to an owned, loaded weapon if
       available. All-guns-empty Marines instead retreat toward supplies. */
    if(v->player_species==0u && v->marine_inventory_valid){
        const unsigned weapon=v->current_weapon;
        const int loaded=weapon>=1u && weapon<=4u &&
            (v->marine_owned&(1u<<weapon)) && v->marine_ammo[weapon-1u]>0u;
        if(!loaded){
            need_fire=0;
            for(unsigned i=0;i<4u;i++)if((v->marine_owned&(1u<<(i+1u))) &&
                v->marine_ammo[i]>0u && v->tick%12u==0u){
                out->actions|=1u<<AVP_MP_NEXT_WEAPON;
                break;
            }
        }
    }
    /* Use original weapon selection to switch to the lower-damage Pulse
     * Rifle (5) or Shotgun (10) when the equipped gun cannot safely chip a
     * choke Alien. Never fabricate ammo or weapon ownership. */
    if(alien_death_blocks_passage && !emergency_alien_kill &&
       !nonlethal_alien_shot && v->player_species==0u &&
       v->marine_inventory_valid && v->target_health_valid &&
       v->target_visible && (v->tick%12u==0u)){
        unsigned desired=0u;
        if((v->marine_owned&(1u<<2)) && v->marine_ammo[1] &&
           v->target_health>3*5)desired=2u;
        else if((v->marine_owned&(1u<<1)) && v->marine_ammo[0] &&
                v->target_health>3*10)desired=1u;
        if(desired && desired!=v->current_weapon){
            unsigned current=v->current_weapon;
            if(current>=1u && current<=4u){
                unsigned forward=current,backward=current;
                unsigned f=0u,b=0u;
                do{forward=forward==4u?1u:forward+1u;f++;}
                while(forward!=desired && f<4u);
                do{backward=backward==1u?4u:backward-1u;b++;}
                while(backward!=desired && b<4u);
                out->actions|=1u<<(f<=b?AVP_MP_NEXT_WEAPON:
                                                    AVP_MP_PREV_WEAPON);
            }else out->actions|=1u<<AVP_MP_NEXT_WEAPON;
        }
    }
    /* R22 Predator Deathmatch weapon formula: 16 distinct randomized orders,
     * traversed once (64 entries) and then repeated. Weapon selection itself
     * uses ONLY the original next/previous weapon controls. Each bot counts
     * 150 of its own OUT-OF-COMBAT 10-Hz ticks (15 seconds). Pursuing a
     * hostile OR having a nearby detected threat pauses the count and forbids
     * changing weapon. It resumes without resetting after battle. When a
     * switch is already due, combat also suspends the pending switch until
     * fighting ends; no timer is spent waiting for the source equip animation.
     * Co-op/campaign weapon priorities remain unchanged. */
    if(v->player_species==2u && v->predator_owned){
        unsigned desired=avp_mp_bot_predator_preferred_weapon(v);
        if(v->deathmatch){
            int in_combat=v->hostile_target || v->combat_enemies_nearby;
            if(!state->predator_pattern_ready){
                uint32_t seed=(uint32_t)v->tick*0x9e3779b9u ^
                    (v->player_index+1u)*0x85ebca6bu;
                seed^=seed>>16;seed*=0x7feb352du;
                seed^=seed>>15;seed*=0x846ca68bu;seed^=seed>>16;
                state->predator_pattern_start=seed&15u;
                state->predator_pattern_stride=1u+2u*((seed>>4)&7u);
                state->predator_pattern_order=0u;
                state->predator_pattern_step=0u;
                state->predator_pattern_ready=1u;
                state->predator_weapon_target=bot_predator_formula_weapon(state);
                state->predator_rotation_last_tick=v->tick-1u;
            }
            /* Count simulation ticks, never host wall-clock time. A gap
             * while an actor was paused/inactive does not earn idle time,
             * and duplicate observations in one tick cannot speed it up. */
            if(state->predator_rotation_last_tick!=v->tick){
                state->predator_rotation_last_tick=v->tick;
                if(!in_combat && !state->predator_switch_pending &&
                   state->predator_idle_ticks<150u)
                    state->predator_idle_ticks++;
            }
            if(!in_combat){
                if(!state->predator_switch_pending &&
                   state->predator_idle_ticks>=150u){
                    /* An order boundary can repeat the current weapon.
                     * Skip identical/unowned entries in the formula rather
                     * than wasting a 15-second interval on a non-switch. */
                    for(unsigned i=0u;i<64u &&
                        (!(v->predator_owned&(1u<<state->predator_weapon_target)) ||
                         state->predator_weapon_target==v->current_weapon);i++)
                        bot_predator_formula_advance(state);
                    if(state->predator_weapon_target!=v->current_weapon &&
                       (v->predator_owned&(1u<<state->predator_weapon_target)))
                        state->predator_switch_pending=1u;
                    else {
                        /* With only one weapon there is no legal switch. */
                        state->predator_weapon_target=v->current_weapon;
                        state->predator_idle_ticks=0u;
                    }
                }
                if(state->predator_switch_pending){
                    if(v->current_weapon==state->predator_weapon_target){
                        /* The original switch animation finished. Commit
                         * one sequence step and restart the 15-sec interval. */
                        bot_predator_formula_advance(state);
                        state->predator_switch_pending=0u;
                        state->predator_idle_ticks=0u;
                    }else desired=state->predator_weapon_target;
                }
            }
            /* While in combat keep exactly the weapon already equipped,
             * including when an out-of-combat switch was pending. */
            if(in_combat || !state->predator_switch_pending)
                desired=v->current_weapon;
        }
        if(desired>=1u && desired<=4u && v->current_weapon!=desired &&
           v->tick%10u==0u){
            unsigned direction=bot_weapon_cycle_direction(
                v->predator_owned,v->current_weapon,desired);
            out->actions|=1u<<direction;
            need_fire=0; /* FIRE remains enabled on non-switching ticks. */
        }
    }
    if(v->player_species==AVP_MP_MARINE && v->marine_unarmed)need_fire=0;
    if(need_fire){
        /* The original Alien tail (cur_wepno==1) marks a Human AMP as
         * STUN. Switching to claw once stunned avoids toggling STUN off
         * with a second tail hit, allowing the native cocoon death path. */
        unsigned weapon=AVP_MP_FIRE;
        if(v->player_species==1u){
            if(v->goal==AVP_MP_BOT_COCOON && v->target_is_marine){
                /* The retail tail toggles STUN, the swipe/claw preserves
                 * it, and the bite clears it. Never tail a stunned Marine.
                 * Each stage lasts an original-animation-sized window. */
                if(v->target_is_stunned){
                    weapon=AVP_MP_ALIEN_CLAW;
                    state->cocoon_phase=2u;
                }else{
                    if(v->tick>=state->cocoon_phase_end){
                        state->cocoon_phase=(state->cocoon_phase+1u)%3u;
                        state->cocoon_phase_end=v->tick+14u;
                    }
                    weapon=state->cocoon_phase==1u ?
                        AVP_MP_ALIEN_TAIL:AVP_MP_ALIEN_CLAW;
                }
            }else if(!v->deathmatch && v->target_is_marine &&
                     v->max_energy>0 &&
                     v->player_energy*100<v->max_energy*45){
                /* Preserve the established low-health Marine stun tactic
                 * in co-op. Do not let the new general Bite preference
                 * undo the working cocoon/combat survival path. */
                weapon=v->target_is_stunned?AVP_MP_ALIEN_CLAW:AVP_MP_ALIEN_TAIL;
            }else if(v->deathmatch || v->goal!=AVP_MP_BOT_COCOON){
                /* The original Alien has three distinct FIRE inputs. Keep
                 * each choice across an attack-sized window: Bite 60%%,
                 * Tail 20%%, Claw 20%%, in co-op and Deathmatch alike.
                 * Source stun/cocoon logic above
                 * remains unchanged for its special Marine objective. */
                if(!state->alien_attack_until ||
                   v->tick>=state->alien_attack_until){
                    uint32_t seed=(uint32_t)v->tick*0x9e3779b9u ^
                        (v->player_index+1u)*0x7f4a7c15u ^
                        (v->target_slot+1u)*0x6c8e9cf5u;
                    seed^=seed>>16;seed*=0x7feb352du;
                    seed^=seed>>15;seed*=0x846ca68bu;
                    seed^=seed>>16;
                    state->alien_attack_choice=seed%10u;
                    state->alien_attack_until=v->tick+12u+(seed%7u);
                }
                weapon=state->alien_attack_choice<6u?AVP_MP_ALIEN_BITE:
                    state->alien_attack_choice<8u?AVP_MP_ALIEN_TAIL:
                    AVP_MP_ALIEN_CLAW;
            }else if(v->target_is_marine && !v->target_is_stunned &&
                     v->max_energy>0 && v->player_energy*100<v->max_energy*45)
                weapon=AVP_MP_ALIEN_TAIL;
            else weapon=AVP_MP_ALIEN_CLAW;
        }
        out->actions|=1u<<weapon;
    }
    /* An acid corridor needs directional commitment, not 10-Hz tactical
     * forward/backward replanning. Emergency death-spray retreats and real
     * acid contact continue to override it unchanged. */
    bot_acid_lane_hysteresis(state,v,goal_x,goal_y,
        splash_retreat_motion || threat_movement || state->evade_ticks ||
        state->acid_commit_ticks,
        &out->actions);
    /* The final tactical decision may have selected a digital acid lane.
     * Convert ONLY passive route travel, after all safety overrides, to a
     * proportional world vector. Also recover tight dry junctions where
     * the normal long probe could not select a motion. */
    if(precise && map_route && need_forward && v->has_target &&
       !v->hostile_target && !v->melee_threat && !state->evade_ticks &&
       !splash_retreat_motion && !threat_movement && !tactical_strafe &&
       !melee_movement && !guerilla_movement && !lure_alien &&
       health_retreat_motion==AVP_MP_INPUT_ACTIONS &&
       teammate_retreat_motion==AVP_MP_INPUT_ACTIONS &&
       !v->retreat_to_teammate && !v->retreat_to_refuge &&
       !(out->actions&(1u<<AVP_MP_USE)) &&
       (v->goal==AVP_MP_BOT_PATROL || v->goal==AVP_MP_BOT_INVESTIGATE ||
        v->goal==AVP_MP_BOT_SUPPLY || v->goal==AVP_MP_BOT_HEALTH ||
        v->goal==AVP_MP_BOT_FOLLOW || v->goal==AVP_MP_BOT_EXIT ||
        v->goal==AVP_MP_BOT_LIFT) &&
       ((v->player_species!=AVP_MP_ALIEN && v->acid_count && bot_hazards_close(v)) ||
        (!(out->actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
          (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT))) &&
         !bot_hazards_close(v))) &&
       (acid_route_steady || v->tick>=state->damage_react_until))
        (void)bot_precise_local_route(v,&route,state->acid_commit_ticks!=0u,out);
    /* Only actual translation requests trigger next-tick collision recovery. */
    state->last_motion=out->actions &
       ((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT));
    /* No pause/menu/cheat/host-only actions are synthesized. */
}

/* Legacy button planner API for existing diagnostic/proxy tests. */
void avp_mp_bot_step(AvpMpBotState *state,const AvpMpBotObservation *v,
                     AvpMpPlayerCommand *out)
{
    if(!state||!v||!out)return;
    bot_step_plan(state,v,out,0);
    out->analog_turn=out->analog_forward=out->analog_strafe=0.0f;
}

/* Actual host: reuse native proportional input, with no physical deadzone.
 * Preserve tactical decisions, collision, weapons and authored door chords. */
static void bot_step_precise_impl(AvpMpBotState *state,const AvpMpBotObservation *v,
                             int maximum_turn,AvpMpPlayerCommand *out)
{
    const unsigned turns=(1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT);
    const unsigned moves=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                         (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
    double error,limit,forward,strafe,scale=1.0,route_f,route_s;
    unsigned a,local_egg;
    if(!state||!v||!out)return;
    bot_remember_travel(state,v);
    state->objective_route=state->objective_wait=0u;
    bot_step_plan(state,v,out,1);
    local_egg=(out->analog_axis_mask&3u)==3u;
    error=out->analog_turn;route_f=out->analog_forward;route_s=out->analog_strafe;
    out->analog_turn=out->analog_forward=out->analog_strafe=0.0f;
    if(v->facehugged)return;
    if(maximum_turn<=0)maximum_turn=v->player_species==1u?0x900:0x600;
    limit=(double)maximum_turn*(2.0*BOT_PI/65536.0);
    a=out->actions;
    /* Recovery scans request a direction without a target. */
    if(error==0.0 && (a&turns))
        error=((a&(1u<<AVP_MP_TURN_LEFT))?limit:0.0)-
              ((a&(1u<<AVP_MP_TURN_RIGHT))?limit:0.0);
    out->analog_enabled=out->enhanced_analog_enabled=1;
    out->analog_axis_mask=4u;
    out->analog_turn=(float)-fmax(-1.0,fmin(1.0,error/limit));
    out->analog_turn_sensitivity=out->analog_turn_speed=1.0f;
    out->digital_actions=a&~turns;
    if(local_egg){
        out->analog_axis_mask|=3u;
        out->analog_forward=(float)route_f;out->analog_strafe=(float)route_s;
        out->digital_actions&=~moves;
        return;
    }
    /* Keep the planner action bits for accounting; the native pad consumes
     * only digital_actions in this mode. Avoid changing emergency motion. */
    if((a&(1u<<AVP_MP_USE)) || state->evade_ticks ||
       state->acid_commit_ticks || bot_hazards_close(v) ||
       v->melee_threat || (v->nearby_peer_valid &&
       (state->peer_pass_valid || v->tick<state->peer_yield_until)))return;
    /* The normal route probe looks 0.68 tiles ahead. For the last few
     * centimeters of an authored seat this can reject a valid adjustment
     * simply because the probe leaves the target tile. Correct along the
     * actual local 2D direction with a shorter geometry-checked probe. */
    if(v->has_target && !v->hostile_target && nav_valid(&v->map) &&
       (v->goal==AVP_MP_BOT_AIRLOCK_SEAT || v->goal==AVP_MP_BOT_ESCAPE_SEAT) &&
       (v->x>>16)==(v->target_x>>16) && (v->y>>16)==(v->target_y>>16)){
        const double dx=((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        const double dy=((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        const double d=hypot(dx,dy);
        const double x=(double)v->x/65536.0,y=(double)v->y/65536.0;
        const double probe=fmin(0.12,d);
        if((fabs(dx)>0.135 || fabs(dy)>0.135) && d>0.0 &&
           bot_peer_precision_clear(v,dx/d,dy/d,0.68) &&
           bot_step_clear_probe(v,dx/d,dy/d,NULL,probe) &&
           (bot_lane_wall_clear(&v->map,x+dx/d*probe,y+dy/d*probe) ||
            bot_wall_margin_recovery(&v->map,x,y,x+dx/d*probe,y+dy/d*probe))){
            const double angle=(double)v->heading*(2.0*BOT_PI/4294967296.0);
            const double fx=cos(angle),fy=-sin(angle);
            const double throttle=fmin(0.55,d/0.85);
            forward=(dx*fx+dy*fy)/d*throttle;
            strafe=(-dx*fy+dy*fx)/d*throttle;
            out->analog_axis_mask|=3u;
            out->analog_forward=(float)forward;out->analog_strafe=(float)strafe;
            out->actions&=~moves;
            if(fabs(forward)>0.00001)out->actions|=1u<<(forward>0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
            if(fabs(strafe)>0.00001)out->actions|=1u<<(strafe>0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT);
            out->digital_actions=out->actions&~(moves|turns);
            state->last_motion=out->actions&moves;
            return;
        }
    }
    if(!(a&moves))return;
    forward=(double)!!(a&(1u<<AVP_MP_FORWARD))-
            (double)!!(a&(1u<<AVP_MP_BACKWARD));
    strafe=(double)!!(a&(1u<<AVP_MP_STRAFE_RIGHT))-
           (double)!!(a&(1u<<AVP_MP_STRAFE_LEFT));
    /* Retain the prior approximate cruise pace, especially strafe. */
    forward*=v->player_species==1u?0.96:0.69;
    strafe*=v->player_species==1u?0.48:0.35;
    /* The optional ratio is valid only if later tactical decisions kept
     * both selected directions. Retain the old component speed ceilings. */
    if(forward*route_f>0.0 && strafe*route_s>0.0){
        const double gain=fmin(fabs(forward/route_f),fabs(strafe/route_s));
        forward=route_f*gain;strafe=route_s*gain;
    }
    if(v->player_species==AVP_MP_MARINE && v->marine_unarmed){
        const double length=hypot(forward,strafe);
        if(length>0.0){forward/=length;strafe/=length;}
    }
    if(v->has_target && !v->hostile_target &&
       (v->goal==AVP_MP_BOT_AIRLOCK_SEAT || v->goal==AVP_MP_BOT_ESCAPE_SEAT ||
        v->goal==AVP_MP_BOT_COCOON || v->goal==AVP_MP_BOT_HEALTH ||
        v->goal==AVP_MP_BOT_SUPPLY || v->goal==AVP_MP_BOT_MEDICAL ||
        v->goal==AVP_MP_BOT_EXIT || v->goal==AVP_MP_BOT_LIFT ||
        v->goal==AVP_MP_BOT_AIRLOCK_CLOSE || v->goal==AVP_MP_BOT_ESCAPE_LAUNCH)){
        const double dx=((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        const double dy=((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        const double angle=(double)v->heading*(2.0*BOT_PI/4294967296.0);
        const double fx=cos(angle),fy=-sin(angle);
        const double along=dx*(fx*forward-fy*strafe)+dy*(fy*forward+fx*strafe);
        const double distance=hypot(dx,dy);
        /* Taper only on approach, never while retreating from the target. */
        if(along>0.0 && distance<0.85)
            scale=fmax(0.20,distance/0.85);
    }
    if(v->nearby_peer_valid && !v->player_collision_disabled){
        const double a_rad=v->heading*(2.0*BOT_PI/4294967296.0);
        const double fx=cos(a_rad),fy=-sin(a_rad);
        const double wx=fx*forward-fy*strafe,wy=fy*forward+fx*strafe;
        const double len=hypot(wx,wy);
        if(len<0.00001 || !bot_peer_precision_clear(v,wx/len,wy/len,0.68))return;
    }
    out->analog_axis_mask|=3u;
    out->analog_forward=(float)(forward*scale);
    out->analog_strafe=(float)(strafe*scale);
    out->digital_actions&=~moves;
}

/* A visible melee attacker can make an otherwise valid lift/duct route
 * unusable. Only after measured inactivity, select a dry local refuge whose
 * first route segment leads away from the observed contact. Never issue a
 * speculative panel use, change native collision, or alter Co-op policy. */
static double tactical_distance(double x,double y,double tx,double ty)
{
    return hypot(x-tx,y-ty);
}
static int tactical_route(const AvpMpBotObservation *v,uint32_t gx,uint32_t gy,
                          AvpMpBotRoute *r)
{
    return avp_mp_bot_route(&v->map,v->x>>16,v->y>>16,gx>>16,gy>>16,r) && r->valid;
}
static int tactical_away(const AvpMpBotObservation *v,const AvpMpBotRoute *r)
{
    const double x=v->x/65536.0,y=v->y/65536.0;
    const double tx=v->melee_x/65536.0,ty=v->melee_y/65536.0;
    const double nx=r->next_x+0.5,ny=r->next_y+0.5;
    if(r->next_x==(v->x>>16) && r->next_y==(v->y>>16))return 0;
    /* Nonnegative initial derivative of squared threat distance. */
    return (x-tx)*(nx-x)+(y-ty)*(ny-y)>=0.0 &&
        tactical_distance(nx,ny,tx,ty)>tactical_distance(x,y,tx,ty)+0.10;
}
/* R500: an acid puddle in the vicinity is not itself a reason to
 * disable emergency retreat. Test the actual short refuge segments.
 * The route cost bound excludes every acid/prop penalty and all doors;
 * with cost <=28 there can be at most two hops. Keep the original
 * in-contact escape and final remembered-acid command guard authoritative. */
static int bot_acid_segment(double ax,double ay,double bx,double by,
                            uint32_t px,uint32_t py);
static int tactical_first_dry(const AvpMpBotObservation *v,const AvpMpBotRoute *r)
{
    const double x=v->x/65536.0,y=v->y/65536.0;
    for(unsigned i=0;i<v->acid_count;i++)
        if(bot_acid_segment(x,y,r->next_x+0.5,r->next_y+0.5,
                            v->acid[i].x,v->acid[i].y))return 0;
    return 1;
}
static int tactical_refuge_dry(const AvpMpBotObservation *v,const AvpMpBotRoute *r,
                               uint32_t gx,uint32_t gy)
{
    if(r->cost>28u || r->requires_door_use || !tactical_first_dry(v,r))return 0;
    if(r->next_x==(gx>>16) && r->next_y==(gy>>16))return 1;
    if(!r->lookahead_valid || r->lookahead_x!=(gx>>16) || r->lookahead_y!=(gy>>16))return 0;
    for(unsigned i=0;i<v->acid_count;i++)
        if(bot_acid_segment(r->next_x+0.5,r->next_y+0.5,gx/65536.0,gy/65536.0,
                            v->acid[i].x,v->acid[i].y))return 0;
    return 1;
}
static int tactical_goal_eligible(unsigned goal)
{
    return goal==AVP_MP_BOT_LIFT || goal==AVP_MP_BOT_EXIT ||
           goal==AVP_MP_BOT_SUPPLY || goal==AVP_MP_BOT_PATROL;
}
int avp_mp_bot_tactical_travel(AvpMpBotState *s,AvpMpBotObservation *v)
{
    if(!s || !v)return 0;
    const int continuous=s->physical_valid && s->physical_level==v->observed_world_level &&
        v->tick==s->physical_tick+1u;
    const double x=v->x/65536.0,y=v->y/65536.0;
    const double tx=v->melee_x/65536.0,ty=v->melee_y/65536.0;
    const double d=tactical_distance(x,y,tx,ty);
    if(!continuous || !v->deathmatch || v->player_species!=AVP_MP_MARINE ||
       !v->marine_inventory_valid || !v->marine_unarmed || v->player_energy<=0 ||
       !v->has_target || v->hostile_target || v->facehugged || v->lift_wait ||
       v->airlock_pressure || v->evacuation_active || s->objective_wait ||
       s->acid_commit_ticks || !v->melee_visible || !v->melee_threat || d>=3.0 ||
       !nav_valid(&v->map))goto reset;
    /* Contact/committed crossings still belong to the existing acid
     * planner. Nearby but non-contacting puddles only veto unsafe paths. */
    if(bot_acid_hits(v,x,y,x,y))goto reset;
    AvpMpBotRoute r={0};
    if(s->tactical_until>v->tick && s->tactical_level==v->observed_world_level &&
       tactical_goal_eligible(v->goal) &&
       tactical_route(v,s->tactical_x,s->tactical_y,&r) &&
       tactical_refuge_dry(v,&r,s->tactical_x,s->tactical_y) && tactical_away(v,&r) &&
       (!v->map.prop_risk || (!v->map.prop_risk[nav_index(r.next_x,r.next_y)] &&
        !v->map.prop_risk[nav_index(s->tactical_x>>16,s->tactical_y>>16)])) &&
       (!v->map.risk || (!v->map.risk[nav_index(r.next_x,r.next_y)] &&
        !v->map.risk[nav_index(s->tactical_x>>16,s->tactical_y>>16)]))){
        AvpMpBotObservation candidate=*v;
        candidate.goal=AVP_MP_BOT_PATROL;candidate.target_x=s->tactical_x;candidate.target_y=s->tactical_y;
        if(!avp_mp_bot_acid_goal_unsafe(s,&candidate))goto select;
    }
    s->tactical_until=0;
    if(!tactical_goal_eligible(v->goal))goto reset;
    if(!tactical_route(v,v->target_x,v->target_y,&r) ||
       (r.requires_door_use && bot_door_animating(v,r.door_offset)) ||
       (r.next_x==(v->x>>16) && r.next_y==(v->y>>16)) ||
       (tactical_away(v,&r) && tactical_first_dry(v,&r)))goto reset;
    if(s->tactical_blocked && llabs((long long)v->x-s->physical_x)<128 &&
       llabs((long long)v->y-s->physical_y)<128){
        if(s->tactical_idle<1000u)s->tactical_idle++;
    }else s->tactical_idle=0;
    s->tactical_blocked=1;
    /* R501: repeated forward/backward commands can keep the physical
     * idle counter at zero while never increasing separation. Count
     * bounded consecutive observations without meaningful progress.
     * Re-anchor on a materially relocated contact; do not accumulate
     * progress debt across unrelated encounters or world changes. */
    if(!s->tactical_progress_valid ||
       hypot(((int64_t)v->melee_x-s->tactical_threat_x)/65536.0,
             ((int64_t)v->melee_y-s->tactical_threat_y)/65536.0)>0.35 ||
       d>=s->tactical_best_separation+0.20){
        s->tactical_threat_x=v->melee_x;s->tactical_threat_y=v->melee_y;
        s->tactical_best_separation=d;s->tactical_progress_ticks=0;s->tactical_progress_path=0;
        s->tactical_progress_valid=1;
    }else{
        if(s->tactical_progress_ticks<1000u)s->tactical_progress_ticks++;
        s->tactical_progress_path=fmin(100.0,s->tactical_progress_path+
            hypot(((int64_t)v->x-s->physical_x)/65536.0,
                  ((int64_t)v->y-s->physical_y)/65536.0));
    }
    if(s->tactical_idle<10u && (s->tactical_progress_ticks<12u ||
       s->tactical_progress_path<0.25))return 0;
    /* R498: a closed door can overlap the native player's wall margin.
     * Turning toward it and issuing normal Use may be the only legal way
     * to clear the starting pose, even if our escape route heads away.
     * Do not replace this first-edge door approach before it can finish
     * turning. The regular door planner owns access, claims and Use;
     * limit this deferment to 30 stationary ticks, not an indefinite wait. */
    if(s->tactical_idle<30u && s->tactical_progress_ticks<30u && r.requires_door_use){
        const unsigned sx=v->x>>16,sy=v->y>>16;
        const unsigned ux=v->x&65535u,uy=v->y&65535u;
        const uint8_t *cell=nav_cell(&v->map,sx,sy);
        unsigned side=4u,coord=0u;
        if(r.next_x+1u==sx && r.next_y==sy && ux<20480u)side=0u,coord=65535u-uy;
        else if(r.next_y+1u==sy && r.next_x==sx && uy<20480u)side=1u,coord=ux;
        else if(r.next_x==sx+1u && r.next_y==sy && ux>45055u)side=2u,coord=uy;
        else if(r.next_y==sy+1u && r.next_x==sx && uy>45055u)side=3u,coord=65535u-ux;
        if(side<4u && (v->map.wall_flags[cell[side]]&AVP_WB_DOOR) &&
           !bot_partial_wall_clear(&v->map,sx,sy,side,coord))return 0;
    }
    /* Bounded local candidate scan; no forced route through a door, prop,
     * acid risk cell or a contact. Prefer separation with low route cost. */
    double best=-1e30;uint32_t bx=0,by=0;
    for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){
        const int gx=(int)(v->x>>16)+dx,gy=(int)(v->y>>16)+dy;
        if((!dx&&!dy)||gx<0||gy<0||gx>=(int)v->map.width||gy>=(int)v->map.height)continue;
        const unsigned n=nav_index((unsigned)gx,(unsigned)gy);
        if((v->map.risk && v->map.risk[n]) || (v->map.prop_risk && v->map.prop_risk[n]))continue;
        const double gain=tactical_distance(gx+0.5,gy+0.5,tx,ty)-d;
        if(gain<0.75)continue;
        const uint32_t cx=((uint32_t)gx<<16)|0x8000u,cy=((uint32_t)gy<<16)|0x8000u;
        AvpMpBotObservation candidate=*v;candidate.goal=AVP_MP_BOT_PATROL;
        candidate.target_x=cx;candidate.target_y=cy;
        if(avp_mp_bot_acid_goal_unsafe(s,&candidate) || !tactical_route(v,cx,cy,&r) ||
           !tactical_refuge_dry(v,&r,cx,cy) || !tactical_away(v,&r) ||
           (v->map.risk && v->map.risk[nav_index(r.next_x,r.next_y)]) ||
           (v->map.prop_risk && v->map.prop_risk[nav_index(r.next_x,r.next_y)]))continue;
        const double score=gain-0.035*r.cost;
        if(score>best){best=score;bx=cx;by=cy;}
    }
    if(best<-1e29){s->tactical_idle=0;s->tactical_progress_valid=0;return 0;}
    s->tactical_x=bx;s->tactical_y=by;s->tactical_level=v->observed_world_level;
    s->tactical_until=v->tick+40u;
    if(s->tactical_retargets<UINT32_MAX)s->tactical_retargets++;
select:
    s->tactical_idle=s->tactical_blocked=s->tactical_progress_valid=0;
    v->goal=AVP_MP_BOT_PATROL;v->target_x=s->tactical_x;v->target_y=s->tactical_y;
    v->target_visible=v->tracker_only_target=0u;
    v->destination_level=v->exit_is_duct=0u;
    v->retreat_to_teammate=v->cover_flank_valid=0u;
    v->retreat_to_refuge=1u;v->retreat_refuge_x=v->target_x;v->retreat_refuge_y=v->target_y;
    return 1;
reset:
    s->tactical_idle=s->tactical_blocked=s->tactical_progress_valid=0;s->tactical_until=0;return 0;
}

int avp_mp_bot_objective_recoverable(const AvpMpBotObservation *v)
{
    if(!v || !v->deathmatch || !v->has_target || v->facehugged ||
       (v->hostile_target && v->target_visible) || v->melee_visible ||
       v->airlock_pressure || v->evacuation_active) return 0;
    switch(v->goal){
    case AVP_MP_BOT_PATROL: case AVP_MP_BOT_SUPPLY: case AVP_MP_BOT_HEALTH:
    case AVP_MP_BOT_MEDICAL: case AVP_MP_BOT_EXIT: case AVP_MP_BOT_LIFT:
    case AVP_MP_BOT_INVESTIGATE: return 1;
    default: return 0;
    }
}

/* R489: motion can succeed every tick while the objective goes nowhere.
 * Detect only compact repeated movement, with a stable noncombat goal.
 * Credit real displacement in ANY direction, including a U-shaped detour.
 * No extra route search, no new native motion, no Co-op policy change. */
static uint64_t bot_loop_dist(uint32_t x,uint32_t y,uint32_t tx,uint32_t ty)
{
    const uint64_t dx=(uint64_t)llabs((long long)x-tx);
    const uint64_t dy=(uint64_t)llabs((long long)y-ty);
    return dx>dy?dx:dy;
}
static int bot_loop_eligible(const AvpMpBotObservation *v)
{
    return avp_mp_bot_objective_recoverable(v) && v->player_energy>0 &&
        !v->lift_wait && bot_loop_dist(v->x,v->y,v->target_x,v->target_y)>49152u;
}
static int bot_loop_same(const AvpMpBotState *s,const AvpMpBotObservation *v)
{
    return s->loop_valid && s->loop_level==v->observed_world_level &&
        s->loop_goal==(unsigned)v->goal && s->loop_target_x==v->target_x &&
        s->loop_target_y==v->target_y;
}
int avp_mp_bot_loop_ready(const AvpMpBotState *s,const AvpMpBotObservation *v)
{
    return s && v && s->loop_ready && bot_loop_eligible(v) && bot_loop_same(s,v) &&
        s->physical_valid && v->tick==s->physical_tick+1u &&
        s->physical_level==v->observed_world_level && !s->objective_wait;
}
void avp_mp_bot_loop_observe(AvpMpBotState *s,const AvpMpBotObservation *v)
{
    uint64_t remaining,step;
    if(!s || !v)return;
    if(!bot_loop_eligible(v) || s->objective_wait || s->objective_route!=1u ||
       (s->physical_valid && (v->tick!=s->physical_tick+1u ||
       s->physical_level!=v->observed_world_level))){
        s->loop_valid=s->loop_ready=0u;return;
    }
    remaining=bot_loop_dist(v->x,v->y,v->target_x,v->target_y);
    step=bot_loop_dist(v->x,v->y,s->physical_x,s->physical_y);
    if(!bot_loop_same(s,v) || !s->physical_valid || step>=65536u ||
       bot_loop_dist(v->x,v->y,s->loop_x,s->loop_y)>=65536u ||
       remaining+32768u<=s->loop_start_distance){
        s->loop_valid=1u;s->loop_ready=0u;s->loop_path=0u;
        s->loop_start_tick=v->tick;s->loop_start_distance=remaining;
        s->loop_x=v->x;s->loop_y=v->y;s->loop_level=v->observed_world_level;
        s->loop_goal=(unsigned)v->goal;s->loop_target_x=v->target_x;s->loop_target_y=v->target_y;
        return;
    }
    /* Count physical distance only for previously requested translation. */
    if(s->physical_requested && step>=128u)s->loop_path+=step;
    if(v->tick-s->loop_start_tick>=40u){
        if(s->loop_path>=98304u)s->loop_ready=1u;
        else {s->loop_start_tick=v->tick;s->loop_path=0u;}
    }
}

/* R486: a successful crossing is not permission to immediately cross the
 * same puddle in the opposite direction. Remember exact observed contacts,
 * rather than goal IDs or tiles (a source square can straddle a tile edge).
 * Keep the guard while nearby; expire after both time and real separation.
 * Never change the original acid actor, health, or collision geometry. */
static double bot_acid_distance(double x,double y,uint32_t px,uint32_t py)
{
    return fmax(fabs(x-px/65536.0),fabs(y-py/65536.0));
}
static int bot_acid_segment(double ax,double ay,double bx,double by,
                            uint32_t px,uint32_t py)
{
    double lo=0.0,hi=1.0;
    const double a[2]={ax,ay},b[2]={bx,by},p[2]={px/65536.0,py/65536.0};
    for(unsigned j=0;j<2;j++){
        const double d=b[j]-a[j];
        if(fabs(d)<1e-9){if(fabs(a[j]-p[j])>BOT_ACID_CONTACT)return 0;}
        else{
            const double u=(p[j]-BOT_ACID_CONTACT-a[j])/d;
            const double w=(p[j]+BOT_ACID_CONTACT-a[j])/d;
            lo=fmax(lo,fmin(u,w));hi=fmin(hi,fmax(u,w));
            if(lo>hi)return 0;
        }
    }
    return 1;
}
static void bot_acid_observe_exits(AvpMpBotState *s,const AvpMpBotObservation *v)
{
    const double x=v->x/65536.0,y=v->y/65536.0;
    const double ox=s->physical_x/65536.0,oy=s->physical_y/65536.0;
    const int consecutive=s->physical_valid && s->physical_level==v->observed_world_level &&
        v->tick==s->physical_tick+1u && hypot(x-ox,y-oy)<1.0;
    s->acid_contact_count=0u;
    if(!consecutive || v->player_species==AVP_MP_ALIEN || v->player_energy<=0)
        s->acid_exit_count=0u;
    if(v->player_species==AVP_MP_ALIEN || v->player_energy<=0)return;
    for(unsigned i=0;i<s->acid_exit_count;){
        unsigned live=0;
        for(unsigned j=0;j<v->acid_count;j++)
            if(s->acid_exits[i].x==v->acid[j].x && s->acid_exits[i].y==v->acid[j].y){live=1;break;}
        if(!live || (v->tick-s->acid_exits[i].tick>=120u &&
            bot_acid_distance(x,y,s->acid_exits[i].x,s->acid_exits[i].y)>3.0)){
            s->acid_exits[i]=s->acid_exits[--s->acid_exit_count];
        }else i++;
    }
    for(unsigned j=0;j<v->acid_count;j++){
        const uint32_t px=v->acid[j].x,py=v->acid[j].y;
        if(bot_acid_distance(x,y,px,py)<=BOT_ACID_CONTACT){s->acid_contact_count++;continue;}
        if(!consecutive || bot_acid_distance(ox,oy,px,py)>BOT_ACID_CONTACT)continue;
        unsigned i=0;
        for(;i<s->acid_exit_count;i++)if(s->acid_exits[i].x==px && s->acid_exits[i].y==py)break;
        if(i==s->acid_exit_count){
            if(i<sizeof(s->acid_exits)/sizeof(s->acid_exits[0]))s->acid_exit_count++;
            else{
                i=0;
                for(unsigned k=1;k<s->acid_exit_count;k++)
                    if(bot_acid_distance(x,y,s->acid_exits[k].x,s->acid_exits[k].y)>
                       bot_acid_distance(x,y,s->acid_exits[i].x,s->acid_exits[i].y))i=k;
            }
        }
        s->acid_exits[i].x=px;s->acid_exits[i].y=py;s->acid_exits[i].tick=v->tick;
    }
}
/* Only hard-block a choke for which the original wall-margin lane
 * prediction finds no bypass. Leave open-room and off-centre puddles to
 * exact-coordinate steering. Never block the actor's current cell, so
 * crossing momentum can finish and a safe exit is always considered. */
unsigned avp_mp_bot_acid_route_memory(AvpMpBotState *s,
    const AvpMpBotObservation *v,uint8_t risk[AVP_MP_BOT_MAP_CELLS])
{
    unsigned marked=0;
    if(!s || !v || !risk || !nav_valid(&v->map))return 0;
    bot_acid_observe_exits(s,v);
    /* A vent can have only one exit: a remembered choke must remain a
     * costly edge, not permanently disconnect its only route. The final
     * command guard still prevents uncommitted repeat crossings. */
    if(v->observed_world_level>=6u && v->observed_world_level<=13u)return 0;
    for(unsigned i=0;i<s->acid_exit_count;i++){
        const uint32_t px=s->acid_exits[i].x,py=s->acid_exits[i].y;
        const unsigned x=px>>16,y=py>>16;
        if(x>=v->map.width || y>=v->map.height ||
           (x==(v->x>>16) && y==(v->y>>16)))continue;
        if(avp_mp_bot_alien_death_passable(&v->map,px,py))continue;
        const unsigned cell=nav_index(x,y);
        if(risk[cell]!=3u){risk[cell]=3u;marked++;}
    }
    return marked;
}
/* Compare against the ordinary costed route so disconnected maps, locked
 * doors and other failures do not masquerade as a protected acid route. */
static int bot_acid_goal_eligible(const AvpMpBotObservation *v)
{
    if(!v || !v->has_target || !v->map.risk || !nav_valid(&v->map) ||
       v->player_species==AVP_MP_ALIEN || v->hostile_target || v->facehugged ||
       v->evacuation_active || v->airlock_pressure || v->lift_wait)return 0;
    switch(v->goal){
    case AVP_MP_BOT_PATROL: case AVP_MP_BOT_SUPPLY: case AVP_MP_BOT_HEALTH:
    case AVP_MP_BOT_MEDICAL: case AVP_MP_BOT_EXIT: case AVP_MP_BOT_LIFT:
    case AVP_MP_BOT_INVESTIGATE: return 1;
    default: return 0;
    }
}
/* A route to a tile is not a route to an exact point inside a puddle.
 * Check live exit memory even when the cell itself has a safe bypass.
 * Do not forbid a nearby safe interaction point or a first crossing. */
int avp_mp_bot_acid_goal_unsafe(const AvpMpBotState *s,const AvpMpBotObservation *v)
{
    if(!s || !bot_acid_goal_eligible(v) || !s->physical_valid ||
       s->physical_level!=v->observed_world_level ||
       v->tick!=s->physical_tick+1u)return 0;
    for(unsigned i=0;i<s->acid_exit_count;i++){
        const uint32_t px=s->acid_exits[i].x,py=s->acid_exits[i].y;
        if(bot_acid_distance(v->target_x/65536.0,v->target_y/65536.0,px,py)>BOT_ACID_CONTACT)continue;
        for(unsigned j=0;j<v->acid_count;j++)
            if(v->acid[j].x==px && v->acid[j].y==py)return 1;
    }
    return 0;
}
int avp_mp_bot_acid_goal_blocked(const AvpMpBotObservation *v)
{
    AvpMpBotRoute route={0};
    uint8_t ordinary[AVP_MP_BOT_MAP_CELLS];
    AvpMpBotMap map;
    if(!bot_acid_goal_eligible(v))return 0;
    if(avp_mp_bot_route(&v->map,v->x>>16,v->y>>16,v->target_x>>16,v->target_y>>16,&route))return 0;
    map=v->map;map.risk=ordinary;
    for(unsigned i=0;i<AVP_MP_BOT_MAP_CELLS;i++)ordinary[i]=v->map.risk[i]==3u?2u:v->map.risk[i];
    return avp_mp_bot_route(&map,v->x>>16,v->y>>16,v->target_x>>16,v->target_y>>16,&route);
}
static int bot_acid_return_risk(const AvpMpBotState *s,double x,double y,double bx,double by)
{
    for(unsigned i=0;i<s->acid_exit_count;i++){
        const uint32_t px=s->acid_exits[i].x,py=s->acid_exits[i].y;
        const double d=bot_acid_distance(x,y,px,py);
        if(d<=BOT_ACID_CONTACT){
            /* Native momentum can carry an actor back into a puddle;
             * never forbid a motion that improves its real clearance. */
            if(bot_acid_distance(bx,by,px,py)<=d+1e-6)return 1;
        }else if(bot_acid_segment(x,y,bx,by,px,py))return 1;
    }
    return 0;
}
static void bot_acid_guard_command(AvpMpBotState *s,const AvpMpBotObservation *v,
                                  AvpMpPlayerCommand *out)
{
    const unsigned moves=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                         (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
    const unsigned a=out->digital_actions;
    const double h=v->heading*(2.0*BOT_PI/4294967296.0),fx=cos(h),fy=-sin(h);
    const double x=v->x/65536.0,y=v->y/65536.0;
    const double f=(out->analog_axis_mask&1u)?out->analog_forward:
        (double)!!(a&(1u<<AVP_MP_FORWARD))-(double)!!(a&(1u<<AVP_MP_BACKWARD));
    const double r=(out->analog_axis_mask&2u)?out->analog_strafe:
        (double)!!(a&(1u<<AVP_MP_STRAFE_RIGHT))-(double)!!(a&(1u<<AVP_MP_STRAFE_LEFT));
    const double dx=f*fx-r*fy,dy=f*fy+r*fx,len=hypot(dx,dy);
    double best=-1e30,ux=0.0,uy=0.0;
    if(!s->acid_exit_count || v->facehugged || v->evacuation_active ||
       v->airlock_pressure || !nav_valid(&v->map) || (out->actions&(1u<<AVP_MP_USE)))return;
    /* Look ahead of native momentum. This is intentionally a conservative
     * steering horizon, not the displacement applied to the actor. */
    if(len<1e-6 || !bot_acid_return_risk(s,x,y,x+0.45*dx/len,y+0.45*dy/len))return;
    if(s->acid_return_blocks<UINT32_MAX)s->acid_return_blocks++;
    for(unsigned n=0;n<16;n++){
        const double angle=n*(2.0*BOT_PI/16.0),cx=cos(angle),cy=sin(angle);
        const double bx=x+cx*0.45,by=y+cy*0.45;
        double score=0.0;int safe=1;
        if(bot_acid_return_risk(s,x,y,bx,by) ||
           !bot_step_clear_probe(v,cx,cy,NULL,0.12) ||
           !bot_short_lane_clear(&v->map,x,y,x+cx*0.12,y+cy*0.12) ||
           !bot_peer_precision_clear(v,cx,cy,0.45))continue;
        for(unsigned i=0;i<v->acid_count;i++){
            const uint32_t px=v->acid[i].x,py=v->acid[i].y;
            const double old=bot_acid_distance(x,y,px,py),d=bot_acid_distance(bx,by,px,py);
            if(old>BOT_ACID_CONTACT && bot_acid_segment(x,y,bx,by,px,py)){safe=0;break;}
            if(old<=1.25)score+=d-old;
        }
        if(!safe)continue;
        /* Prefer clearance, then the original desired direction. */
        score+=0.05*(cx*dx+cy*dy)/len;
        if(score>best){best=score;ux=cx;uy=cy;}
    }
    out->actions&=~moves;out->digital_actions&=~moves;
    out->analog_forward=out->analog_strafe=0.0f;
    out->analog_axis_mask|=3u;
    if(best>-1e29){
        out->analog_forward=(float)(0.65*(ux*fx+uy*fy));
        out->analog_strafe=(float)(0.65*(-ux*fy+uy*fx));
        if(fabs(out->analog_forward)>1e-5)out->actions|=1u<<(out->analog_forward>0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
        if(fabs(out->analog_strafe)>1e-5)out->actions|=1u<<(out->analog_strafe>0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT);
    }
    /* Drop stale navigation commitments; the host may choose a different
     * reachable objective. Combat and native interaction bits survive. */
    s->acid_lane_motion=0u;s->acid_commit_ticks=0u;
    s->last_motion=out->actions&moves;
}

/* R502: a necessary vent crossing is a bounded local movement objective.
 * Only override ordinary commands for an actual unavoidable choke on a
 * route to a dry objective. Never remove the original acid damage or wall
 * checks. A new goal must persist before revisiting a remembered puddle. */
static int bot_vent_acid_cross(AvpMpBotState *s,const AvpMpBotObservation *v,
                               AvpMpPlayerCommand *out)
{
    const unsigned moves=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
    const unsigned turns=(1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT);
    const double x=v->x/65536.0,y=v->y/65536.0;
    const unsigned sx=v->x>>16,sy=v->y>>16,gx=v->target_x>>16,gy=v->target_y>>16;
    AvpMpBotRoute r={0};
    if(v->observed_world_level<6u || v->observed_world_level>13u ||
       v->player_species==AVP_MP_ALIEN || !v->acid_count ||
       !nav_valid(&v->map) || !v->map.risk || v->player_energy<=0 ||
       v->facehugged || v->evacuation_active || v->airlock_pressure || v->lift_wait ||
       (out->actions&(1u<<AVP_MP_USE)) || s->objective_wait){
        s->vent_goal_valid=0u;s->vent_cross_until=0u;return 0;
    }
    if(!s->physical_valid || s->physical_level!=v->observed_world_level ||
       v->tick!=s->physical_tick+1u){s->vent_goal_valid=0u;s->vent_cross_until=0u;}
    if(s->vent_cross_until>v->tick){
        while(s->vent_cross_index<s->vent_cross_count){
            const unsigned i=s->vent_cross_index;
            const double d=hypot(s->vent_cross_path[i].x-x,s->vent_cross_path[i].y-y);
            const int last=i+1u==s->vent_cross_count;
            if(d>(last && s->acid_contact_count?0.025:0.13))break;
            s->vent_cross_index++;
        }
        if(s->vent_cross_index>=s->vent_cross_count)s->vent_cross_until=0u;
    }
    const int committed=s->vent_cross_until>v->tick;
    /* A hostile can interrupt dry travel, but cannot make an actor stop
     * inside actual acid during a committed crossing. Preserve FIRE and
     * finish the native movement to the dry endpoint first. */
    const int eligible=bot_acid_goal_eligible(v);
    if(!eligible && !(committed && s->acid_contact_count)){
        s->vent_goal_valid=0u;s->vent_cross_until=0u;return 0;
    }
    if(eligible && (!s->vent_goal_valid || s->vent_goal_level!=v->observed_world_level ||
       s->vent_goal_cell!=nav_index(gx,gy) || s->vent_goal_kind!=v->goal)){
        s->vent_goal_valid=1u;s->vent_goal_level=v->observed_world_level;
        s->vent_goal_cell=nav_index(gx,gy);s->vent_goal_kind=v->goal;s->vent_goal_since=v->tick;
    }
    if(committed){
        /* Finish the same crossing even if another travel goal wins. */
    }else{
        uint8_t dry_risk[AVP_MP_BOT_MAP_CELLS];
        AvpMpBotMap dry=v->map;AvpMpBotRoute alternate={0};
        double lane,bx,by;unsigned nx,ny;
        s->vent_cross_until=0u;
        if(gx>=v->map.width || gy>=v->map.height ||
           bot_acid_hits_margin(v,v->target_x/65536.0,v->target_y/65536.0,
             v->target_x/65536.0,v->target_y/65536.0,BOT_ACID_CONTACT) ||
           !avp_mp_bot_route(&v->map,sx,sy,gx,gy,&r) ||
           r.requires_door_use || !r.door_is_open ||
           abs((int)r.next_x-(int)sx)+abs((int)r.next_y-(int)sy)!=1 ||
           !bot_acid_hits(v,x,y,r.next_x+0.5,r.next_y+0.5))return 0;
        /* A nearby side branch does not imply a path around the acid.
         * Decide reachability against the entire observed floor graph. */
        const unsigned lane_hits=bot_best_acid_lane(v,&r,&lane);
        if(!lane_hits || lane_hits==~0u)return 0;
        memcpy(dry_risk,v->map.risk,sizeof dry_risk);dry.risk=dry_risk;
        for(unsigned i=0;i<v->acid_count;i++){
            const unsigned ax=v->acid[i].x>>16,ay=v->acid[i].y>>16;
            if(ax<v->map.width && ay<v->map.height && (ax!=sx || ay!=sy) &&
               !avp_mp_bot_alien_death_passable(&v->map,v->acid[i].x,v->acid[i].y))
                dry_risk[nav_index(ax,ay)]=3u;
        }
        if(avp_mp_bot_route(&dry,sx,sy,gx,gy,&alternate))return 0;
        nx=r.next_x;ny=r.next_y;
        int dx=(int)nx-(int)sx,dy=(int)ny-(int)sy;
        unsigned count=0,return_risk=0;
        double from_x=x,from_y=y;
        /* Keep one waypoint per turn (not one per puddle). Follow the
         * actual route through a bend if its centre is hazardous. Stop
         * the commitment at the first dry endpoint. All full segments
         * and subsequent short movement probes remain collision-checked. */
        for(unsigned i=0;i<6u;i++){
            bx=dx?nx+0.5:lane;by=dx?lane:ny+0.5;
            const int dry_end=!bot_acid_hits_margin(v,bx,by,bx,by,BOT_ACID_CONTACT);
            AvpMpBotRoute onward={0};
            int turn=0,ndx=0,ndy=0;
            if(!dry_end){
                if((nx==gx && ny==gy) ||
                   !avp_mp_bot_route(&v->map,nx,ny,gx,gy,&onward) ||
                   onward.requires_door_use || !onward.door_is_open)return 0;
                ndx=(int)onward.next_x-(int)nx;ndy=(int)onward.next_y-(int)ny;
                if(abs(ndx)+abs(ndy)!=1 || (ndx==-dx && ndy==-dy))return 0;
                turn=ndx!=dx || ndy!=dy;
            }
            if(dry_end || turn){
                if(!bot_lane_segment_clear(v,from_x,from_y,bx,by))return 0;
                return_risk|=bot_acid_return_risk(s,from_x,from_y,bx,by);
                s->vent_cross_path[count].x=bx;s->vent_cross_path[count].y=by;count++;
                if(dry_end)break;
                from_x=bx;from_y=by;lane=ndx?by:bx;
            }
            nx=onward.next_x;ny=onward.next_y;dx=ndx;dy=ndy;
            if(i==5u)return 0;
        }
        if(!count || (return_risk && v->tick-s->vent_goal_since<12u))return 0;
        /* Preserve the already verified first-time junction steering. The
         * expanded multi-leg/side-branch override is specifically for a
         * necessary return through remembered acid. Initial straight
         * choke crossings can still use the fast committed controller. */
        if(!return_risk && (count>1u || !bot_route_acid_choke(v,&r)))return 0;
        s->vent_cross_count=count;s->vent_cross_index=0u;s->vent_cross_until=v->tick+90u;
    }
    const double dx=s->vent_cross_path[s->vent_cross_index].x-x,
                 dy=s->vent_cross_path[s->vent_cross_index].y-y,len=hypot(dx,dy);
    if(len<0.001)return 0;
    const double probe=fmin(0.12,len),ux=dx/len,uy=dy/len;
    r.valid=1u;r.next_x=(unsigned)(x+ux*probe);r.next_y=(unsigned)(y+uy*probe);
    if(!bot_step_clear_probe(v,ux,uy,&r,probe) ||
       !bot_short_lane_clear(&v->map,x,y,x+ux*probe,y+uy*probe)){
        s->vent_cross_until=0u;return 0;
    }
    const double angle=v->heading*(2.0*BOT_PI/4294967296.0),fx=cos(angle),fy=-sin(angle);
    const double throttle=fmin(1.0,fmax(0.25,len/0.18));
    out->actions&=~(moves|turns);out->digital_actions&=~(moves|turns);
    out->analog_axis_mask|=7u;out->analog_turn=0.0f;
    out->analog_forward=(float)(throttle*(ux*fx+uy*fy));
    out->analog_strafe=(float)(throttle*(-ux*fy+uy*fx));
    if(fabs(out->analog_forward)>1e-5)out->actions|=1u<<(out->analog_forward>0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
    if(fabs(out->analog_strafe)>1e-5)out->actions|=1u<<(out->analog_strafe>0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT);
    s->last_motion=out->actions&moves;
    return 1;
}

/* R492: a Deathmatch peer's native 0xa000 collision square is wider
 * than a diagonal melee ray. A pair can face each other indefinitely with
 * no valid long forward probe and no valid strike. Search short lateral
 * approaches that improve the actual shot-box margin, NOT just distance.
 * This is a command-only fallback for stalled or regressive Alien versus visible player.
 * Preserve the generic conservative peer guard for every other planner. */
static double bot_peer_melee_deficit(double tx,double ty)
{
    const double d=hypot(tx,ty);
    if(d<1e-6)return -BOT_MELEE_SHOT_WIDTH;
    return fmax(fabs(tx)*(1.0-BOT_MELEE_REACH/d),
                fabs(ty)*(1.0-BOT_MELEE_REACH/d))-BOT_MELEE_SHOT_WIDTH;
}
static unsigned bot_peer_melee_align(const AvpMpBotState *s,
    const AvpMpBotObservation *v,AvpMpPlayerCommand *out)
{
    const unsigned moves=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
    const unsigned attacks=(1u<<AVP_MP_FIRE)|(1u<<AVP_MP_ALIEN_CLAW)|
        (1u<<AVP_MP_ALIEN_TAIL)|(1u<<AVP_MP_ALIEN_BITE);
    if(!v->deathmatch || v->player_species!=AVP_MP_ALIEN ||
       !v->has_target || !v->hostile_target || !v->target_visible ||
       v->goal!=AVP_MP_BOT_HOSTILE || v->target_slot<AVP_NUM_AMPS ||
       v->target_slot>=AVP_NUM_AMPS+AVP_LOCAL_MP_MAX_ACTORS ||
       v->facehugged || v->player_energy<=0 || v->cover_flank_valid ||
       v->evacuation_active || v->airlock_pressure || v->lift_wait ||
       s->objective_wait || v->player_collision_disabled ||
       !v->nearby_peer_valid || !nav_valid(&v->map) ||
       v->prop_count>=AVP_MP_BOT_MAX_NEAR_PROPS ||
       (out->actions&(attacks|(1u<<AVP_MP_USE))))return 0;
    const double x=v->x/65536.0,y=v->y/65536.0;
    const double tx=v->target_x/65536.0-x,ty=v->target_y/65536.0-y;
    const double d=hypot(tx,ty),before=bot_peer_melee_deficit(tx,ty);
    /* If the ordinary shot already fits or turning alone is sufficient,
     * keep the existing attack/aim policy. No widening of weapon reach. */
    if(d>1.5 || before<=0.002 || bot_melee_source_shot_box(v,tx,ty))return 0;
    unsigned target_registered=0,nearest_registered=0;
    for(unsigned i=0;i<v->prop_count;i++){
        const AvpMpBotProp *p=&v->props[i];
        if(p->halfwidth<AVP_MP_PEER_CLEARANCE)continue;
        if(p->x==v->target_x && p->y==v->target_y)target_registered=1;
        if(p->x==v->nearby_peer_x && p->y==v->nearby_peer_y)nearest_registered=1;
    }
    if(!target_registered || !nearest_registered)return 0;
    const double angle=v->heading*(2.0*BOT_PI/4294967296.0);
    const double fx=cos(angle),fy=-sin(angle);
    /* Preserve an existing command that improves the shot margin. A
     * backpedal or sideways command that worsens it is not an approach. */
    if(out->actions&moves){
        const double f=(out->analog_axis_mask&1u)?out->analog_forward:
            (double)!!(out->actions&(1u<<AVP_MP_FORWARD))-(double)!!(out->actions&(1u<<AVP_MP_BACKWARD));
        const double r=(out->analog_axis_mask&2u)?out->analog_strafe:
            (double)!!(out->actions&(1u<<AVP_MP_STRAFE_RIGHT))-(double)!!(out->actions&(1u<<AVP_MP_STRAFE_LEFT));
        const double len=hypot(f,r);
        if(len>1e-5){
            const double dx=(f*fx-r*fy)/len,dy=(f*fy+r*fx)/len;
            if(bot_peer_melee_deficit(tx-dx*0.12,ty-dy*0.12)<before-0.002 &&
               bot_step_clear_probe(v,dx,dy,NULL,0.12) &&
               bot_short_lane_clear(&v->map,x,y,x+dx*0.12,y+dy*0.12))return 0;
        }
    }
    double best=before-0.002,ux=0,uy=0;
    const double probe=0.12;
    for(unsigned i=0;i<32u;i++){
        const double angle=i*(2.0*BOT_PI/32.0),dx=cos(angle),dy=sin(angle);
        const double bx=x+dx*probe,by=y+dy*probe;
        const double merit=bot_peer_melee_deficit(tx-dx*probe,ty-dy*probe);
        if(merit>=best || !bot_step_clear_probe(v,dx,dy,NULL,probe) ||
           !bot_short_lane_clear(&v->map,x,y,bx,by))continue;
        /* Test the actual short swept peer/prop geometry instead of the
         * general travel guard's blanket 1.3-tile exclusion. No close
         * prop overlap or crossing a registered peer's collision square. */
        int clear=1;
        for(unsigned j=1;j<=12u && clear;j++){
            const double px=x+dx*probe*j/12.0,py=y+dy*probe*j/12.0;
            for(unsigned k=0;k<v->prop_count;k++){
                const AvpMpBotProp *p=&v->props[k];
                const double r=p->halfwidth/65536.0;
                if(fabs(px-p->x/65536.0)<r && fabs(py-p->y/65536.0)<r){clear=0;break;}
            }
        }
        if(!clear)continue;
        best=merit;ux=dx;uy=dy;
    }
    if(ux==0.0 && uy==0.0)return 0;
    const double throttle=0.25;
    out->analog_axis_mask|=3u;
    out->analog_forward=(float)((ux*fx+uy*fy)*throttle);
    out->analog_strafe=(float)((-ux*fy+uy*fx)*throttle);
    out->digital_actions&=~moves;
    out->actions&=~moves;
    if(fabs(out->analog_forward)>1e-5)out->actions|=1u<<(out->analog_forward>0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
    if(fabs(out->analog_strafe)>1e-5)out->actions|=1u<<(out->analog_strafe>0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT);
    return 1;
}

/* R494: a passive Deathmatch bot can remain motionless next to a peer
 * even after its recovery goal changes. Do not waive the conservative
 * ordinary travel guard. Only after a measured idle interval, try a short
 * native input that increases clearance from the registered nearby peer.
 * Never displace an interaction, combat, or acid-escape command. */
static unsigned bot_peer_unblock(AvpMpBotState *s,
    const AvpMpBotObservation *v,AvpMpPlayerCommand *out)
{
    const unsigned moves=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
    const unsigned attacks=(1u<<AVP_MP_FIRE)|(1u<<AVP_MP_ALIEN_CLAW)|
        (1u<<AVP_MP_ALIEN_TAIL)|(1u<<AVP_MP_ALIEN_BITE);
    const unsigned sx=v->x>>16,sy=v->y>>16;
    const double x=v->x/65536.0,y=v->y/65536.0;
    const double px=v->nearby_peer_x/65536.0,py=v->nearby_peer_y/65536.0;
    const double before=hypot(x-px,y-py);
    int active=s->peer_unblock_until>v->tick && s->physical_valid &&
        s->physical_level==v->observed_world_level && v->tick==s->physical_tick+1u &&
        s->peer_unblock_level==v->observed_world_level &&
        s->peer_unblock_index==v->nearby_peer_index;
    if(!active)s->peer_unblock_until=0u;
    if(!avp_mp_bot_objective_recoverable(v) || v->hostile_target ||
       v->melee_threat || v->player_energy<=0 || v->lift_wait ||
       s->objective_wait || s->acid_commit_ticks || s->evade_ticks ||
       !v->nearby_peer_valid || v->player_collision_disabled ||
       !nav_valid(&v->map) || v->prop_count>=AVP_MP_BOT_MAX_NEAR_PROPS ||
       (out->actions&(attacks|(1u<<AVP_MP_USE))))goto stop;
    if(!active && (s->objective_stall<6u || (out->actions&moves) || before>=1.30 || before<1e-6))return 0;
    if(active && s->physical_stall>=5u)goto stop;
    /* Dry-only; the existing acid policy retains exclusive hazard authority. */
    for(unsigned i=0;i<v->acid_count;i++)
        if(hypot(x-v->acid[i].x/65536.0,y-v->acid[i].y/65536.0)<2.0)goto stop;
    unsigned registered=0;
    for(unsigned i=0;i<v->prop_count;i++)
        if(v->props[i].x==v->nearby_peer_x && v->props[i].y==v->nearby_peer_y &&
           v->props[i].halfwidth>=AVP_MP_PEER_CLEARANCE)registered=1;
    if(!registered || bot_prop_hit(v,x,y))goto stop;
    if(!active){
        AvpMpBotRoute route={0};
        if(!avp_mp_bot_route(&v->map,sx,sy,v->target_x>>16,v->target_y>>16,&route) ||
           !route.valid || (sx==(v->target_x>>16) && sy==(v->target_y>>16)))return 0;
        /* Commit to a neighbouring safe cell, not a single backstep which
         * the normal planner can undo on the next tick. Only use an open,
         * dry cardinal edge and a fully clear segment to the new centre. */
        double best=-1e30;
        for(unsigned side=0;side<4;side++){
            unsigned nx,ny,door,open;
            if(!nav_edge(&v->map,sx,sy,side,&nx,&ny,&door,&open) || (door && open<40u))continue;
            if(v->map.risk && v->map.risk[nav_index(nx,ny)])continue;
            const double wx=nx+0.5,wy=ny+0.5;
            const double sep=hypot(wx-px,wy-py),d=hypot(wx-x,wy-y);
            if(sep<1.30 || sep<before+0.35)continue;
            const int direct=bot_lane_segment_clear(v,x,y,wx,wy);
            if(!direct && (!bot_lane_segment_clear(v,x,y,sx+0.5,sy+0.5) ||
                !bot_lane_segment_clear(v,sx+0.5,sy+0.5,wx,wy) ||
                hypot(sx+0.5-px,sy+0.5-py)<before))continue;
            const double score=sep-d*0.35-(direct?0.0:0.2);
            if(score>best){best=score;s->peer_unblock_x=(nx<<16)|0x8000u;s->peer_unblock_y=(ny<<16)|0x8000u;
                s->peer_unblock_center=!direct;s->peer_unblock_cx=(sx<<16)|0x8000u;s->peer_unblock_cy=(sy<<16)|0x8000u;}
        }
        if(best<-1e29)return 0;
        s->peer_unblock_until=v->tick+60u;s->peer_unblock_level=v->observed_world_level;
        s->peer_unblock_index=v->nearby_peer_index;
    }
    if(s->peer_unblock_center && hypot(s->peer_unblock_cx/65536.0-x,s->peer_unblock_cy/65536.0-y)<0.08)
        s->peer_unblock_center=0u;
    const double wx=(s->peer_unblock_center?s->peer_unblock_cx:s->peer_unblock_x)/65536.0;
    const double wy=(s->peer_unblock_center?s->peer_unblock_cy:s->peer_unblock_y)/65536.0;
    const double oldgoal=hypot(wx-x,wy-y);
    if(!s->peer_unblock_center && oldgoal<0.14)goto stop;
    const double probe=fmin(0.12,oldgoal);
    double best=-1e30,ux=0,uy=0;
    for(unsigned i=0;i<32u;i++){
        const double a=i*(2.0*BOT_PI/32.0),dx=cos(a),dy=sin(a);
        const double bx=x+dx*probe,by=y+dy*probe;
        const double gain=hypot(bx-px,by-py)-before;
        const double progress=oldgoal-hypot(wx-bx,wy-by);
        if(gain< -0.002 || progress<0.015 || !bot_step_clear_probe(v,dx,dy,NULL,probe) ||
           !bot_short_lane_clear(&v->map,x,y,bx,by) ||
           !bot_prop_motion_clear(v,x,y,bx,by))continue;
        const double score=progress+gain*0.35;
        if(score>best){best=score;ux=dx;uy=dy;}
    }
    if(best<-1e29)goto stop;
    const double h=v->heading*(2.0*BOT_PI/4294967296.0),fx=cos(h),fy=-sin(h);
    const double throttle=fmin(0.25,oldgoal/0.6);
    out->analog_axis_mask|=3u;
    out->analog_forward=(float)(throttle*(ux*fx+uy*fy));
    out->analog_strafe=(float)(throttle*(-ux*fy+uy*fx));
    out->digital_actions&=~moves;out->actions&=~moves;
    if(fabs(out->analog_forward)>1e-5)out->actions|=1u<<(out->analog_forward>0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
    if(fabs(out->analog_strafe)>1e-5)out->actions|=1u<<(out->analog_strafe>0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT);
    return 1;
stop:
    s->peer_unblock_until=0u;return 0;
}

/* Observe the final issued command, independent of planner branch. Count
 * only consecutive live ticks in one world; Use, facehugger and stationary
 * aiming are not translation. This counter never moves a native actor. */
void avp_mp_bot_step_precise(AvpMpBotState *state,const AvpMpBotObservation *v,
                             int maximum_turn,AvpMpPlayerCommand *out)
{
    const unsigned moves=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                         (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
    if(!state||!v||!out)return;
    if(state->physical_valid && state->physical_level==v->observed_world_level &&
       v->tick>state->physical_tick && v->tick-state->physical_tick==1u &&
       state->physical_requested && !v->facehugged &&
       llabs((long long)v->x-state->physical_x)<128 &&
       llabs((long long)v->y-state->physical_y)<128){
        if(state->physical_stall<1000u)state->physical_stall++;
    }else state->physical_stall=0u;
    if(avp_mp_bot_objective_recoverable(v) && state->physical_valid &&
       state->physical_level==v->observed_world_level &&
       v->tick==state->physical_tick+1u && state->objective_requested &&
       llabs((long long)v->x-state->physical_x)<128 &&
       llabs((long long)v->y-state->physical_y)<128){
        if(state->objective_stall<1000u)state->objective_stall++;
    }else state->objective_stall=0u;
    if(!state->physical_valid || state->physical_level!=v->observed_world_level ||
       v->tick!=state->physical_tick+1u)state->objective_recovery_until=0u;
    bot_acid_observe_exits(state,v);
    bot_step_precise_impl(state,v,maximum_turn,out);
    state->peer_melee_align=bot_peer_melee_align(state,v,out);
    state->peer_unblock=bot_peer_unblock(state,v,out);
    if(state->peer_melee_align || state->peer_unblock)state->last_motion=out->actions&moves;
    if(!bot_vent_acid_cross(state,v,out))bot_acid_guard_command(state,v,out);
    /* Allow the existing native panel retry only at the interaction cell. */
    if((v->goal==AVP_MP_BOT_LIFT || v->goal==AVP_MP_BOT_EXIT ||
        v->goal==AVP_MP_BOT_MEDICAL) && (v->x>>16)==(v->target_x>>16) &&
       (v->y>>16)==(v->target_y>>16) && state->last_exit_use &&
       v->tick>=state->last_exit_use && v->tick-state->last_exit_use<35u)
        state->objective_wait=2u;
    state->objective_requested=avp_mp_bot_objective_recoverable(v) &&
        !state->objective_wait && !(out->actions&(moves|(1u<<AVP_MP_USE)|(1u<<AVP_MP_FIRE)));
    if(!state->objective_requested)state->objective_stall=0u;
    avp_mp_bot_loop_observe(state,v);
    state->physical_valid=1u;state->physical_level=v->observed_world_level;
    state->physical_tick=v->tick;state->physical_x=v->x;state->physical_y=v->y;
    state->physical_requested=!v->facehugged && !(out->actions&(1u<<AVP_MP_USE)) &&
        (((out->analog_axis_mask&1u)?fabs(out->analog_forward)>0.00001:
          !!(out->actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)))) ||
         ((out->analog_axis_mask&2u)?fabs(out->analog_strafe)>0.00001:
          !!(out->actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)))));
    state->physical_requested=state->physical_requested && !!(out->actions&moves);
}
