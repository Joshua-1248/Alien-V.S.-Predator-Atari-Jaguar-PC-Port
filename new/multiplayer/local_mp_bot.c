#include "local_mp_bot.h"
#include "local_mp_input.h"
#include "doors.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Coordinate and wall conventions come from original PLAYER.S/COLLIDE.S.
 * This planner NEVER supplies movement vectors to the game, just buttons. */
#define BOT_PI 3.14159265358979323846
#define BOT_INF UINT32_MAX
#define BOT_WALL_SOLID AVP_WB_SOLID
#define BOT_CELL_BYTES 8u
#define BOT_RISK_NEAR 35u
#define BOT_RISK_ACID 275u
#define BOT_DOOR_PENALTY 24u
#define BOT_PROP_PENALTY 85u
#define BOT_ACID_CONTACT ((double)(30u << 9)/65536.0)
#define BOT_ACID_CLEARANCE (BOT_ACID_CONTACT + 0.016)
/* AMP.S::amp_begin_death applies -50 inside an INCLUSIVE square:
 * abs(dx) <= 96<<9 && abs(dy) <= 96<<9 in original 16.16 units.
 * User-requested bot minimum is 0.76 tile, NOT the earlier 1.0 tile:
 * ceil(0.76 * 65536) = 49808 (the first representable offset at or above 0.76).
 * Compare exact integer deltas to avoid a floating-point boundary slip. */
#define BOT_ALIEN_DEATH_STANDOFF_UNITS 49808u
#define BOT_PLAYER_WALL_MARGIN ((double)(40u << 9)/65536.0)
#define BOT_LANE_MARGIN 0.014
/* MAZESCRN.S melee definitions: Alien tail/swipe/bite and Predator
 * combi-stick/wrist blade ALL have distance 0x90.  fire_wep() converts the
 * original value to 16.16 map units via <<8.  These fractions are PC-only
 * bot positioning margins, not changes to the original weapon range. */
#define BOT_MELEE_REACH ((double)(0x90u << 8)/65536.0)
#define BOT_MELEE_HOLD_NEAR (BOT_MELEE_REACH*0.60)
#define BOT_MELEE_HOLD_FAR (BOT_MELEE_REACH*0.80)
/* MAZESCRN.S defines melee shot width as 20 original pixels for Alien
 * tail/swipe/bite and Predator combi-stick/wrist-blades.  PLAYER.S::TestSpark
 * first checks an axis-aligned shot box expanded by this weapon width, then
 * a perpendicular aim strip. This makes diagonal strikes possible outside
 * a plain circular distance test; it does NOT change the original hitbox. */
#define BOT_MELEE_SHOT_WIDTH ((double)(20u << 9)/65536.0)

/* Species restrictions are source-authored in DOORS.S::DoorKeys and
 * DOORS.S::do_lifts; no new physics or access privileges are granted. */
int avp_mp_bot_transition_allowed(unsigned species,unsigned from_level,
                                   unsigned to_level,
                                   enum AvpMpBotTransitionKind kind)
{
    if(species>2u || from_level<1u || from_level>15u ||
       to_level<1u || to_level>15u || kind>AVP_MP_BOT_TRANS_LIFT)
        return 0;
    if(species==1u && kind==AVP_MP_BOT_TRANS_LIFT)return 0;
    if(species==2u && (kind==AVP_MP_BOT_TRANS_DUCT ||
       (from_level>=6u && from_level<=13u) ||
       (to_level>=6u && to_level<=13u)))return 0;
    return 1;
}

int avp_mp_bot_lift_jammed(unsigned level,unsigned x,unsigned y,int direction)
{
    return x==9u && y==21u &&
           ((level==3u && direction>0) || (level==4u && direction<0));
}

int avp_mp_bot_outside_bounds(uint32_t x,uint32_t y,
                              unsigned width,unsigned height)
{
    return width==0u || height==0u || (int32_t)x<0 || (int32_t)y<0 ||
           (x>>16)>=width || (y>>16)>=height;
}
int avp_mp_bot_playable_position(const uint8_t *cells,unsigned width,
                                unsigned height,uint32_t x,uint32_t y)
{
    unsigned cx,cy;
    if(!cells || !width || !height || width>64u || height>64u ||
       avp_mp_bot_outside_bounds(x,y,width,height))return 0;
    cx=x>>16;cy=y>>16;
    /* Retail maze records have eight bytes per cell; byte five is the
     * authored floor selector. No new geometry, invisible walls or nav map. */
    return cells[((size_t)cy*width+cx)*8u+5u]!=0u;
}
static double angle_diff(uint32_t heading,double dx,double dy)
{
    const double aim=atan2(-dy,dx);
    const double current=(double)(heading>>16)*(2.0*BOT_PI/65536.0);
    double difference=aim-current;
    while(difference>BOT_PI)difference-=2.0*BOT_PI;
    while(difference< -BOT_PI)difference+=2.0*BOT_PI;
    return difference;
}
/* PC bot decision only: approximate the original PLAYER.S::TestSpark
 * finite-distance shot box and perpendicular width for native melee weapons.
 * All real hits still go through the unchanged Jaguar TestSpark routine.
 * A simple Euclidean range of 0x90<<8 rejected valid oblique strikes even
 * when the target was straight along the weapon ray; enemies using the
 * original square contact range could then hit a motion-blocked bot freely.
 * Never issue speculative attacks into the back half-space or off the ray. */
static int bot_melee_source_shot_box(const AvpMpBotObservation *v,
                                     double tx,double ty)
{
    const double a=(double)(v->heading>>16)*(2.0*BOT_PI/65536.0);
    const double fx=cos(a),fy=-sin(a);
    const double side=fabs(tx*fy-ty*fx);
    const double forward=tx*fx+ty*fy;
    return forward>=0.0 &&
           fabs(tx)<fabs(fx*BOT_MELEE_REACH)+BOT_MELEE_SHOT_WIDTH &&
           fabs(ty)<fabs(fy*BOT_MELEE_REACH)+BOT_MELEE_SHOT_WIDTH &&
           /* Source truncates the xoff signed fixed-point word to pixels.
            * A sub-pixel remainder is permitted at the outer width edge. */
           side<BOT_MELEE_SHOT_WIDTH+(1.0/128.0);
}
static int nav_valid(const AvpMpBotMap *m)
{
    return m && m->cells && m->wall_flags && m->width>=1u &&
           m->height>=1u && m->width<=64u && m->height<=64u;
}
static unsigned nav_index(unsigned x,unsigned y){return y*64u+x;}
/* At most one bot sends Use for a particular original door byte in a
 * cooldown window. The source door animation remains authoritative. */
int avp_mp_bot_door_claim(AvpMpBotDoorClaims *claims,
    const uint8_t *cells,uint32_t offset,uint64_t tick)
{
    unsigned choice=AVP_MP_BOT_DOOR_CLAIMS;
    if(!claims||!cells)return 0;
    for(unsigned i=0;i<AVP_MP_BOT_DOOR_CLAIMS;i++){
        AvpMpBotDoorClaim *c=&claims->entries[i];
        if(c->cells==cells && c->offset==offset){
            if(tick<c->until)return 0;
            c->until=tick+30u;return 1;
        }
        if(choice==AVP_MP_BOT_DOOR_CLAIMS && (!c->cells || tick>=c->until))choice=i;
    }
    if(choice==AVP_MP_BOT_DOOR_CLAIMS){
        choice=claims->next_evict++%AVP_MP_BOT_DOOR_CLAIMS;
    }
    claims->entries[choice]=(AvpMpBotDoorClaim){cells,offset,tick+30u};
    return 1;
}
/* Only the original game's active door slots can prove a door is
 * currently moving.  The byte's 0 value is ambiguous immediately after
 * start_normal_or_double() and just before its final native DoorStep().
 * This guard affects bot input only; humans and source door physics are
 * unchanged. */
static int bot_door_animating(const AvpMpBotObservation *v,uint32_t offset)
{
    if(!v)return 0;
    for(unsigned i=0;i<v->active_door_count && i<2u;i++)
        if(v->active_door_offsets[i]==offset)return 1;
    return 0;
}
static unsigned nav_x(unsigned n){return n%64u;}
static unsigned nav_y(unsigned n){return n/64u;}
static const uint8_t *nav_cell(const AvpMpBotMap *m,unsigned x,unsigned y)
{ return m->cells+((size_t)y*m->width+x)*BOT_CELL_BYTES; }

/* The original doors store their opening byte in this cell's LEFT/TOP
 * neighbour boundary: RIGHT uses next cell's LEFT, BOTTOM uses next TOP.
 * Reject inaccessible authored doors unless their actual byte is open.
 * An AVP_ACC_EXIT isn't a normal room passage: handled via exit planning. */
static int nav_edge(const AvpMpBotMap *m,unsigned x,unsigned y,unsigned side,
                    unsigned *nx,unsigned *ny,unsigned *door,unsigned *open)
{
    static const int sx[4]={-1,0,1,0},sy[4]={0,-1,0,1};
    static const unsigned opposite[4]={2,3,0,1};
    const uint8_t *c,*n;
    unsigned f1,f2,offset=0,has_door=0;
    int xx=(int)x+sx[side],yy=(int)y+sy[side];
    if(xx<0||yy<0||xx>=(int)m->width||yy>=(int)m->height)return 0;
    c=nav_cell(m,x,y);n=nav_cell(m,(unsigned)xx,(unsigned)yy);
    /* The original placement grid treats floor byte five==0 as unusable. */
    if(!c[5]||!n[5])return 0;
    f1=m->wall_flags[c[side]];f2=m->wall_flags[n[opposite[side]]];
    if((f1|f2)&BOT_WALL_SOLID)return 0;
    if((f1|f2)&AVP_WB_DOOR){
        const AvpDoorLevelMeta *meta=m->door_meta;
        unsigned i;
        has_door=1;
        if(side==0)offset=((size_t)y*m->width+x)*8u+6u;
        else if(side==1)offset=((size_t)y*m->width+x)*8u+7u;
        else if(side==2)offset=((size_t)yy*m->width+(unsigned)xx)*8u+6u;
        else offset=((size_t)yy*m->width+(unsigned)xx)*8u+7u;
        /* An edge with a broken original door reference is not passable. */
        if(offset>=m->width*m->height*8u)return 0;
        if(meta && m->cells[offset]==0u)for(i=0;i<meta->access_count;i++){
            const AvpDoorAccessEntry *a=&meta->access[i];
            if(a->door_offset!=offset)continue;
            if(a->code<0 || (a->code>0 && (unsigned)a->code>m->access_level))
                return 0;
        }
    }
    *nx=(unsigned)xx;*ny=(unsigned)yy;
    *door=has_door;*open=has_door?m->cells[offset]:255u;
    return 1;
}
/* Eight-neighbour PC-only navigation: a diagonal edge is legal only
 * through the ENTIRE open 2x2 square. Both cardinal routes around the
 * corner must exist, without doors, props or marked acid in that square.
 * This cannot invent a shortcut across solid panels, locked doors, narrow
 * props or an acid lane. Species-immunity is passed in by the caller. */
static int nav_diagonal(const AvpMpBotMap *m,unsigned x,unsigned y,
                        int dx,int dy,int ignore_acid)
{
    unsigned hx,hy,vx,vy,nx,ny,door,open;
    const unsigned horizontal=dx<0?0u:2u,vertical=dy<0?1u:3u;
    const int tx=(int)x+dx,ty=(int)y+dy;
    if((dx!=1 && dx!=-1)||(dy!=1 && dy!=-1) ||
       tx<0||ty<0||tx>=(int)m->width||ty>=(int)m->height)return 0;
    if(!nav_edge(m,x,y,horizontal,&hx,&hy,&door,&open)||door)return 0;
    if(!nav_edge(m,x,y,vertical,&vx,&vy,&door,&open)||door)return 0;
    if(!nav_edge(m,hx,hy,vertical,&nx,&ny,&door,&open)||door ||
       nx!=(unsigned)tx||ny!=(unsigned)ty)return 0;
    if(!nav_edge(m,vx,vy,horizontal,&nx,&ny,&door,&open)||door ||
       nx!=(unsigned)tx||ny!=(unsigned)ty)return 0;
    if(m->risk && !ignore_acid &&
       (m->risk[nav_index(x,y)] || m->risk[nav_index(hx,hy)] ||
        m->risk[nav_index(vx,vy)] ||
        m->risk[nav_index((unsigned)tx,(unsigned)ty)]))return 0;
    if(m->prop_risk &&
       (m->prop_risk[nav_index(x,y)] || m->prop_risk[nav_index(hx,hy)] ||
        m->prop_risk[nav_index(vx,vy)] ||
        m->prop_risk[nav_index((unsigned)tx,(unsigned)ty)]))return 0;
    return 1;
}
unsigned avp_mp_bot_components(const AvpMpBotMap *m,
                                uint16_t out[AVP_MP_BOT_MAP_CELLS])
{
    uint16_t queue[AVP_MP_BOT_MAP_CELLS];
    unsigned labels=0u;
    if(!out)return 0u;
    memset(out,0,AVP_MP_BOT_MAP_CELLS*sizeof *out);
    if(!nav_valid(m))return 0u;
    for(unsigned y=0;y<m->height;y++)for(unsigned x=0;x<m->width;x++){
        unsigned head=0u,tail=0u,origin=nav_index(x,y);
        if(out[origin]||!nav_cell(m,x,y)[5])continue;
        if(labels>=UINT16_MAX-1u)return 0u;
        ++labels;out[origin]=(uint16_t)labels;
        queue[tail++]=(uint16_t)origin;
        while(head<tail){
            const unsigned cell=queue[head++];
            const unsigned cx=nav_x(cell),cy=nav_y(cell);
            for(unsigned side=0;side<4u;side++){
                unsigned nx,ny,door,open,n;
                if(!nav_edge(m,cx,cy,side,&nx,&ny,&door,&open))continue;
                n=nav_index(nx,ny);
                if(out[n])continue;
                out[n]=(uint16_t)labels;
                queue[tail++]=(uint16_t)n;
            }
        }
    }
    return labels;
}

/* A source Alien death creates an actual +/-30<<9 acid contact square.
 * Instead of treating the entire 3x3 neighborhood as unsafe (which falsely
 * rejects narrow Jaguar vents), inspect the original four wall/door edges
 * and sample potential player-center lanes inside the target cell. The
 * player must retain the original 40<<9 wall margin. No safe centre line
 * means an acid splash can block a one-cell passage. All movement remains
 * subject to the original SafePos check. This is a tactical prediction, not
 * a change to the original acid geometry or death spawn. */
int avp_mp_bot_alien_death_passable(const AvpMpBotMap *m,
                                    uint32_t x,uint32_t y)
{
    const unsigned cx=x>>16,cy=y>>16;
    unsigned connected=0u;
    unsigned nx,ny,door,open;
    const double hazard_x=(double)(int32_t)x/65536.0;
    const double hazard_y=(double)(int32_t)y/65536.0;
    const double margin=BOT_PLAYER_WALL_MARGIN+0.001;
    if(!nav_valid(m)||cx>=m->width||cy>=m->height)return 1;
    for(unsigned side=0;side<4u;side++)
        if(nav_edge(m,cx,cy,side,&nx,&ny,&door,&open))
            connected|=1u<<side;
    if(!connected)return 1; /* No traversable exit is threatened. */
    /* A genuinely open neighboring 2x2 square supplies a legal detour
     * around the future puddle. Narrow ducts lack this extra space, so
     * don't demand that the entire 3x3 neighborhood be open. */
    for(int dx=-1;dx<=1;dx+=2)for(int dy=-1;dy<=1;dy+=2)
        if(nav_diagonal(m,cx,cy,dx,dy,1))return 1;
    /* Check each source-legal cardinal passage orientation. A one-way
     * branch is still checked; no artificial 3x3 clear room is required. */
    for(unsigned side=0;side<4u;side++)if(connected&(1u<<side)){
        const int vertical=(side==1u || side==3u);
        /* A through-passage requires both ends when available. A dead-end
         * can instead use its single entry, but still needs a safe lane. */
        unsigned other=vertical?(side==1u?3u:1u):(side==0u?2u:0u);
        if((connected & (1u<<other)) ||
           (connected & ~(1u<<side))==0u){
            for(unsigned n=0;n<=8u;n++){
                const double lateral=(double)(vertical?cx:cy)+
                    margin+(1.0-2.0*margin)*(double)n/8.0;
                const double offset=fabs(lateral-(vertical?hazard_x:hazard_y));
                if(offset>BOT_ACID_CONTACT+0.001)return 1;
            }
        }
    }
    /* For a corner, evaluate a two-stage path within the same cell. It
     * can make the turn only if its two intermediate center points and
     * the corner point are outside the exact acid-contact square. */
    if(connected && !(connected&(connected-1u)))return 0;
    for(unsigned a=0;a<4u;a++)if(connected&(1u<<a))
      for(unsigned b=a+1u;b<4u;b++)if((connected&(1u<<b)) && ((a+b)&1u)!=0u){
        double px=(double)cx+0.5,py=(double)cy+0.5;
        if(a==0u||b==0u)px=(double)cx+margin;
        if(a==2u||b==2u)px=(double)cx+1.0-margin;
        if(a==1u||b==1u)py=(double)cy+margin;
        if(a==3u||b==3u)py=(double)cy+1.0-margin;
        if(fabs(px-hazard_x)>BOT_ACID_CONTACT+0.001 ||
           fabs(py-hazard_y)>BOT_ACID_CONTACT+0.001)return 1;
      }
    return 0;
}
/* Indexed min-heap with decrease-key: no dynamic allocation or unbounded
 * queue, at most 4096 cells. For 64x64 Jaguar maps this is inexpensive at
 * the unchanged 10 Hz simulation rate. */
typedef struct NavHeap {
    uint16_t cell[AVP_MP_BOT_MAP_CELLS];
    uint16_t position[AVP_MP_BOT_MAP_CELLS]; /* 0 absent, otherwise index+1 */
    unsigned count;
    const uint32_t *distance;
} NavHeap;
static void heap_swap(NavHeap *h,unsigned i,unsigned j)
{
    uint16_t a=h->cell[i],b=h->cell[j];
    h->cell[i]=b;h->cell[j]=a;
    h->position[a]=(uint16_t)(j+1u);h->position[b]=(uint16_t)(i+1u);
}
static void heap_offer(NavHeap *h,uint16_t cell)
{
    unsigned k;
    if(h->position[cell])k=h->position[cell]-1u;
    else{
        if(h->count>=AVP_MP_BOT_MAP_CELLS)return;
        k=h->count++;h->cell[k]=cell;h->position[cell]=(uint16_t)(k+1u);
    }
    while(k){unsigned p=(k-1u)/2u;
        if(h->distance[h->cell[p]]<=h->distance[h->cell[k]])break;
        heap_swap(h,k,p);k=p;
    }
}
static uint16_t heap_take(NavHeap *h)
{
    uint16_t result=h->cell[0];
    h->position[result]=0u;
    --h->count;
    if(h->count){
        unsigned k=0;h->cell[0]=h->cell[h->count];h->position[h->cell[0]]=1u;
        for(;;){unsigned l=k*2u+1u,r=l+1u,b=k;
            if(l<h->count&&h->distance[h->cell[l]]<h->distance[h->cell[b]])b=l;
            if(r<h->count&&h->distance[h->cell[r]]<h->distance[h->cell[b]])b=r;
            if(b==k)break;
            heap_swap(h,k,b);
            k=b;
        }
    }
    return result;
}
int avp_mp_bot_route(const AvpMpBotMap *m,unsigned sx,unsigned sy,
                     unsigned gx,unsigned gy,AvpMpBotRoute *out)
{
    uint32_t distance[AVP_MP_BOT_MAP_CELLS];
    uint16_t parent[AVP_MP_BOT_MAP_CELLS];
    uint8_t visited[AVP_MP_BOT_MAP_CELLS];
    NavHeap heap={0};
    unsigned start,goal;
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    if(!nav_valid(m)||sx>=m->width||sy>=m->height||
       gx>=m->width||gy>=m->height)return 0;
    start=nav_index(sx,sy);goal=nav_index(gx,gy);
    if(start==goal){out->valid=1;out->next_x=gx;out->next_y=gy;return 1;}
    for(unsigned n=0;n<AVP_MP_BOT_MAP_CELLS;n++){
        distance[n]=BOT_INF;parent[n]=UINT16_MAX;visited[n]=0;
    }
    heap.distance=distance;
    distance[start]=0;heap_offer(&heap,(uint16_t)start);
    while(heap.count){
        unsigned u=heap_take(&heap),x=nav_x(u),y=nav_y(u);
        if(visited[u])continue;
        visited[u]=1;
        if(u==goal)break;
        for(unsigned side=0;side<4u;side++){
            unsigned nx,ny,door,open,v,penalty=10u;
            if(!nav_edge(m,x,y,side,&nx,&ny,&door,&open))continue;
            v=nav_index(nx,ny);
            if(visited[v])continue;
            if(door)penalty+=BOT_DOOR_PENALTY;
            if(m->risk){
                if(m->risk[v]==1u)penalty+=BOT_RISK_NEAR;
                else if(m->risk[v]>=2u)penalty+=BOT_RISK_ACID;
            }
            /* Source AMP props are costly, never fictional solid walls:
             * some have enough floor clearance to walk past on one side. */
            if(m->prop_risk && m->prop_risk[v])
                penalty+=BOT_PROP_PENALTY;
            if(distance[u]!=BOT_INF && distance[u]+penalty<distance[v]){
                distance[v]=distance[u]+penalty;parent[v]=(uint16_t)u;
                heap_offer(&heap,(uint16_t)v);
            }
        }
        /* True diagonal path edges, not merely diagonal steering toward a
         * cardinal waypoint. sqrt(2) grid cost ~= 14 vs cardinal 10. */
        for(int dy=-1;dy<=1;dy+=2)for(int dx=-1;dx<=1;dx+=2){
            unsigned nx,ny,v,penalty=14u;
            if(!nav_diagonal(m,x,y,dx,dy,0))continue;
            nx=(unsigned)((int)x+dx);ny=(unsigned)((int)y+dy);
            v=nav_index(nx,ny);
            if(visited[v])continue;
            if(distance[u]!=BOT_INF && distance[u]+penalty<distance[v]){
                distance[v]=distance[u]+penalty;parent[v]=(uint16_t)u;
                heap_offer(&heap,(uint16_t)v);
            }
        }
    }
    if(distance[goal]==BOT_INF)return 0;
    {
        unsigned next=goal,prev,second=UINT16_MAX;
        for(unsigned steps=0;steps<AVP_MP_BOT_MAP_CELLS;steps++){
            prev=parent[next];
            if(prev==start)break;
            if(prev==UINT16_MAX)return 0;
            second=next;next=prev;
        }
        if(parent[next]!=start)return 0;
        out->valid=1;out->next_x=nav_x(next);out->next_y=nav_y(next);
        if(second!=UINT16_MAX){
            out->lookahead_valid=1u;
            out->lookahead_x=nav_x(second);
            out->lookahead_y=nav_y(second);
        }
        out->cost=distance[goal];
        /* Diagonal edges cross no doors and are already verified open. */
        out->door_is_open=1u;
        for(unsigned dir=0;dir<4u;dir++){
            unsigned nx,ny,door,open;
            if(nav_edge(m,sx,sy,dir,&nx,&ny,&door,&open)&&
               nx==out->next_x && ny==out->next_y){
                out->requires_door_use=door&&open<40u;
                out->door_is_open=!door||open>=40u;
                out->door_value=open;
                if(door){
                    if(dir==0u)out->door_offset=(sy*m->width+sx)*8u+6u;
                    else if(dir==1u)out->door_offset=(sy*m->width+sx)*8u+7u;
                    else if(dir==2u)out->door_offset=(ny*m->width+nx)*8u+6u;
                    else out->door_offset=(ny*m->width+nx)*8u+7u;
                }
                break;
            }
        }
    }
    return 1;
}
/* A Predator cannot pursue a teammate into any of the duct worlds
 * (levels 6..13). Do not hold onto a stale follow goal or keep pressing a
 * vent panel: drop this objective and select other work until the teammate
 * returns to a reachable area. If the teammate is in the same world but
 * behind an untraversable boundary, don't stand facing the wall either.
 * Real level-to-level connectivity is verified by walk_bot_choose_level_route.
 * No permanent blacklist: this decision is evaluated anew each bot tick. */
int avp_mp_bot_follow_target_possible(unsigned species,unsigned from_level,
        unsigned teammate_level,const AvpMpBotMap *map,
        unsigned from_x,unsigned from_y,unsigned teammate_x,unsigned teammate_y)
{
    AvpMpBotRoute route;
    if(species>2u || from_level<1u || from_level>15u ||
       teammate_level<1u || teammate_level>15u)return 0;
    if(from_level!=teammate_level)
        return avp_mp_bot_transition_allowed(species,from_level,
                             teammate_level,AVP_MP_BOT_TRANS_EXIT);
    if(nav_valid(map))
        return avp_mp_bot_route(map,from_x,from_y,
                                teammate_x,teammate_y,&route);
    return 1; /* synthetic/diagnostic view with no authenticated map */
}

/* The original AMPCollisions tests axis-aligned x/y halfwidth squares,
 * not circles. Read-only samples keep player movement authoritative; the
 * bot simply refuses directions leading into a registered solid AMP. */
static int bot_prop_hit(const AvpMpBotObservation *v,double x,double y)
{
    for(unsigned i=0;i<v->prop_count;i++){
        const AvpMpBotProp *p=&v->props[i];
        double px=(double)p->x/65536.0,py=(double)p->y/65536.0;
        double r=(double)p->halfwidth/65536.0;
        if(fabs(px-x)<r && fabs(py-y)<r)return 1;
    }
    return 0;
}
static int bot_prop_motion_clear(const AvpMpBotObservation *v,double ax,double ay,
                                  double bx,double by)
{
    int starting=bot_prop_hit(v,ax,ay);
    /* Test multiple points, but allow an actor already overlapping a
     * prop to escape instead of freezing inside a collision square. */
    for(unsigned i=1;i<=6u;i++){
        double x=ax+(bx-ax)*(double)i/6.0,y=ay+(by-ay)*(double)i/6.0;
        if(bot_prop_hit(v,x,y)){
            if(!starting)return 0;
            if(i==6u)return 0;
        }
    }
    return 1;
}
/* Precise original acid contact square, sampled along a prospective command.
 * Whole maze tiles remain a coarse path COST, not the actual damage hitbox. */
static unsigned bot_acid_hits(const AvpMpBotObservation *v,double ax,double ay,
                              double bx,double by)
{
    unsigned hits=0;
    for(unsigned j=0;j<=6u;j++){
        double x=ax+(bx-ax)*(double)j/6.0,y=ay+(by-ay)*(double)j/6.0;
        for(unsigned i=0;i<v->acid_count;i++){
            double px=(double)v->acid[i].x/65536.0;
            double py=(double)v->acid[i].y/65536.0;
            if(fabs(x-px)<=BOT_ACID_CLEARANCE &&
               fabs(y-py)<=BOT_ACID_CLEARANCE){++hits;break;}
        }
    }
    return hits;
}
/* Original AMP.S amp_in_range(30) tests |dx|<=30<<9 AND |dy|<=30<<9.
 * Original PLAYER.S uses SafePos(80, ..., 40), whose wall margin is
 * 40<<9 on the same axes. A 1x1 cell is not a solid 1x1 actor footprint.
 * Distinguish the read-only *damage* footprint from the wall clearance.
 * The renderer's acid graphic is not the damage geometry. */
void avp_mp_bot_acid_risk_mark(uint8_t risk[AVP_MP_BOT_MAP_CELLS],
                                uint32_t px,uint32_t py)
{
    const int64_t r=(int64_t)(30u<<9);
    const int64_t x0=(int64_t)(int32_t)px-r,x1=(int64_t)(int32_t)px+r;
    const int64_t y0=(int64_t)(int32_t)py-r,y1=(int64_t)(int32_t)py+r;
    /* A contact square narrower than one cell can intersect no more than
     * two cells on either axis. No arbitrary 3x3 hazard inflation. */
    if(!risk || x1<0 || y1<0 || x0>=64LL*65536 || y0>=64LL*65536)return;
    for(unsigned cy=(unsigned)((y0<0?0:y0)>>16);
        cy<=(unsigned)((y1>=64LL*65536?64LL*65536-1:y1)>>16);cy++)
    for(unsigned cx=(unsigned)((x0<0?0:x0)>>16);
        cx<=(unsigned)((x1>=64LL*65536?64LL*65536-1:x1)>>16);cx++){
        const int64_t mx=((int64_t)cx<<16)+32768;
        const int64_t my=((int64_t)cy<<16)+32768;
        const unsigned strength=(llabs(mx-(int64_t)(int32_t)px)<=r &&
                                  llabs(my-(int64_t)(int32_t)py)<=r)?2u:1u;
        const unsigned id=nav_index(cx,cy);
        if(risk[id]<strength)risk[id]=(uint8_t)strength;
    }
}

/* Read-only local counterpart to the original SafePos width=40 wall test.
 * Check the entire proposed lane, not just the cell CENTER. An open area
 * can safely use the 0.18 / 0.82 fringe; a wall-bounded corridor cannot.
 * MovePlayer/SafePos remain fully authoritative on executed commands. */
static int bot_lane_wall_clear(const AvpMpBotMap *m,double x,double y)
{
    unsigned sx,sy,nx,ny,door,open;
    double fx,fy;
    if(!nav_valid(m))return 1;
    if(x<0.0||y<0.0||x>=(double)m->width||y>=(double)m->height)return 0;
    sx=(unsigned)x;sy=(unsigned)y;fx=x-(double)sx;fy=y-(double)sy;
    if(fx<BOT_PLAYER_WALL_MARGIN &&
       (!nav_edge(m,sx,sy,0u,&nx,&ny,&door,&open)||(door&&open<40u)))return 0;
    if(fx>1.0-BOT_PLAYER_WALL_MARGIN &&
       (!nav_edge(m,sx,sy,2u,&nx,&ny,&door,&open)||(door&&open<40u)))return 0;
    if(fy<BOT_PLAYER_WALL_MARGIN &&
       (!nav_edge(m,sx,sy,1u,&nx,&ny,&door,&open)||(door&&open<40u)))return 0;
    if(fy>1.0-BOT_PLAYER_WALL_MARGIN &&
       (!nav_edge(m,sx,sy,3u,&nx,&ny,&door,&open)||(door&&open<40u)))return 0;
    return 1;
}
static int bot_lane_segment_clear(const AvpMpBotObservation *v,
                                  double ax,double ay,double bx,double by)
{
    for(unsigned n=0;n<=12u;n++){
        const double t=(double)n/12.0;
        const double x=ax+(bx-ax)*t,y=ay+(by-ay)*t;
        if(!bot_lane_wall_clear(&v->map,x,y) || bot_prop_hit(v,x,y))return 0;
    }
    return 1;
}
/* Plan a TWO-LEG local line: shift into a safe lane in the current cell,
 * then advance to the next route cell. Search precise damage boundaries in
 * addition to old 0.18/0.82 waypoints. This uses original actor centers,
 * collision margin, source acid contact and real wall/prop geometry. */
static unsigned bot_best_acid_lane(const AvpMpBotObservation *v,
                                   const AvpMpBotRoute *route,
                                   double *selected)
{
    const double ax=(double)(int32_t)v->x/65536.0;
    const double ay=(double)(int32_t)v->y/65536.0;
    const int along_x=route->next_x!=(v->x>>16);
    const double longitudinal=along_x?(double)route->next_x+0.5:
                                      (double)route->next_y+0.5;
    const double current=along_x?ay:ax;
    const double floorcell=floor(current);
    double best_lane=current;
    unsigned best_hits=~0u;
    double best_cost=1e30;
    /* Local candidates start with center/current lane, then approach both
     * sides of EACH relevant exact acid square. Only candidates on this
     * floor cell and physically reachable at original player width count. */
    for(unsigned n=0;n<4u+v->acid_count*2u;n++){
        double lane;
        double bx,by,cx,cy;
        unsigned hits;
        double cost;
        if(n==0u)lane=current;
        else if(n==1u)lane=floorcell+0.18;
        else if(n==2u)lane=floorcell+0.82;
        else if(n==3u)lane=floorcell+0.50;
        else{
            const unsigned id=(n-4u)/2u;
            const double px=(double)(int32_t)v->acid[id].x/65536.0;
            const double py=(double)(int32_t)v->acid[id].y/65536.0;
            const double near=along_x?px:py;
            if(fabs(near-longitudinal)>1.70 &&
               fabs(near-(along_x?ax:ay))>1.70)continue;
            lane=(along_x?py:px)+((n&1u)?1.0:-1.0)*
                    (BOT_ACID_CLEARANCE+BOT_LANE_MARGIN);
        }
        if(lane<floorcell+0.035 || lane>floorcell+0.965)continue;
        bx=along_x?ax:lane;by=along_x?lane:ay;
        cx=along_x?longitudinal:lane;
        cy=along_x?lane:longitudinal;
        if(!bot_lane_segment_clear(v,ax,ay,bx,by) ||
           !bot_lane_segment_clear(v,bx,by,cx,cy))continue;
        hits=bot_acid_hits(v,ax,ay,bx,by)+bot_acid_hits(v,bx,by,cx,cy);
        cost=fabs(lane-current);
        if(hits<best_hits || (hits==best_hits && cost<best_cost)){
            best_hits=hits;best_cost=cost;best_lane=lane;
        }
    }
    *selected=best_lane;
    return best_hits;
}
/* The planner's acid is a COST, not a physical obstacle. Commit through
 * unavoidable contact. A reachable lane with less exposure takes priority
 * over forward-tanking even when the route uses an acid-center grid cell. */
static int bot_route_acid_crossing(const AvpMpBotObservation *v,
                                    const AvpMpBotRoute *route)
{
    const AvpMpBotMap *m=&v->map;
    const unsigned sx=v->x>>16,sy=v->y>>16;
    double lane;
    unsigned best,current;
    if(!route || !route->valid || v->player_species==1u ||
       !nav_valid(m) || !v->acid_count ||
       sx>=m->width || sy>=m->height ||
       route->next_x>=m->width || route->next_y>=m->height)return 1;
    if(route->next_x!=sx && route->next_y!=sy)return 1;
    best=bot_best_acid_lane(v,route,&lane);
    current=bot_acid_hits(v,(double)(int32_t)v->x/65536.0,
         (double)(int32_t)v->y/65536.0,
         (double)route->next_x+0.5,(double)route->next_y+0.5);
    return best>=current;
}
static void bot_acid_waypoint(const AvpMpBotObservation *v,
                              const AvpMpBotRoute *route,double *wx,double *wy)
{
    const double ax=(double)(int32_t)v->x/65536.0;
    const double ay=(double)(int32_t)v->y/65536.0;
    const int along_x=route->next_x!=(v->x>>16);
    double lane;
    if(v->player_species==1u || !v->acid_count ||
       (route->next_x!=(v->x>>16) && route->next_y!=(v->y>>16)))return;
    if(bot_best_acid_lane(v,route,&lane)==~0u)return;
    if(fabs(lane-(along_x?ay:ax))>0.055){
        *wx=along_x?ax:lane;
        *wy=along_x?lane:ay;
    }else{
        *wx=along_x?(double)route->next_x+0.5:lane;
        *wy=along_x?lane:(double)route->next_y+0.5;
    }
}
/* If a prop blocks the central approach, steer to the free side of the
 * same cell before continuing. Choose stable side based on current lane;
 * this prevents reversing left/right every tick around a table/barrel. */
static void bot_prop_waypoint(const AvpMpBotObservation *v,
                              const AvpMpBotRoute *route,double *wx,double *wy)
{
    const double ax=(double)v->x/65536.0,ay=(double)v->y/65536.0;
    const int along_x=route->next_x!=(v->x>>16);
    const int advance=along_x ? (route->next_x>(v->x>>16)?1:-1):
                                (route->next_y>(v->y>>16)?1:-1);
    if(route->next_x!=(v->x>>16) && route->next_y!=(v->y>>16))return;
    for(unsigned i=0;i<v->prop_count;i++){
        const AvpMpBotProp *p=&v->props[i];
        double px=(double)p->x/65536.0,py=(double)p->y/65536.0;
        double pr=(double)p->halfwidth/65536.0;
        double ahead=along_x?advance*(px-ax):advance*(py-ay);
        double here=along_x?ay:ax,side=along_x?py:px;
        double candidates[2],best=1e30,selected=here;
        double cell=(double)(int)floor(side);
        if(ahead< -0.12 || ahead>1.2 || fabs(side-here)>pr+0.05)continue;
        candidates[0]=side-pr-0.03;
        candidates[1]=side+pr+0.03;
        for(unsigned j=0;j<2u;j++){
            double lane=candidates[j];
            double sx=along_x?ax:lane,sy=along_x?lane:ay;
            double score;
            /* Keep the candidate on the current floor cell. We do not
             * assume wall/actor widths; original MovePlayer enforces both. */
            if(lane<cell+0.08 || lane>cell+0.92)continue;
            if(!bot_prop_motion_clear(v,ax,ay,sx,sy))continue;
            score=fabs(lane-here);
            score+=80.0*(double)bot_acid_hits(v,ax,ay,sx,sy);
            if(score<best){best=score;selected=lane;}
        }
        if(best==1e30)continue;
        if(fabs(selected-here)>0.04){
            *wx=along_x?ax:selected;
            *wy=along_x?selected:ay;
        }else{
            *wx=along_x?(double)route->next_x+0.5:selected;
            *wy=along_x?selected:(double)route->next_y+0.5;
        }
        return;
    }
}
static int bot_props_close(const AvpMpBotObservation *v)
{
    const double x=(double)(int32_t)v->x/65536.0;
    const double y=(double)(int32_t)v->y/65536.0;
    for(unsigned i=0;i<v->prop_count;i++){
        const AvpMpBotProp *p=&v->props[i];
        const double margin=(double)p->halfwidth/65536.0+1.10;
        if(fabs((double)p->x/65536.0-x)<margin &&
           fabs((double)p->y/65536.0-y)<margin)return 1;
    }
    return 0;
}
static int bot_hazards_close(const AvpMpBotObservation *v)
{
    const double x=(double)(int32_t)v->x/65536.0;
    const double y=(double)(int32_t)v->y/65536.0;
    for(unsigned i=0;i<v->acid_count;i++)
        if(fabs((double)v->acid[i].x/65536.0-x)<1.25 &&
           fabs((double)v->acid[i].y/65536.0-y)<1.25)return 1;
    return bot_props_close(v);
}
/* Test a short potential movement against original wall/door faces before
 * side-stepping or retreating. This is a COMMAND filter only: real player
 * collision, actor width and acid contact remain the Jaguar engine's job. */
static int bot_step_clear(const AvpMpBotObservation *v,double dx,double dy,
                          const AvpMpBotRoute *route)
{
    unsigned sx,sy,gx,gy,door,open,tx,ty;
    double x,y;
    const AvpMpBotMap *m=&v->map;
    if(!nav_valid(m))return 1; /* diagnostic map has no retail geometry */
    sx=v->x>>16;sy=v->y>>16;
    if(sx>=m->width||sy>=m->height)return 0;
    /* A short step is sufficient for exact local hazard steering. The
     * original 0.68 conservative wall probe remains for coarser maps. */
    x=(double)v->x/65536.0+dx*(bot_hazards_close(v) && route ? 0.28 : 0.68);
    y=(double)v->y/65536.0+dy*(bot_hazards_close(v) && route ? 0.28 : 0.68);
    if(x<0.0||y<0.0||x>=(double)m->width||y>=(double)m->height)return 0;
    if(v->prop_count && !bot_prop_motion_clear(v,(double)v->x/65536.0,
             (double)v->y/65536.0,x,y))return 0;
    gx=(unsigned)x;gy=(unsigned)y;
    /* Never stray from the chosen path. With a genuine diagonal hop,
     * physical movement may enter one of its two side cells FIRST before
     * crossing the other axis. Accept those ONLY when the entire square is
     * verified open, then let the original MovePlayer/SafePos decide. */
    if(route && route->valid && (gx!=sx || gy!=sy) &&
       (gx!=route->next_x || gy!=route->next_y)){
        const int rx=(int)route->next_x-(int)sx;
        const int ry=(int)route->next_y-(int)sy;
        if((rx!=1 && rx!=-1)||(ry!=1 && ry!=-1) ||
           !nav_diagonal(m,sx,sy,rx,ry,v->player_species==1u) ||
           !((gx==route->next_x && gy==sy) ||
             (gx==sx && gy==route->next_y)))return 0;
    }
    /* The route planner already prices acid according to the entire map.
     * Once a safe/best-available route has been chosen, do not veto its
     * actual commands merely because a puddle's contact box spills into
     * an adjacent cell. That disagreement left bots standing in acid, or
     * hesitating outside it, forever.  Walls, doors and solid props remain
     * checked above and below, and native MovePlayer still applies damage. */
    if(v->player_species!=1u && !(route && route->valid)){
        if(v->acid_count){
            unsigned before=bot_acid_hits(v,(double)v->x/65536.0,
                                           (double)v->y/65536.0,
                                           (double)v->x/65536.0,
                                           (double)v->y/65536.0);
            unsigned exposure=bot_acid_hits(v,(double)v->x/65536.0,
                                             (double)v->y/65536.0,x,y);
            if(exposure>before)return 0;
        }else if(m->risk && m->risk[nav_index(gx,gy)]>=2u &&
                 m->risk[nav_index(sx,sy)]<2u)return 0;
    }
    if(gx!=sx){
        if(!nav_edge(m,sx,sy,gx<sx?0u:2u,&tx,&ty,&door,&open) ||
           (door&&open<40u))return 0;
    }
    if(gy!=sy){
        if(!nav_edge(m,sx,sy,gy<sy?1u:3u,&tx,&ty,&door,&open) ||
           (door&&open<40u))return 0;
    }
    /* Diagonal corner traversals are rejected unless both adjacent
     * cells also expose the opposite half of the crossing. */
    if(gx!=sx && gy!=sy){
        if(!nav_edge(m,gx,sy,gy<sy?1u:3u,&tx,&ty,&door,&open) ||
           (door&&open<40u))return 0;
        if(!nav_edge(m,sx,gy,gx<sx?0u:2u,&tx,&ty,&door,&open) ||
           (door&&open<40u))return 0;
    }
    return 1;
}
/* Aim slightly beyond the immediate source-legal next node when TWO
 * consecutive cardinal steps form a safe open lane. Diagonal hops already
 * have a direct heading, so this advance bend helper leaves them alone.
 * Do not shortcut doors, acid lanes, prop avoidance or a closed corner. */
static void bot_route_steer_point(const AvpMpBotObservation *v,
                                  const AvpMpBotRoute *route,
                                  double *wx,double *wy)
{
    const AvpMpBotMap *m=&v->map;
    unsigned sx=v->x>>16,sy=v->y>>16,nx,ny,door,open;
    int step1x,step1y,step2x,step2y;
    double candidate_x,candidate_y;
    const double ax=(double)(int32_t)v->x/65536.0;
    const double ay=(double)(int32_t)v->y/65536.0;
    *wx=(double)route->next_x+0.5;
    *wy=(double)route->next_y+0.5;
    if((route->next_x!=sx && route->next_y!=sy) ||
       !route->lookahead_valid || route->requires_door_use ||
       !route->door_is_open || bot_hazards_close(v) || !nav_valid(m))return;
    step1x=(int)route->next_x-(int)sx;
    step1y=(int)route->next_y-(int)sy;
    step2x=(int)route->lookahead_x-(int)route->next_x;
    step2y=(int)route->lookahead_y-(int)route->next_y;
    if(abs(step1x)+abs(step1y)!=1 ||
       abs(step2x)+abs(step2y)!=1)return;
    if(m->risk &&
       (m->risk[nav_index(route->next_x,route->next_y)] ||
        m->risk[nav_index(route->lookahead_x,route->lookahead_y)]))return;
    if(m->prop_risk &&
       (m->prop_risk[nav_index(route->next_x,route->next_y)] ||
        m->prop_risk[nav_index(route->lookahead_x,route->lookahead_y)]))return;
    /* Verify the next A* edge as an already-open original passage. */
    nx=ny=UINT32_MAX;
    for(unsigned side=0;side<4u;side++){
        if(!nav_edge(m,route->next_x,route->next_y,side,
                     &nx,&ny,&door,&open))continue;
        if(nx==route->lookahead_x && ny==route->lookahead_y)break;
    }
    if(nx!=route->lookahead_x || ny!=route->lookahead_y ||
       (door && open<40u))return;
    if(step1x!=step2x || step1y!=step2y){
        /* A bend can be rounded only when BOTH sides of the open 2x2
         * corner are traversable. Otherwise face the first node normally. */
        unsigned cx,cy,dir;
        if(step2x==0 && step2y==0)return;
        dir=step2x<0?0u:step2x>0?2u:step2y<0?1u:3u;
        if(!nav_edge(m,sx,sy,dir,&cx,&cy,&door,&open) ||
           (door && open<40u))return;
        if(m->risk && m->risk[nav_index(cx,cy)])return;
        if(m->prop_risk && m->prop_risk[nav_index(cx,cy)])return;
        dir=step1x<0?0u:step1x>0?2u:step1y<0?1u:3u;
        if(!nav_edge(m,cx,cy,dir,&nx,&ny,&door,&open) ||
           nx!=route->lookahead_x || ny!=route->lookahead_y ||
           (door && open<40u))return;
    }
    /* Keep the aim near the first path cell, not at a remote goal. The
     * line test checks the original wall margin and solid prop bounds. */
    candidate_x=*wx+0.45*(step2x);
    candidate_y=*wy+0.45*(step2y);
    if(!bot_lane_segment_clear(v,ax,ay,candidate_x,candidate_y))return;
    *wx=candidate_x;*wy=candidate_y;
}

/* A stable open corridor is a DIRECTION, not a succession of cell-center
 * targets. A centered bot should hold its heading instead of alternating
 * left and right as the next waypoint passes from one side of its nose to
 * the other. Route motion can still strafe toward the lane center, and real
 * bends/obstacles keep the original precise waypoint steering. */
static int bot_straight_route_heading(const AvpMpBotObservation *v,
                                      const AvpMpBotRoute *route,
                                      double *dx,double *dy)
{
    const AvpMpBotMap *m=&v->map;
    const unsigned sx=v->x>>16,sy=v->y>>16;
    const int step_x=(int)route->next_x-(int)sx;
    const int step_y=(int)route->next_y-(int)sy;
    const int look_x=(int)route->lookahead_x-(int)route->next_x;
    const int look_y=(int)route->lookahead_y-(int)route->next_y;
    const double px=(double)(int32_t)v->x/65536.0;
    const double py=(double)(int32_t)v->y/65536.0;
    double lateral;
    if(!route->valid || route->requires_door_use || !route->door_is_open ||
       bot_hazards_close(v) || !nav_valid(m) ||
       sx>=m->width || sy>=m->height ||
       route->next_x>=m->width || route->next_y>=m->height ||
       (step_x==0 && step_y==0) ||
       abs(step_x)>1 || abs(step_y)>1)return 0;
    /* Don't lock the heading across a bend. When a straight segment ends
     * at the destination, preserve its stable course through the last cell. */
    if(route->lookahead_valid &&
       (route->lookahead_x>=m->width || route->lookahead_y>=m->height ||
        step_x!=look_x || step_y!=look_y))return 0;
    if(m->risk &&
       (m->risk[nav_index(route->next_x,route->next_y)] ||
        (route->lookahead_valid &&
         m->risk[nav_index(route->lookahead_x,route->lookahead_y)])))return 0;
    if(m->prop_risk &&
       (m->prop_risk[nav_index(route->next_x,route->next_y)] ||
        (route->lookahead_valid &&
         m->prop_risk[nav_index(route->lookahead_x,route->lookahead_y)])))return 0;
    /* Sideways displacement from the segment's centerline, in map tiles.
     * At larger offsets, aim toward the actual waypoint to recenter. */
    lateral=fabs((px-((double)sx+0.5))*(double)step_y-
                 (py-((double)sy+0.5))*(double)step_x);
    if(step_x && step_y)lateral*=0.70710678118654752440;
    if(lateral>0.28)return 0;
    *dx=(double)step_x;
    *dy=(double)step_y;
    return 1;
}

/* Heading zero faces +X. Left/right are the original host sidestep axes,
 * NOT a fabricated world displacement or a new speed multiplier. */
static int bot_safe_action(const AvpMpBotObservation *v,unsigned action,
                           const AvpMpBotRoute *route)
{
    double a=(double)(v->heading>>16)*(2.0*BOT_PI/65536.0);
    double fx=cos(a),fy=-sin(a),dx=0,dy=0;
    switch(action){
    case AVP_MP_FORWARD:dx=fx;dy=fy;break;
    case AVP_MP_BACKWARD:dx=-fx;dy=-fy;break;
    case AVP_MP_STRAFE_LEFT:dx=fy;dy=-fx;break;
    case AVP_MP_STRAFE_RIGHT:dx=-fy;dy=fx;break;
    default:return 0;
    }
    return bot_step_clear(v,dx,dy,route);
}
/* Choose an existing player motion key toward the next cardinal or
 * diagonal waypoint. A fixed movement-angle threshold can reject every
 * legal action during oblique acid navigation; instead check source geometry.
 * Original player physics still owns speed, strafe and collision. */
static unsigned bot_route_motion(const AvpMpBotObservation *v,
                                 const AvpMpBotRoute *route,
                                 int commit_crossing)
{
    static const unsigned actions[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                                      AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
    double a=(double)(v->heading>>16)*(2.0*BOT_PI/65536.0);
    double fx=cos(a),fy=-sin(a);
    double wx=(double)route->next_x+0.5,wy=(double)route->next_y+0.5;
    double vx,vy;
    double best=0.05;
    /* After an observed acid-lane stall, take the known passable cardinal
     * crossing rather than choosing the same ineffective sidestep again. */
    if(!commit_crossing)bot_route_steer_point(v,route,&wx,&wy);
    if(!commit_crossing && !bot_route_acid_crossing(v,route))
        bot_acid_waypoint(v,route,&wx,&wy);
    bot_prop_waypoint(v,route,&wx,&wy);
    vx=wx-(double)v->x/65536.0;
    vy=wy-(double)v->y/65536.0;
    unsigned choice=AVP_MP_INPUT_ACTIONS;
    for(unsigned n=0;n<4u;n++){
        double dx=n==0u?fx:n==1u?-fx:n==2u?fy:-fy;
        double dy=n==0u?fy:n==1u?-fy:n==2u?-fx:fx;
        double score=vx*dx+vy*dy;
        /* The lane waypoint was already evaluated against every nearby
         * acid square, and Dijkstra chose the level-wide least-cost route.
         * A SECOND, per-command acid penalty makes forward progress lose
         * against doing nothing at precisely the chokepoints we must cross.
         * Do not re-price these source-authorized route commands. */
        if(score>best && bot_safe_action(v,actions[n],route)){
            best=score;choice=actions[n];
        }
    }
    if(choice==AVP_MP_INPUT_ACTIONS && v->player_species!=1u && v->acid_count){
        /* If every positive route motion touches acid, take the least
         * hazardous legal path action instead of freezing indefinitely. */
        double worst=-1e30;
        for(unsigned n=0;n<4u;n++){
            double dx=n==0u?fx:n==1u?-fx:n==2u?fy:-fy;
            double dy=n==0u?fy:n==1u?-fy:n==2u?-fx:fx;
            double dot=vx*dx+vy*dy;
            if(dot<=0.05 || !bot_safe_action(v,actions[n],route))continue;
            double risk=(double)bot_acid_hits(v,(double)v->x/65536.0,
                        (double)v->y/65536.0,
                        (double)v->x/65536.0+dx*0.28,
                        (double)v->y/65536.0+dy*0.28);
            double score=dot-risk*100.0;
            if(score>worst){worst=score;choice=actions[n];}
        }
    }
    return choice;
}
/* A bot ALREADY standing in acid must attempt to leave, even if its
 * current combat tactic would otherwise hold position. If there is a route,
 * follow its next waypoint using ordinary player commands. Otherwise take
 * the first locally navigable motion which reduces actual source acid
 * contact. Moving through unavoidable acid is preferable to waiting for
 * death; this helper never changes collision or damage handling. */
static unsigned bot_escape_current_acid(const AvpMpBotObservation *v,
                                         const AvpMpBotRoute *route)
{
    static const unsigned actions[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                                      AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
    double a,fx,fy,base_x,base_y,origin_contact;
    double best=-1e30;
    unsigned chosen=AVP_MP_INPUT_ACTIONS;
    if(v->player_species==1u||!v->acid_count)return chosen;
    base_x=(double)(int32_t)v->x/65536.0;
    base_y=(double)(int32_t)v->y/65536.0;
    origin_contact=(double)bot_acid_hits(v,base_x,base_y,base_x,base_y);
    if(origin_contact==0.0)return chosen;
    if(route&&route->valid &&
       (route->next_x!=(v->x>>16)||route->next_y!=(v->y>>16))){
        chosen=bot_route_motion(v,route,bot_route_acid_crossing(v,route));
        if(chosen<AVP_MP_INPUT_ACTIONS)return chosen;
    }
    a=(double)(v->heading>>16)*(2.0*BOT_PI/65536.0);
    fx=cos(a);fy=-sin(a);
    for(unsigned n=0;n<4u;n++){
        double dx=n==0u?fx:n==1u?-fx:n==2u?fy:-fy;
        double dy=n==0u?fy:n==1u?-fy:n==2u?-fx:fx;
        double x=base_x+dx*0.28,y=base_y+dy*0.28;
        double contact=(double)bot_acid_hits(v,x,y,x,y);
        double score=origin_contact-contact;
        /* The movement geometry check ignores the acid veto only; real
         * walls, doors, props and the Jaguar collision all still apply. */
        AvpMpBotObservation test=*v;
        test.acid_count=0u;
        test.map.risk=NULL;
        if(!bot_safe_action(&test,actions[n],NULL))continue;
        if(score>best){best=score;chosen=actions[n];}
    }
    return chosen;
}
/* This is a behavior decision, never a movement vector fed to the engine.
 * Stay facing the opponent while choosing the ordinary BACKWARD or strafe
 * button whose world direction moves away.  The original MovePlayer,
 * SafePos and AMP collision functions still process every command. */
static unsigned bot_melee_retreat(const AvpMpBotObservation *v,double tx,double ty)
{
    static const unsigned actions[4]={AVP_MP_BACKWARD,AVP_MP_STRAFE_LEFT,
                                       AVP_MP_STRAFE_RIGHT,AVP_MP_FORWARD};
    double a=(double)(v->heading>>16)*(2.0*BOT_PI/65536.0);
    double fx=cos(a),fy=-sin(a),best=-1.0;
    unsigned choice=AVP_MP_INPUT_ACTIONS;
    if(tx*tx+ty*ty<0.000001 && bot_safe_action(v,AVP_MP_BACKWARD,NULL))
        return AVP_MP_BACKWARD;
    for(unsigned n=0;n<4u;n++){
        double vx=n==0u?-fx:n==1u?fy:n==2u?-fy:fx;
        double vy=n==0u?-fy:n==1u?-fx:n==2u?fx:fy;
        double away=-(tx*vx+ty*vy);
        if(away>best && away>0.0 && bot_safe_action(v,actions[n],NULL)){
            best=away;choice=actions[n];
        }
    }
    /* If the only actual retreat direction is blocked (for example by
     * acid), sidestep along the opponent instead of freezing or advancing
     * further into their contact range. */
    if(choice==AVP_MP_INPUT_ACTIONS){
        unsigned side=((v->tick/12u+v->player_index)&1u)?
            AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
        if(bot_safe_action(v,side,NULL))return side;
        side=side==AVP_MP_STRAFE_LEFT?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
        if(bot_safe_action(v,side,NULL))return side;
    }
    return choice;
}
/* Blend an original forward/backward command with one original sidestep
   command while following the same route waypoint (cardinal or diagonal). This avoids the
   visual stop-turn-stop cadence at diagonal headings. The combined move
   must clear the SAME original wall/door/prop checks; native MovePlayer
   still owns the actual velocity and collision resolution. */
static unsigned bot_route_second_motion(const AvpMpBotObservation *v,
                                         const AvpMpBotRoute *route,
                                         unsigned primary)
{
    const double a=(double)(v->heading>>16)*(2.0*BOT_PI/65536.0);
    const double fx=cos(a),fy=-sin(a);
    double way_x=(double)route->next_x+0.5;
    double way_y=(double)route->next_y+0.5;
    double wx,wy,forward,sideways;
    bot_route_steer_point(v,route,&way_x,&way_y);
    if(!bot_route_acid_crossing(v,route))
        bot_acid_waypoint(v,route,&way_x,&way_y);
    bot_prop_waypoint(v,route,&way_x,&way_y);
    wx=way_x-(double)(int32_t)v->x/65536.0;
    wy=way_y-(double)(int32_t)v->y/65536.0;
    forward=fx*wx+fy*wy;
    sideways=-fy*wx+fx*wy;
    unsigned secondary=AVP_MP_INPUT_ACTIONS;
    double x1=0.0,y1=0.0,x2=0.0,y2=0.0;
    /* Preserve single-axis precision at real props and on avoidable acid
     * approaches. On a path that has ALREADY selected an acid-covered tile,
     * keep the ordinary full-rate forward+strafe command available instead
     * of making the player rotate/sidestep a little on every 10-Hz tick. */
    /* Do not disable two-axis commands just because acid is NEARBY.
     * The swept damage square below decides whether combining axes would
     * cut the corner of an otherwise clean lane. */
    if(primary==AVP_MP_FORWARD || primary==AVP_MP_BACKWARD){
        if(fabs(sideways)<0.22)return secondary;
        secondary=sideways>0.0?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
    }else if(primary==AVP_MP_STRAFE_LEFT || primary==AVP_MP_STRAFE_RIGHT){
        if(fabs(forward)<0.22)return secondary;
        secondary=forward>0.0?AVP_MP_FORWARD:AVP_MP_BACKWARD;
    }else return secondary;
    switch(primary){
    case AVP_MP_FORWARD:x1=fx;y1=fy;break;
    case AVP_MP_BACKWARD:x1=-fx;y1=-fy;break;
    case AVP_MP_STRAFE_LEFT:x1=fy;y1=-fx;break;
    case AVP_MP_STRAFE_RIGHT:x1=-fy;y1=fx;break;
    default:break;
    }
    switch(secondary){
    case AVP_MP_FORWARD:x2=fx;y2=fy;break;
    case AVP_MP_BACKWARD:x2=-fx;y2=-fy;break;
    case AVP_MP_STRAFE_LEFT:x2=fy;y2=-fx;break;
    case AVP_MP_STRAFE_RIGHT:x2=-fy;y2=fx;break;
    default:break;
    }
    /* The sidestep by itself can enter a different cell, but its VECTOR
       SUM with forward motion can remain on the authored cardinal route. */
    if(!bot_step_clear(v,x1+x2,y1+y2,route))return AVP_MP_INPUT_ACTIONS;
    if(v->player_species!=1u && v->acid_count &&
       !bot_route_acid_crossing(v,route)){
        const double ax=(double)(int32_t)v->x/65536.0;
        const double ay=(double)(int32_t)v->y/65536.0;
        const unsigned straight=bot_acid_hits(v,ax,ay,
                                  ax+x1*0.28,ay+y1*0.28);
        const unsigned diagonal=bot_acid_hits(v,ax,ay,
                                  ax+(x1+x2)*0.28,ay+(y1+y2)*0.28);
        if(diagonal>straight)return AVP_MP_INPUT_ACTIONS;
    }
    return secondary;
}
/* Original AMP.S::do_score awards melee 1.0, ranged 0.5; invisibility
 * turns these into -1.0 and -2.0 respectively. The *engine* computes the
 * honor; this is purely the bot's weapon preference. Original slots:
 * 1 combi-stick, 2 cannon, 3 smart disc, 4 wrist blades. */
unsigned avp_mp_bot_predator_preferred_weapon(const AvpMpBotObservation *v)
{
    unsigned melee;
    if(!v||v->player_species!=2u)return 0u;
    melee=(v->predator_owned&(1u<<1))?1u:4u;
    if(!(v->predator_owned&(1u<<melee)))return v->current_weapon;
    /* A live Alien's source death spray takes 50 health in a 0.75-tile
     * square. Prefer an ALREADY UNLOCKED ranged weapon against this target,
     * even when healthy; otherwise a melee kill necessarily splashes the
     * Predator. Switching never grants weapons or alters retail honor. */
    if(v->hostile_target && v->target_is_alien &&
       v->goal==AVP_MP_BOT_HOSTILE){
        if(v->predator_owned&(1u<<3))return 3u; /* Smart Disc */
        if(v->predator_owned&(1u<<2))return 2u; /* Cannon */
    }
    /* Against other enemies, retain the original bot honor preference:
     * weak or outnumbered Predators may choose the unlocked Smart Disc. */
    if(v->hostile_target && (v->predator_owned&(1u<<3)) &&
       ((v->max_energy>0 && v->player_energy*100<v->max_energy*45) ||
        v->combat_enemies_nearby>=3u))return 3u;
    return melee;
}

/* Seeded bot-owned choice, isolated from the Jaguar's gameplay RNG.
 * Choose any actually owned combat weapon, avoiding the previous choice
 * when there is more than one. This never grants inventory. */
unsigned avp_mp_bot_random_predator_weapon(unsigned owned,unsigned previous,
                                           uint32_t seed)
{
    unsigned candidates[4],count=0u,choice;
    for(unsigned w=1u;w<=4u;w++)if(owned&(1u<<w))candidates[count++]=w;
    if(!count)return 0u;
    seed^=seed>>16;seed*=0x7feb352du;
    seed^=seed>>15;seed*=0x846ca68bu;seed^=seed>>16;
    choice=seed%count;
    if(count>1u && candidates[choice]==previous)
        choice=(choice+1u)%count;
    return candidates[choice];
}
/* R22 Predator Deathmatch rotation: 16 distinct permutations of all four
 * source weapon slots. There are 24 mathematical possibilities; this formula
 * intentionally uses 16. A bot walks the 16 rows with a private random start
 * and an odd stride, so each row occurs exactly once before the 64-slot
 * formula repeats. Only the source's weapon buttons perform actual switches. */
static const uint8_t predator_weapon_orders[16][4]={
    {1,2,3,4},{1,3,4,2},{1,4,2,3},{1,4,3,2},
    {2,1,3,4},{2,3,4,1},{2,4,1,3},{2,4,3,1},
    {3,1,4,2},{3,2,1,4},{3,4,1,2},{3,4,2,1},
    {4,1,2,3},{4,2,3,1},{4,3,1,2},{4,3,2,1}
};
unsigned avp_mp_bot_predator_order_weapon(unsigned order,unsigned step)
{
    if(order>=16u||step>=4u)return 0u;
    return predator_weapon_orders[order][step];
}
static unsigned bot_predator_formula_weapon(const AvpMpBotState *state)
{
    unsigned order=(state->predator_pattern_start+
        state->predator_pattern_stride*state->predator_pattern_order)&15u;
    return avp_mp_bot_predator_order_weapon(order,state->predator_pattern_step);
}
static void bot_predator_formula_advance(AvpMpBotState *state)
{
    if(++state->predator_pattern_step==4u){
        state->predator_pattern_step=0u;
        state->predator_pattern_order=(state->predator_pattern_order+1u)&15u;
    }
    state->predator_weapon_target=bot_predator_formula_weapon(state);
}
/* Actual route hops use the original LEVDOOR.S and DOORS.S metadata.
 * 1 is the main entry to the Predator ship, 15 is its end-Queen world. */
unsigned avp_mp_bot_alien_campaign_target_level(unsigned current_level)
{
    if(current_level<1u||current_level>15u||current_level==15u)return 0u;
    return current_level==1u?15u:1u;
}

static unsigned bot_weapon_cycle_direction(unsigned owned,unsigned current,
                                            unsigned desired)
{
    unsigned forward=current,backward=current;
    if(current<1u||current>4u)return AVP_MP_NEXT_WEAPON;
    for(unsigned n=0;n<4u;n++){
        forward=forward==4u?1u:forward+1u;
        backward=backward==1u?4u:backward-1u;
        if((owned&(1u<<forward)) && forward==desired)
            return AVP_MP_NEXT_WEAPON;
        if((owned&(1u<<backward)) && backward==desired)
            return AVP_MP_PREV_WEAPON;
    }
    return AVP_MP_NEXT_WEAPON;
}

static int bot_melee_weapon(const AvpMpBotObservation *v)
{
    /* Alien A/B/C are all melee attacks.  Predator ranged slots 2 and 3
     * must NOT be forced into the melee positioning band. */
    return v->player_species==1u ||
           (v->player_species==2u &&
            (v->current_weapon==1u || v->current_weapon==4u));
}
/* Guerilla distance management is independent of the fire decision.
 * Choose ordinary player movement buttons, never synthetic velocity. Close
 * when outside the desired band, BACKPEDAL when inside it, and flank only
 * while inside the band or when the authored collision rejects a step.
 * Melee range uses the original source weapon reach; ranged distance bands
 * are movement preferences only, NEVER artificial maximum firing ranges. */
static int bot_target_back_turned(const AvpMpBotObservation *v,
                                  double tx,double ty)
{
    const double a=(double)(v->target_heading>>16)*
                       (2.0*BOT_PI/65536.0);
    const double dist=sqrt(tx*tx+ty*ty);
    if(!v->target_visible || !v->target_heading_valid || dist<0.01)
        return 0;
    /* tx/ty point from bot TO enemy. A positive projection onto the
     * enemy's forward axis means the bot is behind that enemy. */
    return (cos(a)*tx-sin(a)*ty)>dist*0.60;
}
static unsigned bot_guerilla_approach(const AvpMpBotObservation *v,
                                      double tx,double ty)
{
    static const unsigned actions[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                                      AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
    const double a=(double)(v->heading>>16)*(2.0*BOT_PI/65536.0);
    const double fx=cos(a),fy=-sin(a);
    unsigned choice=AVP_MP_INPUT_ACTIONS;
    double best=0.0;
    for(unsigned n=0;n<4u;n++){
        const double dx=n==0u?fx:n==1u?-fx:n==2u?fy:-fy;
        const double dy=n==0u?fy:n==1u?-fy:n==2u?-fx:fx;
        const double progress=tx*dx+ty*dy;
        if(progress>best && bot_safe_action(v,actions[n],NULL)){
            best=progress;choice=actions[n];
        }
    }
    return choice;
}
static unsigned bot_guerilla_motion(AvpMpBotState *state,
                                    const AvpMpBotObservation *v,
                                    double tx,double ty,int back_turned)
{
    const double dist2=tx*tx+ty*ty;
    const int melee=bot_melee_weapon(v);
    /* Movement preferences only. Even a distant visible target can be
     * shot with a ranged weapon: only source collision/hit rules decide. */
    const double near=melee?BOT_MELEE_HOLD_NEAR:3.0;
    const double far=melee?BOT_MELEE_HOLD_FAR:5.0;
    unsigned side,other,motion;
    if(!state->guerilla_flank_ready){
        state->guerilla_flank_ready=1u;
        state->guerilla_flank_side=v->player_index&1u;
        state->guerilla_flank_until=v->tick+28u;
    }else if(v->tick>=state->guerilla_flank_until){
        state->guerilla_flank_side^=1u;
        state->guerilla_flank_until=v->tick+28u;
    }
    side=state->guerilla_flank_side?
         AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
    other=side==AVP_MP_STRAFE_LEFT?
          AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
    /* Do not orbit indefinitely outside melee reach (or far beyond a
     * preferred ranged fighting distance). Keep closing with legal player
     * controls; the aim and Fire decision is made independently below. */
    if(dist2>far*far || (melee && back_turned && dist2>near*near)){
        motion=bot_guerilla_approach(v,tx,ty);
        if(motion<AVP_MP_INPUT_ACTIONS)return motion;
    }
    /* Backpedal while looking at the opponent; another safe translation
     * axis is chosen when the wall/acid/solid prop blocks backward movement. */
    if(dist2<near*near){
        motion=bot_melee_retreat(v,tx,ty);
        if(motion<AVP_MP_INPUT_ACTIONS)return motion;
    }
    if(bot_safe_action(v,side,NULL))return side;
    if(bot_safe_action(v,other,NULL)){
        state->guerilla_flank_side^=1u;
        state->guerilla_flank_until=v->tick+28u;
        return other;
    }
    /* Corridor fallback: select a safe opening without changing the
     * original movement/collision code or inadvertently cancelling Fire. */
    if(dist2>far*far){
        motion=bot_guerilla_approach(v,tx,ty);
        if(motion<AVP_MP_INPUT_ACTIONS)return motion;
    }
    if(dist2<near*near){
        motion=bot_melee_retreat(v,tx,ty);
        if(motion<AVP_MP_INPUT_ACTIONS)return motion;
    }
    if(bot_safe_action(v,AVP_MP_BACKWARD,NULL))return AVP_MP_BACKWARD;
    if(bot_safe_action(v,AVP_MP_FORWARD,NULL))return AVP_MP_FORWARD;
    return AVP_MP_INPUT_ACTIONS;
}
static void emit_turn(double turn,AvpMpPlayerCommand *out)
{
    if(turn>BOT_PI/23.0)out->actions|=1u<<AVP_MP_TURN_LEFT;
    if(turn< -BOT_PI/23.0)out->actions|=1u<<AVP_MP_TURN_RIGHT;
}
/* When a noclip/glitch position lies beyond the authored map or inside a
 * nonfloor cell, A* cannot start and the normal safety filter refuses all
 * steps. Return using player buttons toward the nearest authored floor cell.
 * Never edit coordinates or bypass the original MovePlayer/SafePos checks.
 * If the native collision refuses re-entry, this remains a best-effort
 * recovery rather than an unsafe teleport across an exterior wall. */
static int bot_recover_outside(AvpMpBotState *state,
                               const AvpMpBotObservation *v,
                               AvpMpPlayerCommand *out)
{
    const AvpMpBotMap *m=&v->map;
    const int32_t px=(int32_t)v->x,py=(int32_t)v->y;
    const double xx=(double)px/65536.0,yy=(double)py/65536.0;
    const int inside=!avp_mp_bot_outside_bounds(v->x,v->y,
                                                m->width,m->height);
    double dx=0.0,dy=0.0,closest=1e99;
    double angle,fx,fy,scores[4];
    unsigned picked=0u,alternate=2u;
    static const unsigned motions[4]={AVP_MP_FORWARD,AVP_MP_BACKWARD,
                                     AVP_MP_STRAFE_LEFT,AVP_MP_STRAFE_RIGHT};
    if(!nav_valid(m))return 0;
    if(inside && nav_cell(m,(uint32_t)px>>16,(uint32_t)py>>16)[5])return 0;
    for(unsigned y=0;y<m->height;y++)for(unsigned x=0;x<m->width;x++){
        double vx,vy,d;
        if(!nav_cell(m,x,y)[5])continue;
        vx=(double)x+0.5-xx;vy=(double)y+0.5-yy;
        d=vx*vx+vy*vy;
        if(d<closest){closest=d;dx=vx;dy=vy;}
    }
    if(closest==1e99)return 1; /* Corrupt/no-floor map: do not improvise. */
    emit_turn(angle_diff(v->heading,dx,dy),out);
    angle=(double)(v->heading>>16)*(2.0*BOT_PI/65536.0);
    fx=cos(angle);fy=-sin(angle);
    scores[0]= dx*fx+dy*fy;
    scores[1]=-scores[0];
    scores[2]=dx*fy-dy*fx;
    scores[3]=-scores[2];
    for(unsigned a=0;a<4u;a++)if(scores[a]>scores[picked])picked=a;
    alternate=picked==0u?1u:0u;
    for(unsigned a=0;a<4u;a++)if(a!=picked && scores[a]>scores[alternate])
        alternate=a;
    if(state->last_motion &&
       llabs((long long)(int64_t)v->x-(long long)(int64_t)state->previous_x)<128 &&
       llabs((long long)(int64_t)v->y-(long long)(int64_t)state->previous_y)<128)
        state->immobile_ticks++;
    else state->immobile_ticks=0;
    /* Try a different real movement axis when the nearest approach is
     * repeatedly blocked; never synthesize collision-bypassing velocity. */
    if(state->immobile_ticks>=9u){
        state->immobile_ticks=0u;
        state->recovery_side^=1u;
        state->evade_ticks=12u;
    }
    if(state->evade_ticks){
        --state->evade_ticks;
        if(scores[alternate]>0.0)picked=alternate;
    }
    out->actions|=1u<<motions[picked];
    state->last_motion=1u<<motions[picked];
    state->previous_x=v->x;state->previous_y=v->y;
    return 1;
}
/* Bot exploration: choose unvisited, far-enough source-connected floors,
 * then VERIFY their routes through the normal original door/collision map.
 * Never teleport or invent a path through an inaccessible cell. */
int avp_mp_bot_patrol_pick(const AvpMpBotMap *map,unsigned x,unsigned y,
    const uint16_t visits[AVP_MP_BOT_MAP_CELLS],uint32_t seed,
    unsigned *target_x,unsigned *target_y)
{
    struct PatrolCandidate {unsigned x,y;uint64_t merit;} top[12]={{0}};
    uint16_t comps[AVP_MP_BOT_MAP_CELLS];
    unsigned count=0,component;
    if(!map||!visits||!target_x||!target_y||!nav_valid(map)||
       x>=map->width||y>=map->height)return 0;
    avp_mp_bot_components(map,comps);
    component=comps[y*AVP_MP_BOT_MAP_MAX+x];
    if(!component)return 0;
    for(unsigned cy=0;cy<map->height;cy++)for(unsigned cx=0;cx<map->width;cx++){
        unsigned idx=cy*AVP_MP_BOT_MAP_MAX+cx;
        unsigned dx=cx>x?cx-x:x-cx,dy=cy>y?cy-y:y-cy;
        unsigned dist=dx>dy?dx:dy,at;
        uint64_t merit;
        if(comps[idx]!=component||dist<2u||visits[idx]>=60000u)continue;
        /* Rare visits dominate; distant cells encourage active exploration.
         * Seed ties differently per player/tick, with no foreign target data. */
        /* Each bot's stable seed materially changes route selection; the
         * old 8-bit tie breaker was swamped by the farthest-cell score and
         * sent every new bot to the same map corner. Preserve its PRIVATE
         * visit history as the primary avoid-repeat signal. */
        {
            uint32_t hash=(uint32_t)idx*2654435761u ^ seed*2246822519u;
            hash^=hash>>16;hash*=0x7feb352du;hash^=hash>>15;
            merit=(visits[idx]==0u?20000u:
                   20000u/(1u+((unsigned)visits[idx]/4u)))+
                  (uint64_t)(dist>24u?24u:dist)*128u+
                  (uint64_t)(hash&0x3fffu);
        }
        for(at=0;at<count && top[at].merit>=merit;at++);
        if(at>=12u)continue;
        if(count<12u)count++;
        for(unsigned i=count-1u;i>at;i--)top[i]=top[i-1u];
        top[at]=(struct PatrolCandidate){cx,cy,merit};
    }
    for(unsigned i=0;i<count;i++){
        AvpMpBotRoute route={0};
        if(avp_mp_bot_route(map,x,y,top[i].x,top[i].y,&route)){
            *target_x=top[i].x;*target_y=top[i].y;return 1;
        }
    }
    return 0;
}
void avp_mp_bot_step(AvpMpBotState *state,const AvpMpBotObservation *v,
                     AvpMpPlayerCommand *out)
{
    double dx,dy,turn;
    int need_forward,need_fire=0;
    unsigned tactical_strafe=0u,threat_movement=0u,melee_movement=0u;
    unsigned guerilla_movement=0u;
    unsigned splash_retreat_motion=0u;
    int melee_engage=0,lure_alien=0,alien_ranged_standoff=0;
    AvpMpBotRoute route={0};
    unsigned start_x,start_y,goal_x,goal_y;
    int map_route=0;
    if(!state||!v||!out)return;
    memset(out,0,sizeof(*out));
    if(!state->initialized){
        state->previous_x=v->x;state->previous_y=v->y;
        state->initialized=1;
    }
    /* MAZESCRN.S::update_wep tests a fresh active-low JOY_LEFT/RIGHT
     * edge once per successful throw. Holding the direction does nothing.
     * The first neutral tick ensures a fresh edge even if the bot happened
     * to be sidestepping or turning when the Facehugger attached. Nine
     * presses, damage, recovery, and the Facehugger release all remain
     * exclusively in the unchanged original gameplay code. */
    if(v->facehugged){
        if(state->facehug_phase==1u){
            out->actions|=1u<<(v->facehug_direction<0 ?
                AVP_MP_STRAFE_RIGHT : AVP_MP_STRAFE_LEFT);
            state->facehug_phase=2u;
        }else state->facehug_phase=1u;
        state->previous_x=v->x;state->previous_y=v->y;
        state->immobile_ticks=0u;state->evade_ticks=0u;
        state->last_motion=0u;
        return;
    }
    state->facehug_phase=0u;
    if(bot_recover_outside(state,v,out))return;
    /* The source-owned Queen ending starts after the last generator dies.
     * Face her without issuing weapons or movement; the original AMP tick
     * will decide if/when the cutscene starts. */
    if(v->goal==AVP_MP_BOT_QUEEN_WAIT && v->has_target &&
       v->player_species==1u){
        emit_turn(angle_diff(v->heading,
            (double)((int64_t)v->target_x-(int64_t)v->x),
            (double)((int64_t)v->target_y-(int64_t)v->y)),out);
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=0u;state->immobile_ticks=0u;
        state->evade_ticks=0u;
        return;
    }
    /* Original Predator OPTION is edge-triggered. Cloak while live Marines
     * exist anywhere on this level, but remain visibly fighting until all
     * nearby enemies are gone. No Marines => stay uncloaked. No alteration
     * to the retail fade/honor flag: no attack until invisflag clears. */
    if(v->player_species==2u){
        int near_fight=v->combat_enemies_nearby!=0u;
        /* Also decloak to attack a genuinely observed nearby Deathmatch
         * player. Without this, invisflag blocked every FIRE forever. */
        if(v->deathmatch && v->hostile_target && v->target_visible){
            double rx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
            double ry=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
            if(rx*rx+ry*ry<=36.0)near_fight=1;
        }
        int want_cloak;
        if(v->max_energy>0 && v->player_energy>0 &&
           v->player_energy*100 < v->max_energy*70 && v->medpak>0)
            out->actions|=1u<<AVP_MP_PRED_MEDKIT;
        if(near_fight)state->combat_hold_until=v->tick+12u;
        want_cloak=(v->marines_on_level>0u ||
                    (v->deathmatch && v->dm_opponents_on_level>0u)) &&
                   !near_fight &&
                   v->tick>=state->combat_hold_until;
        if(want_cloak && !v->invis_act && !v->invis_stat)
            out->actions|=1u<<AVP_MP_PRED_CLOAK;
        if(!want_cloak && v->invis_act)
            out->actions|=1u<<AVP_MP_PRED_CLOAK;
    }
    if(v->player_species!=1u || v->goal!=AVP_MP_BOT_COCOON ||
       !v->target_is_marine){
        state->cocoon_locked=0u;state->cocoon_phase=0u;
    }
    if(v->has_target){
        dx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        dy=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
    }else{
        double direction=(double)((v->tick/50u+v->player_index*3u)&7u)*(BOT_PI/4.0);
        dx=cos(direction);dy=-sin(direction);
    }
    start_x=v->x>>16;start_y=v->y>>16;
    goal_x=v->target_x>>16;goal_y=v->target_y>>16;
    /* Do not reach an invalid interaction cell and press Use forever.
     * Source DOORS.S rejects Aliens at lifts and Predators at ducts. */
    if(v->goal==AVP_MP_BOT_LIFT && v->player_species==1u)return;
    /* DOORS.S::do_lifts only jams one position/direction pair. Do not
     * blacklist the entire lift or either neighboring sub-level: its
     * other directions are still legitimate. Also reject a stale goal
     * before issuing Use if a caller ever bypasses the route selector. */
    if(v->goal==AVP_MP_BOT_LIFT &&
       avp_mp_bot_lift_jammed(v->current_level,
                             v->target_x>>16,v->target_y>>16,
                             v->lift_direction))return;
    if((v->goal==AVP_MP_BOT_EXIT || v->goal==AVP_MP_BOT_LIFT) &&
       v->current_level && v->destination_level &&
       !avp_mp_bot_transition_allowed(v->player_species,
                 v->current_level,v->destination_level,
                 v->goal==AVP_MP_BOT_LIFT?AVP_MP_BOT_TRANS_LIFT:
                 (v->exit_is_duct?AVP_MP_BOT_TRANS_DUCT:AVP_MP_BOT_TRANS_EXIT)))
        return;
    if(v->goal==AVP_MP_BOT_EXIT && v->exit_is_duct &&
       v->player_species==2u)return;
    /* Aliens take no acid damage; ignore the risk cost, not doors, props
       or wall collisions. This also covers independent test observations. */
    AvpMpBotMap species_map=v->map;
    if(v->player_species==1u)species_map.risk=NULL;
    if(v->has_target && nav_valid(&species_map) &&
       avp_mp_bot_route(&species_map,start_x,start_y,goal_x,goal_y,&route)){
        map_route=1;
        if(route.next_x!=start_x || route.next_y!=start_y){
            double wx,wy;
            bot_route_steer_point(v,&route,&wx,&wy);
            dx=wx-(double)v->x/65536.0;
            dy=wy-(double)v->y/65536.0;
            /* Only the TURN target is straightened; the route movement
             * still aims at the actual waypoint and obeys native collision. */
            (void)bot_straight_route_heading(v,&route,&dx,&dy);
        }
    }
    if(v->has_target && nav_valid(&v->map) && !map_route &&
       !(v->hostile_target && v->target_visible)){
        /* Don't blindly drive into a known solid wall or locked door.
         * A visible hostile is different: engage without a planned path. */
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=0u;
        return;
    }
    /* Source DOORS.S requires facing the panel, not looking toward a remote
     * waypoint. At the target cell of an authored exit/lift, turn to panel. */
    if(map_route && start_x==goal_x && start_y==goal_y &&
       (v->goal==AVP_MP_BOT_LIFT||v->goal==AVP_MP_BOT_EXIT)){
        static const int xdir[4]={-1,0,1,0},ydir[4]={0,-1,0,1};
        if(v->target_side>=4u)return;
        turn=angle_diff(v->heading,(double)xdir[v->target_side],
                        (double)ydir[v->target_side]);
        emit_turn(turn,out);
        if(fabs(turn)<BOT_PI/9.0 &&
           (state->last_exit_use==0u||v->tick-state->last_exit_use>=35u)){
            out->actions|=1u<<AVP_MP_USE;
            if(v->goal==AVP_MP_BOT_LIFT)
                out->actions|=1u<<(v->lift_direction<0?AVP_MP_FORWARD:AVP_MP_BACKWARD);
            state->last_exit_use=v->tick+1u;
        }
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=0u;
        return;
    }
    turn=angle_diff(v->heading,dx,dy);
    if(!v->has_target)need_forward=1;
    else if(v->goal==AVP_MP_BOT_COCOON && v->target_is_marine){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        /* Live Marines: stop in actual melee reach for the claw/tail/claw
         * sequence. Stunned fallen Marines: physically approach the source
         * stun_death proximity square, then stay still until converted. */
        need_forward=!v->target_energy_positive ?
            (fabs(tx)>0.10 || fabs(ty)>0.10):
            (tx*tx+ty*ty>BOT_MELEE_HOLD_FAR*BOT_MELEE_HOLD_FAR);
    }
    else if(v->hostile_target){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        if(v->behavior==1u) /* Berserker: press into melee, even with a gun. */
            need_forward=!v->target_visible || tx*tx+ty*ty>0.36*0.36;
        else if(v->behavior==2u && v->player_species!=1u &&
                (v->player_species==0u ||
                 (v->predator_owned&((1u<<2)|(1u<<3)))!=0u))
            need_forward=!v->target_visible && tx*tx+ty*ty>4.5*4.5;
        else need_forward=!v->target_visible || tx*tx+ty*ty>1.6*1.6;
    }else if(v->goal==AVP_MP_BOT_COCOON){
        /* AMP.S::stun_death uses amp_in_range(20), which means the
         * player's XY must be within 20<<9 (=0.15625 cell) on BOTH axes.
         * Merely entering the same maze tile cannot create a cocoon. */
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        need_forward=fabs(tx)>0.10 || fabs(ty)>0.10;
    }else if(v->goal==AVP_MP_BOT_HEALTH ||
             v->goal==AVP_MP_BOT_SUPPLY ||
             v->goal==AVP_MP_BOT_EXIT ||
             v->goal==AVP_MP_BOT_LIFT)
        need_forward=(start_x!=goal_x || start_y!=goal_y);
    else{
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        /* R10 patrol controller only completes at <0.70 tiles along BOTH
         * axes. The old generic 1.8-tile stop radius stopped movement far
         * OUTSIDE that arrival threshold: stand still -> 25-tick timeout
         * -> new goal -> repeat. Patrol is travel, not combat positioning.
         * Continue to the actual destination until the host sees arrival. */
        if(v->goal==AVP_MP_BOT_PATROL)
            need_forward=fabs(tx)>0.50 || fabs(ty)>0.50;
        else need_forward=tx*tx+ty*ty>1.8*1.8;
    }
    /* Stay OUTSIDE the ORIGINAL alien death-splash square while using a
     * ranged gun. Only a visible living target qualifies; do not turn the
     * ordinary floor acid into a wider barrier or flee every Alien when
     * pursuing an exit. An unlocked ranged Predator can retreat WHILE
     * switching away from melee. A melee-only Predator must still attack. */
    alien_ranged_standoff=v->behavior!=1u && v->hostile_target && v->target_is_alien &&
        v->target_visible && v->goal==AVP_MP_BOT_HOSTILE &&
        (v->player_species==0u ||
         (v->player_species==2u &&
          (v->predator_owned&((1u<<2)|(1u<<3)))!=0u));
    if(alien_ranged_standoff){
        const int64_t tx=(int64_t)v->target_x-(int64_t)v->x;
        const int64_t ty=(int64_t)v->target_y-(int64_t)v->y;
        /* Safe once EITHER axis reaches 0.76 tile. The original splash
         * is a square, not a circular Euclidean range. */
        if(llabs(tx)<BOT_ALIEN_DEATH_STANDOFF_UNITS &&
           llabs(ty)<BOT_ALIEN_DEATH_STANDOFF_UNITS){
            unsigned motion=bot_melee_retreat(v,(double)tx/65536.0,
                                               (double)ty/65536.0);
            need_forward=0;
            if(motion<AVP_MP_INPUT_ACTIONS)
                splash_retreat_motion=motion+1u;
        }
    }
    /* Melee positioning is meaningful only for a visible, reachable enemy.
     * If too far, approach; if too close, retreat without turning the back
     * on the enemy; at good strike distance strafe/fire.  The attack itself
     * still goes through the original weapon animation and hit test. */
    melee_engage=v->hostile_target && v->target_visible &&
                 !alien_ranged_standoff &&
                 bot_melee_weapon(v) && v->goal!=AVP_MP_BOT_COCOON;
    if(melee_engage && v->behavior!=1u){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        double distance2=tx*tx+ty*ty;
        if(distance2<BOT_MELEE_HOLD_NEAR*BOT_MELEE_HOLD_NEAR){
            unsigned motion=bot_melee_retreat(v,tx,ty);
            need_forward=0;
            if(motion<AVP_MP_INPUT_ACTIONS)melee_movement=motion+1u;
        }else if(distance2<=BOT_MELEE_HOLD_FAR*BOT_MELEE_HOLD_FAR){
            unsigned preferred=((v->tick/12u+v->player_index)&1u)?
                AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
            unsigned alternate=preferred==AVP_MP_STRAFE_LEFT?
                AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
            need_forward=0;
            if(bot_safe_action(v,preferred,NULL))melee_movement=preferred+1u;
            else if(bot_safe_action(v,alternate,NULL))melee_movement=alternate+1u;
        }else need_forward=1;
    }
    /* On a real choke corridor, briefly lure an Alien off the path before
       delivering the killing shot. Give up after three original seconds,
       or immediately when the Alien is inside the dangerous melee margin.
       No lure against other creatures or while the bot is badly wounded. */
    if(v->behavior==0u && v->hostile_target && v->target_is_alien && v->target_visible &&
       v->player_species==0u && v->goal==AVP_MP_BOT_HOSTILE &&
       /* A vent must remain fightable even if the acid hitbox takes its
        * entire width: do not permanently refuse combat in duct maps. */
       !(v->current_level>=6u && v->current_level<=13u) &&
       (!v->max_energy || v->player_energy*100>=v->max_energy*35) &&
       nav_valid(&v->map) &&
       !avp_mp_bot_alien_death_passable(&v->map,v->target_x,v->target_y)){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        if(tx*tx+ty*ty>2.3*2.3){
            if(state->lure_target_slot!=v->target_slot+1u){
                state->lure_target_slot=v->target_slot+1u;
                state->lure_cooldown_tick=0u;
            }
            if(v->tick>=state->lure_cooldown_tick){
                state->lure_end_tick=v->tick+150u;
                state->lure_cooldown_tick=v->tick+300u;
            }
            lure_alien=v->tick<state->lure_end_tick;
        }
    }
    if(lure_alien)need_forward=0;
    if(v->hostile_target){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        double rawturn=angle_diff(v->heading,tx,ty);
        int melee=bot_melee_weapon(v);
        /* Guerilla ranged FIRE has no tactical distance/lure gate. LOS and
         * aim still matter; actual damage, ammo, equip/cloak state and hit
         * geometry remain the untouched original weapon implementation. */
        if(!lure_alien && v->target_visible &&
           (melee ? bot_melee_source_shot_box(v,tx,ty) :
                    (v->behavior==2u || tx*tx+ty*ty<=6.0*6.0)) &&
           fabs(rawturn)<(tx*tx+ty*ty<2.25 ? BOT_PI/6.0 : BOT_PI/11.0) &&
           (v->player_species!=2u ||
            (!v->invis_act&&!v->invis_stat&&!v->invisflag)))
            need_fire=1;
    }
    /* Melee evasions take priority over ordinary approach only when a
     * living source Alien/Predator/Queen/Facehugger is actually in sight.
     * The margin is larger than the original sub-cell strike distance so
     * movement can start before the next 10-Hz combat tick. */
    if(v->behavior!=1u && v->melee_threat && v->melee_visible &&
       !(v->goal==AVP_MP_BOT_COCOON && v->target_is_marine) &&
       !(melee_engage &&
         llabs((long long)(int64_t)v->target_x-(long long)(int64_t)v->melee_x)<8192 &&
         llabs((long long)(int64_t)v->target_y-(long long)(int64_t)v->melee_y)<8192)){
        /* Do not evade the SAME enemy we are deliberately dueling at melee
         * reach.  Other nearby threats still override melee positioning. */
        double tx=(double)((int64_t)v->melee_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->melee_y-(int64_t)v->y)/65536.0;
        double distance2=tx*tx+ty*ty;
        double limit=v->melee_threat==AVP_MP_BOT_THREAT_QUEEN?3.0:
                     v->melee_threat==AVP_MP_BOT_THREAT_FACEHUG?2.6:2.25;
        if(distance2<limit*limit &&
           v->goal!=AVP_MP_BOT_EXIT && v->goal!=AVP_MP_BOT_LIFT){
            /* Aliens need melee reach to stun/cocoon; strafe rather than
             * permanently fleeing their own intended melee encounter. */
            double retreat=angle_diff(v->heading,-tx,-ty);
            unsigned candidates[4];
            if(v->player_species==1u){
                candidates[0]=(v->tick/8u+v->player_index)%2u?
                    AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
                candidates[1]=candidates[0]==AVP_MP_STRAFE_LEFT?
                    AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
                candidates[2]=AVP_MP_BACKWARD;candidates[3]=AVP_MP_FORWARD;
            }else if(fabs(retreat)<BOT_PI/4.0){
                candidates[0]=AVP_MP_FORWARD;
                candidates[1]=retreat>0?AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
                candidates[2]=candidates[1]==AVP_MP_STRAFE_LEFT?
                    AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
                candidates[3]=AVP_MP_BACKWARD;
            }else if(fabs(retreat)>3.0*BOT_PI/4.0){
                candidates[0]=AVP_MP_BACKWARD;
                candidates[1]=retreat>0?AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
                candidates[2]=candidates[1]==AVP_MP_STRAFE_LEFT?
                    AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
                candidates[3]=AVP_MP_FORWARD;
            }else{
                candidates[0]=retreat>0?AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
                candidates[1]=AVP_MP_BACKWARD;
                candidates[2]=AVP_MP_FORWARD;
                candidates[3]=candidates[0]==AVP_MP_STRAFE_LEFT?
                    AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
            }
            for(unsigned n=0;n<4u;n++)if(bot_safe_action(v,candidates[n],NULL)){
                threat_movement=candidates[n]+1u;break;
            }
        }
    }
    /* While fighting at a distance, sidestep and shoot instead of always
     * walking straight into the enemy. If a wall/acid makes one flank
     * unsafe, attempt the other; never assert two opposite sidestep bits. */
    if(v->behavior!=1u && !threat_movement && !melee_engage && v->goal!=AVP_MP_BOT_COCOON &&
       v->hostile_target && v->target_visible &&
       v->goal==AVP_MP_BOT_HOSTILE){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        double distance2=tx*tx+ty*ty;
        double range=v->player_species==1u?2.0:6.0;
        if(distance2<range*range){
            unsigned preferred=((v->tick/12u+v->player_index)&1u)?
                AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
            unsigned alternate=preferred==AVP_MP_STRAFE_LEFT?
                AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
            if(bot_safe_action(v,preferred,NULL))tactical_strafe=preferred+1u;
            else if(bot_safe_action(v,alternate,NULL))tactical_strafe=alternate+1u;
        }
    }
    /* Guerilla closes when too far away, backpedals when too close, and
     * flanks inside the preferred band. These movement decisions NEVER
     * suppress Fire while a ranged target is visible and aimed at. Melee
     * retains its exact reach gate; emergency movement still takes priority. */
    if(v->behavior==2u && v->hostile_target && v->target_visible &&
       v->goal==AVP_MP_BOT_HOSTILE && !lure_alien){
        double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
        double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
        const int back=bot_target_back_turned(v,tx,ty);
        unsigned motion=bot_guerilla_motion(state,v,tx,ty,back);
        if(motion<AVP_MP_INPUT_ACTIONS)guerilla_movement=motion+1u;
        tactical_strafe=0u; /* one coherent tactical motion, not opposites */
        /* Do not let the old ranged-only stop radius stall a melee bot
         * when a target gives it a clear rear approach. */
        if(back && bot_melee_weapon(v) &&
           tx*tx+ty*ty>BOT_MELEE_HOLD_FAR*BOT_MELEE_HOLD_FAR)
            need_forward=1;
        else if(!bot_melee_weapon(v))need_forward=0;
    }
    /* A blocked corridor requires steering via ORIGINAL MovePlayer. For a
     * closed, accessible authored door, press Use once then let its original
     * animation advance before trying the boundary again. */
    if(map_route && route.requires_door_use &&
       !(v->hostile_target && v->target_visible) &&
       (route.next_x!=start_x||route.next_y!=start_y)){
        uint16_t cell=(uint16_t)nav_index(start_x,start_y),
                 next=(uint16_t)nav_index(route.next_x,route.next_y);
        emit_turn(turn,out);
        if(state->door_cell!=cell || state->door_next!=next){
            state->door_cell=cell;state->door_next=next;state->door_attempts=0;
            state->door_cooldown=0;
        }
        if(fabs(turn)<BOT_PI/9.0 && route.door_value==0u &&
           !bot_door_animating(v,route.door_offset) &&
           (!state->door_cooldown || v->tick>=state->door_cooldown) &&
           (!v->door_claims || avp_mp_bot_door_claim(v->door_claims,
                 v->map.cells,route.door_offset,v->tick))){
            out->actions|=1u<<AVP_MP_USE;
            state->last_door_use=v->tick;
            state->door_cooldown=(unsigned)(v->tick+26u);
            if(state->door_attempts<100u)state->door_attempts++;
        }
        state->immobile_ticks=0;
        state->previous_x=v->x;state->previous_y=v->y;
        state->last_motion=0u;
        return;
    }
    /* Stuck means the LAST command actually tried to translate but the
     * original engine returned essentially the same coordinates. Do not
     * mistake stationary aiming, cocooning or a closed door for collision. */
    if(state->last_motion && !state->evade_ticks &&
       llabs((long long)(int64_t)v->x-(long long)(int64_t)state->previous_x)<128 &&
       llabs((long long)(int64_t)v->y-(long long)(int64_t)state->previous_y)<128){
        if(state->immobile_ticks<1000u)state->immobile_ticks++;
    }else if(!state->evade_ticks)state->immobile_ticks=0;
    /* A harmless sidestep may be selected repeatedly beside an acid
     * puddle even if the original collision prevents actual translation.
     * After four no-progress ticks, commit to the planner's cardinal next
     * cell for a limited period. Acid still hurts; movement never noclips. */
    if(map_route && need_forward && v->player_species!=1u &&
       v->acid_count && state->immobile_ticks>=4u &&
       bot_route_acid_crossing(v,&route)){
        state->acid_commit_ticks=24u;
        state->evade_ticks=0u;
        state->immobile_ticks=0u;
    }
    if(state->acid_commit_ticks)state->acid_commit_ticks--;
    if(state->immobile_ticks>9u){
        state->evade_ticks=19u;state->immobile_ticks=0u;
        state->recovery_side^=1u;
        state->recovery_count++;
    }
    state->previous_x=v->x;state->previous_y=v->y;
    if(state->evade_ticks){
        --state->evade_ticks;
        /* When a target is already visible, do not blindly rotate away
         * for 19 recovery ticks; preserve original-aim attack cadence. */
        if(v->hostile_target && v->target_visible)
            emit_turn(angle_diff(v->heading,
                (double)((int64_t)v->target_x-(int64_t)v->x),
                (double)((int64_t)v->target_y-(int64_t)v->y)),out);
        else out->actions|=1u<<AVP_MP_TURN_RIGHT;
        /* Retreat away from the object first, then translate sideways,
         * and finally test forward progress. Flip flank on each failure. */
        {unsigned motion=AVP_MP_INPUT_ACTIONS;
         unsigned flank=state->recovery_side?AVP_MP_STRAFE_RIGHT:AVP_MP_STRAFE_LEFT;
         unsigned opposite=flank==AVP_MP_STRAFE_RIGHT?
             AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
         if(state->evade_ticks>=14u &&
            bot_safe_action(v,AVP_MP_BACKWARD,NULL))motion=AVP_MP_BACKWARD;
         else if(state->evade_ticks>=6u && bot_safe_action(v,flank,NULL))
            motion=flank;
         else if(state->evade_ticks>=6u && bot_safe_action(v,opposite,NULL))
            motion=opposite;
         else if(bot_safe_action(v,AVP_MP_FORWARD,map_route?&route:NULL))
            motion=AVP_MP_FORWARD;
         else if(bot_safe_action(v,flank,NULL))motion=flank;
         else if(bot_safe_action(v,opposite,NULL))motion=opposite;
         if(motion<AVP_MP_INPUT_ACTIONS)out->actions|=1u<<motion;
        }
    }else{
        /* Face a visible enemy while issuing movement via independent
         * original sidestep controls. Route waypoints still guide pursuit
         * when the source LOS reports a blocked shot. */
        if(v->hostile_target && v->target_visible)
            emit_turn(angle_diff(v->heading,
                (double)((int64_t)v->target_x-(int64_t)v->x),
                (double)((int64_t)v->target_y-(int64_t)v->y)),out);
        else emit_turn(turn,out);
        if(splash_retreat_motion){
            /* Retreat takes priority over routine melee-evasion strafes,
             * which can carry a shooter INTO the 50-damage kill square.
             * Keep the independent face-enemy aim and Fire commands. */
            out->actions|=1u<<(splash_retreat_motion-1u);
        }else if(threat_movement){
            out->actions|=1u<<(threat_movement-1u);
        }else if(lure_alien){
            /* Do not freeze a shooter in the choke while the chasing Alien
             * is still at normal ranged-combat distance. The old four-cell
             * retreat cutoff left a five-cell target in sight but made no
             * retreat command, so the Alien was killed in its doorway when
             * the finite lure timeout expired. Backpedal throughout the
             * existing six-cell engagement band, only while facing the
             * Alien: otherwise BACKWARD would actually close the distance.
             * bot_safe_action retains original wall/prop/acid restrictions;
             * the 150-tick lure timeout and its fire suppression are intact. */
            double tx=(double)((int64_t)v->target_x-(int64_t)v->x)/65536.0;
            double ty=(double)((int64_t)v->target_y-(int64_t)v->y)/65536.0;
            if(tx*tx+ty*ty<6.0*6.0 &&
               fabs(angle_diff(v->heading,tx,ty))<BOT_PI/4.0 &&
               bot_safe_action(v,AVP_MP_BACKWARD,NULL))
                out->actions|=1u<<AVP_MP_BACKWARD;
        }else if(melee_movement){
            out->actions|=1u<<(melee_movement-1u);
        }else if(guerilla_movement){
            out->actions|=1u<<(guerilla_movement-1u);
        }else if(map_route && need_forward && !tactical_strafe &&
                 (route.next_x!=start_x || route.next_y!=start_y)){
            unsigned motion=bot_route_motion(v,&route,
                                               state->acid_commit_ticks!=0u);
            if(motion<AVP_MP_INPUT_ACTIONS){
                unsigned second=bot_route_second_motion(v,&route,motion);
                out->actions|=1u<<motion;
                if(second<AVP_MP_INPUT_ACTIONS)out->actions|=1u<<second;
            }
        }else{
            if(tactical_strafe)out->actions|=1u<<(tactical_strafe-1u);
            if(need_forward && fabs(turn)<BOT_PI*0.53 &&
               bot_safe_action(v,AVP_MP_FORWARD,map_route?&route:NULL))
                out->actions|=1u<<AVP_MP_FORWARD;
            /* Route movement need not rotate 90 degrees first. If the
             * waypoint lies alongside the player, translate sideways. */
            if(need_forward && !tactical_strafe &&
               !(out->actions&(1u<<AVP_MP_FORWARD)) &&
               fabs(turn)>=BOT_PI*0.30 && fabs(turn)<=BOT_PI*0.70){
                unsigned side=turn>0.0?AVP_MP_STRAFE_LEFT:AVP_MP_STRAFE_RIGHT;
                if(bot_safe_action(v,side,map_route?&route:NULL))out->actions|=1u<<side;
            }
        }
    }
    /* Immediate survival override: remaining motionless while inside a
     * native acid contact square is not a valid combat or navigation goal.
     * Choose a source-legal action to leave even if regular tactical motion
     * chose to wait, lure, or strafe against the route. Leave aiming/Fire
     * untouched so the bot can fight while escaping. */
    if(v->player_species!=1u && v->acid_count){
        unsigned escape=bot_escape_current_acid(v,map_route?&route:NULL);
        if(escape<AVP_MP_INPUT_ACTIONS){
            const unsigned move_mask=(1u<<AVP_MP_FORWARD)|
                (1u<<AVP_MP_BACKWARD)|(1u<<AVP_MP_STRAFE_LEFT)|
                (1u<<AVP_MP_STRAFE_RIGHT);
            unsigned second=AVP_MP_INPUT_ACTIONS;
            /* The acid emergency override previously stripped a valid
             * companion motion EVERY tick while standing in the puddle.
             * Use the same collision-tested combined Jaguar commands as
             * the route follower so an oblique crossing keeps moving. */
            if(map_route && bot_route_acid_crossing(v,&route))
                second=bot_route_second_motion(v,&route,escape);
            out->actions=(out->actions&~move_mask)|(1u<<escape);
            if(second<AVP_MP_INPUT_ACTIONS)out->actions|=1u<<second;
        }
    }
    /* Marine rounds never regenerate. No rounds in the currently equipped
       owned gun => do not hammer FIRE; cycle to an owned, loaded weapon if
       available. All-guns-empty Marines instead retreat toward supplies. */
    if(v->player_species==0u && v->marine_inventory_valid){
        const unsigned weapon=v->current_weapon;
        const int loaded=weapon>=1u && weapon<=4u &&
            (v->marine_owned&(1u<<weapon)) && v->marine_ammo[weapon-1u]>0u;
        if(!loaded){
            need_fire=0;
            for(unsigned i=0;i<4u;i++)if((v->marine_owned&(1u<<(i+1u))) &&
                v->marine_ammo[i]>0u && v->tick%12u==0u){
                out->actions|=1u<<AVP_MP_NEXT_WEAPON;
                break;
            }
        }
    }
    /* R22 Predator Deathmatch weapon formula: 16 distinct randomized orders,
     * traversed once (64 entries) and then repeated. Weapon selection itself
     * uses ONLY the original next/previous weapon controls. Each bot counts
     * 150 of its own OUT-OF-COMBAT 10-Hz ticks (15 seconds). Pursuing a
     * hostile OR having a nearby detected threat pauses the count and forbids
     * changing weapon. It resumes without resetting after battle. When a
     * switch is already due, combat also suspends the pending switch until
     * fighting ends; no timer is spent waiting for the source equip animation.
     * Co-op/campaign weapon priorities remain unchanged. */
    if(v->player_species==2u && v->predator_owned){
        unsigned desired=avp_mp_bot_predator_preferred_weapon(v);
        if(v->deathmatch){
            int in_combat=v->hostile_target || v->combat_enemies_nearby;
            if(!state->predator_pattern_ready){
                uint32_t seed=(uint32_t)v->tick*0x9e3779b9u ^
                    (v->player_index+1u)*0x85ebca6bu;
                seed^=seed>>16;seed*=0x7feb352du;
                seed^=seed>>15;seed*=0x846ca68bu;seed^=seed>>16;
                state->predator_pattern_start=seed&15u;
                state->predator_pattern_stride=1u+2u*((seed>>4)&7u);
                state->predator_pattern_order=0u;
                state->predator_pattern_step=0u;
                state->predator_pattern_ready=1u;
                state->predator_weapon_target=bot_predator_formula_weapon(state);
                state->predator_rotation_last_tick=v->tick-1u;
            }
            /* Count simulation ticks, never host wall-clock time. A gap
             * while an actor was paused/inactive does not earn idle time,
             * and duplicate observations in one tick cannot speed it up. */
            if(state->predator_rotation_last_tick!=v->tick){
                state->predator_rotation_last_tick=v->tick;
                if(!in_combat && !state->predator_switch_pending &&
                   state->predator_idle_ticks<150u)
                    state->predator_idle_ticks++;
            }
            if(!in_combat){
                if(!state->predator_switch_pending &&
                   state->predator_idle_ticks>=150u){
                    /* An order boundary can repeat the current weapon.
                     * Skip identical/unowned entries in the formula rather
                     * than wasting a 15-second interval on a non-switch. */
                    for(unsigned i=0u;i<64u &&
                        (!(v->predator_owned&(1u<<state->predator_weapon_target)) ||
                         state->predator_weapon_target==v->current_weapon);i++)
                        bot_predator_formula_advance(state);
                    if(state->predator_weapon_target!=v->current_weapon &&
                       (v->predator_owned&(1u<<state->predator_weapon_target)))
                        state->predator_switch_pending=1u;
                    else {
                        /* With only one weapon there is no legal switch. */
                        state->predator_weapon_target=v->current_weapon;
                        state->predator_idle_ticks=0u;
                    }
                }
                if(state->predator_switch_pending){
                    if(v->current_weapon==state->predator_weapon_target){
                        /* The original switch animation finished. Commit
                         * one sequence step and restart the 15-sec interval. */
                        bot_predator_formula_advance(state);
                        state->predator_switch_pending=0u;
                        state->predator_idle_ticks=0u;
                    }else desired=state->predator_weapon_target;
                }
            }
            /* While in combat keep exactly the weapon already equipped,
             * including when an out-of-combat switch was pending. */
            if(in_combat || !state->predator_switch_pending)
                desired=v->current_weapon;
        }
        if(desired>=1u && desired<=4u && v->current_weapon!=desired &&
           v->tick%10u==0u){
            unsigned direction=bot_weapon_cycle_direction(
                v->predator_owned,v->current_weapon,desired);
            out->actions|=1u<<direction;
            need_fire=0; /* FIRE remains enabled on non-switching ticks. */
        }
    }
    if(need_fire){
        /* The original Alien tail (cur_wepno==1) marks a Human AMP as
         * STUN. Switching to claw once stunned avoids toggling STUN off
         * with a second tail hit, allowing the native cocoon death path. */
        unsigned weapon=AVP_MP_FIRE;
        if(v->player_species==1u){
            if(v->goal==AVP_MP_BOT_COCOON && v->target_is_marine){
                /* The retail tail toggles STUN, the swipe/claw preserves
                 * it, and the bite clears it. Never tail a stunned Marine.
                 * Each stage lasts an original-animation-sized window. */
                if(v->target_is_stunned){
                    weapon=AVP_MP_ALIEN_CLAW;
                    state->cocoon_phase=2u;
                }else{
                    if(v->tick>=state->cocoon_phase_end){
                        state->cocoon_phase=(state->cocoon_phase+1u)%3u;
                        state->cocoon_phase_end=v->tick+14u;
                    }
                    weapon=state->cocoon_phase==1u ?
                        AVP_MP_ALIEN_TAIL:AVP_MP_ALIEN_CLAW;
                }
            }else if(!v->deathmatch && v->target_is_marine &&
                     v->max_energy>0 &&
                     v->player_energy*100<v->max_energy*45){
                /* Preserve the established low-health Marine stun tactic
                 * in co-op. Do not let the new general Bite preference
                 * undo the working cocoon/combat survival path. */
                weapon=v->target_is_stunned?AVP_MP_ALIEN_CLAW:AVP_MP_ALIEN_TAIL;
            }else if(v->deathmatch || v->goal!=AVP_MP_BOT_COCOON){
                /* The original Alien has three distinct FIRE inputs. Keep
                 * each choice across an attack-sized window: Bite 60%%,
                 * Tail 20%%, Claw 20%%, in co-op and Deathmatch alike.
                 * Source stun/cocoon logic above
                 * remains unchanged for its special Marine objective. */
                if(!state->alien_attack_until ||
                   v->tick>=state->alien_attack_until){
                    uint32_t seed=(uint32_t)v->tick*0x9e3779b9u ^
                        (v->player_index+1u)*0x7f4a7c15u ^
                        (v->target_slot+1u)*0x6c8e9cf5u;
                    seed^=seed>>16;seed*=0x7feb352du;
                    seed^=seed>>15;seed*=0x846ca68bu;
                    seed^=seed>>16;
                    state->alien_attack_choice=seed%10u;
                    state->alien_attack_until=v->tick+12u+(seed%7u);
                }
                weapon=state->alien_attack_choice<6u?AVP_MP_ALIEN_BITE:
                    state->alien_attack_choice<8u?AVP_MP_ALIEN_TAIL:
                    AVP_MP_ALIEN_CLAW;
            }else if(v->target_is_marine && !v->target_is_stunned &&
                     v->max_energy>0 && v->player_energy*100<v->max_energy*45)
                weapon=AVP_MP_ALIEN_TAIL;
            else weapon=AVP_MP_ALIEN_CLAW;
        }
        out->actions|=1u<<weapon;
    }
    /* Only actual translation requests trigger next-tick collision recovery. */
    state->last_motion=out->actions &
       ((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
        (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT));
    /* No pause/menu/cheat/host-only actions are synthesized. */
}
