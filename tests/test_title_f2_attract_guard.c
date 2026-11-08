/* RE32 R40: exercise the actual reconstructed title input loop with synthetic
 * Jaguar VBlank/pad events and a PC-only F2 menu visibility callback.
 * No ROM is used or needed. */
#ifdef NDEBUG
#undef NDEBUG /* Regression asserts must run in Release builds. */
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/game/mjp.c"

volatile u32 joy_cur=UINT32_MAX;
static unsigned ticks, menu_start, menu_until, fire_at, launch_at;
static int menu_always, hook_enabled;
static AvpRuntimeOps test_ops;
const AvpRuntimeOps *avp_runtime_ops(void){return &test_ops;}
void VSync(void){++ticks;assert(ticks<4000u);}
void readpad(void)
{
    joy_cur=(ticks>=fire_at)?~(1u<<FIRE_A):UINT32_MAX;
}
static int menu_open(void *user)
{
    (void)user;
    return menu_always || (menu_until && ticks>=menu_start && ticks<=menu_until);
}
static int launch_pending(void *user)
{
    (void)user;
    return launch_at && ticks>=launch_at;
}
static void scenario(unsigned start_tick,unsigned end_tick,unsigned fire_tick,
                     unsigned launch_tick,int always_open,unsigned expected_choice,
                     unsigned expected_ticks)
{
    ticks=0;menu_start=start_tick;menu_until=end_tick;fire_at=fire_tick;launch_at=launch_tick;
    menu_always=always_open;
    memset(&test_ops,0,sizeof(test_ops));
    test_ops.frontend_mp_menu_open=hook_enabled?menu_open:NULL;
    test_ops.frontend_mp_launch_pending=launch_pending;
    title_save_menu=0;title_pad_wait=0;Choice=0;joy_cur=UINT32_MAX;
    title_input_loop();
    assert((unsigned)Choice==expected_choice);
    assert(ticks==expected_ticks);
}
int main(void)
{
    hook_enabled=1;
    /* Retail title inactivity still enters attract at exactly 800 ticks. */
    scenario(0u,0u,9999u,0u,0,4u,800u);
    /* F2 left open longer than 800 ticks must never dismiss itself. */
    scenario(0u,0u,1600u,0u,1,0u,1600u);
    /* After F2 closes, start the full idle interval again. */
    scenario(1u,1000u,1500u,0u,0,0u,1500u);
    scenario(1u,1000u,9999u,0u,0,4u,1800u);
    /* Opening F2 just before the original 800-tick limit must save it. */
    scenario(790u,1000u,9999u,0u,0,4u,1800u);
    /* Start Game from F2 must still take the existing launch bridge. */
    scenario(0u,0u,9999u,50u,1,6u,50u);
    /* Missing host hook is retail behavior, even if setup is claimed open. */
    hook_enabled=0;
    scenario(0u,0u,9999u,0u,1,4u,800u);
    puts("F2 title attract guard: 7 scenarios PASS");
    return 0;
}
