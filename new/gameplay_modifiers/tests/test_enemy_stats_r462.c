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
static void check_spawn(unsigned def,unsigned type,int hp){
    AvpAmp a={0};AvpAmpPlacement p={0x380,0x380,(u16)def};
    for(unsigned units=1;units<=16;units++){
        avp_amp_set_enemy_stat(type,AVP_STAT_HEALTH,units);
        apply_template(&a,&p);
        assert(a.energy==avp_stat_scale(hp,units)&&a.oldenergy==a.energy);
        if(def==3)assert(a.ldir==a.energy);
        assert(a.xvel==retail_source_templates[def].xvel);
    }
    avp_amp_set_enemy_stat(type,AVP_STAT_HEALTH,4);
}
static const AvpAmpPlacement list[]={ {0x380,0x380,0},{0x480,0x380,1},{0x580,0x380,2},{0x680,0x380,3},{0x780,0x380,4},{0x880,0x380,5},{0x380,0x480,20},{0x480,0x480,15},{0x580,0x480,44},{-1,0,0} };
int main(void){
    for(unsigned i=0;i<24*24;i++)maze[i*8+5]=1;
    avp_collision_bind_maze(maze,24,24);avp_amp_bind_templates(retail_source_templates,60);
    avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,1,list);avp_amp_bind_level_list(AVP_AMP_LIST_COMMON,2,list);
    InitAMPs();player_type=PT_HUMAN;x_pos=y_pos=0x18000;restore_level();
    check_spawn(0,AVP_STAT_ALIEN,30);check_spawn(29,AVP_STAT_ALIEN,30);check_spawn(35,AVP_STAT_ALIEN,30);
    check_spawn(1,AVP_STAT_PREDATOR,300);check_spawn(51,AVP_STAT_PREDATOR,300);
    check_spawn(2,AVP_STAT_MARINE,50);check_spawn(3,AVP_STAT_EGG,20);check_spawn(4,AVP_STAT_FACEHUGGER,20);
    check_spawn(5,AVP_STAT_CRAWLER,30);check_spawn(50,AVP_STAT_QUEEN,3000);check_spawn(58,AVP_STAT_QUEEN,3000);check_spawn(59,AVP_STAT_QUEEN,3000);
    for(unsigned e=0;e<AVP_STAT_ENEMIES;e++)avp_amp_set_enemy_stat(e,AVP_STAT_HEALTH,16);
    for(unsigned def=15;def<=59;def++)if(def==15||def==20||def==22||def==39||(def>=44&&def<=49)||(def>=52&&def<=57)){
        AvpAmp a={0};AvpAmpPlacement p={0x380,0x380,(u16)def};apply_template(&a,&p);assert(a.energy==retail_source_templates[def].energy);
    }
    avp_amp_set_enemy_stat(AVP_STAT_ALIEN,AVP_STAT_HEALTH,8);avp_amp_set_persistent_enemy_health(1);
    InitAMPs();cur_level=1;restore_level();assert(amp_data[0].energy==60);amp_data[0].energy=17;
    save_level();cur_level=2;restore_level();assert(amp_data[0].energy==60);
    save_level();cur_level=1;restore_level();assert(amp_data[0].energy==17&&amp_data[0].oldenergy==17);
    avp_amp_set_persistent_enemy_health(0);save_level();cur_level=2;restore_level();assert(amp_data[0].energy==60);
    for(unsigned e=0;e<AVP_STAT_ENEMIES;e++){avp_amp_set_enemy_stat(e,AVP_STAT_HEALTH,4);avp_amp_set_enemy_stat(e,AVP_STAT_DAMAGE,4);avp_amp_set_enemy_stat(e,AVP_STAT_SPEED,4);}
    avp_amp_set_enemy_stat(AVP_STAT_CRAWLER,AVP_STAT_DAMAGE,8);avp_amp_set_enemy_stat(AVP_STAT_ALIEN,AVP_STAT_DAMAGE,2);
    AvpAmp *a=&amp_data[0];a->xpos=a->ypos=0x38000;x_pos=0x3c000;y_pos=0x38000;player_energy=1000;
    a->creature=AC_ACRAWL;a->ldir=4;amp_apply_fight_frame(a,fight_crawl,70,chase_player);assert(player_energy==800);
    a->creature=AC_ALIEN;a->ldir=3;amp_apply_fight_frame(a,fight_bite,65,amp_handalien);assert(player_energy==750);
    avp_amp_set_enemy_stat(AVP_STAT_PREDATOR,AVP_STAT_DAMAGE,12);avp_amp_set_enemy_stat(AVP_STAT_MARINE,AVP_STAT_DAMAGE,6);
    AvpAmp shot={0};shot.creature=AC_SMART;assert(enemy_projectile_damage(&shot,50)==150);shot.creature=AC_BOLT;assert(enemy_projectile_damage(&shot,200)==600);
    shot.creature=AC_FIRE;assert(enemy_projectile_damage(&shot,25)==38);shot.flags=1u<<AMP_PLAYER;assert(enemy_projectile_damage(&shot,25)==25);
    /* Native damage event handlers rather than just the scale helper. */
    a->creature=AC_PREDATOR;a->ldir=8;player_energy=1000;
    amp_apply_fight_frame(a,fight_pred_combi,57,amp_handpred);assert(player_energy==700);
    a->creature=AC_HUMAN;a->energy=a->oldenergy=50;a->timer=0;player_energy=1000;
    while(crouch_frames[(unsigned)a->timer]>=0)++a->timer;
    amp_crouch_fire(a);assert(player_energy==850);
    a->creature=AC_AQUEEN;avp_amp_set_enemy_stat(AVP_STAT_QUEEN,AVP_STAT_DAMAGE,8);
    a->aux_ptr=(void*)(qseq_bite+3);a->animseq=AS_FIGHT1;player_energy=1000;
    queen_fight_mode(a);assert(player_energy==800);
    player_type=PT_PREDATOR;a->aux_ptr=(void*)(qseq_bite+3);player_energy=1000;
    queen_fight_mode(a);assert(player_energy==700);player_type=PT_HUMAN;
    /* Actual NPC weapon hit against a different actor; player projectiles unchanged. */
    AvpAmp *v=&amp_data[1];v->xpos=0x48000;v->ypos=0x38000;v->energy=1000;amp_setgrid(v);
    shot.xpos=v->xpos;shot.ypos=v->ypos;shot.flags=0;shot.creature=AC_BOLT;shot.xvel=0;
    assert(weapon_hit(&shot)==v&&v->energy==700);
    shot.flags=1u<<AMP_PLAYER;assert(weapon_hit(&shot)==v&&v->energy==600);
    avp_amp_set_enemy_stat(AVP_STAT_FACEHUGGER,AVP_STAT_HEALTH,12);
    a->xpos=0x68000;a->ypos=0x38000;a->creature=AC_EGG;unsigned next=free_list[free_count-1];
    fire_hug_mode(a);assert(amp_data[next].creature==AC_FACEHUG&&amp_data[next].energy==60);
    avp_amp_set_enemy_stat(AVP_STAT_QUEEN,AVP_STAT_HEALTH,16);
    a->creature=AC_AQUEEN;a->animframe=0;a->flags=1u<<AMP_ALTFRAME;queen_regen_mode(a);assert(a->energy==4000&&a->oldenergy==4000);
    for(unsigned e=0;e<AVP_STAT_ENEMIES;e++)avp_amp_set_enemy_stat(e,AVP_STAT_SPEED,16);
    a->creature=AC_PREDATOR;a->xvel=a->yvel=retail_source_templates[1].xvel;
    assert(enemy_actor_speed(a,a->xvel)==a->xvel*4);
    avp_amp_set_enemy_stat(AVP_STAT_PREDATOR,AVP_STAT_SPEED,2);assert(enemy_actor_speed(a,a->xvel)==a->xvel/2);
    avp_amp_set_enemy_stat(AVP_STAT_PREDATOR,AVP_STAT_SPEED,99);assert(enemy_actor_speed(a,a->xvel)==a->xvel);
    avp_amp_set_enemy_stat(AVP_STAT_ALIEN,AVP_STAT_SPEED,8);
    InitAMPs();cur_level=1;restore_level();a=&amp_data[0];s32 ox=a->xpos; x_pos=0xa8000;y_pos=(u32)a->ypos; a->mode=chase_player;
    amp_move_toward(a,(s32)x_pos,(s32)y_pos);assert(a->xpos-ox==2*retail_source_templates[0].xvel);
    puts("PASS: 192 spawn cases, variants, hazard exclusion, exact persistent wounds, separate melee/projectile multipliers, player shot isolation, Queen resurrection, movement and invalid preference");return 0;
}
