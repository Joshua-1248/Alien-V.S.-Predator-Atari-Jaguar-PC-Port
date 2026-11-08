#include "amp_free_list.h"
#include "amp.h"
#include <stdint.h>
#include <stdio.h>

static uint32_t rng=0x9E3779B9u;
static uint32_t next_rand(void)
{
    rng^=rng<<13; rng^=rng>>17; rng^=rng<<5;
    return rng;
}
/* Literal copy of the original allocator's range and pairwise tests. */
static int original_valid(const u16 *list,unsigned count)
{
    if(count>AVP_NUM_AMPS)return 0;
    for(unsigned i=0;i<count;i++){
        if(list[i]>=AVP_NUM_AMPS)return 0;
        for(unsigned j=0;j<i;j++)if(list[i]==list[j])return 0;
    }
    return 1;
}
#define CHECK(X) do { if(!(X)){fprintf(stderr,"FAIL at line %d: %s\n",__LINE__,#X);return 1;} } while(0)
int main(void)
{
    u16 list[AVP_NUM_AMPS];
    uint64_t comparisons=0;
    for(unsigned n=0;n<=AVP_NUM_AMPS;n++){
        for(unsigned i=0;i<AVP_NUM_AMPS;i++)list[i]=(u16)i;
        CHECK(original_valid(list,n)==avp_game_micro_amp_free_list_valid(list,n));comparisons++;
        for(unsigned dup=1;dup<n;dup++){
            list[dup]=list[dup-1u];
            CHECK(original_valid(list,n)==avp_game_micro_amp_free_list_valid(list,n));comparisons++;
            list[dup]=(u16)dup;
        }
        for(unsigned i=0;i<n;i++){
            list[i]=(u16)AVP_NUM_AMPS;
            CHECK(original_valid(list,n)==avp_game_micro_amp_free_list_valid(list,n));comparisons++;
            list[i]=(u16)i;
        }
        /* Every legal rotation of a full unique list is still valid. */
        if(n){
            u16 tmp=list[0];
            for(unsigned i=0;i+1u<n;i++)list[i]=list[i+1u];
            list[n-1u]=tmp;
            CHECK(original_valid(list,n)==avp_game_micro_amp_free_list_valid(list,n));comparisons++;
        }
    }
    /* Randomized lengths, valid permutations, duplicates, bad range IDs. */
    for(unsigned trial=0;trial<120000u;trial++){
        unsigned count=next_rand()%(AVP_NUM_AMPS+1u);
        unsigned mode=next_rand()%5u;
        for(unsigned i=0;i<AVP_NUM_AMPS;i++)list[i]=(u16)i;
        for(unsigned i=0;i<count;i++){
            unsigned j=i+(next_rand()%(AVP_NUM_AMPS-i));
            u16 a=list[i];list[i]=list[j];list[j]=a;
        }
        if(count && mode==1u){unsigned i=next_rand()%count,j=next_rand()%count;list[i]=list[j];}
        if(count && mode==2u)list[next_rand()%count]=(u16)(AVP_NUM_AMPS+next_rand()%3000u);
        if(count>=2u && mode==3u){unsigned j=1u+(next_rand()%(count-1u));list[j]=list[0];}
        if(count && mode==4u)list[count-1u]=0xffffu;
        CHECK(original_valid(list,count)==avp_game_micro_amp_free_list_valid(list,count));comparisons++;
    }
    for(unsigned i=0;i<AVP_NUM_AMPS;i++)list[i]=(u16)i;
    CHECK(!avp_game_micro_amp_free_list_valid(list,AVP_NUM_AMPS+1u));comparisons++;
    CHECK(avp_game_micro_amp_free_list_valid(NULL,0u));comparisons++;
    CHECK(!avp_game_micro_amp_free_list_valid(NULL,1u));comparisons++;
    printf("PASS: %llu exhaustive-size, duplicate, invalid-id and randomized allocator validation parity cases\n",(unsigned long long)comparisons);
    return 0;
}
