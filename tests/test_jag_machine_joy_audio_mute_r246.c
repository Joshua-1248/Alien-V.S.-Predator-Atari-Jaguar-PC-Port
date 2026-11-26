/* RE42 R246: JOYSTICK bit 8 gates the physical I2S audio output.
 * Original Jaguar Software Reference V10, JOYSTICK; JERRY audio subsystem.
 * The serial transmitter continues to run and emit DSP word strobe IRQs
 * while muted. The host capture is the physical output, not the DSP TX latch.
 */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static JagMachine m;
static uint8_t cart[0x4000];

static void check_field(uint16_t joy,unsigned audible)
{
    const size_t start=m.audio_n;
    jagm_write16(&m,0xF14000u,joy);
    jagm_run_frames(&m,1u);
    assert(m.audio_n==start+8u); /* HP=31, VP=7: 256 clocks / 64 per word pair */
    for(size_t i=start;i<m.audio_n;i+=2u){
        if(m.audio[i]!=(audible?(int16_t)0x1357:0) ||
           m.audio[i+1]!=(audible?(int16_t)0x2468:0)){
            fprintf(stderr,"R246 baseline defect: JOYSTICK=%04x sample pair #%zu = %04x,%04x, expected %04x,%04x\n",
              joy,(i-start)/2u,(uint16_t)m.audio[i],(uint16_t)m.audio[i+1],
              audible?0x1357:0,audible?0x2468:0);
            assert(0);
        }
    }
}

int main(void)
{
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;
    cart[0x2000]=0x60;cart[0x2001]=0xFE; /* 68000 BRA.S -2 */
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_write16(&m,0xF0002Eu,31u); /* HP=31: 32 system clocks per half-line */
    jagm_write16(&m,0xF0003Eu,7u); /* VP=7: 8 half-lines per field */
    jagm_write16(&m,0xF0004Eu,0x7FFu);
    jagm_write32(&m,0xF1A148u,0x1357u);
    jagm_write32(&m,0xF1A14Cu,0x2468u);
    jagm_write32(&m,0xF1A150u,0u); /* SCLK 0 -> 64 clocks/word pair */
    jagm_write32(&m,0xF1A154u,13u); /* INTERNAL + WSEN + RISING */
    assert(m.ltxd==0x1357u&&m.rtxd==0x2468u&&m.smode==13u);
    m.capture_audio=1;
    check_field(0x0000u,0u); /* reset state: muted */
    check_field(0x0100u,1u); /* bit 8 high: audible without pad outputs */
    check_field(0x8000u,0u); /* pad enable high does NOT override mute */
    check_field(0x8100u,1u); /* both enabled */
    check_field(0x0000u,0u); /* immediate re-mute at next sample boundary */
    assert(m.frames==5u && m.audio_n==40u);
    assert(m.i2s_acc==0u);
    /* Audio muting must not suppress the serial word strobe interrupt. */
    assert((m.dsp.latches&(1u<<1))!=0u);
    free(m.audio);
    puts("R246 JERRY JOYSTICK audio mute/I2S output gate PASS");
    return 0;
}
