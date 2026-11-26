#include "../hud_replay_fast.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static u32 rng=0x4112026u;
static u32 next(void){rng=rng*1664525u+1013904223u;return rng^(rng>>16);}
static u8 q[131072];static u32 rgb[65536],before[AVP_HUD_NATIVE_PIXELS],after[AVP_HUD_NATIVE_PIXELS];
static AvpHudRmwOps ops[AVP_HUD_NATIVE_PIXELS];
static uint32_t ref(void *ctx,uint32_t dst,uint16_t ink){(void)ctx;return avp_micro_cry_rmw(dst,ink,q,sizeof q,rgb);}
int main(void){
    for(unsigned i=0;i<65536;i++){u8 r,g,b;avp_jaguar_cry16_to_rgb((u16)i,&r,&g,&b);rgb[i]=((u32)r<<16)|((u32)g<<8)|b;}
    for(unsigned i=0;i<sizeof q;i++)q[i]=(u8)(next()>>24);
    const unsigned dims[][2]={{1,1},{319,227},{320,228},{641,457},{1280,720},{1920,1080},{2560,1440},{2571,1439},{3840,2160}};
    unsigned long long pixels=0;
    for(unsigned k=0;k<sizeof dims/sizeof dims[0];k++)for(unsigned trial=0;trial<4;trial++){
        unsigned w=dims[k][0],h=dims[k][1],stride=w+7,pitch=w+17,left=9;size_t n=(size_t)w*h,m=(size_t)stride*h,world=(size_t)pitch*h;
        u32 *a=malloc(n*4),*b=malloc(world*4),*ma=malloc(m*4),*mb=malloc(m*4);assert(a&&b&&ma&&mb);
        for(size_t i=0;i<world;i++)b[i]=0xdeadbeefu;
        for(size_t i=0;i<n;i++)a[i]=b[(i/w)*pitch+left+i%w]=(trial&1)?(next()&0xffffff):((i/w/3+i%w/7)%11)*0x123456u;
        avp_micro_hud_sample_strided(before,b+left,w,h,pitch);
        for(unsigned y=0;y<228u;y++)for(unsigned x=0;x<320u;x++)
            assert(before[y*320u+x]==a[(size_t)(((uint64_t)(2u*y+1u)*h)/456u)*w+((uint64_t)(2u*x+1u)*w)/640u]);
        for(size_t i=0;i<m;i++)ma[i]=mb[i]=0xfeedfaceu;
        memset(ops,0,sizeof ops);
        for(size_t i=0;i<AVP_HUD_NATIVE_PIXELS;i++){
            after[i]=before[i];unsigned count=next()%13;
            if(count==0){if(next()&1)after[i]=next();continue;}
            if(count>8){if(count==9)ops[i].count=255;else continue;after[i]=next();continue;}
            ops[i].count=(u8)count;
            for(unsigned j=0;j<count;j++){ops[i].ink[j]=(u16)next();after[i]=ref(NULL,after[i],ops[i].ink[j]);}
            if((next()&15)==0)after[i]=next();
        }
        AvpMicroHudReplay p={0};p.host=b+left;p.host_stride=pitch;p.mask=(trial&2)?mb:NULL;p.before=before;p.after=after;p.ops=ops;p.rgb=rgb;p.q=q;p.qsize=sizeof q;p.width=w;p.stride=stride;
        avp_gpu_hud_replay_scaled_composite(a,w,h,before,after,ops,ref,NULL,(trial&2)?ma:NULL,stride);
        avp_micro_hud_replay_init(&p,h);
        /* Reverse row dispatch order, including uneven bounds, to verify
         * workers' output ranges are disjoint and independent. */
        avp_micro_hud_replay_rows(&p,171,228);avp_micro_hud_replay_rows(&p,51,171);avp_micro_hud_replay_rows(&p,0,51);
        for(unsigned y=0;y<h;y++){
            assert(!memcmp(a+(size_t)y*w,b+(size_t)y*pitch+left,w*4u));
            for(unsigned x=0;x<left;x++)assert(b[(size_t)y*pitch+x]==0xdeadbeefu);
            for(unsigned x=left+w;x<pitch;x++)assert(b[(size_t)y*pitch+x]==0xdeadbeefu);
        }
        assert(!memcmp(ma,mb,m*4));pixels+=n;
        free(a);free(b);free(ma);free(mb);
    }
    printf("PASS: %llu exact pitched HUD pixels, native sampling, masks, guard margins and row partitions\n",pixels);return 0;
}
