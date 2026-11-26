/* RE41 R226: TOM receives Jerry CPU interrupts on a rising edge.
 * TOM INT1 acknowledgment must not mutate Jerry JINTCTRL source latches.
 * Based on Atari Jaguar TRM Rev.8 TOM INT1 and Jerry JINTCTRL tables.
 * Synthetic cartridge; bus/interrupt latency is not claimed cycle-exact. */
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
    cart[0x404u]=0u;cart[0x405u]=0x80u;cart[0x406u]=0x20u;
    cart[0x2000u]=0x60u;cart[0x2001u]=0xFEu; /* BRA.S -2 */
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_write16(&m,0xF0002Eu,31u); /* short half-lines */
    jagm_write16(&m,0xF0003Eu,31u); /* short field */
    jagm_write16(&m,0xF0004Eu,0x7FFu); /* no video interrupt */
    jagm_write16(&m,0xF000E0u,0x0010u); /* enable Jerry at TOM */
}
static void timer_on(unsigned timer)
{
    const uint32_t base=0xF10000u+4u*timer;
    jagm_write16(&m,base,1u); /* (1+1)*(1+1) system ticks */
    jagm_write16(&m,base+2u,1u);
}
static void test_tom_ack_keeps_jerry_source(void)
{
    init();
    jagm_write16(&m,0xF10020u,0x0004u); /* Jerry timer 1 enabled */
    timer_on(0u);
    jagm_run_frames(&m,1u);
    assert((jagm_read16(&m,0xF10020u)&0x04u)!=0u);
    assert((jagm_read16(&m,0xF000E0u)&0x10u)!=0u);
    jagm_write16(&m,0xF000E0u,0x1010u); /* TOM Jerry acknowledge ONLY */
    if((jagm_read16(&m,0xF10020u)&0x04u)==0u){
        fprintf(stderr,"R225 defect: TOM acknowledgment erased Jerry timer 1 latch\n");
        assert(0);
    }
    assert((jagm_read16(&m,0xF000E0u)&0x10u)==0u);
    jagm_run_frames(&m,1u); /* still high: no new edge */
    assert((jagm_read16(&m,0xF000E0u)&0x10u)==0u);
    jagm_write16(&m,0xF10020u,0x0404u); /* now ACK JERRY timer 1 */
    assert((jagm_read16(&m,0xF10020u)&0x04u)==0u);
    jagm_run_frames(&m,1u); /* new timer event: low -> high */
    assert((jagm_read16(&m,0xF10020u)&0x04u)!=0u);
    assert((jagm_read16(&m,0xF000E0u)&0x10u)!=0u);
}
static void test_multiple_latches_require_full_deassertion(void)
{
    init();
    jagm_write16(&m,0xF10020u,0x000Cu); /* both timers enabled */
    timer_on(0u);timer_on(1u);
    jagm_run_frames(&m,1u);
    assert((jagm_read16(&m,0xF10020u)&0x0Cu)==0x0Cu);
    assert((jagm_read16(&m,0xF000E0u)&0x10u)!=0u);
    jagm_write16(&m,0xF000E0u,0x1010u);
    assert((jagm_read16(&m,0xF10020u)&0x0Cu)==0x0Cu);
    jagm_write16(&m,0xF10020u,0x040Cu); /* clear first source only */
    assert((jagm_read16(&m,0xF10020u)&0x0Cu)==0x08u);
    assert((jagm_read16(&m,0xF000E0u)&0x10u)==0u);
    jagm_write16(&m,0xF10020u,0x080Cu); /* now clear other source */
    assert((jagm_read16(&m,0xF10020u)&0x0Cu)==0u);
    jagm_run_frames(&m,1u); /* a fresh timer edge may relatch TOM */
    assert((jagm_read16(&m,0xF000E0u)&0x10u)!=0u);
}
static void test_no_interrupt_from_stale_high_on_enable(void)
{
    init();
    jagm_write16(&m,0xF000E0u,0u); /* TOM Jerry disabled */
    jagm_write16(&m,0xF10020u,0x0004u);
    timer_on(0u);
    jagm_run_frames(&m,1u);
    assert((jagm_read16(&m,0xF10020u)&4u)!=0u);
    assert((jagm_read16(&m,0xF000E0u)&16u)==0u);
    jagm_write16(&m,0xF000E0u,16u); /* enable while input already high */
    assert((jagm_read16(&m,0xF000E0u)&16u)==0u);
    jagm_write16(&m,0xF10020u,0x0404u); /* deassert */
    jagm_run_frames(&m,1u);
    assert((jagm_read16(&m,0xF000E0u)&16u)!=0u);
}
int main(void)
{
    test_tom_ack_keeps_jerry_source();
    test_multiple_latches_require_full_deassertion();
    test_no_interrupt_from_stale_high_on_enable();
    puts("R226 TOM/Jerry edge-triggered source-preserving IRQ PASS");
    return 0;
}
