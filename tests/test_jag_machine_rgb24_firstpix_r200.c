/* RE40 R200: Jaguar TOM SCBITOBJ RGB24 FIRSTPIX source selection.
 * Synthetic 68000 cartridge writes a real object list.  Manual Rev.8
 * pp.18-22: FIRSTPIX on scaled objects selects a single source pixel.
 * RGB24 uses two source pixels/phrase; its depth-five high FIRSTPIX bit
 * selects the second pixel.  Bits below DEPTH do not shift a pixel.
 * Confirm source-pixel skip, 1x scale, reflect/clipping, and write-back.
 */
#include "jaguar/jag_machine.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t cart[0x5000];
static JagMachine m;
static unsigned pc;
static const uint32_t pixels[4]={
    0x121100a1u,0x34bc00b2u,0x567800c3u,0xde9a00d4u
};
static void w16(uint16_t v)
{ assert(pc+2u<=sizeof cart);cart[pc++]=(uint8_t)(v>>8);cart[pc++]=(uint8_t)v; }
static void w32(uint32_t v){w16((uint16_t)(v>>16));w16((uint16_t)v);}
static void movel(uint32_t v,uint32_t a){w16(0x23fcu);w32(v);w32(a);}
static void movew(uint16_t v,uint32_t a){w16(0x33fcu);w16(v);w32(a);}
static uint32_t rgb(unsigned i)
{ return avp_jaguar_vmode_rgb24((uint16_t)(pixels[i]>>16),(uint16_t)pixels[i]); }

static void run_case(unsigned firstpix,int xpos,unsigned reflect)
{
    const uint32_t list=0x1000u,data=0x3000u,stop=list+24u;
    const unsigned first_skip=firstpix>>5;
    const uint64_t p0=1u|((uint64_t)28u<<3)|((uint64_t)1u<<14)|
        ((uint64_t)(stop>>3)<<24)|((uint64_t)(data>>3)<<43);
    const uint64_t p1=((uint64_t)(xpos&0xfff))|((uint64_t)5u<<12)|
        ((uint64_t)1u<<15)|((uint64_t)1u<<18)|
        ((uint64_t)2u<<28)|((uint64_t)reflect<<45)|
        ((uint64_t)firstpix<<49);
    const uint64_t p2=0x2020u; /* HSCALE and VSCALE = 1.0 */
    unsigned x,i;
    memset(cart,0,sizeof cart);
    cart[0x404]=0u;cart[0x405]=0x80u;cart[0x406]=0x20u;cart[0x407]=0u;
    pc=0x2000u;
    for(i=0u;i<4u;i++)movel(pixels[i],data+i*4u);
    movel((uint32_t)(p0>>32),list);movel((uint32_t)p0,list+4u);
    movel((uint32_t)(p1>>32),list+8u);movel((uint32_t)p1,list+12u);
    movel((uint32_t)(p2>>32),list+16u);movel((uint32_t)p2,list+20u);
    movel(0u,stop);movel(4u,stop+4u);
    movew(28u,0xf00046u);movew(508u,0xf00048u);
    movew(0x00cbu,0xf00038u);movew(0x07ffu,0xf0003au);movew(0x0680u,0xf0003cu);
    movel(list<<16,0xf00020u);movew(0x06c3u,0xf00028u);
    w16(0x60feu);
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_run_frames(&m,1u);
    assert(m.frame_w==320u);
    assert(m.op_unsupported_mask==0u);
    for(x=0;x<320u;x++){
        uint32_t expected=0u;
        for(i=0u;i<4u-first_skip;i++){
            const int dest=xpos+(reflect?-(int)i:(int)i);
            if(dest==(int)x)expected=rgb(first_skip+i);
        }
        if(m.frame[x]!=expected){
            fprintf(stderr,"FIRSTPIX=%02x reflect=%u x=%u expected=%06x actual=%06x\n",
                    firstpix,reflect,x,expected,m.frame[x]);
            assert(0);
        }
    }
    {const uint64_t updated=((uint64_t)jagm_read32(&m,list)<<32)|jagm_read32(&m,list+4u);
     assert(((updated>>14)&0x3ffu)==0u);
     assert((((updated>>43)&0x1fffffu)<<3)==data+8u);}
}
int main(void)
{
    run_case(0x00u,10,0u); /* baseline: all four pixels */
    run_case(0x1fu,10,0u); /* lower five bits are subpixel, no skip */
    run_case(0x20u,10,0u); /* skip first pixel of first phrase */
    run_case(0x3fu,10,0u); /* same skip, low five bits insignificant */
    run_case(0x20u,1,1u);  /* reflect and negative-X clipping */
    run_case(0x20u,-1,0u); /* left-of-screen clip still advances source */
    puts("RE40 R200 Jaguar TOM RGB24 scaled FIRSTPIX PASS");
    return 0;
}
