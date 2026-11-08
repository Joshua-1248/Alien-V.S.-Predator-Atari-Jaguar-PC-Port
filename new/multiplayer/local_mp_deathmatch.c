#include "local_mp_deathmatch.h"
#include "player.h"
#include "amp.h"
#include "local_mp_amp_native.h"
#include "doors.h"
#include <string.h>
int avp_mp_dm_spawn_interior(const uint8_t *cells,unsigned width,unsigned height,
    const uint8_t wall_flags[256],unsigned x,unsigned y)
{
    static const int dx[4]={-1,0,1,0},dy[4]={0,-1,0,1};
    static const unsigned opposite[4]={2,3,0,1};
    if(!cells||!wall_flags||width<3u||height<3u||width>64u||height>64u||
       x==0u||y==0u||x+1u>=width||y+1u>=height)return 0;
    const uint8_t *c=cells+((size_t)y*width+x)*8u;
    if(!c[5])return 0;
    for(unsigned side=0;side<4u;side++){
        unsigned nx=(unsigned)((int)x+dx[side]);
        unsigned ny=(unsigned)((int)y+dy[side]);
        const uint8_t *n=cells+((size_t)ny*width+nx)*8u;
        if(n[5])continue;
        /* When adjacent to void, demand an authored blocking wall, not
         * just a valid floor texture. These checks are deliberately more
         * conservative than SafePos at the middle of a cell. */
        /* Original COLLIDE.S check_wall: LEFTONLY and 2SIDES stop only
         * near the edge; an open door admits a player through the center.
         * Against missing floor, require a PERMANENT solid barrier. */
        if(!((wall_flags[c[side]]|wall_flags[n[opposite[side]]]) &
             AVP_WB_SOLID))return 0;
    }
    return 1;
}
int avp_mp_dm_spawn_origin_region(const uint16_t components[4096],
    unsigned width,unsigned height,unsigned origin_x,unsigned origin_y,
    unsigned candidate_x,unsigned candidate_y)
{
    unsigned start,candidate;
    if(!components||!width||!height||width>64u||height>64u||
       origin_x>=width||origin_y>=height||
       candidate_x>=width||candidate_y>=height)return 0;
    start=components[origin_y*64u+origin_x];
    candidate=components[candidate_y*64u+candidate_x];
    return start!=0u && start==candidate;
}
static int dm_is_enemy(const AvpAmp *a)
{
    if(!a || (!(a->mode||a->host_static)) || a->energy<=0 ||
       !(a->flags&(1u<<AMP_KILLABLE)) || (a->flags&(1u<<AMP_PLAYER)))return 0;
    switch(a->creature){
    case AC_ALIEN:case AC_PREDATOR:case AC_HUMAN:case AC_EGG:
    case AC_FACEHUG:case AC_ACRAWL:case AC_AQUEEN:case AC_GEN:
    case AC_AQSHIELD:return 1;
    default:return 0;
    }
}
unsigned avp_mp_dm_remove_enemies(AvpMpAmpNativeState *state)
{
    unsigned count=0u;
    if(!state)return 0u;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        AvpAmp *a=&state->actors[i];
        unsigned x=(unsigned)((uint32_t)a->xpos>>16);
        unsigned y=(unsigned)((uint32_t)a->ypos>>16);
        unsigned n=i+1u;
        if(!dm_is_enemy(a))continue;
        /* Preserve a different AMP that may own this grid position. */
        if(x<AVP_AMP_GRID_W && y<AVP_AMP_GRID_H &&
           state->collmap[y*AVP_AMP_GRID_W+x]==n)
            state->collmap[y*AVP_AMP_GRID_W+x]=0;
        if(x<AVP_AMP_GRID_W && y<AVP_AMP_OBJ_H){
            unsigned o=y*AVP_AMP_OBJ_ROW_BYTES+x*2u;
            if((unsigned)((state->objmap[o]<<8)|state->objmap[o+1u])==n){
                state->objmap[o]=state->objmap[o+1u]=0;
            }
        }
        a->mode=NULL;a->host_static=0;a->aux_ptr=NULL;
        a->energy=0;a->flags=0;
        /* Release the original numeric AMP slot for future player bullets.
         * Never insert duplicate free-list entries. */
        unsigned seen=0u;
        for(unsigned k=0;k<state->free_count;k++)
            if(state->free_list[k]==i){seen=1u;break;}
        if(!seen && state->free_count<AVP_NUM_AMPS)
            state->free_list[state->free_count++]=(uint16_t)i;
        count++;
    }
    return count;
}
static unsigned dm_winner(const AvpMpDeathmatch *m)
{
    unsigned leader=AVP_LOCAL_MP_MAX_ACTORS,score=0u;
    score=0;
    for(unsigned p=0;p<m->players;p++)if(m->kills[p]>score)score=m->kills[p];
    leader=AVP_LOCAL_MP_MAX_ACTORS;
    for(unsigned p=0;p<m->players;p++)if(m->kills[p]==score){
        if(leader!=AVP_LOCAL_MP_MAX_ACTORS)return AVP_LOCAL_MP_MAX_ACTORS;
        leader=p;
    }
    return leader;
}
void avp_mp_dm_init(AvpMpDeathmatch *m,const AvpLocalMpSetup *setup)
{
    if(!m)return;
    memset(m,0,sizeof(*m));m->winner=AVP_LOCAL_MP_MAX_ACTORS;
    if(!setup||setup->game_mode!=AVP_MP_DEATHMATCH)return;
    m->enabled=1u;m->players=avp_local_mp_total_actors(setup);
    m->score_limit=setup->dm_score_limit;
    m->time_limit_minutes=setup->dm_time_limit;
    m->fast_movement=setup->dm_fast_movement;
    m->fast_equip=setup->dm_fast_equip;
    m->enemies=setup->dm_enemies;
    m->level=setup->dm_level;
    m->level_travel=setup->dm_level_travel;
    m->cloak_damage=setup->dm_cloak_damage;
}
void avp_mp_dm_tick(AvpMpDeathmatch *m)
{
    if(!m||!m->enabled||m->finished)return;
    m->elapsed_ticks++;
    if(m->time_limit_minutes && m->elapsed_ticks>=
       (uint64_t)m->time_limit_minutes*60u*10u){
        m->finished=1u;m->winner=dm_winner(m);
    }
}
void avp_mp_dm_kill(AvpMpDeathmatch *m,unsigned killer,unsigned victim)
{
    if(!m||!m->enabled||m->finished||victim>=m->players)return;
    m->deaths[victim]++;
    if(killer>=m->players||killer==victim)return;
    m->kills[killer]++;
    if(m->score_limit && m->kills[killer]>=m->score_limit){
        m->finished=1u;m->winner=killer;
    }
}
unsigned avp_mp_dm_seconds_remaining(const AvpMpDeathmatch *m)
{
    uint64_t max,elapsed;
    if(!m||!m->time_limit_minutes)return 0;
    max=(uint64_t)m->time_limit_minutes*60u;
    elapsed=m->elapsed_ticks/10u;
    return elapsed>=max?0u:(unsigned)(max-elapsed);
}
void avp_mp_dm_speed(unsigned enabled,uint32_t *max_speed,int16_t *acc_shift)
{
    if(!enabled||!max_speed||!acc_shift)return;
    /* Original player movement: double BOTH acceleration and top speed.
     * No additional MovePlayer tick, teleportation or collision bypass. */
    if(*acc_shift>0 && *max_speed<=UINT32_MAX/2u){
        --*acc_shift;*max_speed*=2u;
    }
}
/* The Jaguar charge formula runs before the PvP hit hook. Multiplying the
 * finished hit damage preserves its original rounding/charge cadence. The
 * resulting min/max endpoints are exactly those chosen for Deathmatch. */
int avp_mp_dm_direct_damage(int16_t species,int16_t weapon,int damage)
{
    if(damage<=0)return 0;
    if(species==0){ /* Marine */
        switch(weapon){
        case 1: return damage*20; /* Shotgun: 10 -> 200 (PvP only) */
        case 2: return damage*10; /* Pulse rifle: 5 -> 50 */
        case 4: return damage*2;  /* Smartgun: 60 -> 120 */
        default:return damage;
        }
    }
    if(species==4){ /* Alien: tail, claw, bite */
        switch(weapon){
        case 1:return damage*4; /* 10-20 -> 40-80 */
        case 2:return damage*3; /* 15-30 -> 45-90 */
        case 3:return damage*2; /* 50-100 -> 100-200 */
        default:return damage;
        }
    }
    if(species==8){ /* Predator: combi-stick, wrist blades */
        switch(weapon){
        case 1:return damage*2; /* 50-100 -> 100-200 */
        case 4:return damage*3; /* 15-30 -> 45-90 */
        default:return damage;
        }
    }
    return damage;
}
int avp_mp_dm_victim_damage(unsigned modifier,int16_t species,
                            unsigned invisflag,int damage)
{
    if(damage<=0)return damage;
    if(modifier && species==PT_PREDATOR && invisflag){
        /* Only the current invisible player avatar, not a native AMP and
         * not a merely fading HUD, takes the optional fourfold PvP hit. */
        return damage>INT32_MAX/4?INT32_MAX:damage*4;
    }
    return damage;
}
int avp_mp_dm_projectile_damage(unsigned creature)
{
    /* Original AMP enum: AC_SMART=6, AC_BOLT=7, AC_FIRE=8. */
    switch(creature){
    case 6:return 100; /* Predator Smart Disc */
    case 7:return 100; /* Predator Shoulder Cannon direct */
    case 8:return 125; /* Marine flamethrower, per flame hit */
    default:return 0;
    }
}
/* The Jaguar already has three blood sprites (Alien, Predator, Marine).
 * Only the Marine's ORIGINAL hitscan weapons generate these puffs on
 * Deathmatch player contact. Flame projectiles are deliberately excluded. */
int avp_mp_dm_bullet_impact_creature(int16_t shooter_species,
    int16_t weapon,int16_t victim_species)
{
    if(shooter_species!=PT_HUMAN ||
       (weapon!=1 && weapon!=2 && weapon!=4))return -1;
    switch(victim_species){
    case PT_ALIEN: return AC_ALIEN;
    case PT_PREDATOR:return AC_PREDATOR;
    case PT_HUMAN:return AC_HUMAN;
    default:return -1;
    }
}
int avp_mp_dm_explosion_candidate(int32_t dx,int32_t dy,uint32_t radius)
{
    /* PLAYER.S / collision.c rough_dist is max(dx,dy)+min(dx,dy)/4.
     * Use unsigned intermediate arithmetic for valid retail-world cells. */
    uint64_t x=dx<0?(uint64_t)(-(int64_t)dx):(uint64_t)dx;
    uint64_t y=dy<0?(uint64_t)(-(int64_t)dy):(uint64_t)dy;
    return (x>y?x+(y>>2):y+(x>>2))<=radius;
}
static uint32_t abs32(int32_t v){return v<0?(uint32_t)(-(int64_t)v):(uint32_t)v;}
int avp_mp_dm_shot_candidate(int32_t dx,int32_t dy,int32_t boxx,int32_t boxy,
                             int16_t cs,int16_t sn,uint16_t width,
                             uint32_t nearest,uint32_t *depth)
{
    int32_t rx,ry,z,xoff;
    if(!depth||boxx<=0||boxy<=0 ||abs32(dx)>=(uint32_t)boxx ||
       abs32(dy)>=(uint32_t)boxy)return 0;
    rx=dx>>8;ry=dy>>8;
    z=(int32_t)((int32_t)(int16_t)ry*sn)+
      (int32_t)((int32_t)(int16_t)rx*cs);
    if(z<0||(uint32_t)z>=nearest)return 0;
    xoff=(int32_t)((int32_t)(int16_t)rx*sn)-
         (int32_t)((int32_t)(int16_t)ry*cs);
    if((abs32(xoff)>>16)>(uint32_t)width)return 0;
    *depth=(uint32_t)z;return 1;
}
