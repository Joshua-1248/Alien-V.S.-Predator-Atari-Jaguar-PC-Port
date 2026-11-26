/* Production host healing planner; synthetic geometry and panel table. */
#include "../local_mp_walktest.c"
#include <assert.h>
#define MED_OFFSET ((10u*24u+12u)*8u+2u)
static int medical_enabled;
size_t avp_computer_medical_terminals(unsigned level,u16 *out,size_t cap){
    if(!medical_enabled || level!=5u)return 0u;
    if(cap && out)out[0]=MED_OFFSET;
    return 1u;
}
int avp_computer_is_medical_terminal(unsigned level,u16 offset){return medical_enabled && level==5u && offset==MED_OFFSET;}
int avp_computer_first_aid_ceiling(unsigned access){return access>=4u?500:0;}
static void health(AvpMpNativeWorld *w,unsigned slot,unsigned x,unsigned y){
    AvpAmp *a=&w->amp.actors[slot];memset(a,0,sizeof *a);a->host_static=1;a->astype=11;
    a->xpos=(x<<16)|0x8000u;a->ypos=(y<<16)|0x8000u;w->amp.slot_generation[slot]=7;
}
int main(void){
    AvpMpWalkHost *t=calloc(1,sizeof *t);AvpMpNativeWorld *w=calloc(3,sizeof *w);
    AvpMpActorNativeState *a=calloc(1,sizeof *a);assert(t&&w&&a);
    uint8_t bytes[8+8*24*24]={0},flags[256]={0};
    AvpMpRetailMap maps[3]={{bytes,sizeof bytes,24,24,5,1},{bytes,sizeof bytes,24,24,4,1},{bytes,sizeof bytes,24,24,13,1}};
    for(unsigned i=0;i<24*24;i++)bytes[8+i*8+5]=1;
    AvpDoorLevelMeta metas[AVP_MAX_LEVEL]={0};AvpDoorExitEntry exit={MED_OFFSET,0,0,13};
    metas[4].exits=&exit;metas[4].exit_count=1;
    for(unsigned mode=0;mode<12;mode++){
        memset(t,0,sizeof *t);memset(w,0,3*sizeof *w);memset(a,0,sizeof *a);
        t->actors=t->players=1;t->session.player[0]=a;
        for(unsigned i=0;i<3;i++){w[i].map=&maps[i];t->session.worlds[i]=(AvpMpActiveWorld){0,maps[i].level,1,&w[i],0};}
        w[0].doors.level_meta=metas;w[0].doors.level_meta_count=AVP_MAX_LEVEL;w[0].doors.current_meta=&metas[4];
        memset(&w[0].doors.panels,0xff,sizeof w[0].doors.panels);
        a->access_level=4;medical_enabled=0;bytes[8+MED_OFFSET]=0;
        AvpMpBotObservation v={0};v.map=(AvpMpBotMap){bytes+8,24,24,flags,NULL,0,NULL,NULL,0};
        v.x=v.y=0xa8000;v.tick=100;v.player_species=AVP_MP_MARINE;v.player_energy=200;v.max_energy=1000;
        v.hostile_target=v.target_visible=v.tracker_only_target=1;
        AvpMpBotState *m=&t->bots[0];
        if(mode==0){health(&w[1],0,8,10);health(&w[2],0,9,10);
            assert(walk_bot_choose_known_healing(t,0,a,&w[0],&v));
            assert(v.goal==AVP_MP_BOT_EXIT && v.destination_level==13 && m->healing_level==13);continue;}
        if(mode==1){for(unsigned i=0;i<9;i++)health(&w[1],i,8,10);
            assert(!walk_bot_choose_known_healing(t,0,a,&w[0],&v));
            assert(m->healing_scan_cursor==6 && m->healing_retry_tick==135);
            v.tick=134;assert(!walk_bot_choose_known_healing(t,0,a,&w[0],&v));assert(m->healing_scan_cursor==6);
            v.tick=135;assert(!walk_bot_choose_known_healing(t,0,a,&w[0],&v));assert(m->healing_scan_cursor==3);continue;}
        health(&w[0],0,9,10);health(&w[0],1,14,10);
        m->healing_valid=1;m->healing_level=5;m->healing_last_level=4;m->healing_slot=1;
        m->healing_x=14;m->healing_y=10;m->healing_generation=7;m->healing_until=110;
        m->healing_retry_tick=1000;m->healing_scan_level=5;
        if(mode==3)t->pickup_memory[0].avoided[0]=(AvpMpBotPickupAvoidEntry){5,1,7,200};
        if(mode==4)w[0].amp.slot_generation[1]++;
        if(mode==5)w[0].amp.actors[1].xpos=0xf8000;
        if(mode==6)m->healing_slot=AVP_NUM_AMPS+10;
        if(mode==7)w[0].amp.actors[1].flags=1u<<AMP_COLLECT;
        if(mode>=8){m->healing_medical=1;m->healing_slot=MED_OFFSET;m->healing_x=12;m->healing_y=10;m->healing_generation=0;
            medical_enabled=1;bytes[8+MED_OFFSET]=1;
            if(mode==9)bytes[8+MED_OFFSET]=0;
            if(mode==10)a->access_level=3;
            if(mode==11)v.player_species=AVP_MP_PREDATOR;}
        assert(walk_bot_choose_known_healing(t,0,a,&w[0],&v));
        assert(!v.hostile_target && !v.target_visible && !v.tracker_only_target);
        if(mode==2){assert(v.goal==AVP_MP_BOT_HEALTH && v.target_slot==1 && m->healing_until==250);
            v.tick=101;assert(walk_bot_choose_known_healing(t,0,a,&w[0],&v));assert(m->healing_until==250);}
        else if(mode==8){assert(v.goal==AVP_MP_BOT_MEDICAL && v.target_side==2 && v.target_slot==MED_OFFSET);}
        else assert(v.goal==AVP_MP_BOT_HEALTH && v.target_slot==0);
    }
    free(a);free(w);free(t);puts("PASS 12 healing cases: remote fallback, bounded retry, arrival, stale stock and medical panel revalidation");return 0;
}
