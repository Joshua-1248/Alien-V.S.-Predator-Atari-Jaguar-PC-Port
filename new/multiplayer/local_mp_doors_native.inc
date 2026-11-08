/* This file is included only at the END of original doors.c when the optional
 * PC multiplayer module is built. The original game logic is not rewritten.
 * Keep all extensions in new/; the sole game-source change is this include.
 */
#include "../../new/multiplayer/local_mp_doors_native.h"
#include <stdint.h>

#define AVP_MP_DOOR_SNAPSHOT_MAGIC 0x444F4F52u

static int mp_offset_from_pointer(const u8 *p, size_t *out)
{
    uintptr_t first, end, value;
    size_t bytes;
    if (!out) return 0;
    *out=0;
    if (!p) return 1;
    if (!maze_data || !maze_w || !maze_h) return 0;
    bytes=(size_t)maze_w*maze_h*CELL;
    first=(uintptr_t)maze_data;
    if (bytes>UINTPTR_MAX-first) return 0;
    end=first+bytes;
    value=(uintptr_t)p;
    if (value<first || value>=end) return 0;
    *out=(size_t)(value-first);
    return 1;
}

static int mp_pointer_offset_valid(const AvpMpDoorEngineState *s,
                                   size_t offset, int active)
{
    size_t bytes=(size_t)s->width*s->height*CELL;
    return !active || (s->maze && s->width && s->height &&
                       s->width<=64 && s->height<=64 && offset<bytes);
}

static int mp_slot_capture(const DoorState *native, AvpMpDoorSlot *slot)
{
    memset(slot,0,sizeof(*slot));
    slot->active=(u8)(native->ptr!=NULL);
    if (!mp_offset_from_pointer(native->ptr,&slot->offset)) return 0;
    slot->direction=native->dir;
    slot->limit=native->lim;
    slot->xpos=native->xpos;
    slot->ypos=native->ypos;
    slot->index=native->index;
    slot->grid1=native->grid1;
    slot->grid2=native->grid2;
    return 1;
}
static void mp_slot_restore(DoorState *native, const AvpMpDoorSlot *slot,
                            u8 *maze)
{
    native->ptr=slot->active?maze+slot->offset:NULL;
    native->dir=slot->direction;
    native->lim=slot->limit;
    native->xpos=slot->xpos;
    native->ypos=slot->ypos;
    native->index=slot->index;
    native->grid1=slot->grid1;
    native->grid2=slot->grid2;
}

int avp_mp_doors_native_capture(AvpMpDoorEngineState *out)
{
    AvpMpDoorEngineState temp;
    if(!out) return 0;
    memset(&temp,0,sizeof(temp));
    if(!mp_slot_capture(&door1,&temp.door[0]) ||
       !mp_slot_capture(&door2,&temp.door[1]) ||
       !mp_offset_from_pointer(lift_ptr,&temp.lift_offset)) return 0;
    temp.valid=AVP_MP_DOOR_SNAPSHOT_MAGIC;
    temp.maze=maze_data;temp.width=maze_w;temp.height=maze_h;
    temp.level=cur_level;
    temp.level_meta=level_meta;temp.level_meta_count=level_meta_count;
    temp.current_meta=cur_meta;
    temp.panels=panel_ids;
    memcpy(temp.panel_list,panel_list,sizeof(temp.panel_list));
    memcpy(temp.wall_flags,fullbits,sizeof(temp.wall_flags));
    memcpy(temp.save_arena,l2l_area,sizeof(temp.save_arena));
    memcpy(temp.saved_offsets,l2l_off,sizeof(temp.saved_offsets));
    temp.saved_used=l2l_used;
    temp.access_level=acs_level;temp.lift_key=lift_key;
    temp.computer_panel=comp_panel;temp.computer_offset=comp_offset;
    temp.double_door=dbl_door;temp.lift_direction=lift_dir;
    temp.lift_active=(u8)(lift_ptr!=NULL);
    temp.lift_x=lift_x;temp.lift_y=lift_y;
    temp.alien_use_active=pc_alien_use_active;
    temp.alien_use_down=pc_alien_use_down;
    temp.alien_use_previous=pc_alien_use_previous;
    temp.level_effect_id=levfx_ID;
    *out=temp;
    return 1;
}

int avp_mp_doors_native_restore(const AvpMpDoorEngineState *s)
{
    unsigned i;
    if(!s || s->valid!=AVP_MP_DOOR_SNAPSHOT_MAGIC ||
       s->width>64 || s->height>64 ||
       ((s->width||s->height) && (!s->width||!s->height||!s->maze)) ||
       (s->level<-1 || s->level>AVP_MAX_LEVEL) ||
       s->saved_used>AVP_L2L_SIZE-AVP_MAX_LEVEL*2u ||
       (s->level_meta_count && !s->level_meta) ||
       !mp_pointer_offset_valid(s,s->door[0].offset,s->door[0].active) ||
       !mp_pointer_offset_valid(s,s->door[1].offset,s->door[1].active) ||
       !mp_pointer_offset_valid(s,s->lift_offset,s->lift_active)) return 0;
    for(i=0;i<AVP_MAX_LEVEL;++i)
        if(s->saved_offsets[i]<-1 ||
           (s->saved_offsets[i]>=0 && (unsigned)s->saved_offsets[i]>s->saved_used)) return 0;
    /* All input checks finish BEFORE mutating the running game's globals. */
    maze_data=s->maze;maze_w=s->width;maze_h=s->height;
    cur_level=s->level;
    level_meta=s->level_meta;level_meta_count=s->level_meta_count;
    cur_meta=s->current_meta;panel_ids=s->panels;
    memcpy(panel_list,s->panel_list,sizeof(s->panel_list));
    memcpy(fullbits,s->wall_flags,sizeof(s->wall_flags));
    memcpy(l2l_area,s->save_arena,sizeof(s->save_arena));
    memcpy(l2l_off,s->saved_offsets,sizeof(s->saved_offsets));
    l2l_used=s->saved_used;
    acs_level=s->access_level;lift_key=s->lift_key;
    comp_panel=s->computer_panel;comp_offset=s->computer_offset;
    mp_slot_restore(&door1,&s->door[0],maze_data);
    mp_slot_restore(&door2,&s->door[1],maze_data);
    dbl_door=s->double_door;lift_dir=s->lift_direction;
    lift_ptr=s->lift_active?maze_data+s->lift_offset:NULL;
    lift_x=s->lift_x;lift_y=s->lift_y;
    pc_alien_use_active=s->alien_use_active;
    pc_alien_use_down=s->alien_use_down;
    pc_alien_use_previous=s->alien_use_previous;
    levfx_ID=s->level_effect_id;
    return 1;
}

/* Preserve LEVELS/DOORS.S source lift owner cell for the serialized
 * multiplayer scheduler. This is observation only, not an AI replacement. */
int avp_mp_doors_native_pending_lift(uint16_t *x,uint16_t *y)
{
    if(!x||!y||!lift_ptr)return 0;
    *x=lift_x;*y=lift_y;
    return 1;
}
