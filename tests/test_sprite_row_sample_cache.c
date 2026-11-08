/* R135: actual R134 sprite drawer versus cached vertical nearest-neighbour intervals.
 * Source R134 routine copied verbatim (function renamed only), using synthetic assets. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Verbatim source from R132, with function name changed for parity tests. */
static void r134_draw_amp_sprite(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,const AvpAmp *a,double px,double py,double fx,double fy,double rx,double ry,double focal,const double *zbuf,const AvpMaze2Occlusion *occlusion)
{
    PcSpriteChoice ch=sprite_choice(a,px,py); const u8 *d,*fr; size_t n,fo,coltab; unsigned frames,fi,ncols,ci; int left,right,use_occlusion; double wx,wy,forward,side,cx,scale;
    if(ch.aaf<0||ch.aaf>=63||!state->aaf[ch.aaf])return;
    d=state->aaf[ch.aaf];n=state->aaf_size[ch.aaf];if(n<4)return;frames=pc_be16(d);if(!frames||4u+(size_t)frames*4u>n)return;
    fi=(unsigned)a->animframe%frames;fo=pc_be32(d+4u+(size_t)fi*4u);if(fo+2u>n)return;fr=d+fo;left=(int)(int8_t)fr[0];right=(int)(int8_t)fr[1];if(right<left)return;ncols=(unsigned)(right-left);coltab=fo+2u;if(coltab+(size_t)ncols*2u>n)return;
    wx=(double)a->xpos/65536.0-px;wy=(double)a->ypos/65536.0-py;forward=wx*fx+wy*fy;if(forward<=0.04||forward>=8.0)return;side=wx*rx+wy*ry;cx=(double)fb->width*0.5+(side/forward)*focal;scale=((double)fb->height/228.0)/forward*((double)sprite_rescale/128.0);
    if(scale<0.03)return;
    use_occlusion=occlusion&&occlusion->depth&&occlusion->width==fb->width&&occlusion->height==fb->height;
    /* Sample exactly one source AAF texel at each destination pixel centre.
     * The old source-texel rectangles overlap not only when minified but also
     * with NON-INTEGER magnification.  A transparent texel cannot overwrite
     * the adjacent opaque texel's overshoot, leaving the impact/napalm fringe
     * streaks seen at close range.  Use the same inverse-projection sampling
     * for all positive scales (with the original zero-color transparency). */
    for(ci=0;ci<ncols;ci++){
        unsigned sci=ch.mirror?(ncols-1u-ci):ci;size_t rel=pc_be16(d+coltab+(size_t)sci*2u),co=fo+rel;int top,bottom,npix,syi,x0,x1,sx;
        /* AAF column offsets are frame-relative.  Zero marks an EMPTY
         * column, not a reference back to the frame header.  The latter
         * interpretation renders the column-offset table as CRY16 pixels:
         * the original cartridge contains zero offsets in, among others,
         * the impact puff (AAF 47 frame 3) and flame (AAF 39 frame 5).
         * Never convert those metadata words to blue/purple pixels. */
        if(!avp_gpu_aaf_column_offset_valid(rel,ncols)||co+2u>n)continue;
        top=(int)(int8_t)d[co];bottom=(int)(int8_t)d[co+1u];npix=bottom-top;
        if(npix<=0||co+2u+(size_t)npix*2u>n)continue;
        x0=(int)floor(cx+((double)left+(double)ci)*scale);x1=(int)ceil(cx+((double)left+(double)ci+1.0)*scale);if(x1<=x0)x1=x0+1;
        if(!avp_gpu_sprite_clip_span(&x0,&x1,(int)fb->width))continue;
        /* A single AAF source column projects to many host screen columns at
         * high resolutions. Its vertical spans and decoded CRY colors do not
         * depend on destination X. Compute these once for this column, while
         * leaving the original visibility/depth tests inside the pixel loop. */
        struct { int y0,y1; u32 color; int active; } source_rows[256];
        for(syi=0;syi<npix;syi++){
            int y0=(int)floor((double)fb->height*0.5+((double)top+(double)syi+(double)a->yoffset)*scale);
            int y1=(int)ceil((double)fb->height*0.5+((double)top+(double)syi+1.0+(double)a->yoffset)*scale);
            u16 cry=pc_be16(d+co+2u+(size_t)syi*2u);
            u8 rr,gg,bb;
            source_rows[syi].active=0;
            if(cry==0)continue;
            avp_jaguar_cry16_to_rgb(cry,&rr,&gg,&bb);
            if(y1<=y0)y1=y0+1;
            if(!avp_gpu_sprite_clip_span(&y0,&y1,(int)fb->height))continue;
            source_rows[syi].y0=y0;
            source_rows[syi].y1=y1;
            source_rows[syi].color=rgb(rr,gg,bb);
            source_rows[syi].active=1;
        }
        for(sx=x0;sx<x1;sx++){
            if(avp_gpu_sprite_sample_coord(sx,cx,scale)!=(int)left+(int)ci)continue;
            /* A closed, full-height boundary terminates the ray even when
               the pixel-wise window occlusion mask is available.  The old
               either/or check skipped the solid-boundary depth test whenever
               that mask was allocated, letting parts of distant AMPs appear
               past an occluding wall (notably outside its projected span).
               A framed opening remains traversable: cast_ray_hit only puts
               the next opaque hit into zbuf, while the mask clips the frame. */
            if(zbuf&&forward>=zbuf[sx])continue;
            for(syi=0;syi<npix;syi++){
                int yy;
                if(!source_rows[syi].active)continue;
                for(yy=source_rows[syi].y0;yy<source_rows[syi].y1;yy++){
                    if(avp_gpu_sprite_sample_coord(yy,(double)fb->height*0.5+(double)a->yoffset*scale,scale)!=top+syi)continue;
                    if(use_occlusion&&avp_gpu_maze2_occlusion_blocks(occlusion,sx,yy,(float)forward))continue;
                    fb->pixels[(size_t)yy*fb->width+(unsigned)sx]=source_rows[syi].color;
                }
            }
        }
    }
}


/* In the real game this is maintained by the original maze module. */
s16 sprite_rescale=128;

static void put16(u8 *p,unsigned v){p[0]=(u8)(v>>8);p[1]=(u8)v;}
static void put32(u8 *p,unsigned v){p[0]=(u8)(v>>24);p[1]=(u8)(v>>16);p[2]=(u8)(v>>8);p[3]=(u8)v;}
/* One-frame, 8-column synthetic AAF image with selected empty columns. */
static size_t make_aaf(u8 *out,size_t capacity,unsigned variant)
{
    const unsigned cols=8u, frame=8u, head=2u+2u*cols;
    unsigned used=frame+head;
    assert(capacity>=frame+head+8u*2u*(60u+1u));
    memset(out,0,capacity);
    put16(out,1);put32(out+4,frame);
    out[frame]= (u8)(int8_t)-4;
    out[frame+1u]=4u;
    for(unsigned c=0;c<cols;c++){
        if(c==(variant%8u))continue;
        put16(out+frame+2u+2u*c,used-frame);
        out[used]=(u8)(int8_t)-30;
        out[used+1u]=30u;
        for(unsigned sy=0;sy<60u;sy++){
            unsigned v=(c*3277u+sy*719u+variant*131u)&0xffffu;
            if((c+sy+variant)%7u==0u)v=0;
            if((c+sy+variant)%29u==0u)v=1;
            put16(out+used+2u+2u*sy,v);
        }
        used+=122u;
    }
    return used;
}

/* Also prove that the two trimmed edges contain exactly the same
 * destination pixels as R134's per-pixel inverse sample. The latter is
 * monotonic at every positive scale, including minification and clipping. */
static void verify_vertical_bounds(void)
{
    static const int heights[]={1,7,228,480,1080,2160,3840};
    static const double scales[]={0.03,0.07,0.35,0.75,1.0,1.4,3.0,7.0,24.0,96.0,384.0,1024.0};
    static const int offsets[]={-128,0,99};
    static const int tops[]={-128,-1,0,127};
    static const int source_rows[]={0,1,64,255};
    unsigned long long checks=0;
    for(unsigned hi=0;hi<sizeof(heights)/sizeof(heights[0]);hi++)
    for(unsigned sc=0;sc<sizeof(scales)/sizeof(scales[0]);sc++)
    for(unsigned oi=0;oi<sizeof(offsets)/sizeof(offsets[0]);oi++)
    for(unsigned ti=0;ti<sizeof(tops)/sizeof(tops[0]);ti++)
    for(unsigned ri=0;ri<sizeof(source_rows)/sizeof(source_rows[0]);ri++){
        const int height=heights[hi],top=tops[ti],row=source_rows[ri];
        const double scale=scales[sc];
        const int offset=offsets[oi];
        const double origin=(double)height*0.5+(double)offset*scale;
        int first=(int)floor((double)height*0.5+((double)top+(double)row+(double)offset)*scale);
        int last=(int)ceil((double)height*0.5+((double)top+(double)row+1.0+(double)offset)*scale);
        if(last<=first)last=first+1;
        if(!avp_gpu_sprite_clip_span(&first,&last,height))continue;
        int y0=first,y1=last;
        while(y0<y1 && avp_gpu_sprite_sample_coord(y0,origin,scale)!=top+row)++y0;
        while(y1>y0 && avp_gpu_sprite_sample_coord(y1-1,origin,scale)!=top+row)--y1;
        for(int y=first;y<last;y++){
            int old_pixel=avp_gpu_sprite_sample_coord(y,origin,scale)==top+row;
            int new_pixel=y>=y0 && y<y1;
            if(old_pixel!=new_pixel){
                fprintf(stderr,"Y sample range mismatch height=%d scale=%.9f top=%d row=%d offset=%d y=%d\n",
                        height,scale,top,row,offset,y);
                abort();
            }
            ++checks;
        }
    }
    printf("sprite vertical sample bound equivalence PASS (%llu pixel checks)\n",checks);
}

int main(void)
{
    verify_vertical_bounds();
    /* R276 intentionally changes hosted widescreen sprite geometry. The
     * historical cache-vs-reference parity comparison is valid only at the
     * untouched 320x228 native coordinate plane. The new independent-axis
     * scaling is separately tested by test_r276_sprite_aspect. */
    const unsigned sizes[][2]={{320,228}};
    const double ranges[]={0.05,0.12,0.35,0.9,1.7,3.5,7.9};
    const unsigned actors[]={AC_ALIEN,AC_PREDATOR,AC_HUMAN,AC_SPARK1,AC_EGG,AC_ACRAWL};
    const int offsets[]={-60,-7,0,37,96};
    unsigned long long comparisons=0;
    u8 aaf[4096];
    AvpAvpGpuState *state=(AvpAvpGpuState*)calloc(1,sizeof(*state));
    assert(state);
    for(unsigned si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++){
        unsigned w=sizes[si][0],h=sizes[si][1];
        const size_t pixel_count=(size_t)w*h;
        AvpJaguarFramebuffer a={w,h,NULL},b={w,h,NULL};
        AvpMaze2Occlusion oc={w,h,NULL};
        double *zbuf=(double*)malloc((size_t)w*sizeof(double));
        a.pixels=(u32*)malloc(pixel_count*sizeof(u32));
        b.pixels=(u32*)malloc(pixel_count*sizeof(u32));
        oc.depth=(float*)malloc(pixel_count*sizeof(float));
        assert(a.pixels&&b.pixels&&oc.depth&&zbuf);
        for(unsigned variant=0;variant<168u;variant++){
            AvpAmp actor={0};
            unsigned creature=actors[variant%6u], target=0u;
            const double distance=ranges[(variant/6u)%7u];
            const unsigned oc_mode=(variant/42u)%4u;
            const int dx=(int)((variant*47u)%21u)-10;
            const unsigned aaf_id=creature==AC_ALIEN?8u:creature==AC_PREDATOR?29u:
                creature==AC_HUMAN?55u:creature==AC_SPARK1?47u:creature==AC_EGG?20u:33u;
            actor.creature=(u16)creature;
            actor.animseq=AS_STAND;
            if(creature==AC_HUMAN)actor.animseq=AS_RUN;
            actor.yoffset=(s16)offsets[(variant/3u)%5u];
            actor.angle=(u16)((variant&1u)?0u:0x8000u);
            actor.xpos=(s32)(distance*65536.0);
            actor.ypos=(s32)(dx*65536.0/14.0);
            sprite_rescale=(s16)((variant%4u==0u)?48u:(variant%4u==1u)?128u:(variant%4u==2u)?256u:400u);
            if(creature==AC_HUMAN){
                /* Dynamic Marine-facing sprite art depends on the viewing angle. */
                double look=atan2((double)actor.ypos,(double)actor.xpos);
                double face=-(double)actor.angle*(2.0*M_PI/65536.0),rel=look-face;
                while(rel<-M_PI)rel+=2.0*M_PI;
                while(rel>M_PI)rel-=2.0*M_PI;
                target=fabs(rel)<M_PI/4.0?24u:fabs(rel)>3.0*M_PI/4.0?56u:55u;
            }else target=aaf_id;
            size_t len=make_aaf(aaf,sizeof(aaf),variant);
            memset(state->aaf,0,sizeof(state->aaf));
            memset(state->aaf_size,0,sizeof(state->aaf_size));
            state->aaf[target]=aaf;
            state->aaf_size[target]=len;
            for(size_t k=0;k<pixel_count;k++){
                a.pixels[k]=(u32)(k*2654435761u+variant*31337u);
                oc.depth[k]=(k%11u<3u)?(float)0.1f:12.0f;
            }
            memcpy(b.pixels,a.pixels,pixel_count*sizeof(u32));
            for(unsigned x=0;x<w;x++)zbuf[x]=(x%13u<4u)?0.12:12.0;
            oc.width=oc_mode==2u?w+1u:w;
            const AvpMaze2Occlusion *owner=oc_mode==0u?NULL:&oc;
            if(oc_mode==3u)owner=NULL;
            r134_draw_amp_sprite(state,&a,&actor,0,0,1,0,0,1,(double)w*0.5,
                oc_mode==3u?NULL:zbuf,owner);
            draw_amp_sprite(state,&b,&actor,0,0,1,0,0,1,(double)w*0.5,
                oc_mode==3u?NULL:zbuf,owner);
            if(w==320u&&h==228u&&memcmp(a.pixels,b.pixels,pixel_count*sizeof(u32))){
                fprintf(stderr,"sprite pixel difference width=%u height=%u case=%u species=%u\n",w,h,variant,creature);
                return 1;
            }
            comparisons++;
        }
        free(a.pixels);free(b.pixels);free(oc.depth);free(zbuf);
    }
    free(state);
    printf("sprite native-coordinate span parity PASS (%llu complete-frame pairs)\n",comparisons);
    return 0;
}
