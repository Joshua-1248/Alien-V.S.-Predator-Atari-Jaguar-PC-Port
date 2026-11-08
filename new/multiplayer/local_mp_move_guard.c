#include "local_mp_move_guard.h"
#include <stddef.h>
#include <stdint.h>

static int floor_cell(const uint8_t *cells,unsigned width,unsigned height,
                      int64_t x,int64_t y)
{
    if(!cells || !width || !height || width>64u || height>64u ||
       x<0 || y<0 || (uint64_t)x>=((uint64_t)width<<16) ||
       (uint64_t)y>=((uint64_t)height<<16))return 0;
    return cells[(((size_t)((uint64_t)y>>16)*width)+
                   ((size_t)((uint64_t)x>>16)))*8u+5u]!=0u;
}
int avp_mp_move_guard_floor(const uint8_t *cells,unsigned width,unsigned height,
                            int32_t x,int32_t y,unsigned radius)
{
    /* The source player's halfwidth is 40<<9 == 0x5000.  Reject absent
     * floors underneath ANY part of the player, including a position whose
     * center appears valid but protrudes into exterior void. */
    const int64_t cx=x,cy=y,r=radius;
    if(radius>0x8000u || !floor_cell(cells,width,height,cx,cy))return 0;
    if(!r)return 1;
    return floor_cell(cells,width,height,cx-r,cy-r) &&
           floor_cell(cells,width,height,cx-r,cy+r) &&
           floor_cell(cells,width,height,cx+r,cy-r) &&
           floor_cell(cells,width,height,cx+r,cy+r);
}
int avp_mp_move_guard_sweep(int32_t from_x,int32_t from_y,
                             int32_t to_x,int32_t to_y,unsigned step,
                             AvpMpMovementSample sample,void *user)
{
    const int64_t dx=(int64_t)to_x-from_x,dy=(int64_t)to_y-from_y;
    const uint64_t ax=(uint64_t)(dx<0?-dx:dx),ay=(uint64_t)(dy<0?-dy:dy);
    const uint64_t extent=ax>ay?ax:ay;
    uint64_t count;
    if(!step || !sample)return 1; /* Fail closed, not noclip. */
    count=extent/step+(extent%step!=0u);
    if(count>1024u)return 1; /* Out-of-range velocity cannot bypass walls. */
    if(!count)return 0;
    for(uint64_t i=1u;i<=count;i++){
        int32_t x=(int32_t)((int64_t)from_x+(dx*(int64_t)i)/(int64_t)count);
        int32_t y=(int32_t)((int64_t)from_y+(dy*(int64_t)i)/(int64_t)count);
        int result=sample(x,y,user);
        if(result)return result;
    }
    return 0;
}
