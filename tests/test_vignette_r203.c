/* RE40 R203: host-only Vignette correctness across aspect ratios, viewport
 * origins, split screens, intensity extremes, strides and cache reuse. */
#include "pc/pc_vignette.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void check_shape(unsigned w,unsigned h)
{
    AvpPcVignetteMask mask={0};
    const unsigned pitch=w+9u,fullh=h+8u;
    uint32_t *p=malloc((size_t)pitch*fullh*sizeof(*p));
    uint32_t *baseline=malloc((size_t)pitch*fullh*sizeof(*p));
    assert(p&&baseline);
    for(size_t i=0;i<(size_t)pitch*fullh;i++)p[i]=baseline[i]=0x00c8e080u;
    avp_pc_vignette_apply(&mask,p,pitch,pitch,fullh,4u,3u,w,h,0u);
    assert(memcmp(p,baseline,(size_t)pitch*fullh*sizeof(*p))==0);
    assert(mask.weights==NULL); /* disabled or zero-strength is zero-cost */
    avp_pc_vignette_apply(&mask,p,pitch,pitch,fullh,4u,3u,w,h,100u);
    assert(mask.weights && mask.width==w && mask.height==h);
    assert(p[(size_t)3u*pitch+4u]<baseline[(size_t)3u*pitch+4u]);
    assert(p[(size_t)(h/2u+3u)*pitch+(w/2u+4u)]==0x00c8e080u);
    assert(p[(size_t)3u*pitch+3u]==0x00c8e080u); /* guard column */
    assert(p[(size_t)(h+3u)*pitch+4u]==0x00c8e080u); /* guard row */
    assert(p[(size_t)3u*pitch+4u]==p[(size_t)3u*pitch+(w+3u)]); /* horizontal symmetry */
    assert(p[(size_t)3u*pitch+4u]==p[(size_t)(h+2u)*pitch+4u]); /* vertical symmetry */
    uint8_t *same=mask.weights;
    assert(avp_pc_vignette_prepare(&mask,w,h));
    assert(mask.weights==same);
    /* At 50%, the same corner must be brighter; no stateful accumulation. */
    memcpy(p,baseline,(size_t)pitch*fullh*sizeof(*p));
    avp_pc_vignette_apply(&mask,p,pitch,pitch,fullh,4u,3u,w,h,50u);
    uint32_t half=p[(size_t)3u*pitch+4u];
    assert(half>0u && half<0x00c8e080u);
    memcpy(p,baseline,(size_t)pitch*fullh*sizeof(*p));
    avp_pc_vignette_apply(&mask,p,pitch,pitch,fullh,4u,3u,w,h,100u);
    assert(half>p[(size_t)3u*pitch+4u]);
    assert(avp_pc_vignette_prepare(&mask,w/2u,h/2u));
    assert(mask.width==w/2u && mask.height==h/2u);
    avp_pc_vignette_destroy(&mask);
    assert(!mask.weights && !mask.width && !mask.height);
    free(p);free(baseline);
}

int main(void)
{
    const unsigned shapes[][2]={{320,228},{640,360},{640,400},
        {1024,256},{401,300},{128,128}};
    for(unsigned n=0;n<sizeof(shapes)/sizeof(shapes[0]);n++)
        check_shape(shapes[n][0],shapes[n][1]);
    /* A shared 2-player framebuffer: each player gets local corners. */
    {
        AvpPcVignetteMask a={0},b={0};
        uint32_t p[64*48];
        for(unsigned i=0;i<64u*48u;i++)p[i]=0x00ffffffu;
        avp_pc_vignette_apply(&a,p,64,64,48,0,0,32,48,100);
        assert(p[0]<0x00ffffffu);
        assert(p[31]<0x00ffffffu);
        assert(p[32]==0x00ffffffu); /* second player's pixels untouched */
        avp_pc_vignette_apply(&b,p,64,64,48,32,0,32,48,100);
        assert(p[31]==p[32]); /* symmetric split boundary */
        avp_pc_vignette_destroy(&a);avp_pc_vignette_destroy(&b);
    }
    /* Rectangles outside the target must never write, including overflow. */
    {
        AvpPcVignetteMask m={0};uint32_t p[16];
        for(unsigned i=0;i<16u;i++)p[i]=0x00112233u;
        avp_pc_vignette_apply(&m,p,4,4,4,3,3,2,2,100);
        avp_pc_vignette_apply(&m,p,4,4,4,0,0,4,4,101);
        for(unsigned i=0;i<16u;i++)assert(p[i]==0x00112233u);
        assert(!m.weights);
    }
    puts("R203 vignette 4:3 widescreen split-screen mask cache off/strength PASS");
    return 0;
}
