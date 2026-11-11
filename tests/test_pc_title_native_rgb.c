/* R82: no-ROM proof of original 320x240 reference scanout placement. */
#include "pc/pc_title_native.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void)
{
    const unsigned stride=720u;
    uint32_t *source=(uint32_t*)calloc((size_t)stride*240u,sizeof(uint32_t));
    uint32_t *pixels=(uint32_t*)malloc(320u*254u*sizeof(uint32_t));
    AvpJaguarFramebuffer fb={320u,254u,pixels};
    assert(source&&pixels);
    for(unsigned y=0u;y<240u;y++)for(unsigned x=0u;x<320u;x++)
        source[(size_t)y*stride+x]=((x*37u+y*13u)&255u)<<16|((x+y)&255u)<<8|(y&255u);
    memset(pixels,0xff,320u*254u*sizeof(uint32_t));
    assert(avp_pc_title_native_copy_rgb(source,320u,240u,stride,&fb));
    for(unsigned y=0u;y<14u;y++)for(unsigned x=0u;x<320u;x++)
        assert(pixels[(size_t)y*320u+x]==0u);
    for(unsigned y=0u;y<240u;y++)for(unsigned x=0u;x<320u;x++)
        assert(pixels[(size_t)(y+14u)*320u+x]==source[(size_t)y*stride+x]);
    assert(!avp_pc_title_native_copy_rgb(source,319u,240u,stride,&fb));
    assert(!avp_pc_title_native_copy_rgb(source,320u,239u,stride,&fb));
    assert(!avp_pc_title_native_copy_rgb(source,320u,240u,319u,&fb));
    assert(avp_pc_title_native_create(NULL)==NULL);
    free(source);free(pixels);
    puts("R82 native-title RGB layout: PASS");
    return 0;
}
