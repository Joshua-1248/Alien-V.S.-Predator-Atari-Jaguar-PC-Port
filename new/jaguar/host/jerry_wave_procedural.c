#include "jaguar/host/jerry_wave_procedural.h"
#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* A fixed-seed PRNG is only used to construct the noise-shaped waveforms.
 * It is not a copy of or a decoder for the Jaguar's 4 KiB on-chip ROM.
 * The finished 128 samples repeat as a phase-indexed oscillator, just as the
 * original FullSynth DSP reads the eight $F1D000..$F1DFFF tables. */
static uint32_t wave_rng(uint32_t *state)
{
    uint32_t v=*state;
    v^=v<<13;
    v^=v>>17;
    v^=v<<5;
    *state=v;
    return v;
}

static int16_t clamp_wave(double v)
{
    if(v>32767.0)v=32767.0;
    if(v< -32768.0)v=-32768.0;
    return (int16_t)v; /* Truncation toward zero matches original math tables. */
}

static double noise_unit(uint32_t *state)
{
    /* Three independent uniform draws approximate a centered bell-shaped
     * distribution with finite tails, unlike an over-amplified white square.
     * Variance is normalized below, independently of libc and random(). */
    double a=(double)((wave_rng(state)>>16)&65535u)-32767.5;
    double b=(double)((wave_rng(state)>>16)&65535u)-32767.5;
    double c=(double)((wave_rng(state)>>16)&65535u)-32767.5;
    return (a+b+c)/32768.0;
}

void avp_jerry_generate_waves(int16_t table[8][128])
{
    double n[128], t[128],mean=0.0,variance=0.0;
    /* Phase-zero noise sample controls the FM feedback rate of the retail
     * FX_newal instrument.  The old positive sample made its audible pitch
     * more than an octave too high.  This independent PRNG seed produces a
     * negative phase-zero sample at the observed hardware modulation level;
     * no on-chip table samples are stored or copied. */
    uint32_t seed=11988723u;
    unsigned i;
    if(!table)return;
    for(i=0;i<128u;i++){
        const double x=(double)(i+1u)/128.0;
        const double theta=2.0*M_PI*x;
        const double s=sin(theta);
        const unsigned dist=(i>=63u)?i-63u:63u-i;
        /* TRI is a signed, 14-bit triangle sampled one tick after zero.
         * The peak and trough are aligned with the hardware oscillator. */
        const double tri=(i<64u)?(16383.0*((double)(i+1u)/32.0-1.0)):
                          (16383.0*(3.0-(double)(i+1u)/32.0));
        table[0][i]=clamp_wave(tri);
        table[1][i]=clamp_wave(32767.0*s);
        table[2][i]=clamp_wave(32767.0*s*(double)(i+1u)/100.0);
        table[3][i]=clamp_wave(16383.5*(s+sin(2.0*theta)));
        /* CHIRP16 runs from a fundamental cycle to a 16-cycle quadratic
         * sweep: phi=2*pi*(x+15*x*x), not an arbitrary t*t chirp. */
        table[4][i]=clamp_wave(32767.0*sin(2.0*M_PI*(x+15.0*x*x)));
        table[6][i]=(dist<4u)?clamp_wave(32767.0*(double)(4u-dist)/4.0):0;
        n[i]=noise_unit(&seed);
        mean+=n[i];
    }
    mean/=128.0;
    for(i=0;i<128u;i++){
        double d=n[i]-mean;
        variance+=d*d;
    }
    variance/=128.0;
    /* The Jaguar NOISE table has approximately 6.1k signed PCM RMS;
     * previous fallback used full-scale uniform noise (~19k RMS).
     * The independent PRNG's calibrated initial sample keeps original
     * type-4 FX_newal feedback near its observed effective phase advance.
     * No pitch, oscillator arithmetic, or retail module data is rewritten. */
    {
        const double factor=(variance>0.0)?6098.0/sqrt(variance):0.0;
        for(i=0;i<128u;i++){
            double d=(n[i]-mean)*factor;
            table[7][i]=clamp_wave(d);
        }
    }
    /* NTRI follows the low-amplitude triangle almost exactly, with a small
     * independent high-frequency perturbation (~520 RMS).  The original
     * hardware's individual noise samples are not reconstructed here. */
    seed=0x9c42e7bdu;
    mean=0.0;variance=0.0;
    for(i=0;i<128u;i++){
        t[i]=noise_unit(&seed);
        mean+=t[i];
    }
    mean/=128.0;
    for(i=0;i<128u;i++){
        double d=t[i]-mean;
        variance+=d*d;
    }
    variance/=128.0;
    {
        const double factor=(variance>0.0)?519.0/sqrt(variance):0.0;
        for(i=0;i<128u;i++){
            table[5][i]=clamp_wave((double)table[0][i]*0.9518+(t[i]-mean)*factor);
        }
    }
}
