#ifndef AVP_PC_MOUSE_CAPTURE_H
#define AVP_PC_MOUSE_CAPTURE_H
/* X11/Win32 pointer confinement: minimize pointer warps and the resulting
 * synthetic motion messages. Only recenter as an edge approaches. This does
 * not clamp the horizontal mouse turn or change simulation tick cadence. */
static inline int avp_pc_mouse_needs_recenter(int x,int width)
{
    int margin;
    if(width<=2)return 0;
    margin=width/8;
    if(margin<16)margin=16;
    if(margin>96)margin=96;
    if(margin>=width/2)margin=width/4;
    return x<=margin||x>=width-1-margin;
}
#endif
