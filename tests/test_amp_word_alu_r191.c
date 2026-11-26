/* RE39 R191: m68k 16-bit ADD/SUB.W semantics in original AMP translation.
 * Test the exact production helper across the complete signed-word domain,
 * then run real acid, shield, and spark update paths at word boundaries. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/game/amp.c"

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

static s16 retail_word(int32_t original,int32_t delta)
{
    /* Independent arithmetic model: reduce signed 64-bit sum to low word. */
    int64_t sum=(int64_t)original+delta;
    int64_t mod=((sum%65536)+65536)%65536;
    return (s16)(mod<32768?mod:mod-65536);
}

int main(void)
{
    static const int delta[]={-200,-100,-50,-25,-10,-5,-3,-1,0,1,3,5,10,50,100,200};
    unsigned long long count=0;
    for(int32_t i=-32768;i<=32767;i++){
        for(unsigned j=0;j<sizeof(delta)/sizeof(delta[0]);j++){
            assert(amp_word_add((s16)i,delta[j])==retail_word(i,delta[j]));
            ++count;
        }
    }
    InitAMPs();build_level();
    AvpAmp *a=amp_req();
    assert(a==&level1amps[0]);
    a->xpos=a->ypos=0x00108000;
    x_pos=y_pos=0x00108000;
    player_type=PT_HUMAN;
    player_energy=INT16_MIN;
    a->mode=amp_acid;
    amp_acid(a);
    assert(player_energy==32765);
    assert(a->flags&(1u<<AMP_ALTFRAME));
    assert(avp_amp_acid_contact_player(a)==1);
    assert(player_energy==32762);
    player_type=PT_ALIEN;
    amp_acid(a);
    assert(player_energy==32762);

    player_type=PT_HUMAN;
    player_energy=INT16_MIN;
    a->creature=AC_AQSHIELD;
    a->mode=shield2;
    a->xvel=INT16_MAX;
    a->flags|=(u16)(1u<<AMP_SSHIELD);
    ngens=4;
    shield2(a);
    assert(player_energy==32758);
    assert(a->xvel==INT16_MIN);
    assert(a->mode==shield2);
    a->xvel=6;
    shield2(a);
    assert(a->xvel==0);

    a->mode=spark;
    a->energy=INT16_MIN;
    spark(a);
    assert(a->energy==INT16_MAX);
    a->energy=1;
    spark(a);
    assert(a->energy==0);
    /* On the real AMP pool this final spark update releases its own slot. */
    assert(amp_req()==a);

    a->mode=amp_fallhug;
    a->yoffset=INT16_MAX-5;
    amp_fallhug(a);
    assert(a->yoffset==(s16)(INT16_MIN+4));

    printf("R191 m68k AMP word ALU: %llu full-domain comparisons + acid/shield/spark/facehug PASS\n",count);
    return 0;
}
