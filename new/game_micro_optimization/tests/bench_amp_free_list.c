#define _POSIX_C_SOURCE 200809L
#include "amp_free_list.h"
#include "amp.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static int original_valid(const u16 *list,unsigned count)
{
    if(count>AVP_NUM_AMPS)return 0;
    for(unsigned i=0;i<count;i++){
        if(list[i]>=AVP_NUM_AMPS)return 0;
        for(unsigned j=0;j<i;j++)if(list[i]==list[j])return 0;
    }
    return 1;
}
static uint64_t now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (uint64_t)ts.tv_sec*UINT64_C(1000000000)+(uint64_t)ts.tv_nsec;
}
int main(void)
{
    enum { ITER=25000 };
    u16 list[AVP_NUM_AMPS];
    volatile unsigned checksum=0;
    uint64_t start,original_ns,optimized_ns;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++)list[i]=(u16)i;
    start=now();
    for(unsigned i=0;i<ITER;i++)checksum+=(unsigned)original_valid(list,AVP_NUM_AMPS);
    original_ns=now()-start;
    start=now();
    for(unsigned i=0;i<ITER;i++)checksum+=(unsigned)avp_game_micro_amp_free_list_valid(list,AVP_NUM_AMPS);
    optimized_ns=now()-start;
    printf("AMP allocator 300-free-slot validation: original %.2f ns, optimized %.2f ns, ratio %.2fx, checksum=%u\n",
        (double)original_ns/ITER,(double)optimized_ns/ITER,(double)original_ns/optimized_ns,checksum);
    return checksum==2u*ITER?0:1;
}
