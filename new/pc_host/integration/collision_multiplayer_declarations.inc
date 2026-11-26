/* PC local multiplayer only: if a moving AMP has already entered the player's
 * original collision square, permit motion which reduces that existing overlap.
 * Normal single-player and all initial non-overlapping contacts still use
 * AMPCollisions unchanged. This helper is never consulted by the Jaguar path. */
int avp_collision_mp_amp_escape(s32 from_x,s32 from_y,s32 to_x,s32 to_y);               /* nonzero = safe overlap escape */
