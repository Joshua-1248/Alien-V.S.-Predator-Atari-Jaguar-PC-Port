    /* Optional local Deathmatch player avatars. Original AMP hit-selection
     * and random spread are already complete; leave them untouched. */
    if(player_mp_shot_hook && player_mp_shot_hook(player_mp_shot_user,
           (s32)x_pos,(s32)y_pos,boxx,boxy,cs,sn,fire_width,
           closest,fire_damage))return NULL;

