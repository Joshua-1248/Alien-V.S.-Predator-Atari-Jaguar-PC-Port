/* PC Enhanced Analog Controls only: host-poll-rate heading at game tick. */
void avp_player_host_live_heading(int enabled, u32 heading);
/* Host-only PC Quick Turn animation; leaves Jaguar input mapping untouched. */
void avp_player_host_quick_turn_heading(int enabled, u32 heading);
void avp_player_host_quick_turn_reset(void);
