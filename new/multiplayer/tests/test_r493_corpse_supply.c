/* Host planner with native corpse records, claims, and reward policy. */
#include "../local_mp_walktest.c"
#include <assert.h>
static int visible(void *ctx,u32 x,u32 y,u32 tx,u32 ty){
    (void)x;(void)y;(void)tx;(void)ty;return *(int *)ctx;
}
int main(void){
    AvpMpWalkHost *t=calloc(1,sizeof *t);
    AvpMpNativeWorld *w=calloc(1,sizeof *w);
    AvpMpActorNativeState *a=calloc(1,sizeof *a);assert(t&&w&&a);
    uint8_t bytes[8+8*24*24]={0},flags[256]={0};
    AvpMpRetailMap map={bytes,sizeof bytes,24,24,1,1};
    for(unsigned i=0;i<24*24;i++)bytes[8+i*8+5]=1;
    const u32 start=0xa8000,corpse_x=0xc8000;
    unsigned cases=0;
    for(unsigned mode=0;mode<13;mode++){
        memset(t,0,sizeof *t);memset(w,0,sizeof *w);memset(a,0,sizeof *a);
        t->actors=t->players=1;t->dm.enabled=mode!=1;w->map=&map;
        a->weapon.ammo_info[0].max=100;
        AvpMpBotObservation v={0};v.map=(AvpMpBotMap){bytes+8,24,24,flags,NULL,0,NULL,NULL,0};
        v.x=v.y=start;v.tick=100;v.marine_owned=1u<<1;v.marine_unarmed=mode!=2;
        int sight=mode!=3;uint64_t danger=UINT64_MAX,fixed=UINT64_MAX;u32 tx=0,ty=0;
        assert(avp_mp_corpse_add(&t->corpses,mode==4?2:1,mode==5?1:0,corpse_x,start,0,42));
        if(mode==6)assert(avp_mp_corpse_claim_at(&t->corpses,1,0,corpse_x,start));
        if(mode==7)t->pickup_memory[0].avoided[0]=(AvpMpBotPickupAvoidEntry){1,WALK_BOT_CORPSE_SLOT_BASE,42,200};
        if(mode==8){tx=corpse_x;ty=start;danger=walk_bot_distance(v.x,v.y,tx,ty);}
        if(mode==9)fixed=0;
        if(mode==10)v.facehugged=1;
        if(mode==11){v.marine_owned=30u;memset(a->weapon.ammo_info,0,sizeof a->weapon.ammo_info);}
        if(mode==12)t->corpses.records[0].looted_mask=AVP_MP_ACTOR_BIT(1);
        int got=walk_bot_visible_corpse_supply(t,0,a,w,&v,danger,tx,ty,fixed,visible,&sight);
        assert(got==(mode==0||mode==12));
        if(got){assert(v.goal==AVP_MP_BOT_SUPPLY && v.has_target && !v.hostile_target);
            assert(v.target_slot==WALK_BOT_CORPSE_SLOT_BASE && v.target_x==corpse_x && v.target_y==start);
            assert(avp_mp_corpse_claim_at(&t->corpses,1,0,v.target_x,v.target_y));
            assert(!walk_bot_visible_corpse_supply(t,0,a,w,&v,danger,tx,ty,fixed,visible,&sight));
            AvpMpDmLoot loot={0};
            assert(avp_mp_dm_corpse_loot((uint8_t)v.marine_owned,a->weapon.ammo_info,0,0,&loot));
            assert(loot.type==COLL_PULSERIFLE && loot.amount==60);
            assert(avp_mp_dm_corpse_loot((uint8_t)v.marine_owned,a->weapon.ammo_info,1,0,&loot));
            assert(loot.type==COLL_SHOTAMMO && loot.amount==15);
        }cases++;
    }
    memset(t,0,sizeof *t);memset(w,0,sizeof *w);memset(a,0,sizeof *a);
    t->actors=t->players=1;t->dm.enabled=1;w->map=&map;a->weapon.ammo_info[0].max=100;
    AvpMpBotObservation v={0};v.map=(AvpMpBotMap){bytes+8,24,24,flags,NULL,0,NULL,NULL,0};
    v.x=v.y=start;v.tick=100;v.marine_owned=2;v.marine_unarmed=1;int sight=1;
    assert(avp_mp_corpse_add(&t->corpses,1,0,corpse_x,start,0,42));
    walk_bot_empty_marine(t,0,a,w,&v,visible,&sight);
    assert(v.goal==AVP_MP_BOT_SUPPLY && v.target_slot==WALK_BOT_CORPSE_SLOT_BASE);
    free(a);free(w);free(t);printf("PASS %u corpse eligibility cases and empty Marine integration; real claim and loot policy\n",cases);return 0;
}
