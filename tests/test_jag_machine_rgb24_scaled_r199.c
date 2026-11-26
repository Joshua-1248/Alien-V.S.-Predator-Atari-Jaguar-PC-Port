/* RE40 R199: RGB24 scaled TOM objects in the Jaguar reference machine.
 * Synthetic 68000 cartridge sets up TOM registers and a real Object List;
 * inspect the complete output frame, not a host-side pixel helper.
 * Technical Reference Rev.8: two 32-bit source slots per phrase, two
 * 16-bit line-buffer words per physical RGB24 pixel; JDEV0002 limits
 * RGB24 horizontal scaling to HSCALE=0x20 (one output pixel/source pixel).
 * Source, write-back, reflection, pitch and vertical step are exercised. */
#include "jaguar/jag_machine.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t cart[0x5000];
static unsigned pc;
static JagMachine m;
static const uint32_t sample[12]={
    0x341200abu,0x88800044u,0xe0f00001u,0x567800efu,
    0x77880055u,0x12340066u,0x90120077u,0xf0f00088u,
    0x221100cdu,0x433200bau,0x654300acu,0x87540099u
};
static void w16(uint16_t v)
{ assert(pc+2u<=sizeof cart);cart[pc++]=(uint8_t)(v>>8);cart[pc++]=(uint8_t)v; }
static void w32(uint32_t v){w16((uint16_t)(v>>16));w16((uint16_t)v);}
static void movel(uint32_t imm,uint32_t addr)
{ w16(0x23fcu);w32(imm);w32(addr); }
static void movew(uint16_t imm,uint32_t addr)
{ w16(0x33fcu);w16(imm);w32(addr); }

static uint32_t rgb(unsigned i)
{
    uint32_t v=sample[i];
    return avp_jaguar_vmode_rgb24((uint16_t)(v>>16),(uint16_t)v);
}
static void run_case(unsigned scaled,unsigned height,unsigned hscale,
                     unsigned vscale,unsigned pitch,unsigned dwidth,
                     int xpos,unsigned reflect,unsigned firstpix,
                     int invalid)
{
    const uint32_t list=0x1000u,data=0x3000u;
    const uint32_t stop=list+(scaled?24u:16u);
    uint64_t p0,p1,p2=0u;
    unsigned i,y;
    memset(cart,0,sizeof cart);
    cart[0x404]=0u;cart[0x405]=0x80u;
    cart[0x406]=0x20u;cart[0x407]=0u;
    pc=0x2000u;
    /* Three source scanlines, each with two contiguous RGB24 phrases.
     * Pitch=2 case intentionally pulls every second phrase instead. */
    for(i=0u;i<12u;i++)movel(sample[i],data+4u*i);
    p0=(uint64_t)(scaled?1u:0u)|((uint64_t)28u<<3)|
       ((uint64_t)height<<14)|((uint64_t)(stop>>3)<<24)|
       ((uint64_t)(data>>3)<<43);
    p1=((uint64_t)(xpos&0xfff))|((uint64_t)5u<<12)|
       ((uint64_t)pitch<<15)|((uint64_t)dwidth<<18)|
       ((uint64_t)2u<<28)|((uint64_t)reflect<<45)|
       ((uint64_t)firstpix<<49);
    if(scaled)p2=(uint64_t)hscale|((uint64_t)vscale<<8);
    movel((uint32_t)(p0>>32),list);
    movel((uint32_t)p0,list+4u);
    movel((uint32_t)(p1>>32),list+8u);
    movel((uint32_t)p1,list+12u);
    if(scaled){movel((uint32_t)(p2>>32),list+16u);movel((uint32_t)p2,list+20u);}
    movel(0u,stop);movel(4u,stop+4u);
    movew(28u,0xf00046u);movew(508u,0xf00048u);
    movew(0x00cbu,0xf00038u);movew(0x07ffu,0xf0003au);movew(0x0680u,0xf0003cu);
    movel(list<<16,0xf00020u); /* OLP: word swapped */
    movew(0x06c3u,0xf00028u); /* OP enabled, RGB24, PWIDTH 4 */
    w16(0x60feu);
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_run_frames(&m,1u);
    assert(m.frame_w==320u);
    if(invalid){
        assert((m.op_unsupported_mask&128u)==128u);
        assert(m.frame[10u]==0u);
        return;
    }
    assert(m.op_unsupported_mask==0u);
    for(y=0u;y<height;y++){
        unsigned source_base=y*dwidth*2u;
        for(i=0u;i<4u;i++){
            int x=xpos+(reflect?-(int)i:(int)i);
            unsigned src=source_base+(i/2u)*(pitch*2u)+(i%2u);
            if(x>=0&&x<320){
                assert(src<12u);
                if(m.frame[y*JAGM_MAX_W+(unsigned)x]!=rgb(src)){
                    fprintf(stderr,"RGB24 %s y=%u x=%d expected=%06x actual=%06x\n",
                            scaled?"scaled":"unscaled",y,x,rgb(src),
                            m.frame[y*JAGM_MAX_W+(unsigned)x]);
                    assert(0);
                }
            }
        }
        assert(m.frame[y*JAGM_MAX_W+18u]==0u);
    }
    {const uint64_t np=((uint64_t)jagm_read32(&m,list)<<32)|jagm_read32(&m,list+4u);
     assert(((np>>14)&0x3ffu)==0u);
     assert((((np>>43)&0x1fffffu)<<3)==data+height*dwidth*8u);}
    if(scaled){
        const uint64_t q=((uint64_t)jagm_read32(&m,list+16u)<<32)|
                          jagm_read32(&m,list+20u);
        assert(((q>>16)&0xffu)==0u); /* VSCALE=1.0; remainder starts at zero */
    }
}
int main(void)
{
    /* Unscaled baseline and exact corresponding scaled 1:1 output. */
    run_case(0u,1u,0u,0u,1u,2u,10,0u,0u,0);
    run_case(1u,1u,0x20u,0x20u,1u,2u,10,0u,0u,0);
    /* 1x horizontal / 1x vertical: source DATA/HEIGHT advances two lines. */
    run_case(1u,2u,0x20u,0x20u,1u,2u,10,0u,0u,0);
    /* Mirrored source and clipped destination use PHYSICAL RGB24 pixels. */
    run_case(1u,1u,0x20u,0x20u,1u,2u,13,1u,0u,0);
    run_case(1u,1u,0x20u,0x20u,1u,2u,1,1u,0u,0);
    /* Phrase pitch can skip an entire intervening 64-bit phrase. */
    run_case(1u,1u,0x20u,0x20u,2u,2u,10,0u,0u,0);
    /* Jaguar errata: non-1x RGB24 HSCALE is NOT silently approximated. */
    run_case(1u,1u,0x21u,0x20u,1u,2u,10,0u,0u,1);
    puts("RE40 R199 RGB24 1x SCBITOBJ / TOM physical line buffer PASS");
    return 0;
}
