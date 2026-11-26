/* RE43 R262: the synchronous serial interrupt can reach both the DSP and
 * JERRY JINTCTRL source 5, with TOM's source 4 remaining edge-triggered.
 * Synthetic cartridge; bus latency and physical pin phase not assumed. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static JagMachine m;
static uint8_t cart[0x4000];
static void init(void)
{
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;
    cart[0x2000]=0x60;cart[0x2001]=0xFE;
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_write16(&m,0xF0002Eu,31u); /* 32 SYS clocks per half line */
    jagm_write16(&m,0xF0003Eu,0u);  /* one half line per field */
    jagm_write16(&m,0xF0004Eu,0x7FFu); /* no video IRQ */
    jagm_write32(&m,0xF1A150u,0u); /* 2 clocks/bit, 32/word */
    jagm_write16(&m,0xF000E0u,0x0010u); /* TOM JERRY interrupt enabled */
    jagm_write16(&m,0xF10020u,0x0020u); /* JERRY SYN interrupt enabled */
}
static void step(void){jagm_run_frames(&m,1u);}
static unsigned jerry(void){return jagm_read16(&m,0xF10020u)&0x20u;}
static unsigned tom(void){return jagm_read16(&m,0xF000E0u)&0x10u;}
static unsigned dsp(void){return (m.dsp.latches>>1)&1u;}
static void ack_tom(void){jagm_write16(&m,0xF000E0u,0x1010u);}
static void ack_jerry(void){jagm_write16(&m,0xF10020u,0x2020u);}

static void test_rising_falling_irq_reaches_cpu(void)
{
    init();
    jagm_write32(&m,0xF1A154u,0x000Du); /* INTERNAL WSEN RISING */
    assert(jerry()==0u && tom()==0u);
    step();
    assert(dsp()==1u);
    assert(jerry()==0x20u); /* JINTCTRL SYN source must latch */
    assert(tom()==0x10u);   /* TOM interrupt sees JERRY rising edge */
    ack_tom();
    assert(tom()==0u && jerry()==0x20u); /* distinct ACK domains */
    m.dsp.latches&=~(1u<<1);
    step(); /* falling edge without FALLING selected */
    assert(!dsp() && jerry()==0x20u && tom()==0u);
    ack_jerry();
    assert(!jerry() && !tom());
    jagm_write32(&m,0xF1A154u,0x0015u); /* INTERNAL WSEN FALLING */
    step(); /* rising is disabled */
    assert(!jerry() && !tom() && !dsp());
    step(); /* falling is enabled */
    assert(dsp() && jerry()==0x20u && tom()==0x10u);
}
static void test_everyword_without_strobe_and_no_spurious_irq(void)
{
    init();
    jagm_write32(&m,0xF1A154u,0x0005u); /* clock/WS, no IRQ */
    step();step();
    assert(!jerry() && !tom() && !dsp());
    jagm_write32(&m,0xF1A154u,0x0021u); /* INTERNAL EVERYWORD, no WSEN */
    step();
    assert(dsp() && jerry()==0x20u && tom()==0x10u);
    ack_tom();
    step(); /* JERRY remains high; TOM must not re-latch */
    assert(jerry()==0x20u && !tom());
    ack_jerry();
    step(); /* next word generates a genuine new edge */
    assert(jerry()==0x20u && tom()==0x10u);
}
int main(void)
{
    test_rising_falling_irq_reaches_cpu();
    test_everyword_without_strobe_and_no_spurious_irq();
    free(m.audio);
    puts("R262 JERRY I2S SYN CPU latch, TOM edge and DSP interrupt PASS");
    return 0;
}
