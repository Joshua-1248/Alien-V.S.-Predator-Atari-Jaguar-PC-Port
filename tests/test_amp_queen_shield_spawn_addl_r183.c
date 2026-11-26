/* RE39 R183 — run actual AMP make_shield() and cgenmode() at Jaguar ADD.L
 * boundaries. The independent oracle works on unsigned bit patterns. */
#include "amp.h"
#include "player.h"
#include "hud.h"
#include "collision.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
static s32 trig;
s32 cos_d0(u16 angle){(void)angle;return trig;}
s32 sin_d0(u16 angle){(void)angle;return trig;}

static s32 from_word(u32 bits)
{
    return bits<=INT32_MAX?(s32)bits:(s32)((int64_t)bits-INT64_C(4294967296));
}
static u32 addl(s32 base,s32 delta)
{
    return (u32)base+(u32)delta;
}
static void check_direct_shield(u32 owner_word,s32 dx,s32 dy)
{
    InitAMPs();build_level();
    AvpAmp *owner=amp_req();
    assert(owner==&level1amps[0]);
    owner->xpos=from_word(owner_word);
    owner->ypos=from_word(owner_word);
    AvpAmp *shield=make_shield(owner,dx,dy);
    assert(shield==&level1amps[1]);
    assert((u32)shield->xpos==addl(owner->xpos,dx));
    assert((u32)shield->ypos==addl(owner->ypos,dy));
    assert(shield->xvel==-2);
    assert(shield->creature==AC_AQSHIELD);
    assert(shield->mode && shield->animseq==AS_STAND && shield->animframe==0);
}
static void check_cgenmode(u32 base_bits,s32 sine_cosine)
{
    static const s32 offsets[4][2]={
        {0,-0x1a800},{0,0x1a800},{0x1a800,0},{-0x1a800,0}
    };
    InitAMPs();build_level();
    player_type=PT_ALIEN;
    AvpAmp *queen=amp_req();
    assert(queen==&level1amps[0]);
    queen->xpos=from_word(base_bits);
    queen->ypos=from_word(base_bits);
    trig=sine_cosine;
    cgenmode(queen);
    assert(queen->mode && queen->astype==43 && queen->animseq==AS_HIDDEN);
    for(unsigned i=0;i<4;i++){
        const AvpAmp *side=&level1amps[i+1];
        assert(side->creature==AC_GEN);
        assert(side->astype==(s16)(44u+i));
        assert(side->energy==200 && side->mode);
        assert((u32)side->xpos==addl(queen->xpos,offsets[i][0]));
        assert((u32)side->ypos==addl(queen->ypos,offsets[i][1]));
    }
    const s32 radial=(s32)(((int64_t)sine_cosine*0xf0)>>7);
    for(unsigned i=0;i<12;i++){
        const AvpAmp *shield=&level1amps[i+5];
        assert(shield->creature==AC_AQSHIELD);
        assert(shield->astype==49 && shield->mode);
        assert((u32)shield->xpos==addl(queen->xpos,radial));
        assert((u32)shield->ypos==addl(queen->ypos,radial));
    }
    assert(amp_req()==&level1amps[17]);
}
int main(int argc,char **argv)
{
    /* R182 negative-control mode isolates cgenmode's side generators from
     * make_shield's already independently verified signed overflow. */
    if(argc>1){
        assert(argc==2 && strcmp(argv[1],"--generator-only")==0);
        check_cgenmode(0x7fffaf00u,0x4000);
        puts("R183 Queen side-generator boundary PASS");
        return 0;
    }
    /* Normal level coordinates: exact pre-R183 positions must be preserved. */
    check_direct_shield(0x00128000u,0x7800,-0x7800);
    check_cgenmode(0x00128000u,0x4000);
    check_cgenmode(0x00128000u,-0x4000);
    /* Both signs of 68000 wrap: test the actual generator constellation. */
    check_direct_shield(0x7fffff00u,0x7800,0x8000);
    check_direct_shield(0x80000100u,-0x7800,-0x8000);
    check_cgenmode(0x7fffaf00u,0x4000);
    check_cgenmode(0x80005100u,-0x4000);
    puts("R183 Jaguar Queen generators + shield spawns ADD.L (7 scenarios) PASS");
    return 0;
}
