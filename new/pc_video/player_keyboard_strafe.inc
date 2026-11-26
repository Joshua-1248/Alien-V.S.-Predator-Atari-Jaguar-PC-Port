    /* The PC keyboard can request a sidestep and a turn on the same tick.
     * Apply this extra sidestep with the identical original fixed-point
     * acceleration, even while the Jaguar turn modifier is asserted. */
    if(host_keyboard_strafe_during_turn){
        ax+=sa*host_keyboard_strafe_during_turn;
        ay+=ca*host_keyboard_strafe_during_turn;
    }
