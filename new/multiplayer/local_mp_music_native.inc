#ifdef AVP_LOCAL_MULTIPLAYER_AUDIO_BRIDGE
/* Swap only the original reserved WEAPON voice.  Original 8-voice generic
 * SFX allocator and ident counter continue to be global and shared. */
static int mp_weapon_voice_busy;
static void mp_weapon_read(AvpMpWeaponAudioState *out)
{
    out->type=voice[1].type;out->priority=voice[1].priority;
    out->looping=voice[1].looping;out->reserved=voice[1].reserved;
    out->handle=voice[1].handle;out->sound_id=voice[1].sound_id;
    out->mod_value=voice[1].mod_value;
}
static void mp_weapon_write(const AvpMpWeaponAudioState *in)
{
    voice[1].type=in->type;voice[1].priority=in->priority;
    voice[1].looping=in->looping;voice[1].reserved=in->reserved;
    voice[1].handle=in->handle;voice[1].sound_id=in->sound_id;
    voice[1].mod_value=in->mod_value;
}
int avp_music_mp_weapon_capture(AvpMpWeaponAudioState *out)
{
    if(!out||mp_weapon_voice_busy)return 0;
    mp_weapon_read(out);return 1;
}
int avp_music_mp_weapon_enter(AvpMpWeaponAudioState *old_host,
                              const AvpMpWeaponAudioState *player)
{
    if(!old_host||!player||mp_weapon_voice_busy)return 0;
    mp_weapon_read(old_host);mp_weapon_write(player);
    mp_weapon_voice_busy=1;return 1;
}
int avp_music_mp_weapon_leave(const AvpMpWeaponAudioState *old_host,
                              AvpMpWeaponAudioState *player)
{
    if(!old_host||!player||!mp_weapon_voice_busy)return 0;
    mp_weapon_read(player);mp_weapon_write(old_host);
    mp_weapon_voice_busy=0;return 1;
}
#endif
