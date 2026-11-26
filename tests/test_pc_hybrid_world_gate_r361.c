#include "pc/pc_hybrid_world_gate.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define CHECK(expr) do { if(!(expr)) { \
    fprintf(stderr,"R361 world gate test failed: %s at %d\n",#expr,__LINE__); \
    return 1; \
} } while(0)

int main(void)
{
    const uint8_t rgba[24]={
        0x12,0x34,0x56,255, 0xff,0x00,0x10,255, 0,0,0,255,
        0x00,0xaa,0xbb,255, 0xde,0xad,0xbe,255, 0x66,0x77,0x88,255};
    uint32_t world[8]={0x123456u,0xff0010u,0u,0x7f7f7f7fu,
                       0x00aabbu,0xdeadbeu,0x667788u,0x12345678u};
    uint8_t altered[24];
    uint32_t errors=UINT32_MAX;
    CHECK(avp_hybrid_world_gate_compare(rgba,sizeof(rgba),world,4,3,2,&errors));
    CHECK(errors==0u);
    world[1]^=1u;
    CHECK(!avp_hybrid_world_gate_compare(rgba,sizeof(rgba),world,4,3,2,&errors));
    CHECK(errors==1u);
    world[1]^=1u;
    memcpy(altered,rgba,sizeof(altered));
    altered[3]=254u;
    CHECK(!avp_hybrid_world_gate_compare(altered,sizeof(altered),world,4,3,2,&errors));
    CHECK(errors==1u);
    altered[3]=255u;
    altered[7]^=1u;
    CHECK(!avp_hybrid_world_gate_compare(altered,sizeof(altered),world,4,3,2,&errors));
    CHECK(errors==1u);
    CHECK(!avp_hybrid_world_gate_compare(rgba,sizeof(rgba)-1u,world,4,3,2,&errors));
    CHECK(errors==UINT32_MAX);
    CHECK(!avp_hybrid_world_gate_compare(rgba,sizeof(rgba),world,2,3,2,&errors));
    CHECK(!avp_hybrid_world_gate_compare(rgba,sizeof(rgba),world,4,0,2,&errors));
    CHECK(!avp_hybrid_world_gate_compare(rgba,sizeof(rgba),world,4,UINT32_MAX,UINT32_MAX,&errors));
    CHECK(!avp_hybrid_world_gate_compare(NULL,sizeof(rgba),world,4,3,2,&errors));
    CHECK(!avp_hybrid_world_gate_compare(rgba,sizeof(rgba),world,4,3,2,NULL));
    puts("R361 source-backed world/Software pixel parity gate PASS");
    return 0;
}
