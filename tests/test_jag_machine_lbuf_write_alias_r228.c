/* RE41 R228: TOM LBUF write-only 32-bit address aliases.
 * Atari Jaguar Technical Reference Manual Rev.8, pp.17-18: the A, B and
 * current-write line-buffer windows are at F00800, F01000 and F01800;
 * adding 8000h provides a 32-bit write-only view.  This is a synthetic
 * cartridge regression; it does not assert physical bus-cycle timing. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PS32 (5u<<3)
#define W8 (3u<<11)
#define XADD1 (1u<<16)
#define SRCEN 1u
#define LFU_SRC (0xCu<<21)

static JagMachine machine;
static uint8_t cart[0x4000];
static unsigned pc;
static void w16(unsigned offset,uint16_t v)
{cart[offset]=(uint8_t)(v>>8);cart[offset+1]=(uint8_t)v;}
static void w32(unsigned offset,uint32_t v)
{w16(offset,(uint16_t)(v>>16));w16(offset+2,(uint16_t)v);}
static void emit16(uint16_t v){w16(pc,v);pc+=2u;}
static void emit32(uint32_t v){w32(pc,v);pc+=4u;}
static void emit_movel(uint32_t value,uint32_t addr)
{emit16(0x23FCu);emit32(value);emit32(addr);}
static void reset(void)
{
    memset(cart,0,sizeof cart);
    w32(0x404u,0x00802000u);
    w16(0x2000u,0x60FEu); /* BRA.S -2 */
    assert(jagm_init(&machine,cart,sizeof cart)==0);
}
static void check_words(unsigned bank,unsigned i,uint32_t value)
{
    assert(i+1u<720u);
    if(machine.lbuf[bank][i]!=(uint16_t)(value>>16)||
       machine.lbuf[bank][i+1u]!=(uint16_t)value){
        fprintf(stderr,"R227 missing LBUF alias: bank=%u word=%u got=%04x %04x expected=%08x\n",
                bank,i,machine.lbuf[bank][i],machine.lbuf[bank][i+1u],value);
        assert(0);
    }
}
static void check_direct_32bit_writes(void)
{
    reset();
    jagm_write32(&machine,0xF08800u,0x13579BDFu); /* bank A */
    check_words(0u,0u,0x13579BDFu);
    jagm_write32(&machine,0xF09004u,0xABCDEF12u); /* bank B */
    check_words(1u,2u,0xABCDEF12u);
    machine.lbuf_write=1u;
    jagm_write32(&machine,0xF0980Cu,0x00FFAA55u); /* current=B */
    check_words(1u,6u,0x00FFAA55u);
    machine.lbuf_write=0u;
    jagm_write32(&machine,0xF0980Cu,0x11223344u); /* current=A */
    check_words(0u,6u,0x11223344u);
    /* The final 32-bit word reaches physical indices 718 and 719. */
    jagm_write32(&machine,0xF08D9Cu,0x76543210u);
    check_words(0u,718u,0x76543210u);
    jagm_write32(&machine,0xF08DA0u,0xDEADBEEFu); /* outside 360x32 bits */
    check_words(0u,718u,0x76543210u);
    check_words(1u,0u,0u);
}
static void check_real_68000_write(void)
{
    reset();
    pc=0x2000u;
    emit_movel(0xDEADBEEFu,0xF08810u);
    emit_movel(0x12345678u,0xF09008u);
    emit16(0x60FEu);
    jagm_write16(&machine,0xF0002Eu,127u);
    jagm_write16(&machine,0xF0003Eu,3u);
    jagm_write16(&machine,0xF0004Eu,0x7FFu);
    jagm_run_frames(&machine,1u);
    check_words(0u,8u,0xDEADBEEFu);
    check_words(1u,4u,0x12345678u);
}
static void check_blitter_32bit_pixel(void)
{
    const uint32_t flags=PS32|W8|XADD1;
    reset();
    machine.ram[0x1500u]=0x45u;
    machine.ram[0x1501u]=0x67u;
    machine.ram[0x1502u]=0x89u;
    machine.ram[0x1503u]=0xABu;
    jagm_write32(&machine,0xF02200u,0xF08818u); /* A1 dest bank A alias */
    jagm_write32(&machine,0xF02204u,flags);
    jagm_write32(&machine,0xF0220Cu,0u);
    jagm_write32(&machine,0xF02224u,0x1500u);  /* A2 source */
    jagm_write32(&machine,0xF02228u,flags);
    jagm_write32(&machine,0xF02230u,0u);
    jagm_write32(&machine,0xF0223Cu,(1u<<16)|1u);
    jagm_write32(&machine,0xF02238u,SRCEN|LFU_SRC);
    check_words(0u,12u,0x456789ABu);
}
int main(void)
{
    check_direct_32bit_writes();
    check_real_68000_write();
    check_blitter_32bit_pixel();
    puts("R228 TOM LBUF 32-bit write-only alias CPU/Blitter PASS");
    return 0;
}
