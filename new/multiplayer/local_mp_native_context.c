#include "local_mp_native_context.h"
#include "collision.h"
#include "maze.h"
#include "joypad.h"
#include <stddef.h>
#include <string.h>

/* No worker processes, threads or additional game windows; the original
 * 68000 C source is inherently global and may only be visited serially. */
static int native_world_busy;

int avp_mp_native_world_scope_active(void)
{
    return native_world_busy!=0;
}

int avp_mp_native_world_valid(const AvpMpNativeWorld *w)
{
    const AvpMpRetailMap *m;
    size_t required;
    if (!w || !(m=w->map) || !m->bytes || !m->leases ||
        !m->width || m->width>64u || !m->height || m->height>64u ||
        !m->level || m->level>AVP_MAX_LEVEL) return 0;
    required=8u+(size_t)m->width*m->height*8u;
    if (m->size!=required || m->bytes[0]!=(uint8_t)(m->width>>8) ||
        m->bytes[1]!=(uint8_t)m->width ||
        m->bytes[2]!=(uint8_t)(m->height>>8) ||
        m->bytes[3]!=(uint8_t)m->height) return 0;
    return w->doors.maze==m->bytes+8u && w->doors.width==m->width &&
           w->doors.height==m->height && w->doors.level==(int16_t)m->level &&
           w->doors.valid!=0 && w->amp.valid!=0 &&
           avp_mp_hudworld_native_valid(&w->hud_world);
}

int avp_mp_native_capture_active_world(AvpMpNativeWorld *out,
                                       AvpMpRetailMap *retained_map)
{
    AvpMpNativeWorld snapshot;
    uint8_t *host_cells;
    size_t cells;
    if (!out || !retained_map || native_world_busy || !retained_map->bytes ||
        !retained_map->leases || !retained_map->width ||
        retained_map->width>64u || !retained_map->height ||
        retained_map->height>64u || !retained_map->level ||
        retained_map->level>AVP_MAX_LEVEL ||
        retained_map->size != 8u+(size_t)retained_map->width*retained_map->height*8u ||
        retained_map->bytes[0]!=(uint8_t)(retained_map->width>>8) ||
        retained_map->bytes[1]!=(uint8_t)retained_map->width ||
        retained_map->bytes[2]!=(uint8_t)(retained_map->height>>8) ||
        retained_map->bytes[3]!=(uint8_t)retained_map->height ||
        cur_level!=(int16_t)retained_map->level ||
        maze_width!=retained_map->width || maze_height!=retained_map->height)
        return 0;
    host_cells=avp_collision_maze_data();
    if (!host_cells || host_cells==retained_map->bytes+8u ||
        avp_collision_maze_width()!=retained_map->width ||
        avp_collision_maze_height()!=retained_map->height) return 0;
    memset(&snapshot,0,sizeof(snapshot));
    /* Original snapshots own level access, timers, generator, allocator and
     * current door slots. The door offsets are relative to their maze data,
     * so the exact original pointers can be safely rebound to this copy. */
    if (!avp_mp_doors_native_capture(&snapshot.doors) ||
        snapshot.doors.maze!=host_cells ||
        snapshot.doors.width!=retained_map->width ||
        snapshot.doors.height!=retained_map->height ||
        snapshot.doors.level!=(int16_t)retained_map->level ||
        !avp_mp_amp_native_capture(&snapshot.amp) ||
        !avp_mp_hudworld_native_capture(&snapshot.hud_world))
        return 0;
    snapshot.map=retained_map;
    snapshot.pad_frame_counter=frame_counter;
    snapshot.doors.maze=retained_map->bytes+8u;
    if (!avp_mp_native_world_valid(&snapshot)) return 0;
    /* Commit only after every capture/validation has succeeded. Source maze
     * header is retained byte-for-byte; active doors may have changed cells. */
    cells=(size_t)retained_map->width*retained_map->height*8u;
    memcpy(retained_map->bytes+8u,host_cells,cells);
    *out=snapshot;
    return 1;
}

int avp_mp_native_world_enter(AvpMpNativeScope *s, const AvpMpNativeWorld *w)
{
    if (!s || s->active || native_world_busy || !avp_mp_native_world_valid(w)) return 0;
    /* Save the bindings not covered by DOORS.S / AMP.S snapshots. */
    s->old_collision=avp_collision_maze_data();
    s->old_collision_width=avp_collision_maze_width();
    s->old_collision_height=avp_collision_maze_height();
    s->old_maze_width=maze_width;
    s->old_maze_height=maze_height;
    s->old_pad_frame_counter=frame_counter;
    if(!avp_mp_hudworld_native_capture(&s->old_hud_world))return 0;
    if (!avp_mp_doors_scope_enter(&s->doors,&w->doors)) return 0;
    if (!avp_mp_amp_scope_enter(&s->amp,&w->amp)) {
        AvpMpDoorEngineState discard;
        /* AMP refused to bind; return to the exact original host doors. */
        (void)avp_mp_doors_scope_leave(&s->doors,&discard);
        return 0;
    }
    avp_collision_bind_maze(w->map->bytes+8u,(uint16_t)w->map->width,
                            (uint16_t)w->map->height);
    maze_width=(uint16_t)w->map->width;
    maze_height=(uint16_t)w->map->height;
    frame_counter=w->pad_frame_counter;
    /* Validated in native_world_valid before touching any host state. */
    (void)avp_mp_hudworld_native_restore(&w->hud_world);
    s->bound_world=w;
    s->active=1;
    native_world_busy=1;
    return 1;
}

int avp_mp_native_actor_enter(AvpMpNativeScope *s, const AvpMpActorNativeState *actor)
{
    if (!s || !s->active || s->actor_active || !actor) return 0;
    if (!avp_mp_actor_scope_enter(&s->actor,actor)) return 0;
    s->actor_active=1;
    return 1;
}

int avp_mp_native_actor_leave(AvpMpNativeScope *s, AvpMpActorNativeState *actor)
{
    int ok;
    if (!s || !s->active || !s->actor_active || !actor) return 0;
    ok=avp_mp_actor_scope_leave(&s->actor,actor);
    if (!s->actor.active) s->actor_active=0;
    return ok;
}

int avp_mp_native_world_leave(AvpMpNativeScope *s, AvpMpNativeWorld *w)
{
    int amp_ok, door_ok, hud_ok;
    if (!s || !s->active || s->actor_active || !w || w!=s->bound_world) return 0;
    /* Original door and AMP code may have mutated shared maze bytes in place.
     * Do not revert those bytes: they belong to this world, not the host. */
    w->pad_frame_counter=frame_counter;
    hud_ok=avp_mp_hudworld_native_capture(&w->hud_world);
    amp_ok=avp_mp_amp_scope_leave(&s->amp,&w->amp);
    door_ok=avp_mp_doors_scope_leave(&s->doors,&w->doors);
    avp_collision_bind_maze(s->old_collision,s->old_collision_width,
                            s->old_collision_height);
    maze_width=s->old_maze_width;
    maze_height=s->old_maze_height;
    frame_counter=s->old_pad_frame_counter;
    (void)avp_mp_hudworld_native_restore(&s->old_hud_world);
    if (!s->amp.active && !s->doors.active) {
        s->active=0;
        s->bound_world=NULL;
        native_world_busy=0;
    }
    return amp_ok && door_ok && hud_ok;
}

int avp_mp_native_run_world(AvpMpNativeWorld *w,
                            AvpMpActorNativeState *actors[AVP_LOCAL_MP_MAX_ACTORS], unsigned mask,
                            AvpMpNativeActorStep actor_step,
                            AvpMpNativeWorldStep world_step, void *user)
{
    AvpMpNativeScope scope={0};
    unsigned p;
    int ok=1;
    if (!actors || !actor_step || !world_step || !mask || (mask&~((1u<<AVP_LOCAL_MP_MAX_ACTORS)-1u))) return 0;
    for(p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++)
        if ((mask&(1u<<p)) && !actors[p]) return 0;
    if (!avp_mp_native_world_enter(&scope,w)) return 0;
    /* A retail game tick calls readpad once before UpdatePlayer. Multiple
     * actors must not increment that global clock more than once per world. */
    ++frame_counter;
    for(p=0;p<AVP_LOCAL_MP_MAX_ACTORS;p++) if(mask&(1u<<p)) {
        int step_ok,leave_ok;
        if (!avp_mp_native_actor_enter(&scope,actors[p])) {ok=0;break;}
        step_ok=actor_step(user,p);
        leave_ok=avp_mp_native_actor_leave(&scope,actors[p]);
        if (!step_ok || !leave_ok) {ok=0;break;}
    }
    /* An AMP or door tick is a WORLD operation, never repeated per player. */
    if (ok) ok=world_step(user);
    if (!avp_mp_native_world_leave(&scope,w)) ok=0;
    return ok;
}
