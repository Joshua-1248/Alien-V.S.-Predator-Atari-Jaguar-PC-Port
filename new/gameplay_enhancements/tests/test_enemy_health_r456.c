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
static const AvpAmpPlacement list[]={ {0x180,0x180,0},{0x280,0x180,0}, {0x380,0x180,1}, {0x480,0x180,2}, {0x580,0x180,3}, {0x680,0x180,4},{0x780,0x180,5},{0x880,0x180,25}, {-1,0,0} };
static void visit(s16 level){save_level();cur_level=level;restore_level();}
int main(void){
    avp_amp_bind_templates(retail_source_templates,60);
    avp_amp_bind_level_list(0,1,list);avp_amp_bind_level_list(0,2,list);
    /* common list is index 3 in the retail path. */
    avp_amp_bind_level_list(3,1,list);avp_amp_bind_level_list(3,2,list);
    player_type=PT_HUMAN;InitAMPs();cur_level=1;restore_level();
    assert(amp_data[0].astype==0 && amp_data[7].astype==25);
    amp_data[0].energy=7;visit(2);visit(1);assert(amp_data[0].energy==30);
    avp_amp_set_persistent_enemy_health(1);
    for(unsigned i=0;i<7;i++)amp_data[i].energy=(s16)(i+1);
    amp_data[7].energy=2;visit(2);assert(amp_data[0].energy==30);
    amp_data[0].energy=17;visit(1);
    for(unsigned i=0;i<7;i++)assert(amp_data[i].energy==(s16)(i+1)&&amp_data[i].oldenergy==(s16)(i+1));
    assert(amp_data[7].energy==100);visit(2);assert(amp_data[0].energy==17);
    /* Save skips a freed slot: survivors retain the correct distinct health. */
    amp_data[0].mode=NULL;amp_data[0].host_static=0;
    amp_data[1].energy=9;visit(1);visit(2);assert(amp_data[0].astype==0&&amp_data[0].energy==9);
    /* Queen first and second lives, including genuine regen and wait. */
    visit(14);AvpAmp *q=&amp_data[0];assert(q->astype==50 && q->energy==3000);
    lockxxx(q);q->energy=1234;visit(1);visit(14);q=&amp_data[0];assert(q->energy==1234&&q->mode==queen_lock_mode);
    q->energy=0;q->astype=57;q->mode=queen_wait_card_mode;q->animframe=2;queen_respawns=1;
    visit(1);visit(14);q=&amp_data[0];assert(q->mode==queen_wait_card_mode&&!q->host_static);
    acs_level=10;q->mode(q);assert(q->astype==59 && q->mode==queen_regen_mode);
    visit(1);visit(14);q=&amp_data[0];assert(q->mode==queen_regen_mode);
    for(unsigned i=0;i<10&&q->mode==queen_regen_mode;i++)q->mode(q);
    assert(q->energy==1000&&q->mode==queen_lock_mode);
    q->energy=83;visit(1);visit(14);q=&amp_data[0];assert(q->energy==83&&q->mode==queen_lock_mode);
    avp_amp_set_persistent_enemy_health(0);visit(1);assert(amp_data[0].energy==30);
    avp_amp_set_persistent_enemy_health(1);InitAMPs();cur_level=1;restore_level();assert(amp_data[0].energy==30);
    puts("PASS: retail AMP placement round trips, duplicate enemies, slot compaction, pickup exclusion, Queen both lives and scripted resurrection, off and campaign reset");return 0;
}
