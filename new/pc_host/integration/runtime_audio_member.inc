    /* PC optional atomic start with original caller TFX D1. The 68000 CPU
     * allocator remains the only owner of voice selection / handle. */
    void (*play_sfx_handle_d1)(void *user,unsigned id,u32 handle,unsigned voice,s32 d1);
