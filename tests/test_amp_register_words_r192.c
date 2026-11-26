/* RE39 R192: 68000 signed-word register interpretation in original AMP search,
 * projectile scaling, and saved positions. Work from actual production code;
 * external memory/audio operations are stubbed, not replaced within AMP. */
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
static unsigned rng_calls;
u16 avp_random(void){++rng_calls;return rng_calls==1u?0x1a4u:3u;}
s32 sin_d0(u16 a){(void)a;return 0;}
s32 cos_d0(u16 a){(void)a;return 0x4000;}

static s16 reference_word(uint32_t bits)
{
    int64_t n=(int64_t)(bits&UINT32_C(65535));
    return (s16)(n>=32768?n-65536:n);
}
static s32 reference_projectile(s32 heading,u16 speed,unsigned shift)
{
    int64_t h=reference_word((uint32_t)heading);
    int64_t m=reference_word((uint32_t)speed*256u);
    uint32_t bits=(uint32_t)(h*m);
    return reference_word((bits<<shift)>>16);
}
static void reset_search(AvpAmp *a,u16 dir,u16 count)
{
    InitAMPs();build_level();
    x_pos=y_pos=UINT32_C(0x00100000);
    a->xpos=(s32)x_pos;a->ypos=(s32)y_pos;
    a->mode=lostplayer;a->creature=AC_ALIEN;
    a->energy=a->oldenergy=100;
    a->flags=0;
    invisflag=1;
    a->timer=0;
    a->xvector=amp_projectile_word(((u32)dir<<16)|count);
    rng_calls=0;
}
int main(void)
{
    unsigned long long count=0;
    for(uint32_t b=0;b<=UINT16_MAX;b++){
        const s16 expected=reference_word(b);
        assert(amp_word_signed((u16)b)==expected);
        for(unsigned dir=0;dir<4u;dir++){
            AvpAmp seq={0};
            set_fseq(&seq,(u16)dir,expected);
            assert(fseq_count(&seq)==expected);
            assert(fseq_dir(&seq)==dir);
            ++count;
        }
    }
    for(int32_t h=INT16_MIN;h<=INT16_MAX;h++){
        for(unsigned s=0;s<3u;s++){
            static const u16 speed[]={24u,30u,40u};
            assert(scale_projectile_component(h,speed[s])==reference_projectile(h,speed[s],2));
            assert(amp_projectile_component(h,speed[s])==reference_projectile(h,speed[s],3));
            count+=2u;
        }
    }
    /* Actual cloaked-player search: signed-word decrement, fresh random
     * heading at negative count, and no random at the $8000 word wrap. */
    AvpAmp *a=&level1amps[0];
    reset_search(a,2u,2u);
    lostplayer(a);
    assert(fseq_dir(a)==2u && fseq_count(a)==1 && rng_calls==0u);
    lostplayer(a);
    assert(fseq_dir(a)==2u && fseq_count(a)==0 && rng_calls==0u);
    lostplayer(a);
    assert(fseq_dir(a)==3u && fseq_count(a)==0xa4 && rng_calls==2u);
    reset_search(a,1u,0x8000u);
    lostplayer(a);
    assert(fseq_dir(a)==1u && fseq_count(a)==INT16_MAX && rng_calls==0u);
    reset_search(a,3u,0x7fffu);
    lostplayer(a);
    assert(fseq_dir(a)==3u && fseq_count(a)==INT16_MAX-1 && rng_calls==0u);
    a->flags|=(u16)(1u<<AMP_PHIT);
    lostplayer(a);
    assert(a->mode==chase_player && a->xvector==0 && rng_calls==0u);
    /* One live AMP saved through original level serialization. Low words of
     * shifted 16.16 positions must retain sign in the 16-bit save records. */
    InitAMPs();build_level();
    a=amp_req();
    a->mode=chase_player;a->astype=1;
    a->xpos=INT32_MIN;a->ypos=INT32_MAX;
    ram_save();
    const AvpAmpSavedPlacement *saved=avp_amp_saved_level(1);
    assert(saved[0].x==reference_word(((u32)a->xpos)>>8));
    assert(saved[0].y==reference_word(((u32)a->ypos)>>8));
    assert(saved[1].x==-1);
    /* Actual player weapon spawn, preserving original heading/velocity. */
    x_pos=y_pos=UINT32_C(0x00100000);
    a=player_weapon(INT16_MIN,INT16_MAX,0u,77u,0,0);
    assert(a->xvector==reference_projectile(INT16_MIN,30u,2));
    assert(a->yvector==reference_projectile(INT16_MAX,30u,2));
    printf("R192 Jaguar AMP register words: %llu comparisons + cloak search, save, launch PASS\n",count);
    return 0;
}
