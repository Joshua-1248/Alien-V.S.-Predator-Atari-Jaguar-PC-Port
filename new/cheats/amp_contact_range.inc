/* Only hostile contact and damaging-proximity checks opt into a circle.
 * Signed 64-bit differences avoid wrap, and squared fixed-point distances
 * avoid float, square roots, and heading-dependent approximations. */
static int amp_contact_in_range(const AvpAmp *a,u16 pixels)
{
    u32 lim=(u32)pixels<<9;
    u32 dx,dy;
    if(!host_normalize_diagonal_contact_range)return amp_in_range(a,pixels);
    dx=abs32diff(a->xpos,(s32)x_pos);dy=abs32diff(a->ypos,(s32)y_pos);
    if(dx>lim||dy>lim)return 0;
    return (u64)dx*dx+(u64)dy*dy<=(u64)lim*lim;
}
