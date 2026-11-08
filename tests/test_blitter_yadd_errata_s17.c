/* RE24 Sweep 17: independently written regression for Atari JDEV0002
 * (1995-03-22), Blitter Bugs & Warnings item 1.
 * A2's Y-add enable is ignored: A1's enable controls BOTH generators.
 * YSIGN is negative only when the generator's corresponding YADD bit
 * is set, even though only A1 YADD enables stepping. No SDK code/ROM.
 */
#include "jaguar/blitter_trm.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t ram[1u << 16];
static uint8_t get8(void *ctx, uint32_t addr)
{ (void)ctx; return ram[addr & 0xffffu]; }
static void put8(void *ctx, uint32_t addr, uint8_t value)
{ (void)ctx; ram[addr & 0xffffu] = value; }
static void W(JagBlitter *b, uint32_t offset, uint32_t value)
{ assert(jag_blitter_write(b, offset, value)); }

/* Test all enable combinations, independent signs, destination selection,
 * pixel/phrase operation, and 1 vs 3 pixels per blit. */
static void check_case(int a1_yadd, int a2_yadd, int a1_neg,
                       int a2_neg, int swap, int phrase, int count)
{
    enum { XADD_PIXEL = 1 << 16, YADD = 1 << 18,
           XSIGN_NEG = 1 << 19, YSIGN_NEG = 1 << 20,
           PATDSEL = 1 << 16, DSTA2 = 1 << 11 };
    /* 8-bit pixels, one contiguous phrase, width 20; both start at (0, 4). */
    const uint32_t flags_common = (3u << 3) | (4u << 11) | (1u << 9);
    const uint32_t flags_x = phrase ? 0u : XADD_PIXEL;
    const uint32_t a1flags = flags_common | flags_x |
                             (a1_yadd ? YADD : 0u) |
                             (a1_neg ? YSIGN_NEG : 0u);
    const uint32_t a2flags = flags_common | flags_x |
                             (a2_yadd ? YADD : 0u) |
                             (a2_neg ? YSIGN_NEG : 0u);
    const uint32_t initpos = 4u << 16;
    JagBlitBus bus = {0, get8, put8};
    JagBlitter b;
    memset(ram, 0, sizeof ram);
    jag_blitter_init(&b, bus);
    W(&b, 0x00, 0x2000u);
    W(&b, 0x04, a1flags);
    W(&b, 0x0c, initpos);
    W(&b, 0x24, 0x4000u);
    W(&b, 0x28, a2flags);
    W(&b, 0x30, initpos);
    W(&b, 0x68, 0x5a5a5a5au);
    W(&b, 0x6c, 0x5a5a5a5au);
    W(&b, 0x3c, (1u << 16) | (uint32_t)count);
    W(&b, 0x38, PATDSEL | (swap ? DSTA2 : 0u));

    /* Pixel mode updates after each pixel; phrase mode after each phrase. */
    const int steps = phrase ? 1 : count;
    const int want_a1 = 4 + (a1_yadd ? (a1_neg ? -steps : steps) : 0);
    const int want_a2 = 4 + (a1_yadd ? ((a2_neg && a2_yadd) ? -steps : steps) : 0);
    const int actual_a1 = (int16_t)(b.a1_pixel >> 16);
    const int actual_a2 = (int16_t)(b.a2_pixel >> 16);
    if (actual_a1 != want_a1 || actual_a2 != want_a2) {
        fprintf(stderr, "YADD: A1=%d A2=%d signs=%d/%d swap=%d phrase=%d count=%d: "
                "actual=%d/%d expected=%d/%d\n",
                a1_yadd, a2_yadd, a1_neg, a2_neg, swap, phrase, count,
                actual_a1, actual_a2, want_a1, want_a2);
        assert(actual_a1 == want_a1 && actual_a2 == want_a2);
    }
}

int main(void)
{
    unsigned cases = 0;
    for (int a1 = 0; a1 < 2; ++a1)
    for (int a2 = 0; a2 < 2; ++a2)
    for (int s1 = 0; s1 < 2; ++s1)
    for (int s2 = 0; s2 < 2; ++s2)
    for (int swap = 0; swap < 2; ++swap)
    for (int phrase = 0; phrase < 2; ++phrase)
    for (int count = 1; count <= 3; count += 2) {
        check_case(a1, a2, s1, s2, swap, phrase, count);
        ++cases;
    }
    printf("Atari JDEV0002 A1/A2 Y-add regression passed (%u cases)\n", cases);
    return 0;
}
