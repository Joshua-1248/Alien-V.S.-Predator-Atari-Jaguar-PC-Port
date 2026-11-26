/* The 50-health death spray has its OWN optional radial check: it must
 * not inherit the independently selectable melee / puddle option. */
static int amp_alien_death_spray_in_range(const AvpAmp *a)
{
    const u32 lim=96u<<9;
    u32 dx,dy;
    if(!host_normalize_diagonal_alien_death_spray_range)
        return amp_in_range(a,96); /* exact original Jaguar square */
    dx=abs32diff(a->xpos,(s32)x_pos);
    dy=abs32diff(a->ypos,(s32)y_pos);
    if(dx>lim||dy>lim)return 0;
    return (u64)dx*dx+(u64)dy*dy<=(u64)lim*lim;
}
