#define _POSIX_C_SOURCE 200809L
#include "local_mp_compositor.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec+(double)t.tv_nsec*1e-9;}
int main(int argc,char **argv)
{
    const unsigned players=(argc>1)?(unsigned)atoi(argv[1]):4u;
    const unsigned layout=(argc>2)?(unsigned)atoi(argv[2]):0u;
    const unsigned sw=(argc>3)?(unsigned)atoi(argv[3]):320u;
    const unsigned sh=(argc>4)?(unsigned)atoi(argv[4]):228u;
    const unsigned dw=(argc>5)?(unsigned)atoi(argv[5]):1920u;
    const unsigned dh=(argc>6)?(unsigned)atoi(argv[6]):1080u;
    const unsigned three=(argc>7)?(unsigned)atoi(argv[7]):0u;
    unsigned round,i,p,x,y;uint64_t verify=0u;
    AvpLocalMpSetup cfg;AvpLocalMpSourceFrame frames[4]={{0}};
    AvpLocalMpOutputFrame output={0};
    avp_local_mp_defaults(&cfg);cfg.players=players;cfg.layout=layout;cfg.three_layout=three;
    if(players<1||players>4||!sw||!sh||!dw||!dh)return 2;
    for(p=0;p<players;p++){
        uint32_t *pix=malloc((size_t)sw*sh*sizeof(uint32_t));if(!pix)return 3;
        frames[p]=(AvpLocalMpSourceFrame){pix,sw,sh,sw};
        for(y=0;y<sh;y++)for(x=0;x<sw;x++)pix[(size_t)y*sw+x]=(p<<24)^(x<<12)^y;
    }
    output.pixels=malloc((size_t)dw*dh*sizeof(uint32_t));if(!output.pixels)return 3;
    output.width=dw;output.height=dh;output.pitch=dw;
    for(round=0;round<4;round++){
        const double start=seconds();
        for(i=0;i<90;i++){
            if(avp_local_mp_compose(&cfg,frames,&output,0x00bcdeafu)!=players)return 4;
            verify+=(uint64_t)output.pixels[((i*23u)%dh)*dw+((i*31u)%dw)];
        }
        printf("round %u: %.4f ms/frame\n",round,(seconds()-start)*1000.0/90.0);
    }
    printf("check %llu\n",(unsigned long long)verify);
    for(p=0;p<players;p++)free((void*)frames[p].pixels);
    free(output.pixels);
    return 0;
}
