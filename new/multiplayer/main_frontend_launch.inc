#ifdef AVP_LOCAL_MULTIPLAYER_FRONTEND_LAUNCH_BRIDGE
    if(Choice==6){
        /* New host F2 Start Game request. Do not enter the original species
         * selector or interpret 6 as a Jaguar menu option. The PC main loop
         * consumes the requested setup and enters the native MP experiment. */
        WaitFXFa();StopMusi();VSync();InitFile();return;
    }
#endif
