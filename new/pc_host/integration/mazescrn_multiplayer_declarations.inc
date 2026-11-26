/* PC native multiplayer only; reset after each scoped player weapon tick. */
void avp_mazescrn_mp_fast_equip(int enabled);
/* Death-only, within the correct multiplayer player/weapon audio scope. */
void avp_mazescrn_mp_stop_weapon_audio(void);
/* Original contact SFX shared between native AMP hits and PC Deathmatch
 * player hits; returns 0 for misses, ranged weapons or non-melee attacks.
 * Calling it does not emit sound: the confirmed-hit caller owns TFX(). */
unsigned avp_mazescrn_melee_contact_sfx(s16 species,s16 weapon);
