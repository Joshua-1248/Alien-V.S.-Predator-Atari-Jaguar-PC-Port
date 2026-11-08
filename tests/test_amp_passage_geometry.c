#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "amp.h"
#include "collision.h"
#include "doors.h"
#include "player.h"
#include "levels.h"
#include "hud.h"
#include "avp_runtime.h"

/* Regression combines the REAL SafePos and AMP chase with the same
 * two-face door record arrangement used in the original retail mazes. */
s16 cur_level=1,num_cocoons;
u32 ccn_xsave,ccn_ysave;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
void HumanPain(void){}
s16 game_over,key_lock;u8 acs_level;
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
u32 x_pos,y_pos,centre_angle;
u16 invisflag;
s16 player_energy,player_type,use_cocoon;
s32 score;
u16 maze_width,maze_height;
u8 fullbits[256];
u16 avp_random(void){return 0;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
static u8 map[8u*8u*8u];
static void init_map(void)
{
    memset(map,0,sizeof map);
    memset(fullbits,0,sizeof fullbits);
    for(unsigned y=0;y<8u;y++)for(unsigned x=0;x<8u;x++)
        map[(y*8u+x)*8u+5u]=1u;
    fullbits[64]=AVP_WB_DOOR|AVP_WB_TBDOOR;
    avp_collision_bind_maze(map,8,8);
    maze_width=maze_height=8u;
}
static void actor_setup(u32 x,u32 y)
{
    AvpAmp *a;
    InitAMPs();build_level();
    a=amp_req();
    a->creature=AC_ALIEN;
    a->mode=chase_player;
    a->xpos=(s32)x;a->ypos=(s32)y;
    a->xvel=a->yvel=0x1800;
    a->energy=a->oldenergy=150;
    a->animseq=AS_RUN;
    player_type=PT_HUMAN;
    x_pos=(3u<<16)|0x8000u;
    y_pos=(1u<<16)|0x8000u;
    invisflag=discflag=0;
}
static void retail_elevator(const char *path)
{
    u8 raw[32776u];
    FILE *f=fopen(path,"rb");
    unsigned door=(21u*64u+34u)*8u;
    AvpAmp *a;
    assert(f);
    assert(fread(raw,1,sizeof raw,f)==sizeof raw);
    assert(fgetc(f)==EOF);
    fclose(f);
    assert(raw[0]==0 && raw[1]==64 && raw[2]==0 && raw[3]==64);
    avp_collision_bind_maze(raw+8,64,64);
    maze_width=maze_height=64u;
    memset(fullbits,0,sizeof fullbits);
    fullbits[64]=AVP_WB_DOOR|AVP_WB_TBDOOR;
    /* The original level-1 elevator has a two-sided door on the
     * (33,21)->(34,21) boundary, with its opening at cell 34's byte 6. */
    assert(raw[8+(21u*64u+33u)*8u+2u]==64);
    assert(raw[8+door+0u]==64);
    assert(raw[8+door+5u]!=0);
    raw[8+door+6u]=0;
    InitAMPs();build_level();
    a=amp_req();
    a->mode=chase_player;a->creature=AC_ALIEN;
    a->xpos=(s32)((33u<<16)|0xd000u);
    a->ypos=(s32)((21u<<16)|0x8000u);
    a->xvel=a->yvel=0x1800;
    a->energy=a->oldenergy=150;
    a->animseq=AS_RUN;
    player_type=PT_HUMAN;
    /* The player is actually inside the lift cell being entered. */
    x_pos=(34u<<16)|0xf000u;
    y_pos=(21u<<16)|0x8000u;
    invisflag=discflag=0;
    collmap[21u*AVP_AMP_GRID_W+34u]=0xffffu;
    for(int i=0;i<30;i++)chase_player(a);
    assert((u32)a->xpos<0x00220000u);
    raw[8+door+6u]=80;
    for(int i=0;i<60 && (u32)a->xpos<0x00220000u;i++)chase_player(a);
    assert((u32)a->xpos>=0x00220000u);
}
int main(int argc,char **argv)
{
    AvpAmp *a;
    unsigned left=(1u*8u+1u)*8u,right=(1u*8u+2u)*8u;
    init_map();
    map[left+2u]=64;
    map[right+0u]=64;
    map[right+6u]=0;
    actor_setup((1u<<16)|0xd000u,(1u<<16)|0x8000u);
    a=&level1amps[0];
    collmap[1u*AVP_AMP_GRID_W+2u]=0xffffu;
    for(int i=0;i<30;i++)chase_player(a);
    assert((u32)a->xpos<0x00020000u);
    map[right+6u]=80u;
    for(int i=0;i<45 && (u32)a->xpos<0x00020000u;i++)chase_player(a);
    assert((u32)a->xpos>=0x00020000u);
    map[left+2u]=map[right+0u]=0;
    actor_setup((1u<<16)|0xd000u,(1u<<16)|0x8000u);
    a=&level1amps[0];
    collmap[1u*AVP_AMP_GRID_W+2u]=0xffffu;
    for(int i=0;i<45 && (u32)a->xpos<0x00020000u;i++)chase_player(a);
    assert((u32)a->xpos>=0x00020000u);
    map[right+5u]=0;
    actor_setup((1u<<16)|0xd000u,(1u<<16)|0x8000u);
    a=&level1amps[0];
    collmap[1u*AVP_AMP_GRID_W+2u]=0xffffu;
    for(int i=0;i<30;i++)chase_player(a);
    assert((u32)a->xpos<0x00020000u);
    if(argc>1)retail_elevator(argv[1]);
    return 0;
}
