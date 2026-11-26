#ifndef AVP_MICRO_MAZE_INTERP_FAST_H
#define AVP_MICRO_MAZE_INTERP_FAST_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* Unchanged door pairs need no interpolation. All other bytes are copied
 * exactly and changed pairs use the same arithmetic as the existing path. */
static inline void avp_micro_maze_interp(uint8_t *dst,const uint8_t *cur,
    const uint8_t *prev,size_t bytes,double alpha)
{
    memcpy(dst,cur,bytes);
    for(size_t o=0;o+8u<=bytes;o+=8u){
        uint16_t a,b;
        memcpy(&a,cur+o+6u,sizeof a);memcpy(&b,prev+o+6u,sizeof b);
        if(a==b)continue;
        for(unsigned k=6u;k<8u;k++){
            double q=(double)prev[o+k]+((double)cur[o+k]-(double)prev[o+k])*alpha;
            if(q<0.0)q=0.0;
            if(q>255.0)q=255.0;
            dst[o+k]=(uint8_t)(q+0.5);
        }
    }
}
#endif
