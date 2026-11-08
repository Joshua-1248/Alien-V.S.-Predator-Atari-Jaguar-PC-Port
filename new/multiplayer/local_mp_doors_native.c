#include "local_mp_doors_native.h"
#include "doors.h"
#include <string.h>

/* The engine owns one active door machine. Its state is borrowed serially,
 * never operated simultaneously from multiple threads. */
static int scope_busy;

int avp_mp_doors_scope_enter(AvpMpDoorScope *scope,
                             const AvpMpDoorEngineState *world)
{
    if(!scope || !world || scope_busy || scope->active) return 0;
    if(!avp_mp_doors_native_capture(&scope->original)) return 0;
    if(!avp_mp_doors_native_restore(world)) return 0;
    scope_busy=1;
    scope->active=1;
    return 1;
}

int avp_mp_doors_scope_leave(AvpMpDoorScope *scope,
                             AvpMpDoorEngineState *world)
{
    AvpMpDoorEngineState modified;
    int ok;
    if(!scope || !world || !scope_busy || !scope->active) return 0;
    /* Restore the original even when this world became malformed. */
    ok=avp_mp_doors_native_capture(&modified);
    if(!avp_mp_doors_native_restore(&scope->original)) return 0;
    scope_busy=0;
    scope->active=0;
    if(ok) *world=modified;
    return ok;
}

int avp_mp_doors_world_init(AvpMpDoorEngineState *world, uint8_t *maze,
                             uint16_t width, uint16_t height, unsigned level)
{
    AvpMpDoorEngineState prior, created;
    if(!world || !maze || !width || !height || width>64 || height>64 ||
       !level || level>AVP_MAX_LEVEL || scope_busy) return 0;
    if(!avp_mp_doors_native_capture(&prior)) return 0;
    avp_doors_bind_maze(maze,width,height);
    InitDoors();
    /* No per-species access assumptions. The mixed-campaign gameplay layer
       must initialize its own access rights when it is actually connected. */
    /* The level-specific original door metadata binding is provided later by
       the real game loader; never invent access/exit records here. */
    if(!avp_mp_doors_native_capture(&created)) {
        (void)avp_mp_doors_native_restore(&prior);
        return 0;
    }
    created.level=(int16_t)level;
    /* Never inherit the host campaign's access/exit table or panel IDs.
     * The real level-resource integration must install these explicitly. */
    created.level_meta=NULL;
    created.level_meta_count=0u;
    created.current_meta=NULL;
    memset(&created.panels,0xff,sizeof(created.panels));
    memset(created.panel_list,0xff,sizeof(created.panel_list));
    if(!avp_mp_doors_native_restore(&prior)) return 0;
    *world=created;
    return 1;
}
