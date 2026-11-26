/* RE42 R248 -- Jaguar SCBITOBJ HEIGHT=0 with positive vertical remainder.
 * Rev.8 Scaled Bitmap: HEIGHT encodes source rows - 1.  The final
 * source line remains visible while REMAINDER > 0 (MAME jagobj.ipp,
 * process_scaled_bitmap), unlike ordinary BITOBJ's HEIGHT > 0 gate.
 * Hardware bus/field-cycle parity is not asserted. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t cart[0x3000];
static void put64(uint32_t at,uint64_t v)
{
    jagm_write32(&m,at,(uint32_t)(v>>32));
    jagm_write32(&m,at+4u,(uint32_t)v);
}
static uint64_t get64(uint32_t at)
{
    return ((uint64_t)jagm_read32(&m,at)<<32)|jagm_read32(&m,at+4u);
}
static void run_case(unsigned scaled,unsigned height,unsigned remainder,
                     unsigned expected_nonzero,unsigned expected_rem)
{
    const uint32_t list=0x1000u,stop=0x1020u,src=0x1800u;
    uint64_t p0,p1,p2,before,after;
    memset(cart,0,sizeof cart);
    cart[0x404u]=0u;cart[0x405u]=0x80u;cart[0x406u]=0x20u;
    cart[0x2000u]=0x60u;cart[0x2001u]=0xfeu; /* BRA -2 */
    assert(jagm_init(&m,cart,sizeof cart)==0);
    p0=(scaled?1u:0u)|((uint64_t)28u<<3)|((uint64_t)height<<14)|
       ((uint64_t)(stop>>3)<<24)|((uint64_t)(src>>3)<<43);
    p1=8u|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
       ((uint64_t)1u<<18)|((uint64_t)1u<<28);
    p2=0x20u|((uint64_t)0x20u<<8)|((uint64_t)remainder<<16);
    put64(list,p0);put64(list+8u,p1);
    if(scaled)put64(list+16u,p2);
    put64(stop,4u);
    put64(src,UINT64_C(0xf800f800f800f800));
    jagm_write16(&m,0xf00020u,(uint16_t)list);
    jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf0002eu,31u);
    jagm_write16(&m,0xf0003eu,31u); /* noninterlaced */
    jagm_write16(&m,0xf00046u,28u);
    jagm_write16(&m,0xf00048u,30u);
    jagm_write16(&m,0xf00038u,4u);
    jagm_write16(&m,0xf0003au,127u);
    jagm_write16(&m,0xf0003cu,20u);
    jagm_write16(&m,0xf00028u,7u); /* RGB16 */
    before=get64(list);
    jagm_run_frames(&m,1u);
    after=get64(list);
    assert(m.frames==1u);
    assert(m.op_unsupported_mask==0u);
    assert(m.op_lines==1u&&m.op_objects==2u);
    if((m.frame[8u]!=0u)!=expected_nonzero){
        fprintf(stderr,"R248 failed: scaled=%u height=%u remainder=%u "
                "frame[8]=%08x expected visible=%u\n",
                scaled,height,remainder,m.frame[8u],expected_nonzero);
        assert(0);
    }
    if(!expected_nonzero)assert(after==before);
    /* Exclude the sub-1.0 exhaustion edge (sentinel 256) from exact
     * REMAINDER assertions: its field-specific write-back is separate
     * unresolved hardware parity work, not part of this visibility fix. */
    if(scaled&&expected_rem<=255u){
        assert(((get64(list+16u)>>16)&0xffu)==expected_rem);
    }
}
/* The same active rule must drive the documented adjacent RMW hazard
 * observer: a final scaled row with positive REMAINDER is still active. */
static void check_rmw_neighbor(unsigned remainder,unsigned expected)
{
    const uint32_t list=0x1000u,second=0x1020u,stop=0x1030u,src=0x1800u;
    uint64_t p0,p1,p2,q0,q1;
    memset(cart,0,sizeof cart);
    cart[0x404u]=0u;cart[0x405u]=0x80u;cart[0x406u]=0x20u;
    cart[0x2000u]=0x60u;cart[0x2001u]=0xfeu;
    assert(jagm_init(&m,cart,sizeof cart)==0);
    p0=1u|((uint64_t)28u<<3)|((uint64_t)(second>>3)<<24)|
       ((uint64_t)(src>>3)<<43);
    p1=8u|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
       ((uint64_t)1u<<18)|((uint64_t)1u<<28)|((uint64_t)1u<<46);
    p2=0x2020u|((uint64_t)remainder<<16);
    q0=((uint64_t)28u<<3)|((uint64_t)1u<<14)|
       ((uint64_t)(stop>>3)<<24)|((uint64_t)(src>>3)<<43);
    q1=24u|((uint64_t)4u<<12)|((uint64_t)1u<<15)|
       ((uint64_t)1u<<18)|((uint64_t)1u<<28);
    put64(list,p0);put64(list+8u,p1);put64(list+16u,p2);
    put64(second,q0);put64(second+8u,q1);put64(stop,4u);
    put64(src,UINT64_C(0xf800f800f800f800));
    jagm_write16(&m,0xf00020u,(uint16_t)list);
    jagm_write16(&m,0xf00022u,0u);
    jagm_write16(&m,0xf0002eu,31u);
    jagm_write16(&m,0xf0003eu,31u);
    jagm_write16(&m,0xf00046u,28u);
    jagm_write16(&m,0xf00048u,30u);
    jagm_write16(&m,0xf00038u,4u);
    jagm_write16(&m,0xf0003au,127u);
    jagm_write16(&m,0xf0003cu,20u);
    jagm_write16(&m,0xf00028u,7u);
    jagm_run_frames(&m,1u);
    assert(m.op_unsupported_mask==0u);
    assert(m.op_lines==1u&&m.op_objects==3u);
    assert(m.op_rmw_neighbor_visits==expected);
}

int main(void)
{
    run_case(0u,0u,32u,0u,32u); /* ordinary BITOBJ height zero inactive */
    run_case(1u,0u,0u,0u,0u);  /* scaled zero remainder inactive */
    run_case(1u,0u,32u,1u,0u);/* scaled final row, one display remains */
    run_case(1u,0u,1u,1u,256u); /* sub-line REMAINDER: last row visible */
    run_case(1u,0u,31u,1u,256u);
    run_case(1u,0u,33u,1u,1u);/* >1 line of remainder */
    run_case(1u,0u,255u,1u,223u);
    run_case(1u,1u,32u,1u,0u);/* ordinary scaled active path untouched */
    check_rmw_neighbor(0u,0u);
    check_rmw_neighbor(32u,1u);
    puts("R248 SCBITOBJ final source-row active gate PASS");
    return 0;
}
