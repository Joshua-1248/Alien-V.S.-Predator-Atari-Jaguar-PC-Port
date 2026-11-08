#ifndef AVP_PLATFORM_PC_SCALE_H
#define AVP_PLATFORM_PC_SCALE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* The host's nearest-neighbour window transfer. Integer DDA produces the
 * same floor(x*source_width/output_width) lookup as the previous renderer,
 * with no per-output-pixel 64-bit division. Jaguar gameplay is not involved. */
static inline void avp_pc_scale_nearest32(const uint32_t *src,unsigned sw,unsigned sh,
                                          uint32_t *dst,unsigned dw,unsigned dh)
{
    unsigned sy=0u,yrem=0u;
    unsigned ystep,yfrac,xstep,xfrac;
    if(!src||!dst||!sw||!sh||!dw||!dh)return;
    if(sw==dw&&sh==dh){
        memcpy(dst,src,(size_t)sw*(size_t)sh*sizeof(*src));
        return;
    }
    ystep=sh/dh;yfrac=sh%dh;xstep=sw/dw;xfrac=sw%dw;
    {
        /* The horizontal floor(x*sw/dw) samples are identical for every
         * source row. For normal-sized PC windows, generate the indices
         * once per scale call, avoiding a DDA for each distinct source row.
         * Very wide windows retain the original constant-memory path. */
        unsigned samples[4096];
        const int mapped=dw<=4096u && sw!=dw;
        unsigned prev_sy=~0u;
        const uint32_t *prev_out=NULL;
        if(mapped){
            unsigned sx=0u,xrem=0u;
            for(unsigned x=0u;x<dw;x++){
                samples[x]=sx;
                sx+=xstep;xrem+=xfrac;
                if(xrem>=dw){xrem-=dw;++sx;}
            }
        }
        /* Upscaling repeats each source row on several output rows: build
         * the row once and copy it, instead of re-gathering every pixel.
         * Output is bit-identical to the per-row DDA. */
        for(unsigned y=0u;y<dh;y++){
            uint32_t *out=dst+(size_t)y*dw;
            if(sy==prev_sy && prev_out){
                memcpy(out,prev_out,(size_t)dw*sizeof(*out));
            }else{
                const uint32_t *in=src+(size_t)sy*sw;
                if(sw==dw){
                    memcpy(out,in,(size_t)dw*sizeof(*out));
                }else if(mapped){
                    for(unsigned x=0u;x<dw;x++)out[x]=in[samples[x]];
                }else{
                    unsigned sx=0u,xrem=0u;
                    for(unsigned x=0u;x<dw;x++){
                        out[x]=in[sx];
                        sx+=xstep;xrem+=xfrac;
                        if(xrem>=dw){xrem-=dw;++sx;}
                    }
                }
            }
            prev_sy=sy;prev_out=out;
            sy+=ystep;yrem+=yfrac;
            if(yrem>=dh){yrem-=dh;++sy;}
        }
    }
}
#endif
