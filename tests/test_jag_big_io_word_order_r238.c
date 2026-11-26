/* RE41 R238: TRM Rev.8 pp.59-60/109-110, BIG_IO (DORG bit 0).
 * The 68000-facing I/O bus is 16-bit: with BIG_IO=0 the low word of
 * each GPU/DSP 32-bit register occupies its lower address; with BIG_IO=1
 * the high word occupies the lower address.  This is word lane mapping,
 * not an attempt to implement memory bus arbitration or write hazards. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern unsigned int m68k_read_memory_32(unsigned int a);
static JagMachine m;
static uint8_t rom[0x4000];
static unsigned at;
static void w16at(unsigned o,uint16_t v)
{rom[o]=(uint8_t)(v>>8);rom[o+1u]=(uint8_t)v;}
static void emit16(uint16_t v){w16at(at,v);at+=2u;}
static void emit32(uint32_t v){emit16((uint16_t)(v>>16));emit16((uint16_t)v);}
static void movel_imm(uint32_t v,uint32_t addr)
{emit16(0x23FCu);emit32(v);emit32(addr);}
static void init(void)
{
    memset(rom,0,sizeof rom);
    w16at(0x404u,0x0080u);w16at(0x406u,0x2000u);
    w16at(0x2000u,0x60FEu);
    assert(jagm_init(&m,rom,sizeof rom)==0);
}
static void check_window(uint32_t addr,uint32_t value,int big_io)
{
    uint16_t first=(uint16_t)(big_io?(value>>16):value);
    uint16_t second=(uint16_t)(big_io?value:(value>>16));
    assert(jagm_read16(&m,addr)==first);
    assert(jagm_read16(&m,addr+2u)==second);
}
static void check_registers(void)
{
    init();
    assert(m.gpu.bigend==0u&&m.dsp.bigend==0u);
    jagm_write32(&m,0xF02110u,0x00332468u);
    jagm_write32(&m,0xF1A110u,0x00B00ABCu);
    check_window(0xF02110u,0x00332468u,0);
    check_window(0xF1A110u,0x00B00ABCu,0);
    jagm_write32(&m,0xF0210Cu,1u);
    jagm_write32(&m,0xF1A10Cu,1u);
    check_window(0xF02110u,0x00332468u,1);
    check_window(0xF1A110u,0x00B00ABCu,1);
    /* The 32-bit RISC bus retains its native longword, not a CPU 16-bit view. */
    assert(jagm_read32(&m,0xF02110u)==0x00332468u);
    assert(jagm_read32(&m,0xF1A110u)==0x00B00ABCu);
}
static void check_word_writes(void)
{
    init();
    /* Low-word-first default; the second half commits a single register write. */
    jagm_write16(&m,0xF02110u,0x1234u);
    jagm_write16(&m,0xF02112u,0xF032u);
    assert(m.gpu.pc==0x00321234u);
    jagm_write16(&m,0xF1A110u,0x5678u);
    jagm_write16(&m,0xF1A112u,0xF1B0u);
    assert(m.dsp.pc==0x00B05678u);
    /* RAM halfword writes must use the same endian lane mapping. */
    jagm_write16(&m,0xF03000u,0xABCDu);
    jagm_write16(&m,0xF03002u,0x0123u);
    assert(jagm_read32(&m,0xF03000u)==0x0123ABCDu);
    jagm_write16(&m,0xF1B000u,0x0FEDu);
    jagm_write16(&m,0xF1B002u,0x9988u);
    assert(jagm_read32(&m,0xF1B000u)==0x99880FEDu);
    jagm_write32(&m,0xF0210Cu,1u);
    jagm_write32(&m,0xF1A10Cu,1u);
    jagm_write16(&m,0xF03004u,0x1357u);
    jagm_write16(&m,0xF03006u,0x2468u);
    assert(jagm_read32(&m,0xF03004u)==0x13572468u);
    jagm_write16(&m,0xF1B004u,0xCAFEu);
    jagm_write16(&m,0xF1B006u,0xBABEu);
    assert(jagm_read32(&m,0xF1B004u)==0xCAFEBABEu);
}
static void check_real_cpu(void)
{
    init();
    at=0x2000u;
    movel_imm(0x11223344u,0xF03010u);
    movel_imm(0x55667788u,0xF1B010u);
    /* Set BIG_IO through the CPU using Atari's recommended duplicated
     * 16-bit pattern, then perform a second pair of MOVE.L stores. */
    movel_imm(0x00010001u,0xF0210Cu);
    movel_imm(0x00010001u,0xF1A10Cu);
    movel_imm(0x89ABCDEFu,0xF03014u);
    movel_imm(0x12345678u,0xF1B014u);
    emit16(0x60FEu);
    jagm_write16(&m,0xF0002Eu,127u);
    jagm_write16(&m,0xF0003Eu,3u);
    jagm_run_frames(&m,1u);
    /* A MOVE.L on a 68000 issues high-word at the lower bus address. */
    assert(jagm_read32(&m,0xF03010u)==0x33441122u);
    assert(jagm_read32(&m,0xF1B010u)==0x77885566u);
    assert((m.gpu.bigend&1u)&&(m.dsp.bigend&1u));
    assert(jagm_read32(&m,0xF03014u)==0x89ABCDEFu);
    assert(jagm_read32(&m,0xF1B014u)==0x12345678u);
    assert(m68k_read_memory_32(0xF03014u)==0x89ABCDEFu);
    assert(m68k_read_memory_32(0xF1B014u)==0x12345678u);
}
int main(void)
{
    check_registers();check_word_writes();check_real_cpu();
    puts("R238 GPU/DSP BIG_IO word lanes and 68000 transactions PASS");
    return 0;
}
