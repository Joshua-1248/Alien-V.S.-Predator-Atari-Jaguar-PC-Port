#include "local_mp_bot.h"
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
static uint8_t map_cells[5u*5u*8u];
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
int main(void)
{
    AvpMpBotState s={0};
    AvpMpBotObservation v={0};
    AvpMpPlayerCommand c;
    AvpMpBotRoute route;
    AvpMpBotMap map;
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
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL};
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
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,prop_risk};
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
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,prop_risk};
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
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0u,risk,NULL};
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
        map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL};
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
        map=(AvpMpBotMap){detour,7u,5u,wallflags,NULL,0,NULL,NULL};
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
    map=(AvpMpBotMap){map_cells,3u,3u,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,3u,1u,wallflags,NULL,0u,risk,NULL};
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
     map=(AvpMpBotMap){map_cells,5,1,wallflags,&meta,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL};
    risk[1u+64u]=2u;
    assert(avp_mp_bot_route(&map,0,1,4,1,&route));
    assert(route.valid && route.next_y!=1u);
    /* Alien bots must prefer the shortest walk through an acid-covered
     * corridor even when Marine/Predator bots would choose a safe detour.
     * This test uses the actual bot command planner (not only Dijkstra). */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5,1,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5u,1u,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5,1,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5u,1u,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5,3,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5u,3u,wallflags,NULL,0,risk,NULL};
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

    /* Briefly lure a living Alien away from a two-ended one-cell corridor,
     * but never wait so long that the bot stops fighting indefinitely. */
    blank_map(5u,3u);
    for(unsigned x=0u;x<5u;x++){
        map_cells[CELL(x,0u,5u)+5u]=0u;
        map_cells[CELL(x,2u,5u)+5u]=0u;
    }
    map=(AvpMpBotMap){map_cells,5u,3u,wallflags,NULL,0,risk,NULL};
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.map=map;v.x=0x8000u;v.y=(1u<<16)|0x8000u;
    v.target_x=(3u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0u;v.player_species=0u;v.player_energy=v.max_energy=100;
    v.has_target=v.hostile_target=v.target_visible=v.target_is_alien=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.tick=1u;
    avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FIRE)));
    v.tick=40u;avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    /* The same enemy inside striking distance overrides acid management. */
    v.tick=2u;v.target_x=(1u<<16)|0x8000u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FIRE));
    /* Acid has a 30<<9 XY contact square rather than a full cell. Check
     * narrow-lane strafing while preserving a necessary hazardous route. */
    blank_map(5u,3u);
    map=(AvpMpBotMap){map_cells,5u,3u,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5u,3u,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5u,1u,wallflags,NULL,0,risk,NULL};
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
    map=(AvpMpBotMap){map_cells,5u,1u,wallflags,NULL,0,risk,NULL};
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
    /* Guerilla must MOVE even with a clear target beyond the old 6-tile
     * tactical-strafe radius; keep the flank stable across several ticks. */
    memset(&s,0,sizeof s);memset(&v,0,sizeof v);
    v.x=2u<<16;v.y=2u<<16;v.target_x=12u<<16;v.target_y=v.y;
    v.heading=0u;v.behavior=AVP_MP_BOT_GUERILLA;
    v.player_species=0u;v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.tick=30u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                      (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    {unsigned flank=c.actions&((1u<<AVP_MP_STRAFE_LEFT)|
                               (1u<<AVP_MP_STRAFE_RIGHT));
     assert(flank!=0u);
     for(unsigned i=31u;i<40u;i++){
         v.tick=i;avp_mp_bot_step(&s,&v,&c);
         assert((c.actions&((1u<<AVP_MP_STRAFE_LEFT)|
                            (1u<<AVP_MP_STRAFE_RIGHT)))==flank);
     }}
    /* A blocked preferred flank switches to the other side; the side is
     * then held rather than oscillating each frame. */
    blank_map(5u,5u);
    map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL};
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
    v.map=(AvpMpBotMap){map_cells,5u,5u,wallflags,NULL,0,risk,NULL};
    v.x=(2u<<16)|0x8000u;v.y=(2u<<16)|0x8000u;
    v.target_x=(4u<<16)|0x8000u;v.target_y=v.y;
    v.heading=0u;v.behavior=AVP_MP_BOT_GUERILLA;v.player_species=2u;
    v.current_weapon=4u;
    v.has_target=v.hostile_target=v.target_visible=1u;
    v.goal=AVP_MP_BOT_HOSTILE;v.tick=30u;
    avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&(1u<<AVP_MP_FORWARD));
    assert(!(c.actions&(1u<<AVP_MP_BACKWARD)));
    /* A melee Guerilla sees a TARGET'S BACK, not a remembered heading:
     * commit forward immediately even outside the normal attack window.
     * Turn the target around / invalidate sighted facing -> flank again. */
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
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    assert(c.actions&((1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)));
    v.target_heading_valid=0u; /* tracker/stale facing must not trigger rush */
    memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
    assert(!(c.actions&(1u<<AVP_MP_FORWARD)));
    /* Melee attacks stay active while this aggressive motion is selected. */
    v.target_x=v.x+0x4000u;v.target_heading_valid=1u;v.target_heading=0u;
    memset(&s,0,sizeof s);avp_mp_bot_step(&s,&v,&c);
    assert(c.actions&((1u<<AVP_MP_ALIEN_BITE)|(1u<<AVP_MP_ALIEN_TAIL)|
                      (1u<<AVP_MP_ALIEN_CLAW)));
    puts("local multiplayer source-aware bot routing/tactics (R51): PASS");
    return 0;
}
