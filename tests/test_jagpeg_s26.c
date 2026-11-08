/* RE24 S26: ROM-free boundary tests for native JagPEG preparation and
 * the exact GPU RGB->CRY conversion.  Retail parity is checked separately
 * by tools/jagref/jagpeg_port_parity.sh against the user's own cartridge. */
#include "jaguar/avp_gpu/jagpeg_load.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_CAP (2u*1024u*1024u)
static uint8_t dram[MEM_CAP];
static unsigned invalid,reads,writes;
static uint32_t mread(void *ctx,uint32_t addr,int width)
{
    uint32_t v=0;unsigned i;(void)ctx;
    if(width<1||width>4||addr>=MEM_CAP||(uint32_t)width>MEM_CAP-addr){invalid++;return 0;}
    reads++;
    for(i=0;i<(unsigned)width;i++)v=(v<<8u)|dram[addr+i];
    return v;
}
static void mwrite(void *ctx,uint32_t addr,uint32_t val,int width)
{
    int i;(void)ctx;
    if(width<1||width>4||addr>=MEM_CAP||(uint32_t)width>MEM_CAP-addr){invalid++;return;}
    writes++;
    for(i=width-1;i>=0;i--){dram[addr+(unsigned)i]=(uint8_t)val;val>>=8u;}
}
static int fail(const char *s){fprintf(stderr,"JagPEG S26: %s\n",s);return 1;}
int main(void)
{
    AvpView3DBus bus={0};AvpJagpegJob job;AvpJagpegConvert cv={5,0x10000,0x20000,0x30000};
    const uint32_t record=0x1000u;
    const uint8_t intensities[]={1,32,127,255};
    uint32_t i;
    bus.ctx=NULL;bus.rd=mread;bus.wr=mwrite;
    mwrite(NULL,record+4u,16u,2); /* blocks across */
    mwrite(NULL,record+6u,8u,2);  /* blocks down */
    mwrite(NULL,AVP_JPG_VAR_MODE,2u,1);
    mwrite(NULL,AVP_JPG_VAR_SCRATCH,0x180000u,4);
    avp_jagpeg_prepare(&bus,record,0x140000u,&job);
    if(job.record!=record||job.dest!=0x180000u||job.width_blocks!=16u||
       mread(NULL,AVP_JPG_VAR_WIDTH_PX,2)!=128u||
       mread(NULL,AVP_JPG_VAR_HEIGHT_PX,2)!=64u||
       mread(NULL,AVP_JPG_VAR_BLIT_WIDTH,2)!=(job.width_code&0xFFFFu))
        return fail("mode 2 picture setup was incorrect");
    mwrite(NULL,AVP_JPG_VAR_MODE,1u,1);
    avp_jagpeg_prepare(&bus,record,0x140000u,&job);
    if(job.dest!=0x140000u)return fail("non-conversion output must use direct destination");
    /* The retail table is supplied by the game; a constant synthetic table
       makes the expected low intensity bits and 16-bit store width explicit. */
    memset(dram+0x30000u,0x12,0x20000u);
    mwrite(NULL,0x7F80u,5u,4);
    mwrite(NULL,0x7F84u,cv.source,4);
    mwrite(NULL,0x7F88u,cv.dest,4);
    mwrite(NULL,0x7F8Cu,cv.arg,4);
    for(i=0;i<4;i++){
        uint32_t v=(uint32_t)intensities[i]*0x01010101u;
        mwrite(NULL,cv.source+i*4u,v,4);
    }
    mwrite(NULL,cv.source+16u,0xFFFFFFFFu,4);
    mwrite(NULL,cv.dest+8u,0xDEADBEEFu,4);
    avp_jagpeg_gpu_convert(&bus,&cv);
    for(i=0;i<4;i++)if(mread(NULL,cv.dest+2u*i,2)!=(0x1200u|intensities[i]))
        return fail("converter output or pixel order differs");
    if(mread(NULL,cv.dest+8u,4)!=0xDEADBEEFu)
        return fail("converter must round its pixel count down to four");
    if(mread(NULL,0x7F80u,4)!=0u)return fail("converter must clear completion word");
    if(invalid||!reads||!writes)return fail("invalid host-memory access detected");
    printf("JagPEG S26: preparation modes, four CRY pixels, transfer bounds and handshake PASS\n");
    return 0;
}
