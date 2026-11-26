#define _GNU_SOURCE
#include "pc/pc_hi_res_sprites.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

/* The PNG test exercises shared zlib decoder; original CRY conversion is
 * independently covered by the engine's existing Jaguar color tests. */
int avp_gpu_build_cry_image(AvpGpuRgbImage *im,const uint8_t *q,size_t n)
{
    (void)q;(void)n;(void)im;return 0;
}
static void be32(uint8_t *d,uint32_t x)
{
    d[0]=(uint8_t)(x>>24);d[1]=(uint8_t)(x>>16);
    d[2]=(uint8_t)(x>>8);d[3]=(uint8_t)x;
}
static void chunk(FILE *f,const char tag[4],const uint8_t *data,unsigned len)
{
    uint8_t b[4];uLong c=crc32(0L,Z_NULL,0);
    be32(b,len);assert(fwrite(b,1,4,f)==4);
    assert(fwrite(tag,1,4,f)==4);
    if(len)assert(fwrite(data,1,len,f)==len);
    c=crc32(c,(const Bytef *)tag,4);
    if(len)c=crc32(c,data,len);
    be32(b,(uint32_t)c);assert(fwrite(b,1,4,f)==4);
}
static void png(const char *path,int alpha)
{
    static const uint8_t sig[8]={137,80,78,71,13,10,26,10};
    uint8_t ihdr[13]={0},src[4*(4*4+1)]={0},out[160]={0};
    uLongf n=sizeof(out);const int channels=alpha?4:3;
    uint8_t rgb_src[4*(4*3+1)]={0};
    uint8_t *raw=alpha?src:rgb_src;
    for(int y=0;y<4;y++)for(int x=0;x<4;x++){
        uint8_t *pixel=raw+(size_t)y*(1u+4u*channels)+1u+(size_t)x*channels;
        pixel[0]=(uint8_t)(x+1);pixel[1]=(uint8_t)(y+2);pixel[2]=3u;
        if(alpha)pixel[3]=(x==0&&y==0)?0u:200u;
    }
    be32(ihdr,4);be32(ihdr+4,4);ihdr[8]=8;ihdr[9]=(uint8_t)(alpha?6:2);
    assert(compress2(out,&n,raw,4u*(1u+4u*(unsigned)channels),1)==Z_OK);
    FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(sig,1,8,f)==8);
    chunk(f,"IHDR",ihdr,13);chunk(f,"IDAT",out,(unsigned)n);
    chunk(f,"IEND",NULL,0);assert(fclose(f)==0);
}
int main(void)
{
    char root[]="/tmp/avp_r328_XXXXXX",dir[256],name[320],exe[320];
    const char *folders[]={"hi_res_props","hi_res_enemies","hi_res_weapons","hi_res_hud"};
    static const uint8_t sheet0[]={0,2,0,0,0,0,0,12,0,0,0,12,0,1,0,0};
    static const uint8_t sheet1[]={0,1,0,0,0,0,0,8,0,1,0,0};
    const uint8_t *aaf[63]={0};size_t size[63]={0};
    AvpPcHiResSprites *pack=calloc(1,sizeof(*pack));assert(pack);
    aaf[0]=sheet0;aaf[1]=sheet1;size[0]=sizeof(sheet0);size[1]=sizeof(sheet1);
    assert(mkdtemp(root));
    for(unsigned i=0;i<4;i++){
        assert(snprintf(dir,sizeof(dir),"%s/%s",root,folders[i])<(int)sizeof(dir));
        assert(mkdir(dir,0700)==0);
    }
    assert(snprintf(name,sizeof(name),"%s/hi_res_props/00_000.png",root)<(int)sizeof(name));png(name,1);
    assert(snprintf(name,sizeof(name),"%s/hi_res_enemies/00_001.png",root)<(int)sizeof(name));png(name,1);
    assert(snprintf(name,sizeof(name),"%s/hi_res_enemies/01_000.png",root)<(int)sizeof(name));png(name,1);
    assert(snprintf(name,sizeof(name),"%s/hi_res_props/00_002.png",root)<(int)sizeof(name));png(name,1);
    assert(snprintf(name,sizeof(name),"%s/hi_res_weapons/133.png",root)<(int)sizeof(name));png(name,1);
    assert(snprintf(name,sizeof(name),"%s/hi_res_weapons/184.png",root)<(int)sizeof(name));png(name,0); /* rejected */
    assert(snprintf(name,sizeof(name),"%s/hi_res_hud/185.png",root)<(int)sizeof(name));png(name,1);
    assert(snprintf(exe,sizeof(exe),"%s/avp_pc",root)<(int)sizeof(exe));
    assert(avp_pc_hi_res_sprites_load(pack,exe,aaf,size)==5u);
    assert(pack->images.prop_count==1&&pack->images.enemy_count==2);
    assert(pack->images.weapon_count==1&&pack->images.hud_count==1);
    assert(pack->images.props[0][0]!=NULL&&pack->images.props[0][1]==NULL);
    assert(pack->images.props[0][2]==NULL); /* outside original frame count */
    assert(pack->images.enemies[0][1]&&pack->images.enemies[1][0]);
    assert(pack->images.enemies[0][0]==NULL);
    assert(pack->images.weapons[0]!=NULL&&pack->images.weapons[51]==NULL);
    assert(pack->images.hud[0]!=NULL&&pack->images.hud[2]==NULL);
    assert(pack->images.props[0][0]->alpha[0]==0u);
    assert(pack->images.props[0][0]->alpha[1]==200u);
    assert(pack->decoded_bytes==5u*4u*4u*4u);
    assert(avp_pc_hi_res_sprites_load(pack,exe,aaf,size)==5u); /* safe reload */
    avp_pc_hi_res_sprites_destroy(pack);
    assert(pack->decoded_bytes==0u&&pack->images.props[0][0]==NULL);
    assert(avp_pc_hi_res_sprites_load(pack,"invalid_without_separator",aaf,size)==0u);
    free(pack);
    const char *paths[]={"hi_res_props/00_000.png","hi_res_enemies/00_001.png",
        "hi_res_enemies/01_000.png","hi_res_props/00_002.png",
        "hi_res_weapons/133.png","hi_res_weapons/184.png","hi_res_hud/185.png"};
    for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);i++){
        assert(snprintf(name,sizeof(name),"%s/%s",root,paths[i])<(int)sizeof(name));
        assert(unlink(name)==0);
    }
    for(unsigned i=0;i<4;i++){
        assert(snprintf(dir,sizeof(dir),"%s/%s",root,folders[i])<(int)sizeof(dir));
        assert(rmdir(dir)==0);
    }
    assert(rmdir(root)==0);
    puts("R328 four external PNG directories, original ID/frame mappings, RGBA and missing-frame fallbacks PASS");
    return 0;
}
