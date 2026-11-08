/* Original 68000 32-bit SUB/NEG/ASR/ADD behaviour, including sign limits.
 * Self-contained so the static rough_dist in COLLIDE.S can be exercised. */
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include "../src/game/collision.c"

AvpAmp test_amps[AVP_NUM_AMPS], *amp_data=test_amps,*amps_at=test_amps;
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
u8 objmap[AVP_AMP_OBJ_ROW_BYTES*AVP_AMP_OBJ_H];
u8 fullbits[256];

static uint64_t hash=UINT64_C(14695981039346656037);
static void mix(uint32_t v)
{
    for(unsigned i=0;i<4;i++){hash^=(v>>(i*8))&255u;hash*=UINT64_C(1099511628211);}
}

/* Independent reference based on 64-bit arithmetic, followed by explicit
 * modulo 32-bit and sign decode (no signed overflow or implementation-defined
 * signed negative shifts). */
static int32_t ref32(int64_t val)
{
    uint32_t bits=(uint32_t)(uint64_t)val;
    return bits<=INT32_MAX?(int32_t)bits:(int32_t)((int64_t)bits-INT64_C(4294967296));
}
static int32_t reference_distance(int32_t ax,int32_t ay,int32_t bx,int32_t by)
{
    int32_t dx=ref32((int64_t)ax-bx),dy=ref32((int64_t)ay-by);
    if(dx<0)dx=ref32(-(int64_t)dx);
    if(dy<0)dy=ref32(-(int64_t)dy);
    if(dx>dy){int32_t t=dx;dx=dy;dy=t;}
    int64_t q=(dx>=0)?(int64_t)dx/4:-((-(int64_t)dx+3)/4);
    return ref32((int64_t)dy+q);
}
static int check_cases(const int32_t *v,unsigned n,uint64_t expected,const char *label)
{
    uint32_t count=0;
    hash=UINT64_C(14695981039346656037);
    for(unsigned a=0;a<n;a++)for(unsigned b=0;b<n;b++)
    for(unsigned c=0;c<n;c++)for(unsigned d=0;d<n;d++) {
        int32_t got=rough_dist(v[a],v[b],v[c],v[d]);
        int32_t want=reference_distance(v[a],v[b],v[c],v[d]);
        if(got!=want){fprintf(stderr,"%s mismatch case %u\n",label,count);return 1;}
        mix((uint32_t)got);++count;
    }
    if(hash!=expected){fprintf(stderr,"%s reference hash differs %016llx\n",label,(unsigned long long)hash);return 1;}
    printf("%s: %u 68000-wrap cases hash %016llx\n",label,count,(unsigned long long)hash);
    return 0;
}
int main(void)
{
    static const int32_t normal[]={-0x7f0000,-0x120000,-0x30000,-0x10000,-1,0,1,0x10000,0x30000,0x120000,0x7f0000};
    static const int32_t boundary[]={INT32_MIN,INT32_MIN+1,-0x7f0000,-1,0,1,0x7f0000,INT32_MAX-1,INT32_MAX};
    if(check_cases(normal,11u,UINT64_C(0x42034355295e0235),"ordinary"))return 1;
    if(check_cases(boundary,9u,UINT64_C(0xc9fab6bb50775e61),"boundary"))return 1;
    return 0;
}
