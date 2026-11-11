#include "local_mp_move_guard.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int avp_mp_move_guard_peers(const AvpMpPeerCollisionView *view,
                            int32_t x,int32_t y)
{
    if(!view)return 1;
    for(unsigned q=0;q<view->count && q<AVP_MP_PEER_COUNT;q++){
        if(!view->active[q])continue;
        const int64_t dx=(int64_t)x-(int32_t)view->x[q];
        const int64_t dy=(int64_t)y-(int32_t)view->y[q];
        if(dx>-(int64_t)AVP_MP_PEER_CLEARANCE &&
           dx<(int64_t)AVP_MP_PEER_CLEARANCE &&
           dy>-(int64_t)AVP_MP_PEER_CLEARANCE &&
           dy<(int64_t)AVP_MP_PEER_CLEARANCE){
            /* If the toggle is switched on while two people ALREADY
             * overlap, never pin them together. Only permit displacement
             * which increases separation without shrinking either axis;
             * the next clear position becomes normally solid again. */
            const int64_t sx=(int64_t)view->from_x-(int32_t)view->x[q];
            const int64_t sy=(int64_t)view->from_y-(int32_t)view->y[q];
            const uint64_t ax=(uint64_t)(dx<0?-dx:dx),
                           ay=(uint64_t)(dy<0?-dy:dy);
            const uint64_t oldx=(uint64_t)(sx<0?-sx:sx),
                           oldy=(uint64_t)(sy<0?-sy:sy);
            if(oldx<AVP_MP_PEER_CLEARANCE && oldy<AVP_MP_PEER_CLEARANCE &&
               ax>=oldx && ay>=oldy && (ax>oldx||ay>oldy))continue;
            return 0;
        }
    }
    return 1;
}
void avp_mp_peer_grace_update(AvpMpPeerGrace *grace,
    uint32_t present_mask,const uintptr_t worlds[AVP_MP_PEER_COUNT],
    const uint32_t x[AVP_MP_PEER_COUNT],const uint32_t y[AVP_MP_PEER_COUNT],
    uint32_t arriving_mask)
{
    if(!grace||!worlds||!x||!y)return;
    for(unsigned p=0;p<AVP_MP_PEER_COUNT;p++)
      for(unsigned q=p+1u;q<AVP_MP_PEER_COUNT;q++){
        const int both=(present_mask&(1u<<p)) && (present_mask&(1u<<q));
        const int nearby=both && worlds[p] && worlds[p]==worlds[q] &&
            (int64_t)llabs((long long)(int32_t)x[p]-(int32_t)x[q])<AVP_MP_PEER_CLEARANCE &&
            (int64_t)llabs((long long)(int32_t)y[p]-(int32_t)y[q])<AVP_MP_PEER_CLEARANCE;
        if(!nearby){
            grace->ignore[p]&=(uint16_t)~(1u<<q);
            grace->ignore[q]&=(uint16_t)~(1u<<p);
        }else if(arriving_mask&((1u<<p)|(1u<<q))){
            grace->ignore[p]|=(uint16_t)(1u<<q);
            grace->ignore[q]|=(uint16_t)(1u<<p);
        }
      }
}
int avp_mp_peer_grace_exempt(const AvpMpPeerGrace *g,
                             unsigned p,unsigned q)
{
    return g&&p<AVP_MP_PEER_COUNT&&q<AVP_MP_PEER_COUNT&&p!=q&&
      (g->ignore[p]&(1u<<q)) && (g->ignore[q]&(1u<<p));
}
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
