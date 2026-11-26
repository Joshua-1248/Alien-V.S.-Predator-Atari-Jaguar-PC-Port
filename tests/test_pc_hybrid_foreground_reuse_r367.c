#include "pc/pc_hybrid_stream_policy.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_r367_exact_foreground_reuse(void)
{
    uint32_t cached[12]={0xff000000u,0xff000001u,0xffabcdefu,
        0u,0xff080102u,0u,0xff324556u,0xff000000u,
        0xff112233u,0u,0xffaabbccu,0u};
    uint32_t input[12];
    memcpy(input,cached,sizeof(cached));
    /* Correctly submitted unchanged image: no CPU staging write or GPU copy. */
    assert(avp_hybrid_stream_foreground_upload_needed(cached,input,4u,3u,
        sizeof(cached),1)==0);
    /* Brand-new/unsubmitted images are ALWAYS copied, even if pixels match. */
    assert(avp_hybrid_stream_foreground_upload_needed(cached,input,4u,3u,
        sizeof(cached),0)==1);
    input[11]=0xff010101u;
    assert(avp_hybrid_stream_foreground_upload_needed(cached,input,4u,3u,
        sizeof(cached),1)==1);
    input[11]=cached[11];
    input[0]=0u;
    assert(avp_hybrid_stream_foreground_upload_needed(cached,input,4u,3u,
        sizeof(cached),1)==1);
    assert(avp_hybrid_stream_foreground_upload_needed(NULL,input,4u,3u,
        sizeof(cached),0)==1);
    assert(avp_hybrid_stream_foreground_upload_needed(NULL,input,4u,3u,
        sizeof(cached),1)==-1);
    assert(avp_hybrid_stream_foreground_upload_needed(cached,NULL,4u,3u,
        sizeof(cached),1)==-1);
    assert(avp_hybrid_stream_foreground_upload_needed(cached,input,4u,3u,
        sizeof(cached)-4u,1)==-1);
    assert(avp_hybrid_stream_foreground_upload_needed(cached,input,0u,3u,
        sizeof(cached),1)==-1);
    assert(avp_hybrid_stream_foreground_upload_needed(cached,input,
        UINT32_MAX,UINT32_MAX,sizeof(cached),1)==-1);
}
int main(void)
{
    test_r367_exact_foreground_reuse();
    puts("R367 exact-content foreground transfer policy: PASS");
    return 0;
}
