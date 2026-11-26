#include "local_mp_chamber.h"
#include "doors.h"
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

/* Convert the exact LEVDOOR panel to its underlying left/top door byte.
 * RIGHT and BOTTOM use the neighboring cell's canonical byte; this is
 * identical to DOORS.S::door_ptr_for, not a guessed room-side index. */
static int airlock_panel_door(const AvpMpRetailMap *m,unsigned off,
                              unsigned *door)
{
    unsigned cell,side,x,y;
    if(!m||!m->bytes||!m->width||!m->height ||
       off>=m->width*m->height*8u)return 0;
    cell=off/8u;side=off%8u;
    x=cell%m->width;y=cell/m->width;
    switch(side){
    case 0u:*door=cell*8u+6u;break;
    case 1u:*door=cell*8u+7u;break;
    case 2u:if(x+1u>=m->width)return 0;
        *door=(cell+1u)*8u+6u;break;
    case 3u:if(y+1u>=m->height)return 0;
        *door=(cell+m->width)*8u+7u;break;
    default:return 0;
    }
    return 1;
}
int avp_mp_airlock_room(const AvpMpRetailMap *map,
                       const AvpDoorLevelMeta *meta,
                       unsigned exit_panel_offset,AvpMpAirlockRoom *out)
{
    unsigned door,ex,ey,side,rx,ry,required;
    const AvpDoorInterlockEntry *inter=NULL;
    const AvpDoorExitEntry *exit=NULL;
    if(out)memset(out,0,sizeof *out);
    if(!out||!map||!map->bytes||!meta||map->width!=64u||
       map->height!=64u||!airlock_panel_door(map,exit_panel_offset,&door))
        return 0;
    for(size_t i=0;i<meta->exit_count;i++)
        if(meta->exits[i].panel_offset==exit_panel_offset){
            exit=&meta->exits[i];break;
        }
    for(size_t i=0;i<meta->interlock_count;i++)
        if(meta->interlocks[i].door_offset==door){
            inter=&meta->interlocks[i];break;
        }
    if(!exit||!inter||inter->required_count!=1u ||
       (required=inter->required_offsets[0])>=map->width*map->height*8u ||
       required%8u!=7u)return 0;
    ex=(exit_panel_offset/8u)%map->width;
    ey=(exit_panel_offset/8u)/map->width;
    side=exit_panel_offset%8u;
    rx=(required/8u)%map->width;
    ry=(required/8u)/map->width;
    /* The retail ship chambers are fore/aft passages with exactly two
     * recessed lateral standing bays. Reject unfamiliar level designs. */
    if(rx!=ex || (side!=1u && side!=3u) ||
       (ey>ry?ey-ry:ry-ey)<3u ||
       (ey>ry?ey-ry:ry-ey)>5u || ex<1u||ex+1u>=map->width)
        return 0;
    out->exit_x=ex;out->exit_y=ey;out->exit_side=side;
    out->destination=exit->level;
    out->exit_door_offset=door;out->entry_door_offset=required;
    out->entry_x=ex;
    out->entry_y=ey<ry?ry-1u:ry;
    out->entry_side=ey<ry?3u:1u;
    /* The actual interior-side panel MUST resolve to the authored closing
     * door. If it doesn't, never create an imaginary Use command. */
    {
        unsigned check;
        if(!airlock_panel_door(map,
           ((out->entry_y*map->width+ex)*8u)+out->entry_side,&check)||
           check!=required)return 0;
    }
    {
        unsigned low=ey<ry?ey:ry,high=ey>ry?ey:ry;
        const unsigned mid=(low+high)/2u;
        /* Fill by rows near chamber centre; pair left/right before the
         * next row so two companions stand side-by-side. */
        for(unsigned pass=0;pass<2u;pass++){
            unsigned y=pass?(ey<ry?mid-1u:mid+1u):mid;
            if(y<=low || y>=high)continue;
            for(unsigned lr=0;lr<2u;lr++){
                unsigned x=lr?ex+1u:ex-1u,wall_side=lr?2u:0u;
                const uint8_t *c=avp_mp_retail_map_cell_const(map,x,y);
                const uint8_t *origin=avp_mp_retail_map_cell_const(map,ex,ey);
                if(!c||!origin||!c[5]||!origin[5] ||
                   !wall_boundary(c[wall_side]) ||
                   !avp_mp_chamber_contains(map,
                       (ex<<16)|0x8000u,(ey<<16)|0x8000u,
                       (x<<16)|0x8000u,(y<<16)|0x8000u))continue;
                if(out->seat_count>=AVP_MP_AIRLOCK_SEATS)break;
                out->seats[out->seat_count].x=x;
                out->seats[out->seat_count].y=y;
                out->seats[out->seat_count].face_side=lr?0u:2u;
                out->seat_count++;
            }
        }
    }
    return out->seat_count>=2u;
}

/* R299: the pressure panel is a ONE-WAY completed transaction until an
 * actual new crossing intent is established. Merely seeing a human standing
 * in the destination chamber (or replanning FOLLOW across levels) is not
 * evidence that anyone wants to pressurize straight back. */
void avp_mp_airlock_cycle_arrived(AvpMpAirlockCycle *cycle,
    unsigned from_level,unsigned to_level,uint64_t tick)
{
    if(!cycle)return;
    if(!from_level||!to_level||from_level==to_level){
        memset(cycle,0,sizeof *cycle);return;
    }
    cycle->arrived_from=from_level;
    cycle->arrived_level=to_level;
    cycle->arrived_tick=tick;
}
int avp_mp_airlock_cycle_reverse_block(AvpMpAirlockCycle *cycle,
    unsigned level,unsigned destination,int bot_inside,
    int human_disembarked,int any_living_human,uint64_t tick)
{
    if(!cycle||!cycle->arrived_level)return 0;
    /* A legitimate new level change through another passage ends this
     * particular ship airlock arrival guard. */
    if(level!=cycle->arrived_level){
        memset(cycle,0,sizeof *cycle);return 0;
    }
    /* A return passage can be newly requested once both participants have
     * LEFT the original arrival chamber, not merely after a time delay.
     * If all human players are dead, let autonomous bots rearm once they
     * have moved outside and the original transition has settled. */
    if(!bot_inside && (human_disembarked ||
       (!any_living_human && tick>=cycle->arrived_tick &&
        tick-cycle->arrived_tick>=60u))){
        memset(cycle,0,sizeof *cycle);return 0;
    }
    return destination==cycle->arrived_from;
}

/* R425: Do not discard a real crossing while its operator is physically
 * inside. Outside, refresh only with a matching authored exit request or
 * an existing chamber invitation. Native transitions clear the commitment. */
int avp_mp_airlock_commitment_valid(uint64_t now,uint64_t last_intent,
    int inside,int matching_exit_goal)
{
    return inside || matching_exit_goal ||
        (now>=last_intent && now-last_intent<=200u);
}
AvpMpAirlockAction avp_mp_airlock_action(const AvpMpAirlockRoom *room,
    const uint8_t *cells,size_t cell_bytes,
    const uint32_t *active_offsets,unsigned active_count)
{
    if(!room||!cells||room->entry_door_offset>=cell_bytes||
       room->exit_door_offset>=cell_bytes||
       room->entry_door_offset==room->exit_door_offset||
       active_count>2u||(active_count&&!active_offsets))return AVP_MP_AIRLOCK_WAIT;
    /* Even a zero byte can belong to a just-started opening animation.
     * Do not send pressure or reverse an in-flight close. */
    for(unsigned i=0;i<active_count;i++)
        if(active_offsets[i]==room->entry_door_offset ||
           active_offsets[i]==room->exit_door_offset)return AVP_MP_AIRLOCK_WAIT;
    if(cells[room->entry_door_offset])return AVP_MP_AIRLOCK_CLOSE_ENTRY;
    if(cells[room->exit_door_offset])return AVP_MP_AIRLOCK_CLOSE_EXIT;
    return AVP_MP_AIRLOCK_PRESSURE;
}
