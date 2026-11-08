/* Tiny synthetic FullSynth ROM fixture: no commercial audio or ROM bytes.
 * Checks music stream address decoding and simultaneous note scheduling. */
#include "jaguar/host/fullsynth_host.h"
#include "avp_resources.h"
#include "avp_sfx_ids.h"
#include "maze.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static u8 rdb[16384],wave[128],dsp_text[0x4000];
u32 x_pos,y_pos,centre_angle;
static void write32(u8*p,u32 v){p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;}
int avp_resources_decode_rdb(const AvpRomImage*rom,u8**data,size_t*n)
{(void)rom;*data=(u8*)malloc(sizeof(rdb));if(!*data)return 0;memcpy(*data,rdb,sizeof(rdb));*n=sizeof(rdb);return 1;}
int avp_resources_decode_rdb_text(const AvpRomImage*rom,u8**data,size_t*n)
{(void)rom;*data=(u8*)malloc(sizeof(dsp_text));if(!*data)return 0;memcpy(*data,dsp_text,sizeof(dsp_text));*n=sizeof(dsp_text);return 1;}
int avp_resources_rom_descriptor(const AvpRomImage*rom,u32 addr,const u8**data,size_t*n)
{(void)rom;(void)addr;(void)data;(void)n;return 0;}

static int mix_case(AvpRomImage*rom,unsigned notes,int16_t *stereo)
{
    u8*score=rom->data+0x200000u;
    int16_t samples[2]={0,0};
    u32 ev=0x80000000u|(4096u<<7)|1u;
    memset(score,0,32);
    write32(score,0u);write32(score+4,ev);
    if(notes==2u){write32(score+8,0u);write32(score+12,ev|(1u<<26));}
    write32(score+notes*8u,0x7fffffffu);
    avp_jaguar_fullsynth_stop_all();
    avp_jaguar_fullsynth_start_music(0x20380u);
    avp_jaguar_fullsynth_mix(samples,1u);
    {s32 l=0,r=0;
     avp_jaguar_fullsynth_meter_samples(&l,&r);
     assert(l==samples[0]&&r==samples[1]);}
    avp_jaguar_fullsynth_stop_music();
    if(stereo){stereo[0]=samples[0];stereo[1]=samples[1];}
    if(!stereo)assert(samples[0]>0&&samples[1]>0);
    assert(samples[0]==samples[1]||abs((int)samples[0]-(int)samples[1])<=4);
    return samples[0]+samples[1];
}


/* The FM module's two waveform POINTERS must choose the actual requested
 * oscillator shape.  A MIDI/FX voice set to TRI or NOISE may not become SINE.
 * Keep this fixture out of the game binary. */
static void fm_wave_case(unsigned descriptor,int16_t *out,unsigned frames)
{
    avp_jaguar_fullsynth_stop_all();
    avp_jaguar_fullsynth_play(descriptor,0,0);
    avp_jaguar_fullsynth_mix(out,frames);
}

int main(void)
{
    AvpRomImage rom;int one,two;
    memset(&rom,0,sizeof(rom));
    rom.size=4u*1024u*1024u;rom.data=(u8*)calloc(1,rom.size);assert(rom.data);
    memset(wave,60,sizeof(wave));
    memcpy(rom.data+0x100000u,wave,sizeof(wave));
    /* Music descriptor points to direct ROM data at $A00000, not an RDB
       address.  The PCM patch uses a compressed-sampler voice. */
    write32(rdb+0x80,0xa00000u);write32(rdb+0x84,0x20400u);
    rdb[0x90]=3;rdb[0x91]=231; /* PIT divider 999 => one tick/frame */
    write32(rdb+0x100,32u);write32(rdb+0x104,0x7fffu);
    write32(rdb+0x108,0x900000u);write32(rdb+0x10c,256u);
    write32(rdb+0x110,32000u);write32(rdb+0x14c,0x4000u);
    /* Retail compressed effects refer to pointer cells, and the word after
       each cell may be sample data rather than an 8-byte descriptor count. */
    write32(rom.data+0x800u,0x900000u);
    write32(rom.data+0x804u,0xdeadc0deu);
    /* Level 1 selects the sample in the retail-style 15-entry table. */
    write32(rdb+0x130u,0x20500u);
    write32(rdb+0x200u,32u);
    write32(rdb+0x204u,0x7fffu);
    write32(rdb+0x208u,0x800800u);
    write32(rdb+0x20cu,256u);
    write32(rdb+0x210u,0x7fffu);
    write32(rdb+0x214u,0x7000u); /* ambient sample loop length */
    write32(rdb+0x24cu,0x4000u);
    /* A synthetic DSP RAM envelope at F1C894; none of the Jaguar hardware
     * wave ROM data is included in this test's program or fixture. */
    for(unsigned i=0;i<40u;i++)write32(dsp_text+0x2b2cu+i*4u,0x7fffu);
    for(unsigned voice=0;voice<3u;voice++){
        u8 *p=rdb+0x300u+voice*0x80u;
        write32(p,4u);
        write32(p+4u,voice==1u?0xf1d000u:voice==2u?0xf1de00u:0xf1d200u);
        write32(p+8u,0xf1d200u);
        write32(p+16u,0x30000u);
        write32(p+24u,0x7fffu);
        write32(p+40u,0xf1c894u);
        write32(p+44u,0x10000u);
        write32(p+48u,0x200000u);
        write32(p+52u,0x200000u);
        write32(p+76u,0x4000u);
    }
    /* Source-derived FM carrier interpolation fixture, loaded once with RDB. */
    memcpy(rdb+0x900u,rdb+0x300u,80u);
    write32(rdb+0x900u+16u,0x200000u); /* carrier phase 32 << 16 */
    memcpy(rdb+0x950u,rdb+0x900u,80u);
    write32(rdb+0x950u+16u,0x20ff00u); /* fraction 255 of next sample */
    /* R169: Original FM DSP F1B374/378 multiplies envelope by volume and
     * shifts that register BEFORE F1B37C/38A multiplies the oscillator.
     * The former host ordered waveform*volume before *envelope, which
     * rounds differently.  A deliberately off-peak sine phase and a 10000
     * volume expose the one-unit output difference.  Table and wavetable
     * values here are generated fixtures, never cartridge media. */
    {
        u8 *p=rdb+0x2d00u;
        memcpy(p,rdb+0x900u,80u);
        write32(p+12u,0x80000u);     /* oscillator phase, sine index 8 */
        write32(p+16u,0u);          /* oscillator increment */
        /* R272: TFX stores a type-4 module word LSR.W #2 ($9D40), so the
         * module word $9C40 (40000) reaches the DSP as volume 10000. */
        write32(p+24u,40000u);      /* module word -> DSP volume 10000 */
        write32(p+28u,0u);          /* modulator phase */
        write32(p+32u,0u);          /* modulator increment */
        write32(p+36u,0u);          /* feedback baseline */
        write32(p+44u,0u);          /* envelope increment */
        write32(p+48u,0x200000u);   /* envelope range */
        write32(p+64u,0u);          /* feedback depth */
        write32(p+76u,0x7fffu);     /* full right pan */
    }
    /* R170: Unusual synthetic pan descriptor states deliberately exercise
     * the signed low-half IMULT inputs and the r20=0x7fff-r19 subtraction.
     * Bit15 of pan is a signed bit, not a value to mask off.  The two
     * descriptors differ only in upper 16 bits, which IMULT discards. */
    for(unsigned k=0;k<2u;k++){
        u8 *p=rdb+0x2d50u+0x50u*k;
        memcpy(p,rdb+0x2d00u,80u);
        write32(p+76u,k?0x12348000u:0x00008000u);
    }
    /* R163 synthetic FM feedback: Jaguar IMULT uses the *signed low 16*
     * bits of both operands.  Descriptor p[16] with an additional high bit
     * must therefore give the same waveform as the identical low half.
     * p[9] tests the same property after feedback-depth addition. */
    for(unsigned k=0;k<4u;k++){
        u8 *p=rdb+0x1400u+0x50u*k;
        memcpy(p,rdb+0x300u,80u);
        write32(p+12u,0x1e0000u);   /* carrier phase */
        write32(p+16u,0x10000u);    /* carrier rate */
        write32(p+28u,0x200000u);   /* modulator phase */
        write32(p+32u,0x10000u);    /* modulator rate */
        write32(p+36u,(k>=2u?0x18000u:0x8000u)); /* feedback baseline */
        write32(p+64u,(k&1u)?0x18000u:0x8000u); /* feedback depth */
    }
    /* R165: FM's envelope-end CMP/JR tests the 32-bit subtraction's N flag.
     * The 0xffffffff bound appears larger under an unsigned C comparison,
     * but Jaguar CMP(end - position) sets N and retires this nonlooping
     * synthetic FM descriptor immediately.  The normal bound is audible. */
    for(unsigned k=0;k<3u;k++){
        u8 *p=rdb+0x1b00u+0x50u*k;
        memcpy(p,rdb+0x900u,80u);
        write32(p+44u,0x10000u); /* envelope rate */
        write32(p+48u,k==0u?0x200000u:
                       k==1u?0xffffffffu:0x80000000u); /* end register */
    }
    /* R166: Both Jaguar compressed-sample DSP handlers use CMP end-position
     * minus advanced-position, with JR cc20 (negative flag clear), rather
     * than unsigned position > end.  Our tiny ROM-backed sample occupies
     * just two bytes.  This synthetic register-wrap state is deliberately
     * outside normal retail playback; it verifies branch semantics without
     * including copyrighted audio. */
    for(unsigned k=0;k<4u;k++){
        u8 *p=rdb+0x2100u+0x50u*k;
        write32(p,32u);                 /* un-enveloped sample module */
        write32(p+4u,0x80007fffu);      /* looping, full normal gain */
        write32(p+8u,0x900000u);        /* synthesized sample bytes */
        write32(p+12u,k&1u?0u:0x100u); /* advancement */
        write32(p+16u,0x100u);         /* position end */
        write32(p+20u,0xfffffd00u);    /* 32-bit loop offset */
        write32(p+24u,k&1u?0xfffffe00u:0u); /* initial phase */
        write32(p+76u,0x4000u);
    }
    /* R164: The original FM envelope path loads complete DSP longwords but
     * IMULTN/IMACN at F1B36A/36C multiply their signed lower 16-bit halves.
     * Both synthetic envelopes must render the same PCM even when one table's
     * high halves differ.  No actual ROM or JERRY waveform samples are used. */
    for(unsigned k=0;k<2u;k++){
        u8 *p=rdb+0x1900u+0x50u*k;
        memcpy(p,rdb+0x900u,80u);
        write32(p+40u,k?0xf1c994u:0xf1c894u);
    }
    for(unsigned i=0;i<40u;i++)write32(dsp_text+0x2c2cu+i*4u,0x00017fffu);
    /* R168: compressed+envelope DSP at F1B442/F1B444 and F1B446/F1B448
     * uses signed-low-16 IMULT followed by SHARQ #15 at both stages.
     * Adding bit31 to the accumulator changes its signed >>15 value by
     * 65536 but leaves the multiplier's 16-bit operand identical.
     * These use only synthetic 60-byte sample values and descriptors. */
    for(unsigned k=0;k<2u;k++){
        u8 *p=rdb+0x2700u+k*0x50u;
        memset(p,0,80u);
        write32(p,44u);
        write32(p+4u,0xc0007fffu);  /* active looping envelope */
        write32(p+8u,0x900000u);   /* synthetic sample bytes */
        write32(p+12u,0u);         /* constant sample phase */
        write32(p+16u,0x10000u);   /* end */
        write32(p+44u,k?0xa0000000u:0x20000000u); /* same signed low16 >>15 */
        write32(p+48u,0u);         /* no envelope advance */
        write32(p+52u,0x10000u);   /* no terminal transition */
        write32(p+76u,0x4000u);   /* pan */
    }
    /* MAIN/RDB.TXT installs BSS $2EEC0/$2EEC4 from the cocoon sample
     * pointer cell at $815E44, with offsets $3A98 and $3E80.  The
     * descriptors cannot be decoded as ordinary ROM pointer cells. */
    write32(rom.data+0x15e44u,0x900000u);
    for(unsigned i=0;i<64u;i++){
        rom.data[0x103a98u+i]=30u;
        rom.data[0x103e80u+i]=60u;
    }
    for(unsigned i=0;i<2u;i++){
        u8 *p=rdb+0x600u+i*0x50u;
        write32(p,32u);
        write32(p+4u,0x7fffu);
        write32(p+8u,i?0x2eec4u:0x2eec0u);
        write32(p+12u,0x100u);
        write32(p+16u,0x3fffu);
        write32(p+76u,0x4000u);
    }
    /* Generator FX_force is a ROM-backed compressed sample in the reserved
     * alarm channel.  Its retail descriptor already has the loop flag. */
    {
        u8 *p=rdb+(AVP_FX_FORCE-0x20300u);
        write32(p,32u);write32(p+4u,0x80002000u);
        write32(p+8u,0x800800u);write32(p+12u,0x100u);
        write32(p+16u,0x7fffu);write32(p+20u,0x7000u);
        write32(p+76u,0x4000u);
    }
    /* Explosion FX_xp1b15 uses the same mutable DSP descriptor word 3
     * across separate starts. Its synthetic sample is not game media. */
    {
        u8 *p=rdb+(AVP_FX_XP1B1-0x20300u);
        write32(rom.data+0x1000u,0x910000u);
        for(unsigned i=0;i<512u;i++)rom.data[0x110000u+i]=(u8)(10u+(i*13u)%110u);
        write32(p,32u);write32(p+4u,0x7fffu);write32(p+8u,0x801000u);
        write32(p+12u,0x100u);write32(p+16u,0x1ff00u);
        write32(p+76u,0x4000u);
    }
    /* A known retail vocal descriptor with synthetic waveform data, kept
     * outside the ranges of all other synthetic RDB fixture descriptors. */
    memcpy(rdb+(AVP_FX_B1-0x20300u),rdb+0x200u,80u);
    assert(avp_jaguar_fullsynth_init(&rom));
    /* The optional JERRY ROM loader must reject truncated and mismatched
     * files. Never ship or embed proprietary hardware wave tables in tests. */
    {
        const char *name="/tmp/avp-r193-invalid-jerry-wave.bin";
        u8 blank[4096]={0};
        FILE *f=fopen(name,"wb");
        assert(f && fwrite(blank,1,sizeof(blank),f)==sizeof(blank));
        assert(fclose(f)==0);
        assert(!avp_jaguar_fullsynth_load_wave_rom(name));
        assert(!avp_jaguar_fullsynth_uses_exact_waves());
        f=fopen(name,"wb");
        assert(f && fwrite(blank,1,4095u,f)==4095u);
        assert(fclose(f)==0);
        assert(!avp_jaguar_fullsynth_load_wave_rom(name));
        assert(!avp_jaguar_fullsynth_uses_exact_waves());
        assert(remove(name)==0);
    }
    {
        int16_t near_pcm[64],far_pcm[64],generator_pcm[64],repeat_pcm[64];
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_play_explosion(192);
        avp_jaguar_fullsynth_mix(near_pcm,32u);
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_play_explosion(-1);
        avp_jaguar_fullsynth_mix(far_pcm,32u);
        assert(memcmp(near_pcm,far_pcm,sizeof(near_pcm))==0);
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_play_explosion(170);
        avp_jaguar_fullsynth_mix(generator_pcm,32u);
        assert(memcmp(near_pcm,generator_pcm,sizeof(near_pcm))!=0);
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_play(AVP_FX_XP1B1,0,0);
        avp_jaguar_fullsynth_mix(repeat_pcm,32u);
        assert(memcmp(generator_pcm,repeat_pcm,sizeof(generator_pcm))==0);
        avp_jaguar_fullsynth_stop_all();
    }
    {
        int16_t out[128];
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_play_handle(AVP_FX_FORCE,0xfaceu,2u,0,0);
        assert(avp_jaguar_fullsynth_handle_active(0xfaceu));
        avp_jaguar_fullsynth_mix(out,64u);
        assert(out[0]!=0||out[1]!=0);
        /* Update an existing loop without stopping it or changing handle. */
        avp_jaguar_fullsynth_update_generator_alarm(0,0xe0);
        avp_jaguar_fullsynth_mix(out,64u);
        for(unsigned i=0;i<128u;i++)assert(out[i]==0);
        assert(avp_jaguar_fullsynth_handle_active(0xfaceu));
        avp_jaguar_fullsynth_update_generator_alarm(0x1a00,0x80);
        avp_jaguar_fullsynth_mix(out,64u);
        assert(out[0]!=0||out[1]!=0);
        assert(avp_jaguar_fullsynth_handle_active(0xfaceu));
        /* A different alarm in voice 2 must ignore stale force updates. */
        avp_jaguar_fullsynth_play_handle(0x20500u,0xbabeu,2u,0,0);
        avp_jaguar_fullsynth_update_generator_alarm(0,0);
        avp_jaguar_fullsynth_mix(out,1u);
        assert(out[0]!=0||out[1]!=0);
        avp_jaguar_fullsynth_stop_all();
    }
    /* Pause is not a sound restart.  In retail MAZESCRN.S the second voice
     * and alarm's type are held across LoseSounds/RestoreSounds.  The hosted
     * synthesizer must not reset their oscillator/sample positions. */
    {
        int16_t expected[128], resumed[128];
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_play_handle(0x20500u,0x1001u,1u,0,0);
        avp_jaguar_fullsynth_play_handle(AVP_FX_FORCE,0x1002u,2u,0,0);
        {int16_t warm[64];avp_jaguar_fullsynth_mix(warm,32u);}
        avp_jaguar_fullsynth_pause_save();
        avp_jaguar_fullsynth_mix(expected,64u);
        avp_jaguar_fullsynth_stop_all();
        assert(!avp_jaguar_fullsynth_handle_active(0x1001u));
        assert(!avp_jaguar_fullsynth_handle_active(0x1002u));
        avp_jaguar_fullsynth_pause_restore(0x1001u,0x1002u);
        assert(avp_jaguar_fullsynth_handle_active(0x1001u));
        assert(avp_jaguar_fullsynth_handle_active(0x1002u));
        avp_jaguar_fullsynth_mix(resumed,64u);
        assert(memcmp(expected,resumed,sizeof(expected))==0);
        /* The snapshot is single-use. An unrelated later stop must not
         * resurrect a voice when there was no new pause. */
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_pause_restore(0x1001u,0x1002u);
        assert(!avp_jaguar_fullsynth_handle_active(0x1001u));
        assert(!avp_jaguar_fullsynth_handle_active(0x1002u));
        avp_jaguar_fullsynth_play_handle(AVP_FX_FORCE,0x1003u,2u,0,0);
        avp_jaguar_fullsynth_pause_save();
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_pause_restore(0u,0xdddddu);
        assert(!avp_jaguar_fullsynth_handle_active(0x1003u));
    }
    {
        int16_t egg_pcm[64],computer_pcm[64];
        avp_jaguar_fullsynth_play(0x20900u,0,0);
        avp_jaguar_fullsynth_mix(egg_pcm,32u);
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_play(0x20950u,0,0);
        avp_jaguar_fullsynth_mix(computer_pcm,32u);
        avp_jaguar_fullsynth_stop_all();
        assert(egg_pcm[0]>0&&computer_pcm[0]>egg_pcm[0]);
        assert(memcmp(egg_pcm,computer_pcm,sizeof(egg_pcm))!=0);
    }
    /* The Jaguar DSP's CMP r22,r24 / JR cc20 is not an unsigned C >.
     * Compare one ordinary nonlooping FM envelope with an end register
     * whose high bit is set, using generated waveform/table data only. */
    {
        int16_t normal[2]={0,0},expired[2]={123,456},wrapped[2]={0,0};
        fm_wave_case(0x21e00u,normal,1u);
        fm_wave_case(0x21e50u,expired,1u);
        fm_wave_case(0x21ea0u,wrapped,1u);
        assert(normal[0]!=0 || normal[1]!=0);
        assert(expired[0]==0 && expired[1]==0);
        /* $80000000-$00010000 wraps to $7fff0000: N must be clear.
         * A replacement with a signed relational comparison is wrong. */
        assert(memcmp(wrapped,normal,sizeof(normal))==0);
    }
    /* With a normal phase, both descriptors play the generated sample.
     * The wrapped phase makes the original DSP CMP negative flag CLEAR;
     * the host's sample bounds then silence that unaddressable phase.
     * R165's unsigned comparison incorrectly subtracts a loop length and
     * plays a *different* valid sample in this case. */
    {
        int16_t inrange[2]={0,0},wrapped[2]={123,456};
        fm_wave_case(0x22400u,inrange,1u);
        fm_wave_case(0x22450u,wrapped,1u);
        assert(inrange[0]>0 && inrange[1]>0);
        assert(wrapped[0]==0 && wrapped[1]==0);
    }
    /* FM volume must be applied to the envelope BEFORE the oscillator. */
    {
        int16_t pcm[2]={0,0};
        fm_wave_case(0x23000u,pcm,1u);
        assert(pcm[0]==0 && pcm[1]==3794);
    }
    /* R170: The original DSP loads the complete pan longword, computes
     * (0x7fff-pan) in a 32-bit register, then IMULT observes the signed
     * low 16 bits of each coefficient.  A masking host implementation
     * incorrectly turns 0x8000 (negative) into 0 (hard left). */
    {
        int16_t low[2]={0,0},upper[2]={0,0};
        fm_wave_case(0x23050u,low,1u);
        fm_wave_case(0x230a0u,upper,1u);
        assert(memcmp(low,upper,sizeof(low))==0);
        assert(low[0]<0 && low[1]<0);
        assert(low[1]<low[0]);
    }
    /* R164: Envelope table entries use signed 16-bit multiplication even
     * though their memory cells and load registers are 32 bits wide. */
    {
        int16_t base[128],upper[128];
        fm_wave_case(0x21c00u,base,64u);
        fm_wave_case(0x21c50u,upper,64u);
        assert(memcmp(base,upper,sizeof(base))==0);
        assert(base[0]!=0 || base[1]!=0);
    }
    /* R163: The original FM handler executes signed low-half IMULT at
     * F1B376 for depth and F1B382 for feedback phase.  Adding 0x10000 to
     * the corresponding register must not change the generated PCM. The
     * test covers all subsequent samples, not just the initialization. */
    {
        int16_t fm0[64u*2u],fm1[64u*2u],fm2[64u*2u],fm3[64u*2u];
        fm_wave_case(0x21700u,fm0,64u);
        fm_wave_case(0x21750u,fm1,64u);
        fm_wave_case(0x217a0u,fm2,64u);
        fm_wave_case(0x217f0u,fm3,64u);
        assert(memcmp(fm0,fm1,sizeof(fm0))==0);
        assert(memcmp(fm0,fm2,sizeof(fm0))==0);
        assert(memcmp(fm0,fm3,sizeof(fm0))==0);
        assert(fm0[0]!=0 || fm0[1]!=0);
    }
    /* R162: Original FullSynth FM interpolation at F1B494/496/498 uses
     * (0xff-fraction)*current + fraction*next, then SHARQ #8.
     * At a sine peak and fraction zero this is 255/256 of the current
     * wave value, NOT the full value that R161's 0x100 coefficient gave.
     * All input waveforms here are mathematically synthesized test data. */
    {
        int16_t at_peak[2]={0,0};
        int16_t at_fraction[2]={0,0};
        fm_wave_case(0x20c00u,at_peak,1u);
        fm_wave_case(0x20c50u,at_fraction,1u);
        /* R272: this fixture's type-4 module word $7FFF is stored by TFX
         * as $1FFF ($9D40 LSR.W #2), so the DSP volume input is a quarter
         * of the former full-scale value (former expectations 16252/16253
         * and 16233/16234 assumed the unshifted word).  The interpolation
         * arithmetic under test is unchanged. */
        assert(at_peak[0]==4061&&at_peak[1]==4061);
        assert(at_fraction[0]==4056&&at_fraction[1]==4057);
    }
    {
        int16_t sin_pcm[64],tri_pcm[64],noise_pcm[64];
        fm_wave_case(0x20600u,sin_pcm,32u);
        fm_wave_case(0x20680u,tri_pcm,32u);
        fm_wave_case(0x20700u,noise_pcm,32u);
        assert(memcmp(sin_pcm,tri_pcm,sizeof(sin_pcm))!=0);
        assert(memcmp(sin_pcm,noise_pcm,sizeof(sin_pcm))!=0);
        assert(memcmp(tri_pcm,noise_pcm,sizeof(sin_pcm))!=0);
    }
    one=mix_case(&rom,1u,NULL);two=mix_case(&rom,2u,NULL);
    /* R47 host mix controls operate on independent music/SFX/ambience/
     * vocal buses. Unity remains the exact original DSP output. A mute
     * must advance voice clocks and output silence, not simply stop the
     * active sound. This test uses only synthetic samples and scores. */
    {
        int16_t pcm[2]={13,29};
        avp_jaguar_fullsynth_set_audio_mix(0u,100u,100u,100u,100u,1,1,1);
        assert(mix_case(&rom,1u,pcm)==0&&pcm[0]==0&&pcm[1]==0);
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,0,1,1);
        assert(mix_case(&rom,1u,pcm)==0&&pcm[0]==0&&pcm[1]==0);
        avp_jaguar_fullsynth_set_audio_mix(50u,100u,100u,100u,100u,1,1,1);
        assert(mix_case(&rom,1u,pcm)>0);
        assert(pcm[0]>0&&pcm[0]<one);
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,1,1);
        assert(mix_case(&rom,1u,pcm)==one);
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_set_audio_mix(100u,0u,100u,100u,100u,1,1,1);
        avp_jaguar_fullsynth_play(0x20500u,0,0);
        avp_jaguar_fullsynth_mix(pcm,1u);
        assert(pcm[0]==0&&pcm[1]==0);
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,1,1);
        avp_jaguar_fullsynth_mix(pcm,1u);
        assert(pcm[0]>0&&pcm[1]>0); /* same active SFX, not restarted */
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,0u,100u,1,1,1);
        avp_jaguar_fullsynth_play(AVP_FX_B1,0,0);
        avp_jaguar_fullsynth_mix(pcm,1u);
        assert(pcm[0]==0&&pcm[1]==0);
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,1,1);
        avp_jaguar_fullsynth_mix(pcm,1u);
        assert(pcm[0]>0&&pcm[1]>0);
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,0,1);
        avp_jaguar_fullsynth_start_ambient(1u);
        avp_jaguar_fullsynth_mix(pcm,1u);
        assert(pcm[0]==0&&pcm[1]==0);
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,0u,100u,100u,1,1,1);
        avp_jaguar_fullsynth_mix(pcm,1u);
        assert(pcm[0]==0&&pcm[1]==0);
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,1,1);
        avp_jaguar_fullsynth_mix(pcm,1u);
        assert(pcm[0]>0&&pcm[1]>0);
        avp_jaguar_fullsynth_stop_all();
    }
    avp_jaguar_fullsynth_play(0x20500u,0,0);
    {int16_t pcm[2]={0,0};avp_jaguar_fullsynth_mix(pcm,1u);assert(pcm[0]>100&&pcm[1]>100);}
    avp_jaguar_fullsynth_stop_all();
    assert(two>=one*2-4&&two<=one*2+4); /* chords, not staggered */
    /* R160: original FullSynth's voice handlers IMULT the unshifted pan
     * coefficients and ADD to a shared channel register (r9/r26).  Only at
     * F1B100/F1B102 does the DSP SHARQ #15 the complete channel sum.
     * Two quiet simultaneous notes distinguish this from rounding each
     * separate voice down first.  Synthetic sample byte 3 decodes to 9. */
    {
        int16_t low[2]={0,0};
        memset(rom.data+0x100000u,3u,128u);
        assert(mix_case(&rom,2u,low)>0);
        assert(low[0]==2&&low[1]==3);
        memset(rom.data+0x100000u,60u,128u);
    }
    /* Level ambience must come from the selected compressed-sample module,
       remain active across its loop point, and be separately stoppable. */
    avp_jaguar_fullsynth_stop_all();
    avp_jaguar_fullsynth_start_ambient(1u);
    {int16_t out[512u*2u];avp_jaguar_fullsynth_mix(out,512u);
     assert(out[0]>0&&out[1]>0);}
    {int16_t out[2]={0,0};avp_jaguar_fullsynth_mix(out,1u);
     assert(out[0]>0&&out[1]>0);}
    avp_jaguar_fullsynth_stop_ambient();
    {int16_t out[2]={111,222};avp_jaguar_fullsynth_mix(out,1u);
     assert(out[0]==0&&out[1]==0);}
    /* Same raw sample address must also work for normal SFX.  Once its
       sample ends, the 68000 allocator must observe an inactive handle. */
    avp_jaguar_fullsynth_play_handle(0x20400u,123u,3u,0,0);
    assert(avp_jaguar_fullsynth_handle_active(123u));
    {int16_t out[512u*2u];avp_jaguar_fullsynth_mix(out,512u);}
    assert(!avp_jaguar_fullsynth_handle_active(123u));
    avp_jaguar_fullsynth_play_handle(0x20400u,124u,3u,0,0);
    avp_jaguar_fullsynth_set_master_volume(0u);
    {int16_t out[2]={123,456};avp_jaguar_fullsynth_mix(out,1u);assert(out[0]==0&&out[1]==0);}
    avp_jaguar_fullsynth_set_master_volume(0x7fffu);
    {int16_t out[2]={0,0};avp_jaguar_fullsynth_mix(out,1u);assert(out[0]!=0||out[1]!=0);}
    /* Playback must continue after hundreds of repeated voice allocations.
     * This specifically catches a host mixer that leaves its voices permanently
     * unusable after one-shot handles have been recycled many times. */
    avp_jaguar_fullsynth_stop_all();
    for(unsigned i=0;i<512u;i++){
        int16_t repeated[64]={0};
        avp_jaguar_fullsynth_play_handle(0x20500u,0x3000u+i,3u,0,0);
        assert(avp_jaguar_fullsynth_handle_active(0x3000u+i));
        avp_jaguar_fullsynth_mix(repeated,32u);
        assert(repeated[0]!=0||repeated[1]!=0);
    }
    /* F1B100/102 shift 32-bit r26/r9 totals, and F1B10C/10E IMULT
     * uses only each shifted total's signed low 16 bits.  These synthetic
     * 16-voice cases check both the low-half IMULT narrowing (sample 60)
     * and a 32-bit stereo ADD carry wrap (sample 100), with both signs.
     * The samples have no game/hardware-ROM provenance.  Each predicted
     * result follows dec8, 255/128 interpolation, 0x7fff sample gain,
     * 16 raw pan products, a 32-bit ADD, SHARQ 15, signed16 IMULT by
     * 0x7fff, and the final SHARQ 15. Unbounded 64-bit saturation fails. */
    {
        static const u8 fixtures[4]={60u,188u,100u,228u};
        static const int16_t expect[4][2]={
            {-8180,-8176},{8162,8159},{28277,28287},{-28295,-28304}
        };
        int16_t out[2];
        for(unsigned f=0u;f<4u;f++){
            memset(rom.data+0x100000u,fixtures[f],128u);
            avp_jaguar_fullsynth_stop_all();
            avp_jaguar_fullsynth_start_ambient(1u);
            for(unsigned j=0u;j<8u;j++)
                avp_jaguar_fullsynth_play_handle(0x20500u,0x6000u+j,j,0,0);
            for(unsigned j=0u;j<7u;j++)
                avp_jaguar_fullsynth_play(0x20500u,0,0);
            avp_jaguar_fullsynth_mix(out,1u);
            assert(out[0]==expect[f][0]);
            assert(out[1]==expect[f][1]);
        }
        memset(rom.data+0x100000u,60,128u);
        avp_jaguar_fullsynth_stop_all();
    }
    /* R168: Original DSP IMULT treats the envelope operands as signed 16,
     * not the full (signed or unsigned) 32-bit envelope register.  Both
     * voices must render the same nonzero PCM for every frame.  R167 does
     * not: its full-width multiply changes sign when bit31 is present. */
    {
        int16_t low[128],high[128];
        fm_wave_case(0x22a00u,low,64u);
        fm_wave_case(0x22a50u,high,64u);
        assert(low[0]>0&&low[1]>0);
        assert(memcmp(low,high,sizeof(low))==0);
    }
    /* R157: a compressed/envelope module adds 32-bit DSP registers, whose
     * result wraps across 0x7fffffff.  C signed overflow is undefined and
     * must not influence the rendered sample or depend on optimization.
     * Synthesize the descriptor locally; do not use any commercial audio. */
    {
        u8 *p=rdb+0x1100u;
        int16_t first[2]={0,0},second[2]={0,0};
        memset(p,0,80u);
        write32(p,44u);                 /* compressed sampler + envelope */
        write32(p+4u,0x80007fffu);     /* looping, full volume */
        write32(p+8u,0x900000u);       /* synthetic sample (direct ROM addr) */
        write32(p+12u,0u);             /* 8.8 sample position increment */
        write32(p+16u,0x10000u);       /* inclusive sample end */
        write32(p+44u,0x7fff0000u);   /* envelope phase near INT32_MAX */
        write32(p+48u,0x20000u);      /* crosses sign boundary at 1st tick */
        write32(p+52u,0x7fffu);       /* destination does not end envelope */
        write32(p+76u,0x4000u);       /* stereo centre */
        /* FullSynth caches a private copy of RDB descriptors at init. */
        avp_jaguar_fullsynth_shutdown();
        assert(avp_jaguar_fullsynth_init(&rom));
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_play(0x21400u,0,0);
        avp_jaguar_fullsynth_mix(first,1u);
        avp_jaguar_fullsynth_mix(second,1u);
        /* JERRY's subsequent IMULT consumes only the signed low 16 bits
         * of the shifted envelope.  After the register wraps, the first
         * two gains are 2 and 6: both quantize this small PCM to silence.
         * R157's former negative-output expectation came from incorrectly
         * multiplying the full signed 32-bit envelope instead. */
        assert(first[0]==0&&first[1]==0);
        assert(second[0]==0&&second[1]==0);
        avp_jaguar_fullsynth_stop_all();
    }
    /* Switch the score descriptor to the exact high-bit-tagged reference
       convention in the retail title/end music.  FullSynth's RDB copy is
       immutable during one initialization, so rebuild it after the edit. */
    avp_jaguar_fullsynth_shutdown();
    write32(rom.data+0x1000u,0xa00000u);
    write32(rdb+0x80u,0x80801000u);
    assert(avp_jaguar_fullsynth_init(&rom));
    assert(mix_case(&rom,2u,NULL)==two);
    avp_jaguar_fullsynth_shutdown();
    {
        /* The AVP.jag-only synthesizer must initialize its deterministic
         * internal wave shapes on every new session.  TRI, SINE and NOISE
         * must remain distinct, and no second hardware ROM is supplied. */
        int16_t tri_a[64],tri_b[64],sine[64],noise[64];
        assert(avp_jaguar_fullsynth_init(&rom));
        fm_wave_case(0x20600u, sine,32u);
        fm_wave_case(0x20680u, tri_a,32u);
        fm_wave_case(0x20700u, noise,32u);
        assert(memcmp(tri_a,sine,sizeof(tri_a))!=0);
        assert(memcmp(tri_a,noise,sizeof(tri_a))!=0);
        avp_jaguar_fullsynth_shutdown();
        assert(avp_jaguar_fullsynth_init(&rom));
        fm_wave_case(0x20680u,tri_b,32u);
        assert(memcmp(tri_a,tri_b,sizeof(tri_a))==0);
        avp_jaguar_fullsynth_shutdown();
    }
    /* Independent R52 PC ambient-hissing mix test. Regenerate FullSynth's
     * cached descriptor table instead of mutating an active sound bank. */
    {
        int16_t pcm[2]={0,0};
        /* Dedicated final fixture, after all earlier DSP equivalence tests.
         * descriptor-equivalence tests; those deliberately share offsets. */
        memcpy(rdb+(AVP_FX_AMB2-0x20300u),rdb+0x200u,80u);
        memcpy(rdb+(AVP_FX_AMB3-0x20300u),rdb+0x200u,80u);
        memcpy(rdb+(AVP_FX_AMB2X-0x20300u),rdb+0x200u,80u);
        assert(avp_jaguar_fullsynth_init(&rom));
        /* R52: PLAYER.S::BGSounds random hiss/swish cues, not level ambient.
         * Muting either bus must not inadvertently mute the other one.
         * Synthetic looping descriptor bytes are used here, not retail PCM. */
        for(unsigned k=0;k<2u;k++){
            unsigned id=k?AVP_FX_AMB3:AVP_FX_AMB2;
            avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,1,1);
            avp_jaguar_fullsynth_stop_all();
            avp_jaguar_fullsynth_play(id,0,0);
            avp_jaguar_fullsynth_mix(pcm,1u);
            assert(pcm[0]>0&&pcm[1]>0);
            /* Continuous ambience Off must NOT suppress random hiss. */
            avp_jaguar_fullsynth_set_audio_mix(100u,100u,0u,100u,100u,1,0,1);
            avp_jaguar_fullsynth_mix(pcm,1u);
            assert(pcm[0]>0&&pcm[1]>0);
            /* Random hiss switch is an immediate hard mute. */
            avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,1,0);
            avp_jaguar_fullsynth_mix(pcm,1u);
            assert(pcm[0]==0&&pcm[1]==0);
            /* Zero gain also suppresses the same voice. */
            avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,0u,1,1,1);
            avp_jaguar_fullsynth_mix(pcm,1u);
            assert(pcm[0]==0&&pcm[1]==0);
            /* Midpoint gain and unity; original voice remains active. */
            avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,50u,1,1,1);
            avp_jaguar_fullsynth_mix(pcm,1u);
            assert(pcm[0]>0&&pcm[1]>0);
            avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,1,1);
            avp_jaguar_fullsynth_mix(pcm,1u);
            assert(pcm[0]>0&&pcm[1]>0);
        }
        /* Hiss mute must not affect other environmental effects or the
         * original reserved level ambience. */
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,1,0);
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_play(AVP_FX_AMB2X,0,0);
        avp_jaguar_fullsynth_mix(pcm,1u);
        assert(pcm[0]>0&&pcm[1]>0);
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_start_ambient(1u);
        avp_jaguar_fullsynth_mix(pcm,1u);
        assert(pcm[0]>0&&pcm[1]>0);
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,0,1);
        avp_jaguar_fullsynth_mix(pcm,1u);
        assert(pcm[0]==0&&pcm[1]==0);
        avp_jaguar_fullsynth_set_audio_mix(100u,100u,100u,100u,100u,1,1,1);
        avp_jaguar_fullsynth_stop_all();
        avp_jaguar_fullsynth_shutdown();
    }
    free(rom.data);
    return 0;
}
