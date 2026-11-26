/* RE40 R211: TOM BITOBJ's interlaced two-source-line writeback.
 * Rev. 8 pp. 15, 18: VP+1 odd enables interlace, and each displayed
 * unscaled bitmap line consumes two source rows (HEIGHT decrements two).
 * Synthetic 68000 cartridge; no commercial ROM data. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t cart[0x3000];

static void check(unsigned interlace, unsigned height, unsigned dwidth,
                  unsigned depth, unsigned reflect)
{
    const uint32_t list=0x1000u, stop=0x1020u, src=0x1800u;
    uint64_t first, second, after;
    unsigned stride=interlace?2u:1u;
    memset(cart,0,sizeof cart);
    cart[0x404]=0x00;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0x00;
    cart[0x2000]=0x60;cart[0x2001]=0xfe; /* 68000 loop */
    assert(jagm_init(&m,cart,sizeof cart)==0);
    first=((uint64_t)28u<<3)|((uint64_t)height<<14)|
          ((uint64_t)(stop>>3)<<24)|((uint64_t)(src>>3)<<43);
    second=8u|((uint64_t)depth<<12)|((uint64_t)1u<<15)|
           ((uint64_t)dwidth<<18)|((uint64_t)1u<<28)|
           ((uint64_t)reflect<<45);
    jagm_write32(&m,list,(uint32_t)(first>>32));
    jagm_write32(&m,list+4u,(uint32_t)first);
    jagm_write32(&m,list+8u,(uint32_t)(second>>32));
    jagm_write32(&m,list+12u,(uint32_t)second);
    jagm_write32(&m,stop,0u);jagm_write32(&m,stop+4u,4u);
    jagm_write32(&m,src,0xF800F800u);
    jagm_write32(&m,src+4u,0xF800F800u);
    jagm_write16(&m,0xf00020u,(uint16_t)list);
    jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf0002eu,31u); /* HP+1=32 */
    jagm_write16(&m,0xf0003eu,interlace?30u:31u);
    jagm_write16(&m,0xf00046u,28u);
    jagm_write16(&m,0xf00048u,30u); /* only VC=28 is active */
    jagm_write16(&m,0xf00038u,4u);
    jagm_write16(&m,0xf0003au,4u);
    jagm_write16(&m,0xf0003cu,20u);
    jagm_write16(&m,0xf00028u,7u); /* RGB16, video on */
    jagm_run_frames(&m,1u);
    assert(m.frames==1u);
    assert(m.op_lines==1u && m.op_objects==2u);
    assert(m.op_unsupported_mask==0u);
    after=((uint64_t)jagm_read32(&m,list)<<32)|jagm_read32(&m,list+4u);
    assert(((after>>14)&0x3ffu)==(height>stride?height-stride:0u));
    assert(((after>>43)&0x1fffffu)==(((src>>3)+dwidth*stride)&0x1fffffu));
    assert((((uint64_t)jagm_read32(&m,list+8u)<<32)|jagm_read32(&m,list+12u))==second);
}

int main(void)
{
    /* 240 configurations: both video scan modes, six supported pixel
     * depths, forward/reflected draws, heights 1..5, and two strides. */
    for(unsigned i=0;i<2u;i++)for(unsigned depth=0u;depth<=5u;depth++)
        for(unsigned reflect=0u;reflect<2u;reflect++)
            for(unsigned h=1u;h<=5u;h++){
                check(i,h,1u,depth,reflect);
                check(i,h,3u,depth,reflect);
            }
    puts("R211 TOM unscaled bitmap interlace HEIGHT/DATA writeback PASS");
    return 0;
}
