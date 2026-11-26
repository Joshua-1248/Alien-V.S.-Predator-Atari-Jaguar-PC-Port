/* RE40 R209: synthetic Jaguar cartridge executes real 68000 writes,
 * TOM BRANCH CC4, two bitmap objects, and both Object Processor starts.
 * HDB1 is in the first HC10 half; HDB2 is in the second.  Each start
 * displays its own physical line-buffer bank at the correct screen offset.
 * No commercial cartridge bytes are embedded. */
#include "jaguar/jag_machine.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t cart[0x5000];
static JagMachine m;
static unsigned pc;
static void w16(uint16_t v)
{assert(pc+2u<=sizeof cart);cart[pc++]=(uint8_t)(v>>8);cart[pc++]=(uint8_t)v;}
static void w32(uint32_t v){w16((uint16_t)(v>>16));w16((uint16_t)v);}
static void ml(uint32_t v,uint32_t a){w16(0x23fcu);w32(v);w32(a);}
static void mw(uint16_t v,uint32_t a){w16(0x33fcu);w16(v);w32(a);}
static void phrase(uint32_t a,uint64_t v)
{ml((uint32_t)(v>>32),a);ml((uint32_t)v,a+4u);}

static void check(unsigned hdb2,unsigned expect_second,unsigned rgb24)
{
    /* BRANCH occupies one phrase; put its next BITOBJ on a 16-byte boundary. */
    const uint32_t list=0x1008u,first=list+8u,stop1=first+16u;
    const uint32_t second=0x1040u,stop2=second+16u;
    const uint32_t src1=0x3000u,src2=0x3008u;
    const uint16_t red=0xf800u,blue=0x001fu;
    uint64_t branch,bitmap1,bitmap2,descriptor;
    uint32_t color1,color2;
    unsigned x;
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0;
    pc=0x2000u;
    /* The branch skips to the second-half-only bitmap if HC10 is set. */
    branch=3u|((uint64_t)0x7ffu<<3)|((uint64_t)4u<<14)|
           ((uint64_t)(second>>3)<<24);
    bitmap1=((uint64_t)28u<<3)|((uint64_t)1u<<14)|
            ((uint64_t)(stop1>>3)<<24)|((uint64_t)(src1>>3)<<43);
    bitmap2=((uint64_t)29u<<3)|((uint64_t)1u<<14)|
            ((uint64_t)(stop2>>3)<<24)|((uint64_t)(src2>>3)<<43);
    descriptor=10u|((uint64_t)(rgb24?5u:4u)<<12)|
               ((uint64_t)1u<<15)|((uint64_t)1u<<28);
    phrase(list,branch);
    phrase(first,bitmap1);phrase(first+8u,descriptor);
    phrase(stop1,4u);
    phrase(second,bitmap2);phrase(second+8u,descriptor);
    phrase(stop2,4u);
    if(rgb24){
        ml(0x341200abu,src1);ml(0x341200abu,src1+4u);
        ml(0xe0f00001u,src2);ml(0xe0f00001u,src2+4u);
        color1=avp_jaguar_vmode_rgb24(0x3412u,0x00abu);
        color2=avp_jaguar_vmode_rgb24(0xe0f0u,0x0001u);
    }else{
        ml(((uint32_t)red<<16)|red,src1);
        ml(((uint32_t)red<<16)|red,src1+4u);
        ml(((uint32_t)blue<<16)|blue,src2);
        ml(((uint32_t)blue<<16)|blue,src2+4u);
        color1=avp_jaguar_vmode_rgb16(red);
        color2=avp_jaguar_vmode_rgb16(blue);
    }
    /* HP+1=128 clocks per half; first start=32; second start=192;
     * HDE=240, PWIDTH=4 => output left 40 + right 12 pixels. */
    mw(127u,0xf0002eu);
    mw(28u,0xf00046u);mw(30u,0xf00048u);
    mw(0x0020u,0xf00038u);
    mw((uint16_t)hdb2,0xf0003au);
    mw(0x0470u,0xf0003cu);
    ml(list<<16,0xf00020u);
    mw(rgb24?0x06c3u:0x06c7u,0xf00028u); /* PWIDTH=4, OP on */
    w16(0x60feu);
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_run_frames(&m,1u);
    assert(m.frame_h==1u);
    assert(m.frame_w==52u);
    assert(m.op_unsupported_mask==0u);
    assert(m.frame[10u]==color1);
    for(x=14u;x<40u;x++)assert(m.frame[x]==0u);
    if(expect_second){
        assert(m.frame[50u]==color2);
        assert(m.frame[49u]==0u);
        assert(m.op_lines==2u);
        assert(m.op_objects==6u); /* BRANCH, BITMAP, STOP per half */
    }else{
        assert(m.frame[50u]==0u);
        assert(m.op_lines==1u);
        assert(m.op_objects==3u);
    }
    assert((m.int1_pending&4u)==0u);
}

int main(void)
{
    check(0x07ffu,0u,0u);    /* second comparator outside line */
    check(0x0020u,0u,0u);     /* duplicate HDB1, not a split */
    check(0x0470u,0u,0u);     /* second start at HDE, not visible */
    check(0x0440u,1u,0u);     /* authentic later-half RGB16 OP start */
    check(0x07ffu,0u,1u);    /* RGB24 baseline with HDB2 out of line */
    check(0x0440u,1u,1u);     /* RGB24 uses two words/pixel per bank */
    puts("R209 TOM HDB1/HDB2 split RGB16/RGB24, CC4, separate banks PASS");
    return 0;
}
