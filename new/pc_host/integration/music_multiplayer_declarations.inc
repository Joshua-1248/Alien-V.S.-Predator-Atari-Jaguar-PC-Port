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

