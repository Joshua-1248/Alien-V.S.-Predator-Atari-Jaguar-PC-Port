/* R259: JERRY UART register behaviour, double-buffered data and clocked TX.
 * Atari Jaguar Technical Reference Rev.8 pp.93-95.  No network or serial
 * device is opened by the reference machine. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static JagMachine m;
static uint8_t cart[0x4000];
static void init(void)
{
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;
    cart[0x2000]=0x60;cart[0x2001]=0xFE;
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_write16(&m,0xF0002Eu,0u);
    jagm_write16(&m,0xF0003Eu,0u);
    jagm_write16(&m,0xF0004Eu,0x7FFu);
}
static void test_control_and_clock(void)
{
    init();
    /* The 16-bit divider is RW, not a write-only placeholder. */
    jagm_write16(&m,0xF10034u,0x1234u);
    assert(jagm_read16(&m,0xF10034u)==0x1234u);
    assert((jagm_read16(&m,0xF10032u)&0x0180u)==0x0100u);
    jagm_write16(&m,0xF10032u,0x401Fu);
    assert((jagm_read16(&m,0xF10032u)&0x41FFu)==0x411Fu);
    jagm_write16(&m,0xF10032u,0x0028u);
    assert((jagm_read16(&m,0xF10032u)&0x403Fu)==0x0028u);
    /* Write-only CTRL bit 6 clears errors without becoming readable. */
    jagm_write16(&m,0xF10032u,0x0048u);
    assert((jagm_read16(&m,0xF10032u)&0x407Fu)==0x0008u);
}
static void test_two_deep_receive_errors_and_irq(void)
{
    init();
    jagm_write16(&m,0xF10032u,0x0020u); /* RX interrupt enable */
    jagm_write16(&m,0xF10020u,0x0010u); /* JERRY ASYN IRQ enable */
    jagm_uart_receive_byte(&m,0xA5u,0u);
    assert((jagm_read16(&m,0xF10032u)&0x0080u)!=0u);
    assert((jagm_read16(&m,0xF10020u)&0x0010u)!=0u);
    jagm_uart_receive_byte(&m,0x5Au,0x0200u); /* parity flag */
    assert((jagm_read16(&m,0xF10032u)&0x8200u)==0x8200u);
    /* Third byte overflows two-entry receive storage, without corrupting
     * the first two unread bytes. */
    jagm_uart_receive_byte(&m,0xFFu,0u);
    assert((jagm_read16(&m,0xF10032u)&0x8800u)==0x8800u);
    assert(jagm_read16(&m,0xF10030u)==0x00A5u);
    assert((jagm_read16(&m,0xF10032u)&0x0080u)!=0u);
    assert(jagm_read16(&m,0xF10030u)==0x005Au);
    assert((jagm_read16(&m,0xF10032u)&0x0080u)==0u);
    /* Last received value is preserved when no unread character remains. */
    assert(jagm_read16(&m,0xF10030u)==0x005Au);
    jagm_write16(&m,0xF10032u,0x0060u); /* CLRERR + RINTEN */
    assert((jagm_read16(&m,0xF10032u)&0x8E00u)==0u);
    jagm_write16(&m,0xF10020u,0x1010u); /* acknowledge ASYN */
    assert((jagm_read16(&m,0xF10020u)&0x0010u)==0u);
}
static void test_transmit_holding_and_irq(void)
{
    init();
    jagm_write16(&m,0xF10034u,0u); /* one system clock per UART /16 tick */
    jagm_write16(&m,0xF10032u,0x0010u); /* TX interrupt enable */
    jagm_write16(&m,0xF10020u,0x0010u);
    assert((jagm_read16(&m,0xF10032u)&0x0100u)!=0u);
    jagm_write16(&m,0xF10030u,0x01A5u); /* only low eight bits transmit */
    assert((jagm_read16(&m,0xF10032u)&0x0100u)!=0u);
    jagm_write16(&m,0xF10030u,0x005Au); /* second buffered byte */
    assert((jagm_read16(&m,0xF10032u)&0x0100u)==0u);
    /* Start+8 data+parity slot+stop = 11 bit times, /16 prescaler. */
    jagm_run_frames(&m,175u);
    assert((jagm_read16(&m,0xF10032u)&0x0100u)==0u);
    jagm_run_frames(&m,1u);
    assert((jagm_read16(&m,0xF10032u)&0x0100u)!=0u);
    assert((jagm_read16(&m,0xF10020u)&0x0010u)!=0u);
    jagm_write16(&m,0xF10020u,0x1010u);
    assert((jagm_read16(&m,0xF10020u)&0x0010u)==0u);
}
static void test_nonzero_baud_divisor_and_no_phantom_rx(void)
{
    init();
    jagm_write16(&m,0xF10034u,1u);
    assert(jagm_read16(&m,0xF10034u)==1u);
    assert(jagm_read16(&m,0xF10030u)==0u); /* no attached receiver */
    jagm_write16(&m,0xF10030u,0x0073u);
    jagm_write16(&m,0xF10030u,0x00D4u);
    /* 11 serial bits x 16 ticks x (ASICLK+1). */
    jagm_run_frames(&m,351u);
    assert((jagm_read16(&m,0xF10032u)&0x0100u)==0u);
    jagm_run_frames(&m,1u);
    assert((jagm_read16(&m,0xF10032u)&0x0100u)!=0u);
    assert(jagm_read16(&m,0xF10034u)==1u);
    assert(jagm_read16(&m,0xF10030u)==0u);
}
int main(void)
{
    test_control_and_clock();
    test_nonzero_baud_divisor_and_no_phantom_rx();
    test_two_deep_receive_errors_and_irq();
    test_transmit_holding_and_irq();
    puts("R259 JERRY UART control/status, RX/TX buffering and baud timing PASS");
    return 0;
}
