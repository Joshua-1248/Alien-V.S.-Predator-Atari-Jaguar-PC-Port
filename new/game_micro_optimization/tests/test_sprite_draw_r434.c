#include "../../jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* In the real game this is maintained by the original maze module. */
s16 sprite_rescale=128;

static void put16(u8 *p,unsigned v){p[0]=(u8)(v>>8);p[1]=(u8)v;}
static void put32(u8 *p,unsigned v){p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;}
/* One-frame, 8-column synthetic AAF image with selected empty columns. */
static size_t make_aaf(u8 *out,size_t capacity,unsigned variant)
{
    const unsigned cols=8u, frame=8u, head=2u+2u*cols;
    unsigned used=frame+head;
    assert(capacity>=frame+head+8u*2u*(60u+1u));
    memset(out,0,capacity);
    put16(out,1);put32(out+4,frame);
    out[frame]= (u8)(int8_t)-4;
    out[frame+1u]=4u;
    for(unsigned c=0;c<cols;c++){
        if(c==(variant%8u))continue;
        put16(out+frame+2u+2u*c,used-frame);
        out[used]=(u8)(int8_t)-30;
        out[used+1u]=30u;
        for(unsigned sy=0;sy<60u;sy++){
            unsigned v=(c*3277u+sy*719u+variant*131u)&0xffffu;
            if((c+sy+variant)%7u==0u)v=0;
            if((c+sy+variant)%29u==0u)v=1;
            put16(out+used+2u+2u*sy,v);
        }
        used+=122u;
    }
    return used;
}

static double bench_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void bench_draw(AvpAvpGpuState *state,u8 *aaf,size_t capacity){
    const unsigned w=2560,h=1440;size_t n=(size_t)w*h;
    AvpJaguarFramebuffer fb={w,h,calloc(n,sizeof(u32))};
    AvpMaze2Occlusion oc={w,h,malloc(n*sizeof(float))};
    double *zbuf=malloc(w*sizeof(double));assert(fb.pixels&&oc.depth&&zbuf);
    AvpAmp actor={0};actor.creature=AC_ALIEN;actor.animseq=AS_STAND;
    const PcSpriteChoice ch=sprite_choice(&actor,0,0);assert(ch.aaf>=0);
    memset(state->aaf,0,sizeof(state->aaf));memset(state->aaf_size,0,sizeof(state->aaf_size));
    state->aaf[ch.aaf]=aaf;state->aaf_size[ch.aaf]=make_aaf(aaf,capacity,1);
    sprite_rescale=128;
    const double ranges[]={0.25,0.5,1.0,2.0};
    for(unsigned d=0;d<4;d++)for(int mask=0;mask<2;mask++){
        actor.xpos=(s32)(ranges[d]*65536.0);
        for(size_t k=0;k<n;k++)oc.depth[k]=mask&&k%37u<7u?0.1f:FLT_MAX;
        for(unsigned x=0;x<w;x++)zbuf[x]=mask&&x%53u<5u?0.1:12.0;
        memset(fb.pixels,0,n*sizeof(u32));double total[2]={0,0};
        for(int rep=0;rep<4;rep++){
            int mode=(rep==1||rep==2);state->host_game_micro_optimization=mode;
            double t=bench_now();for(unsigned i=0;i<100;i++)draw_amp_sprite(state,&fb,&actor,0,0,1,0,0,1,w*0.5,zbuf,&oc,NULL,0);
            total[mode]+=bench_now()-t;
        }
        size_t painted=0;for(size_t k=0;k<n;k++)painted+=fb.pixels[k]!=0;assert(painted>0);
        printf("draw range=%.2f mask=%d painted=%zu base=%.6f candidate=%.6f ratio=%.3f\n",ranges[d],mask,painted,total[0],total[1],total[0]/total[1]);
    }
    free(fb.pixels);free(oc.depth);free(zbuf);
}

int main(int argc,char **argv)
{

    /* Compare the actual current sprite drawer with the optimization off/on
     * at native, odd widescreen and 1440p dimensions. Synthetic AAF data. */
    const unsigned sizes[][2]={{320,228},{1279,721},{2560,1440}};
    const double ranges[]={0.05,0.12,0.35,0.9,1.7,3.5,7.9};
    const unsigned actors[]={AC_ALIEN,AC_PREDATOR,AC_HUMAN,AC_SPARK1,AC_EGG,AC_ACRAWL};
    const int offsets[]={-60,-7,0,37,96};
    unsigned long long comparisons=0,pixels=0,painted=0;
    u8 aaf[4096];
    AvpAvpGpuState *state=(AvpAvpGpuState*)calloc(1,sizeof(*state));
    assert(state);
    for(unsigned si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++){
        unsigned w=sizes[si][0],h=sizes[si][1];
        const size_t pixel_count=(size_t)w*h;
        AvpJaguarFramebuffer a={w,h,NULL},b={w,h,NULL};
        AvpMaze2Occlusion oc={w,h,NULL};
        double *zbuf=(double*)malloc((size_t)w*sizeof(double));
        a.pixels=(u32*)malloc(pixel_count*sizeof(u32));
        b.pixels=(u32*)malloc(pixel_count*sizeof(u32));
        oc.depth=(float*)malloc(pixel_count*sizeof(float));
        assert(a.pixels&&b.pixels&&oc.depth&&zbuf);
        for(unsigned variant=0;variant<168u;variant++){
            AvpAmp actor={0};
            unsigned creature=actors[variant%6u], target=0u;
            const double distance=ranges[(variant/6u)%7u];
            const unsigned oc_mode=(variant/42u)%4u;
            const int dx=(int)((variant*47u)%21u)-10;
            const unsigned aaf_id=creature==AC_ALIEN?8u:creature==AC_PREDATOR?29u:
                creature==AC_HUMAN?55u:creature==AC_SPARK1?47u:creature==AC_EGG?20u:33u;
            actor.creature=(u16)creature;
            actor.animseq=AS_STAND;
            if(creature==AC_HUMAN)actor.animseq=AS_RUN;
            actor.yoffset=(s16)offsets[(variant/3u)%5u];
            actor.angle=(u16)((variant&1u)?0u:0x8000u);
            actor.xpos=(s32)(distance*65536.0);
            actor.ypos=(s32)(dx*65536.0/14.0);
            sprite_rescale=(s16)((variant%4u==0u)?48u:(variant%4u==1u)?128u:(variant%4u==2u)?256u:400u);
            if(creature==AC_HUMAN){
                /* Dynamic Marine-facing sprite art depends on the viewing angle. */
                double look=atan2((double)actor.ypos,(double)actor.xpos);
                double face=-(double)actor.angle*(2.0*M_PI/65536.0),rel=look-face;
                while(rel<-M_PI)rel+=2.0*M_PI;
                while(rel>M_PI)rel-=2.0*M_PI;
                target=fabs(rel)<M_PI/4.0?56u:fabs(rel)>3.0*M_PI/4.0?24u:55u;
            }else target=aaf_id;
            size_t len=make_aaf(aaf,sizeof(aaf),variant);
            memset(state->aaf,0,sizeof(state->aaf));
            memset(state->aaf_size,0,sizeof(state->aaf_size));
            state->aaf[target]=aaf;
            state->aaf_size[target]=len;
            for(size_t k=0;k<pixel_count;k++){
                a.pixels[k]=(u32)(k*2654435761u+variant*31337u);
                oc.depth[k]=(k%11u<3u)?(float)0.1f:12.0f;
            }
            memcpy(b.pixels,a.pixels,pixel_count*sizeof(u32));
            for(unsigned x=0;x<w;x++)zbuf[x]=(x%13u<4u)?0.12:12.0;
            oc.width=oc_mode==2u?w+1u:w;
            const AvpMaze2Occlusion *owner=oc_mode==0u?NULL:&oc;
            if(oc_mode==3u)owner=NULL;
            state->host_game_micro_optimization=0;
            draw_amp_sprite(state,&a,&actor,0,0,1,0,0,1,(double)w*0.5,
                oc_mode==3u?NULL:zbuf,owner,NULL,0u);
            state->host_game_micro_optimization=1;
            draw_amp_sprite(state,&b,&actor,0,0,1,0,0,1,(double)w*0.5,
                oc_mode==3u?NULL:zbuf,owner,NULL,0u);
            if(memcmp(a.pixels,b.pixels,pixel_count*sizeof(u32))){
                fprintf(stderr,"sprite pixel difference width=%u height=%u case=%u species=%u\n",w,h,variant,creature);
                return 1;
            }
            comparisons++;pixels+=pixel_count;
            for(size_t k=0;k<pixel_count;k++)if(b.pixels[k]!=(u32)(k*2654435761u+variant*31337u))painted++;
        }
        free(a.pixels);free(b.pixels);free(oc.depth);free(zbuf);
    }
    if(argc>1){(void)argv;bench_draw(state,aaf,sizeof(aaf));}
    free(state);
    printf("R434 actual sprite draw micro toggle parity PASS (%llu complete-frame pairs, %llu pixel checks, %llu painted pixels)\n",comparisons,pixels,painted);
    assert(painted>0);
    return 0;
}
