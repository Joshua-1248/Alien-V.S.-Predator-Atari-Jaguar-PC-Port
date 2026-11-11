#ifndef AVP_JAGUAR_HOST_FULLSYNTH_HOST_H
#define AVP_JAGUAR_HOST_FULLSYNTH_HOST_H

#include "avp_rom.h"
#include "avp_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Hosted implementation of AVP's Jaguar FullSynth/AVPSOUND-facing semantics.
 * FullSynth supplies authentic stereo PCM; the optional host layout converter
 * may route that PCM to multichannel output devices. */
int avp_jaguar_fullsynth_init(const AvpRomImage *rom);
/* Optional user-owned original Jaguar JERRY wave ROM, 4096 bytes, CRC32
 * 7a25ee5b. Invalid or missing files leave fallback instruments unchanged. */
int avp_jaguar_fullsynth_load_wave_rom(const char *path);
int avp_jaguar_fullsynth_uses_exact_waves(void);
void avp_jaguar_fullsynth_shutdown(void);
void avp_jaguar_fullsynth_mix(int16_t *stereo,unsigned frames);
/* Read the latest mixed stereo sample for the original Predator acoustic HUD. */
void avp_jaguar_fullsynth_meter_samples(s32 *left,s32 *right);
void avp_jaguar_fullsynth_play(unsigned descriptor_address,s32 left,s32 right);
/* Start an effect with an explicit TFX D1 priority word (negative = module default). */
void avp_jaguar_fullsynth_play_priority(unsigned descriptor_address,s32 priority);
void avp_jaguar_fullsynth_play_tracker(unsigned descriptor_address,u32 volume,u32 pitch,u32 env_rate,u32 mod_depth);
void avp_jaguar_fullsynth_update_generator_alarm(s32 volume,s32 pitch);
void avp_jaguar_fullsynth_play_explosion(s32 rate);
/* Host-only player attribution of reserved Jaguar CPU voice 1.
 * OFF/legacy mode always uses owner 0 and original DSP slot 2. */
#ifdef AVP_LOCAL_MULTIPLAYER
void avp_jaguar_fullsynth_mp_set_weapon_owner(unsigned player);
/* Freeze the co-op shared mixer without consuming any PCM/MIDI time. */
void avp_jaguar_fullsynth_mp_set_paused(int paused);
/* PC-only bot positional stereo routing. Human viewpoint snapshots determine
 * the nearest audible listener. NULL restores the single-player mix. */
#include "jaguar/host/mp_spatial_audio.h"
void avp_jaguar_fullsynth_mp_set_listeners(const AvpMpSpatialListener listeners[4],
                                           unsigned bot_mask);
/* Actor sound-source attribution also drives the visual waveform meter. */
void avp_jaguar_fullsynth_mp_set_sound_source(unsigned player,unsigned level,s32 x,s32 y);
void avp_jaguar_fullsynth_mp_clear_sound_source(void);
void avp_jaguar_fullsynth_mp_meter_near(unsigned listener,unsigned level,s32 x,s32 y,
                                      s32 *left,s32 *right);
uint64_t avp_jaguar_fullsynth_mp_weapon_start_count(unsigned player);
#endif
void avp_jaguar_fullsynth_play_handle(unsigned descriptor_address,u32 handle,unsigned voice,s32 left,s32 right);
void avp_jaguar_fullsynth_play_handle_d1(unsigned descriptor_address,u32 handle,unsigned voice,s32 left,s32 right,s32 d1);
void avp_jaguar_fullsynth_stop_handle(u32 handle);
int avp_jaguar_fullsynth_handle_active(u32 handle);
int avp_jaguar_fullsynth_voice_active(unsigned voice);
void avp_jaguar_fullsynth_unloop_handle(u32 handle);
void avp_jaguar_fullsynth_mod_handle(u32 handle,s32 value);
void avp_jaguar_fullsynth_position_handle(u32 handle,s32 x,s32 y);
void avp_jaguar_fullsynth_start_ambient(unsigned level);
void avp_jaguar_fullsynth_stop_ambient(void);
void avp_jaguar_fullsynth_start_music(unsigned descriptor_address);
void avp_jaguar_fullsynth_stop_music(void);
void avp_jaguar_fullsynth_stop_all(void);
void avp_jaguar_fullsynth_pause_save(void);
void avp_jaguar_fullsynth_pause_restore(u32 weapon_handle,u32 alarm_handle);
void avp_jaguar_fullsynth_set_master_volume(u32 volume);
/* PC-only independent output mix settings, percent 0..100. */
void avp_jaguar_fullsynth_set_audio_mix(unsigned music,unsigned sfx,
    unsigned ambience,unsigned voice,unsigned hissing,
    int music_on,int ambience_on,int hissing_on);

#ifdef __cplusplus
}
#endif

#endif
