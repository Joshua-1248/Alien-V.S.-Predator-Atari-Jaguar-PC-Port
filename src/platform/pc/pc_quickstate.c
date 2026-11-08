#include "pc/pc_quickstate.h"
#include "collision.h"
#include "maze.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

extern unsigned char __avp_game_state_start[];
extern unsigned char __avp_game_state_end[];

enum { AVP_QS_VERSION=1 };
struct AvpPcQuickState {
    unsigned char *core;
    size_t core_size;
    u8 *maze;
    u8 *maze_source; /* still allocated for the duration of this PC session */
    size_t maze_size;
    u16 maze_w,maze_h;
    u64 sim_ticks;
    unsigned version;
    int valid;
};

size_t avp_pc_quickstate_core_bytes(void)
{
    return (size_t)(__avp_game_state_end-__avp_game_state_start);
}

AvpPcQuickState *avp_pc_quickstate_create(void)
{
    AvpPcQuickState *q=(AvpPcQuickState*)calloc(1,sizeof(*q));
    if(!q)return NULL;
    q->core_size=avp_pc_quickstate_core_bytes();
    if(!q->core_size){free(q);return NULL;}
    q->core=(unsigned char*)malloc(q->core_size);
    if(!q->core){free(q);return NULL;}
    q->version=AVP_QS_VERSION;
    return q;
}

void avp_pc_quickstate_destroy(AvpPcQuickState *q)
{
    if(!q)return;
    free(q->maze);free(q->core);free(q);
}

int avp_pc_quickstate_save(AvpPcQuickState *q,const u8 *maze,u16 width,u16 height,u64 sim_ticks)
{
    size_t n=(size_t)width*(size_t)height*8u;
    u8 *m;
    if(!q||!q->core||!maze||!width||!height||!n)return 0;
    m=(u8*)realloc(q->maze,n);
    if(!m)return 0;
    q->maze=m;q->maze_source=(u8*)maze;q->maze_size=n;
    q->maze_w=width;q->maze_h=height;q->sim_ticks=sim_ticks;
    memcpy(q->core,__avp_game_state_start,q->core_size);
    memcpy(q->maze,maze,n);
    q->version=AVP_QS_VERSION;q->valid=1;
    return 1;
}

int avp_pc_quickstate_load(AvpPcQuickState *q,u64 *sim_ticks)
{
    size_t n;
    if(!q||!q->valid||q->version!=AVP_QS_VERSION||!q->core||!q->maze||!q->maze_source)return 0;
    n=(size_t)q->maze_w*(size_t)q->maze_h*8u;
    if(!q->maze_w||!q->maze_h||n!=q->maze_size||
       q->core_size!=avp_pc_quickstate_core_bytes())return 0;
    /* The saved maze belongs to the ROM-decoded per-level allocations, which
       live until process exit.  It may not be the *current* level's buffer:
       verify snapshot metadata before overwriting any live game state. */
    memcpy(__avp_game_state_start,q->core,q->core_size);
    memcpy(q->maze_source,q->maze,n);
    if(sim_ticks)*sim_ticks=q->sim_ticks;
    return 1;
}

int avp_pc_quickstate_valid(const AvpPcQuickState *q){return q&&q->valid&&q->version==AVP_QS_VERSION;}
u16 avp_pc_quickstate_width(const AvpPcQuickState *q){return q?q->maze_w:0;}
u16 avp_pc_quickstate_height(const AvpPcQuickState *q){return q?q->maze_h:0;}


/* Copy-only hooks for the versioned disk archive.  Never publish an
 * unvalidated disk read directly into live CPU game globals. */
int avp_pc_quickstate_export(const AvpPcQuickState *q,void *core,size_t cn,
                            void *maze,size_t mn,u64 *ticks)
{
    if(!q||!avp_pc_quickstate_valid(q)||!core||!maze||cn!=q->core_size||
       mn!=q->maze_size)return 0;
    memcpy(core,q->core,cn);memcpy(maze,q->maze,mn);
    if(ticks)*ticks=q->sim_ticks;
    return 1;
}
int avp_pc_quickstate_import(AvpPcQuickState *q,const void *core,size_t cn,
    const void *maze,size_t mn,u8 *source,u16 w,u16 h,u64 ticks)
{
    u8 *newmaze;
    if(!q||!core||!maze||!source||!w||!h||!mn||
       cn!=q->core_size||mn!=(size_t)w*h*8u)return 0;
    newmaze=(u8*)realloc(q->maze,mn);
    if(!newmaze)return 0;
    q->maze=newmaze;memcpy(q->core,core,cn);memcpy(q->maze,maze,mn);
    q->maze_size=mn;q->maze_source=source;
    q->maze_w=w;q->maze_h=h;q->sim_ticks=ticks;
    q->valid=1;q->version=AVP_QS_VERSION;
    return 1;
}
uint64_t avp_pc_quickstate_maze_source(const AvpPcQuickState *q)
{ return q?(uint64_t)(uintptr_t)q->maze_source:0u; }
