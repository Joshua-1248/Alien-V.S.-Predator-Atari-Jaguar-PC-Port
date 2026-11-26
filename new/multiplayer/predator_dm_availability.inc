    /* Deathmatch combat weapons are intentionally independent of honour.
     * Do not auto-select or revoke a gun during a native weapon tick. */
    if(mp_predator_dm){
        cur_weps=(u8)(cur_weps|0x1eu);
        return;
    }
