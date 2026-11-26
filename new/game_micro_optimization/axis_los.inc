/* R409: exact straight-axis visibility, including live wall and door checks.
 * 0 delegates, 1 clear, 2 blocked. Zero initial distance and unsupported
 * maps retain the original scanner. No geometry data is cached. */
/* Keep the frequently empty scan small; enter the detailed path only
 * after encountering a nonzero wall. Continue from that slot, not origin. */
static __attribute__((noinline)) int axis_los_walls(const u8 *map,unsigned w,unsigned h,
    unsigned x,unsigned y,unsigned wall,int stride,const u8 *p,unsigned i,unsigned count,
    const AvpXY *from,int vertical,int forward)
{
    const u16 coord=vertical?(forward?(u16)~from->x:(u16)from->x)
                            :(forward?(u16)from->y:(u16)~from->y);
    for (;i<count;i++) {
        if (*p) {
            if (fullbits[*p]&AVP_WB_SOLID) return 2;
            const unsigned cx=vertical?x:(forward?x+i:x-i);
            const unsigned cy=vertical?(forward?y+i:y-i):y;
            if (safe_pos_edge(map,w,h,cx,cy,wall,coord,64u)!=AVP_COLL_SAFE) return 2;
        }
        if (i+1u<count) p+=stride;
    }
    return 1;
}
static int axis_los_result(const AvpXY *from,const AvpXY *to)
{
    const int vertical=from->x==to->x;
    if (!vertical && from->y!=to->y) return 0;
    const unsigned w=micro_width, h=micro_height;
    const u8 *map=micro_maze;
    const unsigned x=(u32)from->x>>16,y=(u32)from->y>>16;
    const unsigned tx=(u32)to->x>>16,ty=(u32)to->y>>16;
    if (!map || w>64u || h>64u || x>=w || tx>=w || y>=h || ty>=h) return 0;
    const u32 a=(u32)(vertical?from->y:from->x), b=(u32)(vertical?to->y:to->x);
    if (a==b) return 1;
    const int forward=b>a;
    const u32 distance=forward?b-a:a-b;
    const unsigned first=forward?(unsigned)(u16)~a:(unsigned)(u16)a;
    /* The original has special d_total==0 behavior on a boundary. */
    if (!first) return 0;
    if (distance<=first) return 1;
    const unsigned count=1u+((distance-first-1u)>>16);
    /* Count cannot exceed the number of cell crossings on this axis. */
    const unsigned crossings=vertical?(forward?ty-y:y-ty):(forward?tx-x:x-tx);
    if (count>crossings) return 0;
    const unsigned wall=vertical?(forward?3u:1u):(forward?2u:0u);
    const int stride=(vertical?(int)(w*8u):8)*(forward?1:-1);
    const u8 *p=map+((size_t)y*w+x)*8u+wall;
    unsigned i=0;
    for (;count-i>=4u;i+=4u) {
        if (p[0]|p[stride]|p[2*stride]|p[3*stride]) break;
        if (count-i==4u) return 1;
        p+=4*stride;
    }
    for (;i<count;i++) {
        if (*p) { if (fullbits[*p]&AVP_WB_SOLID) return 2; return axis_los_walls(map,w,h,x,y,wall,stride,p,i,count,from,vertical,forward); }
        if (i+1u<count) p+=stride;
    }
    return 1;
}
