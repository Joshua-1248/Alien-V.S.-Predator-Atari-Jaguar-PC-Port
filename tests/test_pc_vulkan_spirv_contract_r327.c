/* Hand-built instruction stream exercises the safety gate, not a GPU shader.
 * The stream is intentionally not claimed to be valid executable SPIR-V. */
#include "pc/pc_vulkan_spirv_contract.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
int main(void)
{
    uint32_t x[]={0x07230203u,0x00010000u,0u,32u,0u,
                  (2u<<16)|17u,1u,
                  (3u<<16)|14u,0u,1u,
                  (5u<<16)|15u,0u,4u,0x6e69616du,0u};
    uint32_t *words=NULL;
    size_t bytes=0;
    FILE *tmp;
    assert(avp_spirv_stage_contract(x,15,AVP_SPIRV_VERTEX));
    assert(!avp_spirv_stage_contract(x,15,AVP_SPIRV_FRAGMENT));
    x[11]=AVP_SPIRV_FRAGMENT;
    assert(avp_spirv_stage_contract(x,15,AVP_SPIRV_FRAGMENT));
    x[10]=0; assert(!avp_spirv_stage_contract(x,15,AVP_SPIRV_FRAGMENT)); x[10]=(5u<<16)|15u;
    x[2]=0x0008000Bu; assert(avp_spirv_stage_contract(x,15,AVP_SPIRV_FRAGMENT)); x[2]=0u;
    x[4]=1; assert(!avp_spirv_stage_contract(x,15,AVP_SPIRV_FRAGMENT)); x[4]=0;
    x[0]=0x03022307u; assert(!avp_spirv_stage_contract(x,15,AVP_SPIRV_FRAGMENT)); x[0]=0x07230203u;
    x[3]=4; assert(!avp_spirv_stage_contract(x,15,AVP_SPIRV_FRAGMENT)); x[3]=32;
    x[10]=(16u<<16)|15u;assert(!avp_spirv_stage_contract(x,15,AVP_SPIRV_FRAGMENT));x[10]=(5u<<16)|15u;
    assert(!avp_spirv_stage_contract(x,14,AVP_SPIRV_FRAGMENT));
    assert(!avp_spirv_stage_contract(NULL,15,AVP_SPIRV_FRAGMENT));
    assert(!avp_spirv_read_file("/this/file/does/not/exist.spv",AVP_SPIRV_VERTEX,&words,&bytes));
    tmp=tmpfile(); assert(tmp);
    assert(fwrite(x,sizeof(uint32_t),15,tmp)==15);
    rewind(tmp);
    /* loader accepts only named filesystem paths; parser handles the words. */
    assert(!avp_spirv_read_file("",AVP_SPIRV_FRAGMENT,&words,&bytes));
    assert(!words&&!bytes);
    fclose(tmp);
    puts("R327 SPIR-V structural gate PASS (full validation requires spirv-val)");
    return 0;
}
