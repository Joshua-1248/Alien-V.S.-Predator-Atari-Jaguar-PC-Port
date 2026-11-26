/* Oblique LOS on original-size mazes: 0 delegates, 1 proves clear,
 * 2 proves blocked. Only empty or unconditional solid walls are handled;
 * directional walls and doors retain the original scanner. */
static int oblique_wall(const u8 *map,unsigned w,unsigned h,int x,int y,unsigned side)
{
    if ((unsigned)x>=w || (unsigned)y>=h) return 2;
    const u8 wall=map[((unsigned)y*w+(unsigned)x)*8u+side];
    if (!wall) return 1;
    return (fullbits[wall]&AVP_WB_SOLID)?2:0;
}
static int oblique_los_result(const AvpXY *from,const AvpXY *to)
{
    if (from->x==to->x || from->y==to->y) return 0;
    const unsigned w=micro_width, h=micro_height;
    const u8 *map=micro_maze;
    int x=(int)((u32)from->x>>16), y=(int)((u32)from->y>>16);
    if (!map || w>64 || h>64 || (unsigned)x>=w || (unsigned)y>=h ||
        ((u32)to->x>>16)>=w || ((u32)to->y>>16)>=h) return 0;
    const s32 dx=to->x-from->x, dy=to->y-from->y;
    const u32 ax=dx<0?(u32)-dx:(u32)dx, ay=dy<0?(u32)-dy:(u32)dy;
    const int major_y=ay>=ax;
    const u32 major=major_y?ay:ax, minor=major_y?ax:ay;
    const int px=major_y?0:(dx<0?-1:1), py=major_y?(dy<0?-1:1):0;
    const int sx=major_y?(dx<0?-1:1):0, sy=major_y?0:(dy<0?-1:1);
    const unsigned pw=major_y?(dy<0?1u:3u):(dx<0?0u:2u);
    const unsigned sw=major_y?(dx<0?0u:2u):(dy<0?1u:3u);
    const u16 local_x=major_y?(dy<0?(u16)~from->y:(u16)from->y):(dx<0?(u16)~from->x:(u16)from->x);
    u16 local_y=major_y?(dx>=0?(u16)~from->x:(u16)from->x):(dy>=0?(u16)~from->y:(u16)from->y);
    u32 step=(u16)~local_x,total=0;
    if (!step) return 0;
    /* Valid endpoints bound minor<<8 to 30 bits. */
    u32 full=(minor<<8)/((major>>8)+1u);
    if (full>65535u) full=65535u;
    u16 ys=(u16)((full*step)>>16);
    for (unsigned guard=0;guard<128u;guard++) {
        if (total>=major) return 1;
        if (local_y<ys) {
            const int wall=oblique_wall(map,w,h,x,y,sw);
            if (wall!=1) {
                /* Original secondary crossing distance uses even-rounded
                 * multiplication. Zero distance is also the clear sentinel. */
                const u16 ratio=(u16)(((u32)local_y<<15)/((ys>>1)+1u));
                const u32 dist=total+((((step>>1)*(u32)ratio)>>16)<<1);
                if (dist>=major) return 1;
                if (wall==2) return dist?2:1;
                return 0;
            }
            x+=sx;y+=sy;
        }
        local_y=(u16)(local_y-ys);
        const u32 dist=total+step;
        if (dist>=major) return 1;
        const int wall=oblique_wall(map,w,h,x,y,pw);
        if (wall!=1) return wall;
        x+=px;y+=py;
        total=dist;step=65536u;ys=(u16)full;
    }
    return 0;
}
