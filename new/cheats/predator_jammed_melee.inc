/* A successful Predator close-combat strike may break a JAMMED door open.
 * Use the exact facing door chosen by retail DoorKeys and check the original
 * melee reach: swinging in empty space cannot open one.
 * Only original access metadata may identify a jammed door. No dynamic doors
 * or non-jammed panels are modified, and deathmatch remains separate. */
int avp_doors_predator_melee_jammed(s16 weapon,u32 max_distance)
{
    u8 *cell,*door,*panelp,panel,clean;
    unsigned side;
    unsigned frac,gap;
    u32 reach;
    if(!host_predator_jammed_melee||mp_dm_no_terminal||player_type!=PT_PREDATOR ||
       (weapon!=1 && weapon!=4)||!maze_data||!max_distance)return 0;
    if(new_pos<(u32)((size_t)maze_w*maze_h*CELL))cell=maze_data+new_pos;
    else cell=cell_at((unsigned)(x_pos>>16),(unsigned)(y_pos>>16));
    if(!cell)return 0;
    side=facing_side();panelp=cell+side;panel=*panelp;clean=(u8)(panel&0x7fu);
    if(!(fullbits[clean]&AVP_WB_DOOR))return 0;
    door=door_ptr_for(cell,side);
    if(door<maze_data||door>=maze_data+(size_t)maze_w*maze_h*CELL ||
       !jammed_door(door)||*door)return 0;
    /* The original melee fire_distance is in 16.16 cell units. Require
     * the panel to be physically within that reach: don't let a swing
     * at a wall across the room activate a jammed door. This is an
     * optional host interaction and never changes the original wall ray. */
    reach=max_distance>=65536u?65535u:max_distance;
    frac=(side&1u)?((u16)y_pos):((u16)x_pos);
    gap=(side==0u||side==1u)?frac:(0x10000u-frac);
    if(gap>reach)return 0;
    if(start_normal_or_double(cell,panelp,panel,side,0,1)){
        host_jammed_forced_ever=1;
        return 1;
    }
    return 0;
}

