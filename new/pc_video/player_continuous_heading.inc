    /* Enhanced PC stick heading accumulates continuously in double precision,
     * rather than quantizing each tick to s16 angular velocity.  Keep the
     * 68000 signed velocity for the original weapon swing calculation. */
    if(host_analog_aim && host_enhanced_analog_aim && !host_mouse_aim && !hugkill &&
       !(turn_mode&&(pressed(keys,JOY_RIGHT)||pressed(keys,JOY_LEFT)))){
        enhanced_angle=host_live_heading_enabled ? host_live_heading_value :
            avp_pc_continuous_heading_tick(&host_continuous_heading,
                           centre_angle,host_aim_turn,host_aim_sensitivity,
                           host_aim_speed,max_avel);
        enhanced_angle_active=1;
        bypass_jaguar_angle_snap=1;
    }
