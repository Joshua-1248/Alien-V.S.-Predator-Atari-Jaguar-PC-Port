/* Production host planner with original pickup table, synthetic observations. */
#include "../local_mp_walktest.c"
#include <assert.h>
static void item(AvpAmp *a,unsigned x,unsigned y){
    memset(a,0,sizeof *a);a->host_static=1;a->astype=25;
    a->xpos=(x<<16)|0x8000;a->ypos=(y<<16)|0x8000;
}
static void scan_regression(void){
    struct WalkSupplyScan scan={0};
    struct WalkSupplyCandidate batch[WALK_SUPPLY_PROBES];
    unsigned cursor=0u,seen[AVP_MP_RETAIL_LEVELS][20]={0};
    assert(!walk_bot_supply_batch(&scan,5,&cursor,batch));
    /* A crowded nearby level cannot hide a more distant reachable item. */
    scan.count[3]=20;scan.count[5]=3;scan.count[12]=1;
    for(unsigned l=0;l<AVP_MP_RETAIL_LEVELS;l++)for(unsigned i=0;i<scan.count[l];i++)scan.slots[l][i]=(uint16_t)i;
    for(unsigned retry=0;retry<4;retry++){
        assert(walk_bot_supply_batch(&scan,5,&cursor,batch)==6u);
        if(!retry){assert(batch[0].level==4);assert(batch[1].level==6);assert(batch[2].level==13);}
        for(unsigned i=0;i<6;i++){assert(batch[i].level!=5);assert(!seen[batch[i].level-1][batch[i].slot]++);}
    }
    assert(cursor==0u);
    for(unsigned l=0;l<AVP_MP_RETAIL_LEVELS;l++)for(unsigned i=0;i<scan.count[l];i++)assert(seen[l][i]==1);
    memset(&scan,0,sizeof scan);scan.count[0]=2;scan.slots[0][0]=7;scan.slots[0][1]=9;
    cursor=7;assert(walk_bot_supply_batch(&scan,5,&cursor,batch)==2);
    assert(batch[0].slot==9 && batch[1].slot==7 && cursor==1);
    scan.count[4]=10;assert(walk_bot_supply_batch(&scan,5,&cursor,batch)==2);
    puts("PASS bounded cross-level resupply scan: diverse levels, exhaustive rotating stable set, circular wrap, current-level exclusion");
}
int main(void){
    scan_regression();
    AvpMpWalkHost *t=calloc(1,sizeof *t);AvpMpNativeWorld *w=calloc(1,sizeof *w);
    AvpMpActorNativeState *a=calloc(1,sizeof *a);assert(t&&w&&a);
    uint8_t bytes[8+8*24*24]={0},flags[256]={0};
    AvpMpRetailMap map={bytes,sizeof bytes,24,24,1,1};
    for(unsigned i=0;i<24*24;i++)bytes[8+i*8+5]=1;
    for(unsigned dm=0;dm<2;dm++)for(unsigned mode=0;mode<8;mode++){
        memset(t,0,sizeof *t);memset(w,0,sizeof *w);memset(a,0,sizeof *a);
        t->actors=t->players=1;t->dm.enabled=dm;t->dm.level_travel=1;
        t->session.worlds[0]=(AvpMpActiveWorld){0,1,1,w,0};t->session.player[0]=a;w->map=&map;
        a->weapon.ammo_info[0].max=100;
        AvpMpBotObservation v={0};v.map=(AvpMpBotMap){bytes+8,24,24,flags,NULL,0,NULL,NULL,0};
        v.x=v.y=0xa8000;v.tick=100;v.marine_owned=1u<<1;v.marine_unarmed=1;v.marine_low_ammo=1;
        item(&w->amp.actors[0],9,10);item(&w->amp.actors[1],14,10);
        w->amp.slot_generation[1]=7;
        AvpMpBotState *m=&t->bots[0];m->resupply_valid=1;m->resupply_level=1;m->resupply_last_level=2;
        m->resupply_x=14;m->resupply_y=10;m->resupply_slot=1;m->resupply_generation=7;m->resupply_until=110;m->resupply_retry_tick=1000;
        if(mode==1)w->amp.actors[1].flags|=1u<<AMP_COLLECT;
        if(mode==2)w->amp.slot_generation[1]++;
        if(mode==3)t->pickup_memory[0].avoided[0]=(AvpMpBotPickupAvoidEntry){1,1,7,200};
        if(mode==4){t->collected_identity[0][1]=walk_pickup_identity(&w->amp.actors[1]);t->collected_items[0][1]=1;}
        if(mode==5)w->amp.actors[1].host_static=0;
        if(mode==6)m->resupply_until=100;
        if(mode==7){item(&w->amp.actors[2],12,10);w->amp.actors[2].energy=30;w->amp.actors[2].flags=1u<<AMP_KILLABLE;w->amp.actors[2].creature=AC_ALIEN;t->contacts[0][2].sight=1;t->contacts[0][2].x=0xc8000;t->contacts[0][2].y=v.y;}
        if(mode==0){
            assert(walk_bot_choose_known_resupply(t,0,a,w,&v));
            assert(v.goal==AVP_MP_BOT_SUPPLY && v.target_slot==1 && m->resupply_until==260);
            v.tick=101;assert(walk_bot_resupply_held(t,0,a,w,&v));assert(m->resupply_until==260);
        }
        walk_bot_empty_marine(t,0,a,w,&v,NULL,NULL);
        assert(v.goal==AVP_MP_BOT_SUPPLY && v.target_slot==(mode==0?1u:0u));
        assert(m->resupply_valid==(mode==0));
    }
    free(a);free(w);free(t);puts("PASS 16 host arrival cases: retain exact supply, bounded renewal, collection, generation, cooldown, claim, inactive, timeout and danger fallback");return 0;
}
