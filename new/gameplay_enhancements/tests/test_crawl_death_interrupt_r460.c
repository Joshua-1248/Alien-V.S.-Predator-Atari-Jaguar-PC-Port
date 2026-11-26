#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../src/game/amp.c"
u32 x_pos,y_pos,fire_distance,ccn_xsave,ccn_ysave,centre_angle;
s16 cur_level=1,num_cocoons,game_over,key_lock;
u8 acs_level,fullbits[256];
u16 invisflag,maze_width=8,maze_height=8;
s32 score;
s16 player_type,player_energy,use_cocoon;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
void HumanPain(void){}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
u16 avp_random(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static u8 maze[8*8*8];
static unsigned sounds;
static void sound(void *u,unsigned id){(void)u;(void)id;++sounds;}
static AvpAmp *setup(int near){
    InitAMPs();reset_free_list();memset(maze,0,sizeof maze);memset(fullbits,0,sizeof fullbits);
    avp_collision_bind_maze(maze,8,8);AvpRuntimeOps ops={0};ops.play_sfx=sound;avp_runtime_bind(&ops);
    player_type=PT_HUMAN;x_pos=0x38000+(near?0x4000:0x18000);y_pos=0x38000;player_energy=1000;sounds=0;
    AvpAmp *a=amp_req();a->xpos=a->ypos=0x38000;a->creature=AC_ACRAWL;a->astype=5;
    a->energy=a->oldenergy=30;a->flags=(1u<<AMP_KILLABLE)|(1u<<AMP_MOTION);a->mode=amp_crawlfight;a->animseq=AS_FIGHT1;a->ldir=0;a->timer=0;
    amp_setgrid(a);return a;
}
static void verify_death(AvpAmp *a,int near,int expected){
    assert(a->mode==amp_alien_dead && a->creature==AC_ALIEN && a->astype==52);
    assert(a->energy==0 && (a->flags&(1u<<AMP_DEAD)) && !(a->flags&(1u<<AMP_KILLABLE)));
    assert(a->animseq==AS_DEATH && a->animframe==2 && a->timer==1);
    assert(player_energy==expected-(near?50:0));assert(sounds==1);
    a->mode(a);assert(a->animframe==3);a->mode(a);assert(a->mode==amp_acid);
}
int main(void){
    const unsigned frames[]={0,1,2,3,3,2,1,0};
    assert(host_crawl_death_interrupt==0);
    for(int enabled=0;enabled<=1;enabled++)for(int near=0;near<=1;near++)for(unsigned kill=0;kill<=8;kill++){
        AvpAmp *a=setup(near);avp_amp_set_crawl_death_interrupt(enabled);
        for(unsigned f=0;f<=8;f++){
            if(f==kill)a->energy=-5;
            a->mode(a);
            if(enabled && f==kill){verify_death(a,near,1000-(near&&kill>4?100:0));break;}
            if(f<8){assert(a->mode==amp_crawlfight && a->animseq==AS_FIGHT1 && a->animframe==frames[f] && (unsigned)a->ldir==f+1);}
            else {assert(a->mode==chase_player && a->timer==10 && a->ldir==0);a->mode(a);verify_death(a,near,1000-(near?100:0));}
        }
    }
    for(int enabled=0;enabled<=1;enabled++){
        AvpAmp *a=setup(1);avp_amp_set_crawl_death_interrupt(enabled);
        for(unsigned f=0;f<8;f++){a->energy=17;a->mode(a);assert(a->mode==amp_crawlfight&&a->animframe==frames[f]);}
        a->mode(a);assert(a->mode==chase_player&&player_energy==900&&sounds==0);
    }
    /* Toggle while an attack is pending. */
    AvpAmp *a=setup(1);avp_amp_set_crawl_death_interrupt(0);a->energy=0;a->mode(a);assert(a->mode==amp_crawlfight);avp_amp_set_crawl_death_interrupt(1);a->mode(a);verify_death(a,1,1000);
    puts("PASS: 36 lethal-frame/range/toggle cases; nonlethal hits, live toggle, committed attack damage, death animation and acid handoff");return 0;
}
