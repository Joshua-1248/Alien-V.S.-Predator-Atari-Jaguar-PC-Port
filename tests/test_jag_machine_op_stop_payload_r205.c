/* RE40 R205: TOM Object Processor STOP must publish the stopped object
 * phrase through OB0-3 before raising the host Object interrupt.
 * Original hardware: Jaguar Technical Reference Manual Rev.8 p.20.
 * Exercise the actual 68000 cartridge -> RAM object list -> TOM pipeline,
 * not a mocked STOP helper or synthetic direct register assignment.
 */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t cart[0x4000];
static JagMachine machine;
static unsigned pc;
static void w16(uint16_t v){assert(pc+2u<=sizeof cart);cart[pc++]=(uint8_t)(v>>8);cart[pc++]=(uint8_t)v;}
static void w32(uint32_t v){w16((uint16_t)(v>>16));w16((uint16_t)v);}
static void movel(uint32_t v,uint32_t addr){w16(0x23fcu);w32(v);w32(addr);}
static void movew(uint16_t v,uint32_t addr){w16(0x33fcu);w16(v);w32(addr);}

static void check_stop(uint64_t stop_word, int preceded_by_bitmap)
{
    const uint32_t list=0x1000u;
    const uint32_t stop=preceded_by_bitmap?list+16u:list;
    unsigned j;
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0;
    pc=0x2000u;
    if(preceded_by_bitmap){
        /* Active one-scanline 16-bpp object before STOP. Its header and
         * data-path writeback must not overwrite the reported STOP code. */
        const uint64_t bitmap=((uint64_t)28u<<3)|((uint64_t)1u<<14)|
             ((uint64_t)(stop>>3)<<24)|((uint64_t)(0x3000u>>3)<<43);
        const uint64_t second=3u|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
             ((uint64_t)1u<<28);
        movel(0xf800f800u,0x3000u);
        movel(0x00f800f8u,0x3004u);
        movel((uint32_t)(bitmap>>32),list);
        movel((uint32_t)bitmap,list+4u);
        movel((uint32_t)(second>>32),list+8u);
        movel((uint32_t)second,list+12u);
    }
    movel((uint32_t)(stop_word>>32),stop);
    movel((uint32_t)stop_word,stop+4u);
    movew(28u,0xf00046u);movew(508u,0xf00048u);
    movew(0x00cbu,0xf00038u);movew(0x0680u,0xf0003cu);
    movel(list<<16,0xf00020u);
    movew(0x06c7u,0xf00028u);
    w16(0x60feu); /* infinite branch: leave STOP IRQ pending */
    assert(jagm_init(&machine,cart,sizeof cart)==0);
    jagm_run_frames(&machine,1u);
    assert(machine.op_objects>=1u);
    assert(((machine.int1_pending&4u)!=0u)==((stop_word&8u)!=0u));
    assert(machine.ob==stop_word);
    for(j=0;j<4u;j++){
        /* Flare/Atari OB.NET: OB0 has the phrase's lowest 16 bits. */
        uint16_t expected=(uint16_t)(stop_word>>(j*16u));
        assert(jagm_read16(&machine,0xf00010u+j*2u)==expected);
    }
    assert(machine.op_unsupported_mask==0u);
}

int main(void)
{
    /* STOP payload is always published. Per later Atari Software Reference
     * V10, bit 3 enables its CPU interrupt; Rev.8 omitted that distinction. */
    check_stop(UINT64_C(0xa56c7319428f1004),0);
    check_stop(UINT64_C(0x5a938ce6bd70effc),1);
    puts("R205/R207 STOP object payload + conditional host interrupt PASS");
    return 0;
}
