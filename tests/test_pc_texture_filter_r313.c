/* R313: a wall column's U-axis is invariant. Its hoisted opaque CRY16
 * bilinear/AF sampler must match R312's per-pixel sampler exactly. */
#include "jaguar/avp_gpu/texture_filter.h"
#include "jaguar/cry.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <math.h>
u16 avp_jaguar_rgb_to_cry_qconv(u8 r,u8 g,u8 b,const u8 *q,size_t n)
{(void)q;(void)n;return (u16)(((unsigned)r<<8u)|((unsigned)g ^ b));}
static uint32_t seed=0xbabec0deu;
static uint32_t rnd(void){seed^=seed<<13u;seed^=seed>>17u;seed^=seed<<5u;return seed;}
static double fract(void){return (double)(rnd()&0xffffffu)/16777215.0;}
int main(void)
{
    uint32_t *map=malloc(65536u*sizeof(*map));
    assert(map);
    for(unsigned i=0;i<65536u;i++)map[i]=(rnd()&0xffu)|((rnd()&0xffu)<<8u)|((rnd()&0xffu)<<16u);
    const unsigned dims[][2]={{64,64},{127,239},{1254,1254},{128,128}};
    const int shades[]={-512,-200,-27,0,18,255,512};
    unsigned long long equal=0,fallthrough=0;
    for(unsigned dim=0;dim<sizeof(dims)/sizeof(dims[0]);dim++){
        const unsigned w=dims[dim][0],h=dims[dim][1];
        const size_t count=(size_t)w*h;
        uint8_t *rgb=malloc(count*3u);
        uint16_t *cry=malloc(count*sizeof(*cry));
        assert(rgb&&cry);
        for(size_t i=0;i<count;i++){
            rgb[i*3u]=(uint8_t)rnd();rgb[i*3u+1]=(uint8_t)rnd();rgb[i*3u+2]=(uint8_t)rnd();
            cry[i]=(uint16_t)rnd();
        }
        AvpGpuRgbImage s={0};s.rgb=rgb;s.cry16=cry;s.width=(int)w;s.height=(int)h;
        for(unsigned level=0;level<3;level++){
            AvpFilterImage mip={0};
            /* Explicit test mip CRY and dimensions, no costly production
             * chain conversion required for this sampling parity fixture. */
            if(level){
                mip.levels=level+1u;
                mip.mip[level].width=w>>level;
                mip.mip[level].height=h>>level;
                mip.mip[level].cry=cry;
            }
            for(unsigned sh=0;sh<sizeof(shades)/sizeof(shades[0]);sh++){
                uint8_t lut[256];avp_filter_make_shade_lut(lut,shades[sh]);
                for(unsigned taps=1;taps<=16;taps*=2u)
                for(unsigned col=0;col<96u;col++){
                    AvpFilterPlan p={level,taps,1u,(fract()-0.5)*1.4,(fract()-0.5)*1.4};
                    const double u=(fract()-0.5)*4.0;
                    AvpFilterCryWallColumn column;
                    assert(avp_filter_cry_wall_column_prepare(&column,&s,level?&mip:NULL,u,p,0,map,lut));
                    for(unsigned row=0;row<40;row++){
                        const double v=(fract()-0.5)*4.0;
                        unsigned alpha=0;
                        uint32_t reference=avp_filter_sample_prepared_shaded(&s,level?&mip:NULL,
                            u,v,p,0,shades[sh],map,&alpha,lut);
                        uint32_t fast=avp_filter_cry_wall_column_sample(&column,v);
                        if(fast!=reference||alpha!=255){
                            fprintf(stderr,"R313 failed size %ux%u level %u shade %d taps %u col %u row %u UV %.17g/%.17g orig %08x fast %08x\n",
                                w,h,level,shades[sh],taps,col,row,u,v,reference,fast);
                            return 1;
                        }
                        equal++;
                    }
                    /* Wrap, missing CRY, transparency and non-bilinear
                     * must be routed through the existing generic path. */
                    assert(!avp_filter_cry_wall_column_prepare(&column,&s,level?&mip:NULL,u,p,1,map,lut));fallthrough++;
                    AvpFilterPlan nearest=p;nearest.bilinear=0;
                    assert(!avp_filter_cry_wall_column_prepare(&column,&s,level?&mip:NULL,u,nearest,0,map,lut));fallthrough++;
                }
            }
            /* mip with borrowed CRY ownership in fixture - don't destroy. */
        }
        free(rgb);free(cry);
    }
    free(map);
    printf("R313: %llu pixel-exact wall column comparisons, %llu fallback assertions PASS\n",equal,fallthrough);
    return 0;
}
