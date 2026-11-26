/* Actual host resupply selection, original collectables read-only table,
 * real route planner; synthetic floor and contact observations. */
#include "../local_mp_walktest.c"
#define main retail_graph_main_unused
#include "test_local_mp_bot_retail_graph.c"
#undef main
#include <assert.h>
static void item(AvpAmp *a,unsigned x,unsigned y,int def){
    memset(a,0,sizeof *a);a->host_static=1;a->astype=def;
    a->xpos=(x<<16)|0x8000;a->ypos=(y<<16)|0x8000;
}
int main(void){
    AvpMpWalkHost *t=calloc(1,sizeof *t);AvpMpNativeWorld *w=calloc(1,sizeof *w);
    AvpMpActorNativeState *a=calloc(2,sizeof *a);assert(t&&w&&a);
    uint8_t bytes[8+8*24*24]={0},flags[256]={0};
    AvpMpRetailMap map={bytes,sizeof bytes,24,24,1,1};
    for(unsigned i=0;i<24*24;i++)bytes[8+i*8+5]=1;
    unsigned cases=0;
    for(unsigned dm=0;dm<2;dm++)for(unsigned mode=0;mode<5;mode++){
        memset(t,0,sizeof *t);memset(w,0,sizeof *w);memset(a,0,2*sizeof *a);
        avp_local_mp_defaults(&t->session.setup);t->actors=t->players=2;t->dm.enabled=dm;
        t->session.running=1;t->session.worlds[0]=(AvpMpActiveWorld){0,1,3,w,0};
        t->session.player[0]=&a[0];t->session.player[1]=&a[1];w->map=&map;
        a[0].player.player_energy=a[1].player.player_energy=1000;
        a[0].weapon.ammo_info[0].max=100;a[0].weapon.ammo_info[1].max=100;
        AvpMpBotObservation v={0};v.map=(AvpMpBotMap){bytes+8,24,24,flags,NULL,0,NULL,NULL,0};
        v.x=v.y=(10u<<16)|0x8000;v.tick=10;v.player_energy=v.max_energy=1000;
        v.marine_inventory_valid=1;v.marine_owned=1u<<1;v.current_weapon=1;
        v.marine_unarmed=v.marine_low_ammo=1;v.has_target=v.hostile_target=v.target_visible=1;
        v.goal=AVP_MP_BOT_HOSTILE;v.target_x=v.x+2*65536;v.target_y=v.y;
        v.other_player_swarmed=1;v.cover_flank_valid=1;
        item(&w->amp.actors[0],9,10,16); /* wrong ammo, closer */
        item(&w->amp.actors[1],14,10,25); /* ammo behind threat */
        item(&w->amp.actors[2],6,10,25); /* usable, safe */
        if(mode!=0){
            unsigned i=dm?AVP_NUM_AMPS+1:3;
            if(!dm){item(&w->amp.actors[3],12,10,0);w->amp.actors[3].energy=30;
                w->amp.actors[3].flags=1u<<AMP_KILLABLE;w->amp.actors[3].creature=AC_ALIEN;}
            t->contacts[0][i].sight=1;t->contacts[0][i].x=v.target_x;t->contacts[0][i].y=v.target_y;
        }
        if(mode==2){ /* safe stock claimed by this bot */
            t->collected_identity[0][2]=walk_pickup_identity(&w->amp.actors[2]);
            t->collected_items[0][2]=AVP_MP_ACTOR_BIT(0);
        }
        if(mode==3){ /* safe stock already collected */
            w->amp.actors[2].flags=1u<<AMP_COLLECT;
        }
        if(mode==4){ /* only wrong ammo; must flee not engage */
            memset(&w->amp.actors[1],0,sizeof(AvpAmp));memset(&w->amp.actors[2],0,sizeof(AvpAmp));
        }
        walk_bot_empty_marine(t,0,&a[0],w,&v,NULL,NULL);
        assert(!v.other_player_swarmed && !v.cover_flank_valid);
        if(mode<=1){assert(v.goal==AVP_MP_BOT_SUPPLY && !v.hostile_target);
            assert(v.target_slot==(mode==0?1:2));}
        else {assert(v.goal==AVP_MP_BOT_PATROL && !v.hostile_target);assert(v.target_x<v.x);}
        cases++;
    }
    free(a);free(w);free(t);printf("PASS %u host supply/retreat cases using original pickup table, including co-op and DM\n",cases);return 0;
}
