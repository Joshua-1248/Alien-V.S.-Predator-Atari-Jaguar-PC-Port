#ifdef AVP_LOCAL_MULTIPLAYER_PLAYER_BRIDGE
/* Called only with BOTH the original actor scope and shared-level scope bound.
 * The session has already prepared joy_cur/joy_edge for the selected player:
 * calling readpad() here would overwrite their keys with P1's host device.
 * In the Jaguar source MoveDoors is time-based, unlike DoorKeys which responds
 * to this player's use action; advance it only ONCE after all level members.
 * Per-actor BGSounds keeps the original RNG state belonging to the player. */
void avp_mp_player_native_step_actor(void)
{
    update_player_actor_body();
    BGSounds();
}
void avp_mp_player_native_step_world_doors(void)
{
    MoveDoors();
}
#endif
