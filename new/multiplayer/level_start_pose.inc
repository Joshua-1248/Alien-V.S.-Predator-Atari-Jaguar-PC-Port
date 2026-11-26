int avp_levels_start_pose(unsigned level,u32 *x,u32 *y,u32 *angle)
{
    if (!level_table || level<1u || level>level_count || !x || !y || !angle) return 0;
    *x=level_table[level-1u].start_x;
    *y=level_table[level-1u].start_y;
    *angle=level_table[level-1u].start_angle;
    return 1;
}

