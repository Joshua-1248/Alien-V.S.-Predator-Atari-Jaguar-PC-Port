#include "local_mp_deathmatch.h"
#include "local_mp_amp_native.h"
#include "doors.h"
#include "player.h"
#include <stdlib.h>
#include <string.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
int main(void){
    AvpLocalMpSetup setup;
    AvpMpDeathmatch m;
    uint32_t speed=11900u,depth=0;
    int16_t shift=4;
    avp_local_mp_defaults(&setup);
    avp_mp_dm_init(&m,&setup);
    assert(!m.enabled);
    setup.game_mode=AVP_MP_DEATHMATCH;
    setup.players=3;
    setup.dm_score_limit=0;
    setup.dm_time_limit=0;
    setup.dm_fast_movement=1;
    avp_mp_dm_init(&m,&setup);
    assert(m.enabled&&m.fast_movement);
    for(unsigned i=0;i<36000u;i++)avp_mp_dm_tick(&m);
    assert(!m.finished&&m.elapsed_ticks==36000u);
    avp_mp_dm_kill(&m,1,2);assert(m.kills[1]==1u &&m.deaths[2]==1u&&!m.finished);
    avp_mp_dm_speed(1,&speed,&shift);
    assert(speed==23800u&&shift==3);
    avp_mp_dm_speed(0,&speed,&shift);assert(speed==23800u&&shift==3);
    setup.dm_score_limit=5;setup.dm_time_limit=0;
    avp_mp_dm_init(&m,&setup);
    for(unsigned i=0;i<5u;i++)avp_mp_dm_kill(&m,2,0);
    assert(m.finished&&m.winner==2&&m.kills[2]==5);
    setup.dm_score_limit=0;setup.dm_time_limit=5;
    avp_mp_dm_init(&m,&setup);
    assert(avp_mp_dm_seconds_remaining(&m)==300);
    for(unsigned i=0;i<3000u;i++)avp_mp_dm_tick(&m);
    assert(m.finished&&m.winner==AVP_LOCAL_MP_MAX_ACTORS&&avp_mp_dm_seconds_remaining(&m)==0);
    /* R17: independent bots are real scoring participants, not human slots. */
    setup.players=4u;setup.bot_count=4u;
    setup.dm_time_limit=0u;setup.dm_score_limit=3u;
    avp_mp_dm_init(&m,&setup);
    assert(m.players==8u);
    for(unsigned i=0;i<3u;i++)avp_mp_dm_kill(&m,7u,0u);
    assert(m.finished&&m.winner==7u&&m.kills[7u]==3u&&m.deaths[0u]==3u);
    setup.players=4u;setup.bot_count=12u;
    avp_mp_dm_init(&m,&setup);
    assert(m.players==16u);
    for(unsigned i=0;i<3u;i++)avp_mp_dm_kill(&m,15u,3u);
    assert(m.finished&&m.winner==15u&&m.kills[15u]==3u&&m.deaths[3u]==3u);
    assert(avp_mp_dm_shot_candidate(65536,0,140000,140000,128,0,64,UINT32_MAX,&depth));
    assert(!avp_mp_dm_shot_candidate(-65536,0,140000,140000,128,0,64,UINT32_MAX,&depth));
    /* PvP-only damage table. Values below are the exact limits the user
     * selected; native damage/charge stays unchanged for hostile AMPs. */
    assert(avp_mp_dm_direct_damage(0,1,10)==200); /* Marine shotgun: 5 full hits vs Marine */
    assert(avp_mp_dm_direct_damage(0,2,5)==50);  /* Pulse rifle */
    assert(avp_mp_dm_direct_damage(0,4,60)==120);/* Smartgun */
    assert(avp_mp_dm_projectile_damage(8)==125); /* Flamethrower */
    assert(avp_mp_dm_direct_damage(4,1,10)==40);/* Alien tail minimum */
    assert(avp_mp_dm_direct_damage(4,1,20)==80);/* Alien tail maximum */
    assert(avp_mp_dm_direct_damage(4,2,15)==45);/* Claw minimum */
    assert(avp_mp_dm_direct_damage(4,2,30)==90);/* Claw maximum */
    assert(avp_mp_dm_direct_damage(4,3,50)==100);/* Bite minimum */
    assert(avp_mp_dm_direct_damage(4,3,100)==200);/* Bite maximum */
    assert(avp_mp_dm_direct_damage(8,4,15)==45);/* Wrist blades minimum */
    assert(avp_mp_dm_direct_damage(8,4,30)==90);/* Wrist blades maximum */
    assert(avp_mp_dm_direct_damage(8,1,50)==100);/* Combi-stick minimum */
    assert(avp_mp_dm_direct_damage(8,1,100)==200);/* Combi-stick maximum */
    assert(avp_mp_dm_projectile_damage(6)==100);/* Smart disc */
    assert(avp_mp_dm_projectile_damage(7)==100);/* Cannon direct */
    assert(avp_mp_dm_projectile_damage(1)==0);/* Not a projectile */
    assert(avp_mp_dm_direct_damage(0,3,0)==0); /* No synth flame hit */
    assert(avp_mp_dm_direct_damage(0,1,0)==0); /* Misses never hit */
    /* Original blood sprite type is selected by the VICTIM, never the
     * shooter's species. No puff for melee or flame projectiles. */
    assert(avp_mp_dm_bullet_impact_creature(0,1,4)==0); /* Alien ABLOOD */
    assert(avp_mp_dm_bullet_impact_creature(0,2,8)==1); /* Predator PBLOOD */
    assert(avp_mp_dm_bullet_impact_creature(0,4,0)==2); /* Marine MBLOOD */
    assert(avp_mp_dm_bullet_impact_creature(0,3,4)==-1); /* Flame */
    assert(avp_mp_dm_bullet_impact_creature(4,2,0)==-1); /* Alien claw */
    assert(avp_mp_dm_bullet_impact_creature(8,1,4)==-1); /* Predator stick */
    assert(avp_mp_dm_bullet_impact_creature(0,1,99)==-1);
    /* Original rough-distance blast, including diagonal weighting. */
    assert(avp_mp_dm_explosion_candidate(65536,0,65536));
    assert(!avp_mp_dm_explosion_candidate(65537,0,65536));
    assert(avp_mp_dm_explosion_candidate(49152,49152,65536));
    assert(!avp_mp_dm_explosion_candidate(65536,65536,65536));
    /* Deathmatch-only F2 options survive the native rules bridge. */
    setup.dm_level=15u; setup.dm_enemies=1u; setup.dm_fast_equip=1u;
    avp_mp_dm_init(&m,&setup);
    assert(m.level==15u && m.enemies==1u && m.fast_equip==1u);
    assert(!m.level_travel);
    setup.dm_level_travel=1u;
    avp_mp_dm_init(&m,&setup);
    assert(m.level_travel==1u);
    assert(!m.cloak_damage); /* explicit off by default */
    setup.dm_cloak_damage=1u;
    avp_mp_dm_init(&m,&setup);
    assert(m.cloak_damage==1u);
    assert(avp_mp_dm_victim_damage(0u,PT_PREDATOR,1u,100)==100);
    assert(avp_mp_dm_victim_damage(1u,PT_PREDATOR,0u,100)==100);
    assert(avp_mp_dm_victim_damage(1u,PT_HUMAN,1u,100)==100);
    assert(avp_mp_dm_victim_damage(1u,PT_PREDATOR,1u,100)==400);
    assert(avp_mp_dm_victim_damage(1u,PT_PREDATOR,1u,0)==0);
    setup.game_mode=AVP_MP_COOP;
    avp_mp_dm_init(&m,&setup);
    assert(!m.enabled && !m.fast_equip && !m.enemies);
    /* Valid spawn needs actual floor and an enclosed edge, in addition
     * to the separate native SafePos/connected-component checks. */
    {
        uint8_t maze[5u*5u*8u]={0};
        uint8_t walls[256]={0};
        for(unsigned y=1u;y<4u;y++)for(unsigned x=1u;x<4u;x++)
            maze[(y*5u+x)*8u+5u]=1u;
        assert(avp_mp_dm_spawn_interior(maze,5u,5u,walls,2u,2u));
        assert(!avp_mp_dm_spawn_interior(maze,5u,5u,walls,0u,2u));
        assert(!avp_mp_dm_spawn_interior(maze,5u,5u,walls,2u,0u));
        maze[(2u*5u+3u)*8u+5u]=0u;
        assert(!avp_mp_dm_spawn_interior(maze,5u,5u,walls,2u,2u));
        maze[(2u*5u+2u)*8u+2u]=2u;
        walls[2u]=AVP_WB_2SIDES;
        assert(!avp_mp_dm_spawn_interior(maze,5u,5u,walls,2u,2u));
        walls[2u]=AVP_WB_DOOR;
        assert(!avp_mp_dm_spawn_interior(maze,5u,5u,walls,2u,2u));
        walls[2u]=AVP_WB_SOLID;
        assert(avp_mp_dm_spawn_interior(maze,5u,5u,walls,2u,2u));
        maze[(2u*5u+2u)*8u+5u]=0u;
        assert(!avp_mp_dm_spawn_interior(maze,5u,5u,walls,2u,2u));
    }
    /* The playable region is anchored at the authored level start, not at
     * the dying player's last (possibly isolated/outside) location. */
    {
        uint16_t components[4096]={0};
        components[2u*64u+2u]=1u;  /* original level start */
        components[2u*64u+3u]=1u;  /* playable match room */
        components[8u*64u+8u]=2u;  /* detached textured island */
        assert(avp_mp_dm_spawn_origin_region(components,10u,10u,2u,2u,3u,2u));
        assert(!avp_mp_dm_spawn_origin_region(components,10u,10u,2u,2u,8u,8u));
        /* A respawn on the detached island cannot authorize more spawns
         * there merely by replacing the canonical level start. */
        assert(!avp_mp_dm_spawn_origin_region(components,10u,10u,2u,2u,9u,9u));
        assert(!avp_mp_dm_spawn_origin_region(components,10u,10u,0u,0u,3u,2u));
        assert(!avp_mp_dm_spawn_origin_region(components,10u,10u,2u,2u,10u,2u));
        assert(!avp_mp_dm_spawn_origin_region(components,65u,10u,2u,2u,3u,2u));
    }
    /* Only active hostile NPCs disappear. Keep pickups/projectiles and
     * don't invalidate another AMP that owns the same collision cell. */
    {
        AvpMpAmpNativeState *w=calloc(1,sizeof(*w));
        assert(w);
        w->actors[0].creature=AC_ALIEN;
        w->actors[0].flags=1u<<AMP_KILLABLE;
        w->actors[0].energy=30;
        w->actors[0].mode=(void *)1;
        w->actors[0].xpos=(4<<16)|0x8000;
        w->actors[0].ypos=(4<<16)|0x8000;
        w->collmap[4u*AVP_AMP_GRID_W+4u]=1u;
        w->objmap[4u*AVP_AMP_OBJ_ROW_BYTES+4u*2u+1u]=1u;
        w->actors[1].creature=AC_BOLT;
        w->actors[1].flags=1u<<AMP_KILLABLE;
        w->actors[1].energy=20;
        w->actors[1].mode=(void *)1;
        w->actors[2].creature=AC_HUMAN;
        w->actors[2].energy=50;
        w->actors[2].mode=(void *)1;
        w->actors[2].flags=0u; /* Nonhostile authored decoration. */
        assert(avp_mp_dm_remove_enemies(w)==1u);
        assert(w->actors[0].mode==NULL && w->actors[0].energy==0);
        assert(w->collmap[4u*AVP_AMP_GRID_W+4u]==0u);
        assert(w->objmap[4u*AVP_AMP_OBJ_ROW_BYTES+4u*2u+1u]==0u);
        assert(w->actors[1].mode && w->actors[2].mode);
        assert(w->free_count==1u && w->free_list[0]==0u);
        assert(avp_mp_dm_remove_enemies(w)==0u && w->free_count==1u);
        free(w);
    }
    puts("deathmatch rules, player damage, authored-origin spawn, enemy suppression and fast-equip flags: PASS");return 0;
}
