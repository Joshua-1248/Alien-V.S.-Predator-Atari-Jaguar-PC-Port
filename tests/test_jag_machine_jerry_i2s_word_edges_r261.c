/* RE43 R261: JERRY I2S clock, word-strobe phase and SSTAT WS.
 * Atari Software Reference V10 synchronous serial: INTERNAL clocks the
 * 32-bit stereo pair even without RISING/FALLING/EVERYWORD interrupts;
 * generated WS is high for 16 bit clocks and low for 16 bit clocks.
 * The fixture makes no claim about power-on physical pin phase or external
 * clocked input. Its explicit phase convention is low at enable. */
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
    jagm_write16(&m,0xF0002Eu,31u); /* 32 system clocks/half-line */
    jagm_write16(&m,0xF0003Eu,0u);  /* one half-line/field */
    jagm_write16(&m,0xF0004Eu,0x7FFu);
    jagm_write16(&m,0xF14000u,0x0100u);
    jagm_write32(&m,0xF1A148u,0x1357u);
    jagm_write32(&m,0xF1A14Cu,0x2468u);
    jagm_write32(&m,0xF1A150u,0u); /* 2 sys ticks/bit; 32/half-word */
    m.capture_audio=1;
}
static unsigned ws(void){return jagm_read32(&m,0xF1A150u)&1u;}
static int irq(void){return !!(m.dsp.latches&(1u<<1));}
static void clear_irq(void){m.dsp.latches&=~(1u<<1);}
static void step(void){jagm_run_frames(&m,1u);}
static void finish(void){free(m.audio);m.audio=NULL;}

static void test_clock_without_irq_selects(void)
{
    init();
    jagm_write32(&m,0xF1A154u,0x0005u); /* INTERNAL + WSEN, no IRQ */
    assert(ws()==0u);
    step(); /* first 16 bit clocks: low -> high */
    assert(ws()==1u);
    assert(!irq());
    assert(m.audio_n==0u);
    step(); /* second 16 bit clocks: high -> low */
    assert(ws()==0u);
    assert(m.audio_n==2u);
    assert(m.audio[0]==(int16_t)0x1357&&m.audio[1]==(int16_t)0x2468);
    step();step();
    assert(ws()==0u && m.audio_n==4u && !irq());
    finish();
}
static void test_edge_irq_polarity(void)
{
    init();
    jagm_write32(&m,0xF1A154u,0x000Du); /* rising only */
    step();
    assert(ws()==1u && irq());
    clear_irq();
    step();
    assert(ws()==0u && !irq());
    jagm_write32(&m,0xF1A154u,0x0015u); /* falling only */
    step();
    assert(ws()==1u && !irq());
    step();
    assert(ws()==0u && irq());
    clear_irq();
    jagm_write32(&m,0xF1A154u,0x001Du); /* both edges */
    step();assert(ws()==1u && irq());
    clear_irq();
    step();assert(ws()==0u && irq());
    finish();
}
static void test_everyword_without_strobe(void)
{
    init();
    jagm_write32(&m,0xF1A154u,0x0021u); /* INTERNAL + EVERYWORD */
    step();
    assert(ws()==0u && irq());
    clear_irq();
    step();
    assert(ws()==0u && irq());
    assert(m.audio_n==2u);
    finish();
}
static void test_disable_strobe_finishes_high_pulse(void)
{
    init();
    jagm_write32(&m,0xF1A154u,0x0005u);
    step();assert(ws()==1u);
    jagm_write32(&m,0xF1A154u,0x0009u); /* no new WS high pulses */
    step();assert(ws()==0u);
    step();assert(ws()==0u && !irq());
    step();assert(ws()==0u && m.audio_n==4u);
    jagm_write32(&m,0xF1A154u,0x000Du); /* re-enable rising */
    step();assert(ws()==1u && irq());
    clear_irq();
    jagm_write32(&m,0xF1A154u,0x000Cu); /* external timing unknown: no internal clock */
    step();assert(ws()==0u && !irq() && m.audio_n==4u);
    finish();
}
static void test_sclk_halfword_divider(void)
{
    init();
    jagm_write32(&m,0xF1A150u,1u); /* 4 sys ticks/bit: 64/half-word */
    jagm_write32(&m,0xF1A154u,0x0005u);
    step();assert(ws()==0u && m.audio_n==0u);
    step();assert(ws()==1u && m.audio_n==0u);
    step();assert(ws()==1u && m.audio_n==0u);
    step();assert(ws()==0u && m.audio_n==2u);
    finish();
}
int main(void)
{
    test_clock_without_irq_selects();
    test_edge_irq_polarity();
    test_everyword_without_strobe();
    test_disable_strobe_finishes_high_pulse();
    test_sclk_halfword_divider();
    puts("R261 JERRY I2S 16-bit word phases, IRQ edges, SSTAT and free-running clock PASS");
    return 0;
}
