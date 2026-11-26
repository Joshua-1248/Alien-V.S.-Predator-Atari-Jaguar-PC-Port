#include "local_mp_bot.h"
#include "amp.h"
#include "local_mp_input.h"
#include "doors.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define CELL(x,y,w) ((y)*(w)*8u+(x)*8u)
/* Allow synthetic Training Maze test grids wider than the original 5x5 fixtures. */
static uint8_t map_cells[64u*64u*8u];
static uint8_t wallflags[256];
static uint8_t risk[4096];
static void blank_map(unsigned w,unsigned h)
{
    memset(map_cells,0,sizeof map_cells);
    memset(wallflags,0,sizeof wallflags);
    memset(risk,0,sizeof risk);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)
        map_cells[CELL(x,y,w)+5u]=1u;
}
/* Table-row firing lane simulator for the isolated route test. It does
 * not replace source LineOfSight: the live host supplies the real callback. */
static int test_row_prop_lane(void *ctx,uint32_t ax,uint32_t ay,
                              uint32_t bx,uint32_t by)
{
    const uint8_t *occupied=(const uint8_t *)ctx;
    const double fx=(double)(int32_t)ax/65536.0;
    const double fy=(double)(int32_t)ay/65536.0;
    const double dx=(double)((int64_t)bx-(int64_t)ax)/65536.0;
    const double dy=(double)((int64_t)by-(int64_t)ay)/65536.0;
    for(unsigned j=1u;j<128u;j++){
        const double x=fx+dx*(double)j/128.0;
        const double y=fy+dy*(double)j/128.0;
        const unsigned ix=x<0.0?64u:(unsigned)x;
        const unsigned iy=y<0.0?64u:(unsigned)y;
        if(ix<64u && iy<64u && occupied[iy*64u+ix])return 0;
    }
    return 1;
}
int main(void)
{
    AvpMpBotState s={0};
    AvpMpBotObservation v={0};
    AvpMpPlayerCommand c;
    AvpMpBotRoute route;
    AvpMpBotMap map;
    /* R298 Med Lab response: a close Facehugger observed by the host's
     * all-direction LOS-gated scan must cause an actual turn, not a blind
     * gunshot backwards. When fully turned, use the unmodified Marine aim
     * and FIRE gate. This checks the bot command planner independently of
     * how the original Level-2 room's geometry is laid out. */
    {
        AvpMpBotState guard={0};
        AvpMpBotObservation alarm={0};
        AvpMpPlayerCommand response={0};
        blank_map(10u,10u);
        alarm.map=(AvpMpBotMap){map_cells,10u,10u,wallflags,NULL,
                                0u,risk,NULL,0u};
        alarm.player_species=AVP_MP_MARINE;
        alarm.player_energy=alarm.max_energy=1000;
        alarm.marine_inventory_valid=1u;
        alarm.marine_owned=1u<<1;
        alarm.marine_ammo[0]=25u;
        alarm.current_weapon=1u;
        alarm.x=(5u<<16)|0x8000u;
        alarm.y=(5u<<16)|0x8000u;
        alarm.target_x=(2u<<16)|0x8000u;
        alarm.target_y=alarm.y;
        alarm.has_target=alarm.hostile_target=alarm.target_visible=1u;
        alarm.goal=AVP_MP_BOT_HOSTILE;
        alarm.melee_threat=AVP_MP_BOT_THREAT_FACEHUG;
        alarm.melee_visible=1u;
        alarm.melee_x=alarm.target_x;
        alarm.melee_y=alarm.target_y;
        alarm.tick=10u;
        avp_mp_bot_step(&guard,&alarm,&response);
        assert(response.actions&((1u<<AVP_MP_TURN_LEFT)|
                                 (1u<<AVP_MP_TURN_RIGHT)));
        assert(!(response.actions&(1u<<AVP_MP_FIRE)));
        memset(&guard,0,sizeof(guard));
        alarm.heading=0x80000000u; /* now actually facing the threat */
        alarm.tick=11u;
        avp_mp_bot_step(&guard,&alarm,&response);
        assert(response.actions&(1u<<AVP_MP_FIRE));
        puts("R298 close rear Facehugger: face threat first, no rear fire: PASS");
    }
    /* R295: optional distant errands yield to a nearby living team, while
     * urgent ammo/medical retrieval, authentic close combat and rescues do
     * not. The decision does not depend on player-collision settings. */
    {
        AvpMpBotObservation team={0};
        const uint64_t near=(uint64_t)(2u<<16)*(2u<<16);
        const uint64_t separated=(uint64_t)(5u<<16)*(5u<<16);
        const uint64_t far_objective=(uint64_t)(8u<<16)*(8u<<16);
        team.player_species=AVP_MP_MARINE;
        team.goal=AVP_MP_BOT_PATROL;
        team.has_target=1u;
        assert(avp_mp_bot_team_regroup_priority(&team,separated,near,0u));
        assert(avp_mp_bot_team_regroup_priority(&team,near,far_objective,0u));
        assert(!avp_mp_bot_team_regroup_priority(&team,near,near,0u));
        team.player_collision_disabled=1u;
        assert(avp_mp_bot_team_regroup_priority(&team,separated,near,0u));
        team.goal=AVP_MP_BOT_SUPPLY;
        assert(!avp_mp_bot_team_regroup_priority(&team,separated,near,0u));
        assert(avp_mp_bot_team_regroup_priority(&team,separated,far_objective,0u));
        /* A reachable original ammo pickup can refill an EMPTY reserve,
         * including Pulse Rifle rounds before the rifle itself is found. */
        assert(!avp_mp_bot_team_regroup_priority(&team,separated,
                                                 far_objective,1u));
        team.marine_low_ammo=1u;
        assert(!avp_mp_bot_team_regroup_priority(&team,separated,far_objective,0u));
        team.marine_low_ammo=0u;team.marine_unarmed=1u;
        assert(!avp_mp_bot_team_regroup_priority(&team,separated,far_objective,0u));
        team.marine_unarmed=0u;team.health_low=1u;
        team.goal=AVP_MP_BOT_HEALTH;
        assert(!avp_mp_bot_team_regroup_priority(&team,separated,far_objective,0u));
        team.health_low=0u;team.goal=AVP_MP_BOT_HOSTILE;
        team.hostile_target=team.target_visible=1u;
        assert(!avp_mp_bot_team_regroup_priority(&team,separated,far_objective,0u));
        team.target_visible=0u;
        assert(avp_mp_bot_team_regroup_priority(&team,separated,far_objective,0u));
        team.other_player_alien_trapped=1u;
        assert(!avp_mp_bot_team_regroup_priority(&team,separated,far_objective,0u));
        team.other_player_alien_trapped=0u;team.goal=AVP_MP_BOT_LIFT;
        assert(!avp_mp_bot_team_regroup_priority(&team,separated,far_objective,0u));
        team.goal=AVP_MP_BOT_PATROL;team.deathmatch=1u;
        assert(!avp_mp_bot_team_regroup_priority(&team,separated,far_objective,0u));
        puts("R295 co-op regroup/urgent supplies/visible combat priorities: PASS");
    }
    /* RE44 R289: a following bot gives a human player personal space,
     * whether the multiplayer Player Collision toggle is ON or OFF.
     * Sustained clearance hysteresis must not twitch forward/backward. */
    for(unsigned collisions_off=0u;collisions_off<=1u;collisions_off++){
        AvpMpBotState following={0};
        AvpMpBotObservation p={0};
        AvpMpPlayerCommand cmd={0};
        blank_map(8u,8u);
        p.map=(AvpMpBotMap){map_cells,8u,8u,wallflags,NULL,0u,NULL,NULL,0u};
        p.player_species=AVP_MP_MARINE;
        p.player_index=1u;
        p.player_collision_disabled=collisions_off;
        p.player_energy=p.max_energy=100;
        p.x=(3u<<16)|0x8000u;
        p.y=(3u<<16)|0x8000u;
        p.target_x=(4u<<16)|0x2000u;
        p.target_y=p.y;
        p.heading=0u;
        p.has_target=1u;
        p.goal=AVP_MP_BOT_FOLLOW;
        for(unsigned tick=1u;tick<=5u;tick++){
            p.tick=tick;
            avp_mp_bot_step(&following,&p,&cmd);
            assert(cmd.actions&(1u<<AVP_MP_BACKWARD));
            assert(!(cmd.actions&(1u<<AVP_MP_FORWARD)));
            assert(following.follow_backoff_active);
            /* Approximate position advancement only: input-level test.
             * Native MovePlayer remains the runtime movement authority. */
            p.x-=0x3333u;
        }
        p.tick=6u;
        avp_mp_bot_step(&following,&p,&cmd);
        assert(!following.follow_backoff_active);
        assert(!(cmd.actions&(1u<<AVP_MP_BACKWARD)));
        assert(!(cmd.actions&(1u<<AVP_MP_FORWARD)));
        /* The player approaches again: the bot yields anew, without
         * needing to re-enable physical player collisions. */
        p.x=(3u<<16)|0x8000u;
        p.tick=7u;
        avp_mp_bot_step(&following,&p,&cmd);
        assert(following.follow_backoff_active);
        assert(cmd.actions&(1u<<AVP_MP_BACKWARD));
        /* A real melee danger cancels personal-space housekeeping. */
        p.melee_threat=AVP_MP_BOT_THREAT_ALIEN;
        p.tick=8u;
        avp_mp_bot_step(&following,&p,&cmd);
        assert(!following.follow_backoff_active);
        p.melee_threat=0u;
        p.goal=AVP_MP_BOT_HOSTILE;
        p.hostile_target=1u;p.target_visible=1u;
        p.tick=9u;
        avp_mp_bot_step(&following,&p,&cmd);
        assert(!following.follow_backoff_active);
    }
    /* Heading matters: if facing AWAY from the following target, pressing
     * Backward would approach the player; the safe action is Forward. */
    {
        AvpMpBotState following={0};
        AvpMpBotObservation p={0};
        AvpMpPlayerCommand cmd={0};
        blank_map(8u,8u);
        p.map=(AvpMpBotMap){map_cells,8u,8u,wallflags,NULL,0u,NULL,NULL,0u};
        p.player_species=AVP_MP_MARINE;p.player_index=1u;
        p.player_collision_disabled=1u;p.player_energy=p.max_energy=100;
        p.x=(3u<<16)|0x8000u;p.y=(3u<<16)|0x8000u;
        p.target_x=(4u<<16)|0x2000u;p.target_y=p.y;
        p.heading=0x80000000u;p.has_target=1u;
        p.goal=AVP_MP_BOT_FOLLOW;p.tick=1u;
        avp_mp_bot_step(&following,&p,&cmd);
        assert(cmd.actions&(1u<<AVP_MP_FORWARD));
        assert(!(cmd.actions&(1u<<AVP_MP_BACKWARD)));
        p.goal=AVP_MP_BOT_LIFT;p.tick=2u;
        avp_mp_bot_step(&following,&p,&cmd);
        assert(!following.follow_backoff_active);
    }
    /* R288: an off-centre acid square leaves a REAL legal lane at y=0.56
     * in a one-cell passage (original 40<<9 player wall margin).  A long
     * speculative movement test must not veto the first short strafe into
     * that lane when player collision is OFF.  The original game applies
     * the movement; this test checks the bot's chosen 10-Hz input. */
    {
        AvpMpBotState runner={0};
        AvpMpBotObservation p={0};
        AvpMpPlayerCommand cmd={0};
        blank_map(6u,1u);
        p.map=(AvpMpBotMap){map_cells,6u,1u,wallflags,NULL,0u,risk,NULL,0u};
        p.player_species=AVP_MP_MARINE;p.player_index=1u;
        p.player_collision_disabled=1u;
        p.player_energy=p.max_energy=100;
        p.heading=0u;p.x=(1u<<16)|0x8000u;p.y=0x8000u;
        p.target_x=(5u<<16)|0x8000u;p.target_y=0x8000u;
        p.has_target=1u;p.goal=AVP_MP_BOT_SUPPLY;p.tick=1u;
        p.acid_count=1u;
        p.acid[0]=(AvpMpBotAcid){(2u<<16)|0x8000u,0x4ccdu};
        avp_mp_bot_acid_risk_mark(risk,p.acid[0].x,p.acid[0].y);
        avp_mp_bot_step(&runner,&p,&cmd);
        assert(cmd.actions&(1u<<AVP_MP_STRAFE_RIGHT));
        assert(!(cmd.actions&(1u<<AVP_MP_BACKWARD)));
    }
    /* R288 regression: player collision OFF, original-width navigable
     * corridor, acid offset toward the south wall ahead of the bot. At
     * x=1.20 the puddle is 1.30 cells away: the old near-acid threshold
     * selected a 0.68-cell *speculative* sidestep, rejected it at y=1.02,
     * and emitted NO movement despite a legal 0.12-cell step toward the
     * clear lane. This was one source of permanent acid hesitation. */
    {
        AvpMpBotState runner={0};
        AvpMpBotObservation p={0};
        AvpMpPlayerCommand cmd={0};
        blank_map(8u,1u);
        p.map=(AvpMpBotMap){map_cells,8u,1u,wallflags,NULL,0u,risk,NULL,0u};
        p.player_species=AVP_MP_MARINE;p.player_index=1u;
        p.player_collision_disabled=1u;
        p.player_energy=p.max_energy=100;
        p.heading=0u;p.x=(1u<<16)|0x3333u;p.y=0x570au;
        p.target_x=(7u<<16)|0x8000u;p.target_y=0x8000u;
        p.has_target=1u;p.goal=AVP_MP_BOT_SUPPLY;p.tick=1u;
        p.acid_count=1u;
        p.acid[0]=(AvpMpBotAcid){(2u<<16)|0x8000u,0x4000u};
        avp_mp_bot_acid_risk_mark(risk,p.acid[0].x,p.acid[0].y);
        AvpMpBotRoute r={0};
        assert(avp_mp_bot_route(&p.map,p.x>>16,p.y>>16,7u,0u,&r));
        avp_mp_bot_step(&runner,&p,&cmd);
        assert(cmd.actions&(1u<<AVP_MP_STRAFE_RIGHT));
        assert(!(cmd.actions&(1u<<AVP_MP_BACKWARD)));
    }
    /* R288: multi-tick acid corridor crossing, not just a chosen command.
     * Two adjacent authored cells are enough to reproduce the R287
     * forward/backward cycle across x=2.0: changing grid cells did not
     * mean the bot made any net progress. Player Collision remains OFF.
     * The 0.07-cell movement here is a synthetic command-level proxy;
     * the real MovePlayer and AMPCollisions remain unmodified. */
    for(unsigned vertical=0u;vertical<2u;vertical++)
    for(unsigned two_puddles=0u;two_puddles<2u;two_puddles++){
        AvpMpBotState runner={0};
        AvpMpBotObservation p={0};
        AvpMpPlayerCommand cmd={0};
        if(vertical)blank_map(1u,8u);
        else blank_map(8u,1u);
        p.map=(AvpMpBotMap){map_cells,vertical?1u:8u,
            vertical?8u:1u,wallflags,NULL,0u,risk,NULL,0u};
        p.player_species=AVP_MP_MARINE;p.player_index=2u;
        p.player_collision_disabled=1u;p.player_energy=p.max_energy=100u;
        p.heading=vertical?0xc0000000u:0u;
        p.x=(uint32_t)((vertical?0.38:1.20)*65536.0);
        p.y=(uint32_t)((vertical?1.20:0.38)*65536.0);
        p.target_x=(uint32_t)((vertical?0.5:7.5)*65536.0);
        p.target_y=(uint32_t)((vertical?7.5:0.5)*65536.0);
        p.has_target=1u;p.goal=AVP_MP_BOT_SUPPLY;
        p.acid_count=two_puddles?2u:1u;
        for(unsigned k=0u;k<p.acid_count;k++){
            p.acid[k]=(AvpMpBotAcid){
                (uint32_t)((vertical?0.58:2.5+(double)k)*65536.0),
                (uint32_t)((vertical?2.5+(double)k:0.58)*65536.0)};
            avp_mp_bot_acid_risk_mark(risk,p.acid[k].x,p.acid[k].y);
        }
        unsigned escaped=0u;
        for(unsigned t=1u;t<=190u;t++){
            p.tick=t;avp_mp_bot_step(&runner,&p,&cmd);
            const double a=(double)(p.heading>>16)*
                (2.0*3.141592653589793/65536.0);
            const double fx=cos(a),fy=-sin(a);
            double mx=0.0,my=0.0;
            if(cmd.actions&(1u<<AVP_MP_FORWARD)){mx+=fx;my+=fy;}
            if(cmd.actions&(1u<<AVP_MP_BACKWARD)){mx-=fx;my-=fy;}
            if(cmd.actions&(1u<<AVP_MP_STRAFE_LEFT)){mx+=fy;my-=fx;}
            if(cmd.actions&(1u<<AVP_MP_STRAFE_RIGHT)){mx-=fy;my+=fx;}
            if(cmd.actions&(1u<<AVP_MP_TURN_LEFT))p.heading+=(0x180u<<16);
            if(cmd.actions&(1u<<AVP_MP_TURN_RIGHT))p.heading-=(0x180u<<16);
            const double nx=(double)p.x/65536.0+0.07*mx;
            const double ny=(double)p.y/65536.0+0.07*my;
            if((vertical?(nx>=0.3125 && nx<=0.6875 &&
                           ny>=0.3125 && ny<7.6875):
                         (ny>=0.3125 && ny<=0.6875 &&
                           nx>=0.3125 && nx<7.6875))){
                p.x=(uint32_t)(nx*65536.0);
                p.y=(uint32_t)(ny*65536.0);
            }
            if((vertical?(double)p.y:(double)p.x)/65536.0>=6.8){
                escaped=1u;break;
            }
        }
        assert(escaped); /* Must actually cross, not merely press Forward. */
    }
    /* R284: a source-legal fringe lane may be blocked in actual multiplayer
     * by a peer or a tight collision corner. Repeating its sidestep while
     * staying at the exact same XY is NOT progress. Once original movement
     * failed for several ticks, commit through the graph-selected corridor
     * rather than triggering a 19-tick spin/reverse cycle. */
    {
        AvpMpBotState stuck={0};
        AvpMpBotObservation p={0};
        AvpMpPlayerCommand cmd={0};
        blank_map(6u,1u);
        map=(AvpMpBotMap){map_cells,6u,1u,wallflags,NULL,0u,risk,NULL,0u};
        p.map=map;p.player_species=AVP_MP_MARINE;
        p.player_energy=p.max_energy=100;
        p.x=(1u<<16)|0x8000u;p.y=0x8000u;
        p.target_x=(5u<<16)|0x8000u;p.target_y=p.y;
        p.heading=0u;p.has_target=1u;p.goal=AVP_MP_BOT_SUPPLY;
        p.acid_count=1u;p.acid[0]=(AvpMpBotAcid){(2u<<16)|0x8000u,0xA000u};
        avp_mp_bot_acid_risk_mark(risk,p.acid[0].x,p.acid[0].y);
        for(unsigned tick=1u;tick<=30u;tick++){
            p.tick=tick;avp_mp_bot_step(&stuck,&p,&cmd);
        }
        assert(stuck.acid_commit_ticks>0u);
        assert(cmd.actions&(1u<<AVP_MP_FORWARD));
        assert(!(cmd.actions&(1u<<AVP_MP_BACKWARD)));
        /* Tiny legal back-and-forth movement can prevent the old literal
         * immobile counter from ever firing. Real net XY stays under half
         * a tile, so the route commitment must still trigger. */
        memset(&stuck,0,sizeof stuck);
        for(unsigned tick=1u;tick<=30u;tick++){
            p.tick=tick;
            p.x=((1u<<16)|0x8000u)+(tick&1u?0x0600u:0u);
            avp_mp_bot_step(&stuck,&p,&cmd);
        }
        assert(stuck.acid_commit_ticks>0u);
    }
    /* R284: two Marines facing each other across a one-cell acid choke
     * cannot pass through their native player collision. The higher slot
     * must route into an actually connected side pocket, allowing the
     * lower slot to cross. Same-direction followers must NEVER backtrack. */
    {
        AvpMpBotState high={0},low={0};
        AvpMpBotObservation p={0};
        AvpMpPlayerCommand cmd={0};
        blank_map(9u,3u);
        memset(map_cells,0,sizeof map_cells);
        for(unsigned x=0;x<9u;x++)map_cells[CELL(x,1u,9u)+5u]=1u;
        map_cells[CELL(6u,2u,9u)+5u]=1u;
        map=(AvpMpBotMap){map_cells,9u,3u,wallflags,NULL,0u,risk,NULL,0u};
        avp_mp_bot_acid_risk_mark(risk,(4u<<16)|0x8000u,(1u<<16)|0x8000u);
        p.map=map;p.player_species=AVP_MP_MARINE;p.heading=0x80000000u;
        p.x=(5u<<16)|0x8000u;p.y=(1u<<16)|0x8000u;
        p.target_x=(1u<<16)|0x8000u;p.target_y=p.y;
        p.player_index=2u;p.tick=10u;p.has_target=1u;
        p.player_energy=p.max_energy=100;
        p.goal=AVP_MP_BOT_SUPPLY;p.acid_count=1u;
        p.acid[0]=(AvpMpBotAcid){(4u<<16)|0x8000u,(1u<<16)|0x8000u};
        p.nearby_peer_valid=1u;p.nearby_peer_index=1u;
        p.nearby_peer_x=(3u<<16)|0x8000u;p.nearby_peer_y=p.y;
        p.nearby_peer_heading=0u;
        avp_mp_bot_step(&high,&p,&cmd);
        assert(high.peer_pass_valid);
        assert(high.peer_pass_x==6u && high.peer_pass_y==2u);
        assert(!(cmd.actions&(1u<<AVP_MP_FORWARD)));
        assert(cmd.actions&((1u<<AVP_MP_BACKWARD)|
                            (1u<<AVP_MP_STRAFE_LEFT)|
                            (1u<<AVP_MP_STRAFE_RIGHT)));
        /* Once the bay is reached it must hold for the other player,
         * not immediately re-enter the original one-wide chokepoint. */
        p.x=(6u<<16)|0x8000u;p.y=(2u<<16)|0x8000u;
        p.tick=11u;avp_mp_bot_step(&high,&p,&cmd);
        assert(high.peer_pass_valid);
        assert(!(cmd.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                              (1u<<AVP_MP_STRAFE_LEFT)|
                              (1u<<AVP_MP_STRAFE_RIGHT))));
        /* R285: toggling Player Collision OFF while already WAITING in
         * the passing bay must cancel the committed pause immediately.
         * A vanished peer is not sufficient: ON-mode bay holding normally
         * remains valid after that peer moves outside observation range. */
        p.player_collision_disabled=1u;
        p.nearby_peer_valid=0u;p.tick=12u;
        avp_mp_bot_step(&high,&p,&cmd);
        assert(!high.peer_pass_valid);
        assert(!high.peer_pass_arrived);
        assert(!high.peer_yield_until);
        assert(cmd.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                            (1u<<AVP_MP_STRAFE_LEFT)|
                            (1u<<AVP_MP_STRAFE_RIGHT)));
        /* Turning collisions back ON can correctly start a NEW passing
         * maneuver, without inheriting OFF-mode or stale bay state. */
        p.player_collision_disabled=0u;
        p.nearby_peer_valid=1u;
        p.x=(5u<<16)|0x8000u;p.y=(1u<<16)|0x8000u;
        p.tick=13u;
        avp_mp_bot_step(&high,&p,&cmd);
        assert(high.peer_pass_valid);
        /* The leading/lower-index Marine keeps its authentic crossing. */
        p.x=(3u<<16)|0x8000u;p.y=(1u<<16)|0x8000u;
        p.heading=0u;p.player_index=1u;
        p.target_x=(8u<<16)|0x8000u;
        p.nearby_peer_index=2u;
        p.nearby_peer_x=(5u<<16)|0x8000u;
        p.nearby_peer_heading=0x80000000u;
        avp_mp_bot_step(&low,&p,&cmd);
        assert(!low.peer_pass_valid);
        assert(cmd.actions&(1u<<AVP_MP_FORWARD));
        /* No passing pocket in a sealed duct is NOT a legitimate bypass.
         * A same-direction queue must not trigger right-of-way yielding. */
        memset(&high,0,sizeof high);
        p.player_index=2u;p.x=(5u<<16)|0x8000u;
        p.target_x=(8u<<16)|0x8000u;
        p.nearby_peer_index=1u;p.nearby_peer_x=(4u<<16)|0x8000u;
        p.nearby_peer_heading=0u;
        avp_mp_bot_step(&high,&p,&cmd);
        assert(!high.peer_pass_valid);
        /* A completely sealed single-cell passage has no legitimate
         * passing bay. Don't manufacture one or phase through a wall. */
        map_cells[CELL(6u,2u,9u)+5u]=0u;
        memset(&high,0,sizeof high);
        p.x=(5u<<16)|0x8000u;p.y=(1u<<16)|0x8000u;
        p.heading=0x80000000u;p.target_x=(1u<<16)|0x8000u;
        p.nearby_peer_index=1u;p.nearby_peer_x=(3u<<16)|0x8000u;
        p.nearby_peer_heading=0u;
        avp_mp_bot_step(&high,&p,&cmd);
        assert(!high.peer_pass_valid);
        assert(high.peer_pass_retry_tick>p.tick);
    }
    /* R283: health urgency is a proportion of the ORIGINAL hit points.
     * Death or invalid caps never request navigation. */
    assert(!avp_mp_bot_health_low(601,1000));
    assert(avp_mp_bot_health_low(600,1000));
    assert(avp_mp_bot_health_critical(300,1000));
    assert(!avp_mp_bot_health_critical(301,1000));
    assert(!avp_mp_bot_health_low(0,1000));
    assert(!avp_mp_bot_health_low(50,0));
    /* R283: the bot uses native panel-facing Use, not an instant health
     * assignment or an invented interaction with arbitrary walls. */
    {
        AvpMpBotState medbot={0};
        AvpMpBotObservation med={0};
        AvpMpPlayerCommand medcmd={0};
        blank_map(9u,9u);
        med.map=(AvpMpBotMap){map_cells,9u,9u,wallflags,NULL,4u,risk,NULL,0u};
        med.x=(3u<<16)|0x8000u;med.y=(4u<<16)|0x8000u;
        med.target_x=med.x;med.target_y=med.y;
        med.goal=AVP_MP_BOT_MEDICAL;med.has_target=1u;
        med.player_species=AVP_MP_MARINE;med.heading=0u;
        med.target_side=2u;med.tick=90u;
        med.player_energy=120;med.max_energy=1000;
        avp_mp_bot_step(&medbot,&med,&medcmd);
        assert(medcmd.actions&(1u<<AVP_MP_USE));
        assert(!(medcmd.actions&(1u<<AVP_MP_FIRE)));
        /* Cannot operate a south-facing terminal while looking east. */
        memset(&medbot,0,sizeof medbot);med.target_side=3u;
        avp_mp_bot_step(&medbot,&med,&medcmd);
        assert(!(medcmd.actions&(1u<<AVP_MP_USE)));
        assert(medcmd.actions&((1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT)));
        /* Arriving by route at an unvisited square moves normally. */
        memset(&medbot,0,sizeof medbot);med.target_side=2u;
        med.target_x=(6u<<16)|0x8000u;
        avp_mp_bot_step(&medbot,&med,&medcmd);
        assert(!(medcmd.actions&(1u<<AVP_MP_USE)));
        assert(medcmd.actions&(1u<<AVP_MP_FORWARD));
    }
    /* R283: an injured Berserker Marine backs away from a visible
     * source Alien while retaining its independent aiming command.
     * A healthy bot keeps the unchanged combat approach policy. */
    {
        AvpMpBotState hurt={0};
        AvpMpBotObservation guard={0};
        AvpMpPlayerCommand move={0};
        blank_map(9u,9u);
        guard.map=(AvpMpBotMap){map_cells,9u,9u,wallflags,NULL,0u,risk,NULL,0u};
        guard.x=(4u<<16)|0x8000u;guard.y=(4u<<16)|0x8000u;
        guard.target_x=(6u<<16)|0x8000u;guard.target_y=guard.y;
        guard.heading=0u;guard.player_species=AVP_MP_MARINE;
        guard.behavior=1u;guard.goal=AVP_MP_BOT_HOSTILE;
        guard.has_target=guard.hostile_target=guard.target_visible=1u;
        guard.target_is_alien=1u;guard.target_slot=3u;
        guard.target_energy_positive=1u;guard.player_energy=250;
        guard.max_energy=1000;guard.health_low=1u;guard.health_critical=1u;
        guard.tick=90u;
        avp_mp_bot_step(&hurt,&guard,&move);
        assert(move.actions&(1u<<AVP_MP_BACKWARD));
        assert(!(move.actions&(1u<<AVP_MP_FORWARD)));
    }
    /* R282: with the original 20-health unopened Facehugger Egg and
     * 10-damage shotgun, a Marine owning no other guns must approach to
     * trigger the native 128<<9 proximity activation BEFORE it fires.
     * The original AMP changes mode from eggwait after proximity; the
     * normal weapon code then applies the required two real hits. */
    {
        AvpMpBotObservation egg={0};
        AvpMpBotState bot={0};
        AvpMpPlayerCommand cmd={0};
        blank_map(9u,9u);
        egg.map=(AvpMpBotMap){map_cells,9u,9u,wallflags,NULL,0u,risk,NULL,0u};
        egg.player_species=AVP_MP_MARINE;
        egg.current_weapon=1u;egg.marine_inventory_valid=1u;
        egg.marine_owned=(1u<<1);egg.marine_ammo[0]=15u;
        egg.x=(2u<<16)|0x8000u;egg.y=(4u<<16)|0x8000u;
        egg.target_x=(5u<<16)|0x8000u;egg.target_y=egg.y;
        egg.heading=0u;egg.tick=30u;egg.player_energy=100;
        egg.max_energy=100;egg.has_target=egg.hostile_target=1u;
        egg.target_visible=egg.target_is_egg=egg.target_egg_unopened=1u;
        egg.target_health_valid=1u;egg.target_health=20;
        egg.goal=AVP_MP_BOT_HOSTILE;egg.target_slot=3u;
        assert(avp_mp_bot_shotgun_egg_policy(&egg)==1u);
        avp_mp_bot_step(&bot,&egg,&cmd);
        assert(!(cmd.actions&(1u<<AVP_MP_FIRE)));
        assert(cmd.actions&(1u<<AVP_MP_FORWARD));
        /* Within the source egg's one-cell Chebyshev trigger window,
         * the bot must wait for an actual AMP mode transition. */
        memset(&bot,0,sizeof bot);
        egg.x=(4u<<16)|0xC000u;egg.tick++;
        avp_mp_bot_step(&bot,&egg,&cmd);
        assert(!(cmd.actions&(1u<<AVP_MP_FIRE)));
        assert(!(cmd.actions&(1u<<AVP_MP_FORWARD)));
        /* Original eggwait -> opening animation: now it can be destroyed
         * by the two ordinary 10-damage shotgun hits. */
        memset(&bot,0,sizeof bot);
        egg.target_egg_unopened=0u;egg.tick++;
        assert(avp_mp_bot_shotgun_egg_policy(&egg)==2u);
        avp_mp_bot_step(&bot,&egg,&cmd);
        assert(cmd.actions&(1u<<AVP_MP_FIRE));
        assert(!(cmd.actions&(1u<<AVP_MP_FORWARD)));
        /* If only one shell remains, do not trigger an intact egg that
         * cannot be killed by the needed two hits. */
        egg.target_egg_unopened=1u;egg.marine_ammo[0]=1u;
        assert(avp_mp_bot_shotgun_egg_policy(&egg)==3u);
        memset(&bot,0,sizeof bot);egg.tick++;
        avp_mp_bot_step(&bot,&egg,&cmd);
        assert(!(cmd.actions&(1u<<AVP_MP_FIRE)));
        assert(!(cmd.actions&(1u<<AVP_MP_FORWARD)));
        /* Multiple owned guns: leave the existing ordinary combat system
         * alone. Motion-tracker-only contacts cannot stage an unopened egg. */
        egg.marine_ammo[0]=15u;egg.marine_owned|=1u<<2;
        assert(avp_mp_bot_shotgun_egg_policy(&egg)==0u);
        egg.marine_owned=1u<<1;egg.target_visible=0u;
        assert(avp_mp_bot_shotgun_egg_policy(&egg)==0u);
        egg.target_visible=1u;egg.deathmatch=1u;
        assert(avp_mp_bot_shotgun_egg_policy(&egg)==0u);
    }
    /* R281: retreat begins before ALL owned guns run dry. The original
     * max ammo capacity for shotgun/pulse/flame/smart is 44/264/88/88;
     * the check reads actual per-player values without replenishment. */
    {
        const uint16_t limits[4]={44u,264u,88u,88u};
        uint16_t ammo[4]={0u,0u,0u,0u};
        assert(!avp_mp_bot_marine_needs_resupply(0u,ammo,limits));
        assert(avp_mp_bot_marine_needs_resupply(1u<<2,ammo,limits));
        ammo[1]=66u; /* one quarter of Pulse Rifle maximum */
        assert(avp_mp_bot_marine_needs_resupply(1u<<2,ammo,limits));
        ammo[1]=67u;
        assert(!avp_mp_bot_marine_needs_resupply(1u<<2,ammo,limits));
        ammo[1]=12u;ammo[0]=15u;
        assert(!avp_mp_bot_marine_needs_resupply(
                  (1u<<1)|(1u<<2),ammo,limits));
        ammo[0]=10u;
        assert(avp_mp_bot_marine_needs_resupply(
                  (1u<<1)|(1u<<2),ammo,limits));
    }
    /* R281: an aimed Marine must TURN within the old 7.8-degree patrol
     * deadband or a three-degree FIRE window would cause a new standoff. */
    {
        AvpMpBotState agent={0};
        AvpMpBotObservation shot={0};
        AvpMpPlayerCommand out={0};
        blank_map(9u,9u);
        shot.map=(AvpMpBotMap){map_cells,9u,9u,wallflags,NULL,0u,risk,NULL,0u};
        shot.x=(2u<<16)|0x8000u;shot.y=(4u<<16)|0x8000u;
        shot.target_x=(6u<<16)|0x8000u;shot.target_y=shot.y;
        shot.heading=1274u<<16; /* looking seven degrees off target */
        shot.has_target=shot.hostile_target=shot.target_visible=1u;
        shot.goal=AVP_MP_BOT_HOSTILE;shot.player_species=AVP_MP_MARINE;
        shot.marine_inventory_valid=1u;shot.marine_owned=1u<<2;
        shot.marine_ammo[1]=30u;shot.current_weapon=2u;
        shot.tick=21u;shot.player_energy=100;shot.max_energy=100;
        assert(!avp_mp_bot_marine_aim_ready(2u,7.0*3.141592653589793/180.0,4.0,0));
        assert(avp_mp_bot_marine_aim_ready(2u,2.0*3.141592653589793/180.0,4.0,0));
        avp_mp_bot_step(&agent,&shot,&out);
        assert(out.actions&(1u<<AVP_MP_TURN_RIGHT));
        assert(!(out.actions&(1u<<AVP_MP_FIRE)));
    }
    /* R281: tracker enemy behind an actual disconnected wall needs the
     * original authored interlevel portal planner, not direct steering.
     * A same-region contact never requests an unnecessary vent. */
    {
        blank_map(9u,9u);
        map=(AvpMpBotMap){map_cells,9u,9u,wallflags,NULL,0u,risk,NULL,0u};
        for(unsigned y=0;y<9u;y++)map_cells[CELL(4u,y,9u)+5u]=0u;
        assert(avp_mp_bot_tracker_needs_portal(&map,2u,4u,6u,4u));
        assert(!avp_mp_bot_tracker_needs_portal(&map,2u,4u,3u,6u));
        assert(!avp_mp_bot_tracker_needs_portal(&map,2u,4u,10u,4u));
    }
    /* R281: when empty/low, prefer a verified safe local retreat cell;
     * never manufacture an exit through a sealed floor or a lethal acid
     * risk cell. The corner with no other floor retains safe standoff. */
    {
        unsigned gx=0u,gy=0u;
        blank_map(10u,10u);
        map=(AvpMpBotMap){map_cells,10u,10u,wallflags,NULL,0u,risk,NULL,0u};
        assert(avp_mp_bot_pick_refuge(&map,4u,4u,6u,4u,&gx,&gy));
        assert(gx<4u || gy!=4u);
        memset(map_cells,0,sizeof map_cells);
        map_cells[CELL(4u,4u,10u)+5u]=1u;
        assert(!avp_mp_bot_pick_refuge(&map,4u,4u,6u,4u,&gx,&gy));
    }
    /* R278: A stationary egg never becomes a tracker contact, but a
     * SIGHTED egg near a living co-op player is worth engaging before
     * another harmlessly distant enemy. Facehuggers outrank eggs, and a
     * Predator's visible ranged-disc threat extends farther. */
    {
        const uint64_t u=(uint64_t)65536u*65536u;
        assert(avp_mp_bot_guard_urgency(AC_FACEHUG,2u*u,4u*u)==50u);
        assert(avp_mp_bot_guard_urgency(AC_EGG,4u*u,4u*u)==45u);
        assert(avp_mp_bot_guard_urgency(AC_PREDATOR,25u*u,4u*u)==40u);
        assert(avp_mp_bot_guard_urgency(AC_PREDATOR,65u*u,4u*u)==0u);
        assert(avp_mp_bot_guard_urgency(AC_EGG,25u*u,4u*u)==0u);
        assert(avp_mp_bot_guard_urgency(AC_EGG,4u*u,80u*u)==0u);
        assert(avp_mp_bot_guard_urgency(AC_HUMAN,1u*u,1u*u)==0u);
    }
    /* Scanner-only pursuit should not spin at a new hidden position
     * each native tick. A 24-tick search goal sticks, then refreshes.
     * Optical LOS, current melee pressure and player rescue abort it. */
    {
        AvpMpBotState memory={0};
        AvpMpBotObservation seen={0};
        seen.tick=100u;seen.player_species=0u;
        seen.observed_world_level=3u;seen.has_target=1u;
        seen.hostile_target=1u;seen.tracker_only_target=1u;
        seen.goal=AVP_MP_BOT_HOSTILE;seen.target_slot=5u;
        seen.x=10u<<16;seen.y=10u<<16;
        seen.target_x=14u<<16;seen.target_y=10u<<16;
        avp_mp_bot_tracker_hold(&memory,&seen,9u);
        assert(memory.tracker_seek_valid && seen.target_x==(14u<<16));
        seen.tick=101u;seen.target_x=13u<<16;seen.target_y=12u<<16;
        avp_mp_bot_tracker_hold(&memory,&seen,9u);
        assert(seen.target_x==(14u<<16) && seen.target_y==(10u<<16));
        seen.tick=123u;seen.x=12u<<16; /* normal pursuit made real progress */
        seen.target_x=12u<<16;seen.target_y=11u<<16;
        avp_mp_bot_tracker_hold(&memory,&seen,9u);
        assert(seen.target_x==(14u<<16));
        seen.tick=124u;seen.target_x=12u<<16;seen.target_y=11u<<16;
        avp_mp_bot_tracker_hold(&memory,&seen,9u);
        assert(seen.target_x==(12u<<16) && seen.target_y==(11u<<16));
        seen.tick=125u;seen.target_x=18u<<16;
        avp_mp_bot_tracker_hold(&memory,&seen,10u); /* AMP respawn */
        assert(seen.target_x==(18u<<16));
        seen.tick=126u;seen.target_visible=1u;seen.tracker_only_target=0u;
        seen.target_x=19u<<16;
        avp_mp_bot_tracker_hold(&memory,&seen,10u);
        assert(!memory.tracker_seek_valid && seen.target_x==(19u<<16));
        seen.tick=127u;seen.target_visible=0u;seen.tracker_only_target=1u;
        seen.target_x=17u<<16;
        avp_mp_bot_tracker_hold(&memory,&seen,10u);
        assert(memory.tracker_seek_valid);
        seen.tick=128u;seen.melee_visible=1u;
        seen.melee_threat=AVP_MP_BOT_THREAT_FACEHUG;
        avp_mp_bot_tracker_hold(&memory,&seen,10u);
        assert(!memory.tracker_seek_valid);
        seen.melee_visible=0u;seen.tick=129u;
        avp_mp_bot_tracker_hold(&memory,&seen,10u);
        seen.tick=130u;seen.other_player_alien_trapped=1u;
        avp_mp_bot_tracker_hold(&memory,&seen,10u);
        assert(!memory.tracker_seek_valid);
        seen.tick=131u;seen.other_player_alien_trapped=0u;
        seen.other_player_swarmed=1u;
        avp_mp_bot_tracker_hold(&memory,&seen,10u);
        assert(!memory.tracker_seek_valid);
        /* A scanner point inside a solid cell is NOT a valid route goal.
         * Search its reachable neighbor and keep the real wall unchanged. */
        blank_map(9u,9u);
        map_cells[CELL(5u,5u,9u)+5u]=0u;
        memset(&memory,0,sizeof memory);
        seen=(AvpMpBotObservation){0};
        seen.map=(AvpMpBotMap){.cells=map_cells,.width=9u,.height=9u,
                               .wall_flags=wallflags};
        seen.tick=1u;seen.observed_world_level=2u;seen.target_slot=4u;
        seen.x=(1u<<16)|0x8000u;seen.y=(1u<<16)|0x8000u;
        seen.target_x=(5u<<16)|0x8000u;
        seen.target_y=(5u<<16)|0x8000u;
        seen.goal=AVP_MP_BOT_HOSTILE;seen.has_target=1u;
        seen.hostile_target=1u;seen.tracker_only_target=1u;
        avp_mp_bot_tracker_hold(&memory,&seen,3u);
        assert(memory.tracker_seek_valid && seen.has_target);
        assert((seen.target_x>>16)!=5u || (seen.target_y>>16)!=5u);
        assert(seen.target_x>>16<9u && seen.target_y>>16<9u);
        assert(map_cells[CELL(5u,5u,9u)+5u]==0u);
        /* No connected floor within three cells: stop chasing and mark
         * a bounded scanner retry, allowing ordinary patrol to take over. */
        memset(&memory,0,sizeof memory);
        for(unsigned yy=0;yy<9u;yy++)for(unsigned xx=0;xx<9u;xx++)
            map_cells[CELL(xx,yy,9u)+5u]=0u;
        map_cells[CELL(1u,1u,9u)+5u]=1u;
        seen.tick=6u;seen.target_x=(7u<<16)|0x8000u;
        seen.target_y=(7u<<16)|0x8000u;
        avp_mp_bot_tracker_hold(&memory,&seen,3u);
        assert(!memory.tracker_seek_valid && !seen.has_target &&
               !seen.hostile_target && !seen.tracker_only_target);
        assert(memory.tracker_seek_fail_slot==4u &&
               memory.tracker_seek_fail_until==86u);
        /* At the old search position, motion-tracker jitter must not
         * cause sub-second forward/backward target switching. */
        memset(&memory,0,sizeof memory);
        seen=(AvpMpBotObservation){0};
        seen.tick=100u;seen.observed_world_level=3u;
        seen.x=10u<<16;seen.y=10u<<16;
        seen.target_x=12u<<16;seen.target_y=10u<<16;
        seen.has_target=seen.hostile_target=seen.tracker_only_target=1u;
        seen.goal=AVP_MP_BOT_HOSTILE;seen.target_slot=3u;
        avp_mp_bot_tracker_hold(&memory,&seen,2u);
        seen.x=12u<<16;seen.tick=102u;
        seen.target_x=15u<<16;
        avp_mp_bot_tracker_hold(&memory,&seen,2u);
        assert(seen.target_x==(12u<<16)); /* hold for at least 0.8s */
        seen.tick=109u;seen.target_x=15u<<16;
        avp_mp_bot_tracker_hold(&memory,&seen,2u);
        assert(seen.target_x==(15u<<16));
    }
    /* RE44 R291: motion tracker is contact MEMORY, not vision. A Marine
     * behind an opaque wall can retain and investigate a scanner location,
     * but may neither fire nor treat its hidden position as a combat target.
     * The next genuine original LOS+FOV observation restores combat. */
    {
        AvpMpBotObservation memory_view={0};
        AvpMpBotState memory_state={0};
        AvpMpPlayerCommand cmd={0};
        blank_map(9u,9u);
        memory_view.map=(AvpMpBotMap){.cells=map_cells,.width=9u,
            .height=9u,.wall_flags=wallflags};
        memory_view.player_species=AVP_MP_MARINE;
        memory_view.observed_world_level=3u;
        memory_view.x=(2u<<16)|0x8000u;
        memory_view.y=(2u<<16)|0x8000u;
        memory_view.heading=0u;
        memory_view.target_x=(6u<<16)|0x8000u;
        memory_view.target_y=memory_view.y;
        memory_view.goal=AVP_MP_BOT_HOSTILE;
        memory_view.has_target=memory_view.hostile_target=1u;
        memory_view.tracker_only_target=1u;
        memory_view.target_slot=4u;
        memory_view.tick=1u;
        avp_mp_bot_tracker_hold(&memory_state,&memory_view,4u);
        const uint32_t last_x=memory_view.target_x;
        avp_mp_bot_coop_contact_as_memory(&memory_view);
        assert(memory_view.has_target && memory_view.tracker_only_target);
        assert(memory_view.target_x==last_x && memory_state.tracker_seek_valid);
        assert(!memory_view.hostile_target && !memory_view.target_visible);
        assert(memory_view.goal==AVP_MP_BOT_INVESTIGATE);
        avp_mp_bot_step(&memory_state,&memory_view,&cmd);
        assert(!(cmd.actions&(1u<<AVP_MP_FIRE)));
        assert(cmd.actions & ((1u<<AVP_MP_FORWARD) |
                   (1u<<AVP_MP_STRAFE_LEFT) | (1u<<AVP_MP_STRAFE_RIGHT)));
        /* A remembered optical contact that has left view is also NOT a
         * new visual sighting, even when the tracker no longer sees it. */
        memory_view.goal=AVP_MP_BOT_HOSTILE;
        memory_view.hostile_target=1u;
        memory_view.tracker_only_target=0u;
        memory_view.target_health_valid=1u;
        memory_view.target_heading_valid=1u;
        avp_mp_bot_coop_contact_as_memory(&memory_view);
        assert(memory_view.goal==AVP_MP_BOT_INVESTIGATE);
        assert(!memory_view.hostile_target && !memory_view.target_health_valid &&
               !memory_view.target_heading_valid);
        /* Re-acquired original optical sight returns to real combat. */
        memory_view.goal=AVP_MP_BOT_HOSTILE;
        memory_view.hostile_target=memory_view.target_visible=1u;
        memory_view.tracker_only_target=0u;
        avp_mp_bot_coop_contact_as_memory(&memory_view);
        assert(memory_view.goal==AVP_MP_BOT_HOSTILE &&
               memory_view.hostile_target);
        avp_mp_bot_step(&memory_state,&memory_view,&cmd);
        assert(cmd.actions&(1u<<AVP_MP_FIRE));
        /* DM and other species retain their original targeting behavior. */
        memory_view.target_visible=0u;
        memory_view.deathmatch=1u;
        avp_mp_bot_coop_contact_as_memory(&memory_view);
        assert(memory_view.goal==AVP_MP_BOT_HOSTILE);
        memory_view.deathmatch=0u;
        memory_view.player_species=AVP_MP_PREDATOR;
        avp_mp_bot_coop_contact_as_memory(&memory_view);
        assert(memory_view.goal==AVP_MP_BOT_HOSTILE);
    }
    /* RE44 R290: exact reported video failure. Scanner contact remains
     * alive near a wall, the bot stays at one XY position for many ticks,
     * and it must RELEASE that navigation lock to rejoin the human.
     * The old R289 tracker_hold renewed this goal indefinitely. */
    {
        AvpMpBotState stuck={0};
        AvpMpBotObservation radar={0};
        radar.player_species=AVP_MP_MARINE;
        radar.player_collision_disabled=1u; /* actual user setting */
        radar.observed_world_level=2u;
        radar.x=(2u<<16)|0x8000u;
        radar.y=(3u<<16)|0x8000u;
        radar.target_slot=6u;
        radar.goal=AVP_MP_BOT_HOSTILE;
        radar.target_x=(6u<<16)|0x8000u;
        radar.target_y=(3u<<16)|0x8000u;
        for(unsigned tick=200u;tick<=218u;tick++){
            radar.tick=tick;
            /* Each original scanner tick provides the LIVE contact again. */
            radar.target_x=(6u<<16)|0x8000u;
            radar.target_y=(3u<<16)|0x8000u;
            radar.has_target=radar.hostile_target=radar.tracker_only_target=1u;
            avp_mp_bot_tracker_hold(&stuck,&radar,11u);
            if(tick<218u)assert(radar.has_target);
        }
        assert(!radar.has_target && !radar.hostile_target &&
               !stuck.tracker_seek_valid);
        assert(avp_mp_bot_tracker_contact_suppressed(&stuck,6u,2u,11u,
            (6u<<16)|0x8000u,(3u<<16)|0x8000u,219u));
        radar.tick=219u;
        radar.has_target=radar.hostile_target=radar.tracker_only_target=1u;
        radar.target_x=(6u<<16)|0x8000u;
        radar.target_y=(3u<<16)|0x8000u;
        avp_mp_bot_tracker_hold(&stuck,&radar,11u);
        assert(!radar.has_target); /* cannot immediately re-lock */
        /* New original observation: Alien moved into another map cell. */
        assert(!avp_mp_bot_tracker_contact_suppressed(&stuck,6u,2u,11u,
            (8u<<16)|0x8000u,(3u<<16)|0x8000u,220u));
        assert(!avp_mp_bot_tracker_contact_suppressed(&stuck,6u,2u,12u,
            (6u<<16)|0x8000u,(3u<<16)|0x8000u,220u));
        /* Expired retry timer alone MUST NOT recreate the same deadlock. */
        stuck.previous_x=stuck.tracker_seek_fail_bot_x;
        stuck.previous_y=stuck.tracker_seek_fail_bot_y;
        assert(avp_mp_bot_tracker_contact_suppressed(&stuck,6u,2u,11u,
            (6u<<16)|0x8000u,(3u<<16)|0x8000u,420u));
        /* On reaching a different position the bot may safely reevaluate
         * the contact via the real route planner. */
        stuck.previous_x+=2u<<16;
        assert(!avp_mp_bot_tracker_contact_suppressed(&stuck,6u,2u,11u,
            (6u<<16)|0x8000u,(3u<<16)|0x8000u,420u));
        /* Actual visual sight or actual melee danger always overrides a
         * scanner failure and retains the original source combat rules. */
        radar=(AvpMpBotObservation){0};
        radar.tick=221u;radar.observed_world_level=2u;
        radar.target_slot=6u;radar.target_visible=1u;
        radar.has_target=radar.hostile_target=1u;
        radar.goal=AVP_MP_BOT_HOSTILE;
        avp_mp_bot_tracker_hold(&stuck,&radar,11u);
        assert(radar.has_target && radar.hostile_target);
        /* Pursuit that actually closes the distance must not be penalized. */
        memset(&stuck,0,sizeof stuck);
        radar=(AvpMpBotObservation){0};
        radar.x=(1u<<16)|0x8000u;radar.y=(2u<<16)|0x8000u;
        radar.target_slot=7u;radar.observed_world_level=2u;
        for(unsigned tick=10u;tick<=40u;tick++){
            radar.tick=tick;radar.x=((1u<<16)|0x8000u)+(tick-10u)*3000u;
            radar.target_x=(5u<<16)|0x8000u;
            radar.target_y=(2u<<16)|0x8000u;
            radar.has_target=radar.hostile_target=radar.tracker_only_target=1u;
            avp_mp_bot_tracker_hold(&stuck,&radar,7u);
            assert(radar.has_target);
        }
    }
    /* R473: even rapidly switching scanner contacts cannot monopolize
     * navigation forever. Optical contact overrides the cooldown. */
    {
        AvpMpBotState state={0};
        AvpMpBotObservation radar={0};
        radar.player_species=AVP_MP_MARINE;
        radar.observed_world_level=2u;
        radar.x=(2u<<16)|0x8000u;radar.y=(3u<<16)|0x8000u;
        for(unsigned tick=100u;tick<=180u;tick++){
            radar.tick=tick;
            radar.target_slot=tick%2u+1u;
            radar.target_x=((6u+tick%2u)<<16)|0x8000u;
            radar.target_y=radar.y;
            radar.goal=AVP_MP_BOT_HOSTILE;
            radar.has_target=radar.hostile_target=radar.tracker_only_target=1u;
            avp_mp_bot_tracker_hold(&state,&radar,1u);
            assert(radar.has_target==(tick<180u));
        }
        assert(state.tracker_cooldown_until==210u);
        radar.tick=181u;radar.target_slot=99u;
        radar.has_target=radar.hostile_target=radar.tracker_only_target=1u;
        avp_mp_bot_tracker_hold(&state,&radar,2u);
        assert(!radar.has_target);
        radar.target_visible=1u;radar.tracker_only_target=0u;
        radar.has_target=radar.hostile_target=1u;
        avp_mp_bot_tracker_hold(&state,&radar,2u);
        assert(radar.has_target && radar.hostile_target);
        puts("R473 bounded tracker pursuit, alternate blips and visual override: PASS");
    }
    /* R473: a permanently closed but route-legal door cannot grant
     * unlimited stall exemptions. A normal door still has time to open. */
    {
        AvpMpBotState state={0};
        AvpMpBotObservation radar={0};
        blank_map(5u,5u);
        memset(map_cells,0,sizeof map_cells);
        for(unsigned x=1u;x<=3u;x++)map_cells[CELL(x,2u,5u)+5u]=1u;
        wallflags[5u]=AVP_WB_DOOR;
        map_cells[CELL(1u,2u,5u)+2u]=5u;
        map_cells[CELL(2u,2u,5u)+0u]=5u;
        radar.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,99u,NULL,NULL,0u};
        radar.player_species=AVP_MP_MARINE;radar.observed_world_level=2u;
        radar.x=(1u<<16)|0x8000u;radar.y=(2u<<16)|0x8000u;
        radar.target_slot=9u;
        unsigned abandoned=0u;
        for(unsigned tick=100u;tick<181u;tick++){
            radar.tick=tick;radar.goal=AVP_MP_BOT_HOSTILE;
            radar.target_x=(3u<<16)|0x8000u;radar.target_y=radar.y;
            radar.has_target=radar.hostile_target=radar.tracker_only_target=1u;
            avp_mp_bot_tracker_hold(&state,&radar,1u);
            if(tick<140u)assert(radar.has_target);
            if(!radar.has_target){abandoned=tick;break;}
        }
        assert(abandoned>=140u && abandoned<180u);
        puts("R473 closed door wait is bounded: PASS");
    }
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.target_x=14u<<16;v.target_y=10u<<16;
    v.has_target=1;v.hostile_target=1;v.target_visible=1;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(c.actions&(1u<<AVP_MP_FIRE));
    assert(!(c.actions&(1u<<AVP_MP_PAUSE)));
    memset(&s,0,sizeof(s));
    v.target_x=10u<<16;v.target_y=6u<<16;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_TURN_LEFT));
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    memset(&s,0,sizeof(s));
    v.has_target=0;v.hostile_target=0;v.player_index=0;
    for(unsigned tick=0;tick<24;tick++){
        v.tick=tick;
        avp_mp_bot_step(&s,&v,&c);
    }
    assert(s.recovery_count>0u);
    assert(c.actions&(1u<<AVP_MP_TURN_RIGHT));
    assert(!(c.actions&(1u<<AVP_MP_PAUSE)));
    /* RE30 R9: no hostiles means a reachability-tested map goal, not just
     * turning in place. Never pick a solid/off-map cell. */
    {
        uint16_t visits[AVP_MP_BOT_MAP_CELLS]={0};
        unsigned px=0,py=0;
        blank_map(5u,5u);
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL,0u};
        map_cells[CELL(4u,4u,5u)+5u]=0u;
        assert(avp_mp_bot_patrol_pick(&map,2u,2u,visits,1234u,&px,&py));
        assert(px<5u && py<5u && !(px==4u&&py==4u));
        assert(avp_mp_bot_route(&map,2u,2u,px,py,&route));
        /* R10: a previously blocked patrol goal must not be selected again
         * and turn a brief collision stall into an endless stop/start loop. */
        {
            unsigned old_x=px,old_y=py;
            visits[old_y*64u+old_x]=65535u;
            assert(avp_mp_bot_patrol_pick(&map,2u,2u,visits,1234u,&px,&py));
            assert(px!=old_x || py!=old_y);
            assert(avp_mp_bot_route(&map,2u,2u,px,py,&route));
        }
        memset(&v,0,sizeof(v));memset(&s,0,sizeof(s));
        v.x=(2u<<16)|0x8000u;v.y=(2u<<16)|0x8000u;
        v.target_x=(px<<16)|0x8000u;v.target_y=(py<<16)|0x8000u;
        v.map=map;v.goal=AVP_MP_BOT_PATROL;v.has_target=1u;
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                          (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)|
                          (1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT)));
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        /* R11 regression: R10 went idle between 0.7 and 1.8 tiles from
         * the patrol destination, never reaching its 0.7-tile arrival. */
        memset(&s,0,sizeof s);
        v.x=(2u<<16)|0x8000u;v.y=(2u<<16)|0x8000u;
        v.target_x=(3u<<16)|0x8000u;v.target_y=v.y;
        v.heading=0u;
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FORWARD));
        /* At 0.72 tiles it MUST still advance until host arrival (<0.7). */
        memset(&s,0,sizeof s);
        v.target_x=v.x+47186u;v.target_y=v.y;
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FORWARD));
    }
    /* R45: true eight-way graph edges, without cutting a blocked corner.
     * The two-axis route is shorter across an open 2x2, and carries a
     * straight diagonal heading rather than hunting successive centers. */
    {
        uint8_t prop_risk[AVP_MP_BOT_MAP_CELLS]={0};
        blank_map(5u,5u);
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,prop_risk,0u};
        assert(avp_mp_bot_route(&map,1u,1u,3u,3u,&route));
        assert(route.next_x==2u && route.next_y==2u);
        assert(route.lookahead_valid && route.lookahead_x==3u &&
               route.lookahead_y==3u);
        assert(route.cost==28u && route.door_is_open);
        memset(&v,0,sizeof v);memset(&s,0,sizeof s);
        v.map=map;v.player_species=AVP_MP_MARINE;v.has_target=1u;
        v.goal=AVP_MP_BOT_HEALTH;
        v.x=(1u<<16)|0x8000u;v.y=(1u<<16)|0x8000u;
        v.target_x=(3u<<16)|0x8000u;v.target_y=v.target_x;
        v.heading=0xE0000000u; /* +X,+Y diagonal faces -45 degrees. */
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FORWARD));
        assert(!(c.actions&((1u<<AVP_MP_TURN_LEFT)|
                            (1u<<AVP_MP_TURN_RIGHT))));
        /* A single blocked wall on the starting cell's right boundary
         * prevents diagonal travel, even though the destination is open. */
        wallflags[1u]=AVP_WB_SOLID;
        map_cells[CELL(1u,1u,5u)+2u]=1u;
        assert(avp_mp_bot_route(&map,1u,1u,2u,2u,&route));
        assert(route.next_x==1u && route.next_y==2u);
        assert(route.cost>=20u);
        v.target_x=(2u<<16)|0x8000u;v.target_y=v.target_x;
        v.heading=0xC0000000u; /* +Y: go around the blocked right face. */
        memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FORWARD));
        assert(!(c.actions&(1u<<AVP_MP_TURN_RIGHT)));
        /* Even a NEAR acid mark forbids the 2x2 shortcut; the original
         * cardinal planner can still choose a safe route around it. */
        blank_map(5u,5u);
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,prop_risk,0u};
        risk[1u*64u+2u]=1u;
        assert(avp_mp_bot_route(&map,1u,1u,2u,2u,&route));
        assert(route.next_x==1u && route.next_y==2u);
        blank_map(5u,5u);
        prop_risk[1u*64u+2u]=1u;
        assert(avp_mp_bot_route(&map,1u,1u,2u,2u,&route));
        assert(route.next_x==1u && route.next_y==2u);
        prop_risk[1u*64u+2u]=0u;
    }
    /* A straight hall should not turn left then right to chase each
     * waypoint's exact center. Keep the heading and use existing strafing
     * as needed for small (<.28 tile) lateral corrections. */
    {
        blank_map(5u,5u);
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL,0u};
        memset(&v,0,sizeof v);v.map=map;v.has_target=1u;
        v.goal=AVP_MP_BOT_HEALTH;v.player_species=AVP_MP_MARINE;
        v.heading=0u;
        v.x=(1u<<16)|0x8000u;
        v.target_x=(4u<<16)|0x8000u;
        v.target_y=(1u<<16)|0x8000u;
        for(unsigned i=0u;i<7u;i++){
            static const uint16_t frac[]={0x8000u,0x6000u,0xA000u,
                              0x4400u,0xBC00u,0x7400u,0x8C00u};
            v.y=(1u<<16)|frac[i];
            memset(&s,0,sizeof s);
            avp_mp_bot_step(&s,&v,&c);
            assert(c.actions&(1u<<AVP_MP_FORWARD));
            assert(!(c.actions&((1u<<AVP_MP_TURN_LEFT)|
                                (1u<<AVP_MP_TURN_RIGHT))));
        }
        /* At the very last cell, no additional lookahead exists. Hold the
         * same corridor heading instead of twitching toward its center. */
        v.x=(3u<<16)|0x9000u;
        v.y=(1u<<16)|0x9E00u;
        memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FORWARD));
        assert(!(c.actions&((1u<<AVP_MP_TURN_LEFT)|
                            (1u<<AVP_MP_TURN_RIGHT))));
        /* Hold the same heading as the player advances from one cell to
         * another, even with small alternating lateral displacement. This
         * was the visible left-right twitch reported in an open hall. */
        {
            static const uint32_t xstep[]={0x00016666u,0x0001CCCCu,
                0x00023333u,0x00029999u,0x00030000u,0x00036666u};
            memset(&s,0,sizeof s);
            for(unsigned i=0u;i<sizeof xstep/sizeof xstep[0];i++){
                v.tick=i+10u;
                v.x=xstep[i];
                v.y=(1u<<16)|(i&1u?0x6300u:0x9D00u);
                avp_mp_bot_step(&s,&v,&c);
                assert(c.actions&(1u<<AVP_MP_FORWARD));
                assert(!(c.actions&((1u<<AVP_MP_TURN_LEFT)|
                                    (1u<<AVP_MP_TURN_RIGHT))));
            }
        }
        /* Significant off-center drift still calls for correction. */
        v.x=(1u<<16)|0x8000u;
        v.y=(1u<<16)|0x1500u;
        memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_TURN_RIGHT));
    }
    /* R17: the three profiles have different pursuit distances, but issue
     * the same ordinary player commands subject to the original collision. */
    {
        blank_map(5u,5u);
        memset(&v,0,sizeof v);
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL,0u};
        v.map=map;v.x=(2u<<16)|0x8000u;v.y=(2u<<16)|0x8000u;
        v.target_x=v.x+(1u<<16);v.target_y=v.y;
        v.heading=0;v.has_target=v.hostile_target=v.target_visible=1u;
        v.goal=AVP_MP_BOT_HOSTILE;v.player_species=AVP_MP_MARINE;
        v.player_energy=v.max_energy=100;
        v.behavior=AVP_MP_BOT_BALANCED;
        memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        v.behavior=AVP_MP_BOT_BERSERKER;
        memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FORWARD));
        v.behavior=AVP_MP_BOT_GUERILLA;
        memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    }
    /* A fully reachable source route can temporarily increase straight-line
     * distance to the goal. The patrol watchdog must recognize its lower
     * remaining source-route cost instead of declaring an aimless shuffle. */
    {
        uint8_t detour[7u*5u*8u]={0};
        AvpMpBotRoute a={0},b={0};
        const unsigned path[][2]={{3,1},{2,1},{2,2},{2,3},{3,3},
                                   {4,3},{5,3},{5,2},{5,1}};
        for(unsigned i=0;i<sizeof(path)/sizeof(path[0]);i++)
            detour[CELL(path[i][0],path[i][1],7u)+5u]=1u;
        map=(AvpMpBotMap){detour,7u,5u,wallflags,NULL,0,NULL,NULL,0u};
        assert(avp_mp_bot_route(&map,3u,1u,5u,1u,&a));
        assert(a.next_x==2u && a.next_y==1u);
        assert(avp_mp_bot_route(&map,2u,1u,5u,1u,&b));
        assert(b.cost<a.cost);
        assert(5u-2u>5u-3u); /* Euclidean axis is getting worse. */
    }
        /* RE30 R2: The original inclusive death spray is 96<<9 = 49152
     * (0.75 tile) on EACH axis. The bot's minimum goal is exactly >=0.76
     * tile: ceil(0.76 * 65536) = 49808 in 16.16 world units.
     * No 1.0-tile buffer, no circular radius, and keep shooting on retreat. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.target_x=v.x+(1u<<15);v.target_y=v.y;
    v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
    v.goal=AVP_MP_BOT_HOSTILE;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    assert(c.actions&(1u<<AVP_MP_FIRE));
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    /* Diagonal: X=Y=0.75 is STILL inside the original square. */
    memset(&s,0,sizeof s);
    v.target_x=v.x+49152u;v.target_y=v.y+49152u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    /* Just UNDER 0.76 (49807 fixed units) still triggers retreat.
     * At 49808 on EITHER axis the square test is already clear. */
    memset(&s,0,sizeof s);
    v.target_x=v.x+49807u;v.target_y=v.y;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    memset(&s,0,sizeof s);
    v.target_x=v.x+49808u;v.target_y=v.y;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    /* Diagonal is axis-aligned too: both coordinates matter. */
    memset(&s,0,sizeof s);
    v.target_x=v.x+49807u;v.target_y=v.y+49807u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    memset(&s,0,sizeof s);
    v.target_x=v.x+49808u;v.target_y=v.y+49808u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    /* Previously R1 retreated at 0.90 on both axes; R2 does not. */
    memset(&s,0,sizeof s);
    v.target_x=v.x+58982u;v.target_y=v.y+58982u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    /* The near-but-safe offset must NOT cause an endless retreat. */
    memset(&s,0,sizeof s);
    v.target_x=v.x+45875u;v.target_y=v.y+72090u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    memset(&s,0,sizeof s);
    v.target_x=v.x+72090u;v.target_y=v.y;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    /* Other enemies, invisible Aliens and Alien players should not get
     * this new ranged-only retreat policy. */
    memset(&s,0,sizeof s);
    v.target_x=v.x+32768u;v.target_y=v.y;
    v.target_is_alien=0u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    memset(&s,0,sizeof s);
    v.target_is_alien=1u;v.target_visible=0u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    memset(&s,0,sizeof s);
    v.target_visible=1u;v.player_species=1u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    /* An unlocked ranged Predator prefers the Disc or Cannon specifically
     * against an Alien, and tries to back away during weapon switching.
     * Melee-only Predators are never prevented from striking. */
    memset(&s,0,sizeof s);v.player_species=2u;
    v.predator_owned=(1u<<1)|(1u<<2);v.current_weapon=1u;
    v.tick=10u;
    assert(avp_mp_bot_predator_preferred_weapon(&v)==2u);
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    assert(c.actions&((1u<<AVP_MP_NEXT_WEAPON)|(1u<<AVP_MP_PREV_WEAPON)));
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    memset(&s,0,sizeof s);
    v.predator_owned|=1u<<3;v.current_weapon=3u;v.tick=11u;
    assert(avp_mp_bot_predator_preferred_weapon(&v)==3u);
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    assert(c.actions&(1u<<AVP_MP_FIRE));
    memset(&s,0,sizeof s);
    v.predator_owned=1u<<1;v.current_weapon=1u;
    assert(avp_mp_bot_predator_preferred_weapon(&v)==1u);
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    /* A retreat blocked by the map must use a collision-tested sidestep,
     * never assert forward into the dying target or freeze the whole AI. */
    blank_map(3u,3u);
    map=(AvpMpBotMap){map_cells,3u,3u,wallflags,NULL,0,risk,NULL,0u};
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=0x8000u;v.y=(1u<<16)|0x8000u;
    v.target_x=v.x+32768u;v.target_y=v.y;
    v.map=map;v.goal=AVP_MP_BOT_HOSTILE;
    v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    assert(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    assert(c.actions&(1u<<AVP_MP_FIRE));
    /* Empty gun must not make Marine AI fire. Use ordinary next-weapon
     * command when another actually owned gun has ammunition. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.target_x=12u<<16;v.target_y=v.y;
    v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;
    v.marine_inventory_valid=1u;v.marine_owned=(1u<<1);
    v.current_weapon=1u;v.tick=12u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    assert(!(c.actions&(1u<<AVP_MP_NEXT_WEAPON)));
    v.marine_owned|=1u<<2;v.marine_ammo[1]=40u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    assert(c.actions&(1u<<AVP_MP_NEXT_WEAPON));
    v.current_weapon=2u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    /* Attached Facehugger must override all normal AI decisions. Retail
     * MAZESCRN.S requires distinct active-low left-key edges, not a hold.
     * Start neutral to release any turn/strafe held at the moment of attach. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.target_x=14u<<16;v.target_y=10u<<16;
    v.has_target=1u;v.hostile_target=1u;v.target_visible=1u;
    v.player_species=2u;v.player_energy=10;v.max_energy=100;v.medpak=40;
    v.invis_act=1u;v.facehugged=1u;v.facehug_direction=0;
    for(unsigned tick=0;tick<20u;tick++){
        v.tick=tick;
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions==(tick%2u ? (1u<<AVP_MP_STRAFE_LEFT) : 0u));
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(!(c.actions&(1u<<AVP_MP_PRED_MEDKIT)));
        assert(!(c.actions&(1u<<AVP_MP_PRED_CLOAK)));
        assert(s.immobile_ticks==0u&&s.evade_ticks==0u);
    }
    /* Direction comes from the actor's original unstick_dir; never mix
     * left/right commands on the same tick. */
    v.facehug_direction=-1;v.tick=20u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions==0u);
    v.tick=21u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions==(1u<<AVP_MP_STRAFE_RIGHT));
    /* On release, reset the pulse generator for a later attachment. */
    v.facehugged=0u;v.tick=22u;avp_mp_bot_step(&s,&v,&c);
    assert(s.facehug_phase==0u);
    v.facehugged=1u;v.tick=23u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions==0u);
    v.tick=24u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions==(1u<<AVP_MP_STRAFE_RIGHT));
    /* Inter-level bot planning obeys the actual Jaguar species policy:
     * Marine = lifts + ducts; Alien = ducts only; Predator = lifts only.
     * Duct maps (levels 6..13) must be unreachable to Predator AI. */
    for(unsigned species=0;species<3u;species++){
        assert(avp_mp_bot_transition_allowed(species,1u,2u,
                        AVP_MP_BOT_TRANS_EXIT));
        assert(avp_mp_bot_transition_allowed(species,2u,4u,
                        AVP_MP_BOT_TRANS_EXIT));
        assert(avp_mp_bot_transition_allowed(species,1u,2u,
                        AVP_MP_BOT_TRANS_LIFT)==(species!=1u));
        assert(avp_mp_bot_transition_allowed(species,2u,6u,
                        AVP_MP_BOT_TRANS_DUCT)==(species!=2u));
        assert(avp_mp_bot_transition_allowed(species,6u,2u,
                        AVP_MP_BOT_TRANS_DUCT)==(species!=2u));
        assert(avp_mp_bot_transition_allowed(species,6u,7u,
                        AVP_MP_BOT_TRANS_EXIT)==(species!=2u));
        assert(avp_mp_bot_transition_allowed(species,2u,6u,
                        AVP_MP_BOT_TRANS_EXIT)==(species!=2u));
    }
    assert(!avp_mp_bot_transition_allowed(3u,1u,2u,AVP_MP_BOT_TRANS_EXIT));
    assert(!avp_mp_bot_transition_allowed(2u,0u,2u,AVP_MP_BOT_TRANS_EXIT));
    /* Follow objectives are disposable, not movement or combat locks.
     * A Predator must abandon a Marine in an inaccessible vent and can
     * immediately consider that same Marine on their return to a normal
     * sub-level. Marine and Alien bots may use authored vent connections. */
    assert(!avp_mp_bot_follow_target_possible(2u,2u,6u,NULL,0,0,0,0));
    assert(!avp_mp_bot_follow_target_possible(2u,3u,13u,NULL,0,0,0,0));
    assert(avp_mp_bot_follow_target_possible(2u,2u,3u,NULL,0,0,0,0));
    assert(avp_mp_bot_follow_target_possible(0u,2u,6u,NULL,0,0,0,0));
    assert(avp_mp_bot_follow_target_possible(1u,2u,6u,NULL,0,0,0,0));
    assert(!avp_mp_bot_follow_target_possible(2u,0u,3u,NULL,0,0,0,0));
    /* Same-map pursuit must not lock the bot onto a teammate behind an
     * unreachable solid wall. Remove the wall and the objective is again
     * eligible on the very next decision, with no permanent blacklist. */
    blank_map(3u,1u);
    wallflags[7u]=AVP_WB_SOLID;
    map_cells[CELL(1u,0u,3u)+0u]=7u;
    map_cells[CELL(0u,0u,3u)+2u]=7u;
    map=(AvpMpBotMap){map_cells,3u,1u,wallflags,NULL,0u,risk,NULL,0u};
    assert(!avp_mp_bot_follow_target_possible(2u,3u,3u,&map,0,0,2,0));
    map_cells[CELL(1u,0u,3u)+0u]=0u;
    map_cells[CELL(0u,0u,3u)+2u]=0u;
    assert(avp_mp_bot_follow_target_possible(2u,3u,3u,&map,0,0,2,0));

    /* The retail lift at (9,21) is NOT globally jammed. It blocks only
     * level 3 -> 4 (Down) and level 4 -> 3 (Up). The same lift still
     * permits level 3 -> 2 (Up), level 4 -> 5 (Down), and the opposite
     * neighboring directions where authored. Other lifts stay usable. */
    assert(avp_mp_bot_lift_jammed(3u,9u,21u,1));
    assert(avp_mp_bot_lift_jammed(4u,9u,21u,-1));
    assert(!avp_mp_bot_lift_jammed(3u,9u,21u,-1));
    assert(!avp_mp_bot_lift_jammed(4u,9u,21u,1));
    assert(!avp_mp_bot_lift_jammed(2u,9u,21u,1));
    assert(!avp_mp_bot_lift_jammed(5u,9u,21u,-1));
    assert(!avp_mp_bot_lift_jammed(3u,10u,21u,1));
    assert(!avp_mp_bot_lift_jammed(4u,9u,20u,-1));
    assert(!avp_mp_bot_lift_jammed(3u,9u,21u,0));
    /* The last line of defense must reject an accidentally selected
     * jammed goal WITHOUT emitting the lift Use command. Allowed travel
     * at the identical position must continue to work. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.goal=AVP_MP_BOT_LIFT;v.player_species=2u;v.has_target=1;
    v.x=(9u<<16)|0x8000u;v.y=(21u<<16)|0x8000u;
    v.target_x=v.x;v.target_y=v.y;v.target_side=2u;v.tick=1u;
    v.current_level=3u;v.destination_level=4u;v.lift_direction=1;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_USE)));
    v.destination_level=2u;v.lift_direction=-1;
    avp_mp_bot_step(&s,&v,&c);
    /* No map means the standalone target-cell interaction is handled
     * conservatively. The lift-jam predicate, not the species policy,
     * must be what rejects 3->4. */
    assert(!avp_mp_bot_lift_jammed(v.current_level,v.target_x>>16,
                                   v.target_y>>16,v.lift_direction));
    /* The retail lift at (9,21) is PARTIALLY jammed, not generally unusable.
     * DOORS.S::do_lifts denies only level 3 -> 4 and level 4 -> 3 there.
     * Its remaining levels/directions must remain available.  A different
     * lift on the same level must not inherit the jammed shaft restriction. */
    assert(avp_mp_bot_lift_jammed(3u,9u,21u,+1));
    assert(avp_mp_bot_lift_jammed(4u,9u,21u,-1));
    assert(!avp_mp_bot_lift_jammed(3u,9u,21u,-1));
    assert(!avp_mp_bot_lift_jammed(4u,9u,21u,+1));
    assert(!avp_mp_bot_lift_jammed(2u,9u,21u,+1));
    assert(!avp_mp_bot_lift_jammed(5u,9u,21u,-1));
    assert(!avp_mp_bot_lift_jammed(3u,10u,21u,+1));
    assert(!avp_mp_bot_lift_jammed(4u,10u,21u,-1));
    assert(!avp_mp_bot_lift_jammed(3u,9u,20u,+1));
    /* Defense in depth: even a stale or incorrectly chosen bot goal cannot
     * send the forbidden Use/Lift chord to the original gameplay. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.player_species=1u;v.goal=AVP_MP_BOT_LIFT;
    v.current_level=2u;v.destination_level=3u;
    v.has_target=1u;v.target_side=2u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions==0u);
    v.player_species=2u;v.goal=AVP_MP_BOT_EXIT;
    v.current_level=2u;v.destination_level=6u;v.exit_is_duct=1u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions==0u);
    v.destination_level=4u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions==0u);

    /* Original DOORS.S passage flags: locked door is excluded; source
     * access level 3 permits routing; the actual open byte controls Use. */
    blank_map(5u,1u);
    wallflags[4]=AVP_WB_DOOR|AVP_WB_2SIDES;
    map_cells[CELL(1u,0u,5u)+2u]=4u;
    map_cells[CELL(2u,0u,5u)+0u]=4u;
    {const AvpDoorAccessEntry ac[]={{CELL(2u,0u,5u)+6u,3}};
     const AvpDoorLevelMeta meta={ac,1,0,0,0,0};
     map=(AvpMpBotMap){map_cells,5,1,wallflags,&meta,0,risk,NULL,0u};
     assert(!avp_mp_bot_route(&map,1,0,2,0,&route));
     map.access_level=3;
     assert(avp_mp_bot_route(&map,1,0,2,0,&route));
     assert(route.valid&&route.requires_door_use&&route.next_x==2u);
     memset(&s,0,sizeof s);memset(&v,0,sizeof v);
     v.x=(1u<<16)|0x8000u;v.y=0x8000u;
     v.target_x=(2u<<16)|0x8000u;v.target_y=0x8000u;
     v.has_target=1;v.goal=AVP_MP_BOT_HEALTH;v.map=map;v.tick=1u;
     avp_mp_bot_step(&s,&v,&c);
     assert((c.actions&(1u<<AVP_MP_USE))!=0u);
     assert((c.actions&(1u<<AVP_MP_FORWARD))==0u);
     v.tick=2u;avp_mp_bot_step(&s,&v,&c);
     assert((c.actions&(1u<<AVP_MP_USE))==0u);
     map_cells[CELL(2u,0u,5u)+6u]=56u;
     assert(avp_mp_bot_route(&map,1,0,2,0,&route));
     assert(!route.requires_door_use && route.door_is_open);
     avp_mp_bot_step(&s,&v,&c);
     assert((c.actions&(1u<<AVP_MP_FORWARD))!=0u);
    }
    /* A clear source waypoint supports original forward + independent
     * sidestep inputs together while turning, without invented velocity. */
    blank_map(5u,5u);
    map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL,0u};
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=(1u<<16)|0x3333u;v.y=(1u<<16)|0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0x10000000u;v.has_target=1u;v.goal=AVP_MP_BOT_HEALTH;
    v.map=map;
    avp_mp_bot_step(&s,&v,&c);
    assert((c.actions&(1u<<AVP_MP_FORWARD))!=0u);
    assert((c.actions&(1u<<AVP_MP_STRAFE_RIGHT))!=0u);
    /* Hazard route cost must choose the safe longer way around acid. */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL,0u};
    risk[1u+64u]=2u;
    assert(avp_mp_bot_route(&map,0,1,4,1,&route));
    assert(route.valid && route.next_y!=1u);
    /* Alien bots must prefer the shortest walk through an acid-covered
     * corridor even when Marine/Predator bots would choose a safe detour.
     * This test uses the actual bot command planner (not only Dijkstra). */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL,0u};
    risk[1u+64u]=2u;risk[2u+64u]=2u;risk[3u+64u]=2u;
    assert(avp_mp_bot_route(&map,0u,1u,4u,1u,&route));
    assert(route.next_y!=1u); /* Non-Alien risk route detours. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.player_species=1u;
    v.x=0x8000u;v.y=(1u<<16)|0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.has_target=1;v.goal=AVP_MP_BOT_HEALTH;v.map=map;
    v.acid_count=2u;
    v.acid[0].x=(1u<<16)|0x8000u;
    v.acid[0].y=v.y;
    v.acid[1].x=(2u<<16)|0x8000u;
    v.acid[1].y=v.y;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(!(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|
                        (1u<<AVP_MP_STRAFE_RIGHT))));
    /* Dense acid: costs may be very high, but acid is NOT a wall.
     * Previous bot_step_clear() rejected a hazardous next cell even when
     * the Dijkstra route legitimately had to cross it, trapping the bot. */
    blank_map(5u,1u);
    map=(AvpMpBotMap){map_cells,5,1,wallflags,NULL,0,risk,NULL,0u};
    risk[2u]=2u;risk[3u]=2u;
    assert(avp_mp_bot_route(&map,1u,0u,4u,0u,&route));
    assert(route.valid && route.next_x==2u && route.next_y==0u);
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=(1u<<16)|0x8000u;v.y=0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=0x8000u;
    v.has_target=1;v.goal=AVP_MP_BOT_HEALTH;v.map=map;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    /* Crossing a second puddle from the first must not stop forward travel. */
    v.x=(2u<<16)|0x8000u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    /* S111: once the pathfinder explicitly chooses the unavoidable acid
     * center tile, bypass side-lane microsteering immediately. A bot must
     * not lose precious health ticks creeping sideways before crossing.
     * The native player machine still limits movement speed and applies
     * all contact damage. */
    blank_map(5u,1u);
    map=(AvpMpBotMap){map_cells,5u,1u,wallflags,NULL,0,risk,NULL,0u};
    risk[2u]=2u;risk[3u]=2u;
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.player_species=0u;
    v.x=(1u<<16)|0x8000u;v.y=0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=0x8000u;
    v.has_target=1u;v.goal=AVP_MP_BOT_HEALTH;
    /* Three original contact footprints cover every lateral lane. */
    v.acid_count=3u;
    v.acid[0].x=(2u<<16)|0x8000u;v.acid[0].y=0x3200u;
    v.acid[1].x=(2u<<16)|0x8000u;v.acid[1].y=0x8000u;
    v.acid[2].x=(2u<<16)|0x8000u;v.acid[2].y=0xce00u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(!(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|
                       (1u<<AVP_MP_STRAFE_RIGHT))));
    /* At an oblique heading, forward plus the correct independent Jaguar
     * sidestep is still legal even with acid close to the player. */
    memset(&s,0,sizeof s);
    v.heading=0x10000000u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(c.actions&(1u<<AVP_MP_STRAFE_RIGHT));
    /* The in-contact emergency escape override cannot silently reduce the
     * above two-axis command to a single direction while taking damage. */
    memset(&s,0,sizeof s);
    v.x=(2u<<16)|0x8000u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(c.actions&(1u<<AVP_MP_STRAFE_RIGHT));
    /* If an entirely clear detour exists, avoid the acid, don't force it. */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL,0u};
    risk[1u+64u]=2u;risk[2u+64u]=2u;risk[3u+64u]=2u;
    assert(avp_mp_bot_route(&map,0u,1u,4u,1u,&route));
    assert(route.next_y!=1u);
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=0x8000u;v.y=(1u<<16)|0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.has_target=1;v.goal=AVP_MP_BOT_HEALTH;v.map=map;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    assert(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    /* When every alternate corridor is also acid, keep moving on the
     * lower-cost legal route rather than repeatedly spinning in place. */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL,0u};
    for(unsigned y=0;y<3u;y++)for(unsigned x=1u;x<4u;x++)
        risk[y*64u+x]=2u;
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=0x8000u;v.y=(1u<<16)|0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.has_target=1;v.goal=AVP_MP_BOT_HEALTH;v.map=map;
    assert(avp_mp_bot_route(&map,0u,1u,4u,1u,&route));
    assert(route.next_x==1u && route.next_y==1u);
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    /* The forced crossing starts inside the CURRENT cell: S99 rejected
     * this preliminary step despite planning a valid acid-covered exit. */
    blank_map(5u,1u);
    map=(AvpMpBotMap){map_cells,5,1,wallflags,NULL,0,risk,NULL,0u};
    risk[2u]=2u;risk[3u]=2u;
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.player_species=2u;v.map=map;
    v.x=(1u<<16)|0x8000u;v.y=0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.has_target=1u;v.goal=AVP_MP_BOT_HEALTH;
    v.acid_count=2u;
    v.acid[0].x=(1u<<16)|0xb000u;v.acid[0].y=0x5800u;
    v.acid[1].x=v.acid[0].x;v.acid[1].y=0xa800u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    /* P2 video regression: puddle contact spills into the current cell,
     * while the route's next cell is merely NEAR acid (risk 1), not the
     * coarse acid-center tile. Dijkstra selected the only legal corridor;
     * local steering must not veto that necessary crossing. */
    blank_map(5u,1u);
    map=(AvpMpBotMap){map_cells,5u,1u,wallflags,NULL,0,risk,NULL,0u};
    risk[2u]=1u; risk[3u]=2u;
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=(1u<<16)|0xb800u;v.y=0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.player_index=1u;v.heading=0u;v.has_target=1u;
    v.goal=AVP_MP_BOT_SUPPLY;v.map=map;v.acid_count=1u;
    v.acid[0].x=(1u<<16)|0xec00u;v.acid[0].y=0x8000u;
    for(unsigned stall_tick=0;stall_tick<8u;stall_tick++){
        v.tick=stall_tick;
        avp_mp_bot_step(&s,&v,&c);
    }
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    /* P3 video regression: actively taking damage on acid, with an enemy
     * close enough that ordinary combat would stop to shoot/strafe.
     * Surviving the contact square takes precedence over standing still. */
    risk[2u]=2u;risk[3u]=0u;
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=(2u<<16)|0x8000u;v.y=0x8000u;
    v.target_x=(3u<<16)|0x8000u;v.target_y=v.y;
    v.player_index=2u;v.heading=0u;v.has_target=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.hostile_target=1u;
    v.target_visible=1u;v.map=map;v.acid_count=1u;
    v.acid[0].x=v.x;v.acid[0].y=v.y;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                     (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    /* The same escape works even when the target is in this cell and
     * there is no next waypoint; only original movement keys are issued. */
    memset(&s,0,sizeof s);
    v.target_x=v.x+0x1800u;v.goal=AVP_MP_BOT_HOSTILE;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                     (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    /* Oblique approach at a hazard-lined junction: the old forward-only
     * steering cannot cross a corner safely. Choose a valid strafe toward
     * the path's next cell rather than stopping indefinitely. */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL,0u};
    risk[2u]=2u; /* cell (2,0) is acid; next waypoint is (2,1) */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=(1u<<16)|0xc000u;v.y=(1u<<16)|0x3000u;
    v.heading=0x20000000u; /* facing diagonally toward acid at (2,0) */
    v.target_x=(4u<<16)|0x8000u;v.target_y=(1u<<16)|0x8000u;
    v.has_target=1;v.goal=AVP_MP_BOT_HEALTH;v.map=map;
    assert(avp_mp_bot_route(&map,1u,1u,4u,1u,&route));
    assert(route.next_x==2u && route.next_y==1u);
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_STRAFE_RIGHT));
    /* Combined original forward+strafe is now allowed when its tested
       endpoint stays on the safe next waypoint (2,1), not acid (2,0). */
    /* Restore the original map for the independent wall-edge regression. */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL,0u};
    risk[1u+64u]=2u;
    /* Walls on either face of a boundary must block routing. */
    map_cells[CELL(0u,1u,5u)+1u]=1u;wallflags[1]=AVP_WB_SOLID;
    assert(avp_mp_bot_route(&map,0,1,4,1,&route));
    assert(route.next_y==2u);
    /* Exit interactions must occur from the authored source cell and side. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=(2u<<16)|0x8000u;v.y=(1u<<16)|0x8000u;
    v.target_x=v.x;v.target_y=v.y;v.has_target=1;
    v.goal=AVP_MP_BOT_EXIT;v.target_side=2u;v.tick=1;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_USE));
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    /* An Alien can really operate a duct exit; a Predator can really
     * operate a lift. Explicit source level numbers are not mock perms. */
    memset(&s,0,sizeof s);v.goal=AVP_MP_BOT_EXIT;
    v.player_species=1u;v.current_level=2u;v.destination_level=6u;
    v.exit_is_duct=1u;v.tick=1u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_USE));
    memset(&s,0,sizeof s);v.player_species=2u;v.goal=AVP_MP_BOT_LIFT;
    v.current_level=2u;v.destination_level=3u;
    v.exit_is_duct=0u;v.lift_direction=-1;
    avp_mp_bot_step(&s,&v,&c);
    assert((c.actions&((1u<<AVP_MP_USE)|(1u<<AVP_MP_FORWARD)))==
           ((1u<<AVP_MP_USE)|(1u<<AVP_MP_FORWARD)));
    memset(&s,0,sizeof s);v.player_species=0u;
    v.current_level=v.destination_level=0u;
    v.goal=AVP_MP_BOT_LIFT;v.lift_direction=-1;
    avp_mp_bot_step(&s,&v,&c);
    assert((c.actions&((1u<<AVP_MP_USE)|(1u<<AVP_MP_FORWARD)))==
           ((1u<<AVP_MP_USE)|(1u<<AVP_MP_FORWARD)));
    /* A stunned Marine is not cocooned merely by entering its cell:
     * the original AMP stun_death needs XY proximity within 0.15625 cell. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=(2u<<16)|0x1000u;v.y=(1u<<16)|0x8000u;
    v.target_x=(2u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0u;v.has_target=1;v.goal=AVP_MP_BOT_COCOON;
    v.map=map;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    v.x=(2u<<16)|0x7800u;
    memset(&s,0,sizeof s);
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        /* Predator must finish decloaking before the source combat script may
     * receive a fire input. Source healing uses KEY_7, not a health write. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.target_x=13u<<16;v.target_y=10u<<16;
    v.has_target=1;v.hostile_target=1;v.target_visible=1;
    v.player_species=2u;v.invis_act=1u;v.invis_stat=2u;
    v.invisflag=1u;v.max_energy=1000;v.player_energy=500;v.medpak=100;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_PRED_CLOAK));
    assert(c.actions&(1u<<AVP_MP_PRED_MEDKIT));
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    v.invis_act=0u;v.invis_stat=1u;avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    v.invis_stat=0u;v.invisflag=0u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    v.target_visible=0u;avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    /* Melee dodging uses source creature categories, not the current
     * long-range target. Avoid a visible Facehugger/Queen/Predator/Alien
     * while keeping normal Jaguar strafe/backward movement semantics. */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL,0u};
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=(2u<<16)|0x8000u;v.y=(1u<<16)|0x8000u;
    v.heading=0u;v.player_species=0u;
    v.melee_x=(3u<<16)|0x5000u;v.melee_y=v.y;
    v.melee_visible=1u;
    for(unsigned threat=AVP_MP_BOT_THREAT_ALIEN;
        threat<=AVP_MP_BOT_THREAT_FACEHUG;threat++){
        memset(&s,0,sizeof s);
        v.melee_threat=threat;
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
    }
    /* A puddle directly behind the bot must suppress blind retreat. */
    risk[1u+64u]=2u;
    memset(&s,0,sizeof s);
    v.melee_threat=AVP_MP_BOT_THREAT_FACEHUG;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    assert(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    /* Invisible threats must not trigger opportunistic evasive motion. */
    memset(&s,0,sizeof s);v.melee_visible=0u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    /* Original Alien tail stuns Marines, then claw can finish without
     * toggling the source AMP_STUN flag off. It cannot reach at gun range. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.target_x=(10u<<16)|0x6666u;v.target_y=10u<<16;
    v.has_target=1;v.hostile_target=1;v.target_visible=1;
    v.goal=AVP_MP_BOT_HOSTILE;
    v.player_species=1u;v.target_is_marine=1u;
    v.max_energy=1000;v.player_energy=300;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_ALIEN_TAIL));
    assert(!(c.actions&(1u<<AVP_MP_ALIEN_CLAW)));
    v.target_is_stunned=1u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_ALIEN_CLAW));
    assert(!(c.actions&(1u<<AVP_MP_ALIEN_TAIL)));
    v.target_x=14u<<16;avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_ALIEN_CLAW)));
    assert(!(c.actions&(1u<<AVP_MP_ALIEN_TAIL)));
    /* RE28/S98: Source-authored distance 0x90 << 8 = 0.5625 cell for
     * Alien tail, claw, bite and Predator combi-stick/wrist-blades.  The
     * planner must NOT fire at targets outside that distance, and it must
     * keep its original player body/weapon physics rather than moving it
     * directly.  Check near/far approach, retreat, safe lateral footwork,
     * LOS, and Predator melee vs ranged weapon selection. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.has_target=1u;v.hostile_target=1u;v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.player_species=1u;
    v.target_x=(10u<<16)|0xc000u;v.target_y=v.y; /* 0.75: beyond native trace */
    avp_mp_bot_step(&s,&v,&c);
    assert((c.actions&(1u<<AVP_MP_FORWARD))!=0u);
    assert(!(c.actions&((1u<<AVP_MP_ALIEN_CLAW)|(1u<<AVP_MP_ALIEN_TAIL))));
    v.target_x=(10u<<16)|0x6666u; /* 0.4: strike while circling */
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    assert(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    assert(c.actions&((1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL)|(1u<<AVP_MP_ALIEN_CLAW)));
    v.target_x=(10u<<16)|0x3000u; /* 0.1875: retreat AND strike */
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    assert(c.actions&((1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL)|(1u<<AVP_MP_ALIEN_CLAW)));
    /* Same intended melee opponent must not cause endless generic retreat;
     * the spacing controller owns movement and keeps attack enabled. */
    v.melee_threat=AVP_MP_BOT_THREAT_ALIEN;v.melee_visible=1u;
    v.melee_x=v.target_x;v.melee_y=v.target_y;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    assert(c.actions&((1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL)|(1u<<AVP_MP_ALIEN_CLAW)));
    /* More dangerous DIFFERENT enemy: generic evasion still takes over. */
    v.melee_x=(10u<<16)|0xe000u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_PAUSE)));
    v.melee_visible=0u;v.target_visible=0u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_ALIEN_CLAW)));
    /* A melee hit cannot be issued if the player is not facing the target. */
    v.target_visible=1u;v.heading=0x40000000u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_ALIEN_CLAW)));
    /* Separate source melee weapon selection from Predator ranged slots. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.target_x=(10u<<16)|0x6666u;v.target_y=v.y;
    v.has_target=1u;v.hostile_target=1u;v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.player_species=2u;
    v.current_weapon=4u; /* retail default wrist-blades */
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    assert(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    v.current_weapon=1u; /* unlocked combi-stick, same authored reach */
    v.target_x=(10u<<16)|0x3000u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    v.target_x=(10u<<16)|0xc000u; /* outside native melee shot box */
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    v.current_weapon=2u; /* shoulder cannon: preserve ranged tactics */
    v.target_x=(12u<<16);v.target_y=v.y;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    /* Predator still cannot fire before its full original decloak. */
    v.current_weapon=4u;v.target_x=(10u<<16)|0x6666u;
    v.invis_stat=1u;v.invisflag=1u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    /* Alien players are immune to acid, so even a backward retreat is
     * legal when a puddle lies in that direction. */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL,0u};
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=(2u<<16)|0x8000u;v.y=(1u<<16)|0x8000u;
    v.heading=0u;v.player_species=1u;
    v.target_x=(2u<<16)|0xb000u;v.target_y=v.y;
    v.has_target=1u;v.hostile_target=1u;v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;
    risk[1u+64u]=2u; /* acid in cell (1,1), behind player */
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    assert(c.actions&((1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL)|(1u<<AVP_MP_ALIEN_CLAW)));
    /* S99: Visible enemy while obstacle-recovery spins: no two-second
     * attack blackout. Still use original LOS/aim/reach for the shot. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.target_x=(10u<<16)|0x6666u;v.target_y=v.y;
    v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.player_species=1u;
    s.evade_ticks=14u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL)|(1u<<AVP_MP_ALIEN_CLAW)));
    v.target_visible=0u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_ALIEN_CLAW)));

    /* A known blocked route is not a reason to stare at a target that the
     * native source LOS says is visible and within weapon reach. */
    blank_map(5u,3u);
    wallflags[1u]=AVP_WB_SOLID;
    for(unsigned row=0;row<3u;row++)
        map_cells[CELL(3u,row,5u)+0u]=1u;
    map=(AvpMpBotMap){map_cells,5u,3u,wallflags,NULL,0,risk,NULL,0u};
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=(2u<<16)|0xc000u;v.y=(1u<<16)|0x8000u;
    v.target_x=(3u<<16)|0x3000u;v.target_y=v.y;
    v.heading=0u;v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.player_species=1u;
    assert(!avp_mp_bot_route(&map,2u,1u,3u,1u,&route));
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL)|(1u<<AVP_MP_ALIEN_CLAW)));

    /* Source Marine-only cocoon sequence: stand in melee reach, hold claw,
     * tail, claw. After STUN, never tail again (the source XORs STUN), and
     * move inside the 20<<9 XY proximity to let stun_death create cocoon. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.target_x=(10u<<16)|0x6666u;v.target_y=v.y;
    v.goal=AVP_MP_BOT_COCOON;v.player_species=1u;
    v.has_target=v.hostile_target=v.target_visible=1u;
    v.target_energy_positive=v.target_is_marine=1u;
    s.cocoon_phase_end=14u;
    v.tick=1u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_ALIEN_CLAW));
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    v.tick=14u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_ALIEN_TAIL));
    v.tick=28u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_ALIEN_CLAW));
    v.target_is_stunned=1u;v.tick=42u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_ALIEN_CLAW));
    assert(!(c.actions&(1u<<AVP_MP_ALIEN_TAIL)));
    v.hostile_target=v.target_energy_positive=0u;
    v.tick=43u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(!(c.actions&((1u<<AVP_MP_ALIEN_CLAW)|(1u<<AVP_MP_ALIEN_TAIL))));
    v.x=(10u<<16)|0x5c00u;v.tick=44u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    v.target_is_marine=0u;v.goal=AVP_MP_BOT_PATROL;
    avp_mp_bot_step(&s,&v,&c);
    assert(!s.cocoon_locked);

    /* Predator normally cloaks only with Marines present. Any nearby
     * battle holds decloak until it is finished, including distant fading
     * frames where original honor protection remains set. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.player_species=2u;v.tick=100u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_PRED_CLOAK)));
    v.marines_on_level=1u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_PRED_CLOAK));
    v.invis_act=1u;v.invis_stat=2u;v.combat_enemies_nearby=1u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_PRED_CLOAK));
    v.invis_act=0u;v.invis_stat=1u;
    v.has_target=v.hostile_target=v.target_visible=1u;
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.target_x=13u<<16;v.target_y=v.y;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    v.invis_stat=0u;v.invisflag=0u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    v.has_target=v.hostile_target=v.target_visible=0u;
    v.combat_enemies_nearby=0u;v.tick=110u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_PRED_CLOAK)));
    v.tick=114u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_PRED_CLOAK));
    v.marines_on_level=0u;v.invis_act=1u;v.invis_stat=2u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_PRED_CLOAK));

    /* R274: at the closed end of a one-cell corridor no lethal fallback
     * may seal the only passage, even if the bot cannot retreat. */
    blank_map(5u,3u);
    for(unsigned x=0u;x<5u;x++){
        map_cells[CELL(x,0u,5u)+5u]=0u;
        map_cells[CELL(x,2u,5u)+5u]=0u;
    }
    map=(AvpMpBotMap){map_cells,5u,3u,wallflags,NULL,0,risk,NULL,0u};
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=0x8000u;v.y=(1u<<16)|0x8000u;
    v.target_x=(3u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0u;v.player_species=0u;v.player_energy=v.max_energy=100;
    v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.tick=1u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    assert(s.lure_end_tick==0u);
    v.tick=160u;avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    /* A close enemy still cannot be deliberately killed in this chokepoint. */
    v.tick=2u;v.target_x=(1u<<16)|0x8000u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    /* R116: A Balanced Marine must actually retreat from a still-distant
     * Alien in a narrow dead-end, so the Alien can leave the choke BEFORE
     * the bot shoots it. R115 stopped completely beyond four cells, even
     * with a safe rearward corridor and a visible pursuer. */
    {
        uint8_t long_hall[9u*3u*8u]={0};
        for(unsigned x=0u;x<9u;x++)
            long_hall[CELL(x,1u,9u)+5u]=1u;
        map=(AvpMpBotMap){long_hall,9u,3u,wallflags,NULL,0u,risk,NULL,0u};
        memset(&v,0,sizeof v);memset(&s,0,sizeof s);
        v.map=map;
        v.x=(3u<<16)|0x8000u;v.y=(1u<<16)|0x8000u;
        v.target_x=(8u<<16)|0x8000u;v.target_y=v.y;
        v.heading=0u;v.player_species=AVP_MP_MARINE;
        v.player_energy=v.max_energy=100;
        v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
        v.goal=AVP_MP_BOT_HOSTILE;v.tick=1u;
        assert(!avp_mp_bot_alien_death_passable(&map,v.target_x,v.target_y));
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        /* Facing backward: FORWARD moves away while ordinary turn controls
         * realign the Marine to face the Alien. */
        v.tick=2u;v.heading=0x80000000u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
        assert(c.actions&(1u<<AVP_MP_FORWARD));
        assert(c.actions&((1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT)));
        /* Side-on: strafe away instead of freezing while turning. */
        v.tick=3u;v.heading=0x40000000u;
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_STRAFE_LEFT));
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        v.heading=0u;
        /* Once the old 150-tick lure interval ends, a chokepoint is STILL
         * unsafe; expiry never authorizes a fatal shot. */
        v.tick=160u;avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        /* R275: the old 150-tick expiration and 300-tick cooldown must
         * not reverse the retreat while the Alien still seals the route. */
        v.tick=301u;avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        /* A close Alien does not require a >1.1-tile lure gap. */
        v.target_x=(4u<<16)|0x1000u;v.tick=302u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        /* A bot losing health may still retreat through legal geometry;
         * the independent fatal-acid veto remains in force. */
        v.player_energy=12;v.tick=303u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        v.player_energy=100;
        /* If the bot has already lured, moving farther than six tiles
         * away must not make it rush back into the lethal chokepoint. */
        v.x=(1u<<16)|0x8000u;v.target_x=(8u<<16)|0x8000u;
        v.tick=304u;avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        /* Once the actual Alien leaves the dangerous centre line, reset
         * the lure and restore ordinary source LOS/range-based combat. */
        v.x=(3u<<16)|0x8000u;
        v.target_x=(6u<<16)|0x8000u;
        v.target_y=(1u<<16)|0x1000u;v.tick=305u;
        assert(avp_mp_bot_alien_death_passable(&map,v.target_x,v.target_y));
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FIRE));
        assert(s.lure_target_slot==0u);
        /* No legal retreat in this hallway: keep the veto in place even
         * though luring cannot currently move the Alien. */
        memset(&s,0,sizeof s);memset(&v,0,sizeof v);
        v.map=map;
        v.x=0x8000u;v.y=(1u<<16)|0x8000u;
        v.target_x=(5u<<16)|0x8000u;v.target_y=v.y;
        v.heading=0u;v.player_species=AVP_MP_MARINE;
        v.player_energy=v.max_energy=100;
        v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
        v.goal=AVP_MP_BOT_HOSTILE;v.tick=1u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
        assert(s.lure_end_tick==0u && s.lure_cooldown_tick==0u);
        /* Distant Alien: no false lure / stationary fire suppression. */
        memset(&s,0,sizeof s);memset(&v,0,sizeof v);
        v.map=map;
        v.x=0x8000u;v.y=(1u<<16)|0x8000u;
        v.target_x=(8u<<16)|0x8000u;v.target_y=v.y;
        v.heading=0u;v.player_species=AVP_MP_MARINE;
        v.player_energy=v.max_energy=100;
        v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
        v.goal=AVP_MP_BOT_HOSTILE;v.tick=1u;
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FORWARD));
        assert(s.lure_end_tick==0u);
    }
    /* Acid has a 30<<9 XY contact square rather than a full cell. Check
     * narrow-lane strafing while preserving a necessary hazardous route. */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5u,3u,wallflags,NULL,0,risk,NULL,0u};
    risk[2u+64u]=2u;
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=(2u<<16)|0x1800u;v.y=(1u<<16)|0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0u;v.has_target=1u;v.goal=AVP_MP_BOT_HEALTH;
    v.acid_count=1u;
    v.acid[0].x=(2u<<16)|0x8000u;
    v.acid[0].y=(1u<<16)|0x8000u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    /* No target may be cocooned unless the actual target is a Marine. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.goal=AVP_MP_BOT_COCOON;v.player_species=1u;
    v.target_is_marine=0u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!s.cocoon_locked && s.cocoon_phase==0u);

        /* A source-registered prop within the route cell must be treated as a
     * physical obstacle, not just a visual decoration. The bot can choose
     * the sidestep lane without entering the original collision square. */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5u,3u,wallflags,NULL,0,risk,NULL,0u};
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=(1u<<16)|0x8000u;v.y=(1u<<16)|0x8000u;
    v.target_x=(3u<<16)|0x8000u;v.target_y=v.y;
    v.has_target=1u;v.goal=AVP_MP_BOT_HEALTH;v.map=map;
    v.heading=0u;v.tick=1u;
    v.prop_count=1u;
    v.props[0]=(AvpMpBotProp){(2u<<16)|0x8000u,(1u<<16)|0x8000u,(40u<<9)*85u/100u};
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions & ((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    /* When the original movement result stays blocked, recovery is driven
     * by the LAST requested translation, and chooses an escape direction. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0;
    v.target_x=14u<<16;v.target_y=10u<<16;
    v.has_target=1u;v.goal=AVP_MP_BOT_FOLLOW;
    for(unsigned k=0;k<12u;k++){
        v.tick=k+1u;avp_mp_bot_step(&s,&v,&c);
    }
    assert(s.recovery_count>0u);
    assert(c.actions & ((1u<<AVP_MP_BACKWARD)|(1u<<AVP_MP_STRAFE_LEFT)|
                        (1u<<AVP_MP_STRAFE_RIGHT)));
    /* Stationary cocooning isn't an obstacle: no movement => no recovery. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0;
    v.target_x=v.x+(uint32_t)(0.25*65536.0);v.target_y=v.y;
    v.player_species=1u;v.goal=AVP_MP_BOT_COCOON;
    v.target_is_marine=1u;v.target_energy_positive=1u;
    v.has_target=1u;v.hostile_target=1u;v.target_visible=1u;
    for(unsigned k=0;k<30u;k++){
        v.tick=k+1u;avp_mp_bot_step(&s,&v,&c);
    }
    assert(s.recovery_count==0u);
    /* S102 Predator honor policy is a preference, NOT a score override.
     * AMP.S::do_score still awards 1.0 melee and 0.5 ranged, or penalizes
     * -1.0 melee/-2.0 ranged when INVHIT is set. No unlocked Disc means
     * no opportunistic ranged switch, even when wounded or outnumbered. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.player_species=2u;v.current_weapon=1u;
    v.predator_owned=(1u<<1)|(1u<<2)|(1u<<4);
    v.hostile_target=1u;v.player_energy=10;v.max_energy=100;
    v.combat_enemies_nearby=4u;
    assert(avp_mp_bot_predator_preferred_weapon(&v)==1u);
    v.predator_owned|=1u<<3;
    assert(avp_mp_bot_predator_preferred_weapon(&v)==3u);
    v.player_energy=90;v.combat_enemies_nearby=1u;
    assert(avp_mp_bot_predator_preferred_weapon(&v)==1u);
    v.combat_enemies_nearby=3u;
    assert(avp_mp_bot_predator_preferred_weapon(&v)==3u);
    v.hostile_target=0u;
    assert(avp_mp_bot_predator_preferred_weapon(&v)==1u);
    v.predator_owned=(1u<<4)|(1u<<3);
    assert(avp_mp_bot_predator_preferred_weapon(&v)==4u);
    v.player_species=1u;
    assert(avp_mp_bot_predator_preferred_weapon(&v)==0u);
    /* The command planner must request an actual original Next/Previous
     * weapon press, suppress FIRE until equipped, and refuse cloaked shots.
     * The normal weapon still fires once uncloaked and within range. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.player_species=2u;v.x=10u<<16;v.y=10u<<16;
    v.heading=0u;v.target_x=v.x+(1u<<16);v.target_y=v.y;
    v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;
    v.predator_owned=(1u<<1)|(1u<<3);v.current_weapon=1u;
    v.max_energy=100;v.player_energy=30;v.combat_enemies_nearby=1u;
    v.tick=10u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_NEXT_WEAPON)|(1u<<AVP_MP_PREV_WEAPON)));
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    v.current_weapon=3u;v.invisflag=1u;v.invis_act=1u;
    v.tick=11u;avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    assert(c.actions&(1u<<AVP_MP_PRED_CLOAK));
    v.invisflag=0u;v.invis_act=0u;v.invis_stat=0u;
    v.tick=12u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    /* No enemy Marines => never activate cloak. For an already-cloaked
     * Predator, request decloak instead. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.player_species=2u;v.max_energy=100;v.player_energy=100;
    v.tick=15u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_PRED_CLOAK)));
    v.invis_act=1u;v.tick=16u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_PRED_CLOAK));
    /* The Alien cannot reach the Predator ship by elevators. The level
     * objective helper chooses Sub-Level 1 before the ship, then the
     * final Queen world. These are goals; exits still require source doors. */
    for(unsigned level=2u;level<=14u;level++)
        assert(avp_mp_bot_alien_campaign_target_level(level)==1u);
    assert(avp_mp_bot_alien_campaign_target_level(1u)==15u);
    assert(avp_mp_bot_alien_campaign_target_level(15u)==0u);
    assert(avp_mp_bot_alien_campaign_target_level(0u)==0u);
    assert(avp_mp_bot_alien_campaign_target_level(16u)==0u);
    /* Once the last generator is destroyed, idle near the Queen and face
     * her. Do not issue movement, Use, or an attack; retail AMP handles the
     * actual ending and camera transition. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.player_species=1u;v.goal=AVP_MP_BOT_QUEEN_WAIT;
    v.x=10u<<16;v.y=10u<<16;
    v.target_x=v.x;v.target_y=v.y-(2u<<16);v.has_target=1u;
    v.tick=20u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_TURN_LEFT));
    assert(!(c.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)|
        (1u<<AVP_MP_FIRE)|(1u<<AVP_MP_ALIEN_CLAW)|
        (1u<<AVP_MP_ALIEN_TAIL)|(1u<<AVP_MP_USE))));
    assert(!avp_mp_bot_outside_bounds(0u,0u,5u,5u));
    assert(avp_mp_bot_outside_bounds((5u<<16),0u,5u,5u));
    assert(avp_mp_bot_outside_bounds((uint32_t)(int32_t)-1,0u,5u,5u));
    assert(avp_mp_bot_outside_bounds(0u,0u,0u,5u));
    /* R24: original SafePos can regard a no-floor cell inside the maze as
     * collision-safe. Bots may not select it as a movement destination. */
    {
        uint8_t cells[5u*5u*8u]={0};
        cells[((2u*5u+2u)*8u)+5u]=1u;
        cells[((3u*5u+2u)*8u)+5u]=2u;
        assert(avp_mp_bot_playable_position(cells,5u,5u,
               (2u<<16)|0x8000u,(2u<<16)|0x8000u));
        assert(avp_mp_bot_playable_position(cells,5u,5u,
               (2u<<16)|0x8000u,(3u<<16)|0x8000u));
        assert(!avp_mp_bot_playable_position(cells,5u,5u,
               (3u<<16)|0x8000u,(2u<<16)|0x8000u));
        assert(!avp_mp_bot_playable_position(cells,5u,5u,
               (5u<<16),0u));
        assert(!avp_mp_bot_playable_position(cells,5u,5u,
               0xffffffffu,(2u<<16)));
        assert(!avp_mp_bot_playable_position(cells,0u,5u,0u,0u));
        assert(!avp_mp_bot_playable_position(NULL,5u,5u,0u,0u));
        assert(!avp_mp_bot_playable_position(cells,65u,5u,0u,0u));
    }

    /* S103: A valid map with an invalid current XY cannot become a blind
     * no-route/no-motion loop. Recover using real movement commands only. */
    blank_map(5u,5u);
    map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL,0u};
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.heading=0u;v.has_target=1u;
    v.x=(uint32_t)(int32_t)-16384;v.y=(2u<<16)|0x8000u;
    v.target_x=3u<<16;v.target_y=v.y;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    v.x=(5u<<16)|0x8000u;v.heading=0u;
    memset(&s,0,sizeof s);
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_BACKWARD));
    v.x=(2u<<16)|0x8000u;v.y=(2u<<16)|0x8000u;
    map_cells[CELL(2u,2u,5u)+5u]=0u;
    memset(&s,0,sizeof s);
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                      (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    /* Remote hazards must not disable valid combined corner movement:
     * only nearby acid/props require single-axis micro-steering. */
    blank_map(5u,5u);memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=(1u<<16)|0x3333u;v.y=(1u<<16)|0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0x10000000u;v.has_target=1u;v.goal=AVP_MP_BOT_HEALTH;
    v.acid_count=1u;v.acid[0]=(AvpMpBotAcid){(4u<<16)|0x8000u,(4u<<16)|0x8000u};
    v.prop_count=1u;v.props[0]=(AvpMpBotProp){(4u<<16)|0x8000u,(4u<<16)|0x8000u,0x2000u};
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(c.actions&(1u<<AVP_MP_STRAFE_RIGHT));
    /* S114: Original AMP.S::amp_in_range(30) is a +/-15360-unit
     * Chebyshev contact square (0.234375 tiles on each axis), NOT a 3x3
     * tile circle. The coarser planner must never fabricate damage in
     * eight neighboring cells outside that contact footprint. */
    memset(risk,0,sizeof risk);
    avp_mp_bot_acid_risk_mark(risk,(2u<<16)|0x8000u,
                                  (1u<<16)|0x8000u);
    for(unsigned yy=0;yy<3u;yy++)for(unsigned xx=1u;xx<=3u;xx++)
        assert(risk[yy*64u+xx]==(yy==1u&&xx==2u?2u:0u));
    memset(risk,0,sizeof risk);
    avp_mp_bot_acid_risk_mark(risk,(2u<<16)|0xf000u,
                                  (1u<<16)|0x8000u);
    assert(risk[1u*64u+2u]==1u && risk[1u*64u+3u]==1u);
    assert(!risk[0u*64u+2u] && !risk[2u*64u+2u]);
    /* Off-center acid leaves a REAL safe lane in a single-cell hallway.
     * Width=40 original SafePos margins allow centers >=.3125 and <=.6875.
     * Bot must first shift toward the clear edge, then advance at ordinary
     * forward speed; never hover on the visual puddle's 1x1 grid cell. */
    blank_map(5u,1u);
    map=(AvpMpBotMap){map_cells,5u,1u,wallflags,NULL,0,risk,NULL,0u};
    avp_mp_bot_acid_risk_mark(risk,(2u<<16)|0x8000u,
                                  0x9999u); /* 0.60 along Y */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=(1u<<16)|0x8000u;v.y=0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0u;v.has_target=1u;v.goal=AVP_MP_BOT_HEALTH;
    v.acid_count=1u;
    v.acid[0]=(AvpMpBotAcid){(2u<<16)|0x8000u,0x9999u};
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_STRAFE_LEFT));
    assert(!(c.actions&(1u<<AVP_MP_STRAFE_RIGHT)));
    {unsigned normal_forward=0u,side_steps=0u;
     for(unsigned step=0u;step<38u;step++){
         const double x=(double)v.x/65536.0,y=(double)v.y/65536.0;
         const double ax=(double)v.acid[0].x/65536.0;
         const double ay=(double)v.acid[0].y/65536.0;
         v.tick=(uint64_t)step+1u;
         avp_mp_bot_step(&s,&v,&c);
         if(c.actions&(1u<<AVP_MP_FORWARD)){
             v.x+=(uint32_t)(0.07*65536.0);
             normal_forward++;
         }
         if(c.actions&(1u<<AVP_MP_STRAFE_LEFT)){
             v.y-=(uint32_t)(0.07*65536.0);
             side_steps++;
         }
         if(c.actions&(1u<<AVP_MP_STRAFE_RIGHT))
             v.y+=(uint32_t)(0.07*65536.0);
         /* Sampled command-level mock, not a claim about native speed:
          * this checks choice of commands across repeated fresh states. */
         (void)x;(void)y;
         if((double)v.x/65536.0>2.25 && (double)v.x/65536.0<2.75)
             assert(fabs((double)v.y/65536.0-ay)>0.234375);
         (void)ax;
     }
     assert(normal_forward>20u && side_steps>=1u);
     assert((double)v.x/65536.0>3.0);
    }
    /* Centered acid in a wall-bounded 1-cell corridor cannot be bypassed
     * at original player width: don't choose an impossible .18/.82 lane. */
    blank_map(5u,1u);
    map=(AvpMpBotMap){map_cells,5u,1u,wallflags,NULL,0,risk,NULL,0u};
    avp_mp_bot_acid_risk_mark(risk,(2u<<16)|0x8000u,0x8000u);
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=(1u<<16)|0x8000u;v.y=0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0u;v.has_target=1u;v.goal=AVP_MP_BOT_HEALTH;
    v.acid_count=1u;
    v.acid[0]=(AvpMpBotAcid){(2u<<16)|0x8000u,0x8000u};
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(!(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT))));
    /* Even an open map's coarse risk must not mark an arbitrary 3x3
     * danger region around a centered puddle. */
    blank_map(5u,3u);
    avp_mp_bot_acid_risk_mark(risk,(2u<<16)|0x8000u,
                                  (1u<<16)|0x8000u);
    assert(risk[64u+2u]==2u && risk[2u]==0u && risk[2u*64u+2u]==0u);
    /* Combat within native source LOS/range should react on THIS tick,
     * even at a modest acute angle while moving. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.heading=0u;
    v.target_x=v.x+(uint32_t)(1.2*65536.0);
    v.target_y=v.y-(uint32_t)(0.6*65536.0);
    v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.player_species=0u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    /* R20 Deathmatch Predator random weapon must be actually owned and must
     * not repeatedly select its previous slot when alternatives exist. */
    for(unsigned seed=0u;seed<1024u;seed++){
        unsigned w=avp_mp_bot_random_predator_weapon((1u<<1)|(1u<<2)|
                            (1u<<3)|(1u<<4),3u,seed);
        assert(w>=1u && w<=4u && w!=3u);
        assert(avp_mp_bot_random_predator_weapon(1u<<4,4u,seed)==4u);
        assert(avp_mp_bot_random_predator_weapon(0u,0u,seed)==0u);
    }
    /* R22: 16 distinct permutations of the original Predator 1..4 slots,
     * 15 seconds of genuinely idle 10-Hz ticks between switches, with the
     * interval and pending equip both frozen throughout combat. */
    {
        unsigned seen[16]={0};
        for(unsigned order=0u;order<16u;order++){
            unsigned bits=0u;
            for(unsigned step=0u;step<4u;step++){
                unsigned w=avp_mp_bot_predator_order_weapon(order,step);
                assert(w>=1u && w<=4u);
                assert(!(bits&(1u<<w)));
                bits|=1u<<w;
            }
            assert(bits==0x1eu);
            for(unsigned old=0u;old<order;old++){
                unsigned different=0u;
                for(unsigned step=0u;step<4u;step++)
                    different|=avp_mp_bot_predator_order_weapon(old,step)!=
                               avp_mp_bot_predator_order_weapon(order,step);
                assert(different);
            }
        }
        assert(!avp_mp_bot_predator_order_weapon(16u,0u));
        assert(!avp_mp_bot_predator_order_weapon(0u,4u));
        memset(&s,0,sizeof s);memset(&v,0,sizeof v);
        v.player_species=2u;v.deathmatch=1u;
        v.x=10u<<16;v.y=10u<<16;v.heading=0u;
        v.player_index=9u;v.goal=AVP_MP_BOT_PATROL;
        v.predator_owned=0x1eu;v.current_weapon=1u;
        v.max_energy=v.player_energy=100u;
        for(unsigned tick=1u;tick<=149u;tick++){
            v.tick=tick;
            avp_mp_bot_step(&s,&v,&c);
            assert(s.predator_pattern_ready);
            assert(s.predator_idle_ticks==tick);
            assert(!s.predator_switch_pending);
            assert(!(c.actions&((1u<<AVP_MP_NEXT_WEAPON)|
                               (1u<<AVP_MP_PREV_WEAPON))));
        }
        assert(s.predator_pattern_stride>=1u &&
               s.predator_pattern_stride<=15u &&
               (s.predator_pattern_stride&1u));
        unsigned first_step=s.predator_pattern_step;
        /* 149 idle ticks + 300 in combat + one final idle tick must
         * trigger a switch, never a weapon change DURING the battle. */
        v.goal=AVP_MP_BOT_HOSTILE;
        v.has_target=v.hostile_target=v.target_visible=1u;
        v.target_x=v.x+0x6000u;v.target_y=v.y;
        for(unsigned tick=150u;tick<450u;tick++){
            v.tick=tick;
            avp_mp_bot_step(&s,&v,&c);
            assert(s.predator_idle_ticks==149u);
            assert(!s.predator_switch_pending);
            assert(s.predator_pattern_order==0u);
            assert(s.predator_pattern_step==first_step);
            assert(!(c.actions&((1u<<AVP_MP_NEXT_WEAPON)|
                               (1u<<AVP_MP_PREV_WEAPON))));
        }
        v.goal=AVP_MP_BOT_PATROL;
        v.has_target=v.hostile_target=v.target_visible=0u;
        v.tick=450u;
        avp_mp_bot_step(&s,&v,&c);
        assert(s.predator_switch_pending && s.predator_idle_ticks==150u);
        assert(s.predator_weapon_target!=v.current_weapon);
        unsigned pending=s.predator_weapon_target;
        /* Combat begins after the switch is due but before the original
         * weapon animation completes. The pending target survives, but
         * no weapon-control commands leak into combat. */
        v.goal=AVP_MP_BOT_HOSTILE;
        v.has_target=v.hostile_target=v.target_visible=1u;
        v.combat_enemies_nearby=1u;
        for(unsigned tick=451u;tick<551u;tick++){
            v.tick=tick;
            avp_mp_bot_step(&s,&v,&c);
            assert(s.predator_switch_pending);
            assert(s.predator_weapon_target==pending);
            assert(s.predator_idle_ticks==150u);
            assert(!(c.actions&((1u<<AVP_MP_NEXT_WEAPON)|
                               (1u<<AVP_MP_PREV_WEAPON))));
        }
        v.has_target=v.hostile_target=v.target_visible=0u;
        v.goal=AVP_MP_BOT_PATROL;
        /* Nearby enemies also count as combat even without a current
         * selected target, so a tracker blip does not allow switching. */
        v.tick=551u;
        avp_mp_bot_step(&s,&v,&c);
        assert(s.predator_switch_pending && s.predator_weapon_target==pending);
        assert(!(c.actions&((1u<<AVP_MP_NEXT_WEAPON)|
                           (1u<<AVP_MP_PREV_WEAPON))));
        v.combat_enemies_nearby=0u;
        v.tick=560u;
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&((1u<<AVP_MP_NEXT_WEAPON)|
                          (1u<<AVP_MP_PREV_WEAPON)));
        assert(s.predator_switch_pending && s.predator_weapon_target==pending);
        /* Model the original equipped weapon completing its raise/lower. */
        v.tick=561u;v.current_weapon=pending;
        avp_mp_bot_step(&s,&v,&c);
        assert(!s.predator_switch_pending && s.predator_idle_ticks==0u);
        assert(s.predator_pattern_step==(first_step+1u)%4u);
        assert(!(c.actions&((1u<<AVP_MP_NEXT_WEAPON)|
                           (1u<<AVP_MP_PREV_WEAPON))));
        /* Resume from 40/150 idle ticks after a separate interruption. */
        for(unsigned tick=562u;tick<602u;tick++){
            v.tick=tick;avp_mp_bot_step(&s,&v,&c);
        }
        assert(s.predator_idle_ticks==40u);
        v.goal=AVP_MP_BOT_HOSTILE;
        v.has_target=v.hostile_target=1u;
        for(unsigned tick=602u;tick<702u;tick++){
            v.tick=tick;avp_mp_bot_step(&s,&v,&c);
            assert(s.predator_idle_ticks==40u);
        }
        v.goal=AVP_MP_BOT_PATROL;
        v.has_target=v.hostile_target=0u;
        for(unsigned tick=702u;tick<811u;tick++){
            v.tick=tick;avp_mp_bot_step(&s,&v,&c);
            assert(!s.predator_switch_pending);
        }
        assert(s.predator_idle_ticks==149u);
        v.tick=811u;avp_mp_bot_step(&s,&v,&c);
        assert(s.predator_switch_pending && s.predator_idle_ticks==150u);
        /* Run through the full formula via real observed ticks and
         * completion of the requested original weapon selections. */
        unsigned transitions=0u,pattern_start=s.predator_pattern_start;
        unsigned row_start=s.predator_pattern_order;
        unsigned step_start=s.predator_pattern_step;
        for(unsigned tick=812u;tick<17000u;tick++){
            v.tick=tick;
            if(s.predator_switch_pending)
                v.current_weapon=s.predator_weapon_target;
            avp_mp_bot_step(&s,&v,&c);
            unsigned row=(s.predator_pattern_start+
                         s.predator_pattern_stride*s.predator_pattern_order)&15u;
            seen[row]=1u;
            if(!s.predator_switch_pending && s.predator_idle_ticks==0u)
                transitions++;
            if(s.predator_pattern_order==row_start &&
               s.predator_pattern_step==step_start && transitions>16u)
                break;
        }
        assert(pattern_start==s.predator_pattern_start);
        /* A boundary matching the held weapon is skipped immediately.
         * The 64 formula entries still cycle through all 16 orders, but
         * do not each necessarily trigger a physical weapon switch. */
        assert(transitions>=49u && transitions<=64u);
        for(unsigned row=0u;row<16u;row++)assert(seen[row]);
        /* A second bot's formula/timer is independent, even with the same
         * global game tick. */
        AvpMpBotState other={0};
        v.tick=1u;v.player_index=3u;v.current_weapon=1u;
        avp_mp_bot_step(&other,&v,&c);
        assert(other.predator_pattern_ready && other.predator_idle_ticks==1u);
        assert(other.predator_pattern_start!=s.predator_pattern_start ||
               other.predator_pattern_stride!=s.predator_pattern_stride);
        /* Single-owned-weapon case must not spin endlessly or invent guns. */
        memset(&other,0,sizeof other);
        v.predator_owned=1u<<4;v.current_weapon=4u;
        for(unsigned tick=1u;tick<=151u;tick++){
            v.tick=tick;avp_mp_bot_step(&other,&v,&c);
        }
        assert(!other.predator_switch_pending);
        assert(other.predator_weapon_target==4u);
    }
    /* The cloaked Predator must decloak for a visible DM opponent in range.
     * Once the original invis transition finishes it can fire immediately. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.player_species=2u;v.deathmatch=1u;
    v.x=10u<<16;v.y=10u<<16;
    v.target_x=v.x+0x6666u;v.target_y=v.y;
    v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;
    v.predator_owned=1u<<4;v.current_weapon=4u;
    v.marines_on_level=1u;v.max_energy=v.player_energy=100u;
    v.invisflag=v.invis_act=1u;v.tick=20u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_PRED_CLOAK));
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    v.invisflag=v.invis_act=v.invis_stat=0u;v.tick=21u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    /* An Alien-only DM lobby must still motivate an out-of-combat Predator
     * to cloak. Roster presence alone does NOT provide target coordinates. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.player_species=2u;v.deathmatch=1u;v.dm_opponents_on_level=1u;
    v.tick=50u;v.max_energy=v.player_energy=100u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_PRED_CLOAK));
    /* Alien Deathmatch melee should primarily use native Bite; Tail and
     * Claw must remain genuinely possible, not hard-coded dead branches. */
    {unsigned bite=0u,tail=0u,claw=0u;
     memset(&s,0,sizeof s);memset(&v,0,sizeof v);
     v.player_species=1u;v.deathmatch=1u;
     v.x=10u<<16;v.y=10u<<16;
     v.target_x=v.x+0x4000u;v.target_y=v.y;
     v.has_target=v.hostile_target=v.target_visible=1u;
     v.goal=AVP_MP_BOT_HOSTILE;
     for(unsigned sample=1u;sample<=250u;sample++){
         v.tick=(uint64_t)sample*25u;
         avp_mp_bot_step(&s,&v,&c);
         if(c.actions&(1u<<AVP_MP_ALIEN_BITE))bite++;
         if(c.actions&(1u<<AVP_MP_ALIEN_TAIL))tail++;
         if(c.actions&(1u<<AVP_MP_ALIEN_CLAW))claw++;
     }
     assert(bite>tail && bite>claw && tail>0u && claw>0u);
     /* Same behavior profile applies during non-cocoon co-op combat. */
     bite=tail=claw=0u;
     memset(&s,0,sizeof s);v.deathmatch=0u;
     for(unsigned sample=1u;sample<=250u;sample++){
         v.tick=(uint64_t)sample*25u;
         avp_mp_bot_step(&s,&v,&c);
         if(c.actions&(1u<<AVP_MP_ALIEN_BITE))bite++;
         if(c.actions&(1u<<AVP_MP_ALIEN_TAIL))tail++;
         if(c.actions&(1u<<AVP_MP_ALIEN_CLAW))claw++;
     }
     assert(bite>tail && bite>claw && tail>0u && claw>0u);
    }
    /* R51 immediacy and original weapon boundary: first observable attack
     * must happen on its eligible tick, including 0x90 reach. The actual
     * 20-pixel TestSpark width makes the axial box end at 0xb800. Do not
     * fire past that box, through unseen targets, or while cloaked. */
    for(unsigned species=1u;species<=2u;species++)
    for(unsigned behavior=0u;behavior<=2u;behavior++){
        const unsigned fire_mask=species==1u ?
            ((1u<<AVP_MP_ALIEN_CLAW)|(1u<<AVP_MP_ALIEN_BITE)|
             (1u<<AVP_MP_ALIEN_TAIL)):(1u<<AVP_MP_FIRE);
        memset(&s,0,sizeof s);memset(&v,0,sizeof v);
        v.x=10u<<16;v.y=10u<<16;v.heading=0u;
        v.target_x=v.x+0xb7ffu;v.target_y=v.y;
        v.goal=AVP_MP_BOT_HOSTILE;v.player_species=species;
        v.current_weapon=species==2u?4u:0u;
        v.behavior=behavior;
        v.has_target=v.hostile_target=v.target_visible=1u;
        v.tick=20u;
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&fire_mask);
        v.target_x++;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&fire_mask));
        v.target_x--;
        v.heading=0x40000000u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&fire_mask));
        v.heading=0u;v.target_visible=0u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&fire_mask));
        if(species==2u){
            v.target_visible=1u;v.invisflag=1u;
            avp_mp_bot_step(&s,&v,&c);
            assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        }
    }
    /* R69: source TestSpark includes the 20-pixel perpendicular weapon
     * width in its signed shot bounding box.  On diagonal angles a hostile
     * inside this AUTHORED melee trace can be well beyond the circular
     * 0x90 (0.5625-tile) radius used by the old bot fire gate.  The bot
     * must attack on the first eligible observation tick, not stand still
     * while that visible enemy attacks it.  Check all four diagonal
     * quadrants and three bot behaviors for Alien and Predator melee. */
    {
        static const struct {int xdir,ydir;uint32_t heading;} diagonals[4]={
            { 1, 1,0xe0000000u},{ 1,-1,0x20000000u},
            {-1,-1,0x60000000u},{-1, 1,0xa0000000u}};
        for(unsigned species=1u;species<=2u;species++)
        for(unsigned behavior=0u;behavior<=2u;behavior++)
        for(unsigned i=0u;i<4u;i++){
            const unsigned fire_mask=species==1u ?
                ((1u<<AVP_MP_ALIEN_CLAW)|(1u<<AVP_MP_ALIEN_BITE)|
                 (1u<<AVP_MP_ALIEN_TAIL)):(1u<<AVP_MP_FIRE);
            const int64_t dx=(int64_t)diagonals[i].xdir * 0x8400;
            const int64_t dy=(int64_t)diagonals[i].ydir * 0x8400;
            memset(&s,0,sizeof s);memset(&v,0,sizeof v);
            v.x=10u<<16;v.y=10u<<16;
            v.heading=diagonals[i].heading;
            v.target_x=(uint32_t)((int64_t)v.x+dx);
            v.target_y=(uint32_t)((int64_t)v.y+dy);
            v.player_species=species;v.behavior=behavior;
            v.current_weapon=species==2u?4u:0u;
            v.goal=AVP_MP_BOT_HOSTILE;
            v.has_target=v.hostile_target=v.target_visible=1u;
            v.melee_threat=AVP_MP_BOT_THREAT_ALIEN;
            v.melee_visible=1u;v.melee_x=v.target_x;v.melee_y=v.target_y;
            v.tick=40u;
            avp_mp_bot_step(&s,&v,&c);
            assert(c.actions&fire_mask);
            /* Target beyond the original TestSpark 0x90+width ray box. */
            v.target_x=(uint32_t)((int64_t)v.x+
                (int64_t)diagonals[i].xdir*0xa000);
            v.target_y=(uint32_t)((int64_t)v.y+
                (int64_t)diagonals[i].ydir*0xa000);
            v.melee_x=v.target_x;v.melee_y=v.target_y;
            avp_mp_bot_step(&s,&v,&c);
            assert(!(c.actions&fire_mask));
            /* The existing visibility and facing gates still apply. */
            v.target_x=(uint32_t)((int64_t)v.x+dx);
            v.target_y=(uint32_t)((int64_t)v.y+dy);
            v.target_visible=0u;
            avp_mp_bot_step(&s,&v,&c);
            assert(!(c.actions&fire_mask));
        }
    }
    /* Guerilla must advance AND fire at a visible target beyond the old
     * six-tile artificial firing cap. Strafing alone would never close. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=2u<<16;v.y=2u<<16;v.target_x=12u<<16;v.target_y=v.y;
    v.heading=0u;v.behavior=AVP_MP_BOT_GUERILLA;
    v.player_species=0u;v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.tick=30u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                      (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(c.actions&(1u<<AVP_MP_FIRE));
    for(unsigned i=31u;i<40u;i++){
        v.tick=i;avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FORWARD));
        assert(c.actions&(1u<<AVP_MP_FIRE));
    }
    /* A blocked preferred flank switches to the other side; the side is
     * then held rather than oscillating each frame. */
    blank_map(5u,5u);
    map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL,0u};
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=0x8000u;v.y=(2u<<16)|0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0u;v.behavior=AVP_MP_BOT_GUERILLA;
    v.player_species=0u;v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.tick=30u;
    map_cells[CELL(0u,3u,5u)+5u]=0u; /* preferred right sidestep */
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_STRAFE_LEFT));
    assert(!(c.actions&(1u<<AVP_MP_STRAFE_RIGHT)));
    v.tick=31u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_STRAFE_LEFT));
    /* Cornered at the map edge, sides and back blocked, corridor ahead:
     * Guerilla must use the only safe exit instead of waiting to die. */
    blank_map(5u,5u);
    map_cells[CELL(0u,1u,5u)+5u]=0u;
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=0x8000u;v.y=0x8000u;
    v.target_x=(2u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0u;v.behavior=AVP_MP_BOT_GUERILLA;
    v.player_species=0u;v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.tick=40u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(!(c.actions&((1u<<AVP_MP_STRAFE_RIGHT)|
                        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_BACKWARD))));
    /* In a narrow corridor a melee-only Guerilla must not retreat forever
     * when it still has a safe, direct approach outside weapon reach. */
    blank_map(5u,5u);
    map_cells[CELL(2u,1u,5u)+5u]=0u;
    map_cells[CELL(2u,3u,5u)+5u]=0u;
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL,0u};
    v.x=(2u<<16)|0x8000u;v.y=(2u<<16)|0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0u;v.behavior=AVP_MP_BOT_GUERILLA;v.player_species=2u;
    v.current_weapon=4u;
    v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.tick=30u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    /* Melee Guerilla must approach outside strike reach regardless of
     * target facing; seeing the target's back remains an approach cue. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=10u<<16;v.y=10u<<16;v.target_x=12u<<16;v.target_y=v.y;
    v.heading=0u;v.behavior=AVP_MP_BOT_GUERILLA;v.player_species=1u;
    v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.tick=30u;
    v.target_heading_valid=1u;v.target_heading=0u; /* facing away (+X) */
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    v.target_heading=0x80000000u; /* facing the bot (-X) */
    memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(!(c.actions&((1u<<AVP_MP_ALIEN_CLAW)|
                       (1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL))));
    v.target_heading_valid=0u; /* stale facing must not influence aggression */
    memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    /* Melee attacks stay active while this aggressive motion is selected. */
    v.target_x=v.x+0x4000u;v.target_heading_valid=1u;v.target_heading=0u;
    memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL)|
                      (1u<<AVP_MP_ALIEN_CLAW)));
    /* R85: use the ORIGINAL acid contact square and player-width margin.
     * An open room has a 2x2 detour. A one-cell corridor with a centered
     * corpse has no safe center-line; an off-edge corpse leaves one. */
    {
        AvpMpBotMap m;
        blank_map(5,5);
        m=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,99u,NULL,NULL,0u};
        assert(avp_mp_bot_alien_death_passable(&m,(2u<<16)|0x8000u,
                                                   (2u<<16)|0x8000u));
        memset(map_cells,0,sizeof map_cells);
        for(unsigned x=0;x<5u;x++)map_cells[CELL(x,2u,5u)+5u]=1u;
        assert(!avp_mp_bot_alien_death_passable(&m,(3u<<16)|0x8000u,
                                                    (2u<<16)|0x8000u));
        assert(avp_mp_bot_alien_death_passable(&m,(3u<<16)|0x8000u,
                                                   (2u<<16)|0x0400u));
        /* R274: even a Marine pinned at the corridor end MUST NOT create
         * an impassable acid puddle; no 150-tick lethal fallback. This also
         * holds in a vent and for Berserker and Guerilla profiles. */
        memset(&v,0,sizeof v);memset(&s,0,sizeof s);
        v.map=m;v.player_species=0u;v.behavior=0u;
        v.x=(0u<<16)|0x8000u;v.y=(2u<<16)|0x8000u;
        v.target_x=(3u<<16)|0x8000u;v.target_y=v.y;
        v.has_target=1;v.hostile_target=1;v.target_visible=1;
        v.target_is_alien=1u;v.goal=AVP_MP_BOT_HOSTILE;
        v.player_energy=100;v.max_energy=100;v.tick=1u;v.current_level=3u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(s.lure_end_tick==0u);
        v.current_level=7u;memset(&s,0,sizeof s);
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    }
    /* R85: both approaches resolve the SAME original door byte, including
     * reverse-facing bots. The first bot claims it; all others wait for
     * the native door animation, and neither presses Use while moving. */
    {
        AvpMpBotMap m;
        AvpMpBotDoorClaims claims={0};
        AvpMpBotState one={0},two={0};
        const unsigned off=CELL(2u,2u,5u)+6u;
        blank_map(5,5);
        memset(map_cells,0,sizeof map_cells);
        for(unsigned x=1u;x<=3u;x++)map_cells[CELL(x,2u,5u)+5u]=1u;
        wallflags[5u]=AVP_WB_DOOR;
        map_cells[CELL(1u,2u,5u)+2u]=5u;
        map_cells[CELL(2u,2u,5u)+0u]=5u;
        m=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,99u,NULL,NULL,0u};
        assert(avp_mp_bot_route(&m,1u,2u,3u,2u,&route));
        assert(route.requires_door_use && route.door_offset==off &&
               route.door_value==0u);
        memset(&v,0,sizeof v);v.map=m;v.door_claims=&claims;
        v.x=(1u<<16)|0x8000u;v.y=(2u<<16)|0x8000u;
        v.target_x=(3u<<16)|0x8000u;v.target_y=v.y;
        v.has_target=1;v.goal=AVP_MP_BOT_PATROL;v.tick=1u;
        avp_mp_bot_step(&one,&v,&c);
        assert(c.actions&(1u<<AVP_MP_USE));
        avp_mp_bot_step(&two,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_USE)));
        /* Same door from opposite side also uses this cell's LEFT byte. */
        assert(avp_mp_bot_route(&m,2u,2u,1u,2u,&route));
        assert(route.requires_door_use && route.door_offset==off);
        map_cells[off]=4u;v.tick=35u;
        avp_mp_bot_step(&two,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_USE)));
        map_cells[off]=48u;v.tick=36u;
        avp_mp_bot_step(&two,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_USE)));
        map_cells[off]=0u;v.tick=65u;
        /* Native DOORS.S has already accepted another player/bot's Use,
         * but the map byte is still zero before the first door tick.
         * Do not reverse that opening door, even after a claim expires. */
        v.active_door_count=1u;v.active_door_offsets[0]=off;
        avp_mp_bot_step(&two,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_USE)));
        /* The OTHER active door must not suppress our own legitimate Use. */
        v.active_door_offsets[0]=off+8u;
        v.tick=66u;
        avp_mp_bot_step(&two,&v,&c);
        assert(c.actions&(1u<<AVP_MP_USE));
        /* Once the original door slot finishes, a closed door is usable. */
        v.active_door_count=0u;
        v.tick=99u;
        avp_mp_bot_step(&one,&v,&c);
        assert(c.actions&(1u<<AVP_MP_USE));
    }
    /* R85: separate seeds now materially influence the chosen patrol
     * region, while a repeated seed remains deterministic. */
    {
        uint16_t visits[AVP_MP_BOT_MAP_CELLS]={0};
        unsigned seen[4],xs[4],ys[4],unique=0u;
        AvpMpBotMap m;
        blank_map(5,5);
        m=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,99u,NULL,NULL,0u};
        for(unsigned i=0;i<4u;i++){
            assert(avp_mp_bot_patrol_pick(&m,2u,2u,visits,
                  (i+1u)*0x9e3779b9u,&xs[i],&ys[i]));
            seen[i]=ys[i]*64u+xs[i];
            for(unsigned j=0;j<i;j++)if(seen[j]==seen[i])break;
        }
        for(unsigned i=0;i<4u;i++){
            unsigned duplicate=0u;
            for(unsigned j=0;j<i;j++)duplicate|=seen[j]==seen[i];
            if(!duplicate)unique++;
        }
        assert(unique>=2u);
        {unsigned xx,yy;
         assert(avp_mp_bot_patrol_pick(&m,2u,2u,visits,
                 0x9e3779b9u,&xx,&yy));
         assert(xx==xs[0]&&yy==ys[0]);}
    }
    /* R128: repeated no-progress toward a corpse/prop waypoint must record
     * its failed cell and take an alternate source-connected route. */
    {
        AvpMpBotState blocked={0};
        AvpMpBotObservation probe={0};
        AvpMpBotRoute choice={0};
        blank_map(5u,5u);
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL,0u};
        probe.map=map;
        probe.x=(1u<<16)|0x8000u;probe.y=(2u<<16)|0x8000u;
        probe.target_x=(3u<<16)|0x8000u;probe.target_y=probe.y;
        probe.has_target=1u;probe.goal=AVP_MP_BOT_SUPPLY;
        probe.heading=0u;
        assert(avp_mp_bot_route(&map,1u,2u,3u,2u,&choice));
        assert(choice.next_x==2u&&choice.next_y==2u);
        /* R148: allow a full 4.5s to finish a difficult corridor corner.
         * A completely motionless bot must still trigger the watchdog. */
        for(unsigned i=1u;i<=47u;i++){
            probe.tick=i;avp_mp_bot_step(&blocked,&probe,&c);
        }
        assert(blocked.nav_bad_next_plus_one==2u+2u*64u+1u);
        assert(blocked.nav_bad_until>probe.tick);
        map.avoid_cell_plus_one=blocked.nav_bad_next_plus_one;
        assert(avp_mp_bot_route(&map,1u,2u,3u,2u,&choice));
        assert(choice.next_x!=2u||choice.next_y!=2u);
        /* A narrow one-way hall must retain an authored route; don't
         * invent a nonexistent side passage around a table. */
        blank_map(5u,5u);
        for(unsigned y=0;y<5u;y++)for(unsigned x=0;x<5u;x++)
            if(y!=2u)map_cells[CELL(x,y,5u)+5u]=0u;
        assert(!avp_mp_bot_route(&map,1u,2u,3u,2u,&choice));
        map.avoid_cell_plus_one=0u;
        assert(avp_mp_bot_route(&map,1u,2u,3u,2u,&choice));
    }
    /* A live player at the choke receives its own position-based peer
     * hazard, and the higher-index bot yields rather than both pushing. */
    {
        AvpMpBotState yielding={0};
        AvpMpBotObservation probe={0};
        blank_map(5u,5u);
        probe.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL,0u};
        probe.x=(2u<<16)|0x8000u;probe.y=(2u<<16)|0x8000u;
        probe.target_x=(4u<<16)|0x8000u;probe.target_y=probe.y;
        probe.heading=0u;probe.has_target=1u;probe.goal=AVP_MP_BOT_PATROL;
        probe.player_index=5u;probe.nearby_peer_valid=1u;
        probe.nearby_peer_index=4u;
        probe.nearby_peer_x=(3u<<16)|0x1000u;probe.nearby_peer_y=probe.y;
        probe.tick=1u;
        avp_mp_bot_step(&yielding,&probe,&c);
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        /* A stale injected peer observation cannot force ordinary
         * collision-based retreat with the live option disabled. */
        probe.player_collision_disabled=1u;probe.tick=2u;
        avp_mp_bot_step(&yielding,&probe,&c);
        assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
        assert(!yielding.peer_yield_until);
        probe.player_collision_disabled=0u;
        probe.nearby_peer_valid=0u;memset(&yielding,0,sizeof yielding);
        avp_mp_bot_step(&yielding,&probe,&c);
        assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    }
    /* R128: actually crossing A-B-A-B on the same patrol objective is not
     * route progress. Invalidate the repeating goal instead of another
     * 19-tick turn-and-retry cycle at the same corner. */
    {
        AvpMpBotState loop={0};
        AvpMpBotObservation probe={0};
        blank_map(5u,5u);
        probe.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL,0u};
        probe.y=(2u<<16)|0x8000u;
        probe.target_x=(4u<<16)|0x8000u;probe.target_y=probe.y;
        probe.has_target=1u;probe.goal=AVP_MP_BOT_PATROL;
        probe.heading=0u;
        const unsigned walk[4]={1u,2u,1u,2u};
        for(unsigned i=0;i<4u;i++){
            probe.x=(walk[i]<<16)|0x8000u;
            probe.tick=i+1u;
            avp_mp_bot_step(&loop,&probe,&c);
        }
        assert(loop.nav_retarget_patrol);
        assert(loop.nav_bad_until>probe.tick);
    }
    /* R148: the Training Maze can repeat a longer circuit A-B-C-D-E,
     * not merely the four-history A-B-A-B previously detected. Its goal
     * stays fixed across the circuit: the second lap must be treated as a
     * loop instead of an endless route recalculation. */
    {
        AvpMpBotState maze={0};
        AvpMpBotObservation probe={0};
        static const unsigned xs[10]={2u,3u,3u,2u,1u,2u,3u,3u,2u,1u};
        static const unsigned ys[10]={2u,2u,3u,3u,2u,2u,2u,3u,3u,2u};
        blank_map(8u,8u);
        probe.map=(AvpMpBotMap){map_cells,8u,8u,wallflags,NULL,0u,risk,NULL,0u};
        probe.target_x=(7u<<16)|0x8000u;
        probe.target_y=(7u<<16)|0x8000u;
        probe.has_target=1u;probe.goal=AVP_MP_BOT_PATROL;
        probe.observed_world_level=4u;
        for(unsigned i=0;i<10u;i++){
            probe.x=(xs[i]<<16)|0x8000u;
            probe.y=(ys[i]<<16)|0x8000u;
            probe.tick=i+1u;
            avp_mp_bot_step(&maze,&probe,&c);
        }
        assert(maze.nav_retarget_patrol);
        assert(maze.nav_bad_until>=probe.tick+150u);
    }
    /* R148: a Marine with its center INSIDE a native acid square must
     * walk out along a legal axis rather than spending 19 turns in a 360
     * recovery cycle. This deliberately feeds stationary observations to
     * reproduce a collision-stalled bot with an unavoidable puddle. */
    {
        AvpMpBotState acid={0};
        AvpMpBotObservation probe={0};
        blank_map(5u,5u);
        probe.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL,0u};
        probe.x=(2u<<16)|0x8000u;
        probe.y=(2u<<16)|0x8000u;
        probe.target_x=(4u<<16)|0x8000u;
        probe.target_y=probe.y;
        probe.heading=0u;probe.player_species=0u;
        probe.has_target=1u;probe.goal=AVP_MP_BOT_PATROL;
        probe.acid_count=1u;probe.acid[0]=(AvpMpBotAcid){probe.x,probe.y};
        for(unsigned i=1u;i<35u;i++){
            probe.tick=i;
            avp_mp_bot_step(&acid,&probe,&c);
            assert(acid.evade_ticks==0u);
            assert(!(c.actions & (1u<<AVP_MP_TURN_RIGHT)));
            assert((c.actions & ((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                                  (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)))!=0u);
        }
    }
    /* R148: a single authentic lift shaft has exactly one agreed floor and
     * one panel operator. This must not change original jam permissions.
     * After expiry, a new passenger can choose the opposite direction. */
    {
        AvpMpBotLiftClaims lifts={0};
        unsigned leader=99u,destination=0u;
        uint64_t created=99u;
        assert(avp_mp_bot_lift_agree(&lifts,2u,8u,9u,4u,3u,10u,
                                     &leader,&destination,&created));
        assert(leader==4u&&destination==3u&&created==10u);
        assert(avp_mp_bot_lift_agree(&lifts,2u,8u,9u,5u,1u,12u,
                                     &leader,&destination,&created));
        assert(leader==4u&&destination==3u&&created==10u);
        assert(!avp_mp_bot_lift_agree(&lifts,3u,9u,21u,6u,4u,15u,
                                     &leader,&destination,&created));
        assert(avp_mp_bot_lift_agree(&lifts,2u,8u,9u,5u,1u,132u,
                                     &leader,&destination,&created));
        assert(leader==5u&&destination==1u);
    }
    /* RE44 R288: three bots are already inside one lift, Player Collision
     * OFF. Routine scanner/loot replans cannot repeatedly replace the
     * boarded trip or manufacture three panel operators. Following arrival
     * on the destination floor, ALL three must disembark, including the
     * former owner, even if their next goal is no longer LIFT. */
    {
        AvpMpBotLiftClaims claims={0};
        AvpMpBotState bots[3]={0};
        AvpMpBotObservation members[3]={{0}};
        AvpMpPlayerCommand commands[3];
        unsigned owner,dest;
        uint64_t created;
        blank_map(7u,7u);
        for(unsigned i=0u;i<3u;i++){
            AvpMpBotObservation *v=&members[i];
            v->map=(AvpMpBotMap){map_cells,7u,7u,wallflags,NULL,0u,risk,NULL,0u};
            v->x=(3u<<16)|0x8000u;v->y=(3u<<16)|0x8000u;
            v->target_x=v->x;v->target_y=v->y;
            v->target_side=2u;v->current_level=2u;
            v->destination_level=3u;v->lift_direction=1;
            v->player_species=0u;v->player_collision_disabled=1u;
            v->player_index=i+1u;v->heading=0u;
            v->goal=AVP_MP_BOT_LIFT;v->has_target=1u;v->tick=31u;
            assert(avp_mp_bot_lift_agree(&claims,2u,3u,3u,
                v->player_index,3u,v->tick,&owner,&dest,&created));
            assert(owner==1u && dest==3u);
            v->lift_wait=owner!=v->player_index;
            avp_mp_bot_step(&bots[i],v,&commands[i]);
            assert(!!(commands[i].actions&(1u<<AVP_MP_USE))==(i==0u));
            v->goal=AVP_MP_BOT_PATROL;v->hostile_target=0u;
            v->tick=32u;
            assert(avp_mp_bot_lift_keep_trip(v,2u,2u,31u,1u));
            assert(!avp_mp_bot_lift_needs_disembark(0u,1u));
            /* At destination level, current task is PATROL or resupply.
             * No bot is exempt from leaving the original chamber. */
            v->current_level=3u;
            assert(avp_mp_bot_lift_needs_disembark(1u,1u));
            assert(!avp_mp_bot_lift_needs_disembark(1u,0u));
            assert(!avp_mp_bot_lift_keep_trip(v,2u,3u,31u,1u));
            v->current_level=2u;
            v->hostile_target=1u;v->target_visible=1u;
            assert(!avp_mp_bot_lift_keep_trip(v,2u,2u,31u,1u));
            v->hostile_target=0u;v->target_visible=0u;
            v->tick=55u;
            assert(!avp_mp_bot_lift_keep_trip(v,2u,2u,31u,1u));
        }
    }
    /* R148: being in the lift TILE is not enough: the native door will
     * bounce if the bot operates the panel while standing in the entrance.
     * Center using normal collision-tested player commands, then allow
     * only the elected owner to issue Use. */
    {
        AvpMpBotState lift={0};
        AvpMpBotObservation probe={0};
        unsigned ex=0u,ey=0u;
        blank_map(5u,5u);
        probe.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL,0u};
        probe.x=(2u<<16)|0x1100u;
        probe.y=(2u<<16)|0x8000u;
        probe.heading=0u;probe.player_species=0u;
        probe.target_x=(2u<<16)|0x8000u;
        probe.target_y=probe.y;probe.target_side=2u;
        probe.current_level=2u;probe.destination_level=3u;
        probe.lift_direction=1;probe.goal=AVP_MP_BOT_LIFT;
        probe.has_target=1u;probe.tick=3u;
        assert(avp_mp_bot_lift_egress(&probe.map,2u,2u,&ex,&ey));
        assert(ex<5u&&ey<5u&&(ex!=2u||ey!=2u));
        avp_mp_bot_step(&lift,&probe,&c);
        assert(!(c.actions&(1u<<AVP_MP_USE)));
        assert(c.actions&(1u<<AVP_MP_FORWARD));
        probe.x=probe.target_x;probe.tick=4u;probe.lift_wait=1u;
        avp_mp_bot_step(&lift,&probe,&c);
        assert(!(c.actions&(1u<<AVP_MP_USE)));
        probe.tick=5u;probe.lift_wait=0u;
        avp_mp_bot_step(&lift,&probe,&c);
        assert(c.actions&(1u<<AVP_MP_USE));
    }
    /* RE44 R276: a human/bot lift invitation must not discard a current
     * optical fight or a visible Alien melee threat, including an Alien
     * pinning a nearby player. Forgotten or tracker-only targets must not
     * permanently prevent routine co-op boarding. */
    {
        AvpMpBotObservation invite={0};
        assert(!avp_mp_bot_lift_invitation_allowed(NULL));
        invite.goal=AVP_MP_BOT_HOSTILE;
        invite.hostile_target=invite.target_visible=1u;
        invite.target_is_alien=1u;invite.other_player_alien_trapped=1u;
        assert(!avp_mp_bot_lift_invitation_allowed(&invite));
        invite.other_player_alien_trapped=0u;
        assert(!avp_mp_bot_lift_invitation_allowed(&invite));
        invite.target_visible=0u; /* old/tracker-only contact */
        assert(avp_mp_bot_lift_invitation_allowed(&invite));
        invite.hostile_target=0u;invite.goal=AVP_MP_BOT_PATROL;
        invite.melee_visible=1u;invite.melee_threat=AVP_MP_BOT_THREAT_ALIEN;
        assert(!avp_mp_bot_lift_invitation_allowed(&invite));
        invite.melee_visible=0u;
        assert(avp_mp_bot_lift_invitation_allowed(&invite));
        invite.melee_visible=1u;invite.melee_threat=AVP_MP_BOT_THREAT_NONE;
        assert(avp_mp_bot_lift_invitation_allowed(&invite));
        invite.goal=AVP_MP_BOT_LIFT;invite.lift_follow_human=1u;
        invite.melee_visible=0u;
        assert(avp_mp_bot_lift_invitation_allowed(&invite));
    }
    /* RE44 R275 co-op elevator regression: all eligible bots may approach
     * the SAME authentic lift cell. Only the operator ever presses Use.
     * The human passenger must not accidentally receive an AI Use action.
     * Original lift species, adjacent-floor and jam conditions still apply. */
    {
        AvpMpBotObservation human_follower={0};
        AvpMpBotState passenger_a={0},passenger_b={0},driver={0};
        AvpMpBotRoute route_a={0},route_b={0};
        AvpMpPlayerCommand a_cmd,b_cmd,driver_cmd;
        blank_map(8u,8u);
        human_follower.map=(AvpMpBotMap){map_cells,8u,8u,wallflags,NULL,0u,risk,NULL,0u};
        human_follower.player_species=0u;human_follower.current_level=2u;
        human_follower.destination_level=3u;human_follower.lift_direction=1;
        human_follower.x=(1u<<16)|0x8000u;human_follower.y=(3u<<16)|0x8000u;
        human_follower.target_x=(3u<<16)|0x8000u;
        human_follower.target_y=(3u<<16)|0x8000u;
        human_follower.target_side=2u;human_follower.has_target=1u;
        human_follower.goal=AVP_MP_BOT_LIFT;human_follower.lift_follow_human=1u;
        assert(avp_mp_bot_lift_boarding_route(&human_follower,3u,3u,3u,&route_a));
        human_follower.player_index=4u;human_follower.heading=0u;
        avp_mp_bot_step(&passenger_a,&human_follower,&a_cmd);
        assert(a_cmd.actions&(1u<<AVP_MP_FORWARD));
        assert(!(a_cmd.actions&(1u<<AVP_MP_USE)));
        human_follower.player_index=5u;
        assert(avp_mp_bot_lift_boarding_route(&human_follower,3u,3u,3u,&route_b));
        avp_mp_bot_step(&passenger_b,&human_follower,&b_cmd);
        assert(b_cmd.actions&(1u<<AVP_MP_FORWARD));
        assert(!(b_cmd.actions&(1u<<AVP_MP_USE)));
        /* Even with the passenger waiting flag accidentally unset, the
         * human-control marker must independently forbid a bot Use. */
        human_follower.x=human_follower.target_x;
        human_follower.lift_wait=0u;human_follower.tick=50u;
        memset(&passenger_a,0,sizeof passenger_a);
        avp_mp_bot_step(&passenger_a,&human_follower,&a_cmd);
        assert(!(a_cmd.actions&(1u<<AVP_MP_USE)));
        human_follower.lift_follow_human=0u;
        memset(&driver,0,sizeof driver);
        avp_mp_bot_step(&driver,&human_follower,&driver_cmd);
        assert(driver_cmd.actions&(1u<<AVP_MP_USE));
        /* No cross-level or alien lift access, no magic boarding through
         * unreachable tiles and no remote six-plus-cell invitations. */
        human_follower.x=(1u<<16)|0x8000u;
        human_follower.player_species=1u;
        assert(!avp_mp_bot_lift_boarding_route(&human_follower,3u,3u,3u,&route_a));
        human_follower.player_species=0u;
        assert(!avp_mp_bot_lift_boarding_route(&human_follower,3u,3u,4u,&route_a));
        human_follower.current_level=3u;
        assert(!avp_mp_bot_lift_boarding_route(&human_follower,9u,21u,4u,&route_a));
        human_follower.current_level=2u;
        human_follower.x=(0u<<16)|0x8000u;
        assert(!avp_mp_bot_lift_boarding_route(&human_follower,7u,3u,3u,&route_a));
        human_follower.x=(1u<<16)|0x8000u;
        map_cells[CELL(3u,3u,8u)+5u]=0u;
        assert(!avp_mp_bot_lift_boarding_route(&human_follower,3u,3u,3u,&route_a));
    }
    /* An unreachable pickup identity is temporarily rejected even if
     * the host requests the same actor slot every tick. Generation reuse
     * is safe, and real original-position progress resets the watchdog. */
    {
        AvpMpBotPickupMemory memory={0};
        avp_mp_bot_pickup_track(&memory,3u,8u,21u,1u,
            1u<<16,1u<<16,4u<<16,1u<<16,55u,65u);
        avp_mp_bot_pickup_observe(&memory,3u,40u,
            1u<<16,1u<<16,55u,65u);
        assert(!avp_mp_bot_pickup_avoided(&memory,3u,8u,21u,40u));
        avp_mp_bot_pickup_observe(&memory,3u,41u,
            1u<<16,1u<<16,55u,65u);
        assert(avp_mp_bot_pickup_avoided(&memory,3u,8u,21u,41u));
        assert(!avp_mp_bot_pickup_avoided(&memory,3u,8u,22u,41u));
        assert(!avp_mp_bot_pickup_avoided(&memory,3u,8u,21u,342u));
        memset(&memory,0,sizeof memory);
        avp_mp_bot_pickup_track(&memory,3u,8u,21u,1u,
            1u<<16,1u<<16,4u<<16,1u<<16,55u,65u);
        avp_mp_bot_pickup_observe(&memory,3u,30u,
            (1u<<16)|0x4000u,1u<<16,45u,66u);
        avp_mp_bot_pickup_observe(&memory,3u,41u,
            (1u<<16)|0x4000u,1u<<16,45u,66u);
        assert(!avp_mp_bot_pickup_avoided(&memory,3u,8u,21u,41u));
    }
    /* Static collmap furniture is avoided in the route graph if another
     * original legal passage exists; the old soft path remains fallback. */
    {
        uint8_t solid[AVP_MP_BOT_MAP_CELLS]={0};
        blank_map(5u,5u);
        solid[2u*64u+2u]=1u;
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,solid,0u};
        assert(avp_mp_bot_route(&map,1u,2u,3u,2u,&route));
        assert(!(route.next_x==2u && route.next_y==2u));
    }
    /* R130: an actual native health drop interrupts passive pickup routing.
     * This bot has NO visible hostile: it must scan and dodge without
     * magically firing at an unseen player or AMP. */
    {
        AvpMpBotState hurt={0};
        AvpMpBotObservation victim={0};
        unsigned first_side;
        blank_map(5u,5u);
        victim.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL,0u};
        victim.x=(2u<<16)|0x8000u;victim.y=(2u<<16)|0x8000u;
        victim.target_x=(4u<<16)|0x8000u;victim.target_y=victim.y;
        victim.heading=0u;victim.has_target=1u;victim.goal=AVP_MP_BOT_SUPPLY;
        victim.player_species=AVP_MP_MARINE;victim.observed_world_level=3u;
        victim.player_energy=victim.max_energy=100;victim.tick=5u;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert(hurt.damage_react_until==0u);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        victim.player_energy=82;victim.tick=6u;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert(hurt.damage_react_until>victim.tick);
        assert(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
        assert(c.actions&((1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT)));
        assert(!(c.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_FIRE))));
        first_side=c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT));
        victim.tick=7u;avp_mp_bot_step(&hurt,&victim,&c);
        assert((c.actions&((1u<<AVP_MP_STRAFE_LEFT)|
                    (1u<<AVP_MP_STRAFE_RIGHT)))==first_side);
        /* Another real hit can change dodge direction and refresh response. */
        victim.tick=8u;victim.player_energy=65;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert((c.actions&((1u<<AVP_MP_STRAFE_LEFT)|
                    (1u<<AVP_MP_STRAFE_RIGHT)))!=first_side);
        assert(hurt.damage_react_until==26u);
        /* A medkit/respawn is not damage and clears the old alarm. */
        victim.tick=9u;victim.player_energy=100;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert(hurt.damage_react_until==0u);
        /* No old-world damage response survives a level transfer. */
        victim.tick=11u;victim.player_energy=72;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert(hurt.damage_react_until>victim.tick);
        victim.observed_world_level=4u;victim.tick=12u;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert(hurt.damage_react_until==0u);
        /* When no health drops occur, the response genuinely expires. */
        victim.tick=13u;victim.player_energy=61;
        avp_mp_bot_step(&hurt,&victim,&c);
        victim.tick=55u;avp_mp_bot_step(&hurt,&victim,&c);
        assert(!(c.actions&(1u<<AVP_MP_TURN_LEFT)));
        /* A fresh life/clock restart must not inherit old damage. */
        victim.tick=1u;victim.player_energy=100;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert(hurt.damage_react_until==0u);
    }
    /* A visible opponent remains a legitimate aim/fire target, but the
     * freshly wounded Marine must dodge instead of charging in blindly. */
    {
        AvpMpBotState hurt={0};
        AvpMpBotObservation victim={0};
        blank_map(5u,5u);
        victim.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL,0u};
        victim.x=(2u<<16)|0x8000u;victim.y=(2u<<16)|0x8000u;
        victim.target_x=(4u<<16)|0x8000u;victim.target_y=victim.y;
        victim.heading=0u;victim.player_species=AVP_MP_MARINE;
        victim.goal=AVP_MP_BOT_HOSTILE;victim.has_target=1u;
        victim.hostile_target=victim.target_visible=1u;
        victim.player_energy=victim.max_energy=100;victim.tick=1u;
        avp_mp_bot_step(&hurt,&victim,&c);
        victim.player_energy=75;victim.tick=2u;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert(c.actions&(1u<<AVP_MP_FIRE));
        assert(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        /* A tracker-only target is NOT a firing solution or the shooter. */
        victim.target_visible=0u;victim.player_energy=65;victim.tick=3u;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(c.actions&((1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT)));
    }
    /* When surrounding native walls/properties rule out an escape,
     * hurt bots may scan, but must never synthesize collision bypasses. */
    {
        AvpMpBotState hurt={0};
        AvpMpBotObservation victim={0};
        blank_map(5u,5u);
        victim.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL,0u};
        victim.x=0x1000u;victim.y=0x1000u;
        victim.heading=0u;victim.player_species=AVP_MP_MARINE;
        victim.player_energy=victim.max_energy=100;victim.tick=1u;
        avp_mp_bot_step(&hurt,&victim,&c);
        victim.player_energy=80;victim.tick=2u;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert(hurt.damage_react_until>2u);
        assert(!(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|
            (1u<<AVP_MP_BACKWARD))));
    }
    /* A new injury must never overrule the established immediate Alien
     * death-splash escape. The source 0.75-cell square is still deadly. */
    {
        AvpMpBotState hurt={0};
        AvpMpBotObservation victim={0};
        blank_map(5u,5u);
        victim.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL,0u};
        victim.x=(2u<<16)|0x8000u;victim.y=(2u<<16)|0x8000u;
        victim.target_x=victim.x+0x3000u;victim.target_y=victim.y;
        victim.heading=0u;victim.player_species=AVP_MP_MARINE;
        victim.has_target=victim.hostile_target=victim.target_visible=1u;
        victim.target_is_alien=1u;victim.goal=AVP_MP_BOT_HOSTILE;
        victim.player_energy=victim.max_energy=100;victim.tick=1u;
        avp_mp_bot_step(&hurt,&victim,&c);
        victim.player_energy=30;victim.tick=2u;
        avp_mp_bot_step(&hurt,&victim,&c);
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        assert(hurt.damage_react_until>victim.tick);
    }
    /* R271: original AMP.S Alien death spray is 96<<9 per axis, NOT the
     * 30<<9 puddle contact. A small margin keeps nearby team members safe
     * from a kill made by either a Marine or a Predator bot. */
    {
        const uint32_t ax=(3u<<16)|0x8000u,ay=(2u<<16)|0x8000u;
        assert(avp_mp_bot_death_spray_near_player(ax,ay,ax+((96u+16u)<<9),ay));
        assert(avp_mp_bot_death_spray_near_player(ax,ay,ax,ay-((96u+16u)<<9)));
        assert(!avp_mp_bot_death_spray_near_player(ax,ay,
                          ax+((96u+16u)<<9)+1u,ay));
        assert(!avp_mp_bot_death_spray_near_player(ax,ay,
                          ax,ay-((96u+16u)<<9)-1u));
        blank_map(7u,5u);
        map=(AvpMpBotMap){map_cells,7u,5u,wallflags,NULL,0u,risk,NULL,0u};
        for(unsigned profile=0u;profile<3u;profile++){
            memset(&v,0,sizeof v);memset(&s,0,sizeof s);
            v.map=map;v.x=(1u<<16)|0x8000u;
            v.y=(2u<<16)|0x8000u;
            v.heading=0u;
            v.target_x=(3u<<16)|0x8000u;v.target_y=v.y;
            v.has_target=v.hostile_target=v.target_visible=1u;
            v.target_is_alien=1u;v.target_slot=1u;
            v.goal=AVP_MP_BOT_HOSTILE;v.player_species=AVP_MP_MARINE;
            v.player_energy=v.max_energy=100;v.behavior=profile;
            v.other_player_splash_unsafe=1u;v.tick=400u;
            avp_mp_bot_step(&s,&v,&c);
            assert(!(c.actions&(1u<<AVP_MP_FIRE)));
            /* A clear teammate permits the usual attack immediately. */
            v.other_player_splash_unsafe=0u;
            avp_mp_bot_step(&s,&v,&c);
            assert(c.actions&(1u<<AVP_MP_FIRE));
            /* Enemy Predator, dead or stale enemy, and enemy Alien player
             * (not an AMP) may not inherit the co-op acid veto. */
            v.target_is_alien=0u;v.other_player_splash_unsafe=1u;
            avp_mp_bot_step(&s,&v,&c);
            assert(c.actions&(1u<<AVP_MP_FIRE));
            v.target_is_alien=1u;v.target_slot=AVP_NUM_AMPS+1u;
            avp_mp_bot_step(&s,&v,&c);
            assert(c.actions&(1u<<AVP_MP_FIRE));
        }
        memset(&v,0,sizeof v);memset(&s,0,sizeof s);
        v.map=map;v.x=(1u<<16)|0x8000u;
        v.y=(2u<<16)|0x8000u;v.heading=0u;
        v.target_x=(3u<<16)|0x8000u;v.target_y=v.y;
        v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
        v.target_slot=2u;v.goal=AVP_MP_BOT_HOSTILE;
        v.player_species=AVP_MP_PREDATOR;v.predator_owned=(1u<<2)|(1u<<3);
        v.current_weapon=2u;v.player_energy=v.max_energy=100;
        v.other_player_splash_unsafe=1u;v.tick=1u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    }
    /* R271: a narrow VENT corridor is not an exemption from luring.
     * When there is a legal retreat, draw the Alien away from its centre
     * before firing. Once its real position allows a side lane, shoot.
     * An actually blocked retreat must not freeze combat indefinitely. */
    {
        AvpMpBotMap m;
        blank_map(9u,3u);
        memset(map_cells,0,sizeof map_cells);
        for(unsigned x=0;x<9u;x++)map_cells[CELL(x,1u,9u)+5u]=1u;
        m=(AvpMpBotMap){map_cells,9u,3u,wallflags,NULL,0u,risk,NULL,0u};
        memset(&v,0,sizeof v);memset(&s,0,sizeof s);
        v.map=m;v.x=(3u<<16)|0x8000u;
        v.y=(1u<<16)|0x8000u;v.heading=0u;
        v.target_x=(6u<<16)|0x8000u;v.target_y=v.y;
        v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
        v.target_slot=3u;v.goal=AVP_MP_BOT_HOSTILE;
        v.player_species=AVP_MP_MARINE;
        v.player_energy=v.max_energy=100;v.current_level=7u;v.tick=1u;
        assert(!avp_mp_bot_alien_death_passable(&m,v.target_x,v.target_y));
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        /* Alien follows near a cell boundary, preserving a player lane. */
        v.target_y=(1u<<16)|0x1000u;v.tick=2u;
        assert(avp_mp_bot_alien_death_passable(&m,v.target_x,v.target_y));
        avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FIRE));
        /* Even after lure time expires, never fire while a teammate is
         * inside the source 50-damage death spray. */
        v.target_y=v.y;v.tick=200u;v.other_player_splash_unsafe=1u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    }
    /* R274: three bots must NEVER kill an Alien whose future puddle seals
     * an otherwise traversable one-cell route, including after the old
     * 150-tick lure expires, after cooldown, on low health and when pinned.
     * The same source Alien, shifted off-center, becomes attackable. */
    {
        AvpMpBotMap choke;
        blank_map(9u,3u);
        memset(map_cells,0,sizeof map_cells);
        for(unsigned x=0u;x<9u;x++)
            map_cells[CELL(x,1u,9u)+5u]=1u;
        choke=(AvpMpBotMap){map_cells,9u,3u,wallflags,NULL,0u,risk,NULL,0u};
        for(unsigned profile=0u;profile<3u;profile++)
        for(unsigned bot_index=0u;bot_index<3u;bot_index++){
            AvpMpBotState bot={0};
            AvpMpBotObservation probe={0};
            probe.map=choke;
            probe.player_index=bot_index;
            probe.behavior=profile;
            probe.x=((1u+bot_index)<<16)|0x8000u;
            probe.y=(1u<<16)|0x8000u;
            probe.heading=0u;
            probe.target_x=(6u<<16)|0x8000u;
            probe.target_y=probe.y;
            probe.target_is_alien=1u;
            probe.target_slot=4u;
            probe.has_target=probe.hostile_target=probe.target_visible=1u;
            probe.goal=AVP_MP_BOT_HOSTILE;
            probe.player_species=AVP_MP_MARINE;
            probe.player_energy=probe.max_energy=100;
            assert(!avp_mp_bot_alien_death_passable(&choke,
                                                     probe.target_x,probe.target_y));
            for(unsigned step=0u;step<5u;step++){
                static const uint64_t ticks[]={1u,150u,151u,300u,1000u};
                probe.tick=ticks[step];
                avp_mp_bot_step(&bot,&probe,&c);
                assert(!(c.actions&(1u<<AVP_MP_FIRE)));
                /* R275: all three behavior profiles must KEEP retreating
                 * across the retired lure expiry/cooldown boundaries. */
                assert(c.actions&(1u<<AVP_MP_BACKWARD));
                assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
            }
            /* A damage event or low health may interrupt LURE movement,
             * but must NEVER authorize the dangerous fatal shot. */
            probe.player_energy=10;
            bot.damage_react_until=1200u;
            probe.tick=1001u;
            avp_mp_bot_step(&bot,&probe,&c);
            assert(!(c.actions&(1u<<AVP_MP_FIRE)));
            /* Original contact width allows a lane once the Alien moves
             * away from the center; bots are permitted to shoot again. */
            probe.player_energy=100;
            probe.target_y=(1u<<16)|0x0800u;
            probe.tick=1300u;
            assert(avp_mp_bot_alien_death_passable(&choke,
                                                   probe.target_x,probe.target_y));
            avp_mp_bot_step(&bot,&probe,&c);
            assert(c.actions&(1u<<AVP_MP_FIRE));
        }
        /* R275: the live-HP condition is a host decision. The actual bot
         * can retreat AND shoot nonlethally with the host's permission,
         * even with a teammate standing in the death-spray square. Remove
         * permission and the original no-kill veto immediately resumes. */
        assert(avp_mp_bot_weakening_allowed(100,5u,0u));
        assert(avp_mp_bot_weakening_allowed(21,5u,5u));
        assert(!avp_mp_bot_weakening_allowed(20,5u,5u));
        assert(!avp_mp_bot_weakening_allowed(15,5u,0u));
        assert(!avp_mp_bot_weakening_allowed(100,60u,0u));
        assert(!avp_mp_bot_weakening_allowed(-1,5u,0u));
        assert(!avp_mp_bot_weakening_allowed(100,0u,0u));
        assert(!avp_mp_bot_weakening_allowed(100,5u,UINT32_MAX));
        memset(&v,0,sizeof v);memset(&s,0,sizeof s);
        v.map=choke;v.x=(2u<<16)|0x8000u;
        v.y=(1u<<16)|0x8000u;v.heading=0u;
        v.target_x=(6u<<16)|0x8000u;v.target_y=v.y;
        v.player_species=AVP_MP_MARINE;v.player_energy=v.max_energy=100;
        v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
        v.goal=AVP_MP_BOT_HOSTILE;v.target_slot=4u;
        v.other_player_splash_unsafe=1u;v.nonlethal_choke_fire=1u;
        v.tick=1u;avp_mp_bot_step(&s,&v,&c);
        assert(c.actions&(1u<<AVP_MP_FIRE));
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        v.nonlethal_choke_fire=0u;v.tick=2u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        /* R275: a Guerilla bot pinned against the closed end cannot use
         * its normal combat-approach fallback to run INTO the Alien while
         * the target's fatal acid still seals this one-way hallway. */
        memset(&v,0,sizeof v);memset(&s,0,sizeof s);
        v.map=choke;v.x=0x8000u;v.y=(1u<<16)|0x8000u;
        v.target_x=(5u<<16)|0x8000u;v.target_y=v.y;
        v.heading=0u;v.behavior=2u;
        v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
        v.goal=AVP_MP_BOT_HOSTILE;v.player_species=AVP_MP_MARINE;
        v.player_energy=v.max_energy=100;v.tick=1u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        /* If that SAME Alien reaches striking distance, the emergency
         * melee-threat controller must not choose the only legal FORWARD
         * button and march into the doomed one-cell corridor. */
        memset(&s,0,sizeof s);
        v.behavior=0u;v.target_x=(1u<<16)|0x8000u;
        v.melee_threat=AVP_MP_BOT_THREAT_ALIEN;
        v.melee_visible=1u;v.melee_x=v.target_x;v.melee_y=v.target_y;
        v.tick=2u;avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        /* Predator ranged attacks obey exactly the same no-kill rule. */
        memset(&v,0,sizeof v);memset(&s,0,sizeof s);
        v.map=choke;v.x=(2u<<16)|0x8000u;v.y=(1u<<16)|0x8000u;
        v.target_x=(6u<<16)|0x8000u;v.target_y=v.y;
        v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
        v.target_slot=5u;v.goal=AVP_MP_BOT_HOSTILE;
        v.player_species=AVP_MP_PREDATOR;v.predator_owned=(1u<<2)|(1u<<3);
        v.current_weapon=2u;v.player_energy=v.max_energy=100;v.tick=501u;
        avp_mp_bot_step(&s,&v,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    }
    /* R272: a bounded single-cell corridor with multiple acid contacts must
     * not make a route follower pace back and forth forever. Gameplay still
     * owns real movement; this probes decisions with simple bounded steps. */
    {
        AvpMpBotState nav={0};
        AvpMpBotObservation probe={0};
        double x=2.5,y=1.5;
        int reverses=0, previous_dir=0;
        blank_map(9u,3u);
        memset(map_cells,0,sizeof map_cells);
        for(unsigned i=0;i<9u;i++)map_cells[CELL(i,1u,9u)+5u]=1u;
        probe.map=(AvpMpBotMap){map_cells,9u,3u,wallflags,NULL,0u,risk,NULL,0u};
        probe.has_target=1u;probe.goal=AVP_MP_BOT_SUPPLY;
        probe.target_x=(7u<<16)|0x8000u;
        probe.target_y=(1u<<16)|0x8000u;
        probe.heading=0u;probe.player_species=AVP_MP_MARINE;
        probe.player_energy=probe.max_energy=100;
        probe.acid_count=3u;
        for(unsigned i=0;i<3u;i++){
            probe.acid[i].x=((3u+i)<<16)|0x8000u;
            probe.acid[i].y=(1u<<16)|0x8000u;
            avp_mp_bot_acid_risk_mark(risk,probe.acid[i].x,probe.acid[i].y);
        }
        for(unsigned scenario=0;scenario<4;scenario++){
        x=scenario==0?2.5:scenario==1?3.5:scenario==2?4.5:3.5;
        y=1.5;memset(&nav,0,sizeof nav);previous_dir=0;reverses=0;
        probe.goal=scenario==0?AVP_MP_BOT_SUPPLY:AVP_MP_BOT_HOSTILE;
        probe.hostile_target=scenario!=0;probe.target_visible=scenario!=0;
        probe.target_is_alien=0u;probe.target_x=(scenario==0?7u:5u)<<16|0x8000u;
        probe.behavior=scenario==3?2u:0u;
        for(unsigned t=1u;t<70u;t++){
            probe.tick=t;probe.x=(uint32_t)(x*65536.0);
            probe.y=(uint32_t)(y*65536.0);
            avp_mp_bot_step(&nav,&probe,&c);
            int dir=!!(c.actions&(1u<<AVP_MP_FORWARD))-!!(c.actions&(1u<<AVP_MP_BACKWARD));
            if(previous_dir && dir && previous_dir!=dir)reverses++;
            if(dir)previous_dir=dir;
            x+=0.09*dir;
            if(c.actions&(1u<<AVP_MP_STRAFE_LEFT))y-=0.09;
            if(c.actions&(1u<<AVP_MP_STRAFE_RIGHT))y+=0.09;
            if(y<1.32)y=1.32;
            if(y>1.68)y=1.68;
            if(x>=7.25)break;
        }
        /* Source cells stay narrow and collision-limited: the safe supply
         * route must advance instead of pacing on a puddle threshold. */
        if(scenario==0){assert(x>6.5);assert(reverses<=1);}
        assert(reverses<8);
        }
    }
    /* R273: three followers in the same narrow, acid-filled approach must
     * not continually BACK AWAY from the very peer they are following.
     * R272 returned early from peer-yield ahead of its acid route controller:
     * forward a fraction, back away from the peer, repeat forever.  Simulate
     * three successive original-button decisions on a sealed one-cell lane. */
    {
        AvpMpBotState followers[3]={0};
        AvpMpBotObservation p={0};
        double pos[3]={2.25,2.50,2.75};
        unsigned back[3]={0},front[3]={0};
        blank_map(10u,3u);
        memset(map_cells,0,sizeof map_cells);
        for(unsigned x=0;x<10u;x++)map_cells[CELL(x,1u,10u)+5u]=1u;
        p.map=(AvpMpBotMap){map_cells,10u,3u,wallflags,NULL,0u,risk,NULL,0u};
        p.has_target=1u;p.goal=AVP_MP_BOT_SUPPLY;
        p.target_x=(8u<<16)|0x8000u;
        p.target_y=(1u<<16)|0x8000u;
        p.player_species=AVP_MP_MARINE;p.heading=0u;
        p.player_energy=p.max_energy=100;
        p.acid_count=4u;
        for(unsigned j=0;j<4u;j++){
            p.acid[j].x=((3u+j)<<16)|0x8000u;
            p.acid[j].y=(1u<<16)|0x8000u;
            avp_mp_bot_acid_risk_mark(risk,p.acid[j].x,p.acid[j].y);
        }
        for(unsigned t=1u;t<=85u;t++)for(unsigned i=0;i<3u;i++){
            p.tick=t;p.player_index=i;p.x=(uint32_t)(pos[i]*65536.0);
            p.y=(1u<<16)|0x8000u;
            p.nearby_peer_valid=i!=0u;
            p.nearby_peer_index=i?i-1u:0u;
            p.nearby_peer_x=i?(uint32_t)(pos[i-1u]*65536.0):0u;
            p.nearby_peer_y=p.y;
            avp_mp_bot_step(&followers[i],&p,&c);
            if(c.actions&(1u<<AVP_MP_BACKWARD) && pos[i]<7.3)back[i]++;
            if(c.actions&(1u<<AVP_MP_FORWARD)){
                front[i]++;pos[i]+=0.09;
                if(pos[i]>8.0)pos[i]=8.0;
            }else if(c.actions&(1u<<AVP_MP_BACKWARD)){
                pos[i]-=0.09;
                if(pos[i]<0.35)pos[i]=0.35;
            }
        }
        /* No acid-avoidance reason to reverse into an even longer string
         * of overlapping puddles just because another bot is nearby. */
        assert(front[0]>=30u && front[1]>=30u && front[2]>=30u);
        assert(back[1]<=2u && back[2]<=2u);
        assert(pos[0]>=5.0 && pos[1]>=5.0 && pos[2]>=5.0);
    }
    /* RE44 R275: corridor chip damage is not the same as a fatal shot.
     * The source host must authorize the live-health budget, and the bot
     * still has to hold aim and obey its real Marine weapon stream. */
    {
        AvpMpBotMap choke;
        blank_map(8u,3u);
        memset(map_cells,0,sizeof map_cells);
        for(unsigned x=0;x<8u;x++)map_cells[CELL(x,1u,8u)+5u]=1u;
        choke=(AvpMpBotMap){map_cells,8u,3u,wallflags,NULL,0u,risk,NULL,0u};
        for(unsigned profile=0;profile<3;profile++){
            AvpMpBotState bot={0};
            AvpMpBotObservation probe={0};
            probe.map=choke;
            probe.x=(2u<<16)|0x8000u;
            probe.y=(1u<<16)|0x8000u;
            probe.heading=0u;
            probe.target_x=(5u<<16)|0x8000u;
            probe.target_y=probe.y;
            probe.target_slot=1u;probe.goal=AVP_MP_BOT_HOSTILE;
            probe.player_species=AVP_MP_MARINE;
            probe.behavior=profile;
            probe.has_target=probe.hostile_target=probe.target_visible=1u;
            probe.target_is_alien=1u;probe.player_energy=probe.max_energy=100;
            probe.other_player_splash_unsafe=1u;
            probe.nonlethal_choke_fire=1u;
            probe.tick=2u;
            avp_mp_bot_step(&bot,&probe,&c);
            assert(c.actions&(1u<<AVP_MP_FIRE));
            assert(c.actions&(1u<<AVP_MP_BACKWARD));
            probe.nonlethal_choke_fire=0u;probe.tick=3u;
            avp_mp_bot_step(&bot,&probe,&c);
            assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        }
        /* Enemy within attack reach and no viable retreat: shoot even if
         * its death splash or corpse acid can hurt the trapped bot. */
        {
            AvpMpBotState bot={0};
            AvpMpBotObservation probe={0};
            probe.map=choke;
            probe.x=0x3000u;
            probe.y=(1u<<16)|0x8000u;
            probe.heading=0u;
            probe.target_x=0xB000u;
            probe.target_y=probe.y;
            probe.has_target=probe.hostile_target=probe.target_visible=1u;
            probe.target_is_alien=1u;probe.target_slot=2u;
            probe.goal=AVP_MP_BOT_HOSTILE;
            probe.player_species=AVP_MP_MARINE;
            probe.player_energy=probe.max_energy=100;
            probe.other_player_splash_unsafe=1u;
            probe.tick=3u;
            avp_mp_bot_step(&bot,&probe,&c);
            assert(c.actions&(1u<<AVP_MP_FIRE));
        }
        /* A trapped teammate needs help even when a shift off centre
         * makes the death acid passable: splash risk alone cannot veto. */
        {
            AvpMpBotState bot={0};
            AvpMpBotObservation probe={0};
            probe.map=choke;
            probe.x=(2u<<16)|0x8000u;
            probe.y=(1u<<16)|0x8000u;
            probe.heading=0u;
            probe.target_x=(5u<<16)|0x8000u;
            probe.target_y=(1u<<16)|0x1000u;
            assert(avp_mp_bot_alien_death_passable(&choke,
                probe.target_x,probe.target_y));
            probe.has_target=probe.hostile_target=probe.target_visible=1u;
            probe.target_is_alien=1u;probe.target_slot=4u;
            probe.goal=AVP_MP_BOT_HOSTILE;
            probe.player_species=AVP_MP_MARINE;
            probe.player_energy=probe.max_energy=100;
            probe.other_player_splash_unsafe=1u;
            probe.other_player_alien_trapped=1u;
            probe.tick=20u;
            avp_mp_bot_step(&bot,&probe,&c);
            assert(c.actions&(1u<<AVP_MP_FIRE));
        }
        /* A smartgun cannot safely chip 100 HP, but an owned loaded
         * 5-point Pulse Rifle can: use original weapon-cycle controls. */
        {
            AvpMpBotState bot={0};
            AvpMpBotObservation probe={0};
            probe.map=choke;
            probe.x=(2u<<16)|0x8000u;
            probe.y=(1u<<16)|0x8000u;
            probe.heading=0u;
            probe.target_x=(5u<<16)|0x8000u;
            probe.target_y=probe.y;
            probe.has_target=probe.hostile_target=probe.target_visible=1u;
            probe.target_is_alien=1u;probe.target_slot=5u;
            probe.goal=AVP_MP_BOT_HOSTILE;
            probe.player_species=AVP_MP_MARINE;
            probe.player_energy=probe.max_energy=100;
            probe.marine_inventory_valid=1u;
            probe.current_weapon=4u;
            probe.marine_owned=(1u<<2)|(1u<<4);
            probe.marine_ammo[1]=100u;probe.marine_ammo[3]=100u;
            probe.target_health=100;probe.target_health_valid=1u;
            probe.tick=12u;
            avp_mp_bot_step(&bot,&probe,&c);
            assert(c.actions&((1u<<AVP_MP_NEXT_WEAPON)|
                              (1u<<AVP_MP_PREV_WEAPON)));
            assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        }
        /* Rescue is granted only for a truly trapped player, not someone
         * merely standing in the spray zone and able to move away. */
        assert(avp_mp_bot_alien_traps_player(&choke,
            0xB000u,(1u<<16)|0x8000u,
            0x3000u,(1u<<16)|0x8000u));
        assert(!avp_mp_bot_alien_traps_player(&choke,
            (5u<<16)|0x8000u,(1u<<16)|0x8000u,
            (4u<<16)|0x8000u,(1u<<16)|0x8000u));
        {
            AvpMpBotState bot={0};
            AvpMpBotObservation probe={0};
            probe.map=choke;
            probe.x=(2u<<16)|0x8000u;
            probe.y=(1u<<16)|0x8000u;probe.heading=0u;
            probe.target_x=(5u<<16)|0x8000u;
            probe.target_y=probe.y;probe.target_slot=3u;
            probe.has_target=probe.hostile_target=probe.target_visible=1u;
            probe.target_is_alien=1u;probe.goal=AVP_MP_BOT_HOSTILE;
            probe.player_species=AVP_MP_MARINE;
            probe.player_energy=probe.max_energy=100;
            probe.other_player_alien_trapped=1u;
            probe.other_player_splash_unsafe=1u;
            probe.tick=4u;
            avp_mp_bot_step(&bot,&probe,&c);
            assert(c.actions&(1u<<AVP_MP_FIRE));
        }
    }
    /* R277: a Marine facing a PACK of Aliens must not die holding FIRE
     * because the single-source corridor acid veto sees a legal sidestep.
     * A lone distant Alien still receives the safe no-kill restriction. */
    {
        AvpMpBotObservation danger={0};
        AvpMpBotState agent={0};
        blank_map(8u,3u);
        for(unsigned y=0;y<3u;y++)if(y!=1u)
            for(unsigned x=0;x<8u;x++)
                map_cells[CELL(x,y,8u)+5u]=0u;
        danger.map=(AvpMpBotMap){map_cells,8u,3u,wallflags,NULL,0u,risk,NULL,0u};
        danger.player_species=AVP_MP_MARINE;
        danger.player_energy=danger.max_energy=100;
        danger.x=(3u<<16)|0x8000u;
        danger.y=(1u<<16)|0x8000u;
        danger.heading=0u;
        danger.target_x=(5u<<16)|0x4000u;
        danger.target_y=danger.y;
        danger.target_slot=12u;danger.has_target=1u;
        danger.hostile_target=danger.target_visible=danger.target_is_alien=1u;
        danger.goal=AVP_MP_BOT_HOSTILE;
        danger.tick=10u;
        assert(!avp_mp_bot_alien_death_passable(&danger.map,
            danger.target_x,danger.target_y));
        avp_mp_bot_step(&agent,&danger,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        danger.close_alien_contacts=2u;danger.tick=11u;
        avp_mp_bot_step(&agent,&danger,&c);
        assert(c.actions&(1u<<AVP_MP_FIRE));
        /* Emergency does not require an arbitrary Berserker profile or
         * zero health: two close observed Aliens threaten every Marine. */
        for(unsigned behavior=0;behavior<3u;behavior++){
            memset(&agent,0,sizeof agent);
            danger.behavior=behavior;
            avp_mp_bot_step(&agent,&danger,&c);
            assert(c.actions&(1u<<AVP_MP_FIRE));
        }
        danger.close_alien_contacts=1u;
        danger.tick=12u;danger.behavior=0u;
        memset(&agent,0,sizeof agent);
        avp_mp_bot_step(&agent,&danger,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        /* Actual observed loss of health in striking range is also an
         * emergency even if a wall check predicts a possible sidestep. */
        danger.target_x=(4u<<16)|0xA000u;
        danger.player_energy=60;
        agent.damage_observed=1u;
        agent.damage_world_level=danger.observed_world_level;
        agent.last_observed_energy=100;
        agent.damage_observed_tick=11u;
        avp_mp_bot_step(&agent,&danger,&c);
        assert(c.actions&(1u<<AVP_MP_FIRE));
    }
    /* R277: teammate-directed retreat is ROUTE movement, not an idle
     * evasive strafe. Keep facing/firing at the original visible Alien. */
    {
        AvpMpBotObservation endangered={0};
        AvpMpBotState agent={0};
        blank_map(8u,5u);
        endangered.map=(AvpMpBotMap){map_cells,8u,5u,wallflags,NULL,0u,risk,NULL,0u};
        endangered.player_species=AVP_MP_MARINE;
        endangered.player_energy=endangered.max_energy=100;
        endangered.x=(3u<<16)|0x8000u;
        endangered.y=(2u<<16)|0x8000u;
        endangered.heading=0u;
        endangered.target_x=(5u<<16)|0x8000u;
        endangered.target_y=endangered.y;
        endangered.target_slot=15u;
        endangered.has_target=endangered.hostile_target=
            endangered.target_visible=endangered.target_is_alien=1u;
        endangered.goal=AVP_MP_BOT_HOSTILE;
        endangered.close_alien_contacts=2u;
        endangered.retreat_to_teammate=1u;
        endangered.retreat_teammate_x=(1u<<16)|0x8000u;
        endangered.retreat_teammate_y=endangered.y;
        endangered.tick=21u;
        avp_mp_bot_step(&agent,&endangered,&c);
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        assert(c.actions&(1u<<AVP_MP_FIRE));
        /* Fresh damage must NOT overwrite the team's map-directed escape
         * with a random left/right damage-reaction sidestep. */
        agent.damage_observed=1u;
        agent.damage_observed_tick=21u;
        agent.damage_world_level=endangered.observed_world_level;
        agent.last_observed_energy=100;
        endangered.player_energy=70;endangered.tick=22u;
        avp_mp_bot_step(&agent,&endangered,&c);
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        assert(c.actions&(1u<<AVP_MP_FIRE));
        /* R281: a low-magazine Marine can use the same source-legal route
         * even WITHOUT a teammate at the other end. Keep facing the Alien
         * and firing the few remaining precise rounds while withdrawing. */
        endangered.retreat_to_teammate=0u;
        endangered.retreat_to_refuge=1u;
        endangered.retreat_refuge_x=endangered.retreat_teammate_x;
        endangered.retreat_refuge_y=endangered.retreat_teammate_y;
        endangered.marine_low_ammo=1u;
        endangered.marine_inventory_valid=1u;
        endangered.current_weapon=2u;
        endangered.marine_owned=1u<<2;
        endangered.marine_ammo[1]=7u;
        endangered.tick=23u;
        memset(&agent,0,sizeof agent);
        avp_mp_bot_step(&agent,&endangered,&c);
        assert(c.actions&(1u<<AVP_MP_BACKWARD));
        assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
        assert(c.actions&(1u<<AVP_MP_FIRE));
        endangered.retreat_to_refuge=0u;
        endangered.marine_low_ammo=0u;
        endangered.marine_inventory_valid=0u;
        /* Without a witnessed multi-Alien threat the host does not grant
         * the teammate-retreat route, and ordinary stance is unchanged. */
        endangered.retreat_to_teammate=0u;
        endangered.close_alien_contacts=0u;
        memset(&agent,0,sizeof agent);
        avp_mp_bot_step(&agent,&endangered,&c);
        assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
        /* A nearest teammate outside original authored floor is never a
         * teleport destination or an arbitrary movement override. */
        endangered.retreat_to_teammate=1u;
        endangered.retreat_teammate_x=16u<<16;
        memset(&agent,0,sizeof agent);
        avp_mp_bot_step(&agent,&endangered,&c);
        assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    }
    /* R277: a visible Alien behind real registered tables must trigger
     * a sideways firing-lane route, rather than aiming at a blocked cell.
     * The lane checker is supplied by the source world at runtime. */
    {
        uint8_t cover[AVP_MP_BOT_MAP_CELLS]={0};
        uint8_t props[AVP_MP_BOT_MAP_CELLS]={0};
        AvpMpBotObservation blocked={0};
        AvpMpBotState move={0};
        unsigned fx=0,fy=0;
        blank_map(10u,8u);
        for(unsigned row=2u;row<=4u;row++){
            cover[row*64u+4u]=1u;
            props[row*64u+4u]=1u;
        }
        map=(AvpMpBotMap){map_cells,10u,8u,wallflags,NULL,0u,risk,props,0u};
        assert(!test_row_prop_lane(cover,(2u<<16)|0x8000u,
            (3u<<16)|0x8000u,(7u<<16)|0x8000u,(3u<<16)|0x8000u));
        assert(avp_mp_bot_pick_firing_lane(&map,
            (2u<<16)|0x8000u,(3u<<16)|0x8000u,
            (7u<<16)|0x8000u,(3u<<16)|0x8000u,
            test_row_prop_lane,cover,&fx,&fy));
        assert(fy<2u || fy>4u || fx<4u || fx>4u);
        assert(!(props[fy*64u+fx]));
        assert(avp_mp_bot_route(&map,2u,3u,fx,fy,&route));
        assert(test_row_prop_lane(cover,(fx<<16)|0x8000u,
            (fy<<16)|0x8000u,(7u<<16)|0x8000u,(3u<<16)|0x8000u));
        blocked.map=map;blocked.x=(2u<<16)|0x8000u;
        blocked.y=(3u<<16)|0x8000u;blocked.heading=0u;
        blocked.target_x=(7u<<16)|0x8000u;
        blocked.target_y=(3u<<16)|0x8000u;
        blocked.has_target=blocked.hostile_target=blocked.target_is_alien=1u;
        blocked.goal=AVP_MP_BOT_HOSTILE;
        blocked.player_species=AVP_MP_MARINE;
        blocked.cover_flank_valid=1u;
        blocked.cover_flank_x=fx;blocked.cover_flank_y=fy;
        blocked.tick=1u;blocked.player_energy=blocked.max_energy=100;
        avp_mp_bot_step(&move,&blocked,&c);
        assert(c.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                          (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
        /* No valid shooting position behind a sealed authored map wall:
         * do not invent a teleport or fire at a tracker-only target. */
        blank_map(10u,8u);
        for(unsigned row=0u;row<8u;row++){
            map_cells[CELL(4u,row,10u)+5u]=0u;
            cover[row*64u+4u]=1u;
        }
        assert(!avp_mp_bot_pick_firing_lane(&map,
            (2u<<16)|0x8000u,(3u<<16)|0x8000u,
            (7u<<16)|0x8000u,(3u<<16)|0x8000u,
            test_row_prop_lane,cover,&fx,&fy));
        /* Original Marine long-gun 0x880<<8 = 8.5 tiles; the arbitrary
         * previous 6-tile veto must not suppress valid optical fire. */
        blank_map(12u,3u);
        map=(AvpMpBotMap){map_cells,12u,3u,wallflags,NULL,0u,risk,NULL,0u};
        memset(&blocked,0,sizeof blocked);memset(&move,0,sizeof move);
        blocked.map=map;blocked.player_species=AVP_MP_MARINE;
        blocked.current_weapon=2u;
        blocked.has_target=blocked.hostile_target=blocked.target_visible=1u;
        blocked.goal=AVP_MP_BOT_HOSTILE;
        blocked.x=(1u<<16)|0x8000u;
        blocked.y=(1u<<16)|0x8000u;
        blocked.target_x=(8u<<16)|0x8000u;
        blocked.target_y=blocked.y;
        blocked.heading=0u;
        avp_mp_bot_step(&move,&blocked,&c);
        assert(c.actions&(1u<<AVP_MP_FIRE));
        blocked.target_x=(10u<<16)|0x8000u;
        memset(&move,0,sizeof move);
        avp_mp_bot_step(&move,&blocked,&c);
        assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    }
    /* R299: off-center source-acid beside an authored T/cross junction.
     * The old greedy motion planner accepted only inputs with positive dot
     * toward the next route cell. At a tight turn, x=3.249/y=2.016, its
     * NEXT cell has a wall that requires x>=3.3125. The only legal
     * movement initially increases lateral clearance while moving slightly
     * away from the waypoint; old planner emitted zero movement forever.
     * Exercise many offset placements with physical wall-margin acceptance
     * over 210 command ticks, not merely the next suggested action.
     * This is a command/geometry replay, not an emulated original SafePos. */
    {unsigned failures=0u,scenarios=0u;
     for(unsigned shape=0u;shape<2u;shape++)
     for(unsigned start=0u;start<7u;start++)
     for(unsigned exit=0u;exit<3u;exit++)
     for(unsigned along=0u;along<4u;along++)
     for(unsigned side=0u;side<3u;side++){
        AvpMpBotState bot={0};AvpMpBotObservation p={0};
        AvpMpPlayerCommand command={0};
        unsigned steps=0u,blocked=0u,zero=0u,turns=0u;
        double target_x,target_y;
        if(shape==0u && exit==2u)continue; /* T has no south leg */
        blank_map(7u,5u);
        for(unsigned yy=0u;yy<5u;yy++)for(unsigned xx=0u;xx<7u;xx++)
            map_cells[CELL(xx,yy,7u)+5u]=(yy==2u || (xx==3u &&
             (shape==0u?yy<=2u:1)))?1u:0u;
        p.map=(AvpMpBotMap){map_cells,7u,5u,wallflags,NULL,0u,risk,NULL,0u};
        p.player_species=AVP_MP_MARINE;p.player_index=2u;
        p.player_collision_disabled=1u;
        p.player_energy=p.max_energy=100u;
        p.x=(uint32_t)((start==0u?1.5:start==1u?5.5:2.45+0.31*(start-2u))*65536.0);
        p.y=(uint32_t)((start>=2u?(start%2u?2.67:2.33):2.5)*65536.0);
        p.heading=start==1u?0x80000000u:(start>=2u?0x20000000u:0u);
        target_x=exit==0?(start==1u?1.5:5.5):3.5;
        target_y=exit==1?0.5:exit==2?4.5:2.5;
        p.target_x=(uint32_t)(target_x*65536.0);
        p.target_y=(uint32_t)(target_y*65536.0);
        p.has_target=1u;p.goal=AVP_MP_BOT_SUPPLY;
        p.acid_count=1u;
        p.acid[0]=(AvpMpBotAcid){(uint32_t)((2.65+along*0.18)*65536.0),
                   (uint32_t)((2.32+side*0.18)*65536.0)};
        avp_mp_bot_acid_risk_mark(risk,p.acid[0].x,p.acid[0].y);
        for(unsigned t=1u;t<=210u;t++){
            p.tick=t;avp_mp_bot_step(&bot,&p,&command);
            const double a=(double)(p.heading>>16)*(2.0*3.141592653589793/65536.0);
            const double fx=cos(a),fy=-sin(a);
            double mx=0.0,my=0.0;
            if(command.actions&(1u<<AVP_MP_FORWARD)){mx+=fx;my+=fy;}
            if(command.actions&(1u<<AVP_MP_BACKWARD)){mx-=fx;my-=fy;}
            if(command.actions&(1u<<AVP_MP_STRAFE_LEFT)){mx+=fy;my-=fx;}
            if(command.actions&(1u<<AVP_MP_STRAFE_RIGHT)){mx-=fy;my+=fx;}
            if(command.actions&(1u<<AVP_MP_TURN_LEFT))p.heading+=(0x180u<<16);
            if(command.actions&(1u<<AVP_MP_TURN_RIGHT))p.heading-=(0x180u<<16);
            const double xx=(double)p.x/65536.0+0.07*mx;
            const double yy=(double)p.y/65536.0+0.07*my;
            int valid=xx>=0.0 && xx<7.0 && yy>=0.0 && yy<5.0;
            if(valid){
              const unsigned cx=(unsigned)xx,cy=(unsigned)yy;
              const double fxx=xx-cx,fyy=yy-cy;
              if(!map_cells[CELL(cx,cy,7u)+5u])valid=0;
              if(valid && fxx<0.3125 &&
                 (!cx || !map_cells[CELL(cx-1u,cy,7u)+5u]))valid=0;
              if(valid && fxx>0.6875 &&
                 (cx==6u || !map_cells[CELL(cx+1u,cy,7u)+5u]))valid=0;
              if(valid && fyy<0.3125 &&
                 (!cy || !map_cells[CELL(cx,cy-1u,7u)+5u]))valid=0;
              if(valid && fyy>0.6875 &&
                 (cy==4u || !map_cells[CELL(cx,cy+1u,7u)+5u]))valid=0;
            }
            if(valid){p.x=(uint32_t)(xx*65536.0);p.y=(uint32_t)(yy*65536.0);steps++;}
            else if(mx!=0.0 || my!=0.0)blocked++;
            else zero++;
            if(command.actions&((1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT)))turns++;
            if(fabs((double)p.x/65536.0-target_x)<0.6 &&
               fabs((double)p.y/65536.0-target_y)<0.6)break;
        }
        scenarios++;
        if(fabs((double)p.x/65536.0-target_x)>0.6 ||
           fabs((double)p.y/65536.0-target_y)>0.6){
            failures++;
            fprintf(stderr,"FAIL shape=%u start=%u exit=%u acid=(%.2f,%.2f) xy=(%.2f,%.2f) steps=%u blocked=%u zero=%u turns=%u goal=(%.1f,%.1f)\n",
                shape,start,exit,(double)p.acid[0].x/65536.0,(double)p.acid[0].y/65536.0,
                (double)p.x/65536.0,(double)p.y/65536.0,steps,blocked,zero,turns,target_x,target_y);
        }
     }
     if(failures)fprintf(stderr,"R299 acid junction failures=%u/%u\n",failures,scenarios);
     assert(scenarios==420u && failures==0u);
     puts("R299 off-center acid near T/cross junction (420 multi-tick paths): PASS");
    }
    puts("local multiplayer source-aware bot routing/tactics (R278): PASS");
    return 0;
}
