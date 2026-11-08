/* R184: source-derived initialisation primitives used by PC new campaign.
 * No ROM assets, renderer hooks or product instrumentation are involved. */
#include "sprites.h"
#include "mazescrn.h"
#include "maze.h"
#include "avp_runtime.h"
#include <assert.h>
#include <stdint.h>

/* MAZE.S owns this CPU-visible buffer pointer in the live game. */
void *build_screen;
static unsigned init_calls;
static unsigned reset_calls;
static void observe_init(void *ctx){(void)ctx;++init_calls;}
static void observe_reset(void *ctx){(void)ctx;++reset_calls;}

int main(void)
{
    AvpRuntimeOps ops={0};
    AvpSpriteRecord sprites[3]={{0}};
    ops.init_sprites=observe_init;
    ops.reset_sprites=observe_reset;
    avp_runtime_bind(&ops);
    avp_sprites_bind(sprites,3u);
    for(unsigned i=0u;i<3u;++i)
        sprites[i].expanded=(void *)(uintptr_t)(i+1u);
    InitSprites();
    assert(init_calls==1u);
    for(unsigned i=0u;i<3u;++i)assert(sprites[i].expanded==0);
    for(unsigned i=0u;i<3u;++i)
        sprites[i].expanded=(void *)(uintptr_t)(i+1u);
    ResetSprites();
    assert(reset_calls==1u);
    for(unsigned i=0u;i<3u;++i)assert(sprites[i].expanded==0);
    /* MAZE.S::ResetMaze invokes hug_init on each level transition. Its
     * recovery/throw state needs the same reset on a new PC campaign. */
    hug_recov=12;
    hug_throw=4;
    unstick_dir=0;
    hug_init();
    assert(hug_recov==0);
    assert(hug_throw==0);
    /* R272: retail hug_init never writes unstick_dir (it stays 0). */
    assert(unstick_dir==0);
    /* MAZE.S::InitGame invokes InitDblBufs on every new campaign.
     * The first buffer must be selected even after the previous campaign
     * ended on the other buffer.  This is game state, not a drawing test. */
    {
        unsigned char buffer_a=0,buffer_b=0;
        avp_mazescrn_set_screen_buffers(&buffer_a,&buffer_b,1u);
        assert(build_screen==&buffer_a);
        SwapScreens();
        assert(build_screen==&buffer_b);
        InitDblBufs();
        assert(build_screen==&buffer_a);
        SwapScreens();
        assert(build_screen==&buffer_b);
        InitDblBufs();
        assert(build_screen==&buffer_a);
    }
    avp_runtime_reset_ops();
    avp_sprites_bind(0,0);
    return 0;
}
