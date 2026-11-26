/* R388 pixel-coordinate parity: hoist the horizontal first-person image UV
 * without changing floating-point sample positions on any output row. */
#include "jaguar/avp_gpu/fp_column_uv.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint32_t seed=0xcafefeedu;
static uint32_t rnd(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void)
{
    double uv[AVP_FP_COLUMN_UV_CACHE_MAX];
    uint64_t comparisons=0;
    for(unsigned case_num=0;case_num<30000u;case_num++){
        int width=(int)(rnd()%4096u)+1;
        int x0=(int)(rnd()%1280u)-640;
        int x1=x0+width;
        double left=(double)((int)(rnd()%2001u)-1000)/3.0;
        double sx=((double)(rnd()%3000u)+1.0)/239.0;
        int nx=(int)(rnd()%521u)-260;
        int sw=(int)(rnd()%2048u)+1;
        if(!avp_gpu_fp_column_uv_fill(uv,4096,x0,x1,left,sx,nx,sw))abort();
        for(int x=x0;x<x1;x++){
            double old=(((double)x+0.5-left)/sx-nx)/sw;
            if(memcmp(&old,&uv[x-x0],sizeof(double))!=0){
                fprintf(stderr,"R388 FP UV mismatch case=%u x=%d\n",case_num,x);
                return 1;
            }
            comparisons++;
        }
    }
    if(avp_gpu_fp_column_uv_fill(uv,4096,-100,4097,0.0,1.0,0,128))abort();
    printf("R388 first-person UV: %llu bitwise-equivalent columns PASS\n",
           (unsigned long long)comparisons);
    return 0;
}
