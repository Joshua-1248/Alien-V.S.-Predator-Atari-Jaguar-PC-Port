#include "local_mp_bot_levels.h"
#include "local_mp_chamber.h"
#include "local_mp_input.h"
#include "avp_resources.h"
#include "doors.h"
#include "levels.h"
#include "player.h"
#include "amp.h"
#include "joypad.h"
#include "hud_message.h"
#include "avp_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Preserve production's exact source-authored metadata without exporting a
 * new gameplay-global API. The included definitions remain in this test TU. */
#include "../../../new/jaguar/retail_data/door_retail_meta.c"
u32 levfx_ID;
u8 panel_list[AVP_MAX_PANELS];
u32 panel_pos;
/* Stub hardware/HUD only. Original DoorKeys and MoveDoors operate on
 * decoded retail cells. */
s16 cur_level,new_level,player_type;
s32 new_x,new_y;
u32 x_pos,y_pos,centre_angle,new_pos;
volatile u32 joy_cur,joy_edge;
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
AvpAmp *amp_data;
static unsigned pressure_count,notsecure_count;
static const AvpRuntimeOps silent_ops={0};
const AvpRuntimeOps *avp_runtime_ops(void){return &silent_ops;}
#define MSG(n) const AvpHudMessageStep avp_msg_##n[]={{0,{NULL,NULL,NULL}}}
MSG(pressure); MSG(notsecure); MSG(airlocked); MSG(jammed); MSG(jlift);
MSG(escape); MSG(access_denied);
void avp_hudmsg_queue(const AvpHudMessageStep *m){
    if(m==avp_msg_pressure)pressure_count++;
    if(m==avp_msg_notsecure)notsecure_count++;
}
void avp_hudmsg_set_access_code(unsigned code){(void)code;}
u32 DoEffect(unsigned id){(void)id;return 1u;}
static void door_use(unsigned x,unsigned y,unsigned side,uint32_t fx,uint32_t fy)
{
    static const uint32_t headings[4]={0x80000000u,0x40000000u,0u,0xc0000000u};
    x_pos=(x<<16)|fx;y_pos=(y<<16)|fy;centre_angle=headings[side];
    new_pos=(y*64u+x)*8u;
    joy_cur=joy_edge=~(1u<<FIRE_B);
    DoorKeys();
    joy_cur=joy_edge=~0u;
}

int main(int argc,char **argv)
{
    AvpRomImage rom={0};
    AvpMpBotLevelLayout levels[AVP_MAX_LEVEL]={0};
    uint8_t *maps[AVP_MAX_LEVEL]={0};
    AvpMpBotLevelPlan plan;
    unsigned tested=0,found=0,vent=0,first_failed=0;
    FILE *f=NULL;
    int rc=1;
    long size;
    if(argc!=2){fprintf(stderr,"usage: audit ORIGINAL_RETAIL_ROM.jag\n");return 2;}
    f=fopen(argv[1],"rb");
    if(!f||fseek(f,0,SEEK_END)||(size=ftell(f))!=4194304L||fseek(f,0,SEEK_SET))goto done;
    rom.data=(u8*)malloc((size_t)size);if(!rom.data)goto done;
    rom.size=(size_t)size;
    if(fread(rom.data,1,rom.size,f)!=rom.size)goto done;
    fclose(f);f=NULL;
    /* Caller verifies the canonical ROM digest. This harness inspects only
     * the user's local authentic original file, never exports the bytes. */
    rom.exact_retail_sha256=1;
    InitDoors();
    memset(panel_list,0xa5,sizeof panel_list);
    panel_pos=37u;
    for(unsigned i=0;i<AVP_MAX_LEVEL;i++){
        size_t n=0;
        unsigned w,h;
        if(!avp_resources_decode_file(&rom,87u+i,NULL,&maps[i],&n) ||
           !maps[i]||n<16u)goto done;
        w=(unsigned)maps[i][0]*256u+maps[i][1];
        h=(unsigned)maps[i][2]*256u+maps[i][3];
        if(w==0||h==0||w>64u||h>64u||n!=8u+w*h*8u)goto done;
        if(!avp_resources_retail_panel_info(&rom,i+1u,
                      &levels[i].panel_ids,levels[i].panel_list,NULL))goto done;
        levels[i].map=(AvpMpBotMap){maps[i]+8u,w,h,fullbits,
                                    &retail_meta[i],10u,NULL,NULL,0u};
        levels[i].meta=&retail_meta[i];
    }
    /* R300: unlike arbitrary floor-looking cells, all these pod spaces
     * have a REAL connected original path from the launch panel. The first
     * ten border actual source walls, with their backs against them. */
    {
        const AvpMpBotMap *m=&levels[4].map;
        const unsigned bx[12]={19,19,20,20,22,22,23,23,21,21,19,20};
        const unsigned by[12]={52,54,52,54,52,54,52,54,53,54,53,53};
        const unsigned dx[12]={0,0,0,0,0,0,1,1,1,0,0,0};
        const int dy[12]={-1,1,-1,1,-1,1,0,0,0,1,0,0};
        AvpMpBotRoute path={0};
        if(27272u/8u!=53u*64u+17u ||
           !avp_mp_bot_playable_position(m->cells,m->width,m->height,
               (17u<<16)|0x8000u,(53u<<16)|0x8000u))goto done;
        for(unsigned j=0;j<12u;j++){
            const unsigned x=bx[j],y=by[j];
            const int a=avp_mp_bot_playable_position(m->cells,m->width,m->height,(x<<16)|0x8000u,(y<<16)|0x8000u);
            AvpMpBotRoute across_wall={0};
            const int b=j<10u&&avp_mp_bot_route(m,x,y,x+dx[j],(unsigned)((int)y+dy[j]),&across_wall);
            const int c=avp_mp_bot_route(m,17u,53u,x,y,&path);
            if(!a || b || !c){
                fprintf(stderr,"actual tests a=%d b=%d c=%d for x=%u y=%u wallx=%u wally=%u\n",a,b,c,x,y,x+dx[j],(unsigned)((int)y+dy[j]));
                fprintf(stderr,"R300 authentic pod seat #%u (%u,%u) not reachable/wall-side\n",j,x,y);
                goto done;
            }
        }
        /* Marine's original campaign start is Level 3, coordinate (36,10).
         * The authored graph must be able to reach the ORIGINAL escape
         * chamber through physical lifts and doors; no injected paths. */
        {
            AvpMpBotLevelPlan evac={0};
            if(!avp_mp_bot_plan_levels(levels,AVP_MP_MARINE,3u,
                   36u,10u,5u,20u,53u,1,&evac)){
                fprintf(stderr,"R300 no source-authored evacuation route from Marine start to Level-5 capsule\n");
                goto done;
            }
            if(evac.kind==0u || evac.to_level==0u)goto done;
        }
        /* A floor-looking room on the far side of the maze wall cannot be
         * used merely because its center pixel is legal. */
        if(avp_mp_bot_route(m,17u,53u,24u,53u,&path))goto done;
        fprintf(stderr,"R300 pod: real panel, 10 wall-side seats, two overflow seats and disconnected-room guard PASS\n");
    }
    /* R296: real, bounded ship chambers on BOTH sides of each airlock.
     * Confirm four physical recessed seats, alternating left/right, with
     * every seated bot facing AWAY from the wall behind its back. */
    {
        const unsigned levels_to_test[4]={1u,5u,14u,15u};
        const unsigned exits[4]={1169u,1225u,12483u,15539u};
        const unsigned destinations[4]={15u,14u,5u,1u};
        for(unsigned j=0;j<4u;j++){
            const unsigned level=levels_to_test[j];
            AvpMpAirlockRoom room={0};
            AvpMpBotObservation stance={0};
            AvpMpBotState state={0};
            AvpMpPlayerCommand cmd={0};
            const unsigned req=retail_meta[level-1u].interlocks[0].required_offsets[0];
            uint8_t *cell=maps[level-1u]+8u;
            uint8_t old_req,old_exit;
            if(avp_mp_airlock_room(NULL,&retail_meta[level-1u],
                                  exits[j],&room))goto done;
            AvpMpRetailMap raw={0};
            raw.bytes=maps[level-1u];raw.size=8u+64u*64u*8u;raw.width=raw.height=64u;
            raw.level=level;
            int ok=avp_mp_airlock_room(&raw,&retail_meta[level-1u],exits[j],&room);
            if(!ok || room.seat_count!=4u ||
               room.destination!=destinations[j] ||
               room.entry_door_offset!=req)goto done;
            for(unsigned k=0;k<room.seat_count;k++){
                const unsigned x=room.seats[k].x,y=room.seats[k].y;
                const unsigned toward=room.seats[k].face_side;
                const unsigned dx=x>room.exit_x?x-room.exit_x:room.exit_x-x;
                if(dx!=1u || (x<room.exit_x && toward!=2u) ||
                   (x>room.exit_x && toward!=0u) ||
                   !avp_mp_chamber_contains(&raw,
                     (room.exit_x<<16)|0x8000u,
                     (room.exit_y<<16)|0x8000u,
                     (x<<16)|0x8000u,(y<<16)|0x8000u)){
                    fprintf(stderr,"AIRLOCK SEAT INVALID: level%u seat%u\n",level,k);
                    goto done;
                }
            }
            /* R299: the guard must use REAL authored recess cells and
             * panel destinations on each retail side of BOTH ships. A
             * completed transfer is not an invitation to reverse it, even
             * if all three bots and the human remain inside the chamber. */
            {
                AvpMpAirlockCycle arrivals[3]={{0}};
                uint32_t inside_x=(room.seats[0].x<<16)|0x8000u;
                uint32_t inside_y=(room.seats[0].y<<16)|0x8000u;
                int physically_inside=avp_mp_chamber_contains(&raw,
                    (room.exit_x<<16)|0x8000u,
                    (room.exit_y<<16)|0x8000u,inside_x,inside_y);
                if(!physically_inside)goto done;
                for(unsigned q=0;q<3u;q++){
                    avp_mp_airlock_cycle_arrived(&arrivals[q],
                        room.destination,level,42u);
                    for(unsigned tick=43u;tick<843u;tick+=11u)
                        if(!avp_mp_airlock_cycle_reverse_block(&arrivals[q],
                            level,room.destination,physically_inside,
                            0,1,tick))goto done;
                    if(!avp_mp_airlock_cycle_reverse_block(&arrivals[q],
                            level,room.destination,physically_inside,
                            1,1,844u))goto done;
                    if(avp_mp_airlock_cycle_reverse_block(&arrivals[q],
                            level,room.destination,0,1,1,845u))goto done;
                }
            }
            /* An OPEN entrance does NOT remove the original portal from
             * the high-level BOT planner. Only actual DoorKeys Use needs
             * CLOSED interlock bytes to trigger pressure/transfer. */
            old_req=cell[req];old_exit=cell[room.exit_door_offset];
            cell[req]=48u;cell[room.exit_door_offset]=48u;
            if(!avp_mp_bot_plan_levels(levels,AVP_MP_MARINE,level,
                   room.exit_x,room.exit_y,destinations[j],0u,0u,0,&plan)){
                fprintf(stderr,"OPEN SHIP AIRLOCK PLANNER FAILURE: level%u\n",level);
                goto done;
            }
            cell[req]=old_req;cell[room.exit_door_offset]=old_exit;
            /* A native Use command for the entrance must be inhibited
             * while the original door is in motion. Once fully open,
             * one valid operator may request closing it from INSIDE. */
            {
                AvpMpBotObservation use={0};
                AvpMpBotState operator_state={0};
                AvpMpPlayerCommand before={0},after={0};
                use.x=(room.entry_x<<16)|0x8000u;
                use.y=(room.entry_y<<16)|0x8000u;
                use.target_x=use.x;use.target_y=use.y;
                use.heading=room.entry_side==1u?0x40000000u:0xc0000000u;
                use.target_side=room.entry_side;
                use.has_target=1u;use.goal=AVP_MP_BOT_AIRLOCK_CLOSE;
                use.player_energy=1000;use.max_energy=1000;
                use.map=levels[level-1u].map;
                use.tick=80u;
                use.airlock_door_ready=0u;
                avp_mp_bot_step(&operator_state,&use,&before);
                if(before.actions&(1u<<AVP_MP_USE)){
                    fprintf(stderr,"ANIMATED AIRLOCK DOOR REVERSED level%u\n",level);
                    goto done;
                }
                use.tick=100u;use.airlock_door_ready=1u;
                avp_mp_bot_step(&operator_state,&use,&after);
                if(!(after.actions&(1u<<AVP_MP_USE))){
                    fprintf(stderr,"OPEN AIRLOCK ENTRANCE NOT CLOSED level%u\n",level);
                    goto done;
                }
            }
            /* R425: native entrance and pressure sequence on all four
             * original portals. Movement is tested as commands; harness
             * places the operator at its authored panel to execute Use. */
            {
                const unsigned entry=room.entry_door_offset,exit=room.exit_door_offset;
                const unsigned outer_y=room.entry_side==3u?room.entry_y+1u:room.entry_y-1u;
                const unsigned outer_side=(room.entry_side+2u)&3u;
                uint32_t active[2]={entry,exit};
                AvpMpBotObservation v={0}; AvpMpBotState bot={0}; AvpMpPlayerCommand c={0};
                const unsigned motion=(1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                    (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT);
                cur_level=(s16)level;new_level=0;pressure_count=notsecure_count=0;
                player_type=PT_HUMAN;InitDoors();
                avp_doors_bind_maze(cell,64,64);avp_doors_bind_level_meta(retail_meta,AVP_MAX_LEVEL);
                avp_doors_bind_panel_ids(&levels[level-1u].panel_ids);
                memcpy(panel_list,levels[level-1u].panel_list,sizeof panel_list);acs_level=10u;
                cell[entry]=cell[exit]=0u;
                if(avp_mp_airlock_action(&room,cell,32768u,active,1u)!=AVP_MP_AIRLOCK_WAIT ||
                   avp_mp_airlock_action(&room,cell,32768u,active+1,1u)!=AVP_MP_AIRLOCK_WAIT ||
                   !avp_mp_airlock_commitment_valid(600u,1u,1,0) ||
                   !avp_mp_airlock_commitment_valid(600u,1u,0,1) ||
                   avp_mp_airlock_commitment_valid(600u,1u,0,0))goto done;
                door_use(room.entry_x,outer_y,outer_side,0x8000u,0x8000u);
                for(unsigned n=0;n<40u;n++)MoveDoors();
                if(!cell[entry] || new_level){fprintf(stderr,"R425 native open failure level=%u byte=%u\n",level,cell[entry]);goto done;}
                if(avp_mp_airlock_action(&room,cell,32768u,NULL,0u)!=AVP_MP_AIRLOCK_CLOSE_ENTRY)goto done;
                v.x=(room.entry_x<<16)|0x8000u;
                v.y=(room.entry_y<<16)|(room.entry_side==3u?0xe000u:0x2000u);
                v.target_x=v.x;v.target_y=(room.entry_y<<16)|0x8000u;
                v.heading=room.entry_side==1u?0x40000000u:0xc0000000u;
                v.target_side=room.entry_side;v.has_target=1u;
                v.goal=AVP_MP_BOT_AIRLOCK_CLOSE;v.airlock_door_ready=1u;
                v.player_energy=v.max_energy=1000;v.map=levels[level-1u].map;v.tick=500u;
                avp_mp_bot_step(&bot,&v,&c);
                if((c.actions&(1u<<AVP_MP_USE)) || !(c.actions&motion)){
                    fprintf(stderr,"R425 threshold clearance failed level=%u actions=%x\n",level,c.actions);goto done;}
                v.y=v.target_y;v.tick++;avp_mp_bot_step(&bot,&v,&c);
                if(!(c.actions&(1u<<AVP_MP_USE)) || (c.actions&motion))goto done;
                door_use(room.entry_x,room.entry_y,room.entry_side,0x8000u,0x8000u);
                for(unsigned n=0;n<40u && cell[entry];n++){
                    if(avp_mp_airlock_action(&room,cell,32768u,active,1u)!=AVP_MP_AIRLOCK_WAIT)goto done;
                    MoveDoors();
                }
                if(cell[entry] || new_level || avp_mp_airlock_action(&room,cell,32768u,NULL,0u)!=AVP_MP_AIRLOCK_PRESSURE){
                    fprintf(stderr,"R425 native close failure level=%u byte=%u\n",level,cell[entry]);goto done;}
                v.x=v.target_x=(room.exit_x<<16)|0x8000u;
                v.y=v.target_y=(room.exit_y<<16)|0x8000u;
                v.heading=room.exit_side==1u?0x40000000u:0xc0000000u;
                v.target_side=room.exit_side;v.goal=AVP_MP_BOT_EXIT;
                v.airlock_pressure=1u;v.current_level=level;v.destination_level=room.destination;
                v.tick+=40u;avp_mp_bot_step(&bot,&v,&c);
                if(!(c.actions&(1u<<AVP_MP_USE)))goto done;
                door_use(room.exit_x,room.exit_y,room.exit_side,0x8000u,0x8000u);
                if(new_level!=(s16)room.destination || pressure_count!=1u || notsecure_count){
                    fprintf(stderr,"R425 native pressure failure level=%u to=%d pressure=%u notsecure=%u\n",level,new_level,pressure_count,notsecure_count);goto done;}
                printf("R425 level %u -> %u: native entrance open, clearance, native close, pressure accepted PASS\n",level,room.destination);
                cell[entry]=old_req;cell[exit]=old_exit;
                memset(panel_list,0xa5,sizeof panel_list);
            }
            /* Seated right-recess bot facing EAST (toward its side wall)
             * must turn WEST, away from the wall, without Use/movement. */
            stance.x=(room.seats[1].x<<16)|0x8000u;
            stance.y=(room.seats[1].y<<16)|0x8000u;
            stance.target_x=stance.x;stance.target_y=stance.y;
            stance.has_target=1u;
            stance.heading=0u;stance.goal=AVP_MP_BOT_AIRLOCK_SEAT;
            stance.airlock_face_side=room.seats[1].face_side;
            stance.player_energy=1000;stance.max_energy=1000;
            stance.map=levels[level-1u].map;
            avp_mp_bot_step(&state,&stance,&cmd);
            if(!(cmd.actions&((1u<<AVP_MP_TURN_LEFT)|(1u<<AVP_MP_TURN_RIGHT))) ||
               (cmd.actions&((1u<<AVP_MP_FORWARD)|(1u<<AVP_MP_BACKWARD)|
                              (1u<<AVP_MP_STRAFE_LEFT)|(1u<<AVP_MP_STRAFE_RIGHT)|
                              (1u<<AVP_MP_USE)))){
                fprintf(stderr,"AIRLOCK SEATED INWARD FACING FAILURE: level%u\n",level);
                goto done;
            }
        }
        puts("R296 source ship airlock recesses and inward-wall-facing, all four portals: PASS");
    }
    /* R295 live retail Brig supply route: starting Marine position and
     * original green-uniform Pulse Rifle rounds corpse, not a dummy maze. */
    {
        AvpMpBotRoute brig={0};
        int direct=avp_mp_bot_route(&levels[2].map,36u,10u,0x28u,0x1du,&brig);
        if(!direct || !brig.valid)goto done;
        printf("R295 BRIG pulse ammo source cell (40,29) from Marine start (36,10): route=%d cost=%u next=(%u,%u)\n",
               direct,brig.cost,brig.next_x,brig.next_y);
    }
    if(panel_pos!=37u)goto done;
    for(unsigned k=0;k<AVP_MAX_PANELS;k++)if(panel_list[k]!=0xa5u)goto done;
    /* The original bind operation must receive the SAME index positions
     * and count as the new pure query, preserving retail startup exactly. */
    for(unsigned i=0;i<AVP_MAX_LEVEL;i++){
        AvpDoorPanelIds bound;
        uint8_t verified[AVP_MAX_PANELS];
        unsigned verified_count=0u;
        if(!avp_resources_retail_panel_info(&rom,i+1u,&bound,verified,
                                            &verified_count)||
           !avp_resources_bind_retail_panel_info(&rom,i+1u,&bound)||
           panel_pos!=verified_count ||
           memcmp(panel_list,verified,sizeof verified))goto done;
    }
    for(unsigned i=0;i<AVP_MAX_LEVEL;i++){
        const AvpDoorLevelMeta *m=&retail_meta[i];
        if(i<5u)for(size_t j=0;j<m->access_count;j++){
            const AvpDoorAccessEntry *a=&m->access[j];
            unsigned width=levels[i].map.width;
            unsigned cell=a->door_offset/8u,x=cell%width,y=cell/width;
            unsigned ax=x,ay=y,bx=x,by=y;
            if(a->code!=AVP_ACC_JAMMED)continue;
            if(a->door_offset%8u==6u){if(!x)continue;ax=x-1u;}
            else if(a->door_offset%8u==7u){if(!y)continue;ay=y-1u;}
            else continue;
            tested++;
            if(avp_mp_bot_plan_levels(levels,0u,i+1u,
                             ax,ay,i+1u,bx,by,1,&plan)){
                found++;
                if(plan.kind==AVP_MP_BOT_TRANS_DUCT)vent++;
                printf("JAMMED_LEVEL %u cell(%u,%u) to (%u,%u) => detour via level %u %s (%u hops)\n",
                       i+1u,ax,ay,bx,by,plan.to_level,
                       plan.kind==AVP_MP_BOT_TRANS_DUCT?"VENT":"OTHER",plan.hops);
            }else{
                uint16_t groups[AVP_MP_BOT_MAP_CELLS]={0};
                unsigned comp=avp_mp_bot_components(&levels[i].map,groups);
                printf("NO DETOUR jam level%u door(%u,%u) side%u: comp %u / %u (total %u)\n",
                       i+1u,x,y,a->door_offset%8u,
                       groups[ay*64u+ax],groups[by*64u+bx],comp);
            }
        }
    }
    /* Every successful planned hop must match one of the real records, and
     * no synthetic path exists in a wholly nonfloor map. */
    {
        unsigned examined=0;
        for(unsigned i=0;i<AVP_MAX_LEVEL;i++){
            const AvpDoorLevelMeta *m=&retail_meta[i];
            for(size_t j=0;j<m->exit_count;j++){
                unsigned x=(m->exits[j].panel_offset/8u)%levels[i].map.width;
                unsigned y=(m->exits[j].panel_offset/8u)/levels[i].map.width;
                unsigned dst=m->exits[j].level;
                if(x>=levels[i].map.width||y>=levels[i].map.height||dst<1u||dst>15u)continue;
                examined++;
                if(!avp_mp_bot_plan_levels(levels,0u,i+1u,x,y,dst,0u,0u,0,&plan)){
                    printf("AUTHORED_EXIT_UNREACHABLE level%u tile(%u,%u) target%u panel_offset%u SOURCE panel:%u file:%u floor:%u DEST floor:%u\n",
                           i+1u,x,y,dst,m->exits[j].panel_offset,
                           levels[i].map.cells[m->exits[j].panel_offset],
                           levels[i].panel_list[(levels[i].map.cells[m->exits[j].panel_offset]&0x7f)-1u],
                           levels[i].map.cells[((size_t)y*levels[i].map.width+x)*8u+5u],
                           levels[dst-1u].map.cells[((size_t)m->exits[j].y*levels[dst-1u].map.width+m->exits[j].x)*8u+5u]);
                    first_failed++;
                }
            }
        }
        printf("RETAIL GRAPH: %u authored exits examined; %u first-hop searches unavailable; %u jammed side pairs inspected, %u detours (%u vent)\n",
               examined,first_failed,tested,found,vent);
        if(examined!=64u||tested!=8u||found<7u||vent<7u||first_failed)goto done;
    }
    /* R279: real retail ship airlocks, in BOTH directions. The source
     * LEVDOOR records require these exact level links; a bot must take a
     * real EXIT panel with a walkable landing, never a fabricated teleport. */
    {
        const unsigned origins[4]={1u,5u,14u,15u};
        const unsigned destinations[4]={15u,14u,5u,1u};
        for(unsigned i=0;i<4u;i++){
            unsigned from=origins[i],to=destinations[i];
            const AvpDoorLevelMeta *m=&retail_meta[from-1u];
            int reachable=0;
            for(size_t j=0;j<m->exit_count;j++){
                const AvpDoorExitEntry *e=&m->exits[j];
                unsigned x=e->panel_offset/8u%levels[from-1u].map.width;
                unsigned y=e->panel_offset/8u/levels[from-1u].map.width;
                if(e->level!=to)continue;
                if(avp_mp_bot_plan_levels(levels,AVP_MP_MARINE,from,
                         x,y,to,0u,0u,0,&plan) &&
                   plan.from_level==from && plan.to_level==to &&
                   plan.kind==AVP_MP_BOT_TRANS_EXIT && plan.hops==1u)
                    reachable=1;
            }
            if(!reachable){
                fprintf(stderr,"ORIGINAL AIRLOCK ROUTE MISSING: %u -> %u\n",from,to);
                goto done;
            }
        }
        puts("R279 original ship airlocks: level 1<->15 and level 5<->14 PASS");
    }
    {
        uint16_t groups[AVP_MP_BOT_MAP_CELLS]={0};
        unsigned count=avp_mp_bot_components(&levels[4].map,groups);
        if(count<2u){fprintf(stderr,"Sub-Level 5 partition missing\n");goto done;}
        printf("R279 original Sub-Level 5 separate navigable regions: %u\n",count);
    }
    /* R125: For each genuine authored vent exit that reaches a surface,
     * the idle Deathmatch search must find a physically valid first hop.
     * This uses the canonical 15 unmodified ROM mazes and actual LEVDOOR
     * metadata, not a made-up "nearest" exit across solid walls. */
    {
        unsigned entries=0u,escapes=0u;
        for(unsigned level=6u;level<=13u;level++){
            const AvpDoorLevelMeta *m=&retail_meta[level-1u];
            for(size_t i=0;i<m->exit_count;i++){
                const unsigned target=m->exits[i].level;
                const unsigned x=(m->exits[i].panel_offset/8u)%levels[level-1u].map.width;
                const unsigned y=(m->exits[i].panel_offset/8u)/levels[level-1u].map.width;
                if(!(target<=5u || target>=14u) ||
                   !avp_mp_bot_playable_position(levels[level-1u].map.cells,
                          levels[level-1u].map.width,
                          levels[level-1u].map.height,x<<16,y<<16))continue;
                entries++;
                if(avp_mp_bot_plan_levels(levels,AVP_MP_MARINE,level,
                   x,y,0u,0u,0u,0,&plan) &&
                   plan.from_level==level && plan.goal_level &&
                   (plan.goal_level<=5u||plan.goal_level>=14u) &&
                   plan.hops>=1u)escapes++;
                else fprintf(stderr,"VENT ESCAPE MISSING: vent %u panel(%u,%u) to %u\n",
                              level,x,y,target);
            }
        }
        printf("R125 ROM VENT ESCAPE: %u/%u authored near-exit searches\n",
               escapes,entries);
        if(entries<8u || escapes!=entries)goto done;
    }
    /* R148: Sub-Level 4 is the authored Training Maze: long halls, many
     * detours and dead ends. Walk each sampled REAL retail route one cell
     * at a time. A shortest source-connected route MUST lower its remaining
     * cost and must never revisit an earlier cell in an unchanged maze.
     * This tests the actual ROM, not a synthetic open 5x5 smoke map. */
    {
        const AvpMpBotMap *m=&levels[3].map;
        struct Cell {unsigned x,y;} samples[128];
        unsigned count=0u,examined=0u,long_routes=0u,max_steps=0u;
        for(unsigned y=1u;y<m->height && count<128u;y+=5u)
        for(unsigned x=1u;x<m->width && count<128u;x+=5u)
            if(avp_mp_bot_playable_position(m->cells,m->width,m->height,
                                          x<<16,y<<16))
                samples[count++]=(struct Cell){x,y};
        if(count<8u)goto done;
        for(unsigned n=0u;n<count && examined<6u;n++){
            const unsigned i=(n*7u)%count,j=(count-1u-(n*11u)%count);
            unsigned cx=samples[i].x,cy=samples[i].y;
            const unsigned gx=samples[j].x,gy=samples[j].y;
            uint8_t seen[AVP_MP_BOT_MAP_CELLS]={0};
            AvpMpBotRoute first={0};
            unsigned steps=0u,last_cost=UINT32_MAX;
            if((cx==gx && cy==gy) ||
               !avp_mp_bot_route(m,cx,cy,gx,gy,&first) ||
               first.cost<120u)continue;
            long_routes++;
            while(cx!=gx || cy!=gy){
                AvpMpBotRoute step={0};
                const unsigned at=cy*64u+cx;
                if(at>=AVP_MP_BOT_MAP_CELLS || seen[at] ||
                   steps>=AVP_MP_BOT_MAP_CELLS ||
                   !avp_mp_bot_route(m,cx,cy,gx,gy,&step) ||
                   !step.valid ||step.cost>=last_cost ||
                   (step.next_x==cx && step.next_y==cy)){
                    fprintf(stderr,"TRAINING MAZE loop/stale route: (%u,%u) to (%u,%u) at step %u\n",
                            cx,cy,gx,gy,steps);
                    goto done;
                }
                seen[at]=1u;last_cost=step.cost;
                cx=step.next_x;cy=step.next_y;
                steps++;
            }
            if(steps>max_steps)max_steps=steps;
            examined++;
        }
        printf("TRAINING MAZE retail route audit: %u routes, %u long candidates, longest %u steps\n",
               examined,long_routes,max_steps);
        if(examined<2u || max_steps<10u)goto done;
    }
    rc=0;
done:
    if(f)fclose(f);
    for(unsigned i=0;i<AVP_MAX_LEVEL;i++)free(maps[i]);
    free(rom.data);
    return rc;
}
