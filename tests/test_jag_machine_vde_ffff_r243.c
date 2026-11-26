/* RE42 R243: VDE=$FFFF is the documented Jaguar console workaround
 * to allow Object Processor processing on every half-line.  Ordinary VDE
 * remains an eleven-bit comparator.  Distinguish the sentinel at VC=0x7ff
 * with a second-half HDB start on a synthetic 68000 cartridge. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t cart[0x3000];

static void put64(uint32_t addr, uint64_t value)
{
    jagm_write32(&m,addr,(uint32_t)(value>>32));
    jagm_write32(&m,addr+4u,(uint32_t)value);
}

static uint64_t count_odd_half_lines(uint16_t vde)
{
    memset(cart,0,sizeof cart);
    cart[0x404]=0u;cart[0x405]=0x80u;cart[0x406]=0x20u;
    cart[0x2000]=0x60u;cart[0x2001]=0xfeu; /* BRA -2 */
    assert(jagm_init(&m,cart,sizeof cart)==0);
    put64(0x1000u,4u); /* STOP, no interrupt */
    jagm_write16(&m,0xF00020u,0x1000u);jagm_write16(&m,0xF00022u,0u);
    jagm_write16(&m,0xF0002Eu,31u);    /* 32 video clocks / half-line */
    jagm_write16(&m,0xF0003Eu,0x07ffu); /* maximum 11-bit VP */
    jagm_write16(&m,0xF00046u,0u);     /* VDB=0 */
    jagm_write16(&m,0xF00048u,vde);
    assert(m.vde==(vde==0xffffu?0xffffu:(vde&0x7ffu)));
    jagm_write16(&m,0xF00038u,0x0404u); /* OP in SECOND half only */
    jagm_write16(&m,0xF0003Au,0x07ffu); /* no HDB2 match */
    jagm_write16(&m,0xF0003Cu,0x0420u); /* HDE after HDB1 */
    jagm_write16(&m,0xF00028u,7u);      /* RGB16, video enabled */
    jagm_run_frames(&m,1u);
    assert(m.frames==1u);
    assert(m.op_objects==m.op_lines);
    assert(m.op_unsupported_mask==0u);
    return m.op_lines;
}

int main(void)
{
    const uint64_t bounded=count_odd_half_lines(0x07ffu);
    const uint64_t high_masked=count_odd_half_lines(0xf7ffu);
    const uint64_t every=count_odd_half_lines(0xffffu);
    assert(bounded==1023u); /* VC=1,3,...,2045 */
    assert(high_masked==bounded); /* high bits alone are NOT sentinel */
    assert(every==1024u); /* includes the final VC=2047 OP pass */
    printf("R243 VDE=FFFF full-field OP sentinel PASS (normal=%llu all=%llu)\n",
           (unsigned long long)bounded,(unsigned long long)every);
    return 0;
}
