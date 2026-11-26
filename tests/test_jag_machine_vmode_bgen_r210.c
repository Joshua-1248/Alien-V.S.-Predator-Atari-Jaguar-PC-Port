/* RE40 R210: Jaguar Rev.8 TOM VMODE BGEN line-buffer contract.
 * In 16-bit CRY and RGB16 output, BGEN refills the displayed physical
 * line buffer with BG; it does not run in RGB24 or 16-bit direct mode.
 * Validate the reference machine's actual TOM MMIO and frame scheduler
 * without embedding any proprietary game data. */
#include "jaguar/jag_machine.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine machine;
static uint8_t cart[0x3000];

static void check(unsigned mode, unsigned bgen, unsigned split)
{
    const uint16_t pixel=0x9A5Cu, bg=0x55A8u;
    const uint16_t vm=(uint16_t)(1u|(mode<<1u)|(bgen?0x80u:0u));
    unsigned i;
    memset(cart,0,sizeof cart);
    cart[0x404]=0x00;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0x00;
    /* Cartridge code loops forever without touching the video registers. */
    cart[0x2000]=0x60;cart[0x2001]=0xfe;
    assert(jagm_init(&machine,cart,sizeof cart)==0);
    /* STOP object with interrupt flag clear, installed via TOM-visible DRAM. */
    jagm_write32(&machine,0x1000u,0u);
    jagm_write32(&machine,0x1004u,4u);
    jagm_write16(&machine,0xF00020u,0x1000u);
    jagm_write16(&machine,0xF00022u,0u);
    jagm_write16(&machine,0xF0002Eu,31u);  /* HP = 31 */
    jagm_write16(&machine,0xF0003Eu,31u);  /* VP = 31 */
    jagm_write16(&machine,0xF00046u,28u); /* one full visible line */
    jagm_write16(&machine,0xF00048u,30u);
    jagm_write16(&machine,0xF00038u,4u);
    jagm_write16(&machine,0xF0003Au,split?0x0406u:4u);
    jagm_write16(&machine,0xF0003Cu,split?0x0414u:20u);
    jagm_write16(&machine,0xF00058u,bg);
    jagm_write16(&machine,0xF00028u,vm);
    for(i=0;i<720u;i++){machine.lbuf[0][i]=pixel;machine.lbuf[1][i]=pixel;}
    jagm_run_frames(&machine,1u);
    assert(machine.frames==1u);
    assert(machine.frame_h==1u);
    assert(machine.op_lines==(split?2u:1u));
    assert(machine.op_unsupported_mask==0u);
    /* Both forms of 16-bit pixel conversion generate the same output as
     * their corresponding canonical helpers before the post-display clear. */
    if(mode==0u){
        uint8_t r,g,b;
        avp_jaguar_cry16_to_rgb(pixel,&r,&g,&b);
        assert(machine.frame[0]==((uint32_t)r<<16|((uint32_t)g<<8)|b));
    }else if(mode==3u)assert(machine.frame[0]==avp_jaguar_vmode_rgb16(pixel));
    else if(mode==1u)assert(machine.frame[0]==avp_jaguar_vmode_rgb24(pixel,pixel));
    /* MODE=2 is digital direct output; the host RGB approximation is not
     * asserted here, only the precisely documented physical BGEN behavior. */
    for(i=0;i<720u;i++){
        const uint16_t expect=(bgen&&(mode==0u||mode==3u))?bg:pixel;
        assert(machine.lbuf[0][i]==expect);
        assert(machine.lbuf[1][i]==(split?expect:pixel));
    }
}
int main(void)
{
    unsigned mode,bgen,split;
    for(mode=0u;mode<4u;mode++)for(bgen=0u;bgen<2u;bgen++)
        for(split=0u;split<2u;split++)check(mode,bgen,split);
    puts("R210 TOM VMODE BGEN, 16 modes including HDB2 dual-bank PASS");
    return 0;
}
