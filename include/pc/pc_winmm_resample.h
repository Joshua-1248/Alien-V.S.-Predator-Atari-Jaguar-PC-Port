#ifndef AVP_PC_WINMM_RESAMPLE_H
#define AVP_PC_WINMM_RESAMPLE_H
#include <stdint.h>

/* Output-rate conversion is host-only. Jaguar FullSynth always generates
 * stereo PCM at 20774 Hz; device clocks must never change game sound pitch. */
enum { AVP_SYNTH_RATE=20774, AVP_WINMM_SOURCE_BLOCK=512 };
typedef void (*AvpPcSourceMix)(int16_t *dst, unsigned frames);
typedef struct AvpPcRateBridge {
    uint64_t phase,step;
    int16_t input[AVP_WINMM_SOURCE_BLOCK*2];
    int16_t previous[2],next[2];
    unsigned read_index;
    int initialized;
} AvpPcRateBridge;
/* Keep an exact fractional device-frame remainder from one original
 * 512-sample FullSynth interval to the next.  A fixed rounded frame count
 * makes output drift relative to the game's original audio clock. */
typedef struct AvpPcDeviceFrameClock {
    unsigned output_rate;
    uint64_t remainder;
} AvpPcDeviceFrameClock;
void avp_pc_device_frame_clock_init(AvpPcDeviceFrameClock *clock,
                                    unsigned output_rate);
unsigned avp_pc_device_frame_clock_next(AvpPcDeviceFrameClock *clock);
void avp_pc_rate_bridge_init(AvpPcRateBridge *bridge,unsigned output_rate);
void avp_pc_rate_bridge_render(AvpPcRateBridge *bridge,int16_t *dst,
                               unsigned frames,AvpPcSourceMix source);
#endif
