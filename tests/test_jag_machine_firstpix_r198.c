/* RE40 R198: Synthetic Jaguar 68000 cartridge -> TOM Object Processor.
 * This independently exercises the reference machine's OP execution, not
 * merely the standalone compositor helper; no proprietary ROM is included.
 * Rev.8 OP BITOBJ: FIRSTPIX selects an initial SOURCE PIXEL PAIR.
 * Rev.8 OP SCBITOBJ: FIRSTPIX selects an initial SINGLE SOURCE PIXEL. */
#include "jaguar/jag_machine.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t cart[0x4000];
static unsigned pc;
static JagMachine m;
static void w16(uint16_t v){assert(pc+2<=sizeof cart);cart[pc++]=(uint8_t)(v>>8);cart[pc++]=(uint8_t)v;}
static void w32(uint32_t v){w16((uint16_t)(v>>16));w16((uint16_t)v);}
static void movel(uint32_t imm,uint32_t addr){w16(0x23FCu);w32(imm);w32(addr);}
static void movew(uint16_t imm,uint32_t addr){w16(0x33FCu);w16(imm);w32(addr);}
static const uint16_t pixels[8]={0xf800u,0x07e0u,0x001fu,0xffffu,
                                  0x7c00u,0x03e0u,0x0010u,0xc618u};

static void run_case(unsigned scaled,unsigned firstpix,unsigned offset,
                     unsigned exposed)
{
    const uint32_t list=0x1000u,data=0x2000u;
    const unsigned words=scaled?6u:4u;
    const uint32_t stop=list+words*4u;
    uint64_t p0,p1,p2=0u;
    unsigned i;
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0;
    pc=0x2000u;
    for(i=0u;i<4u;i++)movel(((uint32_t)pixels[2u*i]<<16)|pixels[2u*i+1u],data+4u*i);
    p0=(uint64_t)scaled|((uint64_t)28u<<3)|((uint64_t)1u<<14)|
       ((uint64_t)(stop>>3)<<24)|((uint64_t)(data>>3)<<43);
    p1=10u|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
       ((uint64_t)2u<<28)|((uint64_t)firstpix<<49);
    if(scaled)p2=0x00002020u; /* 1.0 x/y scaling, remainder=0 */
    movel((uint32_t)(p0>>32),list);movel((uint32_t)p0,list+4u);
    movel((uint32_t)(p1>>32),list+8u);movel((uint32_t)p1,list+12u);
    if(scaled){movel((uint32_t)(p2>>32),list+16u);movel((uint32_t)p2,list+20u);}
    movel(0u,stop);movel(4u,stop+4u); /* OP STOP */
    movew(28u,0xF00046u);movew(508u,0xF00048u);
    movew(0x00CBu,0xF00038u);movew(0x07FFu,0xF0003Au);movew(0x0680u,0xF0003Cu);
    movel(list<<16,0xF00020u); /* OLP word swap */
    movew(0x06C7u,0xF00028u); /* RGB16, PWIDTH 3, BGEN */
    w16(0x60FEu);
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_run_frames(&m,1u);
    assert(m.frame_w==320u);
    assert(m.frame[9u]==0u);
    for(i=0u;i<exposed;i++)
        assert(m.frame[10u+i]==avp_jaguar_vmode_rgb16(pixels[i+offset]));
    assert(m.frame[10u+exposed]==0u);
    assert(m.op_unsupported_mask==0u);
    /* Regardless of FIRSTPIX, the OP consumes exactly one source line. */
    {uint64_t updated=((uint64_t)jagm_read32(&m,list)<<32)|jagm_read32(&m,list+4u);
     assert(((updated>>14)&0x3ffu)==0u);
     assert((((updated>>43)&0x1fffffu)<<3)==data);}
}

static void run_4bpp_case(unsigned scaled,unsigned firstpix,unsigned hscale,
                          unsigned skip,unsigned count,unsigned step)
{
    const uint32_t list=0x1000u,data=0x2000u;
    const unsigned words=scaled?6u:4u;
    const uint32_t stop=list+words*4u;
    uint64_t p0,p1,p2=0u;
    unsigned i;
    memset(cart,0,sizeof cart);
    cart[0x404]=0u;cart[0x405]=0x80u;cart[0x406]=0x20u;cart[0x407]=0u;
    pc=0x2000u;
    /* Sixteen 4-bit pixels: 0,1,...15, stored in full 64-bit phrase. */
    movel(0x01234567u,data);movel(0x89abcdefu,data+4u);
    /* The OP indexes the actual TOM CLUT for a 4bpp source. */
    for(i=0u;i<16u;i++)movew(pixels[i%8u],0xF00400u+2u*i);
    p0=(uint64_t)scaled|((uint64_t)28u<<3)|((uint64_t)1u<<14)|
       ((uint64_t)(stop>>3)<<24)|((uint64_t)(data>>3)<<43);
    p1=10u|((uint64_t)2u<<12)|((uint64_t)1u<<15)|
       ((uint64_t)1u<<28)|((uint64_t)firstpix<<49);
    if(scaled)p2=(uint64_t)hscale|((uint64_t)32u<<8);
    movel((uint32_t)(p0>>32),list);movel((uint32_t)p0,list+4u);
    movel((uint32_t)(p1>>32),list+8u);movel((uint32_t)p1,list+12u);
    if(scaled){movel((uint32_t)(p2>>32),list+16u);movel((uint32_t)p2,list+20u);}
    movel(0u,stop);movel(4u,stop+4u);
    movew(28u,0xF00046u);movew(508u,0xF00048u);
    movew(0x00CBu,0xF00038u);movew(0x07FFu,0xF0003Au);movew(0x0680u,0xF0003Cu);
    movel(list<<16,0xF00020u);movew(0x06C7u,0xF00028u);
    w16(0x60FEu);
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_run_frames(&m,1u);
    assert(m.frame[9u]==0u);
    for(i=0u;i<count;i++){
        unsigned src=skip+i*step;
        assert(src<16u);
        assert(m.frame[10u+i]==avp_jaguar_vmode_rgb16(pixels[src%8u]));
    }
    assert(m.frame[10u+count]==0u);
    assert(m.op_unsupported_mask==0u);
}

int main(void)
{
    /* Control: no source skip, all eight RGB16 pixels drawn. */
    run_case(0u,0x00u,0u,8u);
    /* Unscaled RGB16: FIRSTPIX $20 skips exactly one pixel PAIR. */
    run_case(0u,0x20u,2u,6u);
    /* Scaled RGB16: FIRSTPIX $10 skips exactly one source pixel. */
    run_case(1u,0x10u,1u,7u);
    /* Scaled RGB16: FIRSTPIX $30 skips exactly three source pixels. */
    run_case(1u,0x30u,3u,5u);
    /* Indexed 4bpp with an authentic TOM CLUT and pixel pairs.  Lower
     * FIRSTPIX bits select no half-pixel or half-pair. */
    run_4bpp_case(0u,0x18u,32u,6u,10u,1u);
    run_4bpp_case(0u,0x1bu,32u,6u,10u,1u);
    run_4bpp_case(1u,0x09u,16u,2u,7u,2u);
    puts("RE40 R198 Jaguar reference OP FIRSTPIX synthetic cartridge PASS");
    return 0;
}
