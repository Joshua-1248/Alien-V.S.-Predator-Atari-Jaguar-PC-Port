/* RE24 Sweep 07: original AVP MJP Make_New/Make_Sca CPU phrase crosswalk.
 * Source authority src/game/mjp.c::Make_New (retail 68000 $01E240),
 * hardware fields from Atari Jaguar TRM Rev.8 pp.18-19.
 * No proprietary ROM or emulator data; this is a CPU-source-derived fixture. */
#include "jaguar/object_processor.h"
#include <assert.h>
#include <stdio.h>

static void make_new_words(u32 words[6],unsigned slot,u32 data,int xpos,
                           unsigned ypos,unsigned height,unsigned width,unsigned trans)
{
    const u32 address=0x0004fc00u+slot*0x20u;
    const u32 d1=(address+0x20u)>>3;
    const u32 d2=data>>3;
    const u32 width_phrases=width>>2;
    words[0]=(d1>>8)|(d2<<11);
    words[1]=(ypos<<4)|(height<<14)|(d1<<24);
    words[2]=(width_phrases>>4)|trans;
    words[3]=((u32)xpos&0xfffu)|0x0000c000u|
              (width_phrases<<18)|(width_phrases<<10);
    words[4]=0u;
    words[5]=0u;
}

static void test_cpu_bitmap_header(void)
{
    u32 w[6];
    AvpJaguarOpBitmapHeader h;
    make_new_words(w,2u,0x00002000u,2,0x002cu,0x006eu,320u,0x8000u);
    assert(avp_jaguar_op_decode_bitmap_words(w,4u,&h));
    assert(h.type==0u&&h.link_address==0x0004fc60u);
    assert(h.data_address==0x2000u);
    assert(h.ypos==0x58u&&h.height==0x6eu&&h.xpos==2);
    assert(h.iwidth==80u&&h.dwidth==80u);
    assert(h.depth==4u&&h.pitch==3u&&h.trans==1u);
    assert(h.reflect==0u&&h.rmw==0u);
    assert(h.firstpix==0u);
    /* AVP's title objects explicitly select 16-bpp TRANS; the Atari manual
     * does not enumerate the reserved physical transparent values.  The
     * hardware path must reject, NOT silently draw, that unresolved mode. */
    {
        u8 source[8]={0};
        u16 line[64]={0};
        const AvpJaguarOpPixelResult unresolved=
            avp_jaguar_op_compose_unscaled_cry16(&h,source,sizeof(source),
                0x2000u,NULL,0u,line,64u);
        assert(unresolved.status==AVP_JAG_OP_PIXEL_UNSUPPORTED);
    }
    w[1]|=1u; /* Make_Sca: append third phrase and enable SCBITOBJ */
    w[5]=16u|(16u<<8)|((12u+32u)<<16); /* Change_S(16,12) */
    assert(avp_jaguar_op_decode_bitmap_words(w,6u,&h));
    assert(h.type==1u&&h.hscale==16u&&h.vscale==16u&&h.remainder==44u);
    assert(h.link_address==0x4fc60u&&h.data_address==0x2000u);
}

static void test_cpu_unscaled_framebuffer(void)
{
    u32 words[6],updated[6];
    u8 source[1904]={0};
    u16 line[720],pixels[256]={0};
    AvpJaguarOpBitmapHeader h;
    AvpJaguarOpPixelResult r;
    unsigned i,rows;
    make_new_words(words,0u,0x2000u,0,0x001cu,2u,320u,0u);
    assert(avp_jaguar_op_decode_bitmap_words(words,4u,&h));
    assert(h.iwidth==80u&&h.depth==4u&&h.xpos==0);
    /* Make_New's width-dependent bit packing yields PITCH=3 at 320
     * pixels, so subsequent 64-bit phrases are three phrases apart.
     * Preserve this unusual original CPU behavior pending ROM validation. */
    assert(h.pitch==3u);
    source[0]=0x12u;source[1]=0x34u;
    source[24]=0x56u;source[25]=0x78u;
    for(i=0u;i<720u;i++)line[i]=0x7777u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,source,sizeof(source),
                                             0x2000u,pixels,256u,line,720u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==320u);
    assert(line[0]==0x1234u&&line[1]==0u&&line[4]==0x5678u&&line[320]==0x7777u);
    assert(avp_jaguar_op_advance_bitmap_words(words,4u,updated,&rows));
    assert(rows==1u);
    assert(avp_jaguar_op_decode_bitmap_words(updated,4u,&h));
    assert(h.data_address==0x2280u&&h.height==1u);
}
int main(void)
{
    test_cpu_bitmap_header();
    test_cpu_unscaled_framebuffer();
    puts("AVP MJP original CPU object phrase -> TOM decoder crosswalk passed");
    return 0;
}
