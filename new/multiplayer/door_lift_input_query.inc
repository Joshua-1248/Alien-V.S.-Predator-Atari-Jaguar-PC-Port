/* Host input adapter query only. DOORS.S::DoorKeys is left intact: the
 * original active-low FIRE_B + JOY_UP/JOY_DOWN decision runs unchanged.
 * Resolve precisely the same cell, facing side and panel-file-id as DoorKeys
 * so a PC analog stick is converted to Jaguar lift input only at a real lift. */
int avp_doors_player_facing_lift(void)
{
    u8 *cell;
    u8 panel,clean,file_id;
    unsigned side;
    if(!maze_data)return 0;
    if(new_pos<(u32)((size_t)maze_w*maze_h*CELL))
        cell=maze_data+new_pos;
    else
        cell=cell_at((unsigned)(x_pos>>16),(unsigned)(y_pos>>16));
    if(!cell)return 0;
    side=facing_side();
    panel=cell[side];
    clean=(u8)(panel&0x7fu);
    file_id=(clean>=1u&&clean<=AVP_MAX_PANELS)?panel_list[clean-1u]:0xffu;
    return panel_id_is(file_id,panel_ids.lift_back)||
           panel_id_is(file_id,panel_ids.lift_side);
}

