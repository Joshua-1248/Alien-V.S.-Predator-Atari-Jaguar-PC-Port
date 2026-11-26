    /* Quick Turn is a PC-only animated view action. Only its active frames
     * override the host heading: no Jaguar controls, acceleration, camera
     * tilt, or other source-owned movement branches change when inactive. */
    if(host_quick_turn_enabled&&!hugkill){
        enhanced_angle=host_quick_turn_heading;
        enhanced_angle_active=1;
        bypass_jaguar_angle_snap=1;
    }
    /* A completed quick turn must not snap back towards the Jaguar cardinal
     * grid on the next NEUTRAL tick. Manual turning immediately restores the
     * original input branch; no Quick Turn used means no behavior change. */
    if(host_quick_turn_preserve_heading && !host_quick_turn_enabled){
        if(req_turn || (host_analog_aim && host_aim_turn!=0.0f))
            host_quick_turn_preserve_heading=0;
        else bypass_jaguar_angle_snap=1;
    }
