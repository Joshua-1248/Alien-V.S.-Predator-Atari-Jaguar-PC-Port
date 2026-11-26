    /* Deathmatch's optional random corpse loot NEVER replaces authored
     * security cards, which are needed to traverse locked retail maps.
     * With callback NULL, the original CollectIt() branch is untouched. */
    if(body && mp_body_loot && !(type>=COLL_S1 && type<=COLL_S10)){
        mp_body_loot(mp_body_user,(unsigned)cur_level,(unsigned)(no-1u));
        return;
    }
