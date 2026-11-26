#include "pc/pc_hybrid_stream_policy.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static void test_growth(void)
{
    size_t capacity=0u;
    int grow=0;
    assert(avp_hybrid_stream_capacity(56u,0u,AVP_HYBRID_GEOMETRY_MAX_BYTES,&capacity,&grow));
    assert(capacity==4096u&&grow==1);
    assert(avp_hybrid_stream_capacity(4000u,capacity,AVP_HYBRID_GEOMETRY_MAX_BYTES,&capacity,&grow));
    assert(capacity==4096u&&grow==0);
    assert(avp_hybrid_stream_capacity(4097u,capacity,AVP_HYBRID_GEOMETRY_MAX_BYTES,&capacity,&grow));
    assert(capacity==8192u&&grow==1);
    assert(avp_hybrid_stream_capacity(8191u,capacity,AVP_HYBRID_GEOMETRY_MAX_BYTES,&capacity,&grow));
    assert(capacity==8192u&&grow==0);
    assert(avp_hybrid_stream_capacity(30000000u,capacity,AVP_HYBRID_GEOMETRY_MAX_BYTES,&capacity,&grow));
    assert(capacity==32u*1024u*1024u&&grow==1);
    /* 2560x1440 source tape needs 56.25 MiB, within the original 64 MiB
     * AVFL source builder's already-validated maximum. */
    assert(avp_hybrid_stream_capacity(2560u*1440u*16u+32u,capacity,
        AVP_HYBRID_GEOMETRY_MAX_BYTES,&capacity,&grow));
    assert(capacity==AVP_HYBRID_GEOMETRY_MAX_BYTES&&grow==1);
    assert(!avp_hybrid_stream_capacity(0u,0u,AVP_HYBRID_GEOMETRY_MAX_BYTES,&capacity,&grow));
    assert(!avp_hybrid_stream_capacity(AVP_HYBRID_GEOMETRY_MAX_BYTES+1u,0u,
               AVP_HYBRID_GEOMETRY_MAX_BYTES,&capacity,&grow));
    assert(!avp_hybrid_stream_capacity(16u,0u,0u,&capacity,&grow));
    assert(!avp_hybrid_stream_capacity(16u,0u,100u,NULL,&grow));
    assert(!avp_hybrid_stream_capacity(16u,0u,100u,&capacity,NULL));
}
static void test_image(void)
{
    size_t n=0u;
    assert(avp_hybrid_stream_foreground_size(640u,480u,&n));
    assert(n==640u*480u*4u);
    assert(avp_hybrid_stream_foreground_size(3840u,2160u,&n));
    assert(n==3840u*2160u*4u);
    assert(!avp_hybrid_stream_foreground_size(8192u,8192u,&n));
    assert(!avp_hybrid_stream_foreground_size(8193u,1u,&n));
    assert(!avp_hybrid_stream_foreground_size(1u,0u,&n));
    assert(!avp_hybrid_stream_foreground_size(UINT32_MAX,UINT32_MAX,&n));
    assert(!avp_hybrid_stream_foreground_size(1u,1u,NULL));
}
int main(void)
{
    test_growth();test_image();
    puts("R364 reusable hybrid stream allocation policy: PASS");
    return 0;
}
