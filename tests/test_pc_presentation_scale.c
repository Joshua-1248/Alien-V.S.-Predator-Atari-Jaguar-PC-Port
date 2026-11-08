#include "platform_pc_scale.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
int main(void)
{
    static const struct {unsigned sw,sh,dw,dh;} cases[]={
        {320u,228u,2560u,1440u}, /* fullscreen native HUD */
        {320u,228u,320u,228u}, /* direct copy */
        {1280u,912u,2560u,1440u}, /* high-resolution world */
        {320u,228u,853u,607u}, /* uneven scaling */
        {16u,13u,7u,5u}, /* downscale */
        {3u,3u,15u,8u},
        {320u,228u,4096u,40u}, /* upper precomputed-index bound */
        {320u,228u,4097u,40u}, /* constant-memory fallback */
        {1u,17u,4096u,32u}, /* degenerate horizontal source */
        {4096u,3u,1u,7u}, /* degenerate horizontal output */
        {1280u,2u,1280u,67u}, /* no horizontal scale, vertical stretch */
        {1280u,912u,1920u,1080u}, /* asymmetric high-resolution stretch */
        {400u,12u,320u,228u}, /* downscale and vertical upscale */
        {17u,1u,17u,1u} /* direct copy */
    };
    for(unsigned i=0u;i<sizeof(cases)/sizeof(cases[0]);i++){
        unsigned sw=cases[i].sw,sh=cases[i].sh,dw=cases[i].dw,dh=cases[i].dh;
        uint32_t *src=(uint32_t*)malloc((size_t)sw*sh*sizeof(*src));
        uint32_t *dst=(uint32_t*)malloc((size_t)dw*dh*sizeof(*dst));
        assert(src&&dst);
        for(unsigned y=0u;y<sh;y++)for(unsigned x=0u;x<sw;x++)
            src[(size_t)y*sw+x]=0x00800000u|((y&255u)<<8)|(x&255u);
        avp_pc_scale_nearest32(src,sw,sh,dst,dw,dh);
        for(unsigned y=0u;y<dh;y++)for(unsigned x=0u;x<dw;x++){
            size_t sy=((uint64_t)y*sh)/dh,sx=((uint64_t)x*sw)/dw;
            assert(dst[(size_t)y*dw+x]==src[sy*sw+sx]);
        }
        free(src);free(dst);
    }
    /* Independent output/reference check on more narrow/odd-size ratios.
     * The reference uses the mathematical floor() definition, not the DDA
     * or lookup table from the host scaler. */
    uint32_t rng=0x8da6b441u;
    for(unsigned test=0;test<150u;test++){
        rng=rng*1664525u+1013904223u;
        const unsigned sw=1u+(rng%509u);
        rng=rng*1664525u+1013904223u;
        const unsigned sh=1u+(rng%47u);
        rng=rng*1664525u+1013904223u;
        const unsigned dw=1u+(rng%5017u);
        rng=rng*1664525u+1013904223u;
        const unsigned dh=1u+(rng%59u);
        uint32_t *src=malloc((size_t)sw*sh*sizeof(*src));
        uint32_t *dst=malloc((size_t)dw*dh*sizeof(*dst));
        assert(src&&dst);
        for(size_t i=0;i<(size_t)sw*sh;i++)src[i]=(uint32_t)i^(rng<<1u);
        avp_pc_scale_nearest32(src,sw,sh,dst,dw,dh);
        for(unsigned y=0u;y<dh;y++)for(unsigned x=0u;x<dw;x++)
            assert(dst[(size_t)y*dw+x]==
                src[(size_t)(((uint64_t)y*sh)/dh)*sw+(unsigned)(((uint64_t)x*sw)/dw)]);
        free(src);free(dst);
    }
    puts("PC scaler: exact source sampling PASS (14 fixed + 150 randomized cases)");
    return 0;
}
