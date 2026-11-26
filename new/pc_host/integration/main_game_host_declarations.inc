/* Host multiplayer outcome: reuse original ending without Fame/EEPROM insertion. */
void avp_post_game_frontend_multiplayer(void);
/* PC cold-boot music startup runs while the first title screen is black.
 * Original MAIN.S still starts music normally on every later NewTitle. */
#if defined(AVP_PC_HOST) || defined(AVP_PC_REFERENCE_TITLE_TIMING)
void avp_pc_title_music_prestart(void);
#endif
