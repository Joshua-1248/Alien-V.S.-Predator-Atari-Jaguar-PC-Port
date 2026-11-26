#ifdef AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE
        /* The original Jaguar menu counts 800 idle ticks before entering the
         * attract sequence. F2 is a PC-only overlay: leaving that counter
         * running behind it used to dismiss the setup menu unexpectedly.
         * Reset (not merely freeze) the idle counter while F2 is visible so
         * closing F2 starts a fresh idle interval. Without a PC callback the
         * retail counter remains exactly as before. */
        {const AvpRuntimeOps *o=avp_runtime_ops();
         if(o->frontend_mp_menu_open && o->frontend_mp_menu_open(o->user))
             title_pad_wait=0;
         else ++title_pad_wait;}
#else
        ++title_pad_wait;
#endif
#ifdef AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE
        {const AvpRuntimeOps *o=avp_runtime_ops();
         if(!title_save_menu && o->frontend_mp_launch_pending &&
            o->frontend_mp_launch_pending(o->user)){
             Choice=6;return; /* PC-only F2 Start Game sentinel */
         }}
#endif
