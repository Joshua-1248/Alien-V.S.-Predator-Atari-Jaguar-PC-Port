#include <assert.h>
#include <stdio.h>
#include "../../../src/game/amp.c"
u32 x_pos,y_pos,fire_distance,ccn_xsave,ccn_ysave;
s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level;
u16 invisflag;
s32 score;
s16 player_type,player_energy,use_cocoon;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
u16 avp_random(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
/* Distinct barrel and generator records, plus exclusions. */
static const AvpAmpPlacement list[]={
 {0x180,0x180,0},{0x280,0x180,20},{0x380,0x180,20},
 {0x480,0x180,44},{0x580,0x180,45},{0x680,0x180,46},
 {0x780,0x180,47},{0x880,0x180,48},{0x980,0x180,22},
 {0xa80,0x180,25},{0xb80,0x180,49},{0xc80,0x180,13},{0xd80,0x180,15}, {-1,0,0}};
static void visit(s16 level){save_level();cur_level=level;restore_level();}
static void start(int enemy,int hazard){
 avp_amp_set_persistent_enemy_health(enemy);avp_amp_set_persistent_hazard_health(hazard);
 InitAMPs();cur_level=1;restore_level();
}
static void check(unsigned i,s16 energy){assert(amp_data[i].energy==energy&&amp_data[i].oldenergy==energy);}
int main(void){
 avp_amp_bind_templates(retail_source_templates,60);
 avp_amp_bind_level_list(3,1,list);avp_amp_bind_level_list(3,2,list);
 player_type=PT_ALIEN;
 for(int e=0;e<2;e++)for(int h=0;h<2;h++){
  start(e,h);
  assert(amp_data[0].astype==0 && amp_data[11].astype==13);
  for(unsigned i=0;i<13;i++)amp_data[i].energy=(s16)(i+1);
  visit(2);check(1,20);check(3,200);amp_data[1].energy=17;
  visit(1);check(0,e?1:30);check(1,h?2:20);check(2,h?3:20);
  for(unsigned i=3;i<=7;i++)check(i,h?(s16)(i+1):200);
  check(8,h?9:20);check(9,100);check(10,100);check(11,100);check(12,100);
  visit(2);check(1,h?17:20);
 }
 /* Save compaction after a barrel has actually been destroyed. */
 start(1,1);amp_data[1].energy=0;amp_drum_start_mode(&amp_data[1]);
 amp_data[2].energy=9;visit(2);visit(1);
 assert(amp_data[1].astype==20);check(1,9);assert(amp_data[2].astype==44);
 /* A restored live generator still initializes its shield and honors damage. */
 start(0,1);amp_data[3].energy=37;visit(2);visit(1);
 AvpAmp *g=&amp_data[3];g->mode(g);assert(g->mode==genmode&&g->energy==37&&ngens==1);
 g->energy=0;genmode(g);assert(g->flags&(1u<<AMP_DEADGEN));
 visit(2);visit(1);g=&amp_data[3];assert(g->flags&(1u<<AMP_DEADGEN));g->mode(g);assert(g->mode==dead_gen2&&ngens==0);
 /* Disabled arrival and disabled departure must not resurrect an old stored value. */
 start(1,1);amp_data[1].energy=6;visit(2);avp_amp_set_persistent_hazard_health(0);visit(1);check(1,20);
 amp_data[1].energy=4;visit(2);avp_amp_set_persistent_hazard_health(1);visit(1);check(1,20);
 start(1,1);check(1,20);check(3,200);
 puts("PASS: four independent toggle combinations, two maps, duplicate drums, generators, ammo and shield exclusions, destruction, toggle transitions and campaign reset");return 0;
}
