#include "jaguar/avp_gpu/frontend_fade.h"

u16 avp_gpu_rgb16_pack(u8 r,u8 g,u8 b)
{
    return (u16)((((u16)r >> 3) << 11) |
                 (((u16)b >> 3) << 6) |
                 ((u16)g >> 2));
}

void avp_gpu_rgb16_unpack(u16 pixel,u8 *r,u8 *g,u8 *b)
{
    /* TOM's RGB16 DAC path consumes the stored MSBs; it does not replicate
       low bits.  Keep the hosted conversion equally literal. */
    if(r)*r=(u8)(((pixel>>11)&31u)<<3);
    if(b)*b=(u8)(((pixel>>6)&31u)<<3);
    if(g)*g=(u8)((pixel&63u)<<2);
}

u16 avp_gpu_rgb16_fraction(u16 pixel,unsigned phase)
{
    unsigned v=(unsigned)pixel;
    unsigned out=0u;
    if(phase>=16u)return pixel;
    if(phase&8u)out+=(v>>1)&0x7bdfu;
    if(phase&4u)out+=(v>>2)&0x39cfu;
    if(phase&2u)out+=(v>>3)&0x18c7u;
    if(phase&1u)out+=(v>>4)&0x0843u;
    return (u16)out;
}

u16 avp_gpu_rgb16_title_menu_dim(u16 pixel,unsigned passes)
{
    static const unsigned char pass_phase[3]={15u,13u,11u};
    for(unsigned i=0u;i<passes&&i<3u;i++)pixel=avp_gpu_rgb16_fraction(pixel,pass_phase[i]);
    return pixel;
}

/* Reference-machine timing: the three passes start about 4-5 fields apart and
 * each sweeps the bitmap top to bottom over about two fields; the PC applies a
 * pass whole once it would have completed (elapsed 2, 6, 10 fields). */
unsigned avp_gpu_title_menu_dim_passes(unsigned elapsed_vbls)
{
    return elapsed_vbls<2u?0u:(elapsed_vbls<6u?1u:(elapsed_vbls<10u?2u:3u));
}

u16 avp_gpu_rgb16_cross(u16 from,u16 to,unsigned phase)
{
    unsigned a=(unsigned)from;
    unsigned fa=(unsigned)avp_gpu_rgb16_fraction(from,phase);
    unsigned fb=(unsigned)avp_gpu_rgb16_fraction(to,phase);
    return (u16)(a-fa+fb);
}

/* Retail CROSS GPU stage kernels ($F03170..$F0342A, dispatched by $7FB4).
 * Each stage subtracts and adds masked right-shifts of the two packed RGB16
 * words (R5 B5 G6); stages 7 and 10..16 take the incoming word as base.
 * This is NOT the proportional avp_gpu_rgb16_cross(): truncation differs
 * (e.g. stages 10, 12, 14).  The GPU works on two pixels per 32-bit word
 * with per-half masks; base-floor(base>>s) is monotone and never borrows
 * or carries across a channel, so one 16-bit evaluation is exact. */
u16 avp_gpu_rgb16_scene_cross(u16 old_px,u16 new_px,unsigned stage)
{
    static const unsigned char k[17][4]={ /* base_is_new, terms, shift a, b */
        {0,0,0,0},{0,1,4,0},{0,1,3,0},{0,2,3,4},
        {0,1,2,0},{0,2,2,4},{0,2,2,3},{1,2,1,4},
        {0,1,1,0},{0,2,1,4},{1,2,2,3},{1,2,2,4},
        {1,1,2,0},{1,2,3,4},{1,1,3,0},{1,1,4,0},
        {1,0,0,0}
    };
    static const u16 mask[5]={0u,0x7BDFu,0x39CFu,0x18C7u,0x0843u};
    unsigned base,other,out;
    if(stage>16u)stage=16u;
    base=k[stage][0]?new_px:old_px;
    other=k[stage][0]?old_px:new_px;
    out=base;
    for(unsigned i=0u;i<k[stage][1];i++){
        const unsigned sh=k[stage][2u+i];
        out-=(base>>sh)&mask[sh];
        out+=(other>>sh)&mask[sh];
    }
    return (u16)out;
}

int avp_gpu_crossx_seed(unsigned line)
{
    static const signed char seed[80]={
        -15,-15,-15,-15,-14,-14,-13,-13,-12,-11,-11,-10,-9,-8,-8,-9,
        -10,-10,-9,-8,-8,-7,-7,-6,-5,-6,-5,-6,-7,-6,-5,-4,
        -3,-4,-3,-3,-2,-3,-4,-3,-2,-1,-1,-2,-3,-2,-3,-4,
        -4,-5,-6,-6,-8,-7,-6,-7,-7,-8,-9,-10,-9,-9,-10,-11,
        -12,-11,-11,-10,-10,-11,-10,-10,-11,-12,-13,-14,-15,-15,-15,-15
    };
    return line<80u?(int)seed[line]:0;
}

unsigned avp_gpu_crossx_dispatch_phase(int accumulator)
{
    int p=accumulator;
    /* Exact transform at CROSSX GPU $F030E0..$F03120. */
    if(p<8){
        if(p<0)p=0;
    }else if(p<16){
        p=15-p;
    }else{
        p-=16;
        if(p>=17)p=16;
    }
    return (unsigned)p;
}
