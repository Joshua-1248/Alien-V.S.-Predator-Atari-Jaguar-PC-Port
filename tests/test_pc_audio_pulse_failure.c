/* A live PulseAudio server can disappear after pa_simple_new succeeds.
 * Verify that a failed write is closed and that FullSynth time does not run
 * 2.5x faster through the old fixed 10-ms error retry. No audio device,
 * Jaguar ROM bytes, or runtime instrumentation is used. */
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

static unsigned writes,frees;
static int fail_write,stop_after_reconnect;
static unsigned opens;
static pa_simple *fake_pulse_new(const char *server,const char *app,int dir,const char *device,
                                 const char *name,const AvpPaSampleSpec *spec,const void *map,
                                 const void *attrs,int *error)
{
    (void)server;(void)app;(void)dir;(void)device;(void)name;
    (void)map;(void)attrs;(void)error;
    assert(spec&&spec->rate==20774u&&spec->channels==2u);
    opens++;
    return (pa_simple *)(uintptr_t)1u;
}
static int fake_pulse_write(pa_simple *p,const void *buf,size_t bytes,int *error)
{
    (void)error;
    assert((uintptr_t)p==1u);
    assert(buf&&bytes==512u*2u*sizeof(int16_t));
    writes++;
    if(stop_after_reconnect&&!fail_write)D.run=0;
    return fail_write?-1:0;
}
static void fake_pulse_free(pa_simple *p)
{
    assert((uintptr_t)p==1u);
    frees++;
}
static uint64_t monotonic_ns(void)
{
    struct timespec ts;
    assert(clock_gettime(CLOCK_MONOTONIC,&ts)==0);
    return (uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec;
}
int main(void)
{
    int16_t buf[512u*2u]={0};
    uint64_t t0,elapsed;
    D.pulse=(pa_simple *)(uintptr_t)1u;
    D.pulse_write=fake_pulse_write;
    D.pulse_free=fake_pulse_free;
    D.run=1;
    write_pulse_block(buf,512u);
    assert(writes==1u&&frees==0u&&D.pulse);
    /* An immediate device failure must not burn through another synthesized
     * 512-frame block after only 10ms; it represents 24.65ms at 20774Hz. */
    fail_write=1;
    t0=monotonic_ns();
    write_pulse_block(buf,512u);
    elapsed=monotonic_ns()-t0;
    assert(elapsed>=20000000ull);
    assert(writes==2u&&frees==1u&&D.pulse==NULL);
    /* The next worker block must be able to reconnect the lost output
     * instead of remaining silent for the rest of the session. */
    D.pulse_so=(void *)(uintptr_t)1u; /* already resolved fake library */
    D.pulse_new=fake_pulse_new;
    fail_write=0;
    stop_after_reconnect=1;
    D.run=1;
    (void)audio_thread(NULL);
    assert(opens==1u&&writes==3u&&D.pulse!=NULL);
    D.pulse=NULL;D.pulse_so=NULL;
    D.run=0;
    return 0;
}
