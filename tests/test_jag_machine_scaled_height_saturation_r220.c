/* RE41 R220 — Atari TOM SCBITOBJ vertical-step writeback at HEIGHT zero.
 * Rev.8 p.19: each VSCALE addition consumes a DATA stride even when the
 * 10-bit HEIGHT counter has saturated at zero.  Physical OP cartridge.
 * Deliberately non-interlaced; scaled interlaced parity is still open. */
#include "jaguar/jag_machine.h"
#include "jaguar/object_processor.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t rom[0x4000];

static void check(unsigned height,unsigned vscale,unsigned remainder,
                  unsigned dwidth,unsigned advances,unsigned final_remainder)
{
    const uint32_t list=0x1000u,stop=0x1020u,src=0x1800u;
    uint64_t p0,p1,p2,after,after2;
    AvpJaguarOpScaledVerticalStep step;
    AvpJaguarOpBitmapHeader hdr;
    memset(rom,0,sizeof rom);
    rom[0x404u]=0x00;rom[0x405u]=0x80;rom[0x406u]=0x20;
    rom[0x2000u]=0x60;rom[0x2001u]=0xfe;
    assert(jagm_init(&m,rom,sizeof rom)==0);
    p0=1u|((uint64_t)28u<<3)|((uint64_t)height<<14)|
       ((uint64_t)(stop>>3)<<24)|((uint64_t)(src>>3)<<43);
    p1=8u|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
       ((uint64_t)dwidth<<18)|((uint64_t)1u<<28);
    p2=0x20u|((uint64_t)vscale<<8)|((uint64_t)remainder<<16);
    jagm_write32(&m,list,(uint32_t)(p0>>32));
    jagm_write32(&m,list+4u,(uint32_t)p0);
    jagm_write32(&m,list+8u,(uint32_t)(p1>>32));
    jagm_write32(&m,list+12u,(uint32_t)p1);
    jagm_write32(&m,list+16u,(uint32_t)(p2>>32));
    jagm_write32(&m,list+20u,(uint32_t)p2);
    jagm_write32(&m,stop,0u);jagm_write32(&m,stop+4u,4u);
    jagm_write32(&m,src,0xf800f800u);jagm_write32(&m,src+4u,0xf800f800u);
    jagm_write16(&m,0xf00020u,(uint16_t)list);
    jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf0002eu,31u); /* 32 clocks per half line */
    jagm_write16(&m,0xf0003eu,31u); /* VP+1 even: non-interlaced */
    jagm_write16(&m,0xf00046u,28u);jagm_write16(&m,0xf00048u,30u);
    jagm_write16(&m,0xf00038u,4u);
    jagm_write16(&m,0xf0003au,127u); /* disable second comparator */
    jagm_write16(&m,0xf0003cu,20u);
    jagm_write16(&m,0xf00028u,7u); /* RGB16 */
    jagm_run_frames(&m,1u);
    assert(m.frames==1u&&m.op_lines==1u&&m.op_objects==2u);
    assert(m.op_unsupported_mask==0u);
    after=((uint64_t)jagm_read32(&m,list)<<32)|jagm_read32(&m,list+4u);
    after2=((uint64_t)jagm_read32(&m,list+16u)<<32)|jagm_read32(&m,list+20u);
    assert(((after>>14)&0x3ffu)==(height>advances?height-advances:0u));
    if(((after>>43)&0x1fffffu)!=(((src>>3)+advances*dwidth)&0x1fffffu)){
        fprintf(stderr,"R220 baseline defect: height=%u vscale=%u rem=%u "
                "advances=%u DATA actual=%06llx expected=%06x\n",
                height,vscale,remainder,advances,
                (unsigned long long)((after>>43)&0x1fffffu),
                ((src>>3)+advances*dwidth)&0x1fffffu);
        assert(0);
    }
    assert(((after2>>16)&0xffu)==final_remainder);
    hdr.type=1u;hdr.vscale=vscale;hdr.remainder=remainder;
    assert(avp_jaguar_op_scaled_vertical_step(&hdr,&step));
    assert(step.source_lines_advanced==advances);
    assert(step.next_remainder==final_remainder);
}
int main(void)
{
    check(1u, 8u, 0u,1u,4u,0u);
    check(2u, 5u, 1u,3u,7u,4u);
    check(1u, 1u, 0u,2u,32u,0u);
    check(10u,16u,16u,2u,1u,0u);
    check(3u,32u,31u,3u,1u,31u);
    check(3u,128u,0u,2u,1u,96u);
    check(3u,8u,32u,2u,0u,0u);
    check(3u,8u,255u,2u,0u,223u);
    check(1u,224u,0u,1u,1u,192u);
    check(3u,32u,0u,0u,1u,0u);
    check(1023u,32u,0u,1023u,1u,0u);
    puts("R220 TOM SCBITOBJ HEIGHT saturation / DATA / REMAINDER PASS");
    return 0;
}
