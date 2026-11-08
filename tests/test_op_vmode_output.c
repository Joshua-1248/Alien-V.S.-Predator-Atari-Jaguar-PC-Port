/* RE #24 Full Sweep 08: TOM VMODE output colour mux, no emulator code.
 * Reference: Atari Jaguar Technical Reference Manual Rev.8 pp.12-13,22-23. */
#include "jaguar/object_processor.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdio.h>

static u32 cry_pixel(u16 word)
{
    u8 r,g,b;
    avp_jaguar_cry16_to_rgb(word,&r,&g,&b);
    return ((u32)r<<16)|((u32)g<<8)|b;
}
static void test_crymode(void)
{
    u16 src[]={0x0000u,0x0fffu,0xf07eu,0x1234u};
    u32 out[5]={7,7,7,7,0xdeadbeefu};
    AvpJaguarOpVideoResult r=avp_jaguar_op_vmode_decode_line(src,4,0,0,out,4);
    assert(r.status==AVP_JAG_OP_VIDEO_OK&&r.pixels==4);
    for(unsigned i=0;i<4u;i++)assert(out[i]==cry_pixel(src[i]));
    assert(out[4]==0xdeadbeefu);
}
static void test_rgb16_and_rgb24(void)
{
    u16 src[]={0xf800u,0x07c0u,0x003fu,0xffffu};
    u32 out[4]={0,0,0,0};
    AvpJaguarOpVideoResult r=avp_jaguar_op_vmode_decode_line(src,4,3,0,out,4);
    assert(r.status==AVP_JAG_OP_VIDEO_OK&&r.pixels==4);
    for(unsigned i=0;i<4u;i++)assert(out[i]==avp_jaguar_vmode_rgb16(src[i]));
    assert(out[0]==0xf80000u); /* TRM p.23 */
    assert(out[3]==0xf8fcf8u); /* TRM p.23: low bits zero */
    r=avp_jaguar_op_vmode_decode_line(src,4,1,0,out,2);
    assert(r.status==AVP_JAG_OP_VIDEO_OK&&r.pixels==2);
    assert(out[0]==avp_jaguar_vmode_rgb24(src[0],src[1]));
    assert(out[1]==avp_jaguar_vmode_rgb24(src[2],src[3]));
}
static void test_varmod_mixed(void)
{
    /* Bit0=0 chooses original CRY16; bit0=1 selects RGB16, but
     * the format bit cannot double as the least significant green bit. */
    u16 src[]={0xf07eu,0xf801u,0x07c0u,0x003fu};
    u32 out[4]={0};
    AvpJaguarOpVideoResult r=avp_jaguar_op_vmode_decode_line(src,4,0,1,out,4);
    assert(r.status==AVP_JAG_OP_VIDEO_OK&&r.pixels==4);
    assert(out[0]==cry_pixel(src[0]));
    assert(out[1]==avp_jaguar_vmode_rgb16(0xf800u));
    assert(out[2]==cry_pixel(src[2]));
    assert(out[3]==avp_jaguar_vmode_rgb16(0x003eu));
}
static void test_pair_to_vmode_output(void)
{
    AvpJaguarOpCry16LinePair pair;
    u16 *write, published[4];
    u32 rgb[4];
    AvpJaguarOpVideoResult r;
    assert(avp_jaguar_op_cry16_pair_init(&pair,1u,0xaaaau,0x5555u));
    write=avp_jaguar_op_cry16_pair_write(&pair);
    assert(write==pair.bank[1]);
    write[0]=0xf800u;write[1]=0x07c0u;
    write[2]=0x003fu;write[3]=0xffffu;
    assert(avp_jaguar_op_cry16_pair_publish(&pair,published,4u,1,0u));
    r=avp_jaguar_op_vmode_decode_line(published,4u,3u,0u,rgb,4u);
    assert(r.status==AVP_JAG_OP_VIDEO_OK&&r.pixels==4u);
    assert(rgb[0]==0xf80000u&&rgb[3]==0xf8fcf8u); /* TRM p.23 */
    /* BGEN changes the previously displayed physical bank only AFTER
     * publication; it cannot retroactively erase the output words. */
    assert(pair.bank[1][0]==0u&&pair.bank[1][719]==0u);
    assert(pair.bank[0][0]==0xaaaau&&pair.write_bank==0u);
    r=avp_jaguar_op_vmode_decode_line(published,4u,1u,0u,rgb,2u);
    assert(r.status==AVP_JAG_OP_VIDEO_OK&&r.pixels==2u);
    assert(rgb[0]==avp_jaguar_vmode_rgb24(0xf800u,0x07c0u));
}
static void test_guards_and_no_partial_writes(void)
{
    u16 src[4]={1,2,3,4};
    u32 out[4]={0x12345678u,0x12345678u,0x12345678u,0x12345678u};
    AvpJaguarOpVideoResult r;
    r=avp_jaguar_op_vmode_decode_line(src,4,2,0,out,4);
    assert(r.status==AVP_JAG_OP_VIDEO_UNSUPPORTED);
    r=avp_jaguar_op_vmode_decode_line(src,4,3,1,out,4);
    assert(r.status==AVP_JAG_OP_VIDEO_UNSUPPORTED);
    r=avp_jaguar_op_vmode_decode_line(src,3,1,0,out,4);
    assert(r.status==AVP_JAG_OP_VIDEO_BAD_INPUT);
    r=avp_jaguar_op_vmode_decode_line(src,4,0,0,out,3);
    assert(r.status==AVP_JAG_OP_VIDEO_BAD_INPUT);
    r=avp_jaguar_op_vmode_decode_line(NULL,4,0,0,out,4);
    assert(r.status==AVP_JAG_OP_VIDEO_BAD_INPUT);
    r=avp_jaguar_op_vmode_decode_line(src,4,0,0,NULL,4);
    assert(r.status==AVP_JAG_OP_VIDEO_BAD_INPUT);
    for(unsigned i=0;i<4u;i++)assert(out[i]==0x12345678u);
}
int main(void)
{
    test_crymode();test_rgb16_and_rgb24();test_varmod_mixed();
    test_pair_to_vmode_output();
    test_guards_and_no_partial_writes();
    puts("OP VMODE CRY16/RGB16/RGB24/VARMOD output passed");
    return 0;
}
