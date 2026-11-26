#ifdef AVP_PC_HOST
    for(unsigned i=0u;i<AVP_PC_TITLE_SEQUENCE_FIELDS;i++){
        /* Retail $01FC08 and $01FC98 call Pause() with 30 and 260 ticks.
         * A/B/C input can interrupt EACH wait; it does not interrupt the
         * GPU logo animations. Forward only a presentation clock jump, not
         * thousands of game/sound ticks or a synthetic menu Fire press. */
        if(i>=AVP_PC_TITLE_WAIT30_START && i<AVP_PC_TITLE_WAIT_END){
            readpad();
            if((~joy_cur&0x22002000u)!=0u){
                i=i<AVP_PC_TITLE_WAIT260_START?
                    AVP_PC_TITLE_WAIT260_START:AVP_PC_TITLE_WAIT_END;
                fevent(AVP_PC_TITLE_SKIP_EVENT,(s32)i,0);
            }
        }
        VSync();fevent(0x5449434bu,0,0);
    }
#else
    for(unsigned i=0;i<141u;i++){VSync();fevent(0x5449434bu,0,0);}
#endif

