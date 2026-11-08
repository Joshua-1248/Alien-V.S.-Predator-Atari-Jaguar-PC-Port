/* Mock ALSA PCM transport: confirm an underrun does not discard the already
 * rendered tail of a FullSynth buffer. No device, diagnostics, or ROM assets. */
#include "../src/platform/pc/pc_audio.c"
#include "avp_resources.h"
#include <assert.h>
#include <stdint.h>

u32 x_pos,y_pos,centre_angle;
int avp_resources_decode_rdb(const AvpRomImage *rom,u8 **data,size_t *n)
{(void)rom;(void)data;(void)n;return 0;}
int avp_resources_decode_rdb_text(const AvpRomImage *rom,u8 **data,size_t *n)
{(void)rom;(void)data;(void)n;return 0;}
int avp_resources_rom_descriptor(const AvpRomImage *rom,u32 addr,const u8 **data,size_t *n)
{(void)rom;(void)addr;(void)data;(void)n;return 0;}

static const int16_t *first_frame;
static unsigned calls,prepares,accepted;
static snd_pcm_sframes_t fake_write(snd_pcm_t *pcm,const void *data,snd_pcm_uframes_t frames)
{
    const int16_t *p=(const int16_t *)data;
    (void)pcm;
    if(calls==0u){first_frame=p;assert(frames==512u);calls++;accepted+=128u;return 128;}
    if(calls==1u){assert(p==first_frame+256u&&frames==384u);calls++;return -32;}
    if(calls==2u){assert(prepares==1u);assert(p==first_frame+256u&&frames==384u);
                   calls++;accepted+=192u;return 192;}
    assert(calls==3u);
    assert(p==first_frame+640u&&frames==192u);
    calls++;accepted+=192u;
    D.run=0;
    return 192;
}
static int fake_prepare(snd_pcm_t *pcm){(void)pcm;prepares++;return 0;}
static snd_pcm_sframes_t failing_write(snd_pcm_t *pcm,const void *p,snd_pcm_uframes_t frames)
{(void)pcm;(void)p;(void)frames;calls++;return -32;}
int main(void)
{
    int16_t data[512u*2u]={0};
    D.pcm=(snd_pcm_t *)(uintptr_t)1u;
    D.writei=fake_write;
    D.prepare=fake_prepare;
    D.run=1;
    /* Exercises the regular worker's PCM-write branch, not a test-only
       version of the writer. The mock stops it after one full buffer. */
    (void)audio_thread(NULL);
    assert(calls==4u&&prepares==1u&&accepted==512u);
    /* If hardware is permanently broken, recoveries must be bounded and
       synthesis must not be stuck spinning inside the same PCM call. */
    calls=prepares=0;
    D.writei=failing_write;
    D.run=1;
    write_alsa_block(data,512u);
    assert(calls==4u&&prepares==4u);
    D.run=0;
    D.pcm=NULL;
    return 0;
}
