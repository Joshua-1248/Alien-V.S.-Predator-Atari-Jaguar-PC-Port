/* Synthetic cartridge sound fixture: an absent host audio device must not
 * stop Jaguar FullSynth time, or leave a completed one-shot effect allocated.
 * No retail ROM or proprietary waveform data is embedded in this test. */
#include "pc/pc_audio.h"
#include "avp_resources.h"
#include "jaguar/host/fullsynth_host.h"
#include "maze.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static u8 descriptor[0x400];
u32 x_pos,y_pos,centre_angle;
static void write32(u8 *p,u32 n)
{ p[0]=(u8)(n>>24);p[1]=(u8)(n>>16);p[2]=(u8)(n>>8);p[3]=(u8)n; }
int avp_resources_decode_rdb(const AvpRomImage *rom,u8 **data,size_t *size)
{
    (void)rom;
    *data=(u8*)malloc(sizeof(descriptor));
    if(!*data)return 0;
    memcpy(*data,descriptor,sizeof(descriptor));
    *size=sizeof(descriptor);
    return 1;
}
int avp_resources_decode_rdb_text(const AvpRomImage *rom,u8 **data,size_t *size)
{ (void)rom;(void)data;(void)size;return 0; }
int avp_resources_rom_descriptor(const AvpRomImage *rom,u32 addr,const u8 **data,size_t *size)
{ (void)rom;(void)addr;(void)data;(void)size;return 0; }
int main(void)
{
    AvpRomImage rom;
    const u32 handle=0x5150u;
    struct timespec tick={0,10000000L};
    unsigned i;
    memset(&rom,0,sizeof(rom));
    rom.size=2u*1024u*1024u;
    rom.data=(u8*)calloc(1,rom.size);
    assert(rom.data);
    /* Thirty-two positive encoded samples, followed by the module end.
     * The sample pointer is a retail-style first-level cartridge cell. */
    for(i=0;i<32u;i++)rom.data[0x100000u+i]=60u;
    write32(rom.data+0x800u,0x900000u);
    write32(descriptor+0x200u,32u);
    write32(descriptor+0x204u,0x7fffu);
    write32(descriptor+0x208u,0x800800u);
    write32(descriptor+0x20cu,0x100u);
    write32(descriptor+0x210u,0x1f00u);
    write32(descriptor+0x24cu,0x4000u);
    /* Make both backend connection attempts fail even on a desktop with
     * available speakers; the expected return is a running silent mixer. */
    assert(setenv("ALSA_CONFIG_PATH","/dev/null",1)==0);
    assert(setenv("PULSE_SERVER","unix:/tmp/avp-nonexistent-pulse-socket",1)==0);
    assert(avp_pc_audio_init(&rom));
    avp_pc_audio_play_handle(0x20500u,handle,3u,0,0);
    assert(avp_pc_audio_handle_active(handle));
    for(i=0;i<30u&&avp_pc_audio_handle_active(handle);i++)
        (void)nanosleep(&tick,NULL);
    assert(!avp_pc_audio_handle_active(handle));
    avp_pc_audio_shutdown();
    free(rom.data);
    return 0;
}
