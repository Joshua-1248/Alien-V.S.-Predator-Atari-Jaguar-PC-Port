    /* In co-op an authored Security Keycard must remain in its original
     * AMP/corpse and objmap for EVERY other participant. Track a card once
     * per player/life; don't repeatedly award it while standing on the body.
     * The original single-player removal path below is unchanged. */
    if(mp_keycard_owner && type>=COLL_S1 && type<=COLL_S10){
        const u16 bit=(u16)(1u<<((unsigned)type-COLL_S1));
        if(!(*mp_keycard_owner & bit) && CollectIt(type,amount))
            *mp_keycard_owner=(u16)(*mp_keycard_owner | bit);
        return;
    }
