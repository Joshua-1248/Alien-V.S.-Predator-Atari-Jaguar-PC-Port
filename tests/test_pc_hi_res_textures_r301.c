#include "pc/pc_hi_res_textures.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A minimal stand-in for the original QCONV converter, which is tested by
 * the existing GPU/CRY suite. This test checks actual PNG bytes, lifetime,
 * slot indices, missing-file fallback, and per-panel 1254x1254 size. */
int avp_gpu_build_cry_image(AvpGpuRgbImage *im,const uint8_t *qconv,size_t qconv_size)
{
    size_t i,n;
    if(!im||!im->rgb||!qconv||qconv_size<131072u)return 0;
    n=(size_t)im->width*im->height;
    im->cry16=(uint16_t*)malloc(n*sizeof(*im->cry16));
    if(!im->cry16)return 0;
    for(i=0u;i<n;i++)im->cry16[i]=im->rgb[i*3u];
    return 1;
}
#define T(test) do{if(!(test)){fprintf(stderr,"R301 hi-res texture test failure: %s (%u)\n",#test,(unsigned)__LINE__);return 1;}}while(0)
int main(int argc,char **argv)
{
    static const unsigned ids[7]={0u,2u,4u,10u,11u,12u,13u};
    static const uint8_t rgb00[7][3]={
        {86,88,66},{58,51,37},{44,47,36},{58,57,47},
        {75,66,53},{109,99,77},{85,76,56}};
    static const uint8_t rgbmid[7][3]={
        {108,76,66},{253,253,252},{62,41,28},{0,0,0},
        {0,0,0},{87,72,49},{135,110,86}};
    AvpPcHiResTextures *pack;
    uint8_t *qconv;
    char exe[4096];
    if(argc!=2){fputs("Usage: test_pc_hi_res_textures_r301 SOURCE_DIR\n",stderr);return 2;}
    T(snprintf(exe,sizeof(exe),"%s/avp_pc",argv[1])<(int)sizeof(exe));
    pack=(AvpPcHiResTextures*)calloc(1,sizeof(*pack));
    qconv=(uint8_t*)calloc(1,131072u);
    T(pack&&qconv);
    /* R327 distribution policy: hi_res_textures/ is deliberately NOT
     * bundled. Validate clean zero-texture fallback when no external pack
     * is installed. Preserve full 74-file checks when a user-owned pack is
     * provided beside the executable under test. */
    unsigned loaded=avp_pc_hi_res_textures_load(pack,exe,qconv,131072u);
    T(loaded==0u || loaded==74u);
    T(pack->applied==loaded);
    if(loaded==74u){
    for(unsigned j=0u;j<7u;j++){
        const AvpGpuRgbImage *im=&pack->image[ids[j]];
        size_t middle=((size_t)627u*1254u+627u)*3u;
        T(im->width==1254&&im->height==1254);
        T(im->rgb&&im->cry16);
        T(memcmp(im->rgb,rgb00[j],3u)==0);
        T(memcmp(im->rgb+middle,rgbmid[j],3u)==0);
        T(im->cry16[0]==im->rgb[0]);
    }
    /* R318's completed set includes the full 00..73 pack. Sign source
     * components 56..73 have independent native aspect ratios and alpha. */
    for(unsigned i=0;i<74u;i++)T(pack->image[i].rgb&&pack->image[i].cry16);
    for(unsigned i=56u;i<74u;i++)T(pack->image[i].alpha!=NULL);
    } else {
        for(unsigned i=0;i<74u;i++)T(pack->image[i].rgb==NULL);
    }
    T(avp_pc_hi_res_textures_load(pack,"missing_executable_path",qconv,131072u)==0u);
    T(pack->applied==0u&&pack->image[0].rgb==NULL);
    T(avp_pc_hi_res_textures_load(pack,exe,qconv,0u)==0u);
    T(pack->image[2].rgb==NULL);
    T(avp_pc_hi_res_textures_load(pack,exe,qconv,131072u)==loaded);
    avp_pc_hi_res_textures_destroy(pack);
    T(pack->applied==0u&&pack->image[0].rgb==NULL);
    free(pack);free(qconv);
    puts(loaded==74u?
         "R327 external 74-panel/sign PNG mapping, fallback, lifetime PASS":
         "R327 no bundled PNGs: clean missing-pack fallback and lifetime PASS (asset pixel checks skipped)");
    return 0;
}
