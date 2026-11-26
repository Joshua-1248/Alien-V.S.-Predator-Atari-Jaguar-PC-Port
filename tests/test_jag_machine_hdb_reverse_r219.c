/* RE40 R219: TOM HDB1 and HDB2 are independent HC comparators.  They
 * trigger the Object Processor in CLOCK order, not register-number order.
 * Cartridge-backed test; no proprietary code/assets distributed. */
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

/* first/second are PHYSICAL CLOCK positions.  hdb1 and hdb2 may be in
 * either order, absent (outside the 256-clock physical line), or equal. */
static void check(uint16_t hdb1,uint16_t hdb2,uint16_t hde,
                  unsigned rgb24,unsigned expected_starts)
{
    const uint16_t red=0xf800u,blue=0x001fu;
    const uint32_t first=0x3000u,second=0x3008u;
    uint64_t bitmap,desc;
    unsigned starts[2],count=0u;
    unsigned p1=(hdb1&0x3ffu)+((hdb1&0x400u)?128u:0u);
    unsigned p2=(hdb2&0x3ffu)+((hdb2&0x400u)?128u:0u);
    unsigned he=(hde&0x3ffu)+((hde&0x400u)?128u:0u);
    unsigned left,w;
    uint32_t oldcolor,newcolor;
    memset(rom,0,sizeof rom);
    w32(0x404u,0x00802000u);
    w16(0x2000u,0x60feu);
    assert(jagm_init(&m,rom,sizeof rom)==0);
    bitmap=((uint64_t)6u<<14)|((uint64_t)(0x1010u>>3)<<24)|
           ((uint64_t)(first>>3)<<43);
    desc=2u|((uint64_t)(rgb24?5u:4u)<<12)|
         ((uint64_t)1u<<15)|((uint64_t)1u<<18)|
         ((uint64_t)1u<<28);
    phrase(0x1000u,bitmap);phrase(0x1008u,desc);phrase(0x1010u,4u);
    if(rgb24){
        jagm_write32(&m,first,0x1234abcdu);jagm_write32(&m,first+4u,0x1234abcdu);
        jagm_write32(&m,second,0xe0f00001u);jagm_write32(&m,second+4u,0xe0f00001u);
        oldcolor=avp_jaguar_vmode_rgb24(0x1234u,0xabcdu);
        newcolor=avp_jaguar_vmode_rgb24(0xe0f0u,0x0001u);
    }else{
        jagm_write32(&m,first,((uint32_t)red<<16)|red);
        jagm_write32(&m,first+4u,((uint32_t)red<<16)|red);
        jagm_write32(&m,second,((uint32_t)blue<<16)|blue);
        jagm_write32(&m,second+4u,((uint32_t)blue<<16)|blue);
        oldcolor=avp_jaguar_vmode_rgb16(red);
        newcolor=avp_jaguar_vmode_rgb16(blue);
    }
    if((hdb1&0x3ffu)<128u&&p1<256u&&p1<he)starts[count++]=p1;
    if((hdb2&0x3ffu)<128u&&p2<256u&&p2<he&&p2!=p1)starts[count++]=p2;
    if(count==2u&&starts[1]<starts[0]){
        unsigned swap=starts[0];starts[0]=starts[1];starts[1]=swap;
    }
    assert(count==expected_starts);
    assert(count>0u);
    left=count==2u?(starts[1]-starts[0])/4u:0u;
    w=(he-starts[0])/4u;
    jagm_write16(&m,0xf0002eu,127u);
    jagm_write16(&m,0xf0003eu,3u);
    jagm_write16(&m,0xf00046u,0u);jagm_write16(&m,0xf00048u,2u);
    jagm_write16(&m,0xf00038u,hdb1);jagm_write16(&m,0xf0003au,hdb2);
    jagm_write16(&m,0xf0003cu,hde);
    jagm_write16(&m,0xf00020u,0x1000u);jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf00028u,rgb24?0x0603u:0x0607u);
    jagm_run_frames(&m,1u);
    assert(m.op_unsupported_mask==0u);
    assert(m.op_lines==count);
    assert(m.op_objects==count*2u);
    assert(m.frame_h==1u);
    assert(m.frame_w==w);
    assert(m.frame[2u]==oldcolor);
    if(count==2u){
        assert(m.frame[left+2u]==newcolor);
        assert(m.frame[left-1u]==0u);
    }
}

/* The 68000 update falls between HDB2 (earlier) and HDB1 (later).
 * The first pass must see old pixels, the second pass the new pixels. */
static void check_cpu_write_between_reverse_starts(void)
{
    const uint16_t old=0xf800u,updated=0x001fu;
    uint64_t bitmap,desc;
    unsigned pc;
    memset(rom,0,sizeof rom);
    w32(0x404u,0x00802000u);
    pc=0x2000u;
    for(unsigned i=0;i<2u;i++,pc+=2u)w16(pc,0x4e71u);
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
    jagm_write16(&m,0xf00038u,112u); /* HDB1 second */
    jagm_write16(&m,0xf0003au,16u);  /* HDB2 first */
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
    for(unsigned rgb24=0;rgb24<2u;rgb24++){
        check(0x0040u,0x0010u,0x0060u,rgb24,2u); /* reverse in HC10=0 */
        check(0x0040u,0x0000u,0x0060u,rgb24,2u); /* HDB2=0 is a comparator match */
        check(0x0450u,0x0410u,0x0470u,rgb24,2u); /* reverse in HC10=1 */
        check(0x0420u,0x0010u,0x0460u,rgb24,2u); /* reverse across halves */
        check(0x07ffu,0x0020u,0x0070u,rgb24,1u); /* HDB2 alone, first half */
        check(0x07ffu,0x0430u,0x0460u,rgb24,1u); /* HDB2 alone, second half */
        check(0x0020u,0x07ffu,0x0070u,rgb24,1u); /* HDB1 alone */
        check(0x00e0u,0x0020u,0x0470u,rgb24,1u); /* HDB1 low clock >= HP+1, HDB2 only */
        check(0x0020u,0x0020u,0x0070u,rgb24,1u); /* equal registers */
    }
    check_cpu_write_between_reverse_starts();
    puts("R219 TOM HDB comparator chronological order / single HDB2 PASS");
    return 0;
}
