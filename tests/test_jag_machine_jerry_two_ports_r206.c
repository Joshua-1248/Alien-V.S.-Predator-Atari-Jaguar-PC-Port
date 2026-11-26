/* RE40 R206 — Jaguar JERRY dual controller matrix, original V10 pp.14-16.
 * Exercise the reference machine's actual memory-mapped JOYSTICK/JOYBUTS
 * registers, including independent selector nibbles and inverted port-2
 * selector bit order.  These are the two native Jaguar controller sockets;
 * this test does not model a Team Tap or the PC port's extra local players.
 */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static JagMachine machine;
static uint8_t cart[0x3000];
static void rows(uint16_t select,uint16_t joystick,uint16_t buttons)
{
    jagm_write16(&machine,0xf14000u,select);
    assert(jagm_read16(&machine,0xf14000u)==joystick);
    assert(jagm_read16(&machine,0xf14002u)==buttons);
}
int main(void)
{
    assert(jagm_init(&machine,cart,sizeof cart)==0);
    rows(0x81ffu,0xffffu,0xffffu); /* no controller rows selected */
    jagm_set_pad(&machine,0u,0x00020001u); /* P1 up + A */
    jagm_set_pad(&machine,1u,0x00010002u); /* P2 down + Pause */
    rows(0x817eu,0xdeffu,0xfff9u); /* independently selected row 0 */
    rows(0x81feu,0xfeffu,0xfffdu); /* port 1 only */
    rows(0x817fu,0xdfffu,0xfffbu); /* port 2 only */
    rows(0x017eu,0xffffu,0xffffu); /* output-enable bit 15 clear */
    /* Row 1 selectors: J5=0xD, J6=0xB (mirrored). Both buttons B. */
    jagm_set_pad(&machine,0u,0x00080010u);
    jagm_set_pad(&machine,1u,0x00080010u);
    rows(0x81bdu,0xeeffu,0xfff5u);
    /* Isolation: no P2 inputs must leak into J5 or B0/B1. */
    jagm_set_pad(&machine,0u,0u);
    rows(0x81bdu,0xefffu,0xfff7u);
    /* Changing P1 while scanning P2 must not affect P2's inputs. */
    jagm_set_pad(&machine,0u,0x00ffffffu);
    rows(0x81bfu,0xefffu,0xfff7u);
    /* Every standard-controller row must decode on both ports, with the
     * Jaguar's reversed J6 selector wiring.  Both B inputs and all four
     * direction/keypad columns are scanned, without cross-port leakage. */
    for(unsigned row=0u;row<4u;row++){
        uint32_t mask=(0xFu<<(row*4u))|(3u<<(16u+row*2u));
        uint16_t sel=(uint16_t)(0x8100u|
             ((0xFu^(1u<<(3u-row)))<<4u)|(0xFu^(1u<<row)));
        jagm_set_pad(&machine,0u,mask);
        jagm_set_pad(&machine,1u,mask);
        rows(sel,0x00ffu,0xfff0u);
        jagm_set_pad(&machine,1u,0u);
        rows(sel,0xf0ffu,0xfffcu);
    }
    puts("R206 Jaguar two-port JERRY controller matrix PASS");
    return 0;
}
