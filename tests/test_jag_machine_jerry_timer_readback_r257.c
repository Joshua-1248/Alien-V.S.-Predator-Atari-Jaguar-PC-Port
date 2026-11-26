/* RE42 R257: Jerry programmable timer live counter readback.
 * Atari Jaguar Technical Reference Rev.8, Jerry pp.85-86: the two-stage
 * timer counters are readable at F10036/38 (timer 1) and F1003A/3C
 * (timer 2), while the programming registers are write-only at F10000-06.
 * Test the documented down-count with the existing reference-machine
 * whole-system-cycle timer model. No CPU/bus-cycle parity is asserted.
 */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t cart[0x4000];

static void init(void)
{
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;
    cart[0x2000]=0x60;cart[0x2001]=0xFE; /* 68000 BRA.S -2 */
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_write16(&m,0xF0002Eu,0u); /* HP=0: one system tick/half-line */
    jagm_write16(&m,0xF0003Eu,0u); /* VP=0: one half-line/field */
    jagm_write16(&m,0xF0004Eu,0x7FFu); /* no VI interrupts */
}
static void expect_counter(uint32_t addr,uint16_t want,uint64_t tick)
{
    uint16_t got=jagm_read16(&m,addr);
    if(got!=want){
        fprintf(stderr,"R257 Jerry counter %06x at tick %llu: got %04x expected %04x\n",
            (unsigned)addr,(unsigned long long)tick,got,want);
        assert(got==want);
    }
}
static void test_two_timers_and_wraps(void)
{
    init();
    expect_counter(0xF10036u,0u,0);expect_counter(0xF10038u,0u,0);
    expect_counter(0xF1003Au,0u,0);expect_counter(0xF1003Cu,0u,0);
    jagm_write16(&m,0xF10000u,2u); /* timer 1 prescaler = 3 clocks */
    jagm_write16(&m,0xF10002u,3u); /* divider = 4 prescaler pulses */
    jagm_write16(&m,0xF10004u,3u); /* timer 2 prescaler = 4 clocks */
    jagm_write16(&m,0xF10006u,1u); /* divider = 2 prescaler pulses */
    for(uint64_t tick=0;tick<=32u;tick++){
        assert(m.sys_cycles==tick);
        /* Each timer starts with programmed N and M.  The prescaler
         * reloads after N+1 system clocks, and the second stage reloads
         * after M+1 prescaler pulses. */
        expect_counter(0xF10036u,(uint16_t)(2u-(tick%3u)),tick);
        expect_counter(0xF10038u,(uint16_t)(3u-((tick/3u)%4u)),tick);
        expect_counter(0xF1003Au,(uint16_t)(3u-(tick%4u)),tick);
        expect_counter(0xF1003Cu,(uint16_t)(1u-((tick/4u)%2u)),tick);
        /* Reads themselves may never consume or reset timer phase. */
        assert(m.sys_cycles==tick);
        if(tick<32u)jagm_run_frames(&m,1u);
    }
    /* Timer IRQ generation remains part of the existing model. */
    assert((m.jint_pending&0x0Cu)==0x0Cu);
}
static void test_register_writes_reset_live_counter(void)
{
    init();
    jagm_write16(&m,0xF10000u,5u);
    jagm_write16(&m,0xF10002u,6u);
    jagm_run_frames(&m,13u);
    expect_counter(0xF10036u,4u,13u); /* 5 - 13 mod 6 */
    expect_counter(0xF10038u,4u,13u); /* 6 - floor(13/6) */
    jagm_write16(&m,0xF10000u,7u); /* existing timer model restarts on write */
    expect_counter(0xF10036u,7u,13u);
    expect_counter(0xF10038u,6u,13u);
    jagm_write16(&m,0xF10002u,2u);
    expect_counter(0xF10036u,7u,13u);
    expect_counter(0xF10038u,2u,13u);
    /* Prescaler=0 currently disables this reference timer; do not claim
     * to have resolved all hardware timer zero/control-clock semantics. */
    jagm_write16(&m,0xF10000u,0u);
    jagm_run_frames(&m,8u);
    expect_counter(0xF10036u,0u,21u);
    expect_counter(0xF10038u,2u,21u);
}
int main(void)
{
    test_two_timers_and_wraps();
    test_register_writes_reset_live_counter();
    puts("R257 Jerry programmable timer readback PASS");
    return 0;
}
