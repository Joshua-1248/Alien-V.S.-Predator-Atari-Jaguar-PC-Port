/* RE24 Full Sweep 06: isolated TOM OP scanline integration tests.
 * Synthetic header fixtures from the Atari TRM Rev.8 pp.18-23; never
 * modify or embed a retail Jaguar ROM or any emulator implementation. */
#include "jaguar/object_processor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put32(u8 *m,size_t sz,u32 base,u32 a,u32 v)
{
    size_t i;
    assert(a>=base && (size_t)(a-base)<=sz-4u);
    i=(size_t)(a-base);
    m[i]=(u8)(v>>24);m[i+1u]=(u8)(v>>16);
    m[i+2u]=(u8)(v>>8);m[i+3u]=(u8)v;
}
static u32 get32(const u8 *m,size_t sz,u32 base,u32 a)
{
    size_t i;
    assert(a>=base && (size_t)(a-base)<=sz-4u);
    i=(size_t)(a-base);
    return ((u32)m[i]<<24)|((u32)m[i+1u]<<16)|((u32)m[i+2u]<<8)|m[i+3u];
}
static void header(u8 *m,size_t sz,u32 base,u32 a,unsigned type,
                   u32 link,u32 data,unsigned y,unsigned h,int x,
                   unsigned depth,unsigned dwidth,unsigned iwidth,
                   unsigned trans,unsigned hscale,unsigned vscale,unsigned rem)
{
    uint64_t p0=((uint64_t)(data>>3)<<43)|((uint64_t)(link>>3)<<24)|
                ((uint64_t)h<<14)|((uint64_t)y<<3)|type;
    uint64_t p1=((uint64_t)trans<<47)|((uint64_t)iwidth<<28)|
                ((uint64_t)dwidth<<18)|((uint64_t)1u<<15)|
                ((uint64_t)depth<<12)|((unsigned)x&0xfffu);
    uint64_t p2=((uint64_t)rem<<16)|((uint64_t)vscale<<8)|hscale;
    put32(m,sz,base,a,(u32)(p0>>32));
    put32(m,sz,base,a+4u,(u32)p0);
    put32(m,sz,base,a+8u,(u32)(p1>>32));
    put32(m,sz,base,a+12u,(u32)p1);
    if(type==1u){
        put32(m,sz,base,a+16u,(u32)(p2>>32));
        put32(m,sz,base,a+20u,(u32)p2);
    }
}
static unsigned read_header(u8 *ram,size_t size,u32 base,u32 addr,
                            AvpJaguarOpBitmapHeader *out)
{
    u32 w[6];unsigned t,i;
    t=get32(ram,size,base,addr+4u)&7u;
    for(i=0u;i<(t?6u:4u);i++)w[i]=get32(ram,size,base,addr+i*4u);
    return (unsigned)avp_jaguar_op_decode_bitmap_words(w,t?6u:4u,out);
}
static void test_multi_object_and_writeback(void)
{
    u8 ram[256]={0},src[128]={0};
    u16 line[32],scratch[32],clut[256];
    AvpJaguarOpTraceStep trace[10];
    AvpJaguarOpScanlineResult r;
    AvpJaguarOpBitmapHeader h;
    unsigned i;
    for(i=0u;i<256u;i++)clut[i]=(u16)(i*0x0101u);
    for(i=0u;i<32u;i++)line[i]=0x7777u;
    /* Two overlapping original-style 4-bpp bitmap objects. */
    header(ram,sizeof(ram),0x1000u,0x1000u,0u,0x1020u,0x2000u,
           4u,2u,0,2u,1u,1u,0u,0u,0u,0u);
    header(ram,sizeof(ram),0x1000u,0x1020u,0u,0x1040u,0x2010u,
           4u,1u,1,2u,1u,1u,1u,0u,0u,0u);
    put32(ram,sizeof(ram),0x1000u,0x1044u,4u);
    src[0]=0x12u;src[8]=0x45u;src[16]=0x03u;
    r=avp_jaguar_op_compose_scanline_cry16(ram,sizeof(ram),0x1000u,
           0x1000u,4u,0u,src,sizeof(src),0x2000u,
           clut,256u,line,scratch,32u,trace,10u);
    assert(r.status==AVP_JAG_OP_SCANLINE_STOP && r.objects==3u &&
           r.active_bitmaps==2u && r.trace_status==AVP_JAG_OP_TRACE_STOP);
    assert(line[0]==0x0101u && line[1]==0x0202u && line[2]==0x0303u);
    assert(line[16]==0x7777u); /* second object is transparent beyond x=2 */
    assert(read_header(ram,sizeof(ram),0x1000u,0x1000u,&h));
    assert(h.data_address==0x2008u&&h.height==1u);
    assert(read_header(ram,sizeof(ram),0x1000u,0x1020u,&h));
    assert(h.data_address==0x2018u&&h.height==0u);
    /* Second line advances first bitmap, but does not draw exhausted
     * second bitmap. The previous line does not bleed into next frame. */
    for(i=0u;i<32u;i++)line[i]=0x9999u;
    r=avp_jaguar_op_compose_scanline_cry16(ram,sizeof(ram),0x1000u,
           0x1000u,6u,0u,src,sizeof(src),0x2000u,
           clut,256u,line,scratch,32u,trace,10u);
    assert(r.status==AVP_JAG_OP_SCANLINE_STOP&&r.active_bitmaps==1u);
    assert(line[0]==0x0404u && line[1]==0x0505u && line[2]==0u);
    assert(read_header(ram,sizeof(ram),0x1000u,0x1000u,&h));
    assert(h.data_address==0x2010u && h.height==0u);
}
static void test_preflight_and_gpu_are_noncommitting(void)
{
    u8 ram[256]={0},source[64]={0},old[256];
    u16 line[32],scratch[32],clut[256];
    AvpJaguarOpTraceStep steps[10];
    AvpJaguarOpScanlineResult r;
    unsigned i;
    for(i=0;i<256u;i++)clut[i]=(u16)i;
    for(i=0;i<32u;i++)line[i]=0x1234u;
    header(ram,sizeof(ram),0x1000u,0x1000u,0u,0x1020u,0x2000u,
           0u,4u,0,2u,1u,1u,0u,0u,0u,0u);
    header(ram,sizeof(ram),0x1000u,0x1020u,0u,0x1040u,0x2040u,
           0u,4u,0,2u,1u,1u,0u,0u,0u,0u);
    put32(ram,sizeof(ram),0x1000u,0x1044u,4u);
    source[0]=0x12u;
    memcpy(old,ram,sizeof(ram));
    /* The FIRST bitmap could draw. The SECOND bitmap exceeds the
     * source window: neither output pixels nor headers may change. */
    r=avp_jaguar_op_compose_scanline_cry16(ram,sizeof(ram),0x1000u,
          0x1000u,0u,0u,source,sizeof(source),0x2000u,
          clut,256u,line,scratch,32u,steps,10u);
    assert(r.status==AVP_JAG_OP_SCANLINE_RANGE);
    assert(memcmp(ram,old,sizeof(ram))==0);
    for(i=0;i<32u;i++)assert(line[i]==0x1234u);
    /* An active GPU object causes a distinct pending status; it is not
     * silently skipped, and commits no earlier bitmap operation. */
    put32(ram,sizeof(ram),0x1000u,0x1020u,0u);
    put32(ram,sizeof(ram),0x1000u,0x1024u,(0x7ffu<<3)|2u);
    memcpy(old,ram,sizeof(ram));
    r=avp_jaguar_op_compose_scanline_cry16(ram,sizeof(ram),0x1000u,
          0x1000u,0u,0u,source,sizeof(source),0x2000u,
          clut,256u,line,scratch,32u,steps,10u);
    assert(r.status==AVP_JAG_OP_SCANLINE_GPU_PENDING && r.objects==2u);
    assert(memcmp(ram,old,sizeof(ram))==0);
    for(i=0;i<32u;i++)assert(line[i]==0x1234u);
    /* Truncated trace capacity and invalid scratch are also no-ops. */
    r=avp_jaguar_op_compose_scanline_cry16(ram,sizeof(ram),0x1000u,
          0x1000u,0u,0u,source,sizeof(source),0x2000u,
          clut,256u,line,scratch,32u,steps,1u);
    assert(r.status==AVP_JAG_OP_SCANLINE_TRACE_ERROR);
    r=avp_jaguar_op_compose_scanline_cry16(ram,sizeof(ram),0x1000u,
          0x1000u,0u,0u,source,sizeof(source),0x2000u,
          clut,256u,line,line,32u,steps,10u);
    assert(r.status==AVP_JAG_OP_SCANLINE_INVALID);
}
static void test_scaled_object_and_inactive(void)
{
    u8 ram[256]={0},source[64]={0};
    u16 line[32],scratch[32],clut[256];
    AvpJaguarOpTraceStep steps[10];
    AvpJaguarOpScanlineResult r;
    AvpJaguarOpBitmapHeader h;
    unsigned i;
    for(i=0;i<256u;i++)clut[i]=(u16)(i*257u);
    for(i=0;i<32u;i++)line[i]=0x7070u;
    header(ram,sizeof(ram),0x1000u,0x1000u,1u,0x1020u,0x2000u,
           12u,2u,3,2u,1u,1u,0u,32u,32u,0u);
    put32(ram,sizeof(ram),0x1000u,0x1024u,4u);
    source[0]=0x67u;
    r=avp_jaguar_op_compose_scanline_cry16(ram,sizeof(ram),0x1000u,
          0x1000u,10u,0u,source,sizeof(source),0x2000u,
          clut,256u,line,scratch,32u,steps,10u);
    assert(r.status==AVP_JAG_OP_SCANLINE_STOP&&r.active_bitmaps==0u);
    assert(line[3]==0x7070u);
    assert(read_header(ram,sizeof(ram),0x1000u,0x1000u,&h));
    assert(h.height==2u&&h.data_address==0x2000u);
    r=avp_jaguar_op_compose_scanline_cry16(ram,sizeof(ram),0x1000u,
          0x1000u,12u,0u,source,sizeof(source),0x2000u,
          clut,256u,line,scratch,32u,steps,10u);
    assert(r.status==AVP_JAG_OP_SCANLINE_STOP&&r.active_bitmaps==1u);
    assert(line[3]==0x0606u && line[4]==0x0707u);
    assert(read_header(ram,sizeof(ram),0x1000u,0x1000u,&h));
    assert(h.height==1u&&h.data_address==0x2008u&&h.remainder==0u);
}
static void test_authenticated_avp_prelist_windows(void)
{
    /* Original AVP PRE_LIST CPU-derived branch phrases checked in R224.
     * STOP entries are bounded synthetic stubs, not copied ROM assets.
     * The entire scanline path must honor video-window traversal while
     * leaving the physical line buffer untouched before/after VDB/VDE. */
    const u32 video[2][4]={
        {0x0000000eu,0xab0040e3u,0x0000000eu,0xac004fe3u},
        {0x0000000eu,0xab004163u,0x0000000eu,0xac0052e3u}
    };
    u8 ram[0x20],dummy[8]={0};
    u16 line[32],scratch[32],clut[256]={0};
    AvpJaguarOpTraceStep steps[8];
    unsigned mode,vc,i;
    AvpJaguarOpScanlineResult r;
    for(mode=0u;mode<2u;mode++){
        memset(ram,0,sizeof(ram));
        for(i=0u;i<4u;i++)
            put32(ram,sizeof(ram),0x7548u,0x7548u+i*4u,video[mode][i]);
        put32(ram,sizeof(ram),0x7548u,0x755cu,4u);
        put32(ram,sizeof(ram),0x7548u,0x7564u,4u);
        for(vc=0u;vc<=0x7feu;vc+=2u){
            for(i=0u;i<32u;i++)line[i]=(u16)(0x5000u+i);
            r=avp_jaguar_op_compose_scanline_cry16(ram,sizeof(ram),
                0x7548u,0x7548u,vc,0u,dummy,sizeof(dummy),0x2000u,
                clut,256u,line,scratch,32u,steps,8u);
            assert(r.status==AVP_JAG_OP_SCANLINE_STOP);
            assert(r.active_bitmaps==0u && r.drawn_pixels==0u);
            assert(r.objects==(vc<(mode?44u:28u)?2u:3u));
            for(i=0u;i<32u;i++)assert(line[i]==(u16)(0x5000u+i));
        }
    }
}
int main(void)
{
    test_authenticated_avp_prelist_windows();
    test_multi_object_and_writeback();
    test_preflight_and_gpu_are_noncommitting();
    test_scaled_object_and_inactive();
    puts("OP scanline composition + writeback regression passed");
    return 0;
}
