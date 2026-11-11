/* The retail FX_newal is a type-4 FM instrument with both oscillators
 * pointing at JERRY NOISE.  Its modulator rate is ZERO and the 68000
 * installs feedback word $1164.  That makes NOISE's phase-zero sample a
 * CONTROL INPUT to the carrier frequency, not merely a timbre variation.
 *
 * ROM-free synthetic descriptor and DSP envelope: this regression catches
 * the loud, shrill R110 noise generator (positive first sample) without
 * embedding any Jaguar wavetable or commercial game assets. */
#define _DEFAULT_SOURCE 1
#include "../src/jaguar/host/fullsynth_host.c"
#include <assert.h>
#include <stdio.h>

static u8 sfx_rdb[0x5000];
static u8 env_text[0x4000];
u32 x_pos,y_pos,centre_angle;
int avp_resources_decode_rdb(const AvpRomImage *rom,u8 **data,size_t *n)
{
    (void)rom;
    *data=malloc(sizeof sfx_rdb);
    if(!*data)return 0;
    memcpy(*data,sfx_rdb,sizeof sfx_rdb);
    *n=sizeof sfx_rdb;
    return 1;
}
int avp_resources_decode_rdb_text(const AvpRomImage *rom,u8 **data,size_t *n)
{
    (void)rom;
    *data=malloc(sizeof env_text);
    if(!*data)return 0;
    memcpy(*data,env_text,sizeof env_text);
    *n=sizeof env_text;
    return 1;
}
int avp_resources_rom_descriptor(const AvpRomImage *rom,u32 addr,const u8 **data,size_t *n)
{
    (void)rom;(void)addr;(void)data;(void)n;
    return 0;
}
int main(void)
{
    const u32 addr=AVP_FX_NEWAL;
    const unsigned p10_text_off=0x9e98u-0x8000u+0xf1cc50u-0xf1bc00u;
    AvpRomImage rom={0};
    AvpHostedVoice *hv;
    u8 *p=sfx_rdb+(addr-0x20300u);
    int16_t pcm[4];
    int16_t boosted[4096],normal[4096];
    AvpHostedVoice initial;
    long long sum_boost=0,sum_normal=0;
    s32 original_wave_zero,original_modulation;
    u32 phase_rate;
    /* ROM values verified by the existing, independent R99 cartridge audit.
     * Test state is generated in memory; no retail ROM is linked. */
    put_be32(p,4u);
    put_be32(p+4u,0xf1de00u);
    put_be32(p+8u,0xf1de00u);
    put_be32(p+16u,0x28f00u);
    put_be32(p+24u,0x80004000u);
    put_be32(p+36u,0x1164u);
    put_be32(p+40u,0xf1cc50u);
    put_be32(p+44u,0x60u);
    put_be32(p+48u,0x130000u);
    put_be32(p+52u,0x130000u);
    put_be32(p+60u,0x130000u);
    put_be32(p+76u,0x3fffu);
    put_be32(env_text+p10_text_off,0x7fffu);
    put_be32(env_text+p10_text_off+4u,0x7fffu);
    assert(avp_jaguar_fullsynth_init(&rom));
    original_wave_zero=host_wave_table[7][0];
    /* Phase-zero modulation must be NEGATIVE and hardware-like in size.
     * A symmetric RMS test alone failed to catch this R110 regression. */
    assert(original_wave_zero<-4100&&original_wave_zero>-4250);
    original_modulation=host_wave_sample(0xf1de00u,0u);
    assert(original_modulation<0);
    /* The original DSP uses signed-low-16 IMULT followed by SHARQ #7.
     * Modulator rate=0, so this feedback is constant for every sample. */
    phase_rate=(u32)(0x28f00u+(s32)avp_jerry_asr64(
       (int64_t)dsp_low16_signed((u32)original_modulation)*0x1164,7u));
    assert(phase_rate>0x5000u&&phase_rate<0x6500u);
    avp_jaguar_fullsynth_play_handle_d1(addr,7u,2u,0,0,-1);
    hv=&F.v[3u];
    assert(hv->synth.active&&hv->game_voice==2u);
    assert(hv->synth.p[6]==0x80001000u);
    avp_jaguar_fullsynth_mix(pcm,2);
    assert(hv->synth.active);
    assert(hv->synth.p[5]==(u32)(phase_rate-0x28f00u));
    assert((hv->synth.p[3]&0x7fffffu)==((0x28f00u*2u+phase_rate-0x28f00u)&0x7fffffu));
    /* Same synthesized alarm, once with the R114 host-only alarm adjustment,
     * once as an otherwise identical ordinary effect voice. Neither route
     * changes original DSP phase, feedback or voice ownership. */
    initial=*hv;
    avp_jaguar_fullsynth_mix(boosted,2048u);
    *hv=initial;
    hv->sound_id=0u; /* emulate another effect for the volume-gate test */
    avp_jaguar_fullsynth_mix(normal,2048u);
    for(unsigned k=0;k<4096u;k++){
        sum_boost+=(boosted[k]<0?-(long long)boosted[k]:boosted[k]);
        sum_normal+=(normal[k]<0?-(long long)normal[k]:normal[k]);
    }
    assert(sum_normal>0);
    /* This checks a meaningful alarm-only lift without excessive clipping
     * or wrapping; the retail recording determines the final subjective trim. */
    assert(sum_boost>sum_normal*3);
    assert(sum_boost<sum_normal*7);
    avp_jaguar_fullsynth_shutdown();
    printf("PASS: alarm-only host-volume gain; boosted PCM / baseline=%.2f\n",
           (double)sum_boost/(double)sum_normal);
    return 0;
}
