/* RE42 R260: JERRY SCLK is 8-bit and WSEN gates generated word strobe.
 * Atari Jaguar Software Reference V10, JERRY synchronous serial interface.
 * This fixture does not assert electrical pin polarity or subcycle timing. */
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
    cart[0x2000]=0x60;cart[0x2001]=0xFE; /* BRA.S -2 */
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_write16(&m,0xF0002Eu,31u); /* HP=31: 32 clocks/half-line */
    jagm_write16(&m,0xF0003Eu,7u);  /* VP=7: 256 clocks/field */
    jagm_write16(&m,0xF0004Eu,0x7FFu);
    jagm_write16(&m,0xF14000u,0x0100u); /* audio output enabled */
    jagm_write32(&m,0xF1A148u,0x1357u);
    jagm_write32(&m,0xF1A14Cu,0x2468u);
}
static void test_sclk_width_and_mode_mask(void)
{
    init();
    /* The clock divider is physically eight bits: 0x0100 and 0 are equal. */
    jagm_write32(&m,0xF1A150u,0x00000100u);
    if(m.sclk!=0u)fprintf(stderr,"R259 SCLK 0x0100 retained high bits: %04x\n",m.sclk);
    assert(m.sclk==0u);
    jagm_write32(&m,0xF1A154u,0x0000000Du); /* INTERNAL | WSEN | RISING */
    m.capture_audio=1;
    jagm_run_frames(&m,1u);
    assert(m.audio_n==8u); /* 4 stereo pairs over 256 sys clocks */
    assert((m.dsp.latches&(1u<<1))!=0u);
    /* V8 and V10 disagree about SMODE bit 1 (MODE32 vs reserved).
     * Do not silently change its handling without a silicon trace. */
    jagm_write32(&m,0xF1A150u,0x000001FFu);
    assert(m.sclk==0x00FFu);
    free(m.audio);
}
static void test_wsen_controls_word_strobe_irq(void)
{
    init();
    jagm_write32(&m,0xF1A150u,0u);
    jagm_write32(&m,0xF1A154u,0x0009u); /* INTERNAL+RISING but NO WSEN */
    m.capture_audio=1;
    jagm_run_frames(&m,1u);
    if(m.dsp.latches&(1u<<1))fputs("R259 raised word-strobe IRQ while WSEN=0\n",stderr);
    assert((m.dsp.latches&(1u<<1))==0u);
    /* Regression guard: only IRQ delivery changed.  Preserve the old
     * logical host sample cadence until physical WS/DAC behavior is known. */
    assert(m.audio_n==8u);
    jagm_write32(&m,0xF1A154u,0x000Du); /* enable WSEN */
    jagm_run_frames(&m,1u);
    assert((m.dsp.latches&(1u<<1))!=0u);
    /* Removing INTERNAL tristates Jerry's clock and strobe outputs. */
    m.dsp.latches&=~(1u<<1);
    jagm_write32(&m,0xF1A154u,0x000Cu);
    jagm_run_frames(&m,1u);
    assert((m.dsp.latches&(1u<<1))==0u);
    free(m.audio);
}
int main(void)
{
    test_sclk_width_and_mode_mask();
    test_wsen_controls_word_strobe_irq();
    puts("R260 JERRY I2S SCLK width and SMODE WSEN gating PASS");
    return 0;
}
