#include "jaguar/cry.h"
#include <stdint.h>

/* Hardware color modifier tables from Atari's Jaguar TOM Software Reference
 * Manual.  Blue is the red table mirrored on the C axis.  These values were
 * previously buried in the PC M20 renderer; moving them here changes ownership,
 * not behavior. */
static const u8 k_red[16][16] = {
 {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
 {34,34,34,34,34,34,34,34,34,34,34,34,34,34,19,0},
 {68,68,68,68,68,68,68,68,68,68,68,68,64,43,21,0},
 {102,102,102,102,102,102,102,102,102,102,102,95,71,47,23,0},
 {135,135,135,135,135,135,135,135,135,135,130,104,78,52,26,0},
 {169,169,169,169,169,169,169,169,169,170,141,113,85,56,28,0},
 {203,203,203,203,203,203,203,203,203,183,153,122,91,61,30,0},
 {237,237,237,237,237,237,237,237,230,197,164,131,98,65,32,0},
 {255,255,255,255,255,255,255,255,247,214,181,148,115,82,49,17},
 {255,255,255,255,255,255,255,255,255,235,204,173,143,112,81,51},
 {255,255,255,255,255,255,255,255,255,255,227,198,170,141,113,85},
 {255,255,255,255,255,255,255,255,255,255,249,223,197,171,145,119},
 {255,255,255,255,255,255,255,255,255,255,255,248,224,200,177,153},
 {255,255,255,255,255,255,255,255,255,255,255,255,252,230,208,187},
 {255,255,255,255,255,255,255,255,255,255,255,255,255,255,240,221},
 {255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255}
};
static const u8 k_green[16][16] = {
 {0,17,34,51,68,85,102,119,136,153,170,187,204,221,238,255},
 {0,19,38,57,77,96,115,134,154,173,192,211,231,250,255,255},
 {0,21,43,64,86,107,129,150,172,193,215,236,255,255,255,255},
 {0,23,47,71,95,119,142,166,190,214,238,255,255,255,255,255},
 {0,26,52,78,104,130,156,182,208,234,255,255,255,255,255,255},
 {0,28,56,85,113,141,170,198,226,255,255,255,255,255,255,255},
 {0,30,61,91,122,153,183,214,244,255,255,255,255,255,255,255},
 {0,32,65,98,131,164,197,230,255,255,255,255,255,255,255,255},
 {0,32,65,98,131,164,197,230,255,255,255,255,255,255,255,255},
 {0,30,61,91,122,153,183,214,244,255,255,255,255,255,255,255},
 {0,28,56,85,113,141,170,198,226,255,255,255,255,255,255,255},
 {0,26,52,78,104,130,156,182,208,234,255,255,255,255,255,255},
 {0,23,47,71,95,119,142,166,190,214,238,255,255,255,255,255},
 {0,21,43,64,86,107,129,150,172,193,215,236,255,255,255,255},
 {0,19,38,57,77,96,115,134,154,173,192,211,231,250,255,255},
 {0,17,34,51,68,85,102,119,136,153,170,187,204,221,238,255}
};

static int clamp_i(int v,int lo,int hi)
{
    return v<lo?lo:(v>hi?hi:v);
}

void avp_jaguar_cry16_to_rgb(u16 cry,u8 *r,u8 *g,u8 *b)
{
    u8 c=(u8)((cry>>12)&0x0fu);
    u8 ri=(u8)((cry>>8)&0x0fu);
    u8 y=(u8)(cry&0xffu);
    if(r)*r=(u8)(((u32)k_red[c][ri]*y)>>8);
    if(g)*g=(u8)(((u32)k_green[c][ri]*y)>>8);
    if(b)*b=(u8)(((u32)k_red[15u-c][ri]*y)>>8);
}

u16 avp_jaguar_rgb_to_cry_qconv(u8 r,u8 g,u8 b,const u8 *qconv,size_t qconv_size)
{
    u8 y,c=0;
    u32 scale;
    if(!qconv||qconv_size<131072u)return 0;
    y=r>g?(r>b?r:b):(g>b?g:b);
    if(y==0u)return 0;
    scale=0xff00u/(u32)y;
    if(r==y){
        u8 nb=(u8)(((u32)b*scale)>>8);
        u8 ng=(u8)((((u32)g*scale)&0xff00u)>>8);
        c=qconv[((u32)ng<<8)|nb];
    }else if(g==y){
        u8 nr=(u8)(((u32)r*scale)>>8);
        u8 nb=(u8)((((u32)b*scale)&0xff00u)>>8);
        c=qconv[0x10000u|((u32)nb<<8)|nr];
    }else{
        u8 nr=(u8)(((u32)r*scale)>>8);
        u8 ng=(u8)((((u32)g*scale)&0xff00u)>>8);
        c=(u8)(qconv[((u32)ng<<8)|nr]^0xf0u);
    }
    return (u16)(((u16)c<<8)|y);
}

u16 avp_jaguar_srcshade_cry(u16 src,int intensity_delta)
{
    int y=(int)(src&0xffu)+intensity_delta;
    y=clamp_i(y,0,255);
    return (u16)((src&0xff00u)|(u16)y);
}

u16 avp_jaguar_rmw_blend_cry(u16 dst,u16 src)
{
    int y=(int)(dst&0xffu)+(int)(int8_t)(src&0xffu);
    int c1=(int)((dst>>8)&15u);
    int c2=(int)((dst>>12)&15u);
    int sh=(int)((src>>8)&0xffu);
    int dc1=(int)(int8_t)((u8)(sh<<4))>>4;
    int dc2=(int)(int8_t)((u8)(sh&0xf0))>>4;
    y=clamp_i(y,0,255);
    c1=clamp_i(c1+dc1,0,15);
    c2=clamp_i(c2+dc2,0,15);
    return (u16)((c2<<12)|(c1<<8)|y);
}

void avp_jaguar_build_hud_rmw_palette(const u16 source_palette[32],int brightness,u16 out[32])
{
    unsigned i;
    if(!source_palette||!out)return;
    out[0]=0;
    if(brightness<=-96){
        for(i=1;i<32u;i++)out[i]=0;
        return;
    }
    for(i=1;i<32u;i++){
        u16 q=(u16)(source_palette[i]^0x8880u);
        int dy=(int)(int8_t)(q&0xffu)+brightness;
        dy=clamp_i(dy,0,127);
        out[i]=(u16)((q&0xff00u)|(u16)dy);
    }
}

u32 avp_jaguar_rmw_xrgb8888(u32 dst,u16 src,const u8 *qconv,size_t qconv_size)
{
    u8 r=(u8)((dst>>16)&255u),g=(u8)((dst>>8)&255u),b=(u8)(dst&255u);
    u8 rr,gg,bb;
    u16 dcry;
    if(src==0u)return dst;
    if(!qconv||qconv_size<131072u)return dst;
    dcry=avp_jaguar_rgb_to_cry_qconv(r,g,b,qconv,qconv_size);
    dcry=avp_jaguar_rmw_blend_cry(dcry,src);
    avp_jaguar_cry16_to_rgb(dcry,&rr,&gg,&bb);
    return ((u32)rr<<16)|((u32)gg<<8)|bb;
}

/* Optional PC HUD-only color stability. Retail HUD ink is a relative
 * CRY RMW modifier, so a black 3D texture gives it a very different hue from
 * a wall of another color. Keep the original brightness and signed HUD ink
 * modifier, but choose the same neutral CRY starting chroma for every pixel.
 * All other rendering continues to call the unmodified retail adapter. */
u32 avp_jaguar_rmw_fixed_hue_xrgb8888(u32 dst,u16 src,
                                        const u8 *qconv,size_t qconv_size)
{
    u8 r=(u8)((dst>>16)&255u),g=(u8)((dst>>8)&255u),b=(u8)(dst&255u);
    u8 rr,gg,bb,y;
    u16 neutral,base,blended;
    if(src==0u || !qconv || qconv_size<131072u)return dst;
    y=r>g?(r>b?r:b):(g>b?g:b);
    /* The actual loaded Jaguar QCONV table selects the neutral reference
     * hue. The destination intensity still comes from the real scene. */
    neutral=avp_jaguar_rgb_to_cry_qconv(128u,128u,128u,qconv,qconv_size);
    base=(u16)((neutral&0xff00u)|y);
    blended=avp_jaguar_rmw_blend_cry(base,src);
    avp_jaguar_cry16_to_rgb(blended,&rr,&gg,&bb);
    return ((u32)rr<<16)|((u32)gg<<8)|bb;
}

/* Original 68000 MazeList X_FADE ordering: adjust the low intensity byte
 * with saturation before XORing the Predator mode modifier. */
u16 avp_jaguar_game_clut(u16 fade_color,u16 extra_color,s16 fade_level)
{
    int intensity=(int)(fade_color&255u)-(int)fade_level;
    if(intensity<0)intensity=0;
    if(intensity>255)intensity=255;
    return (u16)(((fade_color&0xff00u)|(u16)intensity)^extra_color);
}

/* Jaguar TOM video output converts the *same* 16-bit line-buffer words
 * differently depending on VMODE[2:1]. Do not reinterpret the X_FADE
 * modifier as an arbitrary host RGB filter. */
u32 avp_jaguar_vmode_rgb16(u16 word)
{
    /* TRM Rev.8 p.23: "Bits 0-5 form the six most significant bits of green,
     * bits 6-10 form the five most significant bits of blue and bits 11-15
     * form the five most significant bits of red. All other bits are set to
     * zero."  (RE24-S10: was r*255/31 scaling, not the documented output.) */
    u32 r=(u32)((word>>11)&31u);
    u32 b=(u32)((word>>6)&31u);
    u32 g=(u32)(word&63u);
    return ((r<<3)<<16)|((g<<2)<<8)|(b<<3);
}

u32 avp_jaguar_vmode_rgb24(u16 first,u16 second)
{
    /* RGB24 combines successive 16-bit line-buffer entries: high word
     * carries GREEN then RED; the low word carries unused then BLUE. */
    return ((u32)(first&255u)<<16)|((u32)((first>>8)&255u)<<8)|
           (u32)(second&255u);
}
