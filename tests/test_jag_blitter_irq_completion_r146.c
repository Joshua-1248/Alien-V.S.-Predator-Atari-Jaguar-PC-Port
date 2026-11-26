/* R146: GPU Blitter IRQ must follow transfer completion, not B_CMD writes.
 * Synthetic cartridge only: no copyrighted ROM or timing assumptions.
 * The reference Blitter itself remains synchronous except B_STOP pauses.
 */
#include "jaguar/jag_machine.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SRCEN    (1u << 0)
#define LFU_SRC  (0xCu << 21)
#define DCOMPEN  (1u << 27)
#define BLIT_IRQ (1u << 4)
#define PS8      (3u << 3)
#define W8       (3u << 11)
#define XADD1    (1u << 16)

static JagMachine machine;
static uint8_t cart[0x4000];
static void w(uint32_t addr, uint32_t value) { jagm_write32(&machine, addr, value); }
static void clear_irq(void)
{
    /* GPU G_FLAGS interrupt latch clear bit 13 (interrupt number 4). */
    w(0xF02100u, 1u << 13);
    assert(!(machine.gpu.latches & BLIT_IRQ));
}
static void configure_transfer(void)
{
    const uint32_t f = PS8 | W8 | XADD1;
    w(0xF02200u, 0x1600u); w(0xF02204u, f); w(0xF0220Cu, 0);
    w(0xF02224u, 0x1500u); w(0xF02228u, f); w(0xF02230u, 0);
    w(0xF0223Cu, (1u << 16) | 5u);
    w(0xF02268u, 0); w(0xF0226Cu, 0); /* transparent pattern zero */
    machine.ram[0x1500] = 0x11u;
    machine.ram[0x1501] = 0;
    machine.ram[0x1502] = 0x22u;
    machine.ram[0x1503] = 0;
    machine.ram[0x1504] = 0x33u;
    memset(&machine.ram[0x1600], 0xA5, 5);
}
int main(void)
{
    cart[0x404] = 0; cart[0x405] = 0x80;
    cart[0x406] = 0x20; cart[0x407] = 0;
    cart[0x2000] = 0x60; cart[0x2001] = 0xFE; /* BRA * */
    assert(jagm_init(&machine, cart, sizeof cart) == 0);
    configure_transfer();

    /* NOGO only updates command register; it must not raise completion. */
    w(0xF02238u, SRCEN | LFU_SRC | 0x80u);
    assert(machine.blit.blits == 0u);
    assert(!(machine.gpu.latches & BLIT_IRQ));

    /* Collision stops at the second pixel, leaving unfinished work. */
    w(0xF02278u, 4u); /* STOPEN */
    w(0xF02238u, SRCEN | LFU_SRC | DCOMPEN);
    assert(machine.blit.stopped && machine.blit.pause.active);
    assert((jagm_read32(&machine, 0xF02238u) & 3u) == 2u);
    assert(machine.ram[0x1600] == 0x11u);
    assert(machine.ram[0x1602] == 0xA5u);
    assert(!(machine.gpu.latches & BLIT_IRQ));

    /* Writing CMD during STOPPED does not start or finish another blit. */
    w(0xF02238u, SRCEN | LFU_SRC);
    assert(machine.blit.stopped && machine.blit.blits == 1u);
    assert(!(machine.gpu.latches & BLIT_IRQ));

    /* Resume with STOPEN still enabled; stops again at the fourth pixel. */
    w(0xF02278u, 5u);
    assert(machine.blit.stopped);
    assert(machine.ram[0x1602] == 0x22u);
    assert(!(machine.gpu.latches & BLIT_IRQ));

    /* Resume with STOPEN disabled, finish exactly one original transfer. */
    w(0xF02278u, 1u);
    assert(!machine.blit.stopped && !machine.blit.pause.active);
    assert((jagm_read32(&machine, 0xF02238u) & 3u) == 1u);
    assert(machine.blit.blits == 1u);
    assert(machine.ram[0x1604] == 0x33u);
    assert((machine.gpu.latches & BLIT_IRQ) != 0u);
    clear_irq();

    /* A normal synchronous completed blit also asserts the completion IRQ. */
    w(0xF0220Cu, 0); w(0xF02230u, 0);
    w(0xF02238u, SRCEN | LFU_SRC);
    assert(!machine.blit.stopped && machine.blit.blits == 2u);
    assert((machine.gpu.latches & BLIT_IRQ) != 0u);
    clear_irq();

    /* ABORT discards the pending work; it must not be called a completion. */
    w(0xF0220Cu, 0); w(0xF02230u, 0);
    w(0xF02278u, 4u);
    w(0xF02238u, SRCEN | LFU_SRC | DCOMPEN);
    assert(machine.blit.stopped && !(machine.gpu.latches & BLIT_IRQ));
    w(0xF02278u, 2u);
    assert(!machine.blit.stopped && !machine.blit.pause.active);
    assert(!(machine.gpu.latches & BLIT_IRQ));

    puts("RE38 R146 GPU/Blitter completion IRQ integration PASS");
    return 0;
}
