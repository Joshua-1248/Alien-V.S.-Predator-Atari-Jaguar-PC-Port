#include "../presentation_fast.h"
#include "../maze_interp_fast.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static uint32_t rng=1234;
static uint32_t next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){
    uint8_t a[32768],b[32768],c[32768],d[32768];
    size_t tested=0;
    for(unsigned j=0;j<1024;j++){
        size_t n=(1u+next()%4096u)*8u;
        double alpha=(double)(next()%65537u)/65536.0;
        for(size_t i=0;i<n;i++){a[i]=(uint8_t)next();b[i]=next()%4u?a[i]:(uint8_t)next();}
        memcpy(c,a,n);
        for(size_t o=0;o<n;o+=8u)for(unsigned k=6;k<8;k++){
            double q=(double)b[o+k]+((double)a[o+k]-(double)b[o+k])*alpha;
            if(q<0)q=0;
            if(q>255)q=255;
            c[o+k]=(uint8_t)(q+0.5);
        }
        avp_micro_maze_interp(d,a,b,n,alpha);
        assert(memcmp(c,d,n)==0);tested+=n;
    }
    for(unsigned w=0;w<8;w++)for(unsigned h=0;h<8;h++)for(unsigned iw=0;iw<8;iw++)for(unsigned ih=0;ih<8;ih++)for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++){
        assert(avp_micro_full_cover(1,x,y,w,h,iw,ih)==(w&&h&&w==iw&&h==ih&&x==0&&y==0));
        assert(!avp_micro_full_cover(0,x,y,w,h,iw,ih));
    }
    AvpMicroTitleCache t={0};assert(!avp_micro_title_same(&t,1,"x"));avp_micro_title_remember(&t,1,1,"x");assert(avp_micro_title_same(&t,1,"x"));assert(!avp_micro_title_same(&t,0,"x"));assert(!avp_micro_title_same(&t,1,"y"));avp_micro_title_remember(&t,0,1,"x");assert(!avp_micro_title_same(&t,1,"x"));avp_micro_title_remember(&t,1,0,"x");assert(!t.valid);
    printf("PASS: %zu maze bytes; coverage predicate and title transitions\n",tested);
}
