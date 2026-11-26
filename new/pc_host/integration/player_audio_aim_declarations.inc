/* Host-only audio preference: does not change the Jaguar pain state or RNG. */
void avp_player_set_pain_sounds(int enabled);
/* Per-bound-player co-op visual cue, not a gameplay state or SFX replacement. */
void avp_player_clear_last_pain_sfx(void);
unsigned avp_player_last_pain_sfx(void);
void avp_player_set_facehug_pain_sounds(int enabled);
/* Host Enhancements: melee-only or every player attack ignores weapon sway
 * when calculating aim. Both off gives exact original PLAYER.S behavior. */
void avp_player_set_center_aim_options(int melee,int all_attacks);
int avp_player_center_aim_applies(s16 species,s16 weapon);
