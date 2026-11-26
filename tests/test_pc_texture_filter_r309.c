/* R309: accelerated HUD/first-person filtering must retain the exact
 * original R308 host compositing outcome, including transparent scene pixels. */
#include "jaguar/avp_gpu/texture_filter.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t apply_ink(void *unused,uint32_t c,uint16_t ink)
{
    (void)unused;
    unsigned r=(c>>16)&255u,g=(c>>8)&255u,b=c&255u;
    r=(r+((ink>>8)&3u))>255u?255u:r+((ink>>8)&3u);
    g=(g+(ink&31u))>255u?255u:g+(ink&31u);
    b=(b+((ink>>6)&3u))>255u?255u:b+((ink>>6)&3u);
    return (r<<16)|(g<<8)|b;
}
static void ref_weapon(uint32_t*host,unsigned w,unsigned h,
                       const uint32_t*before,const uint32_t*after)
{
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        double tx=((double)x+0.5)*320.0/w-0.5,ty=((double)y+0.5)*228.0/h-0.5;
        int nx=(int)floor(tx),ny=(int)floor(ty);
        double fx=tx-nx,fy=ty-ny;
        double ac=0,rc=0,gc=0,bc=0;
        for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
            int xx=nx+dx,yy=ny+dy;
            if(xx<0||yy<0||xx>=320||yy>=228)continue;
            size_t at=(size_t)yy*320u+(unsigned)xx;
            if(before[at]==after[at])continue;
            double wt=(dx?fx:1.0-fx)*(dy?fy:1.0-fy);
            uint32_t c=after[at];
            ac+=wt;rc+=wt*((c>>16)&255u);gc+=wt*((c>>8)&255u);bc+=wt*(c&255u);
        }
        if(ac>0.0){
            size_t at=(size_t)y*w+x;
            uint32_t bg=host[at];
            unsigned a=(unsigned)(ac*255.0+0.5);
            unsigned r=(unsigned)(rc/ac+0.5),g=(unsigned)(gc/ac+0.5),b=(unsigned)(bc/ac+0.5);
            unsigned pr=(r*a+((bg>>16)&255u)*(255u-a)+127u)/255u;
            unsigned pg=(g*a+((bg>>8)&255u)*(255u-a)+127u)/255u;
            unsigned pb=(b*a+(bg&255u)*(255u-a)+127u)/255u;
            host[at]=(pr<<16)|(pg<<8)|pb;
        }
    }
}
static void ref_hud(uint32_t*host,unsigned w,unsigned h,
                    const uint32_t*before,const uint32_t*after,
                    const AvpHudRmwOps *ops,uint32_t *mask)
{
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        double tx=((double)x+0.5)*320.0/w-0.5;
        double ty=((double)y+0.5)*228.0/h-0.5;
        int nx=(int)floor(tx),ny=(int)floor(ty);
        double fx=tx-nx,fy=ty-ny;
        size_t host_at=(size_t)y*w+x;
        uint32_t backdrop=host[host_at];
        double rr=0,gg=0,bb=0;
        int changed=0;
        for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++){
            int xx=nx+dx,yy=ny+dy;
            double weight=(dx?fx:1.0-fx)*(dy?fy:1.0-fy);
            uint32_t c=backdrop;
            if(xx>=0&&yy>=0&&xx<320&&yy<228){
                size_t i=(size_t)yy*320u+(unsigned)xx;
                const AvpHudRmwOps *p=&ops[i];
                int replay_ok=p->count>0u&&p->count<=AVP_HUD_REPLAY_MAX_INKS;
                if(replay_ok){
                    uint32_t simulated=before[i];
                    for(unsigned z=0;z<p->count;z++)simulated=apply_ink(NULL,simulated,p->ink[z]);
                    if(simulated!=after[i])replay_ok=0;
                }
                if(replay_ok){
                    for(unsigned z=0;z<p->count;z++)c=apply_ink(NULL,c,p->ink[z]);
                    changed=1;
                }else if(after[i]!=before[i]){c=after[i];changed=1;}
            }
            rr+=weight*((c>>16)&255u);
            gg+=weight*((c>>8)&255u);
            bb+=weight*(c&255u);
        }
        if(changed){
            host[host_at]=((unsigned)(rr+0.5)<<16)|
                          ((unsigned)(gg+0.5)<<8)|(unsigned)(bb+0.5);
            mask[host_at]=1u;
        }
    }
}
static uint32_t hash32(uint32_t x)
{
    x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;
    return x;
}
static void compare(const char *test,const uint32_t *actual,
                    const uint32_t *want,size_t n)
{
    for(size_t i=0;i<n;i++)if(actual[i]!=want[i]){
        fprintf(stderr,"%s mismatch at %zu: %08x vs %08x\n",test,i,actual[i],want[i]);
        exit(1);
    }
}
static void run(unsigned w,unsigned h)
{
    const size_t nat=320u*228u,count=(size_t)w*h;
    uint32_t *before=malloc(nat*sizeof(uint32_t));
    uint32_t *after=malloc(nat*sizeof(uint32_t));
    AvpHudRmwOps *ops=calloc(nat,sizeof(AvpHudRmwOps));
    uint32_t *actual=malloc(count*sizeof(uint32_t));
    uint32_t *expected=malloc(count*sizeof(uint32_t));
    uint32_t *amask=calloc(count,sizeof(uint32_t));
    uint32_t *emask=calloc(count,sizeof(uint32_t));
    assert(before&&after&&ops&&actual&&expected&&amask&&emask);
    for(unsigned y=0;y<228u;y++)for(unsigned x=0;x<320u;x++){
        size_t at=(size_t)y*320u+x;
        before[at]=hash32((uint32_t)at*13u+7u)&0xffffffu;
        after[at]=before[at];
        if(x>23u&&x<292u&&y>22u&&y<206u){
            uint16_t ink=(uint16_t)(11u+((x/48u)&3u)*256u);
            ops[at].count=1u;ops[at].ink[0]=ink;
            after[at]=apply_ink(NULL,before[at],ink);
            if((x+y)%37u==0u){
                after[at]=0x00112233u; /* opaque fallback over native ink */
            }
        }
        if((x+y)%191u==0u){
            ops[at].count=2u;
            ops[at].ink[0]=7u;ops[at].ink[1]=19u;
            after[at]=apply_ink(NULL,apply_ink(NULL,before[at],7u),19u);
        }
        if(x>=130u&&x<190u&&y>=116u&&y<214u){
            after[at]=0x00beef22u; /* weapon foreground */
        }
    }
    for(size_t i=0;i<count;i++){
        actual[i]=expected[i]=hash32((uint32_t)i*37u+7u)&0xffffffu;
    }
    ref_weapon(expected,w,h,before,after);
    avp_filter_native_overlay(actual,w,h,before,after);
    compare("weapon filtering",actual,expected,count);
    ref_hud(expected,w,h,before,after,ops,emask);
    avp_filter_hud_replay(actual,w,h,before,after,ops,apply_ink,NULL,amask,w);
    compare("HUD filtering",actual,expected,count);
    compare("HUD background mask",amask,emask,count);
    free(before);free(after);free(ops);free(actual);free(expected);free(amask);free(emask);
}
int main(void)
{
    run(640u,456u);run(1280u,800u);run(1600u,900u);
    puts("R309: original R308 weapon/HUD pixel and alpha-mask equivalence PASS");
    return 0;
}
