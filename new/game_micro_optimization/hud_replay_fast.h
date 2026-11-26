#ifndef AVP_MICRO_HUD_REPLAY_FAST_H
#define AVP_MICRO_HUD_REPLAY_FAST_H
#include "jaguar/avp_gpu/hud_rmw_host_composite.h"
#include "cry_rmw_fast.h"
#if defined(__SSE2__) && !defined(_WIN32) && \
    (defined(__clang__) || (defined(__linux__) && defined(__GNUC__)) || defined(AVP_MICRO_TEST_SPARSE_HUD))
#include <emmintrin.h>
#endif
/* Exact point-sampled HUD replay. Each worker owns complete native rows,
 * which map to disjoint output rows. Reuse a color only within one tile
 * with an immutable ink sequence; no state survives a tile or frame. */
typedef struct AvpMicroHudReplay {
    u32 *host,*mask;
    const u32 *before,*after,*rgb;
    const AvpHudRmwOps *ops;
    const u8 *q;
    size_t qsize;
    unsigned width,stride,host_stride,xe[321],ye[229];
} AvpMicroHudReplay;
/* Sample the centered native HUD directly from a pitched world framebuffer. */
static inline void avp_micro_hud_sample_strided(u32 *out,const u32 *src,
    unsigned width,unsigned height,unsigned pitch)
{
    unsigned host_x[320];
    for(unsigned x=0;x<320u;x++)host_x[x]=(unsigned)(((uint64_t)(2u*x+1u)*width)/640u);
    for(unsigned y=0;y<228u;y++){
        unsigned sy=(unsigned)(((uint64_t)(2u*y+1u)*height)/456u);
        const u32 *row=src+(size_t)sy*pitch;
        for(unsigned x=0;x<320u;x++)out[(size_t)y*320u+x]=row[host_x[x]];
    }
}
static inline void avp_micro_hud_replay_init(AvpMicroHudReplay *p,unsigned height)
{
    if(!p->host_stride)p->host_stride=p->width;
    for(unsigned x=0;x<=320u;x++)p->xe[x]=(unsigned)(((uint64_t)x*p->width+319u)/320u);
    for(unsigned y=0;y<=228u;y++)p->ye[y]=(unsigned)(((uint64_t)y*height+227u)/228u);
}
static inline u32 avp_micro_hud_inks(const AvpMicroHudReplay *p,u32 c,const AvpHudRmwOps *op)
{
    for(unsigned n=0;n<op->count;n++)c=avp_micro_cry_rmw(c,op->ink[n],p->q,p->qsize,p->rgb);
    return c;
}
static inline void avp_micro_hud_replay_rows(void *context,unsigned first,unsigned last)
{
    const AvpMicroHudReplay *p=(const AvpMicroHudReplay*)context;
    for(unsigned y=first;y<last;y++){
        const unsigned y0=p->ye[y],y1=p->ye[y+1u];
        if(y0==y1)continue;
        for(unsigned x=0;x<320u;x++){
            const size_t i=(size_t)y*320u+x;
#if defined(__SSE2__) && !defined(_WIN32) && \
    (defined(__clang__) || (defined(__linux__) && defined(__GNUC__)) || defined(AVP_MICRO_TEST_SPARSE_HUD))
            /* Four unchanged native tiles with no recorded ink cannot
             * touch the host surface or its protection mask. */
            if((x&3u)==0u && !(p->ops[i].count|p->ops[i+1u].count|
                                p->ops[i+2u].count|p->ops[i+3u].count)){
                const __m128i a=_mm_loadu_si128((const __m128i *)(p->before+i));
                const __m128i b=_mm_loadu_si128((const __m128i *)(p->after+i));
                if(_mm_movemask_epi8(_mm_cmpeq_epi32(a,b))==65535){x+=3u;continue;}
            }
#endif
            const AvpHudRmwOps *op=p->ops+i;
            const u32 before=p->before[i],after=p->after[i];
            if(!op->count && before==after)continue;
            const unsigned x0=p->xe[x],x1=p->xe[x+1u];
            if(x0==x1)continue;
            const int replay=op->count>0u&&op->count<=AVP_HUD_REPLAY_MAX_INKS&&
                avp_micro_hud_inks(p,before,op)==after;
            u32 last_in=before,last_out=after;
            for(unsigned yy=y0;yy<y1;yy++){
                u32 *row=p->host+(size_t)yy*p->host_stride;
                if(replay){
                    if(op->count==1u){
                        const u16 ink=op->ink[0];
                        for(unsigned xx=x0;xx<x1;xx++){
                            const u32 c=row[xx];
                            if(c!=last_in){last_in=c;last_out=avp_micro_cry_rmw(c,ink,p->q,p->qsize,p->rgb);}
                            row[xx]=last_out;
                        }
                    }else for(unsigned xx=x0;xx<x1;xx++){
                        const u32 c=row[xx];
                        if(c!=last_in){last_in=c;last_out=avp_micro_hud_inks(p,c,op);}
                        row[xx]=last_out;
                    }
                }else for(unsigned xx=x0;xx<x1;xx++)row[xx]=after;
                if(p->mask){u32 *mask=p->mask+(size_t)yy*p->stride;for(unsigned xx=x0;xx<x1;xx++)mask[xx]=1u;}
            }
        }
    }
}
#endif
