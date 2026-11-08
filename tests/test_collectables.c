#include "collectables.h"
#include "weapons.h"
#include "player.h"
#include "amp.h"
#include "levels.h"
#include "maze.h"
#include "avp_sfx_ids.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
s16 player_type,player_energy,max_energy; u8 cur_weps,old_weps; s16 cur_wepno,new_wepno; u8 acs_level;
AvpAmp amps[AVP_NUM_AMPS],*amp_data=amps,*amps_at=amps; u8 objmap[AVP_AMP_OBJ_ROW_BYTES*AVP_AMP_OBJ_H]; u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H]; s16 cur_level=1; u32 x_pos,y_pos;
static int notices;
static enum AvpCollectNotice last_notice;
static void n(enum AvpCollectNotice x,unsigned d){(void)d;last_notice=x;++notices;}
static unsigned released_count,released_last;
void amp_release(unsigned n){++released_count;released_last=n;}
static unsigned last_sfx;
static void s(unsigned x){last_sfx=x;}
int main(void){
 avp_collect_set_callbacks(n,s); player_type=PT_HUMAN; player_energy=500; max_energy=1000; acs_level=0;
 assert(CollectIt(COLL_S3,0)==1 && acs_level==3 && last_sfx==AVP_FX_SECUR); assert(CollectIt(COLL_S2,0)==1 && acs_level==3);
 ammo_info[0].max=20; ammo_info[0].cur=0; cur_weps=0; cur_wepno=0; new_wepno=0;
 last_sfx=0;int notices_before=notices;
 assert(CollectIt(COLL_SHOTGUN,8)==1); assert(last_sfx==AVP_FX_WHATO); assert(cur_weps&(1u<<1)); assert(new_wepno==1); assert(ammo_info[0].cur==8);
 /* PLAYER.S give_wep -> give_ammo fall-through writes the same weapon
  * notice twice; another independent pickup writes its own notice last. */
 assert(notices==notices_before+2 && last_notice==AVP_CNOTICE_SHOTGUN);
 assert(CollectIt(COLL_SHOTAMMO,5)==1 && last_notice==AVP_CNOTICE_SHOT_AMMO);
 ammo_info[0].cur=20; assert(CollectIt(COLL_SHOTAMMO,5)==0);
 last_sfx=0; assert(CollectIt(COLL_MEDAID,600)==1); assert(last_sfx==AVP_FX_DIST1); assert(player_energy==1000); assert(CollectIt(COLL_MEDAID,1)==0);
 /* Picking up a settled medkit must clear its stationary-AMP display
    marker, so a later level save does not resurrect it. */
 memset(amps,0,sizeof(amps));memset(objmap,0,sizeof(objmap));
 cur_level=1;ResetClct();player_energy=500;max_energy=1000;
 x_pos=0x00100000u;y_pos=0x00100000u;
 amps[0].astype=11;amps[0].xpos=(s32)x_pos;amps[0].ypos=(s32)y_pos;
 amps[0].host_static=1;
 objmap[0x10u*AVP_AMP_OBJ_ROW_BYTES+0x10u*2u+1u]=1u;
 Collectables();assert(player_energy==800);
 assert(released_count==0); /* R272: PLAYER.S .killamp only clears amp_mode; no amp_release */
 assert(!amps[0].mode&&!amps[0].host_static);
 assert(objmap[0x10u*AVP_AMP_OBJ_ROW_BYTES+0x10u*2u+1u]==0);
 /* A stale duplicate object-grid entry cannot award the same consumed
  * medkit again after its AMP has become inactive. */
 player_energy=500;
 objmap[0x10u*AVP_AMP_OBJ_ROW_BYTES+0x10u*2u+1u]=1u;
 Collectables();assert(player_energy==500);
 assert(released_count==0); /* repeat stale entry must not release */
 assert(objmap[0x10u*AVP_AMP_OBJ_ROW_BYTES+0x10u*2u+1u]==0);
 /* If an AMP slot moved after writing its old object-grid cell, walking
    * over the old cell cannot collect the moved pickup. */
 memset(amps,0,sizeof(amps));memset(objmap,0,sizeof(objmap));
 cur_level=1;ResetClct();player_type=PT_HUMAN;
 player_energy=500;max_energy=1000;
 x_pos=0x00100000u;y_pos=0x00100000u;
 amps[0].astype=11;amps[0].host_static=1;
 amps[0].xpos=0x00110000;amps[0].ypos=(s32)y_pos;
 objmap[0x10u*AVP_AMP_OBJ_ROW_BYTES+0x10u*2u+1u]=1u;
 Collectables();assert(player_energy==500);
 assert(amps[0].host_static==1);
 /* A correct entry at the actor's actual square remains collectable. */
 x_pos=0x00110000u;
 objmap[0x10u*AVP_AMP_OBJ_ROW_BYTES+0x11u*2u+1u]=1u;
 Collectables();assert(player_energy==800);
 assert(released_count==0);
 assert(amps[0].host_static==0);
 /* A corpse with a weapon must be usable even if the object-grid cell is
    empty: its authored level entry is the source of its loot. */
 memset(amps,0,sizeof(amps)); memset(objmap,0,sizeof(objmap));
 cur_level=1; ResetClct(); player_type=PT_HUMAN;
 x_pos=0x00220000u; y_pos=0x000e0000u;
 amps[0].astype=18; amps[0].xpos=(s32)x_pos; amps[0].ypos=(s32)y_pos;
 ammo_info[1].max=100;ammo_info[1].cur=0;cur_weps=0;
 Collectables();
 assert(cur_weps&(1u<<2)); assert(ammo_info[1].cur==60);
 assert(amps[0].flags&(1u<<AMP_COLLECT));
 Collectables(); assert(ammo_info[1].cur==60); /* looted once only */
 assert(released_count==0); /* authored corpse remains allocated */
 /* Level 14 (Alien ship), access card 10, recovered from the authored body. */
 memset(amps,0,sizeof(amps)); memset(objmap,0,sizeof(objmap));
 cur_level=14;ResetClct();acs_level=0;
 x_pos=0x00210000u; y_pos=0x000b0000u;
 amps[1].astype=18; amps[1].xpos=(s32)x_pos;amps[1].ypos=(s32)y_pos;
 Collectables();assert(acs_level==10);
 assert(amps[1].flags&(1u<<AMP_COLLECT));
 Collectables();assert(acs_level==10);
 /* If a grid slot is occupied by a non-collectable AMP, a matching
    authored body can still be collected without modifying the other AMP. */
 memset(amps,0,sizeof(amps)); memset(objmap,0,sizeof(objmap));
 cur_level=2;ResetClct();cur_weps=0;
 x_pos=0x00290000u;y_pos=0x00140000u;
 amps[0].astype=2; amps[0].xpos=(s32)x_pos;amps[0].ypos=(s32)y_pos;
 amps[1].astype=38;amps[1].xpos=(s32)x_pos;amps[1].ypos=(s32)y_pos;
 objmap[(0x14u*AVP_AMP_OBJ_ROW_BYTES)+(0x29u*2u)+1u]=1u;
 ammo_info[1].cur=0;
 Collectables();assert(cur_weps&(1u<<2));assert(ammo_info[1].cur==60);
 assert(amps[1].flags&(1u<<AMP_COLLECT));
 assert(amps[0].flags==0);
 assert(objmap[0x14u*AVP_AMP_OBJ_ROW_BYTES+0x29u*2u+1u]==1u);
 /* Multiplayer switches scoped levels without InitPlayer's ResetClct().
  * Sub-Level 5's authored corpse at (0x22,0x0e) MUST grant the tracker,
  * even after the corpse table was last bound for Sub-Level 3.
  * Check every Marine slot with a fresh copy of the source corpse. */
 for(unsigned marine=0;marine<4u;marine++){
     memset(amps,0,sizeof(amps)); memset(objmap,0,sizeof(objmap));
     cur_level=3; ResetClct(); /* stale prior world */
     cur_level=5; player_type=PT_HUMAN; show_mt=0;
     x_pos=0x00220000u; y_pos=0x000e0000u;
     amps[0].astype=39; amps[0].xpos=(s32)x_pos; amps[0].ypos=(s32)y_pos;
     objmap[0x0eu*AVP_AMP_OBJ_ROW_BYTES+0x22u*2u+1u]=1u;
     ammo_info[0].max=100; ammo_info[0].cur=0;
     last_notice=AVP_CNOTICE_NONE;
     Collectables();
     if(show_mt!=-1 || last_notice!=AVP_CNOTICE_MTRACKER ||
        ammo_info[0].cur!=0 || !(amps[0].flags&(1u<<AMP_COLLECT))){
         fprintf(stderr,"Sub-Level 5 Marine P%u collected wrong item: tracker=%d notice=%d shells=%u\n",
                 marine+1,(int)show_mt,(int)last_notice,(unsigned)ammo_info[0].cur);
         return 1;
     }
     Collectables(); /* The same corpse must not award another item. */
     if(ammo_info[0].cur!=0)return 1;
 }
 player_type=PT_PREDATOR; medpak=0; assert(CollectIt(COLL_SHOTGUN,8)==0); last_sfx=0; assert(CollectIt(COLL_FOOD,200)==1); assert(last_sfx==AVP_FX_CHATC); assert(medpak==200 && ammo_info[5].cur==200);
 ammo_info[5].max=1000;
 assert(avp_give_ammo(6,100,AVP_CNOTICE_NONE,0)==1);
 assert(medpak==300 && ammo_info[5].cur==300);
 return 0;
}
