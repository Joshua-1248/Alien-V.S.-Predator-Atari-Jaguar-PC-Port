/* Single-edge collision: one original wall check, no corner probes. The
 * cell coordinates are already known, so door addressing needs no divide.
 * Corner/overlapping width bands retain the original SafePos. */
static u8 safe_pos_edge(const u8 *map,unsigned w,unsigned h,
                        unsigned x,unsigned y,unsigned slot,u16 coord,u8 height)
{
    const u8 *cell=map+((size_t)y*w+x)*8u;
    const unsigned wall=cell[slot];
    if (!wall) return AVP_COLL_SAFE;
    const unsigned flags=fullbits[wall];
    if (flags&AVP_WB_SOLID) return AVP_COLL_WALL;
    const unsigned pos=(coord>>9)^((wall&128u)?127u:0u);
    if ((flags&AVP_WB_LEFTONLY) && pos<14u) return AVP_COLL_WALL;
    if ((flags&AVP_WB_2SIDES) && (pos<14u || pos>113u)) return AVP_COLL_WALL;
    if (!(flags&AVP_WB_DOOR)) return AVP_COLL_SAFE;
    unsigned open;
    switch (slot) {
    case 0: if (!x) return AVP_COLL_WALL; open=cell[6]; break;
    case 1: if (!y) return AVP_COLL_WALL; open=cell[7]; break;
    case 2: if (x+1u>=w) return AVP_COLL_WALL; open=cell[14]; break;
    default: if (y+1u>=h) return AVP_COLL_WALL; open=cell[(size_t)w*8u+7u]; break;
    }
    if (!(flags&AVP_WB_TBDOOR)) {
        if (flags&AVP_WB_EDGEDOOR) return pos<=144u-open?AVP_COLL_DOOR:AVP_COLL_SAFE;
        return (pos<=80u-open || (int)pos>=48+(int)open)?AVP_COLL_DOOR:AVP_COLL_SAFE;
    }
    if (flags&AVP_WB_EDGEDOOR) return height>=open?AVP_COLL_DOOR:AVP_COLL_SAFE;
    return ((int)height<=64-(int)open || (int)height>=64+(int)open)?AVP_COLL_DOOR:AVP_COLL_SAFE;
}
