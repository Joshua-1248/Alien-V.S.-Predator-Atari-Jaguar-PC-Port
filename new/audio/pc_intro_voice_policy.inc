#ifdef AVP_PC_HOST
/* The PC currently hosts the music sequencer separately from general SFX,
 * so Intro's six nominally unprotected effect lanes can overlap one TYP1B
 * each glyph. The correctly sounding Marine escape-report path has only
 * one effective lane. Until shared JERRY music/effect voice ownership can
 * be reconstructed completely, match that user-verified single-lane
 * TYP1B behavior in INTRO *without* changing the original 68000
 * ProtectV(2) CPU state or any other sound effect. */
static int host_intro_typ1b_single_lane;
void avp_music_set_intro_typ1b_single_lane(int enabled)
{
    host_intro_typ1b_single_lane=!!enabled;
}
#endif
