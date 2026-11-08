#include "local_mp_bot_awareness.h"
#include "local_mp_bot_targeting.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
static int mock_los(void *ctx,uint32_t from_x,uint32_t from_y,
                    uint32_t to_x,uint32_t to_y)
{
    const uint32_t *max_x=(const uint32_t *)ctx;
    (void)from_x;(void)from_y;(void)to_y;
    return to_x<=*max_x;
}
int main(void)
{
    AvpMpBotContact c={0};
    const uint32_t u=65536u;
    /* Original MAZE4 tracker: 3 metres per tile, 15-m limit exclusive.
       The off-axis shape is intentionally not a Euclidean circle. */
    assert(avp_bot_motion_range(10*u,10*u,10*u,14*u));
    assert(!avp_bot_motion_range(10*u,10*u,10*u,15*u));
    assert(avp_bot_motion_range(10*u,10*u,12*u,14*u));
    assert(!avp_bot_motion_range(10*u,10*u,14*u,14*u));
    assert(avp_bot_motion_range(10*u,10*u,10*u,15*u-1u));
    /* Facing +X, target in front visible; behind, side and beyond
       8 original map tiles cannot grant optical awareness. */
    assert(avp_bot_in_view(0u,10*u,10*u,13*u,10*u));
    assert(!avp_bot_in_view(0u,10*u,10*u,9*u,10*u));
    assert(!avp_bot_in_view(0u,10*u,10*u,10*u,13*u));
    assert(!avp_bot_in_view(0u,10*u,10*u,19*u,10*u));
    /* No omniscient initial contact. A sighting captures position once;
       unseen target movement is NOT copied into the contact. */
    avp_bot_contact_update(&c,3,8,10,11*u,12*u,0,0);
    assert(!c.known && !c.sight && !c.tracker);
    avp_bot_contact_update(&c,3,8,11,11*u,12*u,1,0);
    assert(c.known && c.sight && c.x==11*u);
    avp_bot_contact_update(&c,3,8,12,15*u,16*u,0,0);
    assert(c.known && !c.sight && c.x==11*u && c.y==12*u);
    avp_bot_contact_update(&c,3,8,51,20*u,20*u,0,0);
    assert(c.known && c.x==11*u); /* exactly 40 ticks of memory */
    avp_bot_contact_update(&c,3,8,52,20*u,20*u,0,0);
    assert(!c.known);
    /* Real Marine scanner can refresh behind-wall intel only in range. */
    avp_bot_contact_update(&c,3,8,53,15*u,16*u,0,1);
    assert(c.known && c.tracker && !c.sight && c.x==15*u);
    avp_bot_contact_update(&c,3,9,54,15*u,16*u,0,0);
    assert(!c.known); /* retail AMP slot reused */
    avp_bot_contact_update(&c,3,9,55,15*u,16*u,1,0);
    avp_bot_contact_update(&c,4,9,56,15*u,16*u,0,0);
    assert(!c.known); /* different retained world */
    {
        AvpMpBotTrail trail={0};
        AvpMpBotTrailPoint b={0};
        uint32_t wall=11u*u;
        AvpMpBotContact breadcrumb={0};
        /* Invisible target drops coordinates only after moving 0.25 tile;
         * world movement is not disclosed unless LOS and view cone pass. */
        avp_bot_trail_sample(&trail,3u,12u,100u,11u*u,10u*u);
        avp_bot_trail_sample(&trail,3u,12u,101u,11u*u+100u,10u*u);
        assert(trail.count==1u);
        avp_bot_trail_sample(&trail,3u,12u,102u,12u*u,10u*u);
        assert(trail.count==2u);
        assert(avp_bot_trail_visible(&trail,3u,12u,103u,0u,10u*u,
            10u*u,mock_los,&wall,&b));
        assert(b.x==11u*u && b.tick==100u); /* newer trail behind wall */
        avp_bot_contact_update(&breadcrumb,3u,12u,103u,19u*u,19u*u,0,0);
        assert(!breadcrumb.known);
        avp_bot_contact_breadcrumb(&breadcrumb,&b,103u);
        assert(breadcrumb.known && breadcrumb.x==11u*u);
        assert(!breadcrumb.sight && !breadcrumb.tracker);
        /* Once the bot reaches a stale contact square, its already-seen
         * mark must not cause another stationary investigation loop. */
        avp_bot_contact_investigated(&breadcrumb);
        assert(!breadcrumb.known);
        avp_bot_contact_breadcrumb(&breadcrumb,&b,104u);
        assert(!breadcrumb.known);
        /* Same breadcrumb must NOT become permanent tracking. */
        avp_bot_contact_update(&breadcrumb,3u,12u,141u,19u*u,19u*u,0,0);
        assert(!breadcrumb.known);
        assert(!avp_bot_trail_visible(&trail,3u,12u,141u,0u,10u*u,
            10u*u,mock_los,&wall,&b));
        wall=20u*u;
        assert(avp_bot_trail_visible(&trail,3u,12u,120u,0u,10u*u,
            10u*u,mock_los,&wall,&b));
        assert(b.x==12u*u); /* newest visible sighted breadcrumb wins */
        assert(!avp_bot_trail_visible(&trail,3u,12u,120u,0x80000000u,
            10u*u,10u*u,mock_los,&wall,&b)); /* facing away */
        assert(!avp_bot_trail_visible(&trail,3u,13u,120u,0u,
            10u*u,10u*u,mock_los,&wall,&b)); /* respawn generation */
        assert(!avp_bot_trail_visible(&trail,4u,12u,120u,0u,
            10u*u,10u*u,mock_los,&wall,&b)); /* different source level */
        avp_bot_trail_sample(&trail,3u,13u,121u,13u*u,10u*u);
        assert(trail.count==1u); /* reused target slot clears old samples */
        avp_bot_trail_clear(&trail);
        assert(trail.count==0u);
    }
    /* R8 DM dropped the native AMP target on entry to its player scan.
     * The guard must preserve a visible enemy when players are only known
     * from stale trails, and prefer an actually seen nearer player. */
    assert(!avp_mp_bot_dm_prefer_player(1,1,4,0,1,0));
    assert(!avp_mp_bot_dm_prefer_player(1,1,4,1,5,0));
    assert(avp_mp_bot_dm_prefer_player(1,1,5,1,4,0));
    assert(avp_mp_bot_dm_prefer_player(1,0,1,1,5,0));
    assert(!avp_mp_bot_dm_prefer_player(0,0,0,0,((uint64_t)6u<<16)*((uint64_t)6u<<16),1));
    /* R12 only elevated native AMP enemies inside three tiles. Its
     * scavenge goal therefore overrode perfectly visible enemies at normal
     * ranged-weapon distances. Immediate engagement must work at five
     * tiles and must not give bots supernatural awareness. */
    {
        const uint64_t u2=(uint64_t)u*(uint64_t)u;
        assert(avp_mp_bot_immediate_contact_prefer(0,1,0,25u*u2,0,0,0));
        assert(avp_mp_bot_immediate_contact_prefer(2,1,0,25u*u2,0,0,0));
        assert(avp_mp_bot_immediate_contact_prefer(1,1,0,25u*u2,0,0,0));
        assert(!avp_mp_bot_immediate_contact_prefer(0,0,0,1u*u2,0,0,0));
        assert(!avp_mp_bot_immediate_contact_prefer(0,1,0,36u*u2,0,0,0));
        assert(avp_mp_bot_immediate_contact_prefer(0,0,1,9u*u2,0,0,0));
        assert(avp_mp_bot_immediate_contact_prefer(0,1,0,25u*u2,
                                                  1,0,4u*u2));
        assert(!avp_mp_bot_immediate_contact_prefer(0,0,1,1u*u2,
                                                   1,1,25u*u2));
        assert(!avp_mp_bot_immediate_contact_prefer(0,1,0,25u*u2,
                                                   1,1,9u*u2));
    }
    puts("PASS: 15m original tracker geometry, forward optical FOV, cached last-known position, timeout and generation/level isolation");
    return 0;
}
