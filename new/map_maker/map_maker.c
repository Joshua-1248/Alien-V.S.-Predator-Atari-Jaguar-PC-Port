#include "map_maker.h"
#include "map_maker_scripts.h"
#include "map_maker_objects.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void put16(u8 *p,unsigned v){p[0]=(u8)(v>>8);p[1]=(u8)v;}
static unsigned get16(const u8 *p){return ((unsigned)p[0]<<8)|p[1];}
static void put32(u8 *p,u32 v){p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;}
static u32 get32(const u8*p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}
static u32 crc32_bytes(const u8 *p,size_t n)
{
    u32 crc=~(u32)0;
    for(size_t i=0;i<n;i++){
        crc^=p[i];
        for(int j=0;j<8;j++)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return ~crc;
}
static void msg(AvpMapMaker *m,const char*s){snprintf(m->message,sizeof m->message,"%s",s);}
/* Editor-only undo history. Snapshots contain only the authored draft,
 * never borrowed retail-ROM pointers, camera state, keyboard bindings, or
 * live simulation data. Holding the mouse begins one transaction and release
 * commits that entire stroke; unchanged commands never consume a slot. */
#define AVP_MM_UNDO_DEPTH 24u
typedef struct AvpMmEditSnapshot {
    unsigned level;
    u16 w,h;
    int custom_map,dirty;
    u8 source_header[8];
    u8 cells[AVP_MM_MAX_BYTES];
    AvpMmScriptRow scripts[AVP_MM_MAX_SCRIPTS];
    unsigned script_count,script_modified;
    u32 script_source_crc;
    AvpMmObjectRecord objects[AVP_MM_MAX_OBJECTS];
    unsigned object_count;
} AvpMmEditSnapshot;
struct AvpMmHistory {
    AvpMmEditSnapshot *undo[AVP_MM_UNDO_DEPTH];
    AvpMmEditSnapshot *redo[AVP_MM_UNDO_DEPTH];
    unsigned undo_count,redo_count;
    AvpMmEditSnapshot *pending;
};
static AvpMmEditSnapshot *mm_history_capture(const AvpMapMaker *m)
{
    AvpMmEditSnapshot *s;
    if(!m||!m->w||!m->h)return NULL;
    s=calloc(1,sizeof *s);
    if(!s)return NULL;
    s->level=m->level;s->w=m->w;s->h=m->h;
    s->custom_map=m->custom_map;s->dirty=m->dirty;
    memcpy(s->source_header,m->source_header,sizeof s->source_header);
    memcpy(s->cells,m->cells,sizeof s->cells);
    s->script_count=m->script_count;s->script_modified=m->script_modified;
    s->script_source_crc=m->script_source_crc;
    if(s->script_count>AVP_MM_MAX_SCRIPTS){free(s);return NULL;}
    memcpy(s->scripts,m->script_rows,s->script_count*sizeof s->scripts[0]);
    s->object_count=m->object_count;
    if(s->object_count>AVP_MM_MAX_OBJECTS){free(s);return NULL;}
    memcpy(s->objects,m->objects,s->object_count*sizeof s->objects[0]);
    return s;
}
static int mm_history_same(const AvpMmEditSnapshot *s,const AvpMapMaker *m)
{
    if(!s||!m||s->level!=m->level||s->w!=m->w||s->h!=m->h||
       s->custom_map!=m->custom_map||s->script_count!=m->script_count||
       s->script_modified!=m->script_modified||
       s->script_source_crc!=m->script_source_crc||
       s->object_count!=m->object_count||
       memcmp(s->source_header,m->source_header,sizeof s->source_header)||
       memcmp(s->cells,m->cells,sizeof s->cells))return 0;
    if(s->script_count && memcmp(s->scripts,m->script_rows,
                        s->script_count*sizeof s->scripts[0]))return 0;
    if(s->object_count && memcmp(s->objects,m->objects,
                        s->object_count*sizeof s->objects[0]))return 0;
    return 1;
}
static void mm_history_stack_push(AvpMmEditSnapshot **stack,unsigned *n,
                                  AvpMmEditSnapshot *s)
{
    if(*n==AVP_MM_UNDO_DEPTH){
        free(stack[0]);
        memmove(stack,stack+1,(AVP_MM_UNDO_DEPTH-1u)*sizeof stack[0]);
        --*n;
    }
    stack[(*n)++]=s;
}
static void mm_history_stack_clear(AvpMmEditSnapshot **stack,unsigned *n)
{
    while(*n)free(stack[--*n]);
}
void avp_mm_history_reset(AvpMapMaker *m)
{
    struct AvpMmHistory *h;
    if(!m||(h=m->edit_history)==NULL)return;
    free(h->pending);h->pending=NULL;
    mm_history_stack_clear(h->undo,&h->undo_count);
    mm_history_stack_clear(h->redo,&h->redo_count);
}
void avp_mm_history_begin(AvpMapMaker *m)
{
    struct AvpMmHistory *h;
    if(!m||!m->w||!m->h)return;
    if(!m->edit_history){
        m->edit_history=calloc(1,sizeof *m->edit_history);
        if(!m->edit_history){msg(m,"Undo history unavailable (out of memory).");return;}
    }
    h=m->edit_history;
    if(!h->pending)h->pending=mm_history_capture(m);
}
void avp_mm_history_commit(AvpMapMaker *m)
{
    struct AvpMmHistory *h;
    AvpMmEditSnapshot *before;
    if(!m||(h=m->edit_history)==NULL||!h->pending)return;
    before=h->pending;h->pending=NULL;
    if(mm_history_same(before,m)){free(before);return;}
    /* A level or map load always starts a new history. Never replay across
     * a source change, even if a UI command accidentally wrapped that load. */
    if(before->level!=m->level||before->w!=m->w||before->h!=m->h||
       before->custom_map!=m->custom_map){
        free(before);avp_mm_history_reset(m);return;
    }
    mm_history_stack_push(h->undo,&h->undo_count,before);
    mm_history_stack_clear(h->redo,&h->redo_count);
}
static int mm_history_restore(AvpMapMaker *m,const AvpMmEditSnapshot *s)
{
    if(!m||!s||s->level!=m->level||s->w!=m->w||s->h!=m->h||
       s->script_count>AVP_MM_MAX_SCRIPTS||s->object_count>AVP_MM_MAX_OBJECTS)return 0;
    memcpy(m->source_header,s->source_header,sizeof m->source_header);
    memcpy(m->cells,s->cells,sizeof m->cells);
    m->script_count=s->script_count;m->script_modified=s->script_modified;
    m->script_source_crc=s->script_source_crc;
    memcpy(m->script_rows,s->scripts,s->script_count*sizeof s->scripts[0]);
    m->object_count=s->object_count;
    memset(m->selected_objects,0,sizeof m->selected_objects);
    memcpy(m->objects,s->objects,s->object_count*sizeof s->objects[0]);
    m->custom_map=s->custom_map;
    /* Conservative after Save: even a restored state needs another Save. */
    m->dirty=1;
    m->wall_last_valid=0;m->wall_drag_lock=0;m->wall_hover_valid=0;
    return 1;
}
static int mm_history_step(AvpMapMaker *m,int redo)
{
    struct AvpMmHistory *h;
    AvpMmEditSnapshot **source,**dest,*next,*current;
    unsigned *source_count,*dest_count;
    if(!m||(h=m->edit_history)==NULL)return 0;
    /* Finish an in-flight stroke before reversing it. */
    avp_mm_history_commit(m);
    source=redo?h->redo:h->undo;source_count=redo?&h->redo_count:&h->undo_count;
    dest=redo?h->undo:h->redo;dest_count=redo?&h->undo_count:&h->redo_count;
    if(!*source_count){msg(m,redo?"Nothing to redo.":"Nothing to undo.");return 0;}
    next=source[*source_count-1u];
    current=mm_history_capture(m);
    if(!current){msg(m,"Undo/redo unavailable (out of memory).");return 0;}
    if(!mm_history_restore(m,next)){free(current);msg(m,"Undo history does not match this map.");return 0;}
    --*source_count;free(next);
    mm_history_stack_push(dest,dest_count,current);
    msg(m,redo?"Redone. Ctrl+Z to undo.":"Undone. Ctrl+Y to redo.");
    return 1;
}
int avp_mm_undo(AvpMapMaker *m){return mm_history_step(m,0);}
int avp_mm_redo(AvpMapMaker *m){return mm_history_step(m,1);}
const char *avp_mm_name(unsigned level)
{
    static const char *names[]={"No level","Sublevel 1","Sublevel 2","Sublevel 3","Sublevel 4","Sublevel 5",
        "Airduct 1B","Airduct 2B","Airduct 3B","Airduct 3C","Airduct 4B","Airduct 4C",
        "Airduct 5B","Airduct 5C","Alien Ship","Predator Ship"};
    return level<=AVP_MM_LEVELS?names[level]:"Unknown level";
}
unsigned avp_mm_panel_type(u8 panel)
{
    return ((unsigned)panel>>3u)&15u;
}
const char *avp_mm_panel_kind(u8 panel)
{
    unsigned type=avp_mm_panel_type(panel);
    /* Use the exact native 16-type split, not a guess based on artwork.
     * Type 12 is the MAZE2 unrendered handler; it must not be presented
     * as a working opening.  The *appearance* of types 3 and 13..15 is
     * determined by the current level's actual retail panel resources. */
    if(!(panel&0x7fu))return "Empty surface";
    if(type<=2u)return "Solid wall";
    if(type==3u)return "Shaped solid wall";
    if(type<=11u)return "Door panel";
    if(type==12u)return "Unrendered/unused";
    return "Partial/strip panel";
}
int avp_mm_palette_category_start(unsigned field,unsigned key,unsigned *id)
{
    unsigned selected;
    if(!id||field>=4u)return 0;
    switch(key){
    case 'w':case 'W':selected=1u;break;   /* solid wall type 0 */
    case 's':case 'S':selected=24u;break;  /* shaped solid type 3 */
    case 'd':case 'D':selected=32u;break;  /* first door type 4 */
    case 'a':case 'A':selected=104u;break; /* partial/strip type 13 */
    default:return 0;
    }
    *id=selected;
    return 1;
}
void avp_mm_init(AvpMapMaker *m){if(m){memset(m,0,sizeof *m);m->level=1u;m->object_def=21u;m->object_group=3u;m->overhead_zoom=1u;m->overhead_zoom_visual=1.0;m->wall_brush=1u;m->texture_brush[0]=1u;m->texture_brush[1]=1u;m->texture_brush[2]=1u;m->highlight_3d=1;
    snprintf(m->map_stem,sizeof m->map_stem,"saved_map");}}
void avp_mm_destroy(AvpMapMaker *m)
{
    if(!m)return;
    avp_mm_history_reset(m);free(m->edit_history);m->edit_history=NULL;
    for(unsigned i=1;i<=AVP_MM_LEVELS;i++){
        free(m->source[i].original);m->source[i].original=NULL;
        m->source[i].w=m->source[i].h=0;
    }
}
int avp_mm_bind(AvpMapMaker *m,unsigned level,const u8 *decoded,size_t len)
{
    u8 *p;unsigned w,h;size_t expected;
    if(!m||!decoded||level<1u||level>AVP_MM_LEVELS||len<8u)return 0;
    w=get16(decoded);h=get16(decoded+2);
    if(!w||!h||w>64u||h>64u)return 0;
    expected=8u+(size_t)w*h*8u;
    if(len<expected)return 0;
    p=malloc(expected);
    if(!p)return 0;
    memcpy(p,decoded,expected);
    if(m->level==level)avp_mm_history_reset(m);
    free(m->source[level].original);
    m->source[level].original=p;
    m->source[level].w=(u16)w;m->source[level].h=(u16)h;
    m->rom_loaded=1;
    return 1;
}
int avp_mm_select(AvpMapMaker *m,unsigned level)
{
    const AvpMapMakerSource *s;
    if(!m||level<1u||level>AVP_MM_LEVELS)return 0;
    avp_mm_history_reset(m);
    m->level=level;m->x=m->y=m->field=0;m->dirty=0;m->object_count=0;m->custom_map=0;
    m->script_count=0u;m->script_modified=0u;m->script_source_crc=0u;
    m->wall_drag_lock=0;m->wall_last_valid=0;m->wall_hover_valid=0;m->selected_count=0;memset(m->selected_objects,0,sizeof m->selected_objects);m->selection_2d_valid=0;
    s=&m->source[level];
    if(s->original){
        m->w=s->w;m->h=s->h;
        memcpy(m->source_header,s->original,8u);
        memcpy(m->cells,s->original+8u,(size_t)m->w*m->h*8u);
        if(m->w==64u && m->h==64u){
            m->script_count=avp_mm_script_copy_level(level,m->script_rows,AVP_MM_MAX_SCRIPTS);
            m->script_source_crc=crc32_bytes(s->original,8u+64u*64u*8u);
        }
        msg(m,"Original ROM map copied into private editor buffer.");
    }else{
        (void)avp_mm_blank(m);
        m->dirty=0;
        msg(m,"Blank staging map. Load verified ROM to inspect retail maps.");
    }
    /* Place the standalone editor camera inside an authored floor cell. */
    {unsigned cx=m->w/2u,cy=m->h/2u;
     if(!m->cells[((size_t)cy*m->w+cx)*8u+5u]){
       int found=0;
       for(unsigned y=0;y<m->h&&!found;y++)for(unsigned x=0;x<m->w;x++)
         if(m->cells[((size_t)y*m->w+x)*8u+5u]){cx=x;cy=y;found=1;break;}
     }
     m->camera_x=(double)cx+0.5;m->camera_y=(double)cy+0.5;
     m->camera_heading=0.0;m->cursor_u=m->cursor_v=0.5;
     m->target_x=cx;m->target_y=cy;m->target_field=0;
     m->target_valid=0;m->view_ray_scale=1.0;
    }
    return 1;
}
int avp_mm_blank(AvpMapMaker *m)
{
    if(!m||m->level<1u||m->level>AVP_MM_LEVELS)return 0;
    avp_mm_history_reset(m);
    m->w=m->h=64u;m->x=m->y=m->field=0u;m->object_count=0u;
    memset(m->source_header,0,sizeof m->source_header);
    put16(m->source_header,m->w);put16(m->source_header+2,m->h);
    put16(m->source_header+4,2u);
    memset(m->cells,0,sizeof m->cells);
    /* An empty authoring canvas really has no cells, walls or flat textures.
     * Enter creates the first floor. The Jaguar campaign is never edited. */
    m->x=m->w/2u;m->y=m->h/2u;m->field=5u;
    m->camera_x=(double)m->x+0.5;m->camera_y=(double)m->y+0.5;
    m->target_valid=0;m->camera_pitch=0.0;
    m->dirty=0;m->custom_map=1;m->script_count=0u;m->script_modified=0u;
    m->script_source_crc=0u;m->wall_drag_lock=0;m->wall_last_valid=0;m->wall_hover_valid=0;m->selected_count=0;memset(m->selected_objects,0,sizeof m->selected_objects);m->selection_2d_valid=0;
    msg(m,"Empty map. Enter: floor; M: choose edit mode; Esc: menu.");
    return 1;
}
void avp_mm_move(AvpMapMaker *m,int dx,int dy)
{
    int x,y;
    if(!m||!m->w||!m->h)return;
    x=(int)m->x+dx;y=(int)m->y+dy;
    if(x<0)x=0;
    if(y<0)y=0;
    if(x>=(int)m->w)x=m->w-1;
    if(y>=(int)m->h)y=m->h-1;
    m->x=(unsigned)x;m->y=(unsigned)y;
    if(!m->mode_3d){m->selection_2d_valid=1;m->selection_2d_range=0;}
}
void avp_mm_extend_2d_selection(AvpMapMaker *m,int dx,int dy)
{
    int x,y;
    if(!m||m->mode_3d||!m->w||!m->h)return;
    /* Keep the anchor fixed for every repeated Shift+Arrow, including
     * direction reversals and clipping at the map boundary. Do not call
     * avp_mm_move(): ordinary cursor movement intentionally clears ranges. */
    if(!m->selection_2d_range){
        m->select_anchor_x=m->x;m->select_anchor_y=m->y;
        m->selection_2d_range=1;
    }
    x=(int)m->x+dx;y=(int)m->y+dy;
    if(x<0)x=0;
    if(y<0)y=0;
    if(x>=(int)m->w)x=(int)m->w-1;
    if(y>=(int)m->h)y=(int)m->h-1;
    m->x=(unsigned)x;m->y=(unsigned)y;
    m->selection_2d_valid=1;
}
int avp_mm_begin_2d_drag_selection(AvpMapMaker *m,unsigned x,unsigned y)
{
    if(!m||m->mode_3d||!m->w||!m->h||x>=m->w||y>=m->h)return 0;
    m->select_anchor_x=x;m->select_anchor_y=y;
    m->x=x;m->y=y;
    m->selection_2d_valid=1;m->selection_2d_range=1;
    m->wall_hover_valid=0;
    return 1;
}
int avp_mm_update_2d_drag_selection(AvpMapMaker *m,unsigned x,unsigned y)
{
    if(!m||m->mode_3d||!m->selection_2d_valid||!m->selection_2d_range||
       x>=m->w||y>=m->h)return 0;
    /* Only move the opposite corner; never reset the anchor, place a wall,
     * paint a cell, alter an object or make a history entry. */
    m->x=x;m->y=y;
    return 1;
}
int avp_mm_in_2d_selection(const AvpMapMaker *m,unsigned x,unsigned y)
{
    unsigned xmin,xmax,ymin,ymax;
    /* A cell-range selection persists across 2D/3D; geometry isn't changed. */
    if(!m||!m->selection_2d_valid||x>=m->w||y>=m->h)return 0;
    if(!m->selection_2d_range)return x==m->x&&y==m->y;
    xmin=m->x<m->select_anchor_x?m->x:m->select_anchor_x;
    xmax=m->x>m->select_anchor_x?m->x:m->select_anchor_x;
    ymin=m->y<m->select_anchor_y?m->y:m->select_anchor_y;
    ymax=m->y>m->select_anchor_y?m->y:m->select_anchor_y;
    return x>=xmin&&x<=xmax&&y>=ymin&&y<=ymax;
}
u8 avp_mm_value(const AvpMapMaker *m)
{
    if(!m||!m->w||!m->h||m->x>=m->w||m->y>=m->h||m->field>=8u)return 0;
    return m->cells[((size_t)m->y*m->w+m->x)*8u+m->field];
}
/* Retail LEVDOOR.S offsets are relative to the 64*64 eight-byte maze
 * array, not the eight-byte decoded maze header.  Only classify drafts
 * actually bound to an original retail map: never apply retail rules to a
 * blank canvas or to a size-mismatched project. */
static int mm_script_source(const AvpMapMaker *m)
{
    return m && !m->custom_map && m->level>=1u && m->level<=AVP_MM_LEVELS &&
        m->w==64u && m->h==64u && m->source[m->level].original &&
        m->source[m->level].w==64u && m->source[m->level].h==64u &&
        m->script_count>0u && m->script_count<=AVP_MM_MAX_SCRIPTS;
}
static unsigned mm_script_offsets(const AvpMapMaker *m,unsigned x,unsigned y,
                                  unsigned side,unsigned *panel,unsigned *state)
{
    unsigned ox=x,oy=y,state_field;
    if(!mm_script_source(m)||x>=m->w||y>=m->h||side>=4u)return 0;
    *panel=((y*m->w+x)*8u+side);
    if(side==2u){ox++;state_field=6u;}
    else if(side==3u){oy++;state_field=7u;}
    else state_field=side==0u?6u:7u;
    *state=ox<m->w && oy<m->h ? ((oy*m->w+ox)*8u+state_field):65536u;
    return 1;
}
unsigned avp_mm_script_flags(const AvpMapMaker *m,unsigned x,unsigned y,unsigned side)
{
    unsigned panel,state,flags=0u;AvpMmScriptInfo info;
    if(!mm_script_source(m)||x>=m->w||y>=m->h)return 0;
    if(side==6u||side==7u){
        return avp_mm_script_lookup(m->script_rows,m->script_count,m->level,(y*m->w+x)*8u+side,&info)?info.flags:0u;
    }
    if(!mm_script_offsets(m,x,y,side,&panel,&state))return 0;
    if(avp_mm_script_lookup(m->script_rows,m->script_count,m->level,panel,&info))flags|=info.flags;
    if(state!=65536u && avp_mm_script_lookup(m->script_rows,m->script_count,m->level,state,&info))flags|=info.flags;
    return flags;
}
/* An EXIT can be attached to the opposite cell's panel byte, even when
 * this cell's own canonical door-state byte has no script. Treat that
 * physical edge as protected without transferring the opposite anchor. */
static unsigned mm_opposite_panel_script(const AvpMapMaker *m,
                                         unsigned x,unsigned y,unsigned side)
{
    unsigned nx=x,ny=y,other;AvpMmScriptInfo info;
    if(!mm_script_source(m)||x>=m->w||y>=m->h||side>=4u)return 0;
    switch(side){
    case 0u:if(x==0u)return 0;nx--;other=2u;break;
    case 1u:if(y==0u)return 0;ny--;other=3u;break;
    case 2u:if(x+1u>=m->w)return 0;nx++;other=0u;break;
    default:if(y+1u>=m->h)return 0;ny++;other=1u;break;
    }
    return avp_mm_script_lookup(m->script_rows,m->script_count,m->level,
                  (ny*m->w+nx)*8u+other,&info)?info.flags:0u;
}
/* Editor query only: no changes to the original game metadata. */
int avp_mm_inspect_script(AvpMapMaker *m)
{
    unsigned x,y,side,panel,state;AvpMmScriptInfo a={0},b={0};
    char access[35]="",exit[40]="",inter[48]="";
    if(!m)return 0;
    x=m->mode_3d?m->target_x:m->x;
    y=m->mode_3d?m->target_y:m->y;
    side=m->mode_3d?m->target_field:m->field;
    if(m->mode_3d&&!m->target_valid){msg(m,"No targeted surface to inspect.");return 0;}
    if(mm_script_source(m)&&x<m->w&&y<m->h&&(side==6u||side==7u)){
        (void)avp_mm_script_lookup(m->script_rows,m->script_count,m->level,(y*m->w+x)*8u+side,&a);
    }else{
        if(!mm_script_offsets(m,x,y,side,&panel,&state)){
            msg(m,"No retail script index for this draft or surface.");return 0;
        }
        (void)avp_mm_script_lookup(m->script_rows,m->script_count,m->level,panel,&a);
        if(state!=65536u)(void)avp_mm_script_lookup(m->script_rows,m->script_count,m->level,state,&b);
    }
    if(!(a.flags|b.flags)){
        msg(m,"No retail scripted lock, exit or interlock on this edge.");return 1;
    }
    if((a.flags|b.flags)&AVP_MM_SCRIPT_ACCESS){
        s16 code=(a.flags&AVP_MM_SCRIPT_ACCESS)?a.access_code:b.access_code;
        if(code>=0)snprintf(access,sizeof access,"Access %d ",(int)code);
        else snprintf(access,sizeof access,"Access %s ",code==-1?"unused":code==-2?"airlock":
                      code==-3?"jammed":"escape");
    }
    if(a.flags&AVP_MM_SCRIPT_EXIT)
        snprintf(exit,sizeof exit,"Exit L%u (%u,%u) ",a.destination_level,
                 a.destination_x,a.destination_y);
    if((a.flags|b.flags)&AVP_MM_SCRIPT_INTERLOCK)
        snprintf(inter,sizeof inter,"Interlock ");
    if((a.flags|b.flags)&AVP_MM_SCRIPT_REQUIRED)
        snprintf(inter,sizeof inter,"Required by interlock @%u ",
                 (a.flags&AVP_MM_SCRIPT_REQUIRED)?a.interlock_required_by:b.interlock_required_by);
    snprintf(m->message,sizeof m->message,"%s SCRIPT: %.25s%.30s%.30s",
             m->script_modified?"DRAFT":"RETAIL",access,exit,inter);
    return 1;
}

int avp_mm_assign(AvpMapMaker *m,u8 value)
{
    if(!m||!m->w||!m->h||m->x>=m->w||m->y>=m->h||m->field>=8u)return 0;
    u8 *p=&m->cells[((size_t)m->y*m->w+m->x)*8u+m->field];
    if(*p!=value && m->field<4u &&
       (avp_mm_script_flags(m,m->x,m->y,m->field)||
        mm_opposite_panel_script(m,m->x,m->y,m->field))){
        msg(m,"Scripted edge protected. Use Shift+,/. to rotate or K to inspect.");
        return 0;
    }
    if(*p!=value){*p=value;m->dirty=1;msg(m,"Unapplied edit. Press S to save map file.");}
    return 1;
}
int avp_mm_adjust(AvpMapMaker *m,int delta)
{
    if(!m||!delta)return 0;
    int value=(int)avp_mm_value(m)+delta;
    if(value<0)value=0;
    if(value>255)value=255;
    return avp_mm_assign(m,(u8)value);
}
/* AVPMAP01 remains byte-for-byte backward compatible. AVPMAP02 is used
 * only after explicit scripted relocation. Both are PC drafts, not Jaguar
 * FILES.DAT. The v2 suffix is R26M + CRC32 of ORIGINAL bound retail map,
 * BE16 row count + 12-byte encoded rows + BE32 CRC32 of the entire suffix
 * up to (but excluding) this checksum. The old base header CRC is retained. */
static void mm_encode_row(u8 *dst,const AvpMmScriptRow *r)
{
    dst[0]=r->level;dst[1]=r->kind;
    put16(dst+2,r->offset);put16(dst+4,(u16)r->arg);
    put16(dst+6,r->destination_x);put16(dst+8,r->destination_y);
    put16(dst+10,r->destination_level);
}
static void mm_decode_row(AvpMmScriptRow *r,const u8 *src)
{
    r->level=src[0];r->kind=src[1];r->offset=(u16)get16(src+2);
    r->arg=(s16)(u16)get16(src+4);
    r->destination_x=(u16)get16(src+6);
    r->destination_y=(u16)get16(src+8);
    r->destination_level=(u16)get16(src+10);
}
static u32 mm_source_fingerprint(const AvpMapMaker *m,unsigned level)
{
    const AvpMapMakerSource *s;
    if(!m||level<1u||level>AVP_MM_LEVELS)return 0;
    s=&m->source[level];
    if(!s->original||s->w!=64u||s->h!=64u)return 0;
    return crc32_bytes(s->original,8u+64u*64u*8u);
}
int avp_mm_save(AvpMapMaker *m,const char *path)
{
    u8 header[20],suffix[10u+12u*AVP_MM_MAX_SCRIPTS+4u];
    u32 crc;FILE *f;size_t bytes,extra=0u;
    int scripted;
    if(!m||!path||!m->w||!m->h||m->level<1u||m->level>AVP_MM_LEVELS)return 0;
    scripted=m->script_modified!=0u;
    if(scripted){
        if(!mm_script_source(m)||
           m->script_source_crc!=mm_source_fingerprint(m,m->level)||
           !avp_mm_script_validate(m->script_rows,m->script_count,m->level)){
            msg(m,"Script overlay invalid; save rejected.");return 0;
        }
        memcpy(suffix,"R26M",4u);
        put32(suffix+4,mm_source_fingerprint(m,m->level));
        put16(suffix+8,m->script_count);
        extra=10u+(size_t)m->script_count*12u;
        for(unsigned i=0;i<m->script_count;i++)
            mm_encode_row(suffix+10u+(size_t)i*12u,&m->script_rows[i]);
        put32(suffix+extra,crc32_bytes(suffix,extra));
        extra+=4u;
    }
    bytes=(size_t)m->w*m->h*8u;
    crc=crc32_bytes(m->source_header,8u)^crc32_bytes(m->cells,bytes);
    memcpy(header,scripted?"AVPMAP02":"AVPMAP01",8u);
    put16(header+8,m->level);put16(header+10,m->w);
    put16(header+12,m->h);put16(header+14,m->custom_map?1u:0u);put32(header+16,crc);
    f=fopen(path,"wb");if(!f){msg(m,"Save failed (file could not be opened).");return 0;}
    int ok=fwrite(header,1,sizeof header,f)==sizeof header &&
           fwrite(m->source_header,1,8u,f)==8u &&
           fwrite(m->cells,1,bytes,f)==bytes &&
           (!extra||fwrite(suffix,1,extra,f)==extra);
    if(fclose(f)!=0)ok=0;
    if(ok){m->dirty=0;msg(m,"Saved draft. Scripted doors playtest in an isolated game.");}
    else msg(m,"Save failed (incomplete file).");
    return ok;
}
int avp_mm_load(AvpMapMaker *m,const char *path)
{
    u8 header[20],original[8],cells[AVP_MM_MAX_BYTES];
    u8 suffix[10u+12u*AVP_MM_MAX_SCRIPTS+4u];
    AvpMmScriptRow rows[AVP_MM_MAX_SCRIPTS];
    unsigned level=0,w=0,h=0,count=0;size_t bytes,extra=0u;
    u32 expected;FILE*f;int v2=0;
    if(!m||!path)return 0;
    f=fopen(path,"rb");if(!f){msg(m,"No saved map draft found for this level.");return 0;}
    int ok=fread(header,1,sizeof header,f)==sizeof header;
    if(ok){
        v2=!memcmp(header,"AVPMAP02",8u);
        ok=v2||!memcmp(header,"AVPMAP01",8u);
    }
    if(ok){
        level=get16(header+8);w=get16(header+10);h=get16(header+12);
        ok=level==m->level && w>0&&h>0&&w<=64u&&h<=64u&&get16(header+14)<=1u;
    }
    if(ok){
        bytes=(size_t)w*h*8u;expected=get32(header+16);
        ok=fread(original,1,8u,f)==8u && fread(cells,1,bytes,f)==bytes &&
           get16(original)==w && get16(original+2)==h &&
           (crc32_bytes(original,8u)^crc32_bytes(cells,bytes))==expected;
    }
    if(ok&&v2){
        /* An overlay must be backed by the very same original ROM map.
         * Do not silently transplant scripts into a different retail map. */
        ok=get16(header+14)==0u && w==64u && h==64u &&
           m->source[level].original!=NULL &&
           m->source[level].w==64u && m->source[level].h==64u &&
           fread(suffix,1,10u,f)==10u && !memcmp(suffix,"R26M",4u) &&
           get32(suffix+4)==mm_source_fingerprint(m,level);
        if(ok){
            count=get16(suffix+8);
            ok=count>0u && count<=AVP_MM_MAX_SCRIPTS;
        }
        if(ok){
            extra=10u+(size_t)count*12u;
            ok=fread(suffix+10u,1,extra-10u+4u,f)==extra-10u+4u &&
               crc32_bytes(suffix,extra)==get32(suffix+extra);
        }
        if(ok){
            for(unsigned i=0;i<count;i++)mm_decode_row(&rows[i],suffix+10u+(size_t)i*12u);
            ok=avp_mm_script_validate(rows,count,level);
        }
    }
    if(ok)ok=fgetc(f)==EOF;
    if(fclose(f)!=0)ok=0;
    if(!ok){msg(m,"Invalid draft, script fingerprint or checksum; unchanged.");return 0;}
    avp_mm_history_reset(m);
    m->w=(u16)w;m->h=(u16)h;m->x=m->y=m->field=0u;
    memcpy(m->source_header,original,8u);memcpy(m->cells,cells,(size_t)w*h*8u);
    m->target_valid=0;m->selected_count=0;
    memset(m->selected_objects,0,sizeof m->selected_objects);
    if(m->camera_x<0.1||m->camera_x>=(double)m->w-0.1)m->camera_x=(double)m->w/2.0;
    if(m->camera_y<0.1||m->camera_y>=(double)m->h-0.1)m->camera_y=(double)m->h/2.0;
    m->dirty=0;m->custom_map=get16(header+14)==1u;
    m->script_count=0u;m->script_modified=0u;m->script_source_crc=0u;
    if(v2){
        memcpy(m->script_rows,rows,count*sizeof rows[0]);
        m->script_count=count;m->script_modified=1u;
        m->script_source_crc=mm_source_fingerprint(m,level);
    }else if(!m->custom_map&&m->w==64u&&m->h==64u&&
            m->source[level].original!=NULL &&
            m->source[level].w==64u && m->source[level].h==64u){
        m->script_count=avp_mm_script_copy_level(level,m->script_rows,AVP_MM_MAX_SCRIPTS);
        m->script_source_crc=mm_source_fingerprint(m,level);
    }
    msg(m,v2?"Loaded draft script overlay (isolated Play Test supported).":
             "Loaded saved map draft. Gameplay remains unchanged.");
    return 1;
}

/* BUILD-style editing uses an independent editor camera; never changes the
 * original Jaguar player position or live maze. Ray/grid picking is strictly
 * bounded and returns a field of the staged eight-byte cell. */
static int mm_walkable(const AvpMapMaker *m,double x,double y)
{
    if(!m || !m->w || !m->h || !isfinite(x) || !isfinite(y) ||
       x<0.1 || y<0.1 || x>=(double)m->w-0.1 || y>=(double)m->h-0.1)return 0;
    return m->cells[((size_t)(unsigned)y*m->w+(unsigned)x)*8u+5u]!=0u;
}
void avp_mm_toggle_3d(AvpMapMaker *m)
{
    if(!m || !m->w || !m->h)return;
    if(!m->mode_3d){
        if(!mm_walkable(m,m->camera_x,m->camera_y)){
            m->camera_x=(double)m->x+0.5;
            m->camera_y=(double)m->y+0.5;
        }
        m->mode_3d=1;
        m->highlight_3d=1;
        msg(m,"3D: left-click selects, Shift+click adds, wheel changes texture.");
    }else{m->mode_3d=0;
        snprintf(m->message,sizeof m->message,"2D %s: M changes mode; Delete removes selected type.",avp_mm_2d_mode_name(m));}
}
void avp_mm_camera_motion(AvpMapMaker *m,double forward,double strafe,double turn)
{
    double nx,ny,fx,fy,rx,ry;
    if(!m||!m->mode_3d||!m->w||!m->h)return;
    if(!isfinite(forward)||!isfinite(strafe)||!isfinite(turn))return;
    if(forward>0.20)forward=0.20;
    if(forward< -0.20)forward=-0.20;
    if(strafe>0.20)strafe=0.20;
    if(strafe< -0.20)strafe=-0.20;
    if(turn>0.35)turn=0.35;
    if(turn< -0.35)turn=-0.35;
    m->camera_heading+=turn;
    if(m->camera_heading>6.283185307179586)m->camera_heading-=6.283185307179586;
    if(m->camera_heading< -6.283185307179586)m->camera_heading+=6.283185307179586;
    fx=cos(m->camera_heading);fy=sin(m->camera_heading);rx=-fy;ry=fx;
    nx=m->camera_x+forward*fx+strafe*rx;
    ny=m->camera_y+forward*fy+strafe*ry;
    /* Collision against the draft only; allow sliding on X/Y axes. */
    if(mm_walkable(m,nx,m->camera_y))m->camera_x=nx;
    if(mm_walkable(m,m->camera_x,ny))m->camera_y=ny;
}
void avp_mm_toggle_ghost_walls(AvpMapMaker *m)
{
    if(!m)return;
    m->ghost_walls=!m->ghost_walls;
    msg(m,m->ghost_walls?"Ghost walls ON: point at an empty boundary to add a wall.":
                          "Ghost walls OFF: point at visible wall panels.");
    (void)avp_mm_pick(m,m->cursor_u,m->cursor_v);
}
void avp_mm_toggle_highlight(AvpMapMaker *m)
{
    if(!m||!m->mode_3d)return;
    m->highlight_3d=!m->highlight_3d;
    msg(m,m->highlight_3d?"3D surface highlight ON (H to disable).":
                          "3D surface highlight OFF (H to enable).");
}
void avp_mm_toggle_overhead_surface(AvpMapMaker *m)
{
    if(!m)return;
    m->overhead_ceiling=!m->overhead_ceiling;
    msg(m,m->overhead_ceiling?"2D overhead: ceiling textures":"2D overhead: floor textures");
}
/* Traverse exact cell boundaries rather than testing arbitrary points within
 * 0.06 cells of an edge.  This also exposes empty wall slots in ghost mode:
 * the front-facing slot remains selectable after assigning texture ID 0. */
static int mm_pick_flat_at(AvpMapMaker *m,double dx,double dy,double depth,double pv)
{
    double x=m->camera_x+depth*dx,y=m->camera_y+depth*dy;
    if(!isfinite(x)||!isfinite(y)||x<0.0||y<0.0||
       x>=(double)m->w||y>=(double)m->h)return 0;
    m->target_x=(unsigned)x;m->target_y=(unsigned)y;
    m->target_field=pv<0.5?4u:5u;
    m->target_valid=1;
    return 1;
}
int avp_mm_pick(AvpMapMaker *m,double u,double v)
{
    double dx,dy,tx,ty,delta_x,delta_y,t;
    int cx,cy,sx,sy;
    if(!m||!m->w||!m->h||!isfinite(u)||!isfinite(v)||
       !isfinite(m->camera_x)||!isfinite(m->camera_y)||
       !isfinite(m->camera_heading)||!isfinite(m->view_ray_scale)||
       m->camera_x<0||m->camera_y<0||m->camera_x>=m->w||m->camera_y>=m->h)
        return 0;
    if(u<0)u=0;
    if(u>1)u=1;
    if(v<0)v=0;
    if(v>1)v=1;
    m->cursor_u=u;m->cursor_v=v;m->target_valid=0;
    dx=cos(m->camera_heading)-sin(m->camera_heading)*(u-0.5)*2.0*m->view_ray_scale;
    dy=sin(m->camera_heading)+cos(m->camera_heading)*(u-0.5)*2.0*m->view_ray_scale;
    if(!isfinite(dx)||!isfinite(dy))return 0;
    /* Match the highlight's image-plane projection: camera eye is halfway
     * between z +/-0.5; nearest wall or floor/ceiling intersection wins.
     * Previous fixed 0.38/0.62 bands selected the floor in front of walls. */
    double pv=v-m->camera_pitch*0.52;
    double plane_distance=INFINITY;
    if(fabs(pv-0.5)>1e-9)
        plane_distance=(0.5*128.0/228.0)/fabs(pv-0.5);
    cx=(int)m->camera_x;cy=(int)m->camera_y;
    sx=dx>=0?1:-1;sy=dy>=0?1:-1;
    delta_x=fabs(dx)>1e-12?fabs(1.0/dx):INFINITY;
    delta_y=fabs(dy)>1e-12?fabs(1.0/dy):INFINITY;
    tx=isfinite(delta_x)?((dx>0?(double)cx+1.0-m->camera_x:
                   m->camera_x-(double)cx)*delta_x):INFINITY;
    ty=isfinite(delta_y)?((dy>0?(double)cy+1.0-m->camera_y:
                   m->camera_y-(double)cy)*delta_y):INFINITY;
    for(unsigned n=0;n<256u;n++){
        int nx=cx,ny=cy;
        const u8 *before,*after;
        unsigned near_field,far_field;
        if(tx<=ty){
            t=tx;tx+=delta_x;nx+=sx;
        }else{
            t=ty;ty+=delta_y;ny+=sy;
        }
        if(!isfinite(t))break;
        if(t>plane_distance)
            return mm_pick_flat_at(m,dx,dy,plane_distance,pv);
        if(t>41.0)break;
        near_field=nx!=cx?(sx>0?2u:0u):(sy>0?3u:1u);
        far_field=(near_field+2u)&3u;
        if(nx<0||ny<0||nx>=m->w||ny>=m->h){
            const u8 *border=m->cells+((size_t)cy*m->w+(unsigned)cx)*8u;
            if(border[near_field]||m->ghost_walls){
                m->target_x=(unsigned)cx;m->target_y=(unsigned)cy;
                m->target_field=near_field;m->target_valid=1;return 1;
            }
            break;
        }
        before=m->cells+((size_t)cy*m->w+(unsigned)cx)*8u;
        after=m->cells+((size_t)ny*m->w+(unsigned)nx)*8u;
        /* A physical boundary has two independent Jaguar face bytes.
         * Always select the face TOWARD the editor camera, even when only
         * the other cell has artwork: selecting after[far_field] here was
         * responsible for applying an edit to the back of the wall. */
        if(before[near_field]||after[far_field]||m->ghost_walls){
            m->target_x=(unsigned)cx;m->target_y=(unsigned)cy;
            m->target_field=near_field;m->target_valid=1;return 1;
        }
        cx=nx;cy=ny;
    }
    if(isfinite(plane_distance) && plane_distance<=41.0)
        return mm_pick_flat_at(m,dx,dy,plane_distance,pv);
    return 0;
}

void avp_mm_clear_selection(AvpMapMaker *m)
{
    if(m){m->selected_count=0u;memset(m->selected_objects,0,sizeof m->selected_objects);m->selection_2d_valid=0;m->selection_2d_range=0;}
}
int avp_mm_deselect_target(AvpMapMaker *m)
{
    if(!m||!m->mode_3d||!m->target_valid)return 0;
    for(unsigned i=0;i<m->selected_count;i++){
        const AvpMmSelectedFace *f=&m->selected[i];
        if(f->x==m->target_x&&f->y==m->target_y&&f->field==m->target_field){
            for(unsigned j=i+1u;j<m->selected_count;j++)m->selected[j-1u]=m->selected[j];
            --m->selected_count;
            m->target_valid=0; /* don't immediately draw the same outline */
            snprintf(m->message,sizeof m->message,"Wall face deselected (%u remain).",m->selected_count);
            return 1;
        }
    }
    return 0; /* right-click never edits or paints a texture */
}
int avp_mm_select_target_drag(AvpMapMaker *m)
{
    if(!m||!m->mode_3d||!m->target_valid||m->target_field>=6u||
       m->target_x>=m->w||m->target_y>=m->h)return 0;
    for(unsigned i=0;i<m->selected_count;i++){
        const AvpMmSelectedFace *f=&m->selected[i];
        if(f->x==m->target_x&&f->y==m->target_y&&f->field==m->target_field)
            return 1; /* A drag never toggles or deselects an already hit face. */
    }
    if(m->selected_count>=AVP_MM_SELECTION_MAX){
        msg(m,"128 faces selected; release Shift to finish.");return 0;
    }
    m->selected[m->selected_count++]=(AvpMmSelectedFace){
        (u16)m->target_x,(u16)m->target_y,(u8)m->target_field};
    snprintf(m->message,sizeof m->message,"%u faces selected (Shift+drag).",m->selected_count);
    return 1;
}
unsigned avp_mm_selected_object_count(const AvpMapMaker *m)
{
    unsigned n=0;
    if(m)for(unsigned i=0;i<m->object_count&&i<AVP_MM_MAX_OBJECTS;i++)
        n+=(m->selected_objects[i]!=0);
    return n;
}
int avp_mm_select_object(AvpMapMaker *m,unsigned index,int append,int toggle)
{
    if(!m||!m->mode_3d||index>=m->object_count||index>=AVP_MM_MAX_OBJECTS)return 0;
    if(!append){
        m->selected_count=0;
        memset(m->selected_objects,0,sizeof m->selected_objects);
    }
    if(toggle && append)m->selected_objects[index]^=1u;
    else m->selected_objects[index]=1u;
    snprintf(m->message,sizeof m->message,"%u object markers selected.",
             avp_mm_selected_object_count(m));
    return 1;
}
unsigned avp_mm_delete_selected_objects(AvpMapMaker *m)
{
    unsigned n=0;
    if(!m||!m->mode_3d)return 0;
    for(unsigned i=0;i<m->object_count;){
        if(m->selected_objects[i]){
            for(unsigned j=i+1u;j<m->object_count;j++){
                m->objects[j-1u]=m->objects[j];
                m->selected_objects[j-1u]=m->selected_objects[j];
            }
            --m->object_count;
            m->selected_objects[m->object_count]=0;
            ++n;
        }else ++i;
    }
    if(n){m->dirty=1;
        snprintf(m->message,sizeof m->message,"Removed %u selected object marker%s.",n,n==1u?"":"s");}
    return n;
}
int avp_mm_select_target(AvpMapMaker *m,int append)
{
    unsigned i;
    if(!m||!m->mode_3d)return 0;
    if(!m->target_valid){
        if(!append)avp_mm_clear_selection(m);
        return 0;
    }
    for(i=0;i<m->selected_count;i++){
        AvpMmSelectedFace f=m->selected[i];
        if(f.x==m->target_x&&f.y==m->target_y&&f.field==m->target_field)break;
    }
    if(!append){m->selected_count=0u;memset(m->selected_objects,0,sizeof m->selected_objects);}
    else if(i<m->selected_count){
        /* Shift+click an already-selected face to remove just that face. */
        for(unsigned j=i+1u;j<m->selected_count;j++)m->selected[j-1u]=m->selected[j];
        m->selected_count--;
        snprintf(m->message,sizeof m->message,"%u surfaces selected.",m->selected_count);
        return 1;
    }
    if(m->selected_count>=AVP_MM_SELECTION_MAX){
        msg(m,"Too many surfaces selected; release Shift to start a new set.");return 0;
    }
    m->selected[m->selected_count++]=(AvpMmSelectedFace){(u16)m->target_x,(u16)m->target_y,(u8)m->target_field};
    snprintf(m->message,sizeof m->message,"%u face%s selected (Shift+click to add/remove).",
             m->selected_count,m->selected_count==1u?"":"s");
    return 1;
}
/* Apply a 3D wheel/palette/mirror edit atomically across a multi-selection.
 * A scratch editor copy runs the unmodified Jaguar-byte guard for EVERY
 * selected face; a failed script guard rolls back the entire operation. */
static int mm_assign_target_one(AvpMapMaker *m,u8 value);
static int mm_apply_selected(AvpMapMaker *m,int delta,int adjust,u8 value)
{
    AvpMapMaker *copy;
    if(!m||!m->selected_count)return 0;
    copy=malloc(sizeof *copy);
    if(!copy){msg(m,"Unable to edit selected surfaces (out of memory).");return 0;}
    memcpy(copy,m,sizeof *copy);
    copy->selected_count=0u;
    for(unsigned i=0;i<m->selected_count;i++){
        AvpMmSelectedFace f=m->selected[i];
        int v;
        if(f.x>=copy->w||f.y>=copy->h||f.field>=6u){
            free(copy);msg(m,"Selection no longer belongs to this level.");return 0;
        }
        copy->target_x=f.x;copy->target_y=f.y;copy->target_field=f.field;copy->target_valid=1;
        v=copy->cells[((size_t)f.y*copy->w+f.x)*8u+f.field];
        if(adjust){v+=delta;if(v<0)v=0;if(v>255)v=255;}
        else v=value;
        if(!avp_mm_assign_target(copy,(u8)v)){
            snprintf(m->message,sizeof m->message,"Selected faces unchanged: %.84s",copy->message);
            free(copy);return 0;
        }
    }
    memcpy(m->cells,copy->cells,sizeof m->cells);
    m->dirty=copy->dirty;m->ghost_walls=copy->ghost_walls;
    snprintf(m->message,sizeof m->message,"Updated %u selected face%s.",m->selected_count,
             m->selected_count==1u?"":"s");
    free(copy);return 1;
}
static int mm_assign_target_one(AvpMapMaker *m,u8 value)
{
    unsigned x,y,field;int ok;
    if(!m||!m->target_valid||m->target_x>=m->w||m->target_y>=m->h||m->target_field>=8u)return 0;
    x=m->x;y=m->y;field=m->field;
    m->x=m->target_x;m->y=m->target_y;m->field=m->target_field;
    ok=avp_mm_assign(m,value);
    /* Ghost Walls is a viewing/selection preference, not a texture edit.
     * Scrolling a panel down to ID 0 must NEVER turn Ghost Walls on.
     * Explicit 3D face selections remain editable at ID 0 independently. */
    m->x=x;m->y=y;m->field=field;
    return ok;
}
int avp_mm_assign_target(AvpMapMaker *m,u8 value)
{
    if(m&&m->selected_count)return mm_apply_selected(m,0,0,value);
    return mm_assign_target_one(m,value);
}
int avp_mm_adjust_target(AvpMapMaker *m,int delta)
{
    int value;
    if(m&&m->selected_count&&delta)return mm_apply_selected(m,delta,1,0);
    if(!m||!delta||!m->target_valid||m->target_x>=m->w||m->target_y>=m->h||m->target_field>=8u)return 0;
    value=(int)m->cells[((size_t)m->target_y*m->w+m->target_x)*8u+m->target_field]+delta;
    if(value<0)value=0;
    if(value>255)value=255;
    return avp_mm_assign_target(m,(u8)value);
}

int avp_mm_mirror_target(AvpMapMaker *m)
{
    if(!m||!m->mode_3d)return 0;
    if(m->selected_count){
        AvpMmSelectedFace f=m->selected[m->selected_count-1u];
        /* A mixed selection has different reflection bits; toggle each one
         * independently, not all to the bit of the last face. */
        AvpMapMaker *copy=malloc(sizeof *copy);
        if(!copy)return 0;
        memcpy(copy,m,sizeof *copy);copy->selected_count=0;
        for(unsigned i=0;i<m->selected_count;i++){
            f=m->selected[i];
            if(f.x>=copy->w||f.y>=copy->h||f.field>=4u){free(copy);return 0;}
            copy->target_valid=1;copy->target_x=f.x;copy->target_y=f.y;copy->target_field=f.field;
            u8 old=copy->cells[((size_t)f.y*copy->w+f.x)*8u+f.field];
            if(!(old&0x7fu)||!mm_assign_target_one(copy,(u8)(old^0x80u))){
                snprintf(m->message,sizeof m->message,"Mirror blocked: %.89s",copy->message);
                free(copy);return 0;
            }
        }
        memcpy(m->cells,copy->cells,sizeof m->cells);m->dirty=copy->dirty;
        free(copy);msg(m,"Mirrored selected wall faces.");return 1;
    }
    if(m->target_valid&&m->target_field<4u){
        u8 old=m->cells[((size_t)m->target_y*m->w+m->target_x)*8u+m->target_field];
        if(old&0x7fu)return mm_assign_target_one(m,(u8)(old^0x80u));
    }
    msg(m,"Select a textured wall face before mirroring.");return 0;
}
int avp_mm_test_start_valid(const AvpMapMaker *m,double x,double y)
{
    if(!m||!m->w||!m->h||!isfinite(x)||!isfinite(y)||
       x<0.125||y<0.125||x>=(double)m->w-0.125||
       y>=(double)m->h-0.125)return 0;
    return m->cells[((size_t)(unsigned)y*m->w+(unsigned)x)*8u+5u]!=0u;
}

/* Map Maker texture clipboard: raw Jaguar field value, not a transformed
 * texture-file index. Each destination still obeys its field lookup table. */
int avp_mm_copy_surface(AvpMapMaker *m,unsigned x,unsigned y,unsigned field)
{
    if(!m||x>=m->w||y>=m->h||field>=6u)return 0;
    m->copied_texture=m->cells[((size_t)y*m->w+x)*8u+field];
    m->has_copied_texture=1u;
    m->copied_field=field;m->copied_level=m->level;
    snprintf(m->message,sizeof m->message,"Copied texture ID %u.",m->copied_texture);
    return 1;
}
int avp_mm_paste_surface(AvpMapMaker *m,unsigned x,unsigned y,unsigned field)
{
    unsigned ox,oy,of;int ok;
    if(!m||!m->has_copied_texture||x>=m->w||y>=m->h||field>=6u)return 0;
    ox=m->x;oy=m->y;of=m->field;
    m->x=x;m->y=y;m->field=field;
    ok=avp_mm_assign(m,m->copied_texture);
    m->x=ox;m->y=oy;m->field=of;
    if(ok)snprintf(m->message,sizeof m->message,"Pasted texture ID %u.",m->copied_texture);
    return ok;
}
int avp_mm_copy_target(AvpMapMaker *m)
{
    return m&&m->target_valid&&avp_mm_copy_surface(m,m->target_x,m->target_y,m->target_field);
}
int avp_mm_paste_target(AvpMapMaker *m)
{
    if(!m||!m->has_copied_texture||!m->target_valid)return 0;
    return avp_mm_assign_target(m,m->copied_texture);
}
/* 3D held-Enter only paints the face currently under the editor crosshair.
 * A visited bit is marked AFTER the guarded paste succeeds; invalid/scripted
 * surfaces are never modified, and re-aiming a previously painted face in
 * the same stroke does not apply the texture twice. */
int avp_mm_paint_3d_target_once(AvpMapMaker *m,u8 *visited,size_t visited_bytes,
                                AvpMmPaintFace paste,void *context)
{
    size_t bit;
    if(!m||!m->mode_3d||!m->has_copied_texture||!m->target_valid||
       !visited||visited_bytes<AVP_MM_3D_PAINT_VISITED_BYTES||!paste||
       m->target_x>=m->w||m->target_y>=m->h||m->target_field>=6u||
       m->w>AVP_MM_DIM||m->h>AVP_MM_DIM)return 0;
    bit=((size_t)m->target_y*AVP_MM_DIM+m->target_x)*6u+m->target_field;
    if(visited[bit/8u]&(1u<<(bit%8u)))return 0;
    if(!paste(m,m->target_x,m->target_y,m->target_field,context))return 0;
    visited[bit/8u]|=(u8)(1u<<(bit%8u));
    return 1;
}
int avp_mm_place_floor(AvpMapMaker *m)
{
    unsigned char *c;
    if(!m||!m->w||!m->h||m->x>=m->w||m->y>=m->h)return 0;
    c=m->cells+((size_t)m->y*m->w+m->x)*8u;
    if(!c[5]){
        c[5]=m->has_copied_texture && !m->overhead_ceiling && m->copied_texture
             ?m->copied_texture:1u;
        m->dirty=1;
    }
    msg(m,"Floor cell authored. Select a texture or copy/paste one.");
    return 1;
}
int avp_mm_delete_flat(AvpMapMaker *m)
{
    u8 *cell;unsigned field;
    if(!m||!m->w||!m->h||m->x>=m->w||m->y>=m->h)return 0;
    field=m->overhead_ceiling?4u:5u;
    cell=m->cells+((size_t)m->y*m->w+m->x)*8u;
    if(!cell[field])return 0;
    cell[field]=0;m->dirty=1;
    msg(m,field==4u?"Ceiling cleared (walls and objects preserved).":
                      "Floor cleared (walls and objects preserved).");
    return 1;
}
/* Held Enter paints only the active 2D layer; original Jaguar campaign
 * data is never modified. A host bracket groups the whole gesture for Undo. */
int avp_mm_paint_2d_cell(AvpMapMaker *m,unsigned x,unsigned y,unsigned side)
{
    if(!m||m->mode_3d||x>=m->w||y>=m->h)return 0;
    m->x=x;m->y=y;m->selection_2d_valid=1;
    if(m->edit_mode_2d==1u){
        if(side>=4u)return 0;
        m->field=side;
        return avp_mm_wall_place(m,x,y,side);
    }
    if(m->edit_mode_2d==2u)return avp_mm_object_place(m,(double)x+0.5,(double)y+0.5);
    if(m->edit_mode_2d==3u){
        m->field=avp_mm_texture_field(m);
        return avp_mm_assign(m,m->texture_brush[m->texture_target_2d]);
    }
    return avp_mm_place_floor(m);
}
unsigned avp_mm_delete_2d_selection(AvpMapMaker *m)
{
    unsigned removed=0u,cx,cy,side,xmin,xmax,ymin,ymax;
    if(!m||m->mode_3d||!m->selection_2d_valid)return 0u;
    cx=m->x;cy=m->y;side=m->field;
    xmin=m->selection_2d_range && m->select_anchor_x<cx?m->select_anchor_x:cx;
    xmax=m->selection_2d_range && m->select_anchor_x>cx?m->select_anchor_x:cx;
    ymin=m->selection_2d_range && m->select_anchor_y<cy?m->select_anchor_y:cy;
    ymax=m->selection_2d_range && m->select_anchor_y>cy?m->select_anchor_y:cy;
    if(m->edit_mode_2d==3u){
        u8 previous=m->texture_brush[m->texture_target_2d];
        int ok;
        m->texture_brush[m->texture_target_2d]=0u;
        ok=avp_mm_texture_paint_selection(m);
        m->texture_brush[m->texture_target_2d]=previous;
        if(ok)snprintf(m->message,sizeof m->message,
                       "Cleared selected %s textures (geometry unchanged).",
                       avp_mm_texture_target_name(m));
        return ok?(xmax-xmin+1u)*(ymax-ymin+1u):0u;
    }
    for(unsigned y=ymin;y<=ymax;y++)for(unsigned x=xmin;x<=xmax;x++){
        m->x=x;m->y=y;
        if(m->edit_mode_2d==1u){if(side<4u&&avp_mm_wall_erase(m,x,y,side))removed++;}
        else if(m->edit_mode_2d==2u){if(avp_mm_object_remove(m,x,y))removed++;}
        else if(avp_mm_delete_flat(m))removed++;
    }
    m->x=cx;m->y=cy;m->field=side;
    if(!removed)snprintf(m->message,sizeof m->message,"No editable items in selection.");
    else snprintf(m->message,sizeof m->message,"Removed %u items in %s mode.",removed,
                  m->edit_mode_2d==0u?"Cells":m->edit_mode_2d==1u?"Walls":m->edit_mode_2d==2u?"Objects":"Textures");
    return removed;
}
int avp_mm_delete_cell(AvpMapMaker *m)
{
    u8 *c;
    if(!m||!m->w||!m->h||m->x>=m->w||m->y>=m->h)return 0;
    c=m->cells+((size_t)m->y*m->w+m->x)*8u;
    for(unsigned i=0;i<4u;i++)if(avp_mm_script_flags(m,m->x,m->y,i)){
        msg(m,"Cell contains a retail scripted edge; deletion blocked.");return 0;
    }
    for(unsigned i=0;i<8u;i++)if(c[i]){m->dirty=1;break;}
    memset(c,0,8u);
    msg(m,"Cell removed from draft.");
    return 1;
}
/* Each cell has four physical panel sides; rotating a panel relocates its
 * native ID, it does not rotate the texture pixels.  The reflection bit (7)
 * is deliberately preserved.  Doors are encoded in the same side slots, but
 * their motion byte lives on the canonical left/top edge owner (the eastern
 * neighbor for a right edge, the southern neighbor for a bottom edge). */
static int mm_is_door(u8 panel)
{
    unsigned group=((unsigned)panel&0x7fu)>>3;
    return group>=4u && group<=11u; /* matches DOORS.S InitDoors flags */
}
static u8 *mm_door_state(AvpMapMaker *m,unsigned x,unsigned y,unsigned side)
{
    switch(side){
    case 0u: return m->cells+((size_t)y*m->w+x)*8u+6u;
    case 1u: return m->cells+((size_t)y*m->w+x)*8u+7u;
    case 2u: return x+1u<m->w?m->cells+((size_t)y*m->w+x+1u)*8u+6u:NULL;
    case 3u: return y+1u<m->h?m->cells+((size_t)(y+1u)*m->w+x)*8u+7u:NULL;
    default: return NULL;
    }
}
static u8 *mm_opposite(AvpMapMaker *m,unsigned x,unsigned y,unsigned side)
{
    switch(side){
    case 0u: return x>0u?m->cells+((size_t)y*m->w+x-1u)*8u+2u:NULL;
    case 1u: return y>0u?m->cells+((size_t)(y-1u)*m->w+x)*8u+3u:NULL;
    case 2u: return x+1u<m->w?m->cells+((size_t)y*m->w+x+1u)*8u:NULL;
    case 3u: return y+1u<m->h?m->cells+((size_t)(y+1u)*m->w+x)*8u+1u:NULL;
    default: return NULL;
    }
}
static int mm_rotate_wall(AvpMapMaker *m,int clockwise,int with_scripts)
{
    u8 *cell,*src_state=NULL,*dst_state=NULL,*opposite;
    unsigned dest,src_panel,dst_panel;
    unsigned src_flags,dst_flags;u8 panel,open=0;
    AvpMmScriptRow updated[AVP_MM_MAX_SCRIPTS];
    if(!m||m->mode_3d||!m->w||!m->h||m->x>=m->w||m->y>=m->h||m->field>=4u){
        if(m)msg(m,"Select a wall side (F cycles fields 0-3) in 2D mode.");
        return 0;
    }
    cell=m->cells+((size_t)m->y*m->w+m->x)*8u;
    panel=cell[m->field];
    if(!(panel&0x7fu)){msg(m,"No panel on this edge. Add a wall first (T: panel palette).");return 0;}
    dest=(m->field+(clockwise>0?1u:3u))&3u;
    src_flags=avp_mm_script_flags(m,m->x,m->y,m->field);
    dst_flags=avp_mm_script_flags(m,m->x,m->y,dest);
    if(mm_opposite_panel_script(m,m->x,m->y,m->field)||
       mm_opposite_panel_script(m,m->x,m->y,dest)){
        msg(m,"Opposite panel holds a retail script anchor; rotation blocked.");
        return 0;
    }
    if(with_scripts){
        if(!mm_script_source(m)||!src_flags||
           m->script_source_crc!=mm_source_fingerprint(m,m->level)){
            msg(m,"No retail script on source panel; use ordinary rotation.");return 0;
        }
        if(dst_flags){msg(m,"Destination already has scripted metadata; rotation blocked.");return 0;}
    }else if(src_flags||dst_flags){
        msg(m,"Retail scripted edge: use Shift+, or Shift+. to relocate its draft metadata.");
        return 0;
    }
    if(cell[dest]){msg(m,"Rotation blocked: destination wall slot is occupied.");return 0;}
    opposite=mm_opposite(m,m->x,m->y,dest);
    if(opposite && *opposite){msg(m,"Rotation blocked: adjacent cell already owns this edge.");return 0;}
    if(mm_is_door(panel)){
        src_state=mm_door_state(m,m->x,m->y,m->field);
        dst_state=mm_door_state(m,m->x,m->y,dest);
        if(!src_state||!dst_state){
            msg(m,"Door cannot rotate onto an edge without a door-state cell.");return 0;
        }
        opposite=mm_opposite(m,m->x,m->y,m->field);
        if(opposite && mm_is_door(*opposite)){
            msg(m,"Rotation blocked: the original door state is shared.");return 0;
        }
        if(src_state!=dst_state && *dst_state){
            msg(m,"Rotation blocked: destination door-state byte is in use.");return 0;
        }
        open=*src_state;
    }else if(with_scripts){
        /* A non-door panel may have an EXIT anchor, but must not carry
         * access or interlock state: those need a canonical door byte. */
        unsigned state;
        if(mm_script_offsets(m,m->x,m->y,m->field,&src_panel,&state)&&
           state!=65536u && avp_mm_script_lookup(m->script_rows,m->script_count,
                       m->level,state,&(AvpMmScriptInfo){0})){
            msg(m,"Scripted door state requires an actual door panel.");return 0;
        }
    }
    if(with_scripts){
        unsigned src_state_offset,dst_state_offset;
        if(!mm_script_offsets(m,m->x,m->y,m->field,&src_panel,&src_state_offset)||
           !mm_script_offsets(m,m->x,m->y,dest,&dst_panel,&dst_state_offset)){
            msg(m,"Script relocation needs valid canonical offsets.");return 0;
        }
        if(mm_is_door(panel) && (src_state_offset==65536u || dst_state_offset==65536u)){
            msg(m,"Scripted door cannot relocate past the map perimeter.");return 0;
        }
        /* Single preflight: row ordering and every original access, exit and
         * required-state dependency are verified before any map bytes move. */
        memcpy(updated,m->script_rows,m->script_count*sizeof updated[0]);
        for(unsigned i=0;i<m->script_count;i++){
            AvpMmScriptRow *r=&updated[i];
            if(r->offset==src_panel)r->offset=(u16)dst_panel;
            if(mm_is_door(panel) && r->offset==src_state_offset)r->offset=(u16)dst_state_offset;
            if(r->kind==4u && mm_is_door(panel) &&
                (unsigned)r->arg==src_state_offset)r->arg=(s16)dst_state_offset;
        }
        if(!avp_mm_script_validate(updated,m->script_count,m->level)){
            msg(m,"Script relocation rejected: conflicting or orphaned anchors.");return 0;
        }
    }
    cell[dest]=panel;cell[m->field]=0u;
    if(src_state && dst_state && src_state!=dst_state){
        *src_state=0u;*dst_state=open;
    }
    if(with_scripts){
        memcpy(m->script_rows,updated,m->script_count*sizeof updated[0]);
        m->script_modified=1u;
    }
    m->field=dest;m->dirty=1;
    snprintf(m->message,sizeof m->message,"Panel rotated %s to %s edge%s.",
        clockwise>0?"clockwise":"counter-clockwise",
        dest==0u?"LEFT":dest==1u?"TOP":dest==2u?"RIGHT":"BOTTOM",
        with_scripts?" (draft scripts move in isolated Play Test)":
        mm_is_door(panel)?" (door state moved; retail scripts unchanged)":"");
    return 1;
}
int avp_mm_rotate_wall(AvpMapMaker *m,int clockwise)
{
    return mm_rotate_wall(m,clockwise,0);
}
int avp_mm_relocate_scripted_wall(AvpMapMaker *m,int clockwise)
{
    return mm_rotate_wall(m,clockwise,1);
}
/* The wall brush is a convenience layer over the original four Jaguar
 * panel bytes; it does NOT create extra meshes or rewrite script tables.
 * Prefer empty edges: editing an occupied edge requires explicit erasure,
 * avoiding destructive clicks on a retail-derived map. */
void avp_mm_wall_brush_select(AvpMapMaker *m,u8 panel)
{
    if(m && (panel&0x7fu))m->wall_brush=panel;
}
const char *avp_mm_2d_mode_name(const AvpMapMaker *m)
{
    static const char *const names[]={"CELLS","WALLS","OBJECTS","TEXTURES"};
    return names[m&&m->edit_mode_2d<4u?m->edit_mode_2d:0u];
}
static void mm_set_2d_mode(AvpMapMaker *m,unsigned mode)
{
    if(!m||mode>=4u)return;
    avp_mm_wall_drag_end(m);
    m->wall_hover_valid=0;
    /* Changing edit layers must never silently discard a selected region.
     * Enter/Delete use edit_mode_2d; selection is independent of the layer. */
    m->edit_mode_2d=mode;
    m->wall_tool=(mode==1u);
    if(m->wall_tool && m->field<4u)avp_mm_wall_brush_select(m,avp_mm_value(m));
    if(mode==1u && m->field>=4u)m->field=1u;
    snprintf(m->message,sizeof m->message,
             "2D %s: M changes mode; Delete removes only this type.",
             avp_mm_2d_mode_name(m));
}
void avp_mm_cycle_2d_mode(AvpMapMaker *m)
{
    if(m && !m->mode_3d)mm_set_2d_mode(m,(m->edit_mode_2d+1u)%4u);
}
void avp_mm_toggle_wall_tool(AvpMapMaker *m)
{
    if(m && !m->mode_3d)mm_set_2d_mode(m,m->edit_mode_2d==1u?0u:1u);
}
int avp_mm_wall_place(AvpMapMaker *m,unsigned x,unsigned y,unsigned side)
{
    u8 *cell,*opp,*state;
    if(!m||m->mode_3d||x>=m->w||y>=m->h||side>=4u||
       !(m->wall_brush&0x7fu))return 0;
    m->x=x;m->y=y;m->field=side;
    cell=m->cells+((size_t)y*m->w+x)*8u;
    if(cell[side]){
        if(cell[side]!=m->wall_brush)msg(m,"Wall already here. Right-click it to replace it.");
        return cell[side]==m->wall_brush;
    }
    opp=mm_opposite(m,x,y,side);
    if(opp && *opp){msg(m,"This edge already contains the neighboring cell's wall.");return 0;}
    if(avp_mm_script_flags(m,x,y,side)||mm_opposite_panel_script(m,x,y,side)){
        msg(m,"Retail scripted edge protected; no wall was placed.");return 0;
    }
    if(mm_is_door(m->wall_brush)){
        state=mm_door_state(m,x,y,side);
        if(!state){msg(m,"Door needs a valid state cell; choose another edge.");return 0;}
        if(*state){msg(m,"Door-state byte is occupied; placement blocked.");return 0;}
    }
    cell[side]=m->wall_brush;
    m->dirty=1;
    snprintf(m->message,sizeof m->message,"Wall %u placed (%s). ,/. rotate; right-click erase.",
        (unsigned)m->wall_brush,side==0u?"LEFT":side==1u?"TOP":side==2u?"RIGHT":"BOTTOM");
    return 1;
}
int avp_mm_wall_erase(AvpMapMaker *m,unsigned x,unsigned y,unsigned side)
{
    u8 *cell,*other,*state;unsigned rx=x,ry=y,rs=side;
    if(!m||m->mode_3d||x>=m->w||y>=m->h||side>=4u)return 0;
    m->x=x;m->y=y;m->field=side;
    cell=m->cells+((size_t)y*m->w+x)*8u;
    other=mm_opposite(m,x,y,side);
    if(!cell[side] && other && *other){
        /* A Jaguar physical edge may be represented by the neighbor.
         * Erase the actual owner, not an empty byte in the clicked cell. */
        if(side==0u){rx=x-1u;rs=2u;}
        else if(side==1u){ry=y-1u;rs=3u;}
        else if(side==2u){rx=x+1u;rs=0u;}
        else {ry=y+1u;rs=1u;}
        cell=m->cells+((size_t)ry*m->w+rx)*8u;
    }
    if(!cell[rs])return 1;
    /* Do not silently remove one half of a doubled physical edge. */
    other=mm_opposite(m,rx,ry,rs);
    if(other && *other){msg(m,"Both sides contain a panel; erase the conflicting side separately.");return 0;}
    if(avp_mm_script_flags(m,rx,ry,rs)||mm_opposite_panel_script(m,rx,ry,rs)){
        msg(m,"Retail scripted edge protected; erase blocked.");return 0;
    }
    if(mm_is_door(cell[rs])){
        state=mm_door_state(m,rx,ry,rs);
        if(!state){msg(m,"Door-state owner invalid; erase blocked.");return 0;}
        /* The original closed-state byte is zero. When removing an
         * unscripted door its private open-state byte must not survive. */
        *state=0u;
    }
    cell[rs]=0u;m->dirty=1;
    msg(m,"Wall erased from draft. Left-click to draw another.");
    return 1;
}
/* Hover must never advertise a forbidden edit as a valid placement.
 * Apply the same physical-edge and scripted-anchor constraints as place().
 * A panel already matching the brush is a harmless no-op. */
int avp_mm_wall_placeable(const AvpMapMaker *m,unsigned x,unsigned y,unsigned side)
{
    const u8 *c,*opp=NULL,*state=NULL;
    if(!m||m->mode_3d||!m->w||!m->h||x>=m->w||y>=m->h||
       side>=4u||!(m->wall_brush&0x7fu))return 0;
    c=m->cells+((size_t)y*m->w+x)*8u;
    if(c[side])return c[side]==m->wall_brush;
    switch(side){
    case 0u:if(x)opp=m->cells+((size_t)y*m->w+x-1u)*8u+2u;
            state=c+6u;break;
    case 1u:if(y)opp=m->cells+((size_t)(y-1u)*m->w+x)*8u+3u;
            state=c+7u;break;
    case 2u:if(x+1u<m->w){opp=m->cells+((size_t)y*m->w+x+1u)*8u;
                             state=opp+6u;}break;
    case 3u:if(y+1u<m->h){opp=m->cells+((size_t)(y+1u)*m->w+x)*8u+1u;
                             state=opp+6u;}break;
    }
    if((opp&&*opp)||avp_mm_script_flags(m,x,y,side)||
       mm_opposite_panel_script(m,x,y,side))return 0;
    return !mm_is_door(m->wall_brush)||(state&&!*state);
}
/* Bresenham-style cell stroke, including intermediate cells that may be
 * skipped if the mouse moves faster than X11 delivers pointer events.
 * One orientation per stroke: a small slip near the next cell boundary
 * cannot silently turn a straight wall into an accidental corner. */
void avp_mm_wall_drag_end(AvpMapMaker *m)
{
    if(!m)return;
    m->wall_drag_lock=0;
    m->wall_last_valid=0;
}
int avp_mm_wall_stroke(AvpMapMaker *m,unsigned x,unsigned y,unsigned side,int erase)
{
    int x0,y0,x1,y1,dx,dy,sx,sy,err;
    int all_ok=1;
    unsigned locked_side;
    if(!m||!m->wall_tool||m->mode_3d||x>=m->w||y>=m->h||side>=4u)return 0;
    if(!m->wall_drag_lock){
        m->wall_drag_x=m->x;m->wall_drag_y=m->y;
        m->wall_drag_lock=1;
        m->wall_drag_side=side;
        m->wall_last_valid=0;
    }
    locked_side=m->wall_drag_side;
    if(locked_side>=4u)return 0;
    x1=(int)x;y1=(int)y;
    x0=m->wall_last_valid?(int)m->wall_last_x:x1;
    y0=m->wall_last_valid?(int)m->wall_last_y:y1;
    dx=abs(x1-x0);dy=abs(y1-y0);
    sx=x0<x1?1:-1;sy=y0<y1?1:-1;
    err=dx-dy;
    for(;;){
        /* Never create a missing retail/script panel on a drag. */
        if(erase ? !avp_mm_wall_erase(m,(unsigned)x0,(unsigned)y0,locked_side) :
                   !avp_mm_wall_place(m,(unsigned)x0,(unsigned)y0,locked_side))all_ok=0;
        if(x0==x1&&y0==y1)break;
        {int twice=err*2;
         if(twice> -dy){err-=dy;x0+=sx;}
         if(twice< dx){err+=dx;y0+=sy;}}
    }
    m->wall_last_x=x;m->wall_last_y=y;m->wall_last_valid=1;
    /* The stroke's physical orientation remains visually selected. */
    m->field=locked_side;
    return all_ok;
}
int avp_mm_wall_turn(AvpMapMaker *m,int clockwise)
{
    unsigned dest;
    if(!m||m->mode_3d||m->field>=4u){
        if(m)msg(m,"Select a wall edge in 2D first.");
        return 0;
    }
    /* Wall mode rotates the BRUSH direction only. Never modify a placed
     * panel just because the selected cell already has a wall. Explicit
     * scripted relocation is a separate command (Shift+comma/period). */
    dest=(m->field+(clockwise>0?1u:3u))&3u;
    m->field=dest;
    snprintf(m->message,sizeof m->message,"Wall direction: %s. Click a cell to place.",
        dest==0u?"LEFT":dest==1u?"TOP":dest==2u?"RIGHT":"BOTTOM");
    return 1;
}
void avp_mm_zoom(AvpMapMaker *m,int direction)
{
    if(!m||!direction)return;
    if(direction>0&&m->overhead_zoom<28u)m->overhead_zoom++;
    if(direction<0&&m->overhead_zoom>1u)m->overhead_zoom--;
}
void avp_mm_camera_pitch(AvpMapMaker *m,double delta)
{
    if(!m||!m->mode_3d||!isfinite(delta))return;
    m->camera_pitch+=delta;
    if(m->camera_pitch>0.85)m->camera_pitch=0.85;
    if(m->camera_pitch< -0.85)m->camera_pitch=-0.85;
}

/* Host-only Texture mode: the walls use the chosen panel side, the flats
 * have independent Jaguar field IDs. No invented geometry or IDs. */
unsigned avp_mm_texture_field(const AvpMapMaker *m)
{
    if(!m)return 5u;
    if(m->texture_target_2d==0u)return 5u;
    if(m->texture_target_2d==1u)return 4u;
    return m->field<4u?m->field:1u;
}
const char *avp_mm_texture_target_name(const AvpMapMaker *m)
{
    static const char *names[]={"FLOOR","CEILING","WALLS"};
    return names[m&&m->texture_target_2d<3u?m->texture_target_2d:0u];
}
void avp_mm_cycle_texture_target(AvpMapMaker *m)
{
    if(!m||m->mode_3d)return;
    m->texture_target_2d=(m->texture_target_2d+1u)%3u;
    if(m->texture_target_2d<2u)m->overhead_ceiling=(int)m->texture_target_2d;
    snprintf(m->message,sizeof m->message,"Texture target: %s. T choose, Enter paint.",
             avp_mm_texture_target_name(m));
}
void avp_mm_texture_brush_select(AvpMapMaker *m,u8 id)
{
    if(!m||m->texture_target_2d>=3u)return;
    m->texture_brush[m->texture_target_2d]=id;
    snprintf(m->message,sizeof m->message,"%s brush: texture ID %u. Enter to apply.",
             avp_mm_texture_target_name(m),(unsigned)id);
}
int avp_mm_texture_selection(const AvpMapMaker *m,u8 *id,int *mixed)
{
    unsigned xmin,xmax,ymin,ymax,field;u8 initial;int mix=0;
    if(!m||!m->w||!m->h||m->x>=m->w||m->y>=m->h)return 0;
    xmin=m->selection_2d_valid&&m->selection_2d_range&&m->select_anchor_x<m->x?m->select_anchor_x:m->x;
    xmax=m->selection_2d_valid&&m->selection_2d_range&&m->select_anchor_x>m->x?m->select_anchor_x:m->x;
    ymin=m->selection_2d_valid&&m->selection_2d_range&&m->select_anchor_y<m->y?m->select_anchor_y:m->y;
    ymax=m->selection_2d_valid&&m->selection_2d_range&&m->select_anchor_y>m->y?m->select_anchor_y:m->y;
    if(xmax>=m->w||ymax>=m->h)return 0;
    field=avp_mm_texture_field(m);
    initial=m->cells[((size_t)ymin*m->w+xmin)*8u+field];
    for(unsigned y=ymin;y<=ymax;y++)for(unsigned x=xmin;x<=xmax;x++)
        if(m->cells[((size_t)y*m->w+x)*8u+field]!=initial)mix=1;
    if(id)*id=initial;
    if(mixed)*mixed=mix;
    return 1;
}
int avp_mm_texture_paint_selection(AvpMapMaker *m)
{
    unsigned ox,oy,xmin,xmax,ymin,ymax,field,count=0;
    AvpMapMaker *copy;
    if(!m||m->mode_3d||m->edit_mode_2d!=3u||m->texture_target_2d>=3u||
       !m->w||!m->h||m->x>=m->w||m->y>=m->h)return 0;
    ox=m->x;oy=m->y;field=avp_mm_texture_field(m);
    xmin=m->selection_2d_valid&&m->selection_2d_range&&m->select_anchor_x<ox?m->select_anchor_x:ox;
    xmax=m->selection_2d_valid&&m->selection_2d_range&&m->select_anchor_x>ox?m->select_anchor_x:ox;
    ymin=m->selection_2d_valid&&m->selection_2d_range&&m->select_anchor_y<oy?m->select_anchor_y:oy;
    ymax=m->selection_2d_valid&&m->selection_2d_range&&m->select_anchor_y>oy?m->select_anchor_y:oy;
    if(xmax>=m->w||ymax>=m->h)return 0;
    /* Preflight against the exact existing Jaguar scripted-edge guard.
     * A blocked member must not partially texture a selected room. */
    copy=malloc(sizeof *copy);if(!copy)return 0;
    memcpy(copy,m,sizeof *copy);
    for(unsigned y=ymin;y<=ymax;y++)for(unsigned x=xmin;x<=xmax;x++){
        copy->x=x;copy->y=y;copy->field=field;
        if(!avp_mm_assign(copy,m->texture_brush[m->texture_target_2d])){
            snprintf(m->message,sizeof m->message,"Texture selection blocked: %.80s",copy->message);
            free(copy);return 0;
        }
        count++;
    }
    memcpy(m->cells,copy->cells,sizeof m->cells);m->dirty=copy->dirty;
    free(copy);
    snprintf(m->message,sizeof m->message,"Painted %u %s surfaces (ID %u).",
             count,avp_mm_texture_target_name(m),
             (unsigned)m->texture_brush[m->texture_target_2d]);
    return 1;
}
