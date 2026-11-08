#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include "collision.h"
static uint64_t h=UINT64_C(14695981039346656037);
static void mix(uint32_t v) { for(int i=0;i<4;i++){h^=(v>>(i*8))&255u;h*=UINT64_C(1099511628211);} }
int main(void){
    static const int32_t cases[]={INT32_MIN,INT32_MIN+1,-0x10001,-0x10000,-1,0,1,0x10000,0x10001,INT32_MAX-1,INT32_MAX};
    uint32_t count=0;
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
    for(unsigned j=0;j<sizeof(cases)/sizeof(cases[0]);j++)
    for(unsigned k=0;k<sizeof(cases)/sizeof(cases[0]);k++)
    for(unsigned l=0;l<sizeof(cases)/sizeof(cases[0]);l++){
        AvpXY a={cases[i],cases[j]},b={cases[k],cases[l]};
        int32_t vx,vy; uint16_t angle=Angle(&a,&b);Vector(&a,&b,&vx,&vy);
        mix(angle);mix((uint32_t)vx);mix((uint32_t)vy);count++;
    }
    if(count != 14641u || h != UINT64_C(0x02c31d0d8ce78ddb)){
        fprintf(stderr,"Angle/Vector R250 parity mismatch: vectors=%u hash=%016llx\n",count,(unsigned long long)h);
        return 1;
    }
    printf("Angle/Vector R251: 14641 signed-boundary vectors match R250\n");
    return 0;
}
