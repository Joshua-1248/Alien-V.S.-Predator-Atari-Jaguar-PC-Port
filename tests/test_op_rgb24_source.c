/* Atari TRM Rev.8, pp.17,21-23: two 24-bit pixels in each 64-bit phrase.
 * This verifies an isolated off-screen source path; no ROM assets used. */
#include "jaguar/object_processor.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static AvpJaguarOpBitmapHeader hdr(void)
{
    AvpJaguarOpBitmapHeader h;
    memset(&h,0,sizeof(h));
    h.type=0u;h.depth=5u;h.iwidth=2u;h.pitch=1u;
    h.data_address=0x2000u;h.xpos=2;
    return h;
}
static void verify_two_phrases_and_padding(void)
{
    u8 src[32]={0x34,0x12,0x99,0xabu, 0x88,0x80,0x77,0x44,
                0xe0,0xf0,0xfe,0x01, 0x56,0x78,0xcd,0xef};
    u32 line[12],before[12];
    AvpJaguarOpBitmapHeader h=hdr();
    AvpJaguarOpPixelResult r;
    for(unsigned i=0;i<12u;i++)line[i]=before[i]=0xdeadbeefu;
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,sizeof(src),0x2000u,line,12u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==4u);
    assert(line[2]==avp_jaguar_vmode_rgb24(0x3412u,0x99abu));
    assert(line[2]==0x001234abu); /* spare high byte of second word ignored */
    assert(line[3]==0x00808844u);
    assert(line[4]==0x00f0e001u);
    assert(line[5]==0x007856efu);
    assert(line[1]==before[1]&&line[6]==before[6]);
}
static void verify_phrase_pitch_and_repetition(void)
{
    u8 src[40]={0x34,0x12,0x00,0xabu, 0x88,0x80,0x00,0x44,
                0x77,0x66,0x00,0x55, 0x22,0x11,0x00,0x33,
                0xe0,0xf0,0x00,0x01, 0x56,0x78,0x00,0xef};
    u32 line[8]={0};
    AvpJaguarOpBitmapHeader h=hdr();
    AvpJaguarOpPixelResult r;
    h.pitch=2u;h.xpos=0;
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,sizeof(src),0x2000u,line,8u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==4u);
    assert(line[0]==0x001234abu&&line[1]==0x00808844u);
    assert(line[2]==0x00f0e001u&&line[3]==0x007856efu);
    h.pitch=0u;
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,sizeof(src),0x2000u,line,8u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==4u);
    assert(line[2]==line[0]&&line[3]==line[1]);
}
static void verify_reflection_and_clipping(void)
{
    u8 src[16]={0x34,0x12,0x00,0xabu, 0x88,0x80,0x00,0x44,
                0xe0,0xf0,0x00,0x01, 0x56,0x78,0x00,0xef};
    u32 line[4]={9,9,9,9};
    AvpJaguarOpBitmapHeader h=hdr();
    AvpJaguarOpPixelResult r;
    h.reflect=1u;h.xpos=1;
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,sizeof(src),0x2000u,line,4u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==2u);
    assert(line[1]==0x001234abu&&line[0]==0x00808844u);
    assert(line[2]==9u&&line[3]==9u);
    h.reflect=0u;h.xpos=-2;
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,sizeof(src),0x2000u,line,4u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==2u);
    assert(line[0]==0x00f0e001u&&line[1]==0x007856efu);
}
static void verify_real_packed_header_decoder(void)
{
    /* Jaguar CPU-visible high/low longword order, TYPE=0, DEPTH=5,
     * IWIDTH=1 phrase, and PITCH=1 phrase; no synthetic C-only fields. */
    const uint64_t p0=((uint64_t)(0x2000u>>3)<<43)|
                      ((uint64_t)(0x1020u>>3)<<24)|
                      ((uint64_t)2u<<14)|((uint64_t)28u<<3);
    const uint64_t p1=((uint64_t)1u<<28)|((uint64_t)1u<<15)|
                      ((uint64_t)5u<<12)|1u;
    const u32 words[4]={(u32)(p0>>32),(u32)p0,
                        (u32)(p1>>32),(u32)p1};
    const u8 src[8]={0x34,0x12,0x99,0xab,0x88,0x80,0x77,0x44};
    AvpJaguarOpBitmapHeader h;
    u32 output[4]={7u,7u,7u,7u};
    AvpJaguarOpPixelResult r;
    assert(avp_jaguar_op_decode_bitmap_words(words,4u,&h));
    assert(h.type==0u&&h.depth==5u&&h.iwidth==1u&&h.pitch==1u);
    assert(h.data_address==0x2000u&&h.link_address==0x1020u);
    assert(h.ypos==28u&&h.height==2u&&h.xpos==1);
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,sizeof(src),0x2000u,
                                           output,4u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==2u);
    assert(output[0]==7u&&output[1]==0x001234abu&&
           output[2]==0x00808844u&&output[3]==7u);
}
static void verify_unsupported_and_atomic_failures(void)
{
    u8 src[16]={0};
    u32 line[8];
    AvpJaguarOpBitmapHeader h=hdr();
    AvpJaguarOpPixelResult r;
    for(unsigned i=0;i<8u;i++)line[i]=0x12345678u;
    h.trans=1u;
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,sizeof(src),0x2000u,line,8u);
    assert(r.status==AVP_JAG_OP_PIXEL_UNSUPPORTED);
    h.trans=0u;h.rmw=1u;
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,sizeof(src),0x2000u,line,8u);
    assert(r.status==AVP_JAG_OP_PIXEL_UNSUPPORTED);
    h.rmw=0u;h.firstpix=1u;
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,sizeof(src),0x2000u,line,8u);
    assert(r.status==AVP_JAG_OP_PIXEL_UNSUPPORTED);
    h.firstpix=0u;
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,12u,0x2000u,line,8u);
    assert(r.status==AVP_JAG_OP_PIXEL_OUT_OF_RANGE);
    h.data_address=0x1ff8u;
    r=avp_jaguar_op_compose_unscaled_rgb24(&h,src,sizeof(src),0x2000u,line,8u);
    assert(r.status==AVP_JAG_OP_PIXEL_OUT_OF_RANGE);
    for(unsigned i=0;i<8u;i++)assert(line[i]==0x12345678u);
}
int main(void)
{
    verify_two_phrases_and_padding();
    verify_phrase_pitch_and_repetition();
    verify_reflection_and_clipping();
    verify_real_packed_header_decoder();
    verify_unsupported_and_atomic_failures();
    puts("OP RGB24 source phrase layout and clipping passed");
    return 0;
}
