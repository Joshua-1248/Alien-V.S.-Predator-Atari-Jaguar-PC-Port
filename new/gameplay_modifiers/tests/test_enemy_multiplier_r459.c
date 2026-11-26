#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/game/amp.c"
u32 x_pos,y_pos,fire_distance,ccn_xsave,ccn_ysave,centre_angle;
s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level,fullbits[256];
u16 invisflag,maze_width=24,maze_height=24;
s32 score;
s16 player_type,player_energy,use_cocoon;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
static u8 maze[24*24*8];
static unsigned rng_calls;
void place_grid(void){for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)collmap[y*64+x]=(x<24&&y<24&&maze[(y*24+x)*8+5])?0:0xffff;}
void HumanPain(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
u16 avp_random(void){return (u16)++rng_calls;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static const AvpAmpPlacement list[]={ {0x380,0x380,0},{0x480,0x380,1},{0x580,0x380,2},{0x680,0x380,3},{0x780,0x380,4},{0x880,0x380,5},{0x380,0x480,29},{0x480,0x480,31},{0x580,0x480,35},{0x680,0x480,20},{0x780,0x480,25},{0x880,0x480,44},{0x980,0x480,39},{-1,0,0} };
static unsigned count_def(int def){unsigned n=0;for(unsigned i=0;i<AVP_NUM_AMPS;i++)if((amp_data[i].mode||amp_data[i].host_static)&&amp_data[i].astype==def)n++;return n;}
static unsigned count_enemies(void){unsigned n=0;for(unsigned i=0;i<AVP_NUM_AMPS;i++)n+=(unsigned)modifier_enemy(&amp_data[i]);return n;}
static void start(unsigned mult){avp_amp_set_enemy_number_multiplier(mult);InitAMPs();cur_level=1;rng_calls=0;restore_level();}
static void visit(s16 l){save_level();cur_level=l;restore_level();}
int main(void){
    for(unsigned y=0;y<24;y++)for(unsigned x=0;x<24;x++){
        u8 *c=maze+(y*24+x)*8; c[5]=1;
        if(x==11)c[2]=1;
        if(x==12)c[0]=1;
        if(x==0)c[0]=1;
        if(y==0)c[1]=1;
        if(x==23)c[2]=1;
        if(y==23)c[3]=1;
    }
    maze[(5*24+5)*8+5]=0;
    avp_collision_bind_maze(maze,24,24);avp_amp_bind_templates(retail_source_templates,60);
    avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,list);avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,2,list);
    player_type=PT_HUMAN;x_pos=y_pos=0x18000;
    unsigned rng0=0;
    for(unsigned m=1;m<=4;m++){
        start(m);if(m==1)rng0=rng_calls;assert(rng_calls==rng0);
        assert(count_enemies()==9*m);assert(host_amp_capacity==(m==1?AVP_ORIGINAL_NUM_AMPS:AVP_NUM_AMPS));
        for(unsigned j=0;j<9;j++)assert(count_def(list[j].def)==m);
        for(unsigned j=9;j<13;j++)assert(count_def(list[j].def)==1);
        unsigned char occupied[4096]={0};
        for(unsigned i=0;i<AVP_NUM_AMPS;i++)if(amp_data[i].mode||amp_data[i].host_static){
            AvpAmp *a=&amp_data[i];unsigned x=(u32)a->xpos>>16,y=(u32)a->ypos>>16;assert(x<12&&y<24&&maze[(y*24+x)*8+5]);assert(!occupied[y*64+x]);occupied[y*64+x]=1;
            if(i>=13){assert(modifier_enemy(a));assert(a->energy==retail_source_templates[a->astype].energy);assert(x!=1||y!=1);assert(collmap[y*64+x]==i+1);}
        }
        printf("PASS %ux: %u enemies, 4 original props, valid unique reachable positions, unchanged RNG\n",m,count_enemies());
    }
    avp_amp_set_persistent_enemy_health(1);amp_data[13].energy=7;unsigned n=count_enemies();visit(2);assert(count_enemies()==36);avp_amp_set_enemy_number_multiplier(2);visit(1);assert(count_enemies()==n&&amp_data[13].energy==7);visit(2);assert(count_enemies()==36);
    start(2);assert(count_enemies()==18);start(0);assert(count_enemies()==9);start(5);assert(count_enemies()==9);
    /* Force the reserve boundary without an unrealistic full level. */
    start(1);free_count=66;avp_amp_set_enemy_number_multiplier(4);modifier_initial_population();assert(free_count==64&&count_enemies()==11);
    start(1);assert(host_amp_capacity==300);for(unsigned i=13;i<300;i++)assert(amp_req()==&amp_data[i]);assert(free_count==0&&amp_req()==&amp_data[299]);
    /* A multiplied saved level with >300 survivors restores even if the
     * preference has since been turned off. */
    start(4);for(unsigned i=49;i<340;i++){AvpAmp *a=amp_req();AvpAmpPlacement p={0x380,0x380,0};apply_template(a,&p);}
    assert(count_enemies()>300);unsigned before=count_enemies();visit(2);avp_amp_set_enemy_number_multiplier(1);visit(1);assert(host_amp_capacity==AVP_NUM_AMPS&&count_enemies()==before);
    puts("PASS: no revisit multiplication, health persistence, new campaign, invalid config, actor-pool guard");return 0;
}
