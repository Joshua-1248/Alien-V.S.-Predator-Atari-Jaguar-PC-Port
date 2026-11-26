#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/game/amp.c"
u32 x_pos,y_pos,fire_distance,ccn_xsave,ccn_ysave,centre_angle;
s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level,fullbits[256];u16 invisflag,maze_width=24,maze_height=24;
s32 score;s16 player_type,player_energy,use_cocoon;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
static u8 maze[24*24*8];static unsigned rng_calls;
void place_grid(void){for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)collmap[y*64+x]=(x<24&&y<24&&maze[(y*24+x)*8+5])?0:0xffff;}
void HumanPain(void){}void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
u16 avp_random(void){return (u16)++rng_calls;}
s32 sin_d0(u16 a){(void)a;return 0;}s32 cos_d0(u16 a){(void)a;return 0x4000;}
static const AvpAmpPlacement list[]={{0x380,0x380,5},{0x480,0x480,4},{0x580,0x580,15},{-1,0,0}};
static unsigned count_hugs(void){unsigned n=0;for(unsigned i=0;i<AVP_NUM_AMPS;i++)if(amp_data[i].mode&&amp_data[i].astype==4)n++;return n;}
static void start(s16 level,s16 species,int enabled,unsigned m){player_type=species;avp_amp_set_vent_facehuggers(enabled);avp_amp_set_enemy_number_multiplier(m);InitAMPs();cur_level=level;rng_calls=0;restore_level();}
static void visit(s16 l){save_level();cur_level=l;restore_level();}
int main(void){
 for(unsigned y=0;y<24;y++)for(unsigned x=0;x<24;x++){u8*c=maze+(y*24+x)*8;c[5]=1;if(x==0)c[0]=1;if(y==0)c[1]=1;if(x==23)c[2]=1;if(y==23)c[3]=1;}
 maze[(9*24+9)*8+5]=0;
 avp_collision_bind_maze(maze,24,24);avp_amp_bind_templates(retail_source_templates,60);
 for(unsigned l=1;l<=15;l++)avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,l,list);
 x_pos=y_pos=0x18000;
 const s16 species[]={PT_HUMAN,PT_ALIEN,PT_PREDATOR};
 for(unsigned s=0;s<3;s++)for(s16 l=1;l<=15;l++)for(unsigned m=1;m<=4;m++){
  start(l,species[s],0,m);unsigned orig=count_hugs(),rng=rng_calls;
  start(l,species[s],1,m);unsigned expected=s==0&&l>=10&&l<=13?(l<=11?3u:5u):0u;
  assert(count_hugs()==orig+expected);assert(rng_calls==rng);
  if(expected){u8 used[4096]={0};for(unsigned i=0;i<AVP_NUM_AMPS;i++){AvpAmp*a=&amp_data[i];if(!a->mode&&!a->host_static)continue;unsigned x=(u32)a->xpos>>16,y=(u32)a->ypos>>16;assert(x<24&&y<24&&maze[(y*24+x)*8+5]);assert(!used[y*64+x]);used[y*64+x]=1;}}
 }
 avp_amp_set_enemy_stat(AVP_STAT_FACEHUGGER,AVP_STAT_HEALTH,8);avp_amp_set_persistent_enemy_health(1);
 start(12,PT_HUMAN,1,1);assert(count_hugs()==6);unsigned wounded=0;
 for(unsigned i=3;i<AVP_NUM_AMPS;i++)if(amp_data[i].mode&&amp_data[i].astype==4){assert(amp_data[i].energy==40);amp_data[i].energy=7;wounded++;}
 assert(wounded==5);visit(8);assert(count_hugs()==1);visit(12);assert(count_hugs()==6);
 wounded=0;for(unsigned i=0;i<AVP_NUM_AMPS;i++)if(amp_data[i].mode&&amp_data[i].astype==4&&amp_data[i].energy==7)wounded++;
 assert(wounded==5);avp_amp_set_vent_facehuggers(0);visit(13);assert(count_hugs()==1);visit(12);assert(count_hugs()==6);
 start(12,PT_HUMAN,0,1);assert(count_hugs()==1);
 puts("PASS: 180 level/species/multiplier pairs; exactly 3 on 4B/4C and 5 on 5B/5C, zero elsewhere, unchanged RNG, unique floor cells, health and revisit persistence, disabled/new campaign");return 0;
}
