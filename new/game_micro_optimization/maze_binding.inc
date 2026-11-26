/* Mirror only the collision binding, never the mutable wall or door data.
 * The original collision module has one writer: avp_collision_bind_maze.
 * All host/MP callers are external to that compilation unit. GNU wrap
 * intercepts every such rebinding, including when optimization is disabled.
 * The original setter is still called on every bind. The collision API is
 * simulation-thread-owned; no additional synchronization or state lifetime. */
static const u8 *micro_maze;
static unsigned micro_width,micro_height;
void __real_avp_collision_bind_maze(u8 *map,u16 width,u16 height);
void __wrap_avp_collision_bind_maze(u8 *map,u16 width,u16 height)
{
    __real_avp_collision_bind_maze(map,width,height);
    micro_maze=map;micro_width=width;micro_height=height;
}
