#include "jaguar/host/jerry_wave_procedural.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double rms(const int16_t x[128])
{
    double mean=0.0, energy=0.0;
    for(unsigned i=0;i<128u;i++)mean+=x[i];
    mean/=128.0;
    for(unsigned i=0;i<128u;i++){
        double y=(double)x[i]-mean;
        energy+=y*y;
    }
    return sqrt(energy/128.0);
}

int main(void)
{
    int16_t a[8][128], b[8][128];
    avp_jerry_generate_waves(a);
    avp_jerry_generate_waves(b);
    assert(memcmp(a,b,sizeof(a))==0);
    for(unsigned i=0;i<128u;i++){
        double x=(double)(i+1u)/128.0;
        double th=2.0*M_PI*x;
        double tri=(i<64u)?16383.0*((double)(i+1u)/32.0-1.0):
                     16383.0*(3.0-(double)(i+1u)/32.0);
        double expected[5]={tri,32767.0*sin(th),
                            32767.0*sin(th)*(double)(i+1u)/100.0,
                            16383.5*(sin(th)+sin(2.0*th)),
                            32767.0*sin(2.0*M_PI*(x+15.0*x*x))};
        for(unsigned k=0;k<5u;k++){
            int ref=(int)expected[k];
            assert(abs((int)a[k][i]-ref)<=1);
        }
        {
            unsigned d=(i>=63u)?i-63u:63u-i;
            int delta=(d<4u)?(int)(32767.0*(4.0-(double)d)/4.0):0;
            assert((int)a[6][i]==delta);
        }
    }
    /* Hardware noise is non-recoverable without its on-chip sample data;
     * the default procedural replacement must be deterministic, near-zero
     * mean, and have hardware-scale energy (not the older full-scale noise). */
    {
        double noise=rms(a[7]);
        double ntri=rms(a[5]);
        double diff=0.0;
        assert(noise>6000.0&&noise<6200.0);
        assert(ntri>8800.0&&ntri<9250.0);
        for(unsigned i=0;i<128u;i++){
            double d=(double)a[5][i]-(double)a[0][i]*0.9518;
            diff+=d*d;
        }
        diff=sqrt(diff/128.0);
        assert(diff>450.0&&diff<600.0);
        assert(a[7][0]!=a[7][64]);
    }
    puts("PASS: six procedural JERRY math waveforms, independent noise energy, deterministic ROM-free playback");
    return 0;
}
