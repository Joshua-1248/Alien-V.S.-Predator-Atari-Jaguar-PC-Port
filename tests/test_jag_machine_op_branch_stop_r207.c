/* RE40 R207: execute real synthetic 68000 cartridge writes and TOM object
 * lists. The Rev.8 branch table lists CC=4 but mistakenly labels CC bits
 * 14..15; the later Jaguar Software Reference V10 specifies bits 14..16.
 * STOP bit 3 gates host interrupts, but never prevents OB0-3 publication.
 */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t cart[0x4000];
static JagMachine m;
static unsigned pc;
static void w16(uint16_t v){assert(pc+2u<=sizeof cart);cart[pc++]=(uint8_t)(v>>8);cart[pc++]=(uint8_t)v;}
static void w32(uint32_t v){w16((uint16_t)(v>>16));w16((uint16_t)v);}
static void ml(uint32_t v,uint32_t addr){w16(0x23fcu);w32(v);w32(addr);}
static void mw(uint16_t v,uint32_t addr){w16(0x33fcu);w16(v);w32(addr);}
static void phrase(uint32_t addr,uint64_t v){ml((uint32_t)(v>>32),addr);ml((uint32_t)v,addr+4u);}

static void check_branch(unsigned cc,unsigned ypos,int flag,unsigned hdb,int expected_link)
{
    const uint32_t list=0x1000u;
    const uint64_t fall=UINT64_C(0x123456789abcde04);
    const uint64_t link=UINT64_C(0xabcddcba11223304);
    uint64_t branch=3u|((uint64_t)(ypos&0x7ffu)<<3)|
        ((uint64_t)(cc&7u)<<14)|((uint64_t)((list+16u)>>3)<<24);
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0;
    pc=0x2000u;
    phrase(list,branch);phrase(list+8u,fall);phrase(list+16u,link);
    mw(28u,0xf00046u);mw(30u,0xf00048u);
    mw((uint16_t)hdb,0xf00038u);mw(0x0680u,0xf0003cu);
    ml(list<<16,0xf00020u);
    mw((uint16_t)flag,0xf00026u);
    mw(0x06c7u,0xf00028u);
    w16(0x60feu);
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_run_frames(&m,1);
    assert(m.op_objects>=2u);
    assert(m.ob==(expected_link?link:fall));
    assert((m.int1_pending&4u)==0u); /* neither STOP sets INT FLAG */
    assert(m.op_unsupported_mask==0u);
}

static void check_stop_irq(unsigned irq)
{
    const uint32_t list=0x1000u;
    const uint64_t code=UINT64_C(0x123456789abcdf04)|(irq?8u:0u);
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0;
    pc=0x2000u;
    phrase(list,code);
    mw(28u,0xf00046u);mw(30u,0xf00048u);
    mw(0x00cbu,0xf00038u);mw(0x0680u,0xf0003cu);
    ml(list<<16,0xf00020u);
    mw(0x06c7u,0xf00028u);
    w16(0x60feu);
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_run_frames(&m,1);
    assert(m.op_objects>=1u);
    assert(m.ob==code);
    assert(((m.int1_pending&4u)!=0u)==(irq!=0u));
    assert(m.op_unsupported_mask==0u);
}

int main(void)
{
    /* Both STOP interrupt states publish their entire 64-bit phrase. */
    check_stop_irq(0);check_stop_irq(1);
    /* CC 0, 1, 2, 3 remain independent and obey documented comparisons. */
    check_branch(0,0x7ffu,0,0x00cbu,1);
    check_branch(0,27u,0,0x00cbu,0);
    check_branch(1,29u,0,0x00cbu,1);
    check_branch(1,27u,0,0x00cbu,0);
    check_branch(2,27u,0,0x00cbu,1);
    check_branch(2,29u,0,0x00cbu,0);
    check_branch(3,0x7ffu,1,0x00cbu,1);
    check_branch(3,0x7ffu,0,0x00cbu,0);
    /* Crucial: CC=4 must NOT alias CC=0, even with YPOS=0x7ff. */
    check_branch(4,0x7ffu,1,0x00cbu,0);
    check_branch(4,0x7ffu,0,0x04cbu,1);
    puts("R207 branch CC 0..4 / HDB1 HC10 / STOP interrupt gate PASS");
    return 0;
}
