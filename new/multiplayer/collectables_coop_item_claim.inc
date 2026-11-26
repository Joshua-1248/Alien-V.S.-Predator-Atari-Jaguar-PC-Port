    /* Local co-op shares the physical object, but its benefit is claimed
     * separately by each living player. This is also true for weapons,
     * ammunition, food, medkits and the motion tracker. A full ammo/health
     * pool must NOT consume this player's claim: CollectIt() reports 0.
     * Original AMP mode, collect flag and object-grid are not modified.
     * The single-player path below is byte-for-byte unchanged. */
    if(mp_item_claim){
        if(!mp_item_claim(mp_item_user,(unsigned)cur_level,
                          (unsigned)(no-1u),type,0))return;
        if(!CollectIt(type,amount))return;
        (void)mp_item_claim(mp_item_user,(unsigned)cur_level,
                            (unsigned)(no-1u),type,1);
        return;
    }
