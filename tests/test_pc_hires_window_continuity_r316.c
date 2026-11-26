/* R316 regression: an authored 128-column opening mask must be continuous
 * at the midpoint of each source column when displayed at host resolution. */
#include "jaguar/avp_gpu/hires_window_mask.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

static int8_t top_for(int col) { return (int8_t)(-45 + col / 4); }
static int8_t bottom_for(int col) { return (int8_t)(45 - col / 5); }
static void check_close(double a,double b,double tolerance)
{ assert(fabs(a-b)<=tolerance); }
int main(void)
{
    uint8_t pair[256];
    double t,b,t0,b0,t1,b1;
    unsigned comparisons=0;
    for(int i=0;i<128;i++){
        pair[2*i]=(uint8_t)top_for(i);
        pair[2*i+1]=(uint8_t)bottom_for(i);
    }
    for(int i=1;i<127;i++){
        const double at=(double)top_for(i), ab=(double)bottom_for(i);
        const double pt=(double)top_for(i-1), pb=(double)bottom_for(i-1);
        const double nt=(double)top_for(i+1), nb=(double)bottom_for(i+1);
        avp_gpu_hires_window_pair(pair,i,0.0,1,&t,&b);
        check_close(t,(at+pt)*0.5,1e-12);
        check_close(b,(ab+pb)*0.5,1e-12);
        avp_gpu_hires_window_pair(pair,i,0.5,1,&t,&b);
        check_close(t,at,1e-12);check_close(b,ab,1e-12);
        avp_gpu_hires_window_pair(pair,i,1.0,1,&t,&b);
        check_close(t,(at+nt)*0.5,1e-12);
        check_close(b,(ab+nb)*0.5,1e-12);
        for(int j=1;j<=499;j++){
            const double phase=(double)j/1000.0;
            avp_gpu_hires_window_pair(pair,i,phase,1,&t,&b);
            check_close(t,at+(pt-at)*(0.5-phase),1e-12);
            check_close(b,ab+(pb-ab)*(0.5-phase),1e-12);
            comparisons+=2;
        }
        avp_gpu_hires_window_pair(pair,i,0.5-1e-8,1,&t0,&b0);
        avp_gpu_hires_window_pair(pair,i,0.5,1,&t1,&b1);
        check_close(t0,t1,1e-6);check_close(b0,b1,1e-6);
        comparisons+=2;
    }
    /* Closed neighbor may not introduce fractional opening geometry. */
    pair[20*2]=20;pair[20*2+1]=20;
    avp_gpu_hires_window_pair(pair,19,0.75,1,&t,&b);
    check_close(t,(double)top_for(19),1e-12);
    check_close(b,(double)bottom_for(19),1e-12);
    avp_gpu_hires_window_pair(pair,20,0.50,1,&t,&b);
    check_close(t,20,1e-12);check_close(b,20,1e-12);
    /* The native 128-column mode never interpolates. */
    avp_gpu_hires_window_pair(pair,19,0.23,0,&t,&b);
    check_close(t,(double)top_for(19),1e-12);
    check_close(b,(double)bottom_for(19),1e-12);
    printf("R316 high-res window mask continuity and native fallback PASS (%u checks)\n",comparisons);
    return 0;
}
