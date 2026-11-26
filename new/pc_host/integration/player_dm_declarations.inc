/* Optional PC deathmatch shot test. Invoked only after the original
 * PLAYER.S weapon trace and AMP closest-hit comparison; never installed in
 * single-player or co-op. Returns 1 when a closer player took the shot. */
typedef int (*AvpPlayerMpShotHook)(void *user,s32 origin_x,s32 origin_y,
        s32 boxx,s32 boxy,s16 cs,s16 sn,u16 width,u32 closest,s16 damage);
void avp_player_set_mp_shot_hook(AvpPlayerMpShotHook callback,void *user);
