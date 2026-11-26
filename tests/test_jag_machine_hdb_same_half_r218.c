/* RE40 R218: HDB1 and HDB2 are independent 11-bit comparators.  Two
 * ordered distinct starts can occur in the SAME HC10 half.  Verify the
 * synthetic 68000 cartridge's actual Object Processor traversal, two
 * buffer-bank outputs, 16/24-bit video, and the CC4 branch latch.
 * No proprietary ROM or Jaguar SDK code is embedded. */
#include "jaguar/jag_machine.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t rom[0x5000];
static void w16(uint32_t a,uint16_t v)
{rom[a]=(uint8_t)(v>>8);rom[a+1]=(uint8_t)v;}
static void w32(uint32_t a,uint32_t v)
{w16(a,(uint16_t)(v>>16));w16(a+2u,(uint16_t)v);}
static void phrase(uint32_t a,uint64_t p)
{jagm_write32(&m,a,(uint32_t)(p>>32));jagm_write32(&m,a+4u,(uint32_t)p);}

static void check(unsigned second_half,unsigned rgb24,unsigned second_enabled)
{
    /* BRANCH remains phrase-aligned; the following BITOBJ is 16-aligned. */
    const uint32_t list=0x1008u,first=0x1010u,stop1=0x1020u;
    const uint32_t other=0x1040u,stop2=0x1050u;
    const uint32_t src1=0x3000u,src2=0x3010u;
    const uint16_t red=0xf800u,blue=0x001fu;
    uint64_t branch,bmp1,bmp2,desc;
    unsigned hdb1=(second_half?0x0420u:0x0020u);
    unsigned hdb2=second_enabled?(second_half?0x0440u:0x0060u):hdb1;
    unsigned hde=(second_half?0x0470u:0x0070u);
    unsigned left=(second_half?32u:64u)/4u;
    unsigned full=(hde-hdb1)/4u;
    uint32_t expect;
    memset(rom,0,sizeof rom);
    w32(0x404u,0x00802000u);
    w16(0x2000u,0x60feu); /* 68000 BRA.S -2 */
    assert(jagm_init(&m,rom,sizeof rom)==0);
    /* CC4 is only taken when HC10=1; therefore each of the two
     * same-half starts must follow the SAME physical sprite list. */
    branch=3u|((uint64_t)0x7ffu<<3)|((uint64_t)4u<<14)|
           ((uint64_t)(other>>3)<<24);
    bmp1=((uint64_t)2u<<14)|((uint64_t)(stop1>>3)<<24)|
         ((uint64_t)(src1>>3)<<43);
    bmp2=((uint64_t)2u<<14)|((uint64_t)(stop2>>3)<<24)|
         ((uint64_t)(src2>>3)<<43);
    desc=2u|((uint64_t)(rgb24?5u:4u)<<12)|
         ((uint64_t)1u<<15)|((uint64_t)1u<<28);
    phrase(list,branch);
    phrase(first,bmp1);phrase(first+8u,desc);phrase(stop1,4u);
    phrase(other,bmp2);phrase(other+8u,desc);phrase(stop2,4u);
    if(rgb24){
        jagm_write32(&m,src1,0x1234abcdu);jagm_write32(&m,src1+4u,0x1234abcdu);
        jagm_write32(&m,src2,0xe0f00001u);jagm_write32(&m,src2+4u,0xe0f00001u);
        expect=second_half?avp_jaguar_vmode_rgb24(0xe0f0u,0x0001u):
                            avp_jaguar_vmode_rgb24(0x1234u,0xabcdu);
    }else{
        jagm_write32(&m,src1,((uint32_t)red<<16)|red);
        jagm_write32(&m,src1+4u,((uint32_t)red<<16)|red);
        jagm_write32(&m,src2,((uint32_t)blue<<16)|blue);
        jagm_write32(&m,src2+4u,((uint32_t)blue<<16)|blue);
        expect=avp_jaguar_vmode_rgb16(second_half?blue:red);
    }
    jagm_write16(&m,0xf0002eu,127u); /* 128 HC clocks per half */
    jagm_write16(&m,0xf0003eu,3u);   /* one visible line, 4 half-lines */
    jagm_write16(&m,0xf00046u,0u);
    jagm_write16(&m,0xf00048u,2u);
    jagm_write16(&m,0xf00038u,(uint16_t)hdb1);
    jagm_write16(&m,0xf0003au,(uint16_t)hdb2);
    jagm_write16(&m,0xf0003cu,(uint16_t)hde);
    jagm_write16(&m,0xf00020u,(uint16_t)list);
    jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf00028u,rgb24?0x0603u:0x0607u);
    jagm_run_frames(&m,1u);
    assert(m.op_unsupported_mask==0u);
    assert(m.frame_h==1u);
    assert(m.frame_w==full);
    assert(m.op_lines==(second_enabled?2u:1u));
    assert(m.op_objects==(second_enabled?6u:3u));
    assert(m.frame[2u]==expect);
    if(second_enabled){
        assert(m.frame[left+2u]==expect);
        assert(m.frame[left-1u]==0u);
        assert(m.frame[left+1u]==0u);
    }else{
        assert(m.frame[left+2u]==0u);
    }
    assert((m.int1_pending&4u)==0u);
}

/* The first pass must precede a CPU source-data write and the second
 * pass must follow it, even though both HDB comparators are in HC10=0. */
static void check_cpu_write_between_starts(void)
{
    const uint16_t old=0xf800u,updated=0x001fu;
    uint64_t bitmap,desc;
    unsigned pc;
    memset(rom,0,sizeof rom);
    w32(0x404u,0x00802000u);
    pc=0x2000u;
    for(unsigned i=0;i<2u;i++,pc+=2u)w16(pc,0x4e71u); /* NOP */
    w16(pc,0x23fcu);w32(pc+2u,((uint32_t)updated<<16)|updated);
    w32(pc+6u,0x3000u);w16(pc+10u,0x60feu);
    assert(jagm_init(&m,rom,sizeof rom)==0);
    jagm_write32(&m,0x3000u,((uint32_t)old<<16)|old);
    jagm_write32(&m,0x3004u,((uint32_t)old<<16)|old);
    bitmap=((uint64_t)2u<<14)|((uint64_t)(0x1010u>>3)<<24)|
           ((uint64_t)(0x3000u>>3)<<43);
    desc=2u|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
         ((uint64_t)1u<<28);
    phrase(0x1000u,bitmap);phrase(0x1008u,desc);phrase(0x1010u,4u);
    jagm_write16(&m,0xf0002eu,127u);
    jagm_write16(&m,0xf0003eu,3u);
    jagm_write16(&m,0xf00046u,0u);jagm_write16(&m,0xf00048u,2u);
    jagm_write16(&m,0xf00038u,16u);jagm_write16(&m,0xf0003au,112u);
    jagm_write16(&m,0xf0003cu,127u);
    jagm_write16(&m,0xf00020u,0x1000u);jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf00028u,0x0607u);
    jagm_run_frames(&m,1u);
    assert(m.op_unsupported_mask==0u);
    assert(m.op_lines==2u);
    assert(m.op_objects==4u);
    assert(m.frame_w==27u);
    assert(m.frame[2u]==avp_jaguar_vmode_rgb16(old));
    assert(m.frame[26u]==avp_jaguar_vmode_rgb16(updated));
}

int main(void)
{
    check(0u,0u,1u); /* first-half HDB1/HDB2 RGB16 */
    check(1u,0u,1u); /* second-half HDB1/HDB2 RGB16, CC4 taken */
    check(0u,1u,1u); /* first-half HDB1/HDB2 RGB24 */
    check(1u,1u,1u); /* second-half HDB1/HDB2 RGB24 */
    check(0u,0u,0u); /* equal HDB registers must trigger only once */
    check(1u,0u,0u);
    check_cpu_write_between_starts();
    puts("R218 TOM ordered same-half HDB1/HDB2 RGB16/RGB24 and CC4 PASS");
    return 0;
}
