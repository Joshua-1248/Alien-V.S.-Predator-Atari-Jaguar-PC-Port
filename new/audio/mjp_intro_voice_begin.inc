#ifdef AVP_PC_HOST
    /* Keep the original MJP text SFX commands unchanged; restrict only
     * hosted overlap of TYP1B for Intro, matching the one-lane escape
     * report's user-confirmed auditory cadence. MTEST owns cleanup for
     * both ordinary scene completion and an A/B/C interruption. */
    if(m==AVP_MJP_INTRO)avp_music_set_intro_typ1b_single_lane(1);
#endif
