/* R272: MUSIC.o TFX/DoEffect store path ($9CE8-$9D4E) writes the voice
 * volume word.  Type-4 effects keep D1 LSR.W #2, and an explicit caller D1
 * (MJP Do_Escape passes $2600) replaces a sampler's module volume word.
 * Synthetic descriptors only; no retail ROM or audio data. */
#define _DEFAULT_SOURCE 1
#include "../src/jaguar/host/fullsynth_host.c"
#include <assert.h>

static u8 rdb[16384],dsp_text[0x4000];
u32 x_pos,y_pos,centre_angle;
static void w32(u8*p,u32 v){p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;}
int avp_resources_decode_rdb(const AvpRomImage*rom,u8**data,size_t*n)
{(void)rom;*data=(u8*)malloc(sizeof(rdb));if(!*data)return 0;memcpy(*data,rdb,sizeof(rdb));*n=sizeof(rdb);return 1;}
int avp_resources_decode_rdb_text(const AvpRomImage*rom,u8**data,size_t*n)
{(void)rom;*data=(u8*)malloc(sizeof(dsp_text));if(!*data)return 0;memcpy(*data,dsp_text,sizeof(dsp_text));*n=sizeof(dsp_text);return 1;}
int avp_resources_rom_descriptor(const AvpRomImage*rom,u32 addr,const u8**data,size_t*n)
{(void)rom;(void)addr;(void)data;(void)n;return 0;}

static const AvpHostedVoice *find_voice(unsigned id)
{
    for(unsigned i=0;i<16u;i++)if(F.v[i].synth.active&&F.v[i].sound_id==id)return &F.v[i];
    return NULL;
}

int main(void)
{
    AvpRomImage rom;const AvpHostedVoice *hv;
    memset(&rom,0,sizeof(rom));
    rom.size=4u*1024u*1024u;rom.data=(u8*)calloc(1,rom.size);assert(rom.data);
    memset(rom.data+0x100000u,60,128);
    /* Sampler (type 32) at $20500: volume word $2A00 like retail FX_b1..b5. */
    w32(rdb+0x200u,32u);w32(rdb+0x204u,0x2a00u);w32(rdb+0x208u,0x800800u);
    w32(rdb+0x20cu,256u);w32(rdb+0x210u,0x7fffu);w32(rdb+0x214u,0x7000u);
    w32(rom.data+0x800u,0x900000u);
    /* FM (type 4) at $20600: volume/priority word at +$18 = $4000 with the
     * loop flag set, like retail FX_newal ($80004000). */
    w32(rdb+0x300u,4u);w32(rdb+0x304u,0x00010000u);w32(rdb+0x308u,0x00010000u);
    w32(rdb+0x310u,0x00100000u);w32(rdb+0x318u,0x80004000u);
    assert(avp_jaguar_fullsynth_init(&rom));

    /* Ordinary SFX (D1=-1): type 4 starts at the quartered word, loop kept. */
    avp_jaguar_fullsynth_play(0x20600u,0,0);
    hv=find_voice(0x20600u);assert(hv);
    assert(hv->synth.p[6]==0x80001000u);
    assert(hv->synth.orig_volume==0x1000u);

    /* Ordinary sampler: volume word unchanged ($2A00). */
    avp_jaguar_fullsynth_stop_all();
    avp_jaguar_fullsynth_play(0x20500u,0,0);
    hv=find_voice(0x20500u);assert(hv);
    assert((hv->synth.p[1]&0xffffu)==0x2a00u);

    /* MJP Do_Escape: explicit D1=$2600 replaces the sampler volume word. */
    avp_jaguar_fullsynth_stop_all();
    avp_jaguar_fullsynth_play_priority(0x20500u,0x2600);
    hv=find_voice(0x20500u);assert(hv);
    assert((hv->synth.p[1]&0xffffu)==0x2600u);
    assert(hv->synth.orig_volume==0x2600u);
    /* ...and a negative D1 word still selects the module default. */
    avp_jaguar_fullsynth_stop_all();
    avp_jaguar_fullsynth_play_priority(0x20500u,-1);
    hv=find_voice(0x20500u);assert(hv&&(hv->synth.p[1]&0xffffu)==0x2a00u);

    /* HUD.S writes FX_trac1+24 then `SFX trac1`: TFX quarters that word. */
    avp_jaguar_fullsynth_stop_all();
    avp_jaguar_fullsynth_play_tracker(0x20600u,0x0a00u,0x000a7000u,0x2fu,0x100u);
    hv=find_voice(0x20600u);assert(hv);
    assert((hv->synth.p[6]&0xffffu)==0x0280u);

    avp_jaguar_fullsynth_shutdown();free(rom.data);
    puts("R272 TFX volume-word store (type-4 LSR #2, explicit D1) PASS");
    return 0;
}
