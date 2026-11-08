#include "jaguar/avp_gpu/notice_composite.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    AvpGpuRgbImage base={0},over={0},num={0},out={0};
    const size_t n=128u*128u;
    base.width=base.height=128;
    base.rgb=calloc(n,3);base.cry16=calloc(n,sizeof(u16));
    over.width=80;over.height=24;
    over.rgb=calloc(80u*24u,3);over.cry16=calloc(80u*24u,sizeof(u16));
    num.width=24;num.height=32;
    num.rgb=calloc(24u*32u,3);num.cry16=calloc(24u*32u,sizeof(u16));
    assert(base.rgb&&base.cry16&&over.rgb&&over.cry16&&num.rgb&&num.cry16);
    memset(base.rgb,12,n*3u);
    memset(over.rgb,128,80u*24u*3u);
    memset(num.rgb,222,24u*32u*3u);
    for(size_t i=0;i<n;i++)base.cry16[i]=0x1111;
    for(size_t i=0;i<80u*24u;i++)over.cry16[i]=0x2222;
    for(size_t i=0;i<24u*32u;i++)num.cry16[i]=0x3333;
    assert(avp_gpu_notice_composite(&base,&over,NULL,0,&out));
    assert(out.width==128&&out.height==128);
    /* FILES 56's 80x24 AIRLOCK artwork is source-centred at x=24/y=52. */
    assert(out.rgb[((size_t)52*128+24)*3]==128);
    assert(out.cry16[(size_t)52*128+24]==0x2222);
    assert(out.rgb[((size_t)51*128+24)*3]==12);
    assert(out.rgb[((size_t)52*128+23)*3]==12);
    assert(out.rgb[((size_t)76*128+24)*3]==12);
    free(out.rgb);free(out.cry16);memset(&out,0,sizeof(out));
    assert(avp_gpu_notice_composite(&base,&over,&num,-16,&out));
    assert(out.rgb[((size_t)36*128+24)*3]==128);
    assert(out.rgb[((size_t)80*128+52)*3]==222);
    assert(out.cry16[(size_t)80*128+52]==0x3333);
    assert(out.rgb[((size_t)35*128+24)*3]==12);
    free(out.rgb);free(out.cry16);
    free(base.rgb);free(base.cry16);free(over.rgb);free(over.cry16);free(num.rgb);free(num.cry16);
    puts("R276 retail notice-overlay source-centred composition PASS");
}
