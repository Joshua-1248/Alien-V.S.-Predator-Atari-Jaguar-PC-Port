#ifndef AVP_MUSIC_H
#define AVP_MUSIC_H
#include "avp_types.h"
#include <stddef.h>

/* Readable 68000-side AVPSOUND control state.  The DSP mixer/synth itself is
 * intentionally outside this module; the retail CPU owned voice reservation,
 * handle allocation, loop release and effect-priority decisions. */
typedef struct AvpSoundMeta {
    s16 priority;     /* low-word priority compared by DoEffect */
    u8  synth_type;   /* retail voice type; type 4 has a different control slot */
    u8  looping;      /* descriptor requests a looping voice */
} AvpSoundMeta;

typedef struct AvpTransferFile {
    const void *data;
    size_t size;
} AvpTransferFile;

void avp_music_bind_sfx_meta(const AvpSoundMeta *meta,unsigned count);
/* Bind the retail RDB.DTA module controls for AVPSOUND priority arbitration.
 * Synthetic callers using avp_music_bind_sfx_meta keep their explicit table. */
void avp_music_bind_retail_sfx_descriptors(const u8 *rdb,size_t size);
void avp_music_bind_transfer_arena(void *arena,size_t size);
const void *TransnFi(const AvpTransferFile *file);

void KillSynt(void);
void InitSynt(void);
void StopMusi(void);
void InitTnF(void);
/* Retail RDB music descriptors used directly by MAIN.S.  These are addresses
 * of descriptor records reconstructed from the user's AVP.jag/RDB data, not
 * embedded music payloads. */
#define AVP_MUSIC_PREDATOR_ENDING 0x00020610u
#define AVP_MUSIC_TITLE3          0x00020918u
/* Retail RDB.DTA FullSynth record: character-selection/heartbeat score. */
#define AVP_MUSIC_CHARACTER_SELECT 0x0002046Cu

void StartMus(unsigned id);
void ProtectV(unsigned count);
#ifdef AVP_PC_HOST
/* Host-only attract TYP1B voice-contention bridge. Original CPU protection
 * count is unchanged; only the sound's effective hosted lane is restricted. */
void avp_music_set_intro_typ1b_single_lane(int enabled);
#endif
void FXFadeDo(s32 step);
void FXFadeUp(s32 step);
void WaitFXFa(void);
void KillAlar(void);
void StartAla(unsigned id);
void KillAmbi(void);
void Ambient(void);
u32 PlacedEf(unsigned id,s32 x,s32 y);
u32 DoEffect(unsigned id);
u32 TFX(unsigned id);
/* Preserve original TFX/D1 explicit volume/priority word (>=0) when supplied
 * by source MJP $01DC28...; negative selects the module's own control. */
u32 avp_music_tfx_d1(unsigned id,s32 d1);
/* Retail PlacedEf ($9C3A): effect into reserved voice v (wep_sfx=1, pause_sfx=0). */
u32 avp_music_placed_voice(unsigned id,unsigned v);
void KillAll(void);
void KillSfx(u32 handle);
void UnLoopAl(void);
void StopLoop(u32 handle);
void ModSfx(u32 handle,s32 value);
void avp_pause_audio_save(void);
void avp_pause_audio_restore(void);

/* Experimental local co-op: CPU reserved weapon voice 1 is actor-owned.
 * It is the ONLY 68000 AVPSOUND voice exchanged here; general effects,
 * music, ambience, alarm and global identifier allocation remain shared.
 * Single-player never calls these routines. */
typedef struct AvpMpWeaponAudioState {
    s32 type;
    s16 priority;
    u8 looping,reserved;
    u32 handle;
    unsigned sound_id;
    s32 mod_value;
} AvpMpWeaponAudioState;
#ifdef AVP_LOCAL_MULTIPLAYER_AUDIO_BRIDGE
int avp_music_mp_weapon_capture(AvpMpWeaponAudioState *out);
int avp_music_mp_weapon_enter(AvpMpWeaponAudioState *old_host,
                              const AvpMpWeaponAudioState *player);
int avp_music_mp_weapon_leave(const AvpMpWeaponAudioState *old_host,
                              AvpMpWeaponAudioState *player);
#endif

extern s16 fx_fade_level,fx_fade_target;
extern u32 current_music,alarm_id,ambient_id;
#endif
