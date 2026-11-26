    /* Host analog movement targets the character's existing native speed
     * limit, not a fraction of the weaker retail button acceleration. This
     * avoids the half-strength strafe and removes double acceleration when
     * the same stick is also bound as a digital direction. No analog input
     * means the original integer velocity code below runs untouched. */
    const double af=host_analog_motion && !lift_key &&
        isfinite(host_analog_forward) ? host_analog_forward : 0.0;
    const double as=host_analog_motion && isfinite(host_analog_strafe) ?
        host_analog_strafe : 0.0;
    if(af!=0.0 || as!=0.0){
        avp_pc_analog_velocity(&x_vel,&y_vel,cos_ang,sin_ang,af,as,
            digital_forward_ax,digital_forward_ay,
            ax-digital_forward_ax,ay-digital_forward_ay,
            avp_pc_move_max(max_speed,host_double_movement,host_movement_dm_fast),dec_shift);
    }else
