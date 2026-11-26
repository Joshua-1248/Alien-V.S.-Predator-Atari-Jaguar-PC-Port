#ifndef AVP_JAGUAR_HOST_JERRY_WAVE_PROCEDURAL_H
#define AVP_JAGUAR_HOST_JERRY_WAVE_PROCEDURAL_H
#include <stdint.h>
/* Self-contained, deterministic reconstruction of JERRY's eight 128-sample
 * FullSynth waveforms. No Jaguar hardware ROM data is embedded or required.
 * The six mathematical waveform families follow their original shape and
 * amplitude; NOISE and NTRI use independently generated noise with calibrated
 * RMS. This is not claimed to reproduce the hardware ROM's random samples. */
void avp_jerry_generate_waves(int16_t table[8][128]);
#endif
