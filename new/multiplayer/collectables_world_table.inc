    /* Multiplayer binds original worlds serially, changing cur_level without
     * necessarily re-running InitPlayer/ResetClct. The authored corpse-loot
     * table must belong to this world, not the last world that was entered.
     * ResetClct only selects a static table; it does not reset picked items. */
    ResetClct();
