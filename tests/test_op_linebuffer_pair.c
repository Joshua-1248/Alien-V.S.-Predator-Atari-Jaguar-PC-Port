/* RE24 Sweep 07: TOM double line-buffer / BGEN output ordering.
 * Atari Jaguar TRM Rev.8 pp.21-23. Synthetic tests only, no emulator code. */
#include "jaguar/object_processor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_publish_order_and_background_clear(void)
{
    AvpJaguarOpCry16LinePair b;
    u16 out[720],*write;
    unsigned i;
    memset(&b,0x55,sizeof(b));
    assert(!avp_jaguar_op_cry16_pair_init(&b,2u,0,0));
    assert(avp_jaguar_op_cry16_pair_init(&b,0u,0x2222u,0x3333u));
    write=avp_jaguar_op_cry16_pair_write(&b);
    assert(write==b.bank[0]);
    write[0]=0x0123u;write[719]=0xabcd;
    assert(avp_jaguar_op_cry16_pair_publish(&b,out,720u,1,0x6abcu));
    assert(out[0]==0x0123u&&out[719]==0xabcdu&&out[1]==0x2222u);
    assert(b.write_bank==1u);
    for(i=0;i<720u;i++)assert(b.bank[0][i]==0x6abcu);
    assert(b.bank[1][0]==0x3333u);
    write=avp_jaguar_op_cry16_pair_write(&b);
    write[0]=0x9876u;
    assert(avp_jaguar_op_cry16_pair_publish(&b,out,4u,0,0x9999u));
    assert(out[0]==0x9876u&&out[1]==0x3333u);
    assert(b.bank[1][0]==0x9876u&&b.bank[1][719]==0x3333u);
    assert(b.write_bank==0u);
    assert(b.bank[0][0]==0x6abcu);
}

static void test_guards_leave_banks_unchanged(void)
{
    AvpJaguarOpCry16LinePair b,orig;
    u16 output[721];
    assert(avp_jaguar_op_cry16_pair_init(&b,1u,0x1234u,0x2345u));
    memcpy(&orig,&b,sizeof(b));
    assert(!avp_jaguar_op_cry16_pair_publish(&b,output,721u,0,0));
    assert(!avp_jaguar_op_cry16_pair_publish(&b,output,0u,0,0));
    assert(!avp_jaguar_op_cry16_pair_publish(&b,output,1u,2,0));
    assert(!avp_jaguar_op_cry16_pair_publish(&b,b.bank[0],2u,1,0));
    assert(!avp_jaguar_op_cry16_pair_publish(&b,b.bank[1],2u,1,0));
    assert(memcmp(&b,&orig,sizeof(b))==0);
    b.write_bank=2u;
    assert(!avp_jaguar_op_cry16_pair_write(&b));
    assert(!avp_jaguar_op_cry16_pair_publish(&b,output,1u,1,0));
}

/* Cross-component path: the same CRY16 bank returned by the two-bank
 * model is an actual destination for the tested Sweep 06 scanline pipeline.
 * This is NOT a claim of AVP retail object-list pixel parity. */
static void put32(unsigned char *m,unsigned at,unsigned v)
{
    m[at]=(unsigned char)(v>>24);
    m[at+1]=(unsigned char)(v>>16);
    m[at+2]=(unsigned char)(v>>8);
    m[at+3]=(unsigned char)v;
}
static void test_scanline_pipeline_to_physical_bank(void)
{
    AvpJaguarOpCry16LinePair b;
    u8 ram[48]={0},src[8]={0};
    u16 clut[256],scratch[720],out[720];
    AvpJaguarOpTraceStep steps[4];
    AvpJaguarOpScanlineResult r;
    unsigned i;
    /* Single CPU-style 4-bpp bitmap at 0x1000 linked to STOP at 0x1020;
     * virtual source at 0x2000 is one complete Jaguar 64-bit phrase. */
    const uint64_t p0=((uint64_t)(0x2000u>>3)<<43)|
                      ((uint64_t)(0x1020u>>3)<<24)|((uint64_t)1u<<14);
    const uint64_t p1=((uint64_t)1u<<28)|((uint64_t)1u<<18)|
                      ((uint64_t)1u<<15)|((uint64_t)2u<<12);
    put32(ram,0u,(unsigned)(p0>>32));put32(ram,4u,(unsigned)p0);
    put32(ram,8u,(unsigned)(p1>>32));put32(ram,12u,(unsigned)p1);
    put32(ram,36u,4u);
    src[0]=0x12u;
    for(i=0;i<256u;i++)clut[i]=(u16)(i*257u);
    assert(avp_jaguar_op_cry16_pair_init(&b,0u,0x4444u,0x7777u));
    r=avp_jaguar_op_compose_scanline_cry16(ram,sizeof(ram),0x1000u,
         0x1000u,0u,0u,src,sizeof(src),0x2000u,clut,256u,
         avp_jaguar_op_cry16_pair_write(&b),scratch,720u,steps,4u);
    assert(r.status==AVP_JAG_OP_SCANLINE_STOP&&r.active_bitmaps==1u);
    assert(avp_jaguar_op_cry16_pair_publish(&b,out,720u,1,0x5aa5u));
    assert(out[0]==0x0101u&&out[1]==0x0202u&&out[2]==0u);
    assert(out[719]==0x4444u);
    assert(b.bank[0][0]==0x5aa5u&&b.bank[0][719]==0x5aa5u);
    assert(avp_jaguar_op_cry16_pair_write(&b)==b.bank[1]);
    assert(b.bank[1][0]==0x7777u);
}
int main(void)
{
    test_publish_order_and_background_clear();
    test_guards_leave_banks_unchanged();
    test_scanline_pipeline_to_physical_bank();
    puts("OP two-bank CRY16 video output and BGEN order passed");
    return 0;
}
