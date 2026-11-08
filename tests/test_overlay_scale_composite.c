/* Host-only overlay optimization parity: the reference is exactly the
 * pre-R122 per-output-pixel compare. No game/render diagnostics added. */
#include "jaguar/avp_gpu/overlay_scale_composite.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t seed=0x9143abcd;
static uint32_t next_rand(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}

static void check(unsigned w,unsigned h,unsigned mask)
{
    const size_t n=(size_t)320u*228u;
    uint32_t *before=malloc(n*sizeof(uint32_t)),*after=malloc(n*sizeof(uint32_t));
    uint32_t *reference=malloc((size_t)w*h*sizeof(uint32_t));
    uint32_t *changed=malloc((size_t)w*h*sizeof(uint32_t));
    assert(before&&after&&reference&&changed);
    for(size_t i=0;i<n;i++){
        before[i]=next_rand()&0x00ffffffu;
        after[i]=before[i];
        if(mask==1u ? i==0u :
           mask==2u ? i==n-1u :
           mask==3u ? (i%319u)==0u :
           mask==4u ? (next_rand()&63u)==0u :
           mask==5u ? (next_rand()&3u)==0u :
           mask==6u ? 1 : 0)after[i]^=0x00b35731u;
    }
    for(size_t i=0;i<(size_t)w*h;i++)
        reference[i]=changed[i]=next_rand()&0x00ffffffu;
    /* The old high-res compositor is the reference, not a guessed scale. */
    for(unsigned y=0;y<h;y++){
        unsigned ny=(unsigned)(((unsigned long long)y*228u)/h);
        for(unsigned x=0;x<w;x++){
            unsigned nx=(unsigned)(((unsigned long long)x*320u)/w);
            size_t i=(size_t)ny*320u+nx;
            if(before[i]!=after[i])reference[(size_t)y*w+x]=after[i];
        }
    }
    avp_gpu_overlay_scaled_composite(changed,w,h,before,after);
    assert(memcmp(changed,reference,(size_t)w*h*sizeof(uint32_t))==0);
    free(before);free(after);free(reference);free(changed);
}
int main(void)
{
    static const unsigned dims[][2]={
        {1,1},{160,114},{319,227},{320,228},{321,229},
        {640,480},{960,684},{1280,912},{1920,1080},{2560,1440},
        {1600,900},{3440,1440}
    };
    for(unsigned i=0;i<sizeof(dims)/sizeof(dims[0]);i++)
        for(unsigned mask=0;mask<=6;mask++)
            check(dims[i][0],dims[i][1],mask);
    puts("Host high-resolution OP composition matches original pixel compare at 12 dimensions and 7 coverage patterns PASS");
    return 0;
}
