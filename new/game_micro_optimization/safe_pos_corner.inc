/* R408: exact two-wall corner check for nonoverlapping wall bands.
 * The original tests neighboring wall flags before each local wall. The
 * relative order is immaterial: all reads are pure and any wall hit wins.
 * Wide overlapping bands still use the original routine. */
static int corner_outside_solid(unsigned wall,int from_left)
{
    const unsigned f=fullbits[wall];
    return (f&(AVP_WB_SOLID|AVP_WB_2SIDES)) ||
        ((f&AVP_WB_LEFTONLY) && ((wall<128u)==from_left));
}
/* Shared wall logic specialized for a known cell and neighboring door. */
static inline __attribute__((always_inline)) u8 corner_wall(const u8 *c,unsigned slot,u16 coord,u8 height,unsigned x,unsigned y,unsigned w,unsigned h)
{
    const unsigned wall=c[slot];
    if (!wall) return AVP_COLL_SAFE;
    const unsigned flags=fullbits[wall];
    if (flags&AVP_WB_SOLID) return AVP_COLL_WALL;
    const unsigned pos=(coord>>9)^((wall&128u)?127u:0u);
    if ((flags&AVP_WB_LEFTONLY) && pos<14u) return AVP_COLL_WALL;
    if ((flags&AVP_WB_2SIDES) && (pos<14u || pos>113u)) return AVP_COLL_WALL;
    if (!(flags&AVP_WB_DOOR)) return AVP_COLL_SAFE;
    unsigned open;
    switch (slot) {
    case 0: if (!x) return AVP_COLL_WALL; open=c[6]; break;
    case 1: if (!y) return AVP_COLL_WALL; open=c[7]; break;
    case 2: if (x+1u>=w) return AVP_COLL_WALL; open=c[14]; break;
    default: if (y+1u>=h) return AVP_COLL_WALL; open=c[(size_t)w*8u+7u]; break;
    }
    if (!(flags&AVP_WB_TBDOOR)) {
        if (flags&AVP_WB_EDGEDOOR) return pos<=144u-open?AVP_COLL_DOOR:AVP_COLL_SAFE;
        return (pos<=80u-open || (int)pos>=48+(int)open)?AVP_COLL_DOOR:AVP_COLL_SAFE;
    }
    if (flags&AVP_WB_EDGEDOOR) return height>=open?AVP_COLL_DOOR:AVP_COLL_SAFE;
    return ((int)height<=64-(int)open || (int)height>=64+(int)open)?AVP_COLL_DOOR:AVP_COLL_SAFE;
}
static __attribute__((noinline)) u8 safe_pos_corner(u8 height,s32 xpos,s32 ypos,u16 near)
{
    const u8 *map=micro_maze;
    const unsigned x=(u32)xpos>>16,y=(u32)ypos>>16;
    const unsigned w=micro_width,h=micro_height;
    const u16 fx=(u16)xpos,fy=(u16)ypos;
    if (!map || x>=w || y>=h) return AVP_COLL_WALL;
    const size_t stride=(size_t)w*8u;
    const u8 *c=map+(size_t)y*stride+(size_t)x*8u;
    const int left=fx<near,top=fy<near;
    const unsigned horizontal=left?0u:2u,vertical=top?1u:3u;
    const u8 *other=top?(y?c-stride:c):(y+1u<h?c+stride:c);
    const unsigned a=other[horizontal];
    const int has_neighbor=left?x!=0u:x+1u<w;
    const unsigned b=has_neighbor?(left?c-8:c+8)[vertical]:0u;
    if (!(a|b|c[horizontal]|c[vertical]) && !fullbits[0]) return AVP_COLL_SAFE;
    if (corner_outside_solid(a,left==top)) return AVP_COLL_WALL;
    if (has_neighbor && corner_outside_solid(b,left!=top)) return AVP_COLL_WALL;
    const u8 lr=corner_wall(c,horizontal,left?(u16)~fy:fy,height,x,y,w,h);
    if (lr==AVP_COLL_WALL) return lr;
    const u8 tb=corner_wall(c,vertical,top?fx:(u16)~fx,height,x,y,w,h);
    return tb==AVP_COLL_WALL?tb:(u8)(lr|tb);
}
