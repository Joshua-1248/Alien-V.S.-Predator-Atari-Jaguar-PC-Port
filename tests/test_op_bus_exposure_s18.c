/* Synthetic cartridge: JDEV0001's bitmap depth/RELEASE exposure counts.
 * Counters are NOT a Jaguar bus scheduler and never inject audio errors. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char cart[0x6000];static unsigned pc;
static JagMachine M;
static void w16(uint16_t x){cart[pc++]=(uint8_t)(x>>8);cart[pc++]=(uint8_t)x;}
static void w32(uint32_t x){w16((uint16_t)(x>>16));w16((uint16_t)x);}
static void movel(uint32_t x,uint32_t a){w16(0x23FCu);w32(x);w32(a);}
static void movew(uint16_t x,uint32_t a){w16(0x33FCu);w16(x);w32(a);}
static void obj(uint32_t a,uint32_t link,int release,int rmw,int x)
{
    uint64_t p0= ((uint64_t)28u<<3)|((uint64_t)4u<<14)|
                 ((uint64_t)(link>>3)<<24)|((uint64_t)(0x3000u>>3)<<43);
    uint64_t p1=(uint64_t)(unsigned)x|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
                 ((uint64_t)4u<<28)|((uint64_t)(unsigned)release<<48)|((uint64_t)(unsigned)rmw<<46);
    movel((uint32_t)(p0>>32),a);movel((uint32_t)p0,a+4);
    movel((uint32_t)(p1>>32),a+8);movel((uint32_t)p1,a+12);
}
static void invalid_scaled(unsigned hs,unsigned vs,uint32_t expected)
{
    /* TYPE1, DEPTH5; JDEV0002 OP items 2/3. A malformed scale value
       must be surfaced as unsupported instead of an invented image. */
    uint64_t p0,p1,p2;
    memset(cart,0,sizeof cart);
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0;
    pc=0x2000;
    p0=1u|((uint64_t)28u<<3)|((uint64_t)4u<<14)|
       ((uint64_t)(0x1018u>>3)<<24)|((uint64_t)(0x3000u>>3)<<43);
    p1=10u|((uint64_t)5u<<12)|((uint64_t)1u<<15)|((uint64_t)1u<<28);
    p2=(uint64_t)hs|((uint64_t)vs<<8);
    movel((uint32_t)(p0>>32),0x1000);movel((uint32_t)p0,0x1004);
    movel((uint32_t)(p1>>32),0x1008);movel((uint32_t)p1,0x100C);
    movel((uint32_t)(p2>>32),0x1010);movel((uint32_t)p2,0x1014);
    movel(0u,0x1018);movel(4u,0x101C);
    movew(28u,0xF00046u);movew(508u,0xF00048u);
    movew(0x00CBu,0xF00038u);movew(0x0680u,0xF0003Cu);
    movel(0x10000000u,0xF00020u);
    movew(0x06C7u,0xF00028u);w16(0x60FEu);
    assert(jagm_init(&M,cart,sizeof cart)==0);
    jagm_run_frames(&M,1u);
    assert((M.op_unsupported_mask&expected)==expected);
}

int main(void)
{
    cart[0x404]=0x00u;cart[0x405]=0x80u;cart[0x406]=0x20u;cart[0x407]=0x00u;
    pc=0x2000;
    for(unsigned i=0;i<8;i++)movel(0xF800F800u,0x3000u+4u*i);
    obj(0x1000u,0x1010u,1,1,10);obj(0x1010u,0x1020u,0,0,50);
    movel(0u,0x1020u);movel(4u,0x1024u);
    movew(28u,0xF00046u);movew(508u,0xF00048u);
    movew(0x00CBu,0xF00038u);movew(0x0680u,0xF0003Cu);
    movel(0x10000000u,0xF00020u);
    movew(0x06C7u,0xF00028u);w16(0x60FEu);
    assert(jagm_init(&M,cart,sizeof cart)==0);
    jagm_run_frames(&M,1u);
    assert(M.op_deep_bitmap_visits==8u);
    assert(M.op_deep_release_visits==4u);
    assert(M.op_rmw_neighbor_visits==4u); /* adjacent active RMW => SDK risk */
    /* First bitmap is RMW; its CRY blend is intentionally not asserted here. */
    assert(M.frame[0*JAGM_MAX_W+51u]==0xF80000u);
    invalid_scaled(0x20u,0xE1u,64u); /* VSCALE greater than 7.0 */
    invalid_scaled(0x21u,0x40u,128u); /* RGB24 HSCALE other than 1.0 */
    puts("OP/DSP bus exposure, RMW adjacency and scale-bound fixtures passed");
    return 0;
}
