/* RE45 R306: external RGBA masks, exact retail notice placement, window silhouettes. */
#define _GNU_SOURCE
#include "pc/pc_hi_res_textures.h"
#include "jaguar/avp_gpu/hires_window_mask.h"
#include "jaguar/avp_gpu/hires_alpha_blend.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

int avp_gpu_build_cry_image(AvpGpuRgbImage *image,const uint8_t *qconv,size_t size)
{
    size_t count;
    if(!image||!image->rgb||!qconv||size<131072u)return 0;
    count=(size_t)image->width*image->height;
    image->cry16=(uint16_t*)malloc(count*sizeof(uint16_t));
    if(!image->cry16)return 0;
    for(size_t i=0;i<count;i++)image->cry16[i]=(uint16_t)image->rgb[i*3u];
    return 1;
}
static void mk(AvpGpuRgbImage *im,int w,int h,uint8_t ink)
{
    im->width=w;im->height=h;
    im->rgb=(uint8_t*)malloc((size_t)w*h*3u);
    assert(im->rgb);
    memset(im->rgb,ink,(size_t)w*h*3u);
}
static int ink(const AvpGpuRgbImage *im,int x,int y)
{
    assert(im&&im->rgb&&x>=0&&x<im->width&&y>=0&&y<im->height);
    return im->rgb[((size_t)y*im->width+x)*3u];
}
static void png_be32(uint8_t *dst,uint32_t x)
{
    dst[0]=(uint8_t)(x>>24);dst[1]=(uint8_t)(x>>16);dst[2]=(uint8_t)(x>>8);dst[3]=(uint8_t)x;
}
static void png_chunk(FILE *file,const char name[4],const uint8_t *data,uint32_t n)
{
    uint8_t length[4],crcbytes[4];uLong crc;
    png_be32(length,n);
    assert(fwrite(length,1,4,file)==4u);
    assert(fwrite(name,1,4,file)==4u);
    if(n)assert(fwrite(data,1,n,file)==n);
    crc=crc32(0L,(const Bytef*)name,4);
    if(n)crc=crc32(crc,data,n);
    png_be32(crcbytes,(uint32_t)crc);
    assert(fwrite(crcbytes,1,4,file)==4u);
}
static void write_rgba_png(const char *name)
{
    static const uint8_t signature[]={137,80,78,71,13,10,26,10};
    uint8_t ihdr[13]={0},raw[4*(4*4+1)]={0};
    uint8_t compressed[256];uLongf cn=sizeof(compressed);
    FILE *f=fopen(name,"wb");assert(f);
    png_be32(ihdr,4);png_be32(ihdr+4,4);ihdr[8]=8;ihdr[9]=6;
    for(unsigned y=0;y<4;y++){
        for(unsigned x=0;x<4;x++){
            uint8_t *pixel=raw+y*17+1+x*4;
            pixel[0]=120;pixel[1]=90;pixel[2]=60;
            pixel[3]=(x==0 && y==0)?0u:((x==1&&y==0)?128u:255u);
        }
    }
    assert(compress2(compressed,&cn,raw,sizeof(raw),Z_BEST_SPEED)==Z_OK);
    assert(fwrite(signature,1,sizeof(signature),f)==sizeof(signature));
    png_chunk(f,"IHDR",ihdr,13);
    png_chunk(f,"IDAT",compressed,(uint32_t)cn);
    png_chunk(f,"IEND",NULL,0);
    assert(fclose(f)==0);
}
int main(void)
{
    AvpPcHiResTextures *pack=(AvpPcHiResTextures*)calloc(1,sizeof(*pack));
    AvpGpuRgbImage *orig=(AvpGpuRgbImage*)calloc(AVP_PC_HI_RES_SLOTS,sizeof(*orig));
    uint8_t *qconv=(uint8_t*)calloc(1,131072);
    char dir[]="/tmp/avp_r306_XXXXXX",sub[256],png[320],exe[320];
    uint8_t pair[256]={0};double lo,hi;
    assert(pack&&orig&&qconv&&mkdtemp(dir));
    assert(snprintf(sub,sizeof(sub),"%s/hi_res_textures",dir)<(int)sizeof(sub));
    assert(mkdir(sub,0700)==0);
    assert(snprintf(png,sizeof(png),"%s/65.png",sub)<(int)sizeof(png));
    assert(snprintf(exe,sizeof(exe),"%s/avp_pc",dir)<(int)sizeof(exe));
    write_rgba_png(png);
    assert(avp_pc_hi_res_textures_load(pack,exe,qconv,131072u)==1u);
    assert(pack->image[65].width==4&&pack->image[65].height==4);
    assert(pack->image[65].alpha && pack->image[65].alpha[0]==0u);
    assert(pack->image[65].alpha[1]==128u&&pack->image[65].alpha[2]==255u);
    assert(ink(&pack->image[65],0,0)==120); /* RGB no longer black-premultiplied. */
    /* Use a hi-res base and an independent label without affecting the ROM. */
    mk(&orig[16],128,128,10);
    mk(&orig[62],48,32,40);
    for(unsigned id=69;id<=73;id++)mk(&orig[id],24,32,150);
    mk(&pack->image[16],256,256,20);
    mk(&pack->image[62],96,64,200);
    pack->image[62].alpha=(uint8_t*)malloc(96u*64u);
    assert(pack->image[62].alpha);
    memset(pack->image[62].alpha,255,96u*64u);
    /* Fully clear a pixel and partially blend a neighboring letter pixel. */
    pack->image[62].alpha[0]=0;
    pack->image[62].alpha[1]=128;
    assert(avp_pc_hi_res_textures_build_notices(pack,orig,qconv,131072u)==6u);
    assert(pack->notice[0].width==256);
    assert(ink(&pack->notice[0],0,0)==20);         /* hi-res base */
    assert(ink(&pack->notice[0],79,64)==20);       /* before lettering */
    assert(ink(&pack->notice[0],80,64)==20);       /* transparent lettering corner */
    assert(ink(&pack->notice[0],83,64)>20);        /* alpha blended */
    assert(ink(&pack->notice[0],96,80)==200);      /* hi-res lettering */
    assert(ink(&pack->notice[13],104,128)==150);   /* native number at +16 Y */
    assert(ink(&pack->notice[13],96,80)==200);     /* independent lettering */
    assert(pack->notice[5].rgb==NULL);             /* no unrelated signs */
    /* Integer source centres retain original endpoints, fractional columns
     * blend only two genuinely open neighbors. */
    for(unsigned i=0;i<128;i++){
        pair[i*2]= (uint8_t)(int8_t)-20;
        pair[i*2+1]=(uint8_t)(int8_t)20;
    }
    pair[11*2]=(uint8_t)(int8_t)-10;
    pair[11*2+1]=(uint8_t)(int8_t)30;
    avp_gpu_hires_window_pair(pair,10,0.5,1,&lo,&hi);
    assert(lo==-20&&hi==20);
    avp_gpu_hires_window_pair(pair,10,0.75,1,&lo,&hi);
    assert(lo>-20&&lo<-10&&hi>20&&hi<30);
    assert(avp_gpu_hires_edge_y(0,1024,lo)<512);
    /* World alpha is composited after CRY shading; never premultiply black. */
    assert(avp_gpu_hires_alpha_blend(0x204060u,0x80a0c0u,0u)==0x204060u);
    assert(avp_gpu_hires_alpha_blend(0x204060u,0x80a0c0u,255u)==0x80a0c0u);
    assert(avp_gpu_hires_alpha_blend(0x000000u,0xffffffu,128u)==0x808080u);
    pair[11*2]=(uint8_t)(int8_t)5;
    pair[11*2+1]=(uint8_t)(int8_t)5;
    avp_gpu_hires_window_pair(pair,10,0.9,1,&lo,&hi);
    assert(lo==-20&&hi==20); /* never turn a closed cell into an opening */
    assert(avp_pc_hi_res_textures_build_notices(pack,orig,qconv,131072u)==6u);
    avp_pc_hi_res_textures_destroy(pack);
    for(unsigned i=0;i<AVP_PC_HI_RES_SLOTS;i++)free(orig[i].rgb);
    free(orig);free(qconv);free(pack);
    assert(unlink(png)==0&&rmdir(sub)==0&&rmdir(dir)==0);
    puts("R306 independent hi-res sign layers, RGBA PNG masks, Jaguar window-mask fallbacks PASS");
    return 0;
}
