    /* R269 OPTIONAL PC enhancement: a fatal hit interrupts the pending
     * pulse-rifle burst BEFORE either recoil or its authored damage frame.
     * Enter the existing human death routine; never synthesize a new death.
     * Other wounds still use the original recoil path and the switch has no
     * effect on Jaguar/default behavior or the Marine player campaign. */
    if(host_marine_fire_interrupt && a->creature==AC_HUMAN &&
       (host_marine_multiplayer_target ||
        player_type==PT_ALIEN || player_type==PT_PREDATOR) && a->energy<=0){
        a->energy=0;amp_begin_death(a);return;
    }
