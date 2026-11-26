    int do_use;
    if(pc_alien_use_active){
        do_use=pc_alien_use_down&&(lift_key||!pc_alien_use_previous);
        pc_alien_use_previous=pc_alien_use_down;
    }else do_use=pressed(key,FIRE_B);
