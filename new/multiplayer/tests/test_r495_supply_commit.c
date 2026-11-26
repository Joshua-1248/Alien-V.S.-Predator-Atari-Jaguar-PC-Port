/* Final-goal pickup watch is the only local commitment; no new timer. */
#include "../local_mp_walktest.c"
#include <assert.h>
static void item(AvpAmp *a,unsigned x,unsigned y){
    memset(a,0,sizeof *a);a->host_static=1;a->astype=25;
    a->xpos=(x<<16)|0x8000;a->ypos=(y<<16)|0x8000;
}
static int visible(void *ctx,u32 x,u32 y,u32 tx,u32 ty){
    (void)x;(void)y;(void)tx;(void)ty;return *(int *)ctx;
}
int main(void){
    AvpMpWalkHost *t=calloc(1,sizeof *t);AvpMpNativeWorld *w=calloc(1,sizeof *w);
    AvpMpActorNativeState *a=calloc(1,sizeof *a);assert(t&&w&&a);
    uint8_t bytes[8+8*24*24]={0},flags[256]={0};
    AvpMpRetailMap map={bytes,sizeof bytes,24,24,1,1};
    unsigned cases=0;
    for(unsigned mode=0;mode<15;mode++){
        memset(t,0,sizeof *t);memset(w,0,sizeof *w);memset(a,0,sizeof *a);
        for(unsigned i=0;i<24*24;i++)bytes[8+i*8+5]=1;
        t->actors=t->players=1;t->dm.enabled=mode!=1;w->map=&map;
        a->weapon.ammo_info[0].max=100;
        AvpMpBotObservation v={0};v.map=(AvpMpBotMap){bytes+8,24,24,flags,NULL,0,NULL,NULL,0};
        v.x=v.y=0xa8000;v.tick=100;v.marine_owned=2;v.marine_unarmed=1;
        item(&w->amp.actors[0],9,10);item(&w->amp.actors[1],12,10);
        w->amp.slot_generation[1]=7;
        int sight=1,corpse=mode>=9;
        if(corpse)assert(avp_mp_corpse_add(&t->corpses,1,0,0xc8000,v.y,0,42));
        AvpMpBotPickupMemory *m=&t->pickup_memory[0];
        const unsigned slot=corpse?WALK_BOT_CORPSE_SLOT_BASE:1u,gen=corpse?42u:7u;
        avp_mp_bot_pickup_track(m,1,slot,gen,80,v.x,v.y,0xc8000,v.y,20,650);
        unsigned expected=0;
        switch(mode){
        case 0:expected=1;break;
        case 1:break; /* co-op unchanged */
        case 2:m->active=0;break;
        case 3:w->amp.slot_generation[1]++;break;
        case 4:w->amp.actors[1].flags|=1u<<AMP_COLLECT;break;
        case 5:bytes[8+(10*24+12)*8+5]=0;break;
        case 6:avp_mp_bot_pickup_observe(m,1,120,v.x,v.y,20,650);v.tick=120;assert(!m->active);break;
        case 7:m->level=2;break;
        case 8: /* threat on the held route */
            item(&w->amp.actors[2],12,10);w->amp.actors[2].energy=30;
            w->amp.actors[2].flags=1u<<AMP_KILLABLE;w->amp.actors[2].creature=AC_ALIEN;
            t->contacts[0][2].sight=1;t->contacts[0][2].x=0xc8000;t->contacts[0][2].y=v.y;break;
        case 9:expected=slot;break;
        case 10:assert(avp_mp_corpse_claim_at(&t->corpses,1,0,0xc8000,v.y));break;
        case 11:sight=0;break;
        case 12:t->corpses.records[0].death_tick++;break;
        case 13:avp_mp_bot_pickup_observe(m,1,120,v.x,v.y,20,650);v.tick=120;assert(!m->active);break;
        case 14:m->target_x+=0x10000;break;
        }
        walk_bot_empty_marine(t,0,a,w,&v,visible,&sight);
        assert(v.goal==AVP_MP_BOT_SUPPLY && v.target_slot==expected);
        if(mode==0||mode==9){
            avp_mp_bot_pickup_track(m,1,slot,gen,v.tick,v.x,v.y,v.target_x,v.target_y,20,650);
            assert(m->last_progress_tick==80); /* do not renew on reselection */
            avp_mp_bot_pickup_observe(m,1,120,v.x,v.y,20,650);assert(!m->active);
            v.tick=120;walk_bot_empty_marine(t,0,a,w,&v,visible,&sight);
            assert(v.target_slot==0); /* stalled target now on cooldown */
        }
        cases++;
    }
    /* An earlier generic scan picked an unusable supply. It may not come
     * back during the cross-level search cooldown, even if its cell is
     * reachable. Co-op's card/mission fallback is unchanged. */
    for(unsigned dm=0;dm<2;dm++){
        memset(t,0,sizeof *t);memset(w,0,sizeof *w);memset(a,0,sizeof *a);
        for(unsigned i=0;i<24*24;i++)bytes[8+i*8+5]=1;
        t->actors=t->players=1;t->dm.enabled=dm;w->map=&map;
        a->weapon.ammo_info[0].max=100;
        AvpMpBotObservation v={0};v.map=(AvpMpBotMap){bytes+8,24,24,flags,NULL,0,NULL,NULL,0};
        v.x=v.y=0xa8000;v.tick=100;v.marine_owned=4;v.marine_unarmed=1;
        item(&w->amp.actors[0],9,10); /* shotgun shells without shotgun */
        v.has_target=1;v.goal=AVP_MP_BOT_SUPPLY;v.target_slot=0;
        v.target_x=0x98000;v.target_y=v.y;
        t->bots[0].resupply_retry_tick=140;
        walk_bot_empty_marine(t,0,a,w,&v,NULL,NULL);
        assert(v.has_target && v.goal==(dm?AVP_MP_BOT_PATROL:AVP_MP_BOT_SUPPLY));
        if(dm){AvpMpBotRoute r={0};assert(avp_mp_bot_route(&v.map,10,10,v.target_x>>16,v.target_y>>16,&r) && r.valid);}
        cases++;
    }
    free(a);free(w);free(t);printf("PASS %u local supply commitment cases, preserved watchdog age and fallback\n",cases);return 0;
}
