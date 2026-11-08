/* Original DOORS.S runtime state isolated in one process, no windows/workers.
 * Engine test uses only public original InitDoors, SaveDoors, ResetDoors and
 * the optional snapshot API. It does not fake the original door algorithm.
 */
#include "local_mp_doors_native.h"
#include "doors.h"
#include "levels.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} } while(0)

/* Standalone test provides the globals owned by the unlinked levels module.
 * Function-section GC discards the unused DOORS.S game entry points. */
s16 cur_level, new_level;
u8 panel_list[AVP_MAX_PANELS];
u32 levfx_ID;

int main(void)
{
    uint8_t host[4*4*8] = {0};
    uint8_t map_a[4*4*8] = {0};
    uint8_t map_b[4*4*8] = {0};
    uint8_t *a = &map_a[(1*4+1)*8];
    uint8_t *b = &map_b[(1*4+1)*8];
    AvpMpDoorEngineState host_before, host_after, world_a={0}, world_b={0};
    AvpMpDoorEngineState invalid, observed;
    AvpMpDoorScope scope={0}, nested={0};

    cur_level=9;
    avp_doors_bind_maze(host,4,4);
    InitDoors();
    panel_list[0]=93;
    acs_level=18;
    levfx_ID=321;
    CHECK(avp_mp_doors_native_capture(&host_before));

    /* Original Jaguar wall type 4, index 32, has DOOR classification. */
    a[0]=32;b[0]=32;
    a[6]=24;b[6]=8;
    CHECK(avp_mp_doors_world_init(&world_a,map_a,4,4,7));
    CHECK(avp_mp_doors_world_init(&world_b,map_b,4,4,8));
    CHECK(avp_mp_doors_native_capture(&host_after));
    CHECK(memcmp(&host_before,&host_after,sizeof(host_before))==0);

    /* Save opening byte using real SaveDoors() and hold the state for A. */
    CHECK(avp_mp_doors_scope_enter(&scope,&world_a));
    CHECK(cur_level==7);
    CHECK((fullbits[32]&AVP_WB_DOOR)!=0);
    CHECK(!avp_mp_doors_scope_enter(&nested,&world_b));
    SaveDoors();
    a[6]=0;
    ResetDoors();
    CHECK(a[6]==24);
    a[6]=0;
    CHECK(avp_mp_doors_scope_leave(&scope,&world_a));
    CHECK(!avp_mp_doors_scope_leave(&scope,&world_a));
    CHECK(cur_level==9 && levfx_ID==321 && acs_level==18);
    CHECK(avp_mp_doors_native_capture(&host_after));
    CHECK(memcmp(&host_before,&host_after,sizeof(host_before))==0);

    /* A different level/campaign has its OWN complete saved-door history. */
    CHECK(avp_mp_doors_scope_enter(&scope,&world_b));
    CHECK(cur_level==8);
    SaveDoors();
    b[6]=0;
    ResetDoors();
    CHECK(b[6]==8);
    b[6]=0;
    CHECK(avp_mp_doors_scope_leave(&scope,&world_b));

    /* Revisited world A still has the original saved value, not B's. */
    CHECK(avp_mp_doors_scope_enter(&scope,&world_a));
    ResetDoors();
    CHECK(a[6]==24);
    CHECK(b[6]==0);
    CHECK(avp_mp_doors_scope_leave(&scope,&world_a));
    CHECK(avp_mp_doors_scope_enter(&scope,&world_b));
    ResetDoors();
    CHECK(b[6]==8);
    CHECK(a[6]==24);
    CHECK(avp_mp_doors_scope_leave(&scope,&world_b));

    /* Active ordinary Jaguar door-slot internals are also saved per world.
     * Rebinding recreates the real slot pointer into A, never into B/host.
     * This tests state transfer, NOT a live multi-actor MoveDoors tick. */
    world_a.door[0].active=1;
    world_a.door[0].offset=(size_t)(&a[6]-map_a);
    world_a.door[0].direction=4;
    world_a.door[0].limit=48;
    world_a.door[0].xpos=0x10000;
    world_a.door[0].ypos=0x20000;
    world_a.door[0].grid1=65;
    CHECK(avp_mp_doors_scope_enter(&scope,&world_a));
    CHECK(avp_mp_doors_native_capture(&observed));
    CHECK(observed.door[0].active && observed.door[0].offset==(size_t)(&a[6]-map_a));
    CHECK(observed.door[0].direction==4 && observed.door[0].limit==48);
    CHECK(observed.door[0].grid1==65);
    CHECK(avp_mp_doors_scope_leave(&scope,&world_a));
    CHECK(avp_mp_doors_scope_enter(&scope,&world_b));
    CHECK(avp_mp_doors_native_capture(&observed));
    CHECK(!observed.door[0].active);
    CHECK(avp_mp_doors_scope_leave(&scope,&world_b));

    /* Reject invalid pointers with zero changes to legacy globals. */
    invalid=world_a;
    invalid.door[0].active=1;
    invalid.door[0].offset=999999;
    CHECK(!avp_mp_doors_scope_enter(&scope,&invalid));
    CHECK(avp_mp_doors_native_capture(&host_after));
    CHECK(memcmp(&host_before,&host_after,sizeof(host_before))==0);
    CHECK(!avp_mp_doors_native_restore(NULL));
    CHECK(!avp_mp_doors_world_init(&invalid,map_a,0,4,7));

    puts("PASS: original Jaguar SaveDoors/ResetDoors persisted separately per world, native private door state restored, no host contamination, no processes/windows");
    return 0;
}
