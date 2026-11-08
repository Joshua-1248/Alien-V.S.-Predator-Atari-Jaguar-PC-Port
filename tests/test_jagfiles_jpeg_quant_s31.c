#include "jaguar/avp_gpu/files_jpeg_init.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned char ram[0x40000u],gpu[0x2000u];
static unsigned writes;
static unsigned char *addr(uint32_t a,unsigned n)
{
    if(a<sizeof ram && n<=sizeof ram-a)return ram+a;
    if(a>=0xF02000u && a-0xF02000u<sizeof gpu && n<=sizeof gpu-(a-0xF02000u))return gpu+(a-0xF02000u);
    fprintf(stderr,"S31 mock addr out-of-range: a=%08x n=%u\n",a,n);abort();
}
static uint32_t rd(void *ctx,uint32_t a,int n)
{
    uint32_t v=0;(void)ctx;unsigned char*p=addr(a,(unsigned)n);
    for(int i=0;i<n;i++)v=(v<<8)|p[i];
    return v;
}
static void wr(void *ctx,uint32_t a,uint32_t v,int n)
{
    (void)ctx;uint32_t issued=v;unsigned char*p=addr(a,(unsigned)n);
    for(int i=n-1;i>=0;i--){p[i]=(unsigned char)v;v>>=8;}
    writes++;
    /* The original $BE00 uses $8C28 (Jaguar Blitter), not CPU byte copies.
     * Emulate only the bounded copy used by this ROM-free quant test. */
    if(a==0xF02238u && n==4){
        uint32_t src=rd(NULL,0xF02224u,4)+rd(NULL,0xF02230u,4);
        uint32_t dst=rd(NULL,0xF02200u,4)+rd(NULL,0xF0220Cu,4);
        uint32_t count=rd(NULL,0xF0223Cu,4)&0xffffu;
        if(!count || count>AVP_JPEG_QUANT_BYTES ||
           (issued!=0x01800001u && issued!=0x01800005u)){fprintf(stderr,"S31 mock Blitter invalid cmd=%08x count=%u src=%08x dst=%08x\n",issued,count,src,dst);abort();}
        for(uint32_t i=0;i<count;i++)*addr(dst+i,1)=*addr(src+i,1);
    }
}
#define CHECK(c) do{if(!(c)){fprintf(stderr,"S31 JPEG quant test line %d: %s\n",__LINE__,#c);exit(1);}}while(0)
int main(void)
{
    AvpView3DBus bus={0,rd,wr};
    AvpJagFilesJpegQuant p,old;
    static const uint16_t keys[]={0x3630u,0x3635u,0x3730u,0x3735u,0x3830u,0x3835u,0x3930u};
    for(unsigned i=0;i<7u;i++){
        uint32_t ptr=0x2DD98u+i*0x140u;
        wr(NULL,AVP_JPEG_QUALITY_LIST+i*6u,keys[i],2);
        wr(NULL,AVP_JPEG_QUALITY_LIST+i*6u+2u,ptr,4);
        for(unsigned j=0;j<AVP_JPEG_QUANT_BYTES;j++)ram[ptr+j]=(unsigned char)(i*17u+j);
    }
    wr(NULL,AVP_JPEG_QUALITY_LIST+42u,0u,2);
    for(unsigned q=0;q<8u;q++){
        uint16_t key=q<7u?keys[q]:0x3131u;
        uint32_t source=q<7u?0x2DD98u+q*0x140u:AVP_JPEG_DEFAULT_TABLE;
        unsigned previous=writes;
        CHECK(avp_jagfiles_jpeg_quant_plan(&bus,key,&p));
        CHECK(writes==previous && p.source==source && p.quality==key);
        memset(gpu+(AVP_JPEG_GPU_QUANT_TABLE-0xF02000u),0xCC,AVP_JPEG_QUANT_BYTES+8u);
        CHECK(avp_jagfiles_jpeg_quant_apply(&bus,&p));
        CHECK(memcmp(addr(AVP_JPEG_GPU_QUANT_TABLE,AVP_JPEG_QUANT_BYTES),ram+source,AVP_JPEG_QUANT_BYTES)==0);
        for(unsigned i=0;i<8u;i++)CHECK(rd(0,AVP_JPEG_GPU_QUANT_TABLE+AVP_JPEG_QUANT_BYTES+i,1)==0xCCu);
    }
    CHECK(avp_jagfiles_jpeg_quant_plan(&bus,keys[1],&p));old=p;
    wr(NULL,AVP_JPEG_QUALITY_LIST+8u,0x1FFFF0u,4);
    unsigned count=writes;
    CHECK(!avp_jagfiles_jpeg_quant_apply(&bus,&p) && writes==count);
    CHECK(!avp_jagfiles_jpeg_quant_plan(&bus,keys[1],&p));
    CHECK(memcmp(&old,&p,sizeof p)==0);
    wr(NULL,AVP_JPEG_QUALITY_LIST+8u,old.source,4);
    CHECK(avp_jagfiles_jpeg_quant_plan(&bus,keys[1],&p));
    CHECK(!avp_jagfiles_jpeg_quant_apply(NULL,&p));
    CHECK(!avp_jagfiles_jpeg_quant_plan(NULL,keys[0],&p));
    /* Original $BD92 also handles quality changes without reinstalling the
     * GPU program when the cached tag begins with JP.  Mode 2 requires
     * the separate color table to have been loaded already. */
    for(unsigned mode=0;mode<=2u;mode++)for(unsigned q=0;q<8u;q++){
        uint16_t key=q<7u?keys[q]:0x3131u;
        uint32_t source=q<7u?0x2DD98u+q*0x140u:AVP_JPEG_DEFAULT_TABLE;
        wr(NULL,AVP_JPEG_PREVIOUS_TAG,0x4A503830u,4);
        wr(NULL,AVP_JPEG_MODE_VARIABLE,mode,1);
        wr(NULL,AVP_JPEG_CRY_LOADED,1u,1);
        wr(NULL,AVP_JPEG_QUALITY_VARIABLE,0u,2);
        memset(addr(AVP_JPEG_GPU_QUANT_TABLE,AVP_JPEG_QUANT_BYTES),0,AVP_JPEG_QUANT_BYTES);
        CHECK(avp_jagfiles_jpeg_init_cached(&bus,0x4A500000u|key));
        CHECK(rd(0,AVP_JPEG_QUALITY_VARIABLE,2)==key);
        CHECK(memcmp(addr(AVP_JPEG_GPU_QUANT_TABLE,AVP_JPEG_QUANT_BYTES),ram+source,AVP_JPEG_QUANT_BYTES)==0);
    }
    wr(NULL,AVP_JPEG_PREVIOUS_TAG,0x475A4950u,4);
    wr(NULL,AVP_JPEG_QUALITY_VARIABLE,0x1234u,2);
    count=writes;
    CHECK(!avp_jagfiles_jpeg_init_cached(&bus,0x4A503830u));
    CHECK(writes==count && rd(NULL,AVP_JPEG_QUALITY_VARIABLE,2)==0x1234u);
    wr(NULL,AVP_JPEG_PREVIOUS_TAG,0x4A503830u,4);
    wr(NULL,AVP_JPEG_MODE_VARIABLE,2u,1);
    wr(NULL,AVP_JPEG_CRY_LOADED,0u,1);
    count=writes;
    CHECK(!avp_jagfiles_jpeg_init_cached(&bus,0x4A503730u));
    CHECK(writes==count);
    wr(NULL,AVP_JPEG_MODE_VARIABLE,3u,1);
    count=writes;
    CHECK(!avp_jagfiles_jpeg_init_cached(&bus,0x4A503730u));
    CHECK(writes==count);
    CHECK(!avp_jagfiles_jpeg_init_cached(&bus,0x475A4950u));
    puts("S31 JPEG quantization and cached-JP init: 32 synthetic cases, bounds, staleness PASS");
    return 0;
}
