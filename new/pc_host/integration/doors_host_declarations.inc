/* Read-only PC input routing: whether DoorKeys would select do_lifts. */
int avp_doors_player_facing_lift(void);
/* PC-only transition provenance. Reset before each original player update;
 * zero means a non-elevator/non-airlock exit. Never alters Jaguar behavior. */
enum { AVP_DOORS_EXIT_NONE=0, AVP_DOORS_EXIT_LIFT=1, AVP_DOORS_EXIT_AIRLOCK=2 };
void avp_doors_host_clear_exit_kind(void);
/* PC-local Deathmatch interaction gating; zero means original Jaguar rules.
 * Called only while one native multiplayer actor is bound. */
void avp_doors_mp_restrict_interactions(int deathmatch,int level_travel);
unsigned avp_doors_host_exit_kind(void);
/* Only the bot's native actor scope may enable the stricter interlock. */
void avp_doors_mp_bot_airlock_interlock(int enabled);

