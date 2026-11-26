/* RE39 R182 — actual AMP enemy chase uses 68000 ADD.L for 16.16 moves.
 * Observe the candidate through the linked SafePos test stub, before the
 * source's post-collision target-clamp. No replacement chase implementation. */
#include "amp.h"
#include "collision.h"
#include "player.h"
#include "hud.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

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
extern unsigned avp_test_amp_safe_calls;
extern s32 avp_test_amp_safe_x,avp_test_amp_safe_y;
extern int avp_test_amp_force_blocked;
static s32 word_signed(u32 bits)
{
    return bits<=INT32_MAX?(s32)bits:(s32)((int64_t)bits-INT64_C(4294967296));
}
static void test_direction(int dx,int dy,int edge,int reverse)
{
    const u32 high=UINT32_C(0x7fffff00),low=UINT32_C(0x80000100);
    const u32 normal=UINT32_C(0x00028000);
    const u32 base=edge?(reverse?low:high):normal;
    AvpAmp *a;
    s32 ox,oy,xstep,ystep;
    u32 expect_x,expect_y;
    InitAMPs();build_level();
    a=amp_req();assert(a==&level1amps[0]);
    a->mode=chase_player;a->creature=AC_ALIEN;
    a->animseq=AS_RUN;a->energy=a->oldenergy=100;
    a->xvel=a->yvel=edge?-0x1800:0x1800;
    /* A negative signed speed is the CPU-boundary stress input: all original
     * retail templates use positive speeds, checked independently below. */
    ox=word_signed(edge&&!dx?(reverse?UINT32_C(0x80008000):UINT32_C(0x7fff8000)):base);
    oy=word_signed(edge&&!dy?(reverse?UINT32_C(0x80008000):UINT32_C(0x7fff8000)):base);
    a->xpos=ox;a->ypos=oy;a->yvector=0;
    player_type=PT_HUMAN;discflag=invisflag=0;
    /* Centered target: near signed edges, travel direction must be - at
     * the high end and + at the low end. Ordinarily pick either direction. */
    if(edge){
        assert(dx==0 || dx==(reverse?1:-1));
        assert(dy==0 || dy==(reverse?1:-1));
        /* Keep the player outside the original 60-pixel melee square, yet
         * close enough to remain in the 1152-pixel pursuit wake gate. */
        const u32 target=reverse?UINT32_C(0x80008000):UINT32_C(0x7fff8000);
        x_pos=dx?target:(u32)ox;
        y_pos=dy?target:(u32)oy;
    }else{
        x_pos=(u32)(normal+(dx<0?-0x10000:dx>0?0x10000:0));
        y_pos=(u32)(normal+(dy<0?-0x10000:dy>0?0x10000:0));
    }
    xstep=dx*(s32)a->xvel;
    ystep=dy*(s32)a->yvel;
    expect_x=(u32)ox+(u32)xstep;
    expect_y=(u32)oy+(u32)ystep;
    avp_test_amp_force_blocked=0;
    avp_test_amp_safe_calls=0;
    chase_player(a);
    assert(avp_test_amp_safe_calls==1u);
    assert((u32)avp_test_amp_safe_x==expect_x);
    assert((u32)avp_test_amp_safe_y==expect_y);
    assert(avp_test_amp_safe_x==word_signed(expect_x));
    assert(avp_test_amp_safe_y==word_signed(expect_y));
    if(edge){
        if(dx || dy){
            if(reverse){
                if(dx)assert(avp_test_amp_safe_x>0);
                if(dy)assert(avp_test_amp_safe_y>0);
            }else{
                if(dx)assert(avp_test_amp_safe_x<0);
                if(dy)assert(avp_test_amp_safe_y<0);
            }
        }
    }
}
int main(void)
{
    /* The 32-bit edge tests actually overflow in the R181 signed C code.
     * The normal tests prove identical results with retail positive speeds. */
    for(int reverse=0;reverse<=1;reverse++){
        const int dir=reverse?1:-1;
        test_direction(dir,0,1,reverse);
        test_direction(0,dir,1,reverse);
        test_direction(dir,dir,1,reverse);
    }
    test_direction(1,0,0,0);
    test_direction(-1,0,0,0);
    test_direction(0,1,0,0);
    test_direction(0,-1,0,0);
    test_direction(1,1,0,0);
    test_direction(-1,-1,0,0);
    test_direction(1,-1,0,0);
    test_direction(-1,1,0,0);
    puts("R182 production-linked AMP chase ADD.L: 14 ordinary/boundary moves PASS");
    return 0;
}
