/* RE #45 R307 regression: full external art pack + wall-sign composites must
 * fit the decoded image allocation policy. No commercial ROM or user PNGs. */
#define _GNU_SOURCE
#include "pc/pc_hi_res_textures.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

int avp_gpu_build_cry_image(AvpGpuRgbImage *im,const uint8_t *qconv,size_t qsize)
{
    size_t n;
    if(!im || !im->rgb || !qconv || qsize<131072u)return 0;
    n=(size_t)im->width*im->height;
    im->cry16=(uint16_t *)malloc(n*sizeof(uint16_t));
    if(!im->cry16)return 0;
    for(size_t i=0u;i<n;i++)im->cry16[i]=(uint16_t)im->rgb[3u*i];
    return 1;
}
static void be32(uint8_t *p,uint32_t n)
{
    p[0]=(uint8_t)(n>>24u);p[1]=(uint8_t)(n>>16u);
    p[2]=(uint8_t)(n>>8u);p[3]=(uint8_t)n;
}
static void chunk(FILE *file,const char name[4],const uint8_t *data,uint32_t n)
{
    uint8_t word[4];uLong crc;
    be32(word,n);assert(fwrite(word,1,4,file)==4u);
    assert(fwrite(name,1,4,file)==4u);
    if(n)assert(fwrite(data,1,n,file)==n);
    crc=crc32(0L,(const Bytef*)name,4u);
    if(n)crc=crc32(crc,data,n);
    be32(word,(uint32_t)crc);assert(fwrite(word,1,4,file)==4u);
}
static void png_solid(const char *path,unsigned id)
{
    static const uint8_t sig[8]={137,80,78,71,13,10,26,10};
    enum { WIDTH=1254,HEIGHT=1254, CHANNELS=3 };
    const size_t stride=(size_t)WIDTH*CHANNELS+1u;
    const size_t raw_size=stride*HEIGHT;
    uint8_t ihdr[13]={0};uint8_t *raw=(uint8_t*)calloc(1,raw_size);
    uLongf n=compressBound((uLong)raw_size);
    uint8_t *compressed=(uint8_t*)malloc((size_t)n);
    FILE *f;
    assert(raw&&compressed);
    for(unsigned y=0u;y<HEIGHT;y++){
        uint8_t *r=raw+stride*y+1u;
        for(unsigned x=0u;x<WIDTH;x++){
            r[3u*x]=(uint8_t)(id+20u);
            r[3u*x+1u]=(uint8_t)(100u+(id%50u));
            r[3u*x+2u]=30u;
        }
    }
    assert(compress2(compressed,&n,raw,(uLong)raw_size,Z_BEST_SPEED)==Z_OK);
    f=fopen(path,"wb");assert(f);
    assert(fwrite(sig,1,sizeof(sig),f)==sizeof(sig));
    be32(ihdr,WIDTH);be32(ihdr+4u,HEIGHT);ihdr[8]=8u;ihdr[9]=2u;
    chunk(f,"IHDR",ihdr,13u);chunk(f,"IDAT",compressed,(uint32_t)n);
    chunk(f,"IEND",NULL,0u);assert(fclose(f)==0);
    free(raw);free(compressed);
}
static void original(AvpGpuRgbImage *im,int w,int h,uint8_t value)
{
    im->width=w;im->height=h;
    im->rgb=(uint8_t*)malloc((size_t)w*h*3u);
    assert(im->rgb);memset(im->rgb,value,(size_t)w*h*3u);
}
int main(void)
{
    static const unsigned ids[44]={
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,15,16,17,18,19,20,21,
        30,31,32,33,34,35,36,37,38,39,40,41,42,43,47,48,49,50,51,52,53,54,55};
    char dir[]="/tmp/avp_r307_pack_XXXXXX",sub[256],file[320],exe[320];
    AvpPcHiResTextures *pack=(AvpPcHiResTextures*)calloc(1,sizeof(*pack));
    AvpGpuRgbImage *orig=(AvpGpuRgbImage*)calloc(AVP_PC_HI_RES_SLOTS,sizeof(*orig));
    uint8_t *qconv=(uint8_t*)calloc(1,131072u);
    assert(pack&&orig&&qconv&&mkdtemp(dir));
    assert(snprintf(sub,sizeof(sub),"%s/hi_res_textures",dir)<(int)sizeof(sub));
    assert(mkdir(sub,0700)==0);
    assert(snprintf(exe,sizeof(exe),"%s/avp_pc",dir)<(int)sizeof(exe));
    for(unsigned i=0u;i<44u;i++){
        assert(snprintf(file,sizeof(file),"%s/%02u.png",sub,ids[i])<(int)sizeof(file));
        png_solid(file,ids[i]);
    }
    /* Original notice component dimensions/IDs; no ROM assets required. */
    original(&orig[16],128,128,45u);
    for(unsigned i=56u;i<=68u;i++)original(&orig[i],80,24,210u);
    for(unsigned i=69u;i<=73u;i++)original(&orig[i],24,32,175u);
    assert(avp_pc_hi_res_textures_load(pack,exe,qconv,131072u)==44u);
    for(unsigned i=0u;i<44u;i++){
        const AvpGpuRgbImage *im=&pack->image[ids[i]];
        assert(im->rgb&&im->cry16&&im->width==1254&&im->height==1254);
        assert(im->rgb[0]==(uint8_t)(ids[i]+20u));
    }
    assert(avp_pc_hi_res_textures_build_notices(pack,orig,qconv,131072u)==18u);
    for(unsigned i=0u;i<18u;i++){
        assert(pack->notice[i].rgb&&pack->notice[i].cry16);
        assert(pack->notice[i].width==1254&&pack->notice[i].height==1254);
    }
    avp_pc_hi_res_textures_destroy(pack);
    for(unsigned i=0u;i<AVP_PC_HI_RES_SLOTS;i++)free(orig[i].rgb);
    free(pack);free(orig);free(qconv);
    for(unsigned i=0u;i<44u;i++){
        assert(snprintf(file,sizeof(file),"%s/%02u.png",sub,ids[i])<(int)sizeof(file));
        assert(unlink(file)==0);
    }
    assert(rmdir(sub)==0&&rmdir(dir)==0);
    puts("R307 complete 44x1254 PNG pack + 18 hi-res wall-sign composites PASS");
    return 0;
}
