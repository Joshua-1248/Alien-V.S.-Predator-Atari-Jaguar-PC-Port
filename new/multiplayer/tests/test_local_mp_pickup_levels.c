/* Original Collectables() executed against intentionally stale preceding world.
 * No fabricated loot logic: collectables.c is compiled into this executable. */
#include "collectables.h"
#include "amp.h"
#include "player.h"
#include "maze.h"
#include "levels.h"
#include "weapons.h"
#include "avp_runtime.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}} while(0)
AvpAmp level1amps[AVP_NUM_AMPS];
AvpAmp *amp_data=level1amps;
u8 objmap[AVP_AMP_OBJ_ROW_BYTES*AVP_AMP_OBJ_H];
s16 cur_level,player_type,player_energy,max_energy,cur_wepno,new_wepno;
u8 cur_weps,acs_level;
u32 x_pos,y_pos;
static enum AvpCollectNotice last_notice;
static unsigned last_sound;
static void notice(enum AvpCollectNotice n,unsigned detail){(void)detail;last_notice=n;}
static void sound_event(unsigned id){last_sound=id;}
const AvpRuntimeOps *avp_runtime_ops(void){static const AvpRuntimeOps r={0};return &r;}
static void prepare(unsigned level,unsigned x,unsigned y,s16 def)
{
    memset(level1amps,0,sizeof(level1amps));
    memset(objmap,0,sizeof(objmap));
    cur_level=(s16)level;
    x_pos=x<<16;y_pos=y<<16;
    level1amps[0].astype=def;
    level1amps[0].xpos=(s32)x_pos;
    level1amps[0].ypos=(s32)y_pos;
    level1amps[0].host_static=1;
    objmap[y*AVP_AMP_OBJ_ROW_BYTES+x*2+1]=1;
    last_notice=AVP_CNOTICE_NONE;last_sound=0;
}
static void reset_actor(void)
{
    player_type=PT_HUMAN;player_energy=100;max_energy=1000;
    acs_level=0;cur_weps=0;new_wepno=cur_wepno=0;show_mt=0;
    memset(ammo_info,0,sizeof(ammo_info));
    for(unsigned i=0;i<6;i++)ammo_info[i].max=200;
}
/* Run the co-op actor-scope collection hook against ORIGINAL Collectables.
 * Each player has an independent one-time claim per live AMP pickup. */
static unsigned coop_actor;
static uint8_t coop_owned[AVP_NUM_AMPS];
static int coop_claim(void *u,unsigned level,unsigned slot,s16 type,int commit)
{
    const uint8_t bit=(uint8_t)(1u<<coop_actor);
    (void)u;
    if(level!=3u || slot>=AVP_NUM_AMPS ||type<COLL_S1 || type>COLL_MEDAID)
        return 0;
    if(coop_owned[slot]&bit)return 0;
    if(commit)coop_owned[slot]|=bit;
    return 1;
}
static int test_per_player_coop_pickups(void)
{
    const unsigned med_off=0x21*AVP_AMP_OBJ_ROW_BYTES+0x25*2u;
    const unsigned ammo_off=5u*AVP_AMP_OBJ_ROW_BYTES+5u*2u;
    /* Corpse weapon: all four actors can collect without consuming it. */
    memset(coop_owned,0,sizeof(coop_owned));
    prepare(3,0x24,0x09,39);
    avp_collect_set_mp_item_claim(coop_claim,NULL);
    for(coop_actor=0;coop_actor<4;coop_actor++){
        reset_actor();Collectables();
        CHECK((cur_weps & (1u<<1)) && ammo_info[0].cur==15);
        CHECK(!(level1amps[0].flags&(1u<<AMP_COLLECT)));
        CHECK(level1amps[0].host_static && objmap[0x09*AVP_AMP_OBJ_ROW_BYTES+0x24*2+1u]==1u);
        ammo_info[0].cur=1;
        Collectables();CHECK(ammo_info[0].cur==1); /* no farming */
    }
    /* P2 dies: only P2's claim is cleared, others retain theirs. */
    coop_owned[0]&=(uint8_t)~(1u<<1);
    coop_actor=1;reset_actor();Collectables();
    CHECK((cur_weps & (1u<<1)) && ammo_info[0].cur==15);
    coop_actor=0;reset_actor();Collectables();CHECK(cur_weps==0);

    /* S107: P2 can collect Security Card #01 first, then P1 must still get
     * the SAME authored corpse reward. Test with an unrelated live Food AMP
     * shadowing the keycard corpse in objmap, as can happen when the world
     * updates the one-owner-per-cell object grid between player ticks.
     * The original corpse is still present and never marked collected.
     * The co-op card must win for each player who has not claimed it yet;
     * after claiming it, the ordinary food item remains available. */
    memset(coop_owned,0,sizeof(coop_owned));
    prepare(3,0x22,0x11,39);
    level1amps[1].astype=21; /* legitimate co-located Food pickup */
    level1amps[1].xpos=level1amps[0].xpos;
    level1amps[1].ypos=level1amps[0].ypos;
    level1amps[1].host_static=1;
    objmap[0x11*AVP_AMP_OBJ_ROW_BYTES+0x22*2+1u]=2;
    avp_collect_set_mp_item_claim(coop_claim,NULL);
    coop_actor=1;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==1 && last_notice==AVP_CNOTICE_CARD);
    CHECK((coop_owned[0]&(1u<<1))!=0 && (coop_owned[1]&(1u<<1))==0);
    coop_actor=2;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==1 && last_notice==AVP_CNOTICE_CARD);
    coop_actor=3;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==1 && last_notice==AVP_CNOTICE_CARD);
    /* P1 arrives LAST, after all the other players have taken this card. */
    coop_actor=0;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==1 && last_notice==AVP_CNOTICE_CARD);
    CHECK(coop_owned[0]==0x0f && coop_owned[1]==0);
    CHECK(!(level1amps[0].flags&(1u<<AMP_COLLECT)));
    CHECK(objmap[0x11*AVP_AMP_OBJ_ROW_BYTES+0x22*2+1u]==2);
    /* A collected keycard does not block the separate physical Food item. */
    player_energy=100;last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==1 && last_notice==AVP_CNOTICE_FOOD);
    CHECK(player_energy==200 && (coop_owned[1]&1u)!=0);
    /* S110: A pre-collected corpse can persist from a legacy world/save while
     * co-op players still need independent claims. P2 takes the real card;
     * after the corpse is flagged collected and FOOD owns objmap, P1 must
     * still receive Card 01 from that exact, authenticated body position. */
    avp_collect_set_mp_item_claim(NULL,NULL);
    memset(coop_owned,0,sizeof(coop_owned));
    prepare(3,0x22,0x11,39);
    level1amps[1].astype=21;
    level1amps[1].xpos=level1amps[0].xpos;
    level1amps[1].ypos=level1amps[0].ypos;
    level1amps[1].host_static=1;
    avp_collect_set_mp_item_claim(coop_claim,NULL);
    coop_actor=1;reset_actor();Collectables();
    CHECK(acs_level==1 && (coop_owned[0]&(1u<<1))!=0);
    level1amps[0].flags|=(1u<<AMP_COLLECT);
    level1amps[0].mode=NULL;level1amps[0].host_static=1;
    objmap[0x11*AVP_AMP_OBJ_ROW_BYTES+0x22*2+1u]=2;
    coop_actor=0;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==1 && last_notice==AVP_CNOTICE_CARD);
    CHECK((coop_owned[0]&0x03u)==0x03u && coop_owned[1]==0);
    CHECK((level1amps[0].flags&(1u<<AMP_COLLECT))!=0);
    CHECK(objmap[0x11*AVP_AMP_OBJ_ROW_BYTES+0x22*2+1u]==2);
    /* The unrelated food remains collectible on the next player step. */
    player_energy=100;last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==1 && last_notice==AVP_CNOTICE_FOOD);
    CHECK(player_energy==200 && (coop_owned[1]&1u));
    /* After clearing objmap, the same intact corpse is still eligible for
     * an unclaimed player; a recycled/unrelated item must not become Card 1. */
    objmap[0x11*AVP_AMP_OBJ_ROW_BYTES+0x22*2+1u]=0;
    coop_actor=2;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==1 && last_notice==AVP_CNOTICE_CARD);
    level1amps[0].astype=21; /* the original corpse was actually replaced */
    coop_actor=3;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==0 && last_notice==AVP_CNOTICE_NONE);
    /* Keep actual authored cells distinct. Food at (0x24,0x11) must not
     * remotely award the card at (0x22,0x11). */
    avp_collect_set_mp_item_claim(NULL,NULL);
    memset(coop_owned,0,sizeof(coop_owned));
    prepare(3,0x24,0x11,39);
    avp_collect_set_mp_item_claim(coop_claim,NULL);
    coop_actor=0;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==0 && last_notice==AVP_CNOTICE_FOOD);

    /* S115 retail first-visit regression: the verified Level 3 corpse is
     * AMP definition 38 at (34.414,17.238).  A multiplayer world can have
     * BOTH transient lifecycle bits clear before anyone's first pickup.
     * The unclaimed authored corpse must still win over an unrelated Food
     * objmap owner for P1/P2/P3/P4, WITHOUT needing to die first. */
    avp_collect_set_mp_item_claim(NULL,NULL);
    memset(coop_owned,0,sizeof(coop_owned));
    prepare(3,0x22,0x11,38);
    level1amps[0].xpos=(s32)((0x22u<<16)|(106u<<8));
    level1amps[0].ypos=(s32)((0x11u<<16)|(61u<<8));
    level1amps[0].mode=NULL;
    level1amps[0].host_static=0;
    level1amps[1].astype=21;
    level1amps[1].xpos=level1amps[0].xpos;
    level1amps[1].ypos=level1amps[0].ypos;
    level1amps[1].host_static=1;
    objmap[0x11*AVP_AMP_OBJ_ROW_BYTES+0x22*2u+1u]=2u;
    avp_collect_set_mp_item_claim(coop_claim,NULL);
    for(coop_actor=0;coop_actor<4u;coop_actor++){
        reset_actor();last_notice=AVP_CNOTICE_NONE;
        Collectables();
        CHECK(acs_level==1 && last_notice==AVP_CNOTICE_CARD);
        CHECK((coop_owned[0]&(1u<<coop_actor))!=0);
        CHECK((coop_owned[1]&(1u<<coop_actor))==0);
        CHECK(objmap[0x11*AVP_AMP_OBJ_ROW_BYTES+0x22*2u+1u]==2u);
    }
    CHECK(coop_owned[0]==0x0f && coop_owned[1]==0);
    /* After the card, real Food can still be claimed independently. */
    coop_actor=0;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();
    CHECK(acs_level==0 && last_notice==AVP_CNOTICE_FOOD);
    CHECK(coop_owned[1]==1u && coop_owned[0]==0x0f);
    /* When the corpse AMP is actually gone/retyped, do NOT fabricate a card. */
    coop_actor=1;reset_actor();last_notice=AVP_CNOTICE_NONE;
    level1amps[0].astype=-1;
    Collectables();
    CHECK(acs_level==0 && last_notice==AVP_CNOTICE_FOOD);
    CHECK((coop_owned[0]&(1u<<1))!=0);
    avp_collect_set_mp_item_claim(NULL,NULL);

    /* Repeat with no competing item: P2->P1 works on the same corpse. */
    avp_collect_set_mp_item_claim(NULL,NULL);
    memset(coop_owned,0,sizeof(coop_owned));
    prepare(3,0x22,0x11,39);
    avp_collect_set_mp_item_claim(coop_claim,NULL);
    coop_actor=1;reset_actor();Collectables();
    CHECK(acs_level==1 && (coop_owned[0]&(1u<<1))!=0);
    coop_actor=0;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();CHECK(acs_level==1 && last_notice==AVP_CNOTICE_CARD);
    avp_collect_set_mp_item_claim(NULL,NULL);

    memset(coop_owned,0,sizeof(coop_owned));
    avp_collect_set_mp_item_claim(coop_claim,NULL);
    /* Corpse medkit: full health does not claim. Each injured player can
     * take this same pack once per life. The world corpse stays intact. */
    memset(coop_owned,0,sizeof(coop_owned));
    prepare(3,0x25,0x21,40);
    coop_actor=0;reset_actor();player_energy=max_energy;
    Collectables();CHECK(player_energy==max_energy && coop_owned[0]==0);
    player_energy=600;Collectables();CHECK(player_energy==900 && coop_owned[0]==1);
    player_energy=200;Collectables();CHECK(player_energy==200);
    coop_actor=1;reset_actor();Collectables();CHECK(player_energy==400);
    CHECK(!(level1amps[0].flags&(1u<<AMP_COLLECT)) && objmap[med_off+1u]==1u);
    coop_actor=2;reset_actor();Collectables();CHECK(player_energy==400);
    coop_actor=3;reset_actor();Collectables();CHECK(player_energy==400);
    /* Predator medpack uses its own original medpak energy semantics. */
    coop_actor=1;coop_owned[0]&=(uint8_t)~(1u<<1);
    reset_actor();player_type=PT_PREDATOR;medpak=0;
    Collectables();CHECK(medpak==300 && ammo_info[5].cur==300);

    /* Ordinary world pickup: full ammo does not consume a player's claim,
     * and subsequent actors retain their opportunity to collect. */
    memset(coop_owned,0,sizeof(coop_owned));
    prepare(3,5,5,25);
    coop_actor=0;reset_actor();ammo_info[0].cur=ammo_info[0].max;
    Collectables();CHECK(!coop_owned[0] && objmap[ammo_off+1u]==1u);
    ammo_info[0].cur=180;Collectables();CHECK(coop_owned[0]==1 && ammo_info[0].cur==195);
    ammo_info[0].cur=0;Collectables();CHECK(ammo_info[0].cur==0);
    coop_actor=1;reset_actor();Collectables();CHECK(ammo_info[0].cur==15);
    CHECK(!(level1amps[0].flags&(1u<<AMP_COLLECT)) && level1amps[0].host_static);
    /* Single-player still consumes world pickups in original manner. */
    avp_collect_set_mp_item_claim(NULL,NULL);
    reset_actor();prepare(3,0x25,0x21,40);Collectables();
    CHECK((level1amps[0].flags&(1u<<AMP_COLLECT))!=0);
    return 0;
}
/* Mirror the multiplayer host's per-player/per-physical-AMP eligibility hook.
 * The actual host tracks this on every level and clears one player's bit on
 * respawn; this test ensures Collectables only commits after a pickup grants.
 */
static unsigned claim_actor;
static unsigned char claimed_items[AVP_NUM_AMPS];
static unsigned claim_calls,claim_commits;
static int test_item_claim(void *user,unsigned level,unsigned slot,s16 type,int commit)
{
    const unsigned char bit=(unsigned char)(1u<<claim_actor);
    (void)user;(void)level;(void)type;
    if(slot>=AVP_NUM_AMPS || claim_actor>=4u)return 0;
    if(claimed_items[slot]&bit)return 0;
    claim_calls++;
    if(commit){claimed_items[slot]|=bit;claim_commits++;}
    return 1;
}
static unsigned dm_body_calls;
static void dm_body_test(void *user,unsigned level,unsigned slot)
{
    (void)user;
    if(level!=3u || slot!=0u)return;
    ++dm_body_calls;
    (void)CollectIt(COLL_PULSERIFLE,60);
}
static int test_dm_body_collectables(void)
{
    /* This tests the actual original Collectables() call path, not a model.
     * Callback is Deathmatch-only and must never steal keycard corpses. */
    dm_body_calls=0u;
    reset_actor();prepare(3,0x24,0x09,39); /* original shotgun body */
    avp_collect_set_mp_body_loot(dm_body_test,NULL);
    Collectables();
    CHECK(dm_body_calls==1u);
    CHECK((cur_weps&(1u<<2)) && !(cur_weps&(1u<<1)));
    CHECK(ammo_info[1].cur==60);
    CHECK(!(level1amps[0].flags&(1u<<AMP_COLLECT)));
    CHECK(objmap[0x09*AVP_AMP_OBJ_ROW_BYTES+0x24*2u+1u]==1);
    reset_actor();prepare(3,0x22,0x11,39); /* authored Card 1 */
    Collectables();CHECK(dm_body_calls==1u);
    CHECK(acs_level==1u && last_notice==AVP_CNOTICE_CARD);
    reset_actor();prepare(3,0x30,0x30,18); /* non-authored corpse */
    Collectables();CHECK(dm_body_calls==2u);
    CHECK((cur_weps&(1u<<2)) && ammo_info[1].cur==60);
    avp_collect_set_mp_body_loot(NULL,NULL);
    reset_actor();prepare(3,0x24,0x09,39);
    Collectables();
    CHECK((cur_weps&(1u<<1)) && !(cur_weps&(1u<<2)));
    return 0;
}
int main(void)
{
    {s16 type,amount;
     CHECK(avp_collect_peek(3,0x22,0x19,39,&type,&amount));
     CHECK(type==COLL_SHOTAMMO && amount==15);
     CHECK(avp_collect_peek(5,0x22,0x0e,39,&type,&amount));
     CHECK(type==COLL_MTRACKER);
     CHECK(avp_collect_peek(3,0x22,0x11,18,&type,&amount));
     CHECK(type==COLL_S1);
     CHECK(avp_collect_peek(4,5,5,16,&type,&amount));
     CHECK(type==COLL_PULSEAMMO && amount==60);
     CHECK(!avp_collect_peek(3,5,5,18,&type,&amount));
    }
    reset_actor();avp_collect_set_callbacks(notice,sound_event);
    prepare(3,0x22,0x19,39);Collectables();
    CHECK(last_notice==AVP_CNOTICE_SHOT_AMMO);
    prepare(5,0x22,0x0e,39);Collectables();
    CHECK(show_mt!=0 && last_notice==AVP_CNOTICE_MTRACKER);
    CHECK(ammo_info[0].cur==15);
    CHECK(level1amps[0].flags&(1u<<AMP_COLLECT));
    const struct {unsigned level,x,y,card;} keys[]={
       {1,0x22,0x21,7},{2,0x1e,0x29,3},{3,0x22,0x11,1},
       {3,0x08,0x2a,2},{4,0x17,0x11,4},{4,0x0c,0x13,6},
       {5,0x19,0x27,5},{14,0x0a,0x03,8},{14,0x21,0x0b,10},
       {15,0x0b,0x15,9}};
    for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);i++){
        reset_actor();
        cur_level=(s16)(keys[i].level==3?14:3);ResetClct();
        prepare(keys[i].level,keys[i].x,keys[i].y,39);
        Collectables();
        CHECK(acs_level==keys[i].card);
        CHECK(last_notice==AVP_CNOTICE_CARD);
        CHECK(ammo_info[0].cur==0);
        CHECK(level1amps[0].flags&(1u<<AMP_COLLECT));
    }
    /* Co-op card availability is per-player and survives sharing the same
     * world/corpse. A card must never disappear for the next participant. */
    {
        u16 owned[4]={0,0,0,0};
        reset_actor();prepare(3,0x22,0x11,39); /* S1 authored corpse */
        avp_collect_mp_keycard_owner(&owned[0]);
        Collectables();
        CHECK(acs_level==1 && owned[0]==1u);
        CHECK(!(level1amps[0].flags&(1u<<AMP_COLLECT)));
        CHECK(objmap[0x11*AVP_AMP_OBJ_ROW_BYTES+0x22*2+1]==1);
        last_notice=AVP_CNOTICE_NONE;
        Collectables(); /* P1 cannot farm/re-trigger the card */
        CHECK(owned[0]==1u && last_notice==AVP_CNOTICE_NONE);
        reset_actor();
        avp_collect_mp_keycard_owner(&owned[1]);
        Collectables(); /* P2 same world, same body */
        CHECK(acs_level==1 && owned[1]==1u && last_notice==AVP_CNOTICE_CARD);
        reset_actor();avp_collect_mp_keycard_owner(&owned[2]);
        Collectables();CHECK(acs_level==1 && owned[2]==1u);
        reset_actor();avp_collect_mp_keycard_owner(&owned[3]);
        Collectables();CHECK(acs_level==1 && owned[3]==1u);
        /* P2 dies: its per-life record clears while P1's stays intact.
         * The body stays available for that player to recover the key. */
        owned[1]=0;reset_actor();
        avp_collect_mp_keycard_owner(&owned[1]);
        Collectables();CHECK(acs_level==1 && owned[1]==1u);
        avp_collect_mp_keycard_owner(NULL);
        CHECK(!(level1amps[0].flags&(1u<<AMP_COLLECT)));
        CHECK(objmap[0x11*AVP_AMP_OBJ_ROW_BYTES+0x22*2+1]==1);
        /* Non-card loot still follows original shared-world consumption. */
        reset_actor();prepare(3,0x22,0x19,39);
        avp_collect_mp_keycard_owner(&owned[0]);
        Collectables();
        avp_collect_mp_keycard_owner(NULL);
        CHECK(level1amps[0].flags&(1u<<AMP_COLLECT));
    }
    /* Four co-op Marines may collect the SAME physical pulse rifle body.
     * No per-world collected bit or objmap erasure occurs. */
    memset(claimed_items,0,sizeof(claimed_items));
    reset_actor();prepare(4,0x2c,0x12,39);
    avp_collect_set_mp_item_claim(test_item_claim,NULL);
    for(claim_actor=0;claim_actor<4;claim_actor++){
        reset_actor();Collectables();
        CHECK((cur_weps&(1u<<2))!=0 && ammo_info[1].cur==60);
        CHECK(!(level1amps[0].flags&(1u<<AMP_COLLECT)));
        CHECK(objmap[0x12*AVP_AMP_OBJ_ROW_BYTES+0x2c*2+1]==1);
        last_notice=AVP_CNOTICE_NONE;
        Collectables();
        CHECK(last_notice==AVP_CNOTICE_NONE);
    }
    CHECK(claimed_items[0]==0x0f);
    /* Reset only P2's death ownership; no other player can farm the rifle. */
    claimed_items[0]&=(unsigned char)~(1u<<1);
    claim_actor=1;reset_actor();Collectables();
    CHECK((cur_weps&(1u<<2))!=0 && claimed_items[0]==0x0f);
    claim_actor=0;reset_actor();last_notice=AVP_CNOTICE_NONE;
    Collectables();CHECK(last_notice==AVP_CNOTICE_NONE);
    /* Ordinary AMP ammunition is likewise not consumed for other actors. */
    memset(claimed_items,0,sizeof(claimed_items));
    prepare(4,5,5,16);
    for(claim_actor=0;claim_actor<4;claim_actor++){
        reset_actor();Collectables();
        CHECK(ammo_info[1].cur==60);
        CHECK(level1amps[0].mode==NULL && level1amps[0].host_static==1);
        CHECK(objmap[5*AVP_AMP_OBJ_ROW_BYTES+5*2+1]==1);
    }
    /* A player at its ammo cap has not claimed and can return later. */
    memset(claimed_items,0,sizeof(claimed_items));
    claim_actor=0;reset_actor();ammo_info[1].cur=ammo_info[1].max;
    Collectables();CHECK(claimed_items[0]==0);
    ammo_info[1].cur=70;Collectables();
    CHECK(ammo_info[1].cur==130 && claimed_items[0]==1);
    /* Health on a body remains collectible for every player. */
    memset(claimed_items,0,sizeof(claimed_items));
    prepare(3,0x25,0x21,40);
    for(claim_actor=0;claim_actor<4;claim_actor++){
        reset_actor();Collectables();
        CHECK(player_energy==400 && claimed_items[0]==(unsigned char)((1u<<(claim_actor+1u))-1u));
        CHECK(!(level1amps[0].flags&(1u<<AMP_COLLECT)));
    }
    /* Full health must not make a permanent pickup claim. */
    memset(claimed_items,0,sizeof(claimed_items));
    claim_actor=0;reset_actor();player_energy=max_energy;
    Collectables();CHECK(claimed_items[0]==0);
    player_energy=200;Collectables();
    CHECK(player_energy==500 && claimed_items[0]==1);
    avp_collect_set_mp_item_claim(NULL,NULL);
    reset_actor();cur_level=5;ResetClct();
    prepare(4,0x2c,0x12,39);Collectables();
    CHECK((cur_weps&(1u<<2))!=0 && ammo_info[1].cur==60);
    CHECK(last_notice==AVP_CNOTICE_PULSE);
    reset_actor();cur_level=4;ResetClct();
    prepare(3,0x25,0x21,40);Collectables();
    CHECK(player_energy==400 && last_notice==AVP_CNOTICE_MEDAID);
    /* White-coated authored corpse is 300 health with NO DM random
     * override registered. Food stays 100, static medaid stays 300. */
    {s16 pickup,amount;
     CHECK(avp_collect_peek(3,0x25,0x21,40,&pickup,&amount));
     CHECK(pickup==COLL_MEDAID && amount==300);
     CHECK(avp_collect_peek(3,0x04,0x23,39,&pickup,&amount));
     CHECK(pickup==COLL_FOOD && amount==100);}
    player_energy=200;objmap[0x21*AVP_AMP_OBJ_ROW_BYTES+0x25*2+1]=1;
    Collectables();CHECK(player_energy==200);
    CHECK(last_sound!=0);
    CHECK(test_per_player_coop_pickups()==0);
    CHECK(test_dm_body_collectables()==0);
    puts("PASS: level-switch loot and four-player independent keycards, weapons, ammo, health, respawn claims");
    return 0;
}
