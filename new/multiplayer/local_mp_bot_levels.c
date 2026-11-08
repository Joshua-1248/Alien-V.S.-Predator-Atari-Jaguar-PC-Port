#include "local_mp_bot_levels.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PORTAL_LIMIT 512u
#define LIFTDOR 64u /* DOORS.S do_lifts' authored wall identifier */
#define CELLS AVP_MP_BOT_MAP_CELLS

typedef struct BotPortal {
    uint16_t src_comp,dst_comp;
    uint16_t x,y,landing_x,landing_y;
    uint8_t from,to,side,kind;
} BotPortal;
typedef struct BotVisit {
    uint16_t component,first;
    uint8_t level;
    uint8_t hops;
} BotVisit;

static unsigned index_of(unsigned x,unsigned y){return y*64u+x;}
static uint16_t comp_at(const AvpMpBotLevelLayout *l,const uint16_t *c,
                        unsigned x,unsigned y)
{
    if(!l->map.cells||x>=l->map.width||y>=l->map.height)return 0;
    return c[index_of(x,y)];
}
static unsigned panel_id(const AvpMpBotLevelLayout *l,unsigned x,
                         unsigned y,unsigned side)
{
    const AvpMpBotMap *m=&l->map;
    unsigned index,clean;
    if(!m->cells||x>=m->width||y>=m->height||side>=4u)return 0xffu;
    index=((y*m->width+x)*8u)+side;
    clean=m->cells[index]&0x7fu;
    return clean>=1u&&clean<=AVP_MAX_PANELS?
        l->panel_list[clean-1u]:0xffu;
}
static unsigned portal_kind(const AvpMpBotLevelLayout *l,unsigned file)
{
    return ((l->panel_ids.wall2!=0xffu&&file==l->panel_ids.wall2)||
            (l->panel_ids.airduct1!=0xffu&&file==l->panel_ids.airduct1))?
               AVP_MP_BOT_TRANS_DUCT:AVP_MP_BOT_TRANS_EXIT;
}
/* Some original airlock faces are numeric door-wall IDs (e.g. 40), NOT
 * entries in the RDB LoadPanels list. They are still authored LEVDOOR
 * connections, but only if this exact panel resolves to a real EXIT door.
 * DOORS.S::door_ptr_for resolves RIGHT/BOTTOM against the neighbor's
 * LEFT/TOP door byte. This is why level-14/15 exit offsets differ from
 * their access records by 516 bytes in the 64-column retail maze. */
static int source_airlock(const AvpMpBotLevelLayout *l,
                          unsigned x,unsigned y,unsigned side)
{
    const AvpMpBotMap *m=&l->map;
    unsigned offset,wall;
    int found_exit=0;
    if(!m->cells||!m->wall_flags||!l->meta||side>=4u||
       x>=m->width||y>=m->height)return 0;
    wall=m->cells[((size_t)y*m->width+x)*8u+side];
    if(!(m->wall_flags[wall]&AVP_WB_DOOR))return 0;
    switch(side){
    case 0u:offset=(y*m->width+x)*8u+6u;break;
    case 1u:offset=(y*m->width+x)*8u+7u;break;
    case 2u:
        if(x+1u>=m->width)return 0;
        offset=(y*m->width+x+1u)*8u+6u;break;
    default:
        if(y+1u>=m->height)return 0;
        offset=((y+1u)*m->width+x)*8u+7u;break;
    }
    if(m->cells[offset])return 0; /* access_ok bypasses closed-door test. */
    for(size_t j=0;j<l->meta->access_count;j++)
        if(l->meta->access[j].door_offset==offset &&
           l->meta->access[j].code==AVP_ACC_EXIT){found_exit=1;break;}
    if(!found_exit)return 0;
    for(size_t j=0;j<l->meta->interlock_count;j++){
        const AvpDoorInterlockEntry *ie=&l->meta->interlocks[j];
        if(ie->door_offset!=offset)continue;
        for(size_t k=0;k<ie->required_count;k++)
            if(ie->required_offsets[k]>=m->width*m->height*8u ||
               m->cells[ie->required_offsets[k]])return 0;
        return 1;
    }
    return 0; /* airlock_exit rejects a missing authored interlock. */
}

static int add_portal(BotPortal *p,unsigned *count,
     const AvpMpBotLevelLayout layouts[AVP_MAX_LEVEL],
     const uint16_t *components,unsigned species,unsigned from,
     unsigned x,unsigned y,unsigned side,unsigned to,
     unsigned lx,unsigned ly,unsigned kind)
{
    uint16_t a,b;
    if(from<1u||from>AVP_MAX_LEVEL||to<1u||to>AVP_MAX_LEVEL||
       !avp_mp_bot_transition_allowed(species,from,to,
                                      (enum AvpMpBotTransitionKind)kind))return 1;
    a=comp_at(&layouts[from-1u],components+(from-1u)*CELLS,x,y);
    b=comp_at(&layouts[to-1u],components+(to-1u)*CELLS,lx,ly);
    if(!a||!b)return 1; /* No true walkable landing / panel. */
    if(*count>=PORTAL_LIMIT)return 0; /* Fail closed, never silently truncate. */
    p[*count]=(BotPortal){a,b,(uint16_t)x,(uint16_t)y,
        (uint16_t)lx,(uint16_t)ly,(uint8_t)from,(uint8_t)to,
        (uint8_t)side,(uint8_t)kind};
    ++*count;
    return 1;
}

int avp_mp_bot_plan_levels(const AvpMpBotLevelLayout layouts[AVP_MAX_LEVEL],
    unsigned species,unsigned current,unsigned sx,unsigned sy,
    unsigned goal_level,unsigned gx,unsigned gy,int goal_known,
    AvpMpBotLevelPlan *out)
{
    uint16_t *components=NULL;
    BotPortal *portals=NULL;
    BotVisit visits[PORTAL_LIMIT+1u];
    unsigned portal_count=0,head=0,tail=0,first=UINT16_MAX;
    uint16_t begin,goal=0u;
    int success=0;
    if(out)memset(out,0,sizeof *out);
    if(!out||!layouts||species>2u||current<1u||current>AVP_MAX_LEVEL||
       goal_level<1u||goal_level>AVP_MAX_LEVEL)return 0;
    components=(uint16_t*)calloc((size_t)AVP_MAX_LEVEL*CELLS,sizeof *components);
    portals=(BotPortal*)calloc(PORTAL_LIMIT,sizeof *portals);
    if(!components||!portals)goto done;
    for(unsigned i=0;i<AVP_MAX_LEVEL;i++)
        if(layouts[i].map.cells)
            (void)avp_mp_bot_components(&layouts[i].map,components+i*CELLS);
    begin=comp_at(&layouts[current-1u],
                  components+(current-1u)*CELLS,sx,sy);
    if(goal_known)
        goal=comp_at(&layouts[goal_level-1u],
                     components+(goal_level-1u)*CELLS,gx,gy);
    if(!begin||(goal_known&&!goal))goto done;
    /* This is a cross-region/level planner, not a replacement for the
     * ordinary single-level costed route. */
    if(current==goal_level&&(!goal_known||begin==goal))goto done;
    for(unsigned level=1u;level<=AVP_MAX_LEVEL;level++){
        const AvpMpBotLevelLayout *l=&layouts[level-1u];
        const AvpMpBotMap *m=&l->map;
        if(!m->cells)continue;
        if(l->meta)for(size_t i=0;i<l->meta->exit_count;i++){
            const AvpDoorExitEntry *e=&l->meta->exits[i];
            const unsigned idx=e->panel_offset/8u,side=e->panel_offset%8u;
            unsigned x=idx%m->width,y=idx/m->width,file;
            if(side>=4u||y>=m->height)continue;
            file=panel_id(l,x,y,side);
            if(file==0xffu && !source_airlock(l,x,y,side))
                continue; /* Never invent an unregistered exit. */
            if(!add_portal(portals,&portal_count,layouts,components,species,
                    level,x,y,side,e->level,e->x,e->y,
                    portal_kind(l,file)))goto done;
        }
        /* Original DOORS.S only allows lifts between adjacent 1..5 levels,
         * only where this source cell has both a lift facing and LIFTDOR.
         * No guessed vertical links or forbidden jammed lift directions. */
        if(level<=5u && species!=1u){
            for(unsigned y=0;y<m->height;y++)for(unsigned x=0;x<m->width;x++){
                const uint8_t *cell=m->cells+((size_t)y*m->width+x)*8u;
                int has_liftdoor=0;
                for(unsigned s=0;s<4u;s++)if(cell[s]==LIFTDOR)has_liftdoor=1;
                if(!has_liftdoor)continue;
                for(unsigned s=0;s<4u;s++){
                    unsigned file=panel_id(l,x,y,s);
                    if(file==0xffu ||
                       !((l->panel_ids.lift_back!=0xffu &&
                          file==l->panel_ids.lift_back)||
                         (l->panel_ids.lift_side!=0xffu &&
                          file==l->panel_ids.lift_side)))continue;
                    for(int d=-1;d<=1;d+=2){
                        int next=(int)level+d;
                        if(next<1||next>5||
                           avp_mp_bot_lift_jammed(level,x,y,d))continue;
                        if(!add_portal(portals,&portal_count,layouts,components,
                            species,level,x,y,s,(unsigned)next,x,y,
                            AVP_MP_BOT_TRANS_LIFT))goto done;
                    }
                }
            }
        }
    }
    visits[tail++]=(BotVisit){begin,UINT16_MAX,(uint8_t)current,0};
    while(head<tail){
        const BotVisit state=visits[head++];
        for(unsigned i=0;i<portal_count;i++){
            const BotPortal *p=&portals[i];
            unsigned j,seen=0;
            uint16_t route_first;
            if(p->from!=state.level||p->src_comp!=state.component)continue;
            route_first=state.first==UINT16_MAX?(uint16_t)i:state.first;
            if(p->to==goal_level&&(!goal_known||p->dst_comp==goal)){
                first=route_first;out->hops=(unsigned)state.hops+1u;
                success=1;goto done;
            }
            for(j=0;j<tail;j++)if(visits[j].level==p->to &&
                                       visits[j].component==p->dst_comp){seen=1;break;}
            if(seen)continue;
            if(tail>=PORTAL_LIMIT+1u)goto done;
            visits[tail++]=(BotVisit){p->dst_comp,route_first,p->to,
                                     (uint8_t)(state.hops+1u)};
        }
    }
done:
    if(success&&first!=UINT16_MAX){
        const BotPortal *p=&portals[first];
        out->from_level=p->from;out->to_level=p->to;
        out->x=p->x;out->y=p->y;out->side=p->side;
        out->landing_x=p->landing_x;out->landing_y=p->landing_y;
        out->kind=(enum AvpMpBotTransitionKind)p->kind;
    }else success=0;
    free(components);free(portals);
    return success;
}
