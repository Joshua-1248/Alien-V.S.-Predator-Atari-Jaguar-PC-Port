static int pc_alien_use_active,pc_alien_use_down,pc_alien_use_previous;
void avp_doors_pc_alien_use(int active,int down)
{
    if(!active){pc_alien_use_active=pc_alien_use_down=pc_alien_use_previous=0;return;}
    if(!pc_alien_use_active)pc_alien_use_previous=0;
    pc_alien_use_active=1;pc_alien_use_down=down?1:0;
}
