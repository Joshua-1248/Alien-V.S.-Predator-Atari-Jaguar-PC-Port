/* Interior 3x3 AMP query. In-grid validated actor coordinates are at most
 * 0x003fffff, so subtraction and absolute value cannot overflow.  The live
 * owner, static/mode, creature and cell checks are the original rules. */
static inline int amp_owned_hit(u16 n,u32 x,u32 y,s32 px,s32 py)
{
    static const u16 widths[40]={
    (50<<9)*85/100,(50<<9)*85/100,(50<<9)*85/100,
    (40<<9)*85/100,(40<<9)*85/100,50<<9,
    0,0,0,0,0,
    (40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,
    (10<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,(35<<9)*85/100,(40<<9)*85/100,(35<<9)*85/100,
    (40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,
    0,0,0,0,0,(50<<9)*85/100,(40<<9)*85/100,(40<<9)*85/100,(80<<9)*85/100,(40<<9)*85/100,(80<<9)*85/100
    };
    if(!n || n>AVP_NUM_AMPS)return 0;
    const AvpAmp *a=&amp_data[n-1u];
    if((!a->mode && !a->host_static) || ((u32)a->xpos>>16)!=x || ((u32)a->ypos>>16)!=y || a->creature>=40u)return 0;
    const s32 w=widths[a->creature];
    if(!w)return 0;
    const s32 dx=a->xpos-px,dy=a->ypos-py;
    return dx>-w && dx<w && dy>-w && dy<w;
}
static __attribute__((noinline)) int amp_interior_hit(const u16 *r,u32 x,u32 y,s32 px,s32 py)
{
    const unsigned s=AVP_AMP_GRID_W;
    for(unsigned j=0;j<3;j++,r+=s){
        if(amp_owned_hit(r[0],x-1,y+j-1,px,py) || amp_owned_hit(r[1],x,y+j-1,px,py) || amp_owned_hit(r[2],x+1,y+j-1,px,py))return 1;
    }
    return 0;
}
