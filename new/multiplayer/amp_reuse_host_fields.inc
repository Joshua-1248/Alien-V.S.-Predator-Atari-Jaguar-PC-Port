    mark_amp_slot(index);
    /* host_static and aux_ptr are host-only fields absent from the original
     * 48-byte Jaguar AMP. A reused slot must not inherit a static/dead actor
     * state: UpdateAMPs deliberately skips host_static even when mode is live.
     * This left newly spawned player projectiles suspended indefinitely. */
    a->host_static=0;
    a->aux_ptr=NULL;
