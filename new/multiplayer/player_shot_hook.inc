static AvpPlayerMpShotHook player_mp_shot_hook;
static void *player_mp_shot_user;
void avp_player_set_mp_shot_hook(AvpPlayerMpShotHook callback,void *user)
{player_mp_shot_hook=callback;player_mp_shot_user=callback?user:NULL;}
