#include "jaguar/avp_gpu/sprite_scratch.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define CHECK(q) do{if(!(q)){fprintf(stderr,"R390 UV FAIL line %d: %s\n",__LINE__,#q);return 1;}}while(0)
static uint32_t rng=0x390aaa4fu;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){
    AvpGpuSpriteScratch views[4]={{0}};
    uint64_t checked=0,retained=0;
    const int heights[]={228,456,720,1080,1440,2160,3000};
    for(int i=0;i<240;i++){
        AvpGpuSpriteScratch*s=&views[i&3];
        unsigned n=(unsigned)heights[(i/4)%7];
        double *old=s->sample_v;
        size_t old_capacity=s->sample_capacity;
        double *rows=avp_gpu_sprite_scratch_rows(s,n);
        CHECK(rows!=NULL);
        if(old_capacity>=n){CHECK(rows==old);retained++;}
        const double sy=(double)(1u+(rnd()%1600u))/19.0;
        const double origin=(double)(rnd()%3000u)/7.0-250.0;
        const int top=(int)(rnd()%256u)-128,span=(int)(rnd()%255u)+1;
        for(unsigned y=0;y<n;y++)
            s->sample_v[y]=(((double)y+0.5-origin)/sy-(double)top)/(double)span;
        for(int col=0;col<12;col++)for(unsigned y=0;y<n;y++){
            double reference=(((double)y+0.5-origin)/sy-(double)top)/(double)span;
            CHECK(memcmp(&reference,&s->sample_v[y],sizeof(double))==0);
            checked++;
        }
    }
    AvpGpuSpriteScratch copy=views[0];
    avp_gpu_sprite_scratch_detach(&copy);
    CHECK(!copy.sample_v&&!copy.sample_capacity);
    avp_gpu_sprite_scratch_destroy(&copy);
    for(unsigned i=0;i<4u;i++)avp_gpu_sprite_scratch_destroy(&views[i]);
    printf("R390 sprite: %llu bit-identical V samples, %llu retained allocations PASS\n",(unsigned long long)checked,(unsigned long long)retained);
    return 0;
}
