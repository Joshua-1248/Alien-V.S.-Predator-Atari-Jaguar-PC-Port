/* Real retail door activation through DoorKeys, then the R499 host presence
 * filter and native occupancy wrapper. Requires the user's own ROM; no
 * resource bytes are written or redistributed. */
#define main r499_synthetic_main
#include "test_r499_door_occupancy.c"
#undef main
#include "hud_message.h"
#include "avp_resources.h"
#include "../../../new/jaguar/retail_data/door_retail_meta.c"
#define MSG(n) const AvpHudMessageStep avp_msg_##n[]={{0,{NULL,NULL,NULL}}}
MSG(pressure); MSG(notsecure); MSG(airlocked); MSG(jammed); MSG(jlift); MSG(escape); MSG(access_denied);
void avp_hudmsg_queue(const AvpHudMessageStep *m){(void)m;}
void avp_hudmsg_set_access_code(unsigned code){(void)code;}
static u8 original[8+64*64*8];
static AvpDoorPanelIds ids;
static unsigned close_case(AvpMpWalkHost *t,unsigned player,int present,int candidate){
    setup();memcpy(cells,original+8,sizeof cells);cur_level=5;player_type=PT_HUMAN;
    avp_doors_bind_level_meta(retail_meta,AVP_MAX_LEVEL);avp_doors_bind_panel_ids(&ids);acs_level=10;
    const unsigned off=(21u*64u+10u)*8u+6u;cells[off]=0;
    x_pos=0xa8000;y_pos=0x158000;centre_angle=0x80000000;new_pos=(21u*64u+10u)*8u;
    joy_cur=joy_edge=~(1u<<FIRE_B);DoorKeys();joy_cur=joy_edge=UINT32_MAX;
    for(unsigned i=0;i<40;i++)MoveDoors();assert(cells[off]>0);
    joy_cur=joy_edge=~(1u<<FIRE_B);DoorKeys();joy_cur=joy_edge=UINT32_MAX;
    for(unsigned i=0;i<t->actors;i++){
        AvpMpActorNativeState *a=t->session.player[i];a->player.x_pos=0xa8000;a->player.y_pos=0x158000;a->player.player_energy=1000;
        t->session.setup.level[i]=5;t->vent_fade[i].phase=AVP_MP_LIFE_ALIVE;t->player_life[i].phase=AVP_MP_LIFE_ALIVE;
    }
    AvpMpActorNativeState *a=t->session.player[player];a->player.x_pos=0xa3613;a->player.y_pos=0x159d44;
    if(!present)t->vent_fade[player].phase=AVP_MP_LIFE_FADE_OUT;
    /* Deliberately keep the bound context remote. Occupant can be any actor. */
    x_pos=0xa8000;y_pos=0x158000;
    for(unsigned i=0;i<40;i++){
        if(candidate){AvpMpDoorOccupant p[AVP_LOCAL_MP_MAX_ACTORS];unsigned n=walk_door_occupants(t,5,p);avp_mp_doors_native_step_occupied(p,n,0);}
        else MoveDoors();
    }
    assert(x_pos==0xa8000 && y_pos==0x158000);return cells[off];
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    AvpRomImage rom={0};rom.size=4194304;rom.data=malloc(rom.size);rom.exact_retail_sha256=1;
    FILE *f=fopen(argv[1],"rb");if(!f||!rom.data||fread(rom.data,1,rom.size,f)!=rom.size||fgetc(f)!=EOF)return 2;fclose(f);
    u8 *decoded=NULL;size_t size=0;
    if(!avp_resources_decode_file(&rom,91,NULL,&decoded,&size)||size!=sizeof original)return 2;
    memcpy(original,decoded,size);free(decoded);if(!avp_resources_retail_panel_info(&rom,5,&ids,panel_list,NULL))return 2;free(rom.data);
    AvpMpWalkHost *t=calloc(1,sizeof *t);AvpMpActorNativeState *a=calloc(AVP_LOCAL_MP_MAX_ACTORS,sizeof *a);assert(t&&a);
    avp_local_mp_defaults(&t->session.setup);t->actors=AVP_LOCAL_MP_MAX_ACTORS;t->players=4;
    t->session.setup.players=4;t->session.setup.bot_count=t->actors-4;t->session.setup.campaign=0;t->session.running=1;
    t->session.worlds[0]=(AvpMpActiveWorld){0,5,AVP_MP_ACTOR_VALID_MASK,t,0};
    for(unsigned i=0;i<t->actors;i++)t->session.player[i]=a+i;
    unsigned baseline=0,candidate=0,absent=0;
    for(unsigned i=0;i<t->actors;i++){
        const unsigned b=close_case(t,i,1,0),c=close_case(t,i,1,1),d=close_case(t,i,0,1);
        baseline+=b>0;candidate+=c>0;absent+=d==0;
        printf("actor=%u legacy=%u occupied=%u fading=%u\n",i+1,b,c,d);
    }
    printf("Retail SL5 normal native DoorKeys/MoveDoors: occupied reversal legacy=%u/%u candidate=%u/%u; unoccupied closure=%u/%u\n",baseline,t->actors,candidate,t->actors,absent,t->actors);
    assert(baseline==0 && candidate==t->actors && absent==t->actors);free(a);free(t);return 0;
}
