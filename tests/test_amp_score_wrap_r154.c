/* RE38 R154: original-only 68000 score arithmetic, excluding multiplayer and
 * all PC gameplay additions. A Motorola 68000 ADD.L wraps modulo 2^32;
 * the subsequent negative-result clamp must not invoke signed C overflow. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include "amp.h"
#include "player.h"

s32 score;
s16 player_type, player_energy;
u32 x_pos,y_pos;

static const int32_t retail_scores[40]={
    10000,50000,5000,300,900,6000,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    100,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,200000,50000,0
};

static int32_t expected(int32_t initial,unsigned species,unsigned creature,unsigned mask)
{
    int32_t add=retail_scores[creature];
    if(!(mask&1u))return initial;
    if(species==PT_ALIEN){
        if(creature==AC_HUMAN)add*=4;
    }else if(species==PT_PREDATOR){
        if(mask&4u){
            add=-add;
            if(!(mask&2u))add*=2;
        }else if(!(mask&2u))add/=2;
    }
    /* Use 64-bit math to reconstruct the 68000 low 32-bit result. */
    const uint32_t wrapped=(uint32_t)((int64_t)initial+(int64_t)add);
    return (wrapped&UINT32_C(0x80000000))?0:(int32_t)wrapped;
}

int main(void)
{
    static const int32_t inputs[]={
        0,1,50,100000,INT32_MAX-199999,INT32_MAX-1,INT32_MAX,
        INT32_MIN,INT32_MIN+1,-1,-10000
    };
    static const unsigned species[]={PT_HUMAN,PT_ALIEN,PT_PREDATOR};
    AvpAmp actor={0};
    unsigned count=0;
    for(unsigned p=0;p<sizeof(species)/sizeof(species[0]);++p)
      for(unsigned creature=0;creature<40u;++creature)
        for(unsigned mask=0;mask<8u;++mask)
          for(unsigned s=0;s<sizeof(inputs)/sizeof(inputs[0]);++s){
              player_type=(s16)species[p];
              actor.creature=(u16)creature;
              actor.flags=0;
              if(mask&1u)actor.flags|=(u16)(1u<<AMP_PHIT);
              if(mask&2u)actor.flags|=(u16)(1u<<AMP_CLOSEHIT);
              if(mask&4u)actor.flags|=(u16)(1u<<AMP_INVHIT);
              score=inputs[s];
              do_score(&actor);
              const int32_t want=expected(inputs[s],species[p],creature,mask);
              if(score!=want){
                  fprintf(stderr,"R154 mismatch p=%u creature=%u mask=%u input=%ld got=%ld want=%ld\n",
                      species[p],creature,mask,(long)inputs[s],(long)score,(long)want);
                  return 1;
              }
              ++count;
          }
    assert(count==3u*40u*8u*11u);
    printf("R154 original 68000 do_score parity: %u cases PASS\n",count);
    return 0;
}
