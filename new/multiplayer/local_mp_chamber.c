#include "local_mp_chamber.h"
#include <string.h>
#include <stdlib.h>
/* Original DOORS.S InitDoors() maps wall high-nibble types 1..3 to solid,
 * 4..11 to doors; 13/14 are directional bulkhead edges, 15 is two-sided.
 * Type ZERO with a nonzero low index is SOLID too; only exact 0/0x80
 * are empty wall slots. This distinction is essential on retail levels.
 * Keep those as chamber boundaries EVEN while a door is fully open. */
static int wall_boundary(uint8_t wall)
{
    unsigned type=(wall&0x78u)>>3;
    return (wall&0x7fu)!=0u && (type<=11u || type>=13u);
}
static int can_cross(const AvpMpRetailMap *m,unsigned x,unsigned y,
                     unsigned nx,unsigned ny)
{
    const uint8_t *a=avp_mp_retail_map_cell_const(m,x,y);
    const uint8_t *b=avp_mp_retail_map_cell_const(m,nx,ny);
    if(!a||!b)return 0;
    if(nx==x+1u)return !wall_boundary(a[2])&&!wall_boundary(b[0]);
    if(x==nx+1u)return !wall_boundary(a[0])&&!wall_boundary(b[2]);
    if(ny==y+1u)return !wall_boundary(a[3])&&!wall_boundary(b[1]);
    return !wall_boundary(a[1])&&!wall_boundary(b[3]);
}
int avp_mp_chamber_contains(const AvpMpRetailMap *m,uint32_t lx,uint32_t ly,
                           uint32_t ox,uint32_t oy)
{
    /* Retail maps are <=64x64; this is a conservative chamber guard. A
     * connected area larger than thirty-two cells is not provably the same
     * elevator/airlock and must NEVER transport unrelated hallway players. */
    unsigned queue[4096],head=0,tail=0;
    uint8_t seen[4096]={0};
    unsigned x=(unsigned)(lx>>16),y=(unsigned)(ly>>16);
    unsigned tx=(unsigned)(ox>>16),ty=(unsigned)(oy>>16);
    unsigned target,origin;
    int target_found=0;
    if(!m||!m->bytes||!m->width||!m->height||m->width>64||m->height>64||
       x>=m->width||y>=m->height||tx>=m->width||ty>=m->height)return 0;
    origin=y*m->width+x;target=ty*m->width+tx;
    queue[tail++]=origin;seen[origin]=1;
    while(head<tail){
        unsigned cell=queue[head++],cx=cell%m->width,cy=cell/m->width;
        if(cell==target)target_found=1;
        /* Enormous connected areas include corridors; they are not airlocks. */
        if(tail>32u)return 0;
        if(cx==0||cy==0||cx+1u>=m->width||cy+1u>=m->height)return 0;
        static const int dx[4]={-1,1,0,0},dy[4]={0,0,-1,1};
        for(unsigned k=0;k<4u;k++){
            unsigned nx=(unsigned)((int)cx+dx[k]);
            unsigned ny=(unsigned)((int)cy+dy[k]);
            unsigned next=ny*m->width+nx;
            if(!seen[next]&&can_cross(m,cx,cy,nx,ny)){
                seen[next]=1;queue[tail++]=next;
            }
        }
    }
    return target_found;
}

int avp_mp_chamber_translate(int32_t destination_x,int32_t destination_y,
                              uint32_t leader_x,uint32_t leader_y,
                              uint32_t passenger_x,uint32_t passenger_y,
                              int32_t *out_x,int32_t *out_y)
{
    const int64_t x=(int64_t)destination_x+(int64_t)passenger_x-(int64_t)leader_x;
    const int64_t y=(int64_t)destination_y+(int64_t)passenger_y-(int64_t)leader_y;
    if(!out_x||!out_y||destination_x<0||destination_y<0||
       x<0||y<0||x>INT32_MAX||y>INT32_MAX)return 0;
    *out_x=(int32_t)x;*out_y=(int32_t)y;
    return 1;
}

int avp_mp_chamber_translate_or_stack(int32_t destination_x,int32_t destination_y,
                              uint32_t leader_x,uint32_t leader_y,
                              uint32_t passenger_x,uint32_t passenger_y,
                              int32_t *out_x,int32_t *out_y)
{
    if(!out_x||!out_y||destination_x<0||destination_y<0)return 0;
    if(avp_mp_chamber_translate(destination_x,destination_y,
            leader_x,leader_y,passenger_x,passenger_y,out_x,out_y))return 1;
    *out_x=destination_x;
    *out_y=destination_y;
    return 1;
}

/* Original PLAYER_WIDTH 40 -> player clearance of 0x5000 (16.16).
 * Retain an arrival exemption until there is sufficient independent space
 * on at least one axis. Never add exemptions during an ordinary movement
 * tick; the two actors can then be solid again with a collision toggle. */
void avp_mp_shared_arrival_update(AvpMpSharedArrivalGrace *state,
                                  const uint8_t present[4],
                                  const unsigned world_level[4],
                                  const uint32_t x[4],const uint32_t y[4],
                                  unsigned arriving_mask)
{
    if(!state||!present||!world_level||!x||!y)return;
    for(unsigned p=0;p<4u;p++)for(unsigned q=p+1u;q<4u;q++){
        int together=present[p]&&present[q]&&world_level[p]==world_level[q]&&
            llabs((int64_t)x[p]-(int64_t)x[q])<0x5000 &&
            llabs((int64_t)y[p]-(int64_t)y[q])<0x5000;
        if(!together){
            state->ignore_player_collision[p]&=(uint8_t)~(1u<<q);
            state->ignore_player_collision[q]&=(uint8_t)~(1u<<p);
        }else if(arriving_mask&((1u<<p)|(1u<<q))){
            state->ignore_player_collision[p]|=(uint8_t)(1u<<q);
            state->ignore_player_collision[q]|=(uint8_t)(1u<<p);
        }
    }
}
int avp_mp_shared_arrival_exempt(const AvpMpSharedArrivalGrace *state,
                                 unsigned p,unsigned q)
{
    return state&&p<4u&&q<4u&&p!=q &&
           (state->ignore_player_collision[p]&(1u<<q)) &&
           (state->ignore_player_collision[q]&(1u<<p));
}
