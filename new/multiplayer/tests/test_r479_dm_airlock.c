#include "../local_mp_walktest.c"
#include <assert.h>
static uint8_t bytes[8+64*64*8];
static uint8_t *cell(unsigned x,unsigned y){return bytes+8+(y*64+x)*8;}
static AvpMpBotObservation goal(AvpMpBotObservation base){
    base.goal=AVP_MP_BOT_EXIT;base.has_target=1;base.exit_is_duct=0;
    base.target_x=0xa8000;base.target_y=0xa8000;base.target_side=1;
    base.current_level=1;base.destination_level=15;base.airlock_pressure=0;
    return base;
}
int main(void){
    AvpMpWalkHost *t=calloc(1,sizeof *t);AvpMpNativeWorld *w=calloc(1,sizeof *w);
    assert(t&&w);AvpMpRetailMap map={bytes,sizeof bytes,64,64,1,1};
    AvpDoorLevelMeta meta[AVP_MAX_LEVEL]={0};
    u16 required=(14*64+10)*8+7;AvpDoorExitEntry e={(10*64+10)*8+1,10,10,15};
    AvpDoorInterlockEntry inter={(10*64+10)*8+7,&required,1};
    meta[0].exits=&e;meta[0].exit_count=1;meta[0].interlocks=&inter;meta[0].interlock_count=1;
    for(unsigned y=10;y<=13;y++)for(unsigned x=9;x<=11;x++){
        uint8_t *c=cell(x,y);c[5]=1;
        if(x==9)c[0]=8;if(x==11)c[2]=8;if(y==10)c[1]=8;if(y==13)c[3]=8;
    }
    cell(10,10)[1]=cell(10,13)[3]=32;cell(10,9)[3]=cell(10,14)[1]=32;
    AvpMpAirlockRoom room;assert(avp_mp_airlock_room(&map,&meta[0],e.panel_offset,&room));
    t->actors=2;t->retail_catalog_ready=1;t->dm.enabled=t->dm.level_travel=1;
    t->session.setup.bot_enabled[1]=1;t->session.worlds[0]=(AvpMpActiveWorld){0,1,3,w,0};
    w->map=&map;w->doors.level_meta=meta;w->doors.level_meta_count=AVP_MAX_LEVEL;
    AvpMpBotObservation base={0};base.x=0xa8000;base.y=0xb8000;base.tick=100;
    base.map.cells=bytes+8;base.map.width=base.map.height=64;base.deathmatch=1;
    AvpMpBotObservation v=goal(base);uint8_t *cells=bytes+8;
    cells[room.entry_door_offset]=48;cells[room.exit_door_offset]=48;
    walk_bot_coordinate_airlock(t,1,w,&v);assert(v.goal==AVP_MP_BOT_AIRLOCK_CLOSE && v.target_y>>16==room.entry_y && v.airlock_door_ready);
    v=goal(base);v.active_door_count=1;v.active_door_offsets[0]=room.entry_door_offset;
    walk_bot_coordinate_airlock(t,1,w,&v);assert(v.goal==AVP_MP_BOT_AIRLOCK_SEAT&&!v.airlock_pressure);
    cells[room.entry_door_offset]=0;v=goal(base);walk_bot_coordinate_airlock(t,1,w,&v);
    assert(v.goal==AVP_MP_BOT_AIRLOCK_CLOSE&&v.target_y>>16==room.exit_y&&v.airlock_door_ready);
    cells[room.exit_door_offset]=0;v=goal(base);walk_bot_coordinate_airlock(t,1,w,&v);assert(v.goal==AVP_MP_BOT_EXIT&&v.airlock_pressure);
    v=goal(base);v.y=0xe8000;walk_bot_coordinate_airlock(t,1,w,&v);assert(v.goal==AVP_MP_BOT_EXIT&&!v.airlock_pressure);
    v=goal(base);v.hostile_target=v.target_visible=1;walk_bot_coordinate_airlock(t,1,w,&v);assert(!v.airlock_pressure);
    t->dm.level_travel=0;v=goal(base);walk_bot_coordinate_airlock(t,1,w,&v);assert(!v.airlock_pressure);
    t->dm.level_travel=1;avp_mp_airlock_cycle_arrived(&t->bots[1].airlock_cycle,15,1,99);
    v=goal(base);walk_bot_coordinate_airlock(t,1,w,&v);assert(v.goal==AVP_MP_BOT_PATROL&&!v.destination_level);
    free(w);free(t);puts("PASS synthetic production Deathmatch coordinator: entry close, animation wait, exit close, pressure, outside/combat/travel guards, reverse arrival egress");return 0;
}
