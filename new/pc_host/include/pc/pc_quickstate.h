#ifndef AVP_PC_QUICKSTATE_H
#define AVP_PC_QUICKSTATE_H
#include "avp_types.h"
#include "avp_resources.h"
#include <stddef.h>

typedef struct AvpPcQuickState AvpPcQuickState;
AvpPcQuickState *avp_pc_quickstate_create(void);
void avp_pc_quickstate_destroy(AvpPcQuickState *q);
int avp_pc_quickstate_save(AvpPcQuickState *q,const u8 *maze,u16 width,u16 height,u64 sim_ticks);
int avp_pc_quickstate_load(AvpPcQuickState *q,u64 *sim_ticks);
int avp_pc_quickstate_valid(const AvpPcQuickState *q);
size_t avp_pc_quickstate_core_bytes(void);
u16 avp_pc_quickstate_width(const AvpPcQuickState *q);
u16 avp_pc_quickstate_height(const AvpPcQuickState *q);
int avp_pc_quickstate_export(const AvpPcQuickState *q,void *core,size_t core_n,
    void *maze,size_t maze_n,u64 *ticks);
int avp_pc_quickstate_import(AvpPcQuickState *q,const void *core,size_t core_n,
    const void *maze,size_t maze_n,u8 *maze_source,u16 w,u16 h,u64 ticks);
uint64_t avp_pc_quickstate_maze_source(const AvpPcQuickState *q);
int avp_pc_quickstate_disk_save(const AvpPcQuickState *q,
    const AvpRomImage *rom,unsigned level,const void *map,size_t map_size);
int avp_pc_quickstate_disk_load(AvpPcQuickState *q,
    const AvpRomImage *rom,const AvpMazeCandidate *mazes,unsigned maze_count,
    void *map,size_t map_size);
#endif
