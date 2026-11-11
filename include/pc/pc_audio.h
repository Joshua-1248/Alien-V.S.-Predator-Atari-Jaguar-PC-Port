#ifndef AVP_PC_AUDIO_H
#define AVP_PC_AUDIO_H
#include "pc/pc_platform.h"
int avp_pc_audio_init(const AvpRomImage *rom);
void avp_pc_audio_shutdown(void);
void avp_pc_audio_play(unsigned descriptor_address,s32 left,s32 right);
void avp_pc_audio_play_priority(unsigned descriptor_address,s32 priority);
void avp_pc_audio_play_tracker(unsigned descriptor_address,u32 volume,u32 pitch,u32 env_rate,u32 mod_depth);
void avp_pc_audio_update_generator_alarm(s32 volume,s32 pitch);
/* FX_xp1b15 word 3; rate -1 preserves the last descriptor value. */
void avp_pc_audio_play_explosion(s32 rate);
void avp_pc_audio_play_handle(unsigned descriptor_address,u32 handle,unsigned voice,s32 left,s32 right);
void avp_pc_audio_play_handle_d1(unsigned descriptor_address,u32 handle,unsigned voice,s32 left,s32 right,s32 d1);
void avp_pc_audio_stop_handle(u32 handle);
int avp_pc_audio_handle_active(u32 handle);
int avp_pc_audio_voice_active(unsigned voice);
void avp_pc_audio_unloop_handle(u32 handle);
void avp_pc_audio_mod_handle(u32 handle,s32 value);
void avp_pc_audio_position_handle(u32 handle,s32 x,s32 y);
void avp_pc_audio_start_ambient(unsigned level);
void avp_pc_audio_stop_ambient(void);
void avp_pc_audio_start_music(unsigned descriptor_address);
void avp_pc_audio_stop_music(void);
void avp_pc_audio_stop_all(void);
void avp_pc_audio_pause_save(void);
void avp_pc_audio_pause_restore(u32 weapon_handle,u32 alarm_handle);
void avp_pc_audio_set_master_volume(u32 volume);
/* Speaker layout selection uses host matrix routing and device negotiation.
 * Audio engine still generates Jaguar-authentic stereo at 20774 Hz. */
void avp_pc_audio_set_speaker_mode(unsigned mode,unsigned subwoofers);
unsigned avp_pc_audio_active_channels(void);
void avp_pc_audio_set_mix(unsigned music,unsigned sfx,unsigned ambience,
                          unsigned voice,unsigned hissing,
                          int music_on,int ambience_on,int hissing_on);
#endif
