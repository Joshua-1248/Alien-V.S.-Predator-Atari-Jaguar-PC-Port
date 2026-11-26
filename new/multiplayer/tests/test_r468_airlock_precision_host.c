/* R425: exercise the real host coordinator (including its private state),
 * not a duplicate implementation. Compile with function-section GC, the
 * retail graph's original door/resource test objects and multiplayer lib.
 * The fixture supplies observed player poses; not a full native movement
 * simulation. Real original DoorKeys/MoveDoors execute on the ROM maze. */
#include "../local_mp_walktest.c"
#define main retail_graph_main_unused
#include "test_local_mp_bot_retail_graph.c"
#undef main
#define CHECK(e) do { if(!(e)){fprintf(stderr,"R467 host failure level=%u line=%d: %s\n",level,__LINE__,#e);goto done;} } while(0)
int main(int argc,char **argv)
{
    const unsigned origins[]={1u,5u,14u,15u};
    const unsigned exits[]={1169u,1225u,12483u,15539u};
    AvpRomImage rom={0};
    AvpMpWalkHost *t=calloc(1,sizeof *t);
    AvpMpActorNativeState *actors=calloc(2,sizeof *actors);
    AvpMpNativeWorld *world=calloc(1,sizeof *world);
    uint8_t *bytes=NULL; FILE *f=NULL; unsigned level=0;
    int rc=1;
    if(argc!=2||!t||!actors||!world)goto done;
    f=fopen(argv[1],"rb");rom.data=malloc(4194304u);rom.size=4194304u;
    if(!f||!rom.data||fread(rom.data,1,rom.size,f)!=rom.size||fgetc(f)!=EOF)goto done;
    fclose(f);f=NULL;rom.exact_retail_sha256=1;
    for(unsigned i=0;i<4u;i++){
        AvpMpRetailMap map={0};AvpMpAirlockRoom room={0};AvpMpBotObservation v={0};
        AvpMpPlayerCommand cmd={0};AvpDoorPanelIds ids;size_t n=0;
        level=origins[i];
        CHECK(avp_resources_decode_file(&rom,87u+level-1u,NULL,&bytes,&n));
        CHECK(n==8u+64u*64u*8u);
        map=(AvpMpRetailMap){bytes,n,64u,64u,level,1u};
        CHECK(avp_mp_airlock_room(&map,&retail_meta[level-1u],exits[i],&room));
        memset(t,0,sizeof *t);memset(actors,0,2*sizeof *actors);memset(world,0,sizeof *world);
        avp_local_mp_defaults(&t->session.setup);
        t->actors=t->players=2u;t->retail_catalog_ready=1;
        t->session.setup.players=2u;t->session.setup.campaign=0u;
        t->session.setup.level[0]=level;t->session.setup.level[1]=level;
        t->session.setup.bot_enabled[1]=1u;t->session.running=1;
        t->session.worlds[0]=(AvpMpActiveWorld){0u,level,3u,world,0u};
        t->session.player[0]=&actors[0];t->session.player[1]=&actors[1];
        actors[0].player.player_energy=actors[1].player.player_energy=1000;
        /* Human outside and not inviting the bot; the bot's own authored
         * transition request is the ONLY cause of the commitment. */
        unsigned outside_y=room.entry_side==3u?room.entry_y+1u:room.entry_y-1u;
        actors[0].player.x_pos=(room.entry_x<<16)|0x8000u;
        actors[0].player.y_pos=(outside_y<<16)|0x8000u;
        actors[1].player.x_pos=actors[0].player.x_pos;
        actors[1].player.y_pos=actors[0].player.y_pos;
        world->map=&map;world->doors.level_meta=retail_meta;
        world->doors.level_meta_count=AVP_MAX_LEVEL;
        world->doors.current_meta=&retail_meta[level-1u];
        cur_level=(s16)level;new_level=0;pressure_count=notsecure_count=0;
        player_type=PT_HUMAN;InitDoors();avp_doors_bind_maze(bytes+8,64,64);
        avp_doors_bind_level_meta(retail_meta,AVP_MAX_LEVEL);
        CHECK(avp_resources_retail_panel_info(&rom,level,&ids,panel_list,NULL));
        avp_doors_bind_panel_ids(&ids);acs_level=10u;
        uint8_t *cells=bytes+8;cells[room.entry_door_offset]=cells[room.exit_door_offset]=0;
        v.map=(AvpMpBotMap){cells,64,64,fullbits,&retail_meta[level-1u],10u,NULL,NULL,0u};
        v.x=actors[1].player.x_pos;v.y=actors[1].player.y_pos;v.tick=1;
        v.player_energy=v.max_energy=1000;v.goal=AVP_MP_BOT_EXIT;v.has_target=1;
        v.target_x=(room.exit_x<<16)|0x8000;v.target_y=(room.exit_y<<16)|0x8000;
        v.target_side=room.exit_side;v.current_level=level;v.destination_level=room.destination;
        walk_bot_coordinate_airlock(t,1,world,&v);
        CHECK(v.goal==AVP_MP_BOT_AIRLOCK_SEAT && t->bots[1].airlock_panel_offset==exits[i]);
        door_use(room.entry_x,outside_y,(room.entry_side+2u)&3u,0x8000,0x8000);
        for(unsigned k=0;k<40;k++)MoveDoors();
        CHECK(cells[room.entry_door_offset]>0);
        /* More than 200 ticks later a routine patrol replan occurs. The
         * physical bot is now alone INSIDE and at the entrance threshold. */
        v.tick=500;v.goal=AVP_MP_BOT_PATROL;v.destination_level=0;
        v.x=actors[1].player.x_pos=(room.entry_x<<16)|0x8000;
        v.y=actors[1].player.y_pos=(room.entry_y<<16)|(room.entry_side==3u?0xe000u:0x2000u);
        v.heading=room.entry_side==1u?0x40000000u:0xc0000000u;
        walk_bot_coordinate_airlock(t,1,world,&v);
        CHECK(v.goal==AVP_MP_BOT_AIRLOCK_CLOSE && v.airlock_door_ready);
        avp_mp_bot_step_precise(&t->bots[1],&v,0,&cmd);
        CHECK(!(cmd.actions&(1u<<AVP_MP_USE)) && (cmd.actions&(1u<<AVP_MP_BACKWARD)));
        v.y=actors[1].player.y_pos=(room.entry_y<<16)|0x8000;
        v.tick++;walk_bot_coordinate_airlock(t,1,world,&v);
        avp_mp_bot_step_precise(&t->bots[1],&v,0,&cmd);
        CHECK(cmd.actions&(1u<<AVP_MP_USE));
        door_use(room.entry_x,room.entry_y,room.entry_side,0x8000,0x8000);
        for(unsigned k=0;k<40&&cells[room.entry_door_offset];k++){
            v.tick++;v.goal=AVP_MP_BOT_PATROL;
            v.active_door_count=1;v.active_door_offsets[0]=room.entry_door_offset;
            walk_bot_coordinate_airlock(t,1,world,&v);
            avp_mp_bot_step_precise(&t->bots[1],&v,0,&cmd);
            CHECK(v.goal==AVP_MP_BOT_AIRLOCK_SEAT && !(cmd.actions&(1u<<AVP_MP_USE)));
            MoveDoors();
        }
        CHECK(!cells[room.entry_door_offset]);
        v.tick+=40;v.active_door_count=0;v.goal=AVP_MP_BOT_PATROL;
        v.x=actors[1].player.x_pos=(room.exit_x<<16)|0x8000;
        v.y=actors[1].player.y_pos=(room.exit_y<<16)|0x8000;
        v.heading=room.exit_side==1u?0x40000000u:0xc0000000u;
        walk_bot_coordinate_airlock(t,1,world,&v);
        CHECK(v.goal==AVP_MP_BOT_EXIT && v.airlock_pressure);
        avp_mp_bot_step_precise(&t->bots[1],&v,0,&cmd);CHECK(cmd.actions&(1u<<AVP_MP_USE));
        /* Another actor opens the entry AFTER the bot planned pressure. */
        door_use(room.entry_x,room.entry_y,room.entry_side,0x8000,0x8000);
        CHECK(!cells[room.entry_door_offset]);
        CHECK(avp_mp_doors_native_capture(&world->doors));
        CHECK(world->doors.door[0].active || world->doors.door[1].active);
        avp_doors_mp_bot_airlock_interlock(1);
        door_use(room.exit_x,room.exit_y,room.exit_side,0x8000,0x8000);
        CHECK(!new_level && !pressure_count);
        for(unsigned k=0;k<40;k++)MoveDoors();
        CHECK(cells[room.entry_door_offset]>0);
        door_use(room.exit_x,room.exit_y,room.exit_side,0x8000,0x8000);
        CHECK(!new_level && !pressure_count);
        door_use(room.entry_x,room.entry_y,room.entry_side,0x8000,0x8000);
        for(unsigned k=0;k<40;k++)MoveDoors();
        CHECK(!cells[room.entry_door_offset]);
        door_use(room.exit_x,room.exit_y,room.exit_side,0x8000,0x8000);
        CHECK(new_level==(s16)room.destination && pressure_count==1u && !notsecure_count);
        AvpMpTransitionRequest request={0};
        request.target_level=new_level;request.new_x=new_x;request.new_y=new_y;
        CHECK(avp_mp_doors_native_capture(&world->doors));
        CHECK(walk_bot_airlock_commit_ready(world,&actors[1],&request));
        /* Both doors, every nonzero opening byte, either active slot. */
        for(unsigned d=0;d<2;d++){
            unsigned off=d?room.exit_door_offset:room.entry_door_offset;
            for(unsigned value=1;value<=255;value++){
                cells[off]=(uint8_t)value;
                CHECK(!walk_bot_airlock_commit_ready(world,&actors[1],&request));
            }
            cells[off]=0;
            for(unsigned slot=0;slot<2;slot++){
                world->doors.door[slot].active=1;
                world->doors.door[slot].offset=off;
                CHECK(!walk_bot_airlock_commit_ready(world,&actors[1],&request));
                world->doors.door[slot].active=0;
            }
        }
        CHECK(walk_bot_airlock_commit_ready(world,&actors[1],&request));
        /* Real same-tick reopening AFTER pressure invalidates commit. */
        door_use(room.entry_x,room.entry_y,room.entry_side,0x8000,0x8000);
        CHECK(!cells[room.entry_door_offset]);
        CHECK(avp_mp_doors_native_capture(&world->doors));
        CHECK(!walk_bot_airlock_commit_ready(world,&actors[1],&request));
        /* Human native behavior remains unchanged. */
        avp_doors_mp_bot_airlock_interlock(0);
        new_level=0;
        door_use(room.exit_x,room.exit_y,room.exit_side,0x8000,0x8000);
        CHECK(new_level==(s16)room.destination && pressure_count==2u);
        CHECK(!notsecure_count);
        printf("R467 production coordinator solo bot %u -> %u after 499-tick replan: PASS\n",level,room.destination);
        free(bytes);bytes=NULL;
    }
    rc=0;
done:
    if(f)fclose(f);
    free(bytes);free(rom.data);free(world);free(actors);free(t);return rc;
}
