/* R385: exact native-ink HUD replay differential, including sparse corners,
 * irregular host dimensions and multiple blend/opaque fallback cases. */
#define _POSIX_C_SOURCE 200809L
#include "jaguar/avp_gpu/texture_filter.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static uint32_t hash(uint32_t x)
{
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;
    return x;
}
static uint32_t ink_apply(void *ctx,uint32_t c,uint16_t ink)
{
    (void)ctx;
    unsigned r=(c>>16u)&255u,g=(c>>8u)&255u,b=c&255u;
    r=(r+((ink>>8u)&3u))>255u?255u:r+((ink>>8u)&3u);
    g=(g+(ink&31u))>255u?255u:g+(ink&31u);
    b=(b+((ink>>6u)&3u))>255u?255u:b+((ink>>6u)&3u);
    return (r<<16u)|(g<<8u)|b;
}
static int modified(unsigned x,unsigned y,unsigned style)
{
    switch(style){
    case 0:return 0;
    case 1:return (x==0u&&y==0u)||(x==319u&&y==227u)||
        (x==0u&&y==227u)||(x==319u&&y==0u);
    case 2:return ((x>=10u&&x<75u)&&(y<38u||y>=201u))||
        ((x>=200u&&x<275u)&&(y>=83u&&y<115u));
    case 3:return ((x*17u+y*43u)%127u)==0u;
    default:return (x+y)%3u==0u;
    }
}
static void run(unsigned w,unsigned h,unsigned style)
{
    const size_t native=320u*228u,sz=(size_t)w*h;
    uint32_t *before=malloc(native*sizeof(*before));
    uint32_t *after=malloc(native*sizeof(*after));
    AvpHudRmwOps *ops=calloc(native,sizeof(*ops));
    uint32_t *expected=malloc(sz*sizeof(*expected));
    uint32_t *actual=malloc(sz*sizeof(*actual));
    uint32_t *reference_mask=calloc(sz,sizeof(*reference_mask));
    uint32_t *actual_mask=calloc(sz,sizeof(*actual_mask));
    assert(before&&after&&ops&&expected&&actual&&reference_mask&&actual_mask);
    for(unsigned y=0;y<228u;y++)for(unsigned x=0;x<320u;x++){
        size_t i=(size_t)y*320u+x;
        before[i]=hash((uint32_t)i+0x5151u)&0xffffffu;
        after[i]=before[i];
        if(!modified(x,y,style))continue;
        unsigned type=(unsigned)(hash((uint32_t)i+style)&7u);
        uint16_t ink=(uint16_t)(1u+(hash((uint32_t)i)&0x3ffu));
        ops[i].count=(uint8_t)((type==0u)?255u:(type==1u?2u:1u));
        ops[i].ink[0]=ink;
        if(ops[i].count==2u)ops[i].ink[1]=(uint16_t)(ink+2u);
        if(ops[i].count==255u)after[i]=0x000f8429u;
        else if(ops[i].count==2u)after[i]=ink_apply(NULL,ink_apply(NULL,before[i],ink),ops[i].ink[1]);
        else after[i]=ink_apply(NULL,before[i],ink);
        if(type==2u)after[i]=0x00a0cd71u; /* explicit opaque-over-ink */
        if(type==3u)after[i]=before[i]; /* no native pixel change */
    }
    for(size_t i=0;i<sz;i++)expected[i]=actual[i]=hash((uint32_t)i+0x8a80u)&0xffffffu;
    assert(setenv("AVP_HUD_REPLAY_FULL_SCAN","1",1)==0);
    avp_filter_hud_replay(expected,w,h,before,after,ops,ink_apply,NULL,reference_mask,w);
    assert(unsetenv("AVP_HUD_REPLAY_FULL_SCAN")==0);
    avp_filter_hud_replay(actual,w,h,before,after,ops,ink_apply,NULL,actual_mask,w);
    if(memcmp(expected,actual,sz*sizeof(*expected))||
       memcmp(reference_mask,actual_mask,sz*sizeof(*actual_mask))){
        for(size_t i=0;i<sz;i++){
            if(expected[i]!=actual[i]||reference_mask[i]!=actual_mask[i]){
                fprintf(stderr,"R385 mismatch %ux%u style %u pixel %zu: %08x/%08x mask %u/%u\n",
                    w,h,style,i,expected[i],actual[i],reference_mask[i],actual_mask[i]);
                exit(1);
            }
        }
        exit(1);
    }
    free(before);free(after);free(ops);free(expected);free(actual);
    free(reference_mask);free(actual_mask);
}
int main(void)
{
    static const unsigned sizes[][2]={{320u,228u},{853u,480u},{1280u,800u},
                                      {1920u,1080u},{2560u,1440u},{513u,289u}};
    unsigned cases=0;
    for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)
        for(unsigned style=0;style<5u;style++){
            run(sizes[i][0],sizes[i][1],style);cases++;
        }
    printf("R385: %u source-output and background-mask differential cases PASS\n",cases);
    return 0;
}
