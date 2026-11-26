/* Debug-only, invalidates ONLY the current level save/visited flag.
 * The next restore_level executes the original first-visit population. */
void avp_amp_dev_freshen_current_level(void);
/* Multiplayer-only campaign objective controls. Default state preserves the
 * exact retail single-player AMP modes and restore_level decisions. */
void avp_amp_mp_defer_end_queen(int enabled);
void avp_amp_mp_set_campaign_type(s16 type);
int avp_amp_mp_queen_predator_ending(s16 acting_player_type);

