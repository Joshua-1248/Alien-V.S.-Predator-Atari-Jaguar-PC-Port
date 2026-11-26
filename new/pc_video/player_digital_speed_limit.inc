            if(host_digital_turn_speed!=1.0f){
                lim=(s32)((float)max_avel*host_digital_turn_speed+0.5f);
                if(lim<1)lim=1;
                if(lim>32767)lim=32767;
            }
