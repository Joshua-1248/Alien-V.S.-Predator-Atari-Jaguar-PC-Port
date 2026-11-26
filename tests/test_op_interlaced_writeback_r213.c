/* RE40 R213: executable and standalone TOM BITOBJ source-row parity.
 * TRM Rev.8 p.18: non-interlace consumes one source line; interlace two.
 * The standalone non-interlaced API remains byte-compatible. No ROM data. */
#include "jaguar/object_processor.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void pack(u32 *w,unsigned type,unsigned height,unsigned width,
                 unsigned data,unsigned depth)
{
    uint64_t p0=(uint64_t)type|((uint64_t)17u<<3)|
                ((uint64_t)height<<14)|((uint64_t)(data&0x1fffffu)<<43);
    uint64_t p1=(uint64_t)11u|((uint64_t)depth<<12)|
                ((uint64_t)3u<<15)|((uint64_t)width<<18)|
                ((uint64_t)2u<<28)|((uint64_t)1u<<48);
    w[0]=(u32)(p0>>32);w[1]=(u32)p0;
    w[2]=(u32)(p1>>32);w[3]=(u32)p1;
    w[4]=0xfeedc0deu;w[5]=0x00202020u;
}
static void set_bytes(u8 *ram,unsigned offset,const u32 *words,unsigned count)
{
    for(unsigned i=0;i<count;i++){
        const u32 v=words[i];
        ram[offset+i*4u]=(u8)(v>>24);
        ram[offset+i*4u+1u]=(u8)(v>>16);
        ram[offset+i*4u+2u]=(u8)(v>>8);
        ram[offset+i*4u+3u]=(u8)v;
    }
}
static void get_bytes(const u8 *ram,unsigned offset,u32 *words,unsigned count)
{
    for(unsigned i=0;i<count;i++){
        words[i]=((u32)ram[offset+4u*i]<<24)|
            ((u32)ram[offset+4u*i+1u]<<16)|
            ((u32)ram[offset+4u*i+2u]<<8)|ram[offset+4u*i+3u];
    }
}
static void check(unsigned vp,unsigned height,unsigned dwidth,
                  unsigned data,unsigned depth)
{
    u32 w[6],out[6],ref[6],unchanged[6];
    u8 ram[48];
    AvpJaguarOpBitmapHeader hdr;
    unsigned rows=999u,memory_rows=999u;
    unsigned expected_rows=height?((vp&1u)?1u:2u):0u;
    unsigned newheight=height>expected_rows?height-expected_rows:0u;
    unsigned newdata=(data+expected_rows*dwidth)&0x1fffffu;
    pack(w,0u,height,dwidth,data,depth);
    assert(avp_jaguar_op_advance_bitmap_words_at_vp(
        w,4u,out,&rows,vp));
    assert(rows==expected_rows);
    assert(out[2]==w[2]&&out[3]==w[3]);
    assert(avp_jaguar_op_decode_bitmap_words(out,4u,&hdr));
    assert(hdr.height==newheight);
    assert(hdr.data_address==(newdata<<3));
    assert(hdr.ypos==17u&&hdr.xpos==11&&hdr.release==1u);
    assert(hdr.dwidth==dwidth&&hdr.depth==depth);
    memcpy(unchanged,w,sizeof unchanged);
    memcpy(ref,w,sizeof ref);
    assert(avp_jaguar_op_advance_bitmap_words_at_vp(
        ref,4u,ref,NULL,vp));
    assert(!memcmp(ref,out,4u*sizeof(u32)));
    assert(!memcmp(w,unchanged,sizeof unchanged));
    /* Big-endian RAM mutation has the same output and does not touch
     * prefix, suffix or the second phrase's unrelated bits. */
    memset(ram,0xa5u,sizeof ram);
    set_bytes(ram,8u,w,4u);
    assert(avp_jaguar_op_writeback_bitmap_memory_at_vp(
        ram,sizeof ram,0x8000u,0x8008u,&memory_rows,vp));
    assert(memory_rows==rows);
    get_bytes(ram,8u,ref,4u);
    assert(!memcmp(ref,out,4u*sizeof(u32)));
    for(unsigned i=0;i<8u;i++)assert(ram[i]==0xa5u);
    for(unsigned i=24u;i<sizeof ram;i++)assert(ram[i]==0xa5u);
    /* Legacy API is fixed to a non-interlaced display line. */
    assert(avp_jaguar_op_advance_bitmap_words(w,4u,ref,NULL));
    assert(avp_jaguar_op_decode_bitmap_words(ref,4u,&hdr));
    assert(hdr.height==(height?height-1u:0u));
}
static void failure_cases(void)
{
    u32 w[6],dst[6];
    u8 ram[48],copy[48];
    unsigned rows=77u;
    pack(w,1u,3u,2u,0x124u,4u);
    for(unsigned i=0;i<6u;i++)dst[i]=0x12345600u+i;
    /* No verified scaled interlace behavior: reject without clobbering
     * header or caller's row count. Non-interlaced remains available. */
    assert(!avp_jaguar_op_advance_bitmap_words_at_vp(w,6u,dst,&rows,524u));
    for(unsigned i=0;i<6u;i++)assert(dst[i]==0x12345600u+i);
    assert(rows==77u);
    assert(avp_jaguar_op_advance_bitmap_words_at_vp(w,6u,dst,&rows,523u));
    memset(ram,0x57u,sizeof ram);
    set_bytes(ram,8u,w,6u);
    memcpy(copy,ram,sizeof ram);
    rows=77u;
    assert(!avp_jaguar_op_writeback_bitmap_memory_at_vp(
        ram,sizeof ram,0x8000u,0x8008u,&rows,524u));
    assert(!memcmp(ram,copy,sizeof ram)&&rows==77u);
    assert(!avp_jaguar_op_writeback_bitmap_memory_at_vp(
        ram,31u,0x8000u,0x8008u,&rows,523u));
    assert(!memcmp(ram,copy,sizeof ram));
    assert(!avp_jaguar_op_advance_bitmap_words_at_vp(NULL,4u,dst,&rows,523u));
}
int main(void)
{
    static const unsigned h[]={0u,1u,2u,3u,17u,1023u};
    static const unsigned stride[]={0u,1u,7u,511u,1023u};
    static const unsigned d[]={0u,1u,2u,3u,4u,5u};
    static const unsigned vp[]={523u,524u,31u,32u};
    unsigned count=0;
    for(unsigned a=0;a<sizeof vp/sizeof *vp;a++)
    for(unsigned b=0;b<sizeof h/sizeof *h;b++)
    for(unsigned c=0;c<sizeof stride/sizeof *stride;c++)
    for(unsigned e=0;e<sizeof d/sizeof *d;e++){
        check(vp[a],h[b],stride[c],0x1ffffcu,d[e]);
        count++;
    }
    failure_cases();
    printf("R213 standalone interlaced BITOBJ writeback %u configurations PASS\n",count);
    return 0;
}
