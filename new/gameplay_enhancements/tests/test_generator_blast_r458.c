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
static const s32 q=0x38000;
static unsigned callback_count,sound_count;
static void noop(AvpAmp *a){(void)a;}
static void sound(void *u,unsigned id){(void)u;(void)id;++sound_count;}
static void area(void *u,s32 x,s32 y,u32 r,s16 damage,u16 flags){(void)u;assert(x==q&&y==q&&r==0x10000&&damage==50&&flags==0);++callback_count;AreaDamage(x,y,r,damage,flags);}
static AvpAmp *setup(int enabled,s32 dx,s32 dy,int wall,int invincible,int callback){
    InitAMPs();reset_free_list();memset(maze,0,sizeof maze);memset(fullbits,0,sizeof fullbits);fullbits[1]=AVP_WB_SOLID;
    if(wall){maze[(3*8+3)*8+2]=1;maze[(3*8+4)*8]=1;}
    avp_collision_bind_maze(maze,8,8);
    avp_amp_set_generator_blast_radius(enabled);avp_new_damage_gate_set(invincible);
    AvpRuntimeOps ops={0};ops.play_sfx=sound;if(callback)ops.area_damage=area;avp_runtime_bind(&ops);
    x_pos=q+dx;y_pos=q+dy;player_energy=500;sound_count=callback_count=0;
    AvpAmp *g=amp_req();g->xpos=g->ypos=q;g->energy=0;g->flags=1u<<AMP_KILLABLE;g->mode=genmode;
    ngens=1;collmap[3*AVP_AMP_GRID_W+3]=1;genmode(g);
    assert(ngens==0&&g->mode==dead_gen2&&g->animframe==0&&sound_count==1);
    return g;
}
static void finish(AvpAmp *g,int expected){
    for(int f=1;f<=5;f++){g->mode(g);assert(g->animframe==f&&player_energy==500&&g->mode==dead_gen2);}
    g->mode(g);if(player_energy!=expected)fprintf(stderr,"dx=%d dy=%d enabled=%d actual=%d expected=%d LOS=%d\n",(s32)x_pos-q,(s32)y_pos-q,host_generator_blast_radius,player_energy,expected,LineOfSight(&(AvpXY){(s32)x_pos,(s32)y_pos},&(AvpXY){q,q}));assert(g->animframe==6&&g->mode==NULL&&!g->host_static&&player_energy==expected);
    assert(collmap[3*AVP_AMP_GRID_W+3]==0&&sound_count==1);
}
int main(void){
    AvpAmp *g;
    finish(setup(0,0x20000,0,0,0,0),450);
    finish(setup(0,0x10000,0,1,0,0),450);
    finish(setup(0,0,0,0,1,0),500);
    const s32 offsets[][2]={{0,0},{0xffff,0},{0x10000,0},{0x10001,0},{-0x10000,0},{0,-0x10001},{0xc000,0xc000},{0xe000,0xe000}};
    for(unsigned i=0;i<sizeof offsets/sizeof *offsets;i++){
        g=setup(1,offsets[i][0],offsets[i][1],0,0,0);
        /* Use actual barrel explosion as oracle; damage is fixed, not falloff. */
        AvpAmp *b=amp_req();b->xpos=b->ypos=q;b->energy=200;b->timer=0x10000;b->xvel=4;b->yvel=6;b->animframe=3;b->mode=exp_handle;b->creature=AC_DRUM;exp_handle(b);
        int hit=player_energy==300;assert(hit||player_energy==500);g=setup(1,offsets[i][0],offsets[i][1],0,0,0);
        finish(g,hit?450:500);
        assert(hit==(i==0||i==1||i==2||i==4||i==6));
    }
    finish(setup(1,0x10000,0,1,0,0),500);
    finish(setup(1,0,0,0,1,0),500);
    g=setup(1,0x8000,0,0,0,1);
    AvpAmp *near=amp_req();near->xpos=q-0x8000;near->ypos=q;near->mode=noop;near->flags=1u<<AMP_KILLABLE;near->energy=100;
    AvpAmp *far=amp_req();far->xpos=q-0x18000;far->ypos=q;far->mode=noop;far->flags=1u<<AMP_KILLABLE;far->energy=100;
    finish(g,450);assert(callback_count==1&&near->energy==50&&far->energy==100&&g->energy==0);
    puts("PASS: generator death frame, legacy behavior, barrel distance parity (8 positions), LOS, invincibility, collateral damage, callback and single sound");return 0;
}
