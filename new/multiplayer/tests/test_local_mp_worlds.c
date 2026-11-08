#include "local_mp_worlds.h"
#include "local_mp_life.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(X) do { if (!(X)) { fprintf(stderr, "FAIL %s at %d\n", #X, __LINE__); return 1; } } while (0)

typedef struct Player {
    unsigned number, species, visits, accumulated_moves, health;
    uint32_t frame[16];
} Player;
typedef struct World {
    unsigned campaign, level, joined_mask, ai_ticks;
} World;
typedef struct Mock {
    unsigned created_worlds, destroyed_worlds, created_players, destroyed_players;
    unsigned enters, exits, total_ticks, total_renders;
    int duplicate_frames;
    int reject_level, reject_enter_player, reject_render_player, reject_tick_level;
} Mock;

static int create_player(void *u, unsigned index, unsigned species, void **out)
{
    Mock *m = u; Player *p = calloc(1, sizeof(*p));
    if (!p) return 0;
    p->number = index; p->species = species; p->health = 100 + index;
    ++m->created_players; *out = p; return 1;
}
static void destroy_player(void *u, void *player)
{
    ++((Mock *)u)->destroyed_players; free(player);
}
static int create_world(void *u, unsigned campaign, unsigned level, void **out)
{
    Mock *m = u; World *w;
    if (m->reject_level == (int)level) return 0;
    w = calloc(1, sizeof(*w));
    if (!w) return 0;
    w->campaign = campaign; w->level = level;
    ++m->created_worlds; *out = w; return 1;
}
static void destroy_world(void *u, void *world)
{
    World *w = world;
    if (w->joined_mask) abort(); /* no actor left attached */
    ++((Mock *)u)->destroyed_worlds; free(w);
}
static int enter_world(void *u, void *world, unsigned player, void *player_ctx)
{
    Mock *m = u; World *w = world; Player *p = player_ctx;
    if (m->reject_enter_player == (int)player) return 0;
    if (w->joined_mask & (1u << player)) abort();
    w->joined_mask |= 1u << player;
    ++p->visits; ++m->enters; return 1;
}
static void exit_world(void *u, void *world, unsigned player, void *player_ctx)
{
    Mock *m = u; World *w = world;
    (void)player_ctx;
    if (!(w->joined_mask & (1u << player))) abort();
    w->joined_mask &= ~(1u << player);
    ++m->exits;
}
static int tick_world(void *u, void *world, unsigned member_mask,
                      void *const players[AVP_LOCAL_MP_MAX_ACTORS], const AvpMpPlayerCommand commands[AVP_LOCAL_MP_MAX_ACTORS],
                      uint64_t tick)
{
    Mock *m = u; World *w = world; unsigned p;
    if (m->reject_tick_level == (int)w->level) return 0;
    if (w->joined_mask != member_mask || !tick) abort();
    ++w->ai_ticks; ++m->total_ticks;
    for (p = 0; p < AVP_LOCAL_MP_MAX_ACTORS; ++p)
        if (member_mask & (1u << p)) {
            Player *actor = players[p];
            if (!actor) abort();
            actor->accumulated_moves += commands[p].actions;
        }
    return 1;
}
static uint32_t shared_frame[16];
static int render_player(void *u, void *world, unsigned player,
                         void *player_ctx, AvpLocalMpSourceFrame *out)
{
    Mock *m = u; World *w = world; Player *p = player_ctx; unsigned y, x;
    if (m->reject_render_player == (int)player) return 0;
    if (!(w->joined_mask & (1u << player))) abort();
    ++m->total_renders;
    for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x)
        p->frame[y * 4 + x] = y == 3 ? (0xB000u + p->health) :
                (w->level * 256u + w->ai_ticks * 8u + p->number);
    out->pixels = m->duplicate_frames ? shared_frame : p->frame;
    out->pitch = out->width = out->height = 4;
    return 1;
}

/* Simulated backend supports an atomic batch arrival. Rejecting one member
 * never changes the destination membership or either actor. */
static int enter_world_group(void *u,void *world,unsigned mask,void *const actors[AVP_LOCAL_MP_MAX_ACTORS])
{
    Mock *m=u;World *w=world;
    for(unsigned p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(mask&(1u<<p)){
        if(m->reject_enter_player==(int)p||!actors[p]||
           (w->joined_mask&(1u<<p)))return 0;
    }
    for(unsigned p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)if(mask&(1u<<p)){
        ++((Player *)actors[p])->visits;++m->enters;
        w->joined_mask|=1u<<p;
    }
    return 1;
}
static const AvpMpWorldBackend backend = {
    .create_player=create_player,.destroy_player=destroy_player,
    .create_world=create_world,.destroy_world=destroy_world,
    .enter_world=enter_world,.exit_world=exit_world,
    .tick_world=tick_world,.render_player=render_player,
    .enter_world_group=enter_world_group
};

int main(void)
{
    AvpLocalMpSetup config; AvpMpSession s = {0}, broken = {0}; Mock m = {0};
    AvpMpPlayerCommand commands[AVP_LOCAL_MP_MAX_ACTORS] = {{0}};
    uint32_t picture[10 * 8];
    AvpLocalMpOutputFrame output = {picture, 8, 8, 10};
    const AvpMpActiveWorld *w;
    unsigned p, old_visits;
    m.reject_level = m.reject_enter_player = m.reject_render_player = m.reject_tick_level = -1;
    avp_local_mp_defaults(&config);
    config.players = 4; config.campaign = AVP_MP_ALIEN;
    config.species[0] = AVP_MP_MARINE;
    config.species[1] = AVP_MP_PREDATOR;
    config.species[2] = AVP_MP_ALIEN;
    config.species[3] = AVP_MP_PREDATOR;
    config.level[0] = 7; config.level[1] = 12;
    config.level[2] = 7; config.level[3] = 9;
    CHECK(!avp_mp_session_start(&s, &config, &backend, &m)); /* disabled gate */
    config.enabled = 1;
    CHECK(avp_mp_session_start(&s, &config, &backend, &m));
    CHECK(avp_mp_session_world_count(&s) == 3);
    CHECK(m.created_worlds == 3 && m.created_players == 4);
    CHECK(avp_mp_session_player_world(&s, 0) == avp_mp_session_player_world(&s, 2));
    CHECK(avp_mp_session_player_world(&s, 0)->member_mask == 5);
    CHECK(avp_mp_session_player_world(&s, 1)->level == 12);
    commands[0].actions = 1; commands[1].actions = 2;
    commands[2].actions = 3; commands[3].actions = 4;
    CHECK(avp_mp_session_tick(&s, commands));
    CHECK(m.total_ticks == 3 && s.tick == 1);
    w = avp_mp_session_player_world(&s, 0);
    CHECK(w->ticks == 1 && ((World *)w->context)->ai_ticks == 1);
    CHECK(((Player *)s.player[2])->accumulated_moves == 3);
    CHECK(((Player *)s.player[1])->accumulated_moves == 2);
    for (p = 0; p < 80; ++p) picture[p] = 0xEEEEEEEEu;
    CHECK(avp_mp_session_compose(&s, &output, 0) == 4);
    CHECK(picture[0] == 7 * 256u + 8u); /* player 1 world 7 */
    CHECK(picture[4] == 12 * 256u + 8u + 1); /* player 2 world 12 */
    CHECK(picture[4 * 10] == 7 * 256u + 8u + 2); /* player 3 world 7 */
    CHECK(picture[4 * 10 + 4] == 9 * 256u + 8u + 3);
    CHECK(picture[3 * 10] == 0xB000u + 100u); /* P1 full original HUD row */
    CHECK(picture[3 * 10 + 4] == 0xB000u + 101u);
    CHECK(picture[7 * 10 + 4] == 0xB000u + 103u);
    CHECK(picture[8] == 0xEEEEEEEEu && picture[9] == 0xEEEEEEEEu);
    m.reject_render_player = 2;
    for (p = 0; p < 80; ++p) picture[p] = 0xAAAAAAAAu;
    CHECK(!avp_mp_session_compose(&s, &output, 0));
    for (p = 0; p < 80; ++p) CHECK(picture[p] == 0xAAAAAAAAu);
    m.reject_render_player = -1;
    m.duplicate_frames = 1;
    CHECK(!avp_mp_session_compose(&s, &output, 0));
    for (p = 0; p < 80; ++p) CHECK(picture[p] == 0xAAAAAAAAu);
    m.duplicate_frames = 0;
    /* Fail a level load; P2 must retain world 12 and persistent state. */
    m.reject_level = 88;
    CHECK(!avp_mp_session_move(&s, 1, 88));
    CHECK(avp_mp_session_player_world(&s, 1)->level == 12);
    m.reject_level = -1;
    old_visits = ((Player *)s.player[1])->visits;
    m.reject_enter_player = 1;
    CHECK(!avp_mp_session_move(&s, 1, 7));
    CHECK(avp_mp_session_player_world(&s, 1)->level == 12);
    CHECK(((Player *)s.player[1])->visits == old_visits);
    m.reject_enter_player = -1;
    CHECK(avp_mp_session_move(&s, 1, 7));
    CHECK(avp_mp_session_world_count(&s) == 2);
    CHECK(avp_mp_session_player_world(&s, 0)->member_mask == 7);
    CHECK(((Player *)s.player[1])->health == 101); /* preserved actor */
    CHECK(avp_mp_session_move(&s, 3, 7));
    CHECK(avp_mp_session_world_count(&s) == 1);
    CHECK(avp_mp_session_player_world(&s, 0)->member_mask == 15);
    CHECK(avp_mp_session_tick(&s, commands));
    CHECK(m.total_ticks == 4); /* 1 shared AI step rather than 4 */
    CHECK(((Player *)s.player[3])->accumulated_moves == 8);
    /* Lift/airlock group transfer: only occupants of one SOURCE world may
     * commit together. A rejected passenger leaves ALL members unmoved. */
    m.reject_enter_player=2;
    CHECK(!avp_mp_session_move_group(&s,5u,25u));
    CHECK(avp_mp_session_world_count(&s)==1u);
    CHECK(avp_mp_session_player_world(&s,0)->level==7u);
    CHECK(avp_mp_session_player_world(&s,2)->level==7u);
    m.reject_enter_player=-1;
    CHECK(avp_mp_session_move_group(&s,5u,25u));
    CHECK(avp_mp_session_player_world(&s,0)->level==25u);
    CHECK(avp_mp_session_player_world(&s,2)->level==25u);
    CHECK(avp_mp_session_player_world(&s,1)->level==7u);
    CHECK(avp_mp_session_player_world(&s,3)->level==7u);
    CHECK(!avp_mp_session_move_group(&s,3u,26u)); /* different sources */
    CHECK(avp_mp_session_move_group(&s,5u,7u));
    CHECK(avp_mp_session_world_count(&s)==1u);
    /* Four distinct active worlds; a solo player can travel to a fifth
     * level by replacing its now-unoccupied world slot. */
    CHECK(avp_mp_session_move(&s, 1, 21));
    CHECK(avp_mp_session_move(&s, 2, 22));
    CHECK(avp_mp_session_move(&s, 3, 23));
    CHECK(avp_mp_session_world_count(&s) == 4);
    CHECK(avp_mp_session_move(&s, 0, 24));
    CHECK(avp_mp_session_world_count(&s) == 4);
    CHECK(avp_mp_session_player_world(&s, 0)->level == 24);
    CHECK(((Player *)s.player[0])->health == 100);
    CHECK(avp_mp_session_player_world(&s, 1)->level == 21);
    /* A failed world tick never leads to accidental partial-frame replay. */
    m.reject_tick_level = 22;
    CHECK(!avp_mp_session_tick(&s, commands));
    CHECK(s.faulted && !avp_mp_session_tick(&s, commands));
    CHECK(!avp_mp_session_move(&s, 0, 7));
    CHECK(!avp_mp_session_compose(&s, &output, 0));
    avp_mp_session_stop(&s);
    CHECK(m.created_worlds == m.destroyed_worlds);
    CHECK(m.created_players == m.destroyed_players);
    CHECK(!s.running && avp_mp_session_world_count(&s) == 0);
    /* S82 regression: P2 travels from Sub-Level 1 into an elevator already
     * occupied by P1 on Sub-Level 2. Both (plus P3) then take that lift
     * together to Sub-Level 3. No source-world pointer may be reused while
     * occupied, and the world containing P4 must remain untouched. */
    {
        AvpMpSession lift={0}; AvpLocalMpSetup lc;
        avp_local_mp_defaults(&lc);
        lc.enabled=1;lc.players=4;lc.campaign=AVP_MP_MARINE;
        lc.level[0]=2u;lc.level[1]=1u;lc.level[2]=2u;lc.level[3]=4u;
        CHECK(avp_mp_session_start(&lift,&lc,&backend,&m));
        CHECK(avp_mp_session_world_count(&lift)==3u);
        CHECK(avp_mp_session_move(&lift,1u,2u));
        CHECK(avp_mp_session_player_world(&lift,0u)->member_mask==7u);
        CHECK(avp_mp_session_move_group(&lift,7u,3u));
        CHECK(avp_mp_session_player_world(&lift,0u)->member_mask==7u);
        CHECK(avp_mp_session_player_world(&lift,1u)->level==3u);
        CHECK(avp_mp_session_player_world(&lift,2u)->level==3u);
        CHECK(avp_mp_session_player_world(&lift,3u)->level==4u);
        CHECK(avp_mp_session_world_count(&lift)==2u);
        CHECK(avp_mp_session_tick(&lift,commands));
        CHECK(!lift.faulted);
        /* A blocked group move preserves all three occupants for retry. */
        m.reject_enter_player=1;
        CHECK(!avp_mp_session_move_group(&lift,7u,2u));
        CHECK(avp_mp_session_player_world(&lift,0u)->level==3u);
        CHECK(avp_mp_session_player_world(&lift,1u)->level==3u);
        m.reject_enter_player=-1;
        CHECK(avp_mp_session_move_group(&lift,7u,2u));
        CHECK(avp_mp_session_tick(&lift,commands));
        CHECK(!lift.faulted);
        avp_mp_session_stop(&lift);
        CHECK(m.created_worlds==m.destroyed_worlds);
    }
    /* S89: destination Sub-Level 3 is ALREADY occupied by P4. P2
     * joins P1/P3 in the Level-2 elevator, all three arrive in P4's
     * existing world, then a second lift can carry all four to Level 4.
     * The destination world must be merged, not replaced or invalidated. */
    {
        AvpMpSession lift={0}; AvpLocalMpSetup lc;
        avp_local_mp_defaults(&lc);
        lc.enabled=1;lc.players=4;lc.campaign=AVP_MP_MARINE;
        lc.level[0]=2u;lc.level[1]=1u;lc.level[2]=2u;lc.level[3]=3u;
        CHECK(avp_mp_session_start(&lift,&lc,&backend,&m));
        CHECK(avp_mp_session_world_count(&lift)==3u);
        CHECK(avp_mp_session_move(&lift,1u,2u));
        CHECK(avp_mp_session_player_world(&lift,0u)->member_mask==7u);
        CHECK(avp_mp_session_move_group(&lift,7u,3u));
        CHECK(avp_mp_session_world_count(&lift)==1u);
        CHECK(avp_mp_session_player_world(&lift,3u)->member_mask==15u);
        CHECK(avp_mp_session_tick(&lift,commands));
        CHECK(!lift.faulted);
        /* A subsequent shared lift must preserve all actor memberships. */
        CHECK(avp_mp_session_move_group(&lift,15u,4u));
        CHECK(avp_mp_session_world_count(&lift)==1u);
        CHECK(avp_mp_session_player_world(&lift,0u)->member_mask==15u);
        CHECK(avp_mp_session_player_world(&lift,3u)->level==4u);
        CHECK(avp_mp_session_tick(&lift,commands));
        CHECK(!lift.faulted);
        avp_mp_session_stop(&lift);
        CHECK(m.created_worlds==m.destroyed_worlds);
    }
    /* RE27: P1 death in a 3P or 4P session must affect P1 alone.  Model
     * the exact native coordinator and per-view fade/transition sequencing;
     * this is NOT a replacement for a live ROM-backed combat reproduction. */
    for(unsigned count=3u;count<=4u;count++){
        AvpMpSession death={0};
        AvpMpLife life[4]={{0}};
        AvpLocalMpSetup dc;
        Player *first,*other;
        avp_local_mp_defaults(&dc);
        dc.enabled=1;dc.players=count;dc.campaign=AVP_MP_PREDATOR;
        for(unsigned j=0;j<count;j++){
            dc.species[j]=AVP_MP_PREDATOR;
            dc.level[j]=9u;
        }
        CHECK(avp_mp_session_start(&death,&dc,&backend,&m));
        first=(Player *)death.player[0];
        other=(Player *)death.player[1];
        first->health=0u; /* source player one is dead, others remain alive */
        CHECK(other->health==101u);
        avp_mp_life_begin(&life[0]);
        CHECK(avp_mp_life_busy(&life[0]));
        for(unsigned step=0;step<63u;step++)
            CHECK(!avp_mp_life_display_step(&life[0]));
        CHECK(avp_mp_life_display_step(&life[0]));
        CHECK(avp_mp_session_move(&death,0u,15u)); /* original Predator ship */
        CHECK(avp_mp_session_player_world(&death,0u)->level==15u);
        CHECK(avp_mp_session_player_world(&death,1u)->level==9u);
        CHECK(avp_mp_session_world_count(&death)==2u);
        first->health=100u;
        avp_mp_life_resumed(&life[0]);
        CHECK(avp_mp_life_busy(&life[0]));
        CHECK(avp_mp_session_tick(&death,commands));
        CHECK(avp_mp_session_compose(&death,&output,0)==count);
        CHECK(other->health==101u);
        for(unsigned j=1;j<count;j++){
            Player *remaining=(Player *)death.player[j];
            CHECK(remaining->health==100u+j);
            CHECK(avp_mp_session_player_world(&death,j)->level==9u);
            CHECK(!avp_mp_life_busy(&life[j]));
        }
        for(unsigned step=0;step<64u;step++)
            CHECK(!avp_mp_life_display_step(&life[0]));
        CHECK(!avp_mp_life_busy(&life[0]));
        avp_mp_session_stop(&death);
        CHECK(m.created_worlds==m.destroyed_worlds);
        CHECK(m.created_players==m.destroyed_players);
    }
    /* R17: four human viewports plus four *separate* native bot actors.
     * The world backend must simulate all eight without rendering P5-P8. */
    {
        AvpMpSession eight={0};AvpLocalMpSetup ec;
        unsigned renders_before=m.total_renders;
        avp_local_mp_defaults(&ec);
        ec.enabled=1;ec.players=4;ec.bot_count=4;
        for(unsigned j=0;j<4;j++){
            ec.level[j]=3+j;
            ec.bot_species[j]=j%3;
            ec.level[j+4]=7+j;
        }
        m.reject_level=m.reject_enter_player=m.reject_render_player=
            m.reject_tick_level=-1;
        CHECK(avp_mp_session_start(&eight,&ec,&backend,&m));
        CHECK(avp_local_mp_total_actors(&eight.setup)==8);
        CHECK(avp_mp_session_world_count(&eight)==8);
        for(unsigned j=0;j<8;j++){
            CHECK(eight.player[j]!=NULL);
            CHECK(avp_mp_session_player_world(&eight,j)->level==3+j);
        }
        memset(commands,0,sizeof(commands));
        commands[7].actions=13;
        CHECK(avp_mp_session_tick(&eight,commands));
        CHECK(((Player *)eight.player[7])->accumulated_moves==13);
        CHECK(avp_mp_session_compose(&eight,&output,0)==4);
        CHECK(m.total_renders-renders_before==4);
        /* Bot P8 can change worlds without altering human world ownership. */
        CHECK(avp_mp_session_move(&eight,7,3));
        CHECK(avp_mp_session_player_world(&eight,7)->level==3);
        CHECK(avp_mp_session_player_world(&eight,0)->member_mask==(1u|128u));
        /* Eight actors in ONE world must share exactly one AMP/door tick,
         * then travel as one group without creating extra viewports. */
        for(unsigned j=1;j<7;j++)CHECK(avp_mp_session_move(&eight,j,3));
        CHECK(avp_mp_session_world_count(&eight)==1);
        CHECK(avp_mp_session_player_world(&eight,0)->member_mask==255u);
        unsigned tick_before=m.total_ticks;
        CHECK(avp_mp_session_tick(&eight,commands));
        CHECK(m.total_ticks==tick_before+1u);
        CHECK(avp_mp_session_move_group(&eight,255u,5u));
        CHECK(avp_mp_session_player_world(&eight,7)->level==5u);
        CHECK(avp_mp_session_player_world(&eight,0)->member_mask==255u);
        CHECK(avp_mp_session_compose(&eight,&output,0)==4u);
        avp_mp_session_stop(&eight);
        CHECK(m.created_worlds==m.destroyed_worlds);
        CHECK(m.created_players==m.destroyed_players);
    }
    /* R19: four human viewports plus TWELVE separately simulated bots.
     * All 16 must receive real actor ticks, score/transfer masks must
     * retain bits 8..15, and rendering remains P1-P4 only. */
    {
        AvpMpSession sixteen={0};AvpLocalMpSetup ec;
        unsigned renders_before=m.total_renders;
        avp_local_mp_defaults(&ec);
        ec.enabled=1u;ec.players=4u;ec.bot_count=12u;
        for(unsigned j=0;j<16u;j++){
            ec.level[j]=1u+(j%15u);
            if(j>=4u){
                ec.bot_species[j-4u]=j%3u;
                ec.bot_behavior[j-4u]=j%3u;
            }
        }
        m.reject_level=m.reject_enter_player=m.reject_render_player=
            m.reject_tick_level=-1;
        CHECK(avp_mp_session_start(&sixteen,&ec,&backend,&m));
        CHECK(avp_local_mp_total_actors(&sixteen.setup)==16u);
        CHECK(avp_mp_session_world_count(&sixteen)==15u);
        for(unsigned j=0;j<16u;j++){
            CHECK(sixteen.player[j]!=NULL);
            CHECK(avp_mp_session_player_world(&sixteen,j)->level==ec.level[j]);
            commands[j].actions=1u+j;
        }
        CHECK(avp_mp_session_tick(&sixteen,commands));
        for(unsigned j=0;j<16u;j++)
            CHECK(((Player *)sixteen.player[j])->accumulated_moves==1u+j);
        CHECK(avp_mp_session_compose(&sixteen,&output,0)==4u);
        CHECK(m.total_renders-renders_before==4u);
        /* A high-index bot's bit must survive both normal and batch joins. */
        CHECK(avp_mp_session_move(&sixteen,15u,7u));
        CHECK((avp_mp_session_player_world(&sixteen,15u)->member_mask&(1u<<15))!=0u);
        for(unsigned j=0;j<16u;j++)if(j!=15u)CHECK(avp_mp_session_move(&sixteen,j,7u));
        CHECK(avp_mp_session_world_count(&sixteen)==1u);
        CHECK(avp_mp_session_player_world(&sixteen,15u)->member_mask==65535u);
        unsigned tick_before=m.total_ticks;
        CHECK(avp_mp_session_tick(&sixteen,commands));
        CHECK(m.total_ticks==tick_before+1u);
        CHECK(avp_mp_session_move_group(&sixteen,65535u,8u));
        CHECK(avp_mp_session_player_world(&sixteen,0u)->member_mask==65535u);
        CHECK(avp_mp_session_player_world(&sixteen,15u)->level==8u);
        CHECK(avp_mp_session_compose(&sixteen,&output,0)==4u);
        avp_mp_session_stop(&sixteen);
        CHECK(m.created_worlds==m.destroyed_worlds);
        CHECK(m.created_players==m.destroyed_players);
    }
    /* Ensure partial startup destroys only successfully created contexts. */
    m.reject_level = 12; m.reject_tick_level = -1;
    CHECK(!avp_mp_session_start(&broken, &config, &backend, &m));
    CHECK(m.created_worlds == m.destroyed_worlds);
    CHECK(m.created_players == m.destroyed_players);
    CHECK(!broken.running);
    puts("PASS: up to 16 native actor contexts with 4 human views, independent worlds, shared-level AI ticks, transaction-safe transitions, full HUD frames, rollback and fail-stop");
    return 0;
}
