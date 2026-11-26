#ifdef AVP_PC_HOST
    /* The intro matches the one-voice escape text reference for TYP1B
     * only. Normal voice counts and retail priority/effect identity remain
     * unchanged; an occupied equal-priority lane rejects the request. */
    if(host_intro_typ1b_single_lane && id==AVP_FX_TYP1B)
        search_start=AVP_SOUND_VOICES-1u;
#endif
