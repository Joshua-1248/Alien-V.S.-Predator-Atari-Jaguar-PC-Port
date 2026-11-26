    /* Local Deathmatch avatars are not AMP objects. Treat them as a
     * second possible projectile contact only AFTER the original wall/AMP
     * handling. Source hit width and tick-by-tick movement are unchanged. */
    if(mp_projectile_hit && (a->flags&(1u<<AMP_PLAYER)) &&
       mp_projectile_hit(mp_projectile_user,a)){
        /* A Smart Disc hitting a player must sound like the same disc
         * hitting an AMP: the original QHIT contact, not the DISK1
         * impact-on-player/enemy-weapon sound. */
        const AvpRuntimeOps *o=avp_runtime_ops();
        if(o->play_sfx)o->play_sfx(o->user,AVP_FX_QHIT);
        disc_kill1(a);return;
    }
