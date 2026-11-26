/* RE40 R197: real source FP texture blit with host-wide OP extension.
 * Synthetic palette/texels make the former 320-column crop measurable. */
#include "jaguar/avp_gpu/fp_wide_blit.h"
#include "jaguar/avp_gpu/fp_overlay.h"
#include "jaguar/avp_gpu/overlay_scale_composite.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned x_boundary(unsigned width,unsigned height,int native_x)
{
    unsigned c=avp_host_wide_center_width(height);
    return (width-c)/2u+(unsigned)avp_fp_wide_ceil_div((int64_t)native_x*c,320);
}
static void check(unsigned width,unsigned height)
{
    static const uint8_t pal[208][3]={{0,0,0},{180,32,50},{20,180,30},{15,19,230}};
    const size_t count=(size_t)width*height;
    uint32_t *p=malloc(count*sizeof(*p));
    uint8_t weapon[12]={1,1,1,1,0,1,1,1,1,1,1,1};
    uint8_t flash[6]={0x23,0x10,0x32,0x10,0x00,0x00};
    assert(p);
    for(size_t i=0;i<count;i++)p[i]=0x00090807u;
    /* 319..330: 11 of 12 pixels should be visible beyond the former
     * Jaguar x=320 clipping edge, even while the HUD stays centered. */
    avp_gpu_fp_wide_blit(p,width,height,weapon,sizeof weapon,12,1,pal,208,319,114,0);
    unsigned mid_y=(unsigned)avp_fp_wide_ceil_div(114LL*height,228);
    unsigned edge=x_boundary(width,height,320);
    assert(edge<width);
    assert(p[(size_t)mid_y*width+edge]==0x00b42032u);
    assert(p[(size_t)mid_y*width+edge-1u]==0x00b42032u);
    assert(p[(size_t)mid_y*width+edge+1u]==0x00b42032u);
    assert(p[(size_t)mid_y*width+x_boundary(width,height,323)]==0x00090807u);
    assert(p[(size_t)mid_y*width+x_boundary(width,height,326)]==0x00b42032u);
    /* Source palette has higher priority when USE flash is composited,
     * and 4-bpp index 0 must leave existing pixels unchanged. */
    avp_gpu_fp_wide_blit(p,width,height,flash,sizeof flash,4,3,pal,16,319,114,1);
    assert(p[(size_t)mid_y*width+x_boundary(width,height,319)]==0x0014b41eu);
    assert(p[(size_t)mid_y*width+x_boundary(width,height,320)]==0x000f13e6u);
    assert(p[(size_t)mid_y*width+x_boundary(width,height,321)]==0x00b42032u);
    assert(p[(size_t)mid_y*width+x_boundary(width,height,322)]==0x00b42032u);
    /* And the left side must extend beyond the left HUD edge too. */
    uint8_t left=2;
    avp_gpu_fp_wide_blit(p,width,height,&left,1,1,1,pal,208,-1,114,0);
    assert(p[(size_t)mid_y*width+x_boundary(width,height,0)-1u]==0x0014b41eu);
    /* Never write past the REAL output frame, and reject corrupt textures. */
    uint32_t before=p[(size_t)mid_y*width+x_boundary(width,height,330)];
    avp_gpu_fp_wide_blit(p,width,height,weapon,2,12,1,pal,208,319,114,0);
    assert(p[(size_t)mid_y*width+x_boundary(width,height,330)]==before);
    free(p);
}
static void check_legacy_center_parity(unsigned width,unsigned height)
{
    /* Within the old centered 320x228 picture this raster must be EXACTLY
     * the prior native scratch + nearest-neighbor composite, pixel for pixel.
     * Only the new side columns are allowed to contain additional OP art. */
    static const uint8_t pal[208][3]={{0,0,0},{18,72,121}};
    uint8_t weapon[16];
    for(unsigned i=0;i<16u;i++)weapon[i]=1;
    unsigned cw=avp_host_wide_center_width(height),left=(width-cw)/2u;
    const size_t pixels=(size_t)width*height,cn=(size_t)cw*height;
    const size_t native_n=320u*228u;
    uint32_t *new_output=malloc(pixels*sizeof(uint32_t));
    uint32_t *old_output=malloc(cn*sizeof(uint32_t));
    uint32_t *before=malloc(native_n*sizeof(uint32_t));
    uint32_t *after=malloc(native_n*sizeof(uint32_t));
    assert(new_output&&old_output&&before&&after);
    for(size_t i=0;i<pixels;i++)new_output[i]=0x00090807u;
    for(size_t i=0;i<cn;i++)old_output[i]=0x00090807u;
    for(size_t i=0;i<native_n;i++)before[i]=after[i]=0x00090807u;
    for(int sx=0;sx<8;sx++)after[114u*320u+(312u+(unsigned)sx)]=0x00124879u;
    avp_gpu_overlay_scaled_composite(old_output,cw,height,before,after);
    avp_gpu_fp_wide_blit(new_output,width,height,weapon,sizeof weapon,16,1,
                         pal,208,312,114,0);
    for(unsigned y=0;y<height;y++)
        for(unsigned x=0;x<cw;x++)
            assert(new_output[(size_t)y*width+left+x]==old_output[(size_t)y*cw+x]);
    assert(new_output[(size_t)(height/2u)*width+left+cw]!=0x00090807u);
    free(new_output);free(old_output);free(before);free(after);
}
static void check_edges(void)
{
    uint32_t p[1920];uint8_t sprite[4]={1,1,1,1};
    static const uint8_t pal[208][3]={{0,0,0},{3,4,5}};
    for(unsigned i=0;i<1920;i++)p[i]=0x00aabbccu;
    /* Host 640x3 is much wider than 4:3: full output ends before the sprite. */
    avp_gpu_fp_wide_blit(p,640,3,sprite,4,4,1,pal,208,100000,0,0);
    for(unsigned i=0;i<1920;i++)assert(p[i]==0x00aabbccu);
}
int main(void)
{
    /* Retail MAZESCRN.S weapon offsets genuinely place some pixels beyond
     * native x=319.  The former 320-wide crop lost those pixels on 16:9. */
    AvpFpOverlayDesc combi,punch;
    assert(avp_gpu_fp_overlay_desc(18,&combi)); /* fourth Combi-stick pose */
    assert(avp_gpu_fp_overlay_desc(23,&punch)); /* Predator punch pose */
    assert(160+combi.x_offset+combi.width>320);
    assert(160+punch.x_offset+punch.width>320);
    check(1600,900);check(1920,1080);check(2560,1440);check(3440,1440);
    check_legacy_center_parity(1600,900);
    check_legacy_center_parity(1920,1080);
    check_edges();
    puts("R197 host-widescreen weapon/USE extend beyond centered 4:3 HUD; original positions, texture clipping and output bounds PASS");
    return 0;
}
