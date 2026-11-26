    /* The original HUD runs once per simulated frame. In local co-op
     * multiple Marines call HUD_human() while the same world is scoped.
     * Advance the shared countdown/flash only once, then publish the SAME
     * visual colour to every actor's independently snapshotted xtra_c. */
    if(countdown_coop_mode && countdown_last_frame==frames_now){
        xtra_c=countdown_flash_color;
        return;
    }
    if(countdown_coop_mode)countdown_last_frame=frames_now;
