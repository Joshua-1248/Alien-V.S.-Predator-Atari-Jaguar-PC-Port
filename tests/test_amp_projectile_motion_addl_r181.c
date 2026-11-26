/* RE39 R181: execute original AMP Smart Disc / Flame / Bolt motion modes
 * against 68000 32-bit ADD.L wrap boundaries. Uses the actual production
 * amp.c, not a duplicate implementation of discmove/flame_on. */
#include "amp.h"
#include "collision.h"
#include "player.h"
#include "hud.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

s16 cur_level=1,num_cocoons;
u32 ccn_xsave,ccn_ysave;
AvpCocoonState cocoon_data[AVP_MAX_COCOONS];
void place_grid(void){}
s16 game_over,key_lock;u8 acs_level;
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
u32 x_pos,y_pos;u16 invisflag;
s32 score;u32 fire_distance;
s16 player_energy,player_type,use_cocoon;
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}
u16 avp_random(void){return 0;}

/* Independent two's-complement word oracle: no signed arithmetic overflow. */
static s32 from_bits(u32 bits)
{
    return bits<=INT32_MAX?(s32)bits:(s32)((int64_t)bits-INT64_C(4294967296));
}
static void one(unsigned kind,int edge,int reverse)
{
    AvpAmp *a;
    s32 x,y,vx,vy;
    u32 expected_x,expected_y;
    InitAMPs();build_level();
    a=amp_req();
    assert(a==&level1amps[0]);
    /* A player-owned projectile avoids player-contact damage. The fixture
     * leaves collision map empty and SafePos walkable, so production executes
     * its actual travel step. No AI, bot or host hook is substituted. */
    a->flags=(u16)(1u<<AMP_PLAYER);
    a->creature=(u16)kind;
    a->mode=(kind==AC_SMART)?discmove:flame_on;
    a->animseq=AS_STAND;a->animframe=0;a->timer=0;
    if(edge){
        x=reverse?INT32_MIN+0x100:INT32_MAX-0x100;
        y=reverse?INT32_MAX-0x100:INT32_MIN+0x100;
        vx=reverse?-0x1800:0x1800;
        vy=reverse?0x2400:-0x2400;
    }else{
        x=reverse?0x00050000:0x00028000;
        y=reverse?0x00040000:0x00038000;
        vx=reverse?-0x1800:0x1800;
        vy=reverse?0x2400:-0x2400;
    }
    a->xpos=x;a->ypos=y;a->xvector=vx;a->yvector=vy;
    x_pos=y_pos=0x00100000u;
    player_type=PT_HUMAN;
    expected_x=(u32)x+(u32)vx;
    expected_y=(u32)y+(u32)vy;
    if(kind==AC_SMART)discmove(a);
    else flame_on(a);
    assert(a->xpos==from_bits(expected_x));
    assert(a->ypos==from_bits(expected_y));
    assert((u32)a->xpos==expected_x && (u32)a->ypos==expected_y);
    if(kind==AC_SMART)assert(a->mode==discmove);
    else assert(a->mode==flame_on);
    if(edge){
        if(reverse)assert(a->xpos>0 && a->ypos<0);
        else assert(a->xpos<0 && a->ypos>0);
    }
}
int main(void)
{
    const unsigned kinds[]={AC_SMART,AC_FIRE,AC_BOLT};
    for(unsigned i=0;i<sizeof(kinds)/sizeof(kinds[0]);i++)
        for(int edge=0;edge<=1;edge++)
            for(int reverse=0;reverse<=1;reverse++)
                one(kinds[i],edge,reverse);
    puts("R181 actual AMP Smart Disc/Flame/Bolt ADD.L travel: 12 cases PASS");
    return 0;
}
