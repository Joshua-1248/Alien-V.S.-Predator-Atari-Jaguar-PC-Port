#include "jaguar/avp_gpu/tracker_brush.h"

enum { RING_WIDTH=128, RING_HEIGHT=60, RING_ROW_BYTES=16, RING_FRAMES=8 };

int avp_gpu_tracker_ring_bit(const u8 *brush,size_t brush_size,
                             unsigned frame,unsigned x,unsigned y)
{
    size_t index;
    if(!brush||frame>=RING_FRAMES||x>=RING_WIDTH||y>=RING_HEIGHT)return 0;
    index=(size_t)frame*RING_ROW_BYTES*RING_HEIGHT+
          (size_t)y*RING_ROW_BYTES+x/8u;
    if(index>=brush_size)return 0;
    return (brush[index]&(u8)(0x80u>>(x&7u)))!=0;
}

unsigned avp_gpu_tracker_blip_index(s16 view_x,s16 view_z)
{
    int x=view_x<0?-(int)view_x:(int)view_x;
    int z=view_z<0?-(int)view_z:(int)view_z;
    return (unsigned)(x|z)&7u;
}
