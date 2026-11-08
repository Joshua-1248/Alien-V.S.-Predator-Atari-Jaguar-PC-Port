#include "jaguar/avp_gpu/frontend_renderer.h"
#include "jaguar/avp_gpu/frontend_fade.h"
#include "jaguar/cry.h"
#include "mjp.h"
#include "player.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t rgb(unsigned r,unsigned g,unsigned b){return (r<<16)|(g<<8)|b;}
static void put(AvpJaguarFramebuffer *fb,int x,int y,uint32_t c)
{
    if(x>=0&&y>=0&&(unsigned)x<fb->width&&(unsigned)y<fb->height)
        fb->pixels[(size_t)y*fb->width+(unsigned)x]=c;
}

static uint32_t frontend_rgb16_to_host(u16 p);
/* NTSC InitVideo + SetScreen(320,240) in the retail ROM: VDB=$001c.
 * Make_New packs BITOBJ YPOS as 2 * Ob_Pos_Y (via Ob_Pos_Y << 4). */
#define AVP_TITLE_NTSC_VDB_HALFLINES 0x1cu
static int frontend_title_op_y(unsigned halfline_ypos);

/* MJP title FILES 208 is an unscaled 320x280 BITOBJ, not a
 * fitted background. Make_TLi sets Ob_Pos_Y=Zero_Line ($1c on NTSC);
 * Make_New encodes that as YPOS=2*$1c=$38, and TOM starts objects at
 * VDB=$1c. Thus the first picture row appears at native screen Y=14.
 * The 320x228 gameplay framebuffer is a clipping aperture, NOT a scale. */
/* Full-screen MJP pictures are unscaled 320x240 OP bitmaps whose first line
 * is the first displayed line (VDB), i.e. port frontend row 14.  Returns the
 * source row for port row y, or -1 outside the bitmap. */
static int frontend_native_full_row(const AvpGpuRgbImage *im,unsigned y)
{
    const int sy=(int)y-14;
    if(!im||im->width!=320||im->height!=240)return -2; /* not a full-screen picture */
    return (sy>=0&&sy<240)?sy:-1;
}
static void frontend_blit_image(AvpJaguarFramebuffer *fb,const AvpGpuRgbImage *im,
                                int title_background)
{
    unsigned x,y;
    if(!fb||!fb->pixels||!im||!im->rgb||im->width<=0||im->height<=0)return;
    for(y=0;y<fb->height;y++){
        int logical_h=im->height>240?240:im->height;
        const int native_title=(title_background && im->width==320 && im->height==280);
        const int title_y=frontend_title_op_y(2u*AVP_TITLE_NTSC_VDB_HALFLINES);
        int crop_y=native_title?0:(im->height-logical_h)/2;
        /* Reproduce the retail title object's *position* and 1:1 vertical
         * BITOBJ sampling. The first 14 scanlines contain no title bitmap.
         * Only the title/credits bitmap path changes; other frontends retain
         * their existing host picture conversion and placement. */
        if(native_title && (int)y<title_y)continue;
        const int full=frontend_native_full_row(im,y);
        if(full==-1){
            for(x=0;x<fb->width;x++)fb->pixels[(size_t)y*fb->width+x]=0u;
            continue;
        }
        int sy=native_title?(int)y-title_y:full>=0?full:
            crop_y+(int)(((unsigned long long)y*(unsigned)logical_h)/fb->height);
        if(sy<0)sy=0;
        if(sy>=im->height)sy=im->height-1;
        for(x=0;x<fb->width;x++){
            int sx=(int)((unsigned long long)x*(unsigned)im->width/fb->width);
            const u8 *q;
            if(sx>=im->width)sx=im->width-1;
            q=im->rgb+((size_t)sy*(size_t)im->width+(size_t)sx)*3u;
            /* The title and its credits reuse share the retail 16-bit
             * RGB bitmap Object Processor output.  Do not retain the low
             * RGB888 bits of JagPEG's host decoder when the OP is showing
             * the unscaled FILES 208 background at full brightness. The
             * independently scaled title objects already take this path. */
            fb->pixels[(size_t)y*fb->width+x]=title_background
                ? frontend_rgb16_to_host(avp_gpu_rgb16_pack(q[0],q[1],q[2]))
                : rgb(q[0],q[1],q[2]);
        }
    }
}

/* Retail MJP $01DF2C, $01E046 and $01E0F2 all build the 192x144
 * frontend picture objects with Ob_Pos_X=$12 and
 * Ob_Pos_Y=High_Line+$10 (Make_New encodes its OP YPOS as 2*Ob_Pos_Y).
 * On NTSC the video VDB is $1c and High_Line=$1c, so native visible Y is
 * (2*($1c+$10)-$1c)/2 = 30.  The previous bottom-centred (64,76) placement
 * was a host invention, not a Jaguar object position.  This is the
 * picture's unscaled geometry. This common MJP picture program is called
 * by Do_Intro, Do_Escap and Do_Lose: the intro, escape, terminated and
 * base-explodes 192x144 FILES images share these same coordinates.
 * The eight-VBL transition *effect* remains separately unverified against
 * the original GPU/OP intermediate frames. */
#define AVP_INTRO_PICTURE_X 0x12
/* $12+4*10: the retail X whenever $4BED4=0 (Do_Lose, Do_Escap, $01DB40). */
#define AVP_INTRO_PICTURE_X_CENTRE (AVP_INTRO_PICTURE_X+40)
#define AVP_INTRO_PICTURE_Y_NTSC 30
#define AVP_MJP_STORY_TEXT_Y (0xa6+14)
/* MJP screen-buffer row -> port row.  The buffer is displayed from title
 * line 0 (VDB=28 half-lines, top_line=14); port rows add that 14, exactly as
 * Legal_Pic ($A8+14 -> 196) and the story text ($A6 -> 180). */
#define AVP_MJP_BUFFER_Y(row) ((int)(row)+14)
static int frontend_native_mjp_picture(unsigned mode,const AvpGpuRgbImage *im)
{
    return im && im->width==192 && im->height==144 &&
           (mode==AVP_MJP_INTRO || mode==AVP_MJP_ESCAPE ||
            mode==AVP_MJP_SIM_TERMINATED || mode==AVP_MJP_BASE_EXPLODES);
}

static void frontend_blit_native_mjp_picture(AvpJaguarFramebuffer *fb,const AvpGpuRgbImage *im,int x0)
{
    if(!fb||!im||!im->rgb||im->width<=0||im->height<=0)return;
    for(int y=0;y<im->height;y++){
        const int dy=AVP_INTRO_PICTURE_Y_NTSC+y;
        if(dy<0||(unsigned)dy>=fb->height)continue;
        for(int x=0;x<im->width;x++){
            const int dx=x0+x;
            if(dx<0||(unsigned)dx>=fb->width)continue;
            const u8 *q=im->rgb+((size_t)y*(size_t)im->width+(size_t)x)*3u;
            fb->pixels[(size_t)dy*fb->width+(unsigned)dx]=rgb(q[0],q[1],q[2]);
        }
    }
}

static unsigned frontend_normal_role(int species)
{
    return species==PT_PREDATOR?1u:(species==PT_HUMAN?3u:5u);
}

static unsigned frontend_detail_role(int species)
{
    return species==PT_PREDATOR?2u:(species==PT_HUMAN?4u:6u);
}

static u16 frontend_image_rgb16(const AvpGpuRgbImage *im,unsigned x,unsigned y,
                                unsigned dst_w,unsigned dst_h)
{
    int logical_h,crop_y,sy,sx;
    const u8 *q;
    if(!im||!im->rgb||im->width<=0||im->height<=0||!dst_w||!dst_h)return 0u;
    if(dst_h!=(unsigned)im->height){ /* screen-space call, not a texel lookup */
        const int full=frontend_native_full_row(im,y);
        if(full==-1)return 0u;
        if(full>=0){
            q=im->rgb+((size_t)full*320u+(x<320u?x:319u))*3u;
            return avp_gpu_rgb16_pack(q[0],q[1],q[2]);
        }
    }
    logical_h=im->height>240?240:im->height;
    crop_y=im->height>logical_h?(im->height-logical_h)/2:0;
    sy=crop_y+(int)(((unsigned long long)y*(unsigned)logical_h)/dst_h);
    if(sy<0)sy=0;
    if(sy>=im->height)sy=im->height-1;
    sx=(int)(((unsigned long long)x*(unsigned)im->width)/dst_w);
    if(sx<0)sx=0;
    if(sx>=im->width)sx=im->width-1;
    q=im->rgb+((size_t)sy*(size_t)im->width+(size_t)sx)*3u;
    return avp_gpu_rgb16_pack(q[0],q[1],q[2]);
}

static uint32_t frontend_rgb16_to_host(u16 p)
{
    u8 r,g,b;
    avp_gpu_rgb16_unpack(p,&r,&g,&b);
    return rgb(r,g,b);
}

/* SELECT confirmation preserves the active Jaguar SCBITOBJ source space
 * throughout scaling.  R194 sampled the outgoing image in source-card
 * coordinates but sampled incoming art with fixed screen x/y.  Consequently
 * two different body/letter/border positions were crossfaded, leaving a
 * ghosted portrait that the retail capture does not show.
 *
 * The real SOBJ handoff is 21/32, followed by the observed 18..32/32
 * integer scale sequence.  The retail video at 27.13..27.3 seconds also
 * shows the art change completing near the *start* of enlargement, not
 * continuing as an invented 50-tick dissolve.  Both planes now use the
 * same SCBITOBJ source coordinate; visual color transition and CPU-owned
 * 50-VBL positioning are independent. */
static void frontend_blit_select_zoom_cross(AvpJaguarFramebuffer *fb,
                                            const AvpGpuRgbImage *from,
                                            const AvpGpuRgbImage *to,
                                            const AvpAvpGpuState *st,
                                            unsigned phase)
{
    int x0,y0,w,h;
    unsigned scale=0u,has_retail_pose=0u;
    unsigned tick;
    if(!fb||!fb->pixels||!from||!from->rgb||!to||!to->rgb||!st)return;
    tick=st->frontend_select_cross_tick>50u ? 50u :
         st->frontend_select_cross_tick;
    if(phase>16u)phase=16u;
    if(from->width==320 && from->height>=239){
        for(unsigned i=0u;i<3u;i++){
            if(!st->frontend_select_pose_valid[i] ||
               st->frontend_select_pose_species[i]!=st->frontend_selected_species)
                continue;
            const unsigned initial=(unsigned)st->frontend_select_pose_scale[i];
            if(initial && initial<=32u &&
               st->frontend_select_pose_x[i]==st->frontend_select_zoom_x &&
               st->frontend_select_pose_y[i]==st->frontend_select_zoom_y &&
               ((320u*initial+31u)/32u)==(unsigned)st->frontend_select_zoom_w &&
               ((239u*initial+31u)/32u)==(unsigned)st->frontend_select_zoom_h){
                has_retail_pose=1u;
                scale=initial;
                break;
            }
        }
    }
    if(!has_retail_pose){
        /* A bypassed carousel provides no OP pose.  Do not resurrect the
         * legacy host-authored rectangle interpolation in this path. */
        x0=0;y0=0;w=(int)fb->width;h=(int)fb->height;
        scale=0u;
    }else if(tick<=3u){
        x0=st->frontend_select_zoom_x;
        /* Only the three-tick SGCF hold still uses the last SOBJ OP pose. */
        y0=st->frontend_select_zoom_y;   /* OP line = frontend row (row 14 = VDB) */
        w=st->frontend_select_zoom_w;
        h=st->frontend_select_zoom_h;
    }else{
        /* Retail capture's integer 18,19,...32 scale steps. */
        scale=18u+(tick-4u)/3u;
        if(scale>32u)scale=32u;
        w=(int)((320u*scale+31u)/32u);
        h=(int)((240u*scale+31u)/32u);
        x0=(320-w)/2;
        y0=73-(int)((5u*scale)/2u);
        if(y0<0)y0=0;
        y0+=14;  /* capture-measured title rows -> port frontend rows */
    }
    if(w<=0||h<=0)return;
    for(unsigned y=0;y<fb->height;y++)for(unsigned x=0;x<fb->width;x++){
        u16 a=0,b=0;
        if((int)x>=x0&&(int)x<x0+w&&(int)y>=y0&&(int)y<y0+h){
            const unsigned ix=(unsigned)((int)x-x0);
            const unsigned iy=(unsigned)((int)y-y0);
            const unsigned sx=scale ? ix*32u/scale :
                (unsigned)(((uint64_t)ix*(unsigned)from->width)/(unsigned)w);
            const unsigned sy=scale ? iy*32u/scale :
                (unsigned)(((uint64_t)iy*(unsigned)(from->height>239?239:from->height))/(unsigned)h);
            const unsigned ax=sx>=(unsigned)from->width?(unsigned)from->width-1u:sx;
            const unsigned ay=sy>=(unsigned)from->height?(unsigned)from->height-1u:sy;
            const u8 *src=from->rgb+3u*((size_t)ay*(size_t)from->width+ax);
            a=avp_gpu_rgb16_pack(src[0],src[1],src[2]);
            /* Both picture planes address the *same* OP source texel;
             * screen-space sampling here doubled letters/characters while
             * zooming in the shipped R194 executable. */
            b=frontend_image_rgb16(to,ax,ay,(unsigned)from->width,
                                  (unsigned)from->height);
        }
        fb->pixels[(size_t)y*fb->width+x]=
            frontend_rgb16_to_host(avp_gpu_rgb16_cross(a,b,phase));
    }
}

/* The retail $01DF2C transition constructs a 192x144 Object Processor
 * picture at X=$12,Y=30 on NTSC.  Keep this native position through the
 * hosted transition rather than using the previous invented centre/bottom
 * coordinates.  The original GPU transition microcode is not yet lifted. */
static u16 frontend_scene_sample(const AvpGpuRgbImage *im,unsigned mode,int picture_x,
                                 unsigned width,unsigned height,unsigned x,unsigned y)
{
    const u8 *q;
    unsigned sx,sy;
    if(!im||!im->rgb||im->width<=0||im->height<=0)return 0u;
    if(frontend_native_mjp_picture(mode,im)){
        const int x0=picture_x;
        const int y0=AVP_INTRO_PICTURE_Y_NTSC;
        if((int)x<x0||(int)x>=x0+192||(int)y<y0||(int)y>=y0+144)return 0u;
        sx=(unsigned)((int)x-x0);sy=(unsigned)((int)y-y0);
    }else{
        int hh=im->height>240?240:im->height;
        int crop=(im->height-hh)/2;
        sx=(unsigned)(((uint64_t)x*(unsigned)im->width)/width);
        sy=(unsigned)(crop+(int)(((uint64_t)y*(unsigned)hh)/height));
        if(sx>=(unsigned)im->width)sx=(unsigned)im->width-1u;
        if(sy>=(unsigned)im->height)sy=(unsigned)im->height-1u;
    }
    q=im->rgb+((size_t)sy*(size_t)im->width+sx)*3u;
    return avp_gpu_rgb16_pack(q[0],q[1],q[2]);
}

static void frontend_blit_scene_cross(AvpJaguarFramebuffer *fb,
                                     const AvpGpuRgbImage *from,
                                     const AvpGpuRgbImage *to,
                                     unsigned mode,int picture_x,unsigned phase)
{
    if(!fb||!fb->pixels)return;
    if(phase>16u)phase=16u;
    /* A NULL operand is the cleared $4BE18 buffer: CROSS reads only its
     * first 144 rows, and MJP text starts at row $A6, so it is black. */
    for(unsigned y=0;y<fb->height;y++)for(unsigned x=0;x<fb->width;x++){
        u16 a=frontend_scene_sample(from,mode,picture_x,fb->width,fb->height,x,y);
        u16 b=frontend_scene_sample(to,mode,picture_x,fb->width,fb->height,x,y);
        fb->pixels[(size_t)y*fb->width+x]=
            frontend_rgb16_to_host(avp_gpu_rgb16_scene_cross(a,b,phase));
    }
}

static void frontend_blit_fade(AvpJaguarFramebuffer *fb,const AvpGpuRgbImage *im,unsigned phase)
{
    unsigned x,y;
    if(!fb||!fb->pixels||!im||!im->rgb)return;
    if(phase>16u)phase=16u;
    for(y=0;y<fb->height;y++)for(x=0;x<fb->width;x++){
        u16 p=frontend_image_rgb16(im,x,y,fb->width,fb->height);
        fb->pixels[(size_t)y*fb->width+x]=frontend_rgb16_to_host(avp_gpu_rgb16_fraction(p,phase));
    }
}

/* Original Make_TLi binds the ALIEN object to FILES 236 and PREDATOR
 * to FILES 237, both GZIP-compressed, precomposed RGB16/320-wide OP
 * buffers. The raw JPEG 205/207 dimensions are not the title object
 * source rasters. Only Jaguar original OP scale and positions are applied. */
static void frontend_blit_title_sprite(AvpJaguarFramebuffer *fb,
                                        const AvpGpuRgbImage *im,
                                        int dx,int dy,unsigned dst_w,unsigned dst_h,
                                        unsigned op_src_h,unsigned op_scale,unsigned reveal)
{
    unsigned source_row=0u;
    unsigned remainder=op_scale+32u;
    if(!fb||!fb->pixels||!im||!im->rgb||im->width!=320||im->height<=0||
       !dst_w||!dst_h||!op_src_h||!op_scale||!reveal)return;
    if(reveal>16u)reveal=16u;
    /* Make_TLi/Update/Update2 program OP phrase 3 as
     * HSCALE | (VSCALE<<8) | ((VSCALE+0x20)<<16). Unlike a proportional
     * image resize, TOM's SCBITOBJ advances the source DATA line when the
     * 3.5-format REMAINDER is below 0x20, then subtracts 0x20 after
     * each displayed line. Preserve this initial remainder and the OP's
     * source-height termination. In particular, the first source line can
     * appear more than once before the data pointer advances, and downscale
     * phases skip source rows. Never restart the counter at the framebuffer
     * clipping boundary: offscreen lines still advance the OP state.
     */
    for(unsigned y=0;y<dst_h && source_row<op_src_h;y++){
        const int py=dy+(int)y;
        /* Ob_Height is the source-data read aperture (110/82), separate
         * from the GZIP allocation (122/76). An unallocated source line
         * remains transparent until actual Jaguar memory reads are proven. */
        if(py>=0&&(unsigned)py<fb->height&&source_row<(unsigned)im->height){
        const unsigned sy=source_row;
        for(unsigned x=0;x<dst_w;x++){
            const int px=dx+(int)x;
            if(px<0||(unsigned)px>=fb->width)continue;
            /* 320 source pixels = 80 16-bpp phrases. The destination's
             * integer width is 320*HSCALE/32, exactly, so this is the
             * horizontal OP 0x20-increment stepping for these title slots. */
            const unsigned sx=(unsigned)((uint64_t)x*32u/op_scale);
            if(sx>=(unsigned)im->width)continue;
            const u8 *q=im->rgb+((size_t)sy*(size_t)im->width+sx)*3u;
            /* Jaguar Object Processor TRANS skips RGB16 pixel zero, not
             * arbitrary dark pixels. The title GZIP buffer already stores
             * native RGB16; the resource loader expands those channel bits
             * only for the existing software-framebuffer representation.
             * See retail Make_TLi Ob_Trans=$8000 object phrases. */
            if(avp_gpu_rgb16_pack(q[0],q[1],q[2])==0u)continue;
            /* Repack the losslessly expanded original RGB16 samples before
             * host output; don't substitute source JPEG colors or palettes. */
            const uint32_t col=frontend_rgb16_to_host(
                avp_gpu_rgb16_pack(q[0],q[1],q[2]));
            const size_t at=(size_t)py*fb->width+(unsigned)px;
            if(reveal==16u)fb->pixels[at]=col;
            else{
                const uint32_t old=fb->pixels[at];
                const unsigned inv=16u-reveal;
                const unsigned r=(((old>>16)&255u)*inv+(unsigned)q[0]*reveal)>>4;
                const unsigned g=(((old>>8)&255u)*inv+(unsigned)q[1]*reveal)>>4;
                const unsigned b=((old&255u)*inv+(unsigned)q[2]*reveal)>>4;
                fb->pixels[at]=rgb(r,g,b);
            }
        }
        }
        /* SCBITOBJ source-data writeback occurs after the displayed line;
         * the live phrase remainder is kept across line-buffer swaps. */
        while(remainder<32u){
            remainder+=op_scale;
            source_row++;
        }
        remainder-=32u;
    }
}

/* CPU Title object coordinates/scale (retail Make_TLi, Update, Update2).
 * The Jaguar Object Processor YPOS is in half-lines (TOM manual, phrase 1
 * bits 3..13), while the framebuffer uses full active video lines.
 * Retail NTSC InitMJP: VDB = video_vcenter - 240 = $10C - $F0 = $1C.
 * Make_New encodes Ob_Pos_Y << 4, i.e. YPOS = 2 * Ob_Pos_Y; Update2 writes
 * its half-line YPOS directly at phrase bit 3.  The old host used the CPU's
 * unconverted Y expression for both title sprites, putting them at wrong
 * heights even though the original x/scale formulas were preserved.
 * Convert the actual phrase YPOS relative to VDB, without changing CPU MJP,
 * sprite pixels, original scale/phase or the shared selection renderer.
 * The separately authored 16-bit Object Processor scales use 32 as 1:1. */
static int frontend_title_op_y(unsigned halfline_ypos)
{
    return ((int)halfline_ypos - AVP_TITLE_NTSC_VDB_HALFLINES)/2;
}
typedef struct AvpTitleOpPose {
    int x,y;
    unsigned width,height,scale;
} AvpTitleOpPose;

static AvpTitleOpPose frontend_title_alien_pose(unsigned phase)
{
    AvpTitleOpPose pose;
    unsigned scale;
    /* Retail Clear_Bu $1FA82-$1FAE0: the 68000 only calls One_Tick+Update
     * while the GPU program advances $7FB4 for the $23 (35) steps passed in
     * its parameter block; Update ($1E7B4) uses scale=$2C-$7FB4 and
     * x=($20-$7FB4)*8+$2A.  The logo therefore settles at phase 35
     * (scale 9, x 18) -- matching BigPEmu's ALIEN left edge (~21.4 px) --
     * not phase 32 (scale 12, x 42). */
    if(phase>35u)phase=35u;
    scale=44u-phase;
    pose.x=(int)(0x20-(int)phase)*8+0x2a;
    /* Make_TLi slot 2: Ob_Pos_Y=Zero_Line+$20, with <<4 in Make_New. */
    pose.y=frontend_title_op_y(2u*(AVP_TITLE_NTSC_VDB_HALFLINES+0x20u));
    pose.width=(320u*scale)/32u;
    /* The initialized REMAINDER=VSCALE+0x20 adds two displayed lines
     * before HEIGHT source rows are exhausted, for retail title scales. */
    pose.height=(110u*scale)/32u+2u;
    pose.scale=scale;
    return pose;
}

static unsigned frontend_title_predator_phase(unsigned tick)
{
    /* Retail startup at 68000 $1FB98 writes 1 to $7FB8. Its loop makes
     * two Update2 calls at each phase before adding 2, and exits when the
     * value becomes $23 (35). Therefore the visible phases are odd 1..33.
     * This is independent of the host's redraw frequency. */
    unsigned step=(tick>33u)?(tick-33u)/2u:0u;
    if(step>16u)step=16u;
    return 1u+step*2u;
}

/* Retail $01FD0A: after the Update2 loop ends, the blitter copies the
 * 160x40 FILES 232 wordmark opaquely into the title buffer at (153,33);
 * one VBL later ($01FBF6/$01FBFC) the scaled FILES 237 object is hidden.
 * Title buffer row 0 is port frontend row 14. */
static int title_predator_stamp(const AvpAvpGpuState *st,AvpJaguarFramebuffer *fb)
{
    const AvpGpuRgbImage *im=&st->frontend_img[AVP_MJP_TITLE][6];
    if(!st->frontend_loaded[AVP_MJP_TITLE][6]||!im->rgb||im->width!=160||im->height!=40)return 0;
    for(unsigned y=0u;y<40u;y++){
        const unsigned py=14u+33u+y;
        if(py>=fb->height)break;
        for(unsigned x=0u;x<160u;x++){
            const u8 *q=im->rgb+((size_t)y*160u+x)*3u;
            fb->pixels[(size_t)py*fb->width+153u+x]=
                frontend_rgb16_to_host(avp_gpu_rgb16_pack(q[0],q[1],q[2]));
        }
    }
    return 1;
}
#define AVP_TITLE_PREDATOR_STAMP_TICK 67u /* first VBL after Update2(33) */
/* Retail $01FAE2..$01FB00: when the ALIEN GPU loop ends, $01F554 blits the
 * 96x40 FILES 205 wordmark opaquely to (15,33) of the title buffers, and
 * one VBL later ($01FB06/$01FB0C) hides the scaled FILES 236 object. */
static int title_alien_stamp(const AvpAvpGpuState *st,AvpJaguarFramebuffer *fb)
{
    const AvpGpuRgbImage *im=&st->frontend_img[AVP_MJP_TITLE][5];
    if(!st->frontend_loaded[AVP_MJP_TITLE][5]||!im->rgb||im->width!=96||im->height!=40)return 0;
    for(unsigned y=0u;y<40u;y++){
        const unsigned py=14u+33u+y;
        if(py>=fb->height)break;
        for(unsigned x=0u;x<96u;x++){
            const u8 *q=im->rgb+((size_t)y*96u+x)*3u;
            fb->pixels[(size_t)py*fb->width+15u+x]=
                frontend_rgb16_to_host(avp_gpu_rgb16_pack(q[0],q[1],q[2]));
        }
    }
    return 1;
}
#define AVP_TITLE_ALIEN_STAMP_TICK 36u /* first VBL after Update phase 35 */
static AvpTitleOpPose frontend_title_predator_pose(unsigned phase)
{
    AvpTitleOpPose pose;
    unsigned scale;
    if(phase>33u)phase=33u;
    scale=32u-(phase>>1);
    pose.x=(int)(8u+phase*3u+((phase&1u)?0u:3u)+(phase>>1)*3u);
    /* Update2 writes YPOS = (High_Line<<1)+$D8-(p*4+p/2).
     * On NTSC High_Line==Zero_Line==VDB. */
    pose.y=frontend_title_op_y(2u*AVP_TITLE_NTSC_VDB_HALFLINES+
                             0xd8u-(phase*4u+(phase>>1)));
    pose.width=(320u*scale)/32u;
    pose.height=(82u*scale)/32u+2u;
    pose.scale=scale;
    return pose;
}

/* Original BigPEmu credit scroll, transcribed from the user's retail video.
 * This is a hosted presentation replacement for the previously invented
 * paginated credits page, not a claim of byte-matched Jaguar text ROM data.
 * All glyph pixels still come from the player's own retail FILES 235 font.
 * The title's independent JagPEG objects are likewise loaded from that ROM. */
static const char *const retail_credit_lines[]={
    "Produced by:","James Purple Hampton","",
    "Programmed by:","Mike Beaton (Rebellion)","Andrew Whittaker","",
    "Additional Coding by:","Mike Pooler","",
    "Art by:","Toby Harrison-Banfield","(Rebellion)",
    "Stuart Wilson (Rebellion)","Jeffery Gatrall","Keoni Los Banos","",
    "Sound Effects & Music by:","James Grunke","M Stevens",
    "Nathan Brenholdt","Will Davis","Paul Foster","Tom Gillen",
    "Alex Quarmby (Rebellion)","",
    "Level Design by:","Hank Cappa","Hans Jacobsen","Andrew Keim",
    "Lance Lewis","Dan McNamee","Sean Patten","",
    "Tested by:","Hank Cappa","Tom Gillen","Scott Hunter",
    "Hans Jacobsen","Andrew Keim","Lance Lewis","Dan McNamee",
    "Martin Mueller","Sean Patten","Joe Sousa","",
    "Continuity to Films","& Technical Advisor:","Sean Patten","",
    "Computer Text by:","Lance Lewis","",
    "Box Art/Title Screen:","Andrew H. Denton","(on Lightwave 3D)","",
    "Image Touch-up:","Jeffery Gatrall","",
    "Manual by:","Chris Hudak","Lance Lewis","",
    "Special Thanks To:","Avalon Rising","Terry Bratcher","Matt Clayson",
    "Tom Gillen","David Johny","Scott Marcus","Susan McBride",
    "Sandra Miller & Baby","Lena Tan Cheng Im","Phil O'Pray",
    "Ed Sicard","Kathie Sleeper","Tom Sloper","James Sposato",
    "Carrie Tahquechi","Ted Tahquechi","Tabitha Tosti",
    "Sean Yew","The Whittaker Family","46 Western Rd."
};

static unsigned title_reveal_phase(unsigned tick,unsigned start,unsigned ticks)
{
    if(tick<=start)return 0u;
    if(tick>=start+ticks)return 16u;
    return (unsigned)((tick-start)*16u/ticks);
}

static void frontend_font_glyph(AvpJaguarFramebuffer *fb,const u8 *font,size_t font_size,
                                int font_w,int font_h,int glyph_w,int glyph_h,
                                int x,int y,unsigned char ch)
{
    unsigned gi,gx,gy,xx,yy;
    if(!fb||!font||font_w<=0||font_h<=0||glyph_w<=0||glyph_h<=0)return;
    if(font_size<(size_t)font_w*(size_t)font_h*2u)return;
    /* FILES 235 begins with '!' at cell zero; 0x20 is not a glyph.
       The old 0x20 origin shifted every letter to the following cell. */
    if(ch==0x20u)return; /* No space glyph in FILES 235: advance only. */
    if(ch<0x21u||ch>0x7fu)ch='_';
    gi=(unsigned)ch-0x21u;
    gx=(gi&31u)*(unsigned)glyph_w;gy=(gi>>5)*(unsigned)glyph_h;
    for(yy=0;yy<(unsigned)glyph_h;yy++)for(xx=0;xx<(unsigned)glyph_w;xx++){
        size_t o=2u*((size_t)(gy+yy)*(size_t)font_w+(gx+xx));
        /* FILES 235 glyph words are blitted into the MJP RGB16 screen
         * buffer, so TOM shows them as RGB16 (R5 B5 G6), not CRY: the
         * story-text word $5AFF is (90,255,90), as in the BigPEmu
         * reference, where a CRY decode gave cyan (140,254,226). */
        const u16 w=(u16)(((u16)font[o]<<8)|font[o+1u]);
        if(!w)continue;
        put(fb,x+(int)xx,y+(int)yy,frontend_rgb16_to_host(w));
    }
}

/* BigPEmu retail 60-fps capture, 2026-10-21 15-32-16, frames 2810..2857:
 * the orange third score row rises ~11.2 display pixels per captured VBlank.
 * Dividing by the picture's ~3.75 display/native-pixel scale gives the Jaguar
 * presentation step of 3 native picture pixels per VBlank. Its end position
 * remains fixed thereafter. Source-side CPU score calculations are unchanged.
 * The video constrains the step, not a cycle-exact reconstruction of the
 * original OP/GPU scroll microcode or its initiation latency. */
static int frontend_fame_row_slide(unsigned tick)
{
    return tick>=80u?0:(int)(80u-tick)*3;
}

static unsigned fame_band_from_marker(unsigned char marker)
{
    /* Retail Fame_Tex: m -> Comp_Font 0, p -> 2, otherwise a -> 4.
       Each value is a 16-pixel source-row index within the 320x96 CRY sheet. */
    if(marker=='m')return 0u;
    if(marker=='p')return 2u;
    return 4u;
}

static void fame_font_glyph(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                            int x,int y,unsigned species_band,unsigned char ch)
{
    unsigned idx,gx,gy,xx,yy;
    if(!state||!fb||!state->avp_font||state->avp_font_size<320u*96u*2u)return;

    /* Retail FILES 234 glyphs have a 10-pixel ink width and 11-pixel
       source origin spacing. Confirmed by decoding ROM font pixels into
       the original menu labels (R180, independently repeated R207).
       320/32 is NOT the source stride: R205 wrongly reinstated 10 and
       scrambled both menu labels and Fame scores. Display advance = 11. */
    if(ch==' ')ch=0x5cu;
    if(ch<0x21u||ch>0x60u)return;
    idx=(unsigned)ch-0x21u;
    gx=(idx&31u)*11u;
    gy=(species_band+((idx>>5)&1u))*16u;
    /* Source row is only 320 pixels wide; upper-index cells crossing its
       end cannot be read as if they wrapped into the following row. */
    if(gx+10u>320u)return;
    for(yy=0;yy<16u;yy++)for(xx=0;xx<10u;xx++){
        size_t o=2u*((size_t)(gy+yy)*320u+(gx+xx));
        /* MJP runs TOM in RGB16 (VMODE $6C7 at $01BEB8, VARMOD clear), so
         * FILES 234 words are RGB16: $AEEA = (172,170,222), the BigPEmu
         * menu-text colour.  The former CRY decode flattened it to grey. */
        const u16 w=(u16)(((u16)state->avp_font[o]<<8)|state->avp_font[o+1u]);
        if(!w)continue;
        put(fb,x+(int)xx,y+(int)yy,frontend_rgb16_to_host(w));
    }
}

static void fame_font_text(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                           int x,int y,unsigned species_band,const char *text)
{
    if(!text)return;
    while(*text){fame_font_glyph(state,fb,x,y,species_band,(unsigned char)*text++);x+=11;}
}

static void authored_fame_text(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                               int x,int y,const char *text)
{
    unsigned band=4u;
    if(!text)return;
    if(*text=='m'||*text=='p'||*text=='a')band=fame_band_from_marker((unsigned char)*text++);
    fame_font_text(state,fb,x,y,band,text);
}

/* The title selector is white on the dimmed TITLE artwork in the retail
 * video, not an alien-colour Fame scoreboard. Use FILES 234's original
 * 11-pixel source-origin stride and 11-pixel destination advance, but keep
 * menu ink independent from the species-coloured Fame display. */
static void title_menu_glyph(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                             int x,int y,unsigned char ch)
{
    /* Retail $01D462 draws the menu's 'a'-marked strings from FILES 234
     * band 4, whose RGB16 words give the lavender-to-white gradient seen
     * in BigPEmu.  The former neutral-white luma was a CRY-era workaround. */
    fame_font_glyph(state,fb,x,y,4u,ch);
}
static void title_menu_text(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                            int x,int y,const char *s)
{
    if(!s)return;
    if(*s=='a'||*s=='m'||*s=='p')++s; /* authored Fame_Tex band marker */
    while(*s){title_menu_glyph(state,fb,x,y,(unsigned char)*s++);x+=11;}
}
/* Legal notice visible after the logo settles and before the original menu.
 * Text is transcribed from the user's retail BigPEmu reference; glyph pixels
 * come only from their local ROM FILES 235. The original bitmap-level
 * trademark/copyright marks and detailed timing still require OP tracing. */
static void title_legal_glyph(AvpAvpGpuState *st,AvpJaguarFramebuffer *fb,
                              int x,int y,unsigned char ch)
{
    if(!st||!st->mjp_font||st->mjp_font_size<224u*39u*2u||ch==' ')return;
    if(ch<0x21u||ch>0x7fu)return;
    unsigned gi=(unsigned)ch-0x21u;
    unsigned gx=(gi&31u)*7u,gy=(gi>>5)*13u;
    for(unsigned dy=0u;dy<8u;dy++)for(unsigned dx=0u;dx<5u;dx++){
        const unsigned sx=gx+(dx*7u)/5u,sy=gy+(dy*13u)/8u;
        const size_t o=2u*((size_t)sy*224u+sx);
        const unsigned luma=st->mjp_font[o+1u];
        if(luma)put(fb,x+(int)dx,y+(int)dy,rgb(luma,luma,luma));
    }
}
static void title_legal_text(AvpAvpGpuState *st,AvpJaguarFramebuffer *fb,
                             int y,const char *s)
{
    size_t n=strlen(s);
    int x=((int)fb->width-(int)(n*5u))/2;
    if(x<0)x=0;
    for(;*s;s++,x+=5)title_legal_glyph(st,fb,x,y,(unsigned char)*s);
}
/* Retail MJP Clear_Bu ($1FC2A-$1FC8E): GPU blit of Legal_Pic (FILES 233,
 * 320x40 RGB16) to buffer row $A8+$4BE24, where $4BE24 = $2EE92 = VDB/2 = 14
 * on NTSC, i.e. picture row 182, shown through the same OP position as the
 * title picture (+14 screen lines).  Colour 0 is transparent in the overlay. */
static int title_legal_bitmap(AvpAvpGpuState *st,AvpJaguarFramebuffer *fb)
{
    const AvpGpuRgbImage *im;
    const int top=0xa8+AVP_TITLE_NTSC_VDB_HALFLINES/2+
                  frontend_title_op_y(2u*AVP_TITLE_NTSC_VDB_HALFLINES);
    if(!st||!fb||!fb->pixels||!st->frontend_loaded[AVP_MJP_TITLE][4])return 0;
    im=&st->frontend_img[AVP_MJP_TITLE][4];
    if(!im->rgb||im->width!=320||im->height!=40)return 0;
    for(int y=0;y<im->height;y++){
        const int dy=top+y;
        if(dy<0||(unsigned)dy>=fb->height)continue;
        for(int x=0;x<im->width&&(unsigned)x<fb->width;x++){
            const u8 *q=im->rgb+((size_t)y*320u+(size_t)x)*3u;
            if(!q[0]&&!q[1]&&!q[2])continue;
            fb->pixels[(size_t)dy*fb->width+(size_t)x]=
                frontend_rgb16_to_host(avp_gpu_rgb16_pack(q[0],q[1],q[2]));
        }
    }
    return 1;
}
static void title_legal_notice(AvpAvpGpuState *st,AvpJaguarFramebuffer *fb)
{
    if(title_legal_bitmap(st,fb))return;
    /* The ©/™ glyphs are expressed as readable ASCII until the exact Jaguar
     * hardware glyph raster is recovered. Do not fabricate proprietary art. */
    title_legal_text(st,fb,182,"ALIEN AND PREDATOR (TM) (C) 1994 Twentieth Century Fox");
    title_legal_text(st,fb,193,"Film Corporation. All rights reserved. Used under sub-");
    title_legal_text(st,fb,204,"license from Activision. (C) 1994 Atari Corporation.");
}
/* Captured Jaguar Credits lettering is neutral white. Preserve the retail
 * FILES 235 glyph *coverage*, but do not use the CRY hue of the page/typewriter
 * brush for this distinct credit plane. The supplied retail ROM still owns
 * all bitmap pixels; this code only chooses title-overlay ink. */
static void frontend_credit_text(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                                 int x,int y,const char *text)
{
    if(!state||!state->mjp_font||state->mjp_font_size<224u*39u*2u||!text)return;
    for(;*text;text++,x+=7){
        unsigned char ch=(unsigned char)*text;
        if(ch<0x21u||ch>0x7fu)continue;
        unsigned index=(unsigned)ch-0x21u;
        unsigned sx=(index&31u)*7u,sy=(index>>5)*13u;
        for(unsigned yy=0u;yy<13u;yy++)for(unsigned xx=0u;xx<7u;xx++){
            size_t off=2u*((size_t)(sy+yy)*224u+sx+xx);
            unsigned intensity=state->mjp_font[off+1u];
            if(intensity)put(fb,x+(int)xx,y+(int)yy,rgb(intensity,intensity,intensity));
        }
    }
}

/* The retail video shows the Fame prompt as green small text at the bottom
 * of the engraved score image. Use FILES 235's original glyph coverage,
 * not the coloured 320x96 Fame brush used for names/scores. */
static void frontend_fame_prompt_text(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                                      int x,int y,const char *text)
{
    if(!state||!text||!state->mjp_font||state->mjp_font_size<224u*39u*2u)return;
    for(;*text;text++,x+=7){
        const unsigned char ch=(unsigned char)*text;
        if(ch<0x21u||ch>0x7fu)continue;
        const unsigned idx=ch-0x21u;
        const unsigned sx=(idx&31u)*7u,sy=(idx>>5)*13u;
        for(unsigned yy=0;yy<13u;yy++)for(unsigned xx=0;xx<7u;xx++){
            const size_t off=2u*((size_t)(sy+yy)*224u+sx+xx);
            /* Same native RGB16 FILES 235 word as the story text; the
             * former rgb(0,low byte,0) was a hand-tuned green. */
            const u16 w=(u16)(((u16)state->mjp_font[off]<<8)|state->mjp_font[off+1u]);
            if(w)put(fb,x+(int)xx,y+(int)yy,frontend_rgb16_to_host(w));
        }
    }
}

static void frontend_font_text(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,int large,int x,int y,const char *text)
{
    if(!text)return;
    if(large)authored_fame_text(state,fb,x,y,text);
    else if(state && state->frontend_mode==AVP_MJP_SELECT){
        /* FILES 235 glyphs, decoded as native RGB16 like every MJP text. */
        frontend_fame_prompt_text(state,fb,x,y,text);
    }else{
        while(*text){frontend_font_glyph(fb,state->mjp_font,state->mjp_font_size,224,39,7,13,x,y,(unsigned char)*text++);x+=7;}
    }
}

/* Jaguar MJP's 320x240 portraits are scaled with the OP's 0..32 fixed
 * scale field and displayed at the positions computed by select_track.
 * Render each slot in its original object ordering; the gameplay selector
 * owns the actual pose updates, not host frame interpolation. */
static void frontend_blit_select_object(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,unsigned slot)
{
    const AvpGpuRgbImage *im;
    unsigned role,scale;
    int species,x0,y0,w,h;
    if(slot>=3u||!state->frontend_select_pose_valid[slot])return;
    species=state->frontend_select_pose_species[slot];
    if(species!=PT_HUMAN&&species!=PT_ALIEN&&species!=PT_PREDATOR)return;
    role=frontend_normal_role(species);
    if(!state->frontend_loaded[1u][role])return;
    im=&state->frontend_img[1u][role];
    if(!im->rgb||im->width<=0||im->height<=0)return;
    scale=(unsigned)state->frontend_select_pose_scale[slot];
    if(scale==0u||scale>32u)return;
    x0=state->frontend_select_pose_x[slot];
    /* Recovered R196: NTSC SetScreen(320,240) programs VDB=$001C
     * (28 half-lines = 14 noninterlaced active scanlines). Object Y is
     * in the OP coordinate domain, not a framebuffer-relative ordinate. */
    y0=state->frontend_select_pose_y[slot]; /* frontend row 14 = VDB line 14 */
    /* SCBITOBJ uses 3.5 fixed-point HSCALE/VSCALE, not a new ratio
     * computed from a rounded destination rectangle. The OP starts with
     * REMAINDER=VSCALE, consuming source lines whenever that accumulator
     * crosses zero. At output pixel n, the source index is floor(n*32/scale),
     * and the final partial repetition is still displayed. */
    w=(im->width*(int)scale+31)/32;
    /* Retail select_authored_object addresses 239 lines from each 320x240
     * JagPEG. The decoded final row is never read, including at half scale. */
    const int source_h=im->height>239?239:im->height;
    h=(source_h*(int)scale+31)/32;
    if(w<1||h<1)return;
    for(int y=0;y<h;y++){
        int dy=y0+y;
        if(dy<0||(unsigned)dy>=fb->height)continue;
        int sy=y*32/(int)scale;
        for(int x=0;x<w;x++){
            int dx=x0+x;
            if(dx<0||(unsigned)dx>=fb->width)continue;
            int sx=x*32/(int)scale;
            const u8 *q=im->rgb+((size_t)sy*(size_t)im->width+(size_t)sx)*3u;
            /* SCBITOBJ's 16-bit source pixel is quantized at the OP,
             * not retained as the host JagPEG decoder's RGB888 triple.
             * The later SGCF RGB16 cross already enforces this precision. */
            fb->pixels[(size_t)dy*fb->width+(unsigned)dx]=frontend_rgb16_to_host(
                avp_gpu_rgb16_pack(q[0],q[1],q[2]));
        }
    }
}

void avp_gpu_frontend_enter_mode(AvpAvpGpuState *state,unsigned mode)
{
    if(!state||state->frontend_mode==mode)return;
    state->frontend_page=0;
    memset(state->frontend_text,0,sizeof(state->frontend_text));
    memset(state->frontend_text_large,0,sizeof(state->frontend_text_large));
    memset(state->frontend_text_glyph_x,0,sizeof(state->frontend_text_glyph_x));
    memset(state->frontend_text_species,0,sizeof(state->frontend_text_species));
    memset(state->frontend_fame_score,0,sizeof(state->frontend_fame_score));
    memset(state->frontend_fame_prompt,0,sizeof(state->frontend_fame_prompt));
    state->frontend_fame_prompt_active=0u;
    state->frontend_fame_intro_tick=0u;
    state->frontend_fame_depart_active=0u;
    state->frontend_fame_depart_phase=16u;
    state->frontend_terminated_transitions=0u;
    state->frontend_terminated_fade_active=0u;
    state->frontend_terminated_fade_phase=16u;
    state->frontend_terminated_fade_direction=0u;
    state->frontend_fame_active=0;
    state->frontend_fame_editing=0u;
    state->frontend_fame_edit_name_valid=0u;
    memset(state->frontend_fame_edit_name,0,sizeof(state->frontend_fame_edit_name));
    state->frontend_fx_kind=0u;
    state->frontend_fx_active=0u;
    state->frontend_fx_phase=0u;
    state->frontend_select_cross_tick=0u;
    state->frontend_fx_from_role=0u;
    state->frontend_fx_to_role=0u;
    state->frontend_dim_active=0u;
    state->frontend_dim_phase=16u;
    state->frontend_transition_active=0;
    state->frontend_transition_polls=0;
    state->frontend_page_cross_active=0u;
    state->frontend_page_cross_phase=0u;
    state->frontend_picture_x=AVP_INTRO_PICTURE_X_CENTRE;
    state->frontend_scene_blank=0u;
    state->frontend_select_brief_active=0u;
    state->frontend_select_brief_count=0u;
    state->frontend_select_visible_chars=0u;
    state->frontend_select_total_chars=0u;
        state->frontend_select_loading_ticks=0u;
    state->frontend_select_fire_prompt_active=0u;
    state->frontend_select_fire_prompt[0]=0;
    state->frontend_select_status[0]=0;
    state->frontend_select_instructions[0][0]=0;
    state->frontend_select_instructions[1][0]=0;
    state->frontend_credits_scrolling=0u;
    state->frontend_credits_tick=0u;
    if(mode==0u){state->frontend_title_splash=0u;state->frontend_title_choice=0;state->frontend_title_save=0;
        state->frontend_title_reveal_tick=0u;state->frontend_title_reveal_active=0u;
        state->frontend_title_menu_dim_phase=6u;state->frontend_title_menu_pending=0u;
        state->frontend_title_depart_active=0u;state->frontend_title_depart_phase=16u;}
    if(mode==1u){state->frontend_selected_species=PT_ALIEN;state->frontend_select_detail=0;
        memset(state->frontend_select_pose_valid,0,sizeof(state->frontend_select_pose_valid));}
    state->frontend_mode=mode;
}

/* Retail BigPEmu 60-Hz recording (2026-10-21 15-37-18): the Marine
 * loading line is fully present by ~32s; PRESS FIRE appears at ~39.4s.
 * This PC presentation-only 7.5-second hold is calibrated to that capture,
 * not a reverse-engineered Jaguar loader instruction count. */
enum { AVP_SELECT_LOADING_HOLD_VBLS = 450 };

int avp_gpu_frontend_select_loading_ready(const AvpAvpGpuState *state)
{
    return state && state->frontend_mode==AVP_MJP_SELECT &&
           state->frontend_select_brief_active &&
           state->frontend_select_visible_chars>=state->frontend_select_total_chars &&
           state->frontend_select_loading_ticks>=AVP_SELECT_LOADING_HOLD_VBLS;
}

void avp_gpu_frontend_event(AvpAvpGpuState *state,unsigned event,s32 arg0,s32 arg1)
{
    if(!state)return;
    /* MAIN/MJP advances its object-presentation effects from One_Tick,
     * not from the number of host redraws. pc_front_event can repaint a
     * page several times in one VBL, which must not accelerate its fade. */
    if(event==0x5449434bu){ /* TICK, emitted once after each One_Tick */
        if(state->frontend_mode==AVP_MJP_FAME && state->frontend_fame_intro_tick<96u)
            ++state->frontend_fame_intro_tick;
        if(state->frontend_mode==AVP_MJP_SIM_TERMINATED &&
           state->frontend_terminated_fade_active){
            const unsigned old=state->frontend_terminated_fade_phase;
            if(state->frontend_terminated_fade_direction==1u){
                state->frontend_terminated_fade_phase=(unsigned char)
                    (old<14u?old+2u:16u);
                if(state->frontend_terminated_fade_phase==16u)
                    state->frontend_terminated_fade_active=0u;
            }else{
                state->frontend_terminated_fade_phase=(unsigned char)
                    (old>2u?old-2u:0u);
                if(state->frontend_terminated_fade_phase==0u)
                    state->frontend_terminated_fade_active=0u;
            }
        }
        /* The captured Jaguar SELECT briefing types out from top to bottom.
         * The PC receives cartridge text in one SELP resource callback, but
         * its display progresses one authored character per Jaguar CPU tick. */
        if(state->frontend_mode==AVP_MJP_SELECT &&
           state->frontend_select_brief_active){
            if(state->frontend_select_visible_chars<state->frontend_select_total_chars)
                ++state->frontend_select_visible_chars;
            else if(state->frontend_select_loading_ticks<AVP_SELECT_LOADING_HOLD_VBLS)
                ++state->frontend_select_loading_ticks;
        }
        if(state->frontend_mode==AVP_MJP_CREDITS && state->frontend_credits_scrolling && state->frontend_credits_tick<1600u)
            ++state->frontend_credits_tick;
        if(state->frontend_mode==AVP_MJP_TITLE){
            if(state->frontend_title_reveal_active && state->frontend_title_reveal_tick<96u)
                ++state->frontend_title_reveal_tick;
            if(!state->frontend_title_splash && state->frontend_title_menu_pending){
                if(state->frontend_title_menu_dim_phase>6u)
                    --state->frontend_title_menu_dim_phase;
                if(state->frontend_title_menu_dim_phase==6u)
                    state->frontend_title_menu_pending=0u;
            }
        }
        /* Scene CROSS stage and picture X come from the CPU's MJPX
         * event ($7FB4 progress), never from host tick counting. */
        if(state->frontend_fx_active){
            if(state->frontend_fx_kind==1u){
                /* The original 50-VBL CPU selection hold drives the
                 * card scale, NOT a 50-VBL portrait dissolve.  The retail
                 * recording completes the art transition within the first
                 * eight updated frames after the three-frame hold.  Keep
                 * presenting the moving card until the CPU's SGFI event. */
                if(state->frontend_select_cross_tick<50u)
                    ++state->frontend_select_cross_tick;
                {
                    const unsigned tick=state->frontend_select_cross_tick;
                    const unsigned blend=tick>3u?(tick-3u)*2u:0u;
                    state->frontend_fx_phase=(unsigned char)
                        (blend>16u?16u:blend);
                }
                if(state->frontend_select_cross_tick>=50u)
                    state->frontend_fx_active=0u;
            }else if(state->frontend_fx_kind==2u){
                if(state->frontend_fx_phase>7u)--state->frontend_fx_phase;
                else state->frontend_fx_active=0u;
                state->frontend_dim_phase=state->frontend_fx_phase;
            }
        }
        return;
    }
    if(event==0x4d4a5050u||event==0x4d4a5053u){(void)arg0;if(arg1>=0&&arg1<9)state->frontend_page=(unsigned)arg1;}
    if(event==0x4d4a5042u || event==0x434c5231u || event==0x434c5232u || event==0x43525047u){
        /* MJPB begins a scene; CLR1/CLR2 clear the old text surface when a
           retail page advances.  Keeping the previous line buffer here used
           to concatenate unrelated pages and draw the stale text again. */
        memset(state->frontend_text,0,sizeof(state->frontend_text));
        memset(state->frontend_text_large,0,sizeof(state->frontend_text_large));
        memset(state->frontend_text_glyph_x,0,sizeof(state->frontend_text_glyph_x));
        memset(state->frontend_text_species,0,sizeof(state->frontend_text_species));
        memset(state->frontend_text_x,0,sizeof(state->frontend_text_x));
        memset(state->frontend_fame_score,0,sizeof(state->frontend_fame_score));
        state->frontend_fame_active=0;
    }
    if(event==0x4d4a5042u){ /* MJPB: $01D740 scene start, before any $01DF2C */
        state->frontend_picture_x=AVP_INTRO_PICTURE_X_CENTRE;
        state->frontend_scene_blank=0u;
    }
    if(event==0x4d4a5050u){ /* MJPP: begin original eight-tick page transition. */
        /* The same-page LOSE transitions are NOT no-ops.  Do_Lose starts
         * and ends with one eight-VBL GPU presentation transition. */
        if(state->frontend_mode==AVP_MJP_SIM_TERMINATED){
            const unsigned outbound=state->frontend_terminated_transitions!=0u;
            if(state->frontend_terminated_transitions<2u)
                ++state->frontend_terminated_transitions;
            state->frontend_terminated_fade_active=1u;
            state->frontend_terminated_fade_direction=(unsigned char)(outbound?2u:1u);
            state->frontend_terminated_fade_phase=(unsigned char)(outbound?16u:0u);
        }
        unsigned from=(unsigned)arg0,to=(unsigned)arg1;
        const unsigned from_ok=from==AVP_MJP_PAGE_BLANK ||
            (from<9u && state->frontend_loaded[state->frontend_mode][from]);
        const unsigned to_ok=to==AVP_MJP_PAGE_BLANK ||
            (to<9u && state->frontend_loaded[state->frontend_mode][to]);
        state->frontend_page_cross_from=(unsigned char)(from<9u||from==AVP_MJP_PAGE_BLANK?from:0u);
        state->frontend_page_cross_to=(unsigned char)(to<9u||to==AVP_MJP_PAGE_BLANK?to:0u);
        state->frontend_page_cross_phase=0u;
        state->frontend_page_cross_active=(unsigned char)(
            (state->frontend_mode==AVP_MJP_INTRO || state->frontend_mode==AVP_MJP_ESCAPE) &&
            from!=to && from_ok && to_ok);
        /* First epoch shows the outgoing picture (Lister4 publishes $7FA4). */
        state->frontend_scene_blank=(unsigned char)(from==AVP_MJP_PAGE_BLANK);
        state->frontend_page=(unsigned char)(to<9u?to:(from<9u?from:0u));
    }
    if(event==0x4d4a5058u){ /* MJPX: CPU picture X and published CROSS stage. */
        state->frontend_picture_x=(int)arg0;
        if(state->frontend_page_cross_active)
            state->frontend_page_cross_phase=(unsigned char)(arg1<0?0:(arg1>16?16:arg1));
    }
    if(event==0x4d4a5053u){ /* MJPS: GPU completion ($7F80=2). */
        state->frontend_page_cross_active=0u;
        state->frontend_page_cross_phase=16u;
        state->frontend_scene_blank=(unsigned char)(
            (unsigned)arg1==AVP_MJP_PAGE_BLANK);
    }
    if(event==0x4d4a5700u){state->frontend_transition_phase=0u;state->frontend_transition_polls=0u;state->frontend_transition_active=1u;}
    if(event==0x4d4a5702u){state->frontend_transition_phase=1u;state->frontend_transition_polls=0u;state->frontend_transition_active=1u;}
    if(event==0x46494e53u){ /* FINS: selected ranking for the name editor */
        state->frontend_fame_edit_slot=(unsigned char)(arg1>=0&&arg1<5?arg1:0);
        state->frontend_fame_edit_cursor=0u;
        state->frontend_fame_edit_character='A';
        state->frontend_fame_editing=1u;
        state->frontend_fame_edit_name_valid=1u;
        memset(state->frontend_fame_edit_name,' ',sizeof(state->frontend_fame_edit_name));
    }
    if(event==0x464e4348u && state->frontend_fame_edit_name_valid){
        if(arg0>=0&&arg0<6&&arg1>=32&&arg1<127)
            state->frontend_fame_edit_name[(unsigned)arg0]=(unsigned char)arg1;
    }
    if(event==0x464e414du && state->frontend_fame_editing){
        if(arg0>=0&&arg0<6)state->frontend_fame_edit_cursor=(unsigned char)arg0;
        if(arg1>=32&&arg1<127)state->frontend_fame_edit_character=(unsigned char)arg1;
    }
    if(event==0x464e444eu)state->frontend_fame_editing=0u;
    /* Retail Fame_Tex has finished building the five display records when
     * Show_Tex begins authored $2A925. Its prompt must not be appended to
     * the first Fame score name or inherit the score's large species font. */
    if(event==0x4d4a5054u && state->frontend_mode==AVP_MJP_FAME &&
       (u32)arg0==0x0002A925u){ /* MJPT: source RDB text identifier */
        state->frontend_fame_prompt_active=1u;
        memset(state->frontend_fame_prompt,0,sizeof(state->frontend_fame_prompt));
    }
    if(event==0x464d4654u){state->frontend_fame_species=(unsigned char)(arg0<=2?arg0:1);state->frontend_fame_active=1u;} /* FMFT */
    if(event==0x464d5350u && arg0>=0 && arg0<12){
        state->frontend_text_large[(unsigned)arg0]=1u;
        state->frontend_text_species[(unsigned)arg0]=(unsigned char)(arg1<=2?arg1:1);
    } /* FMSP */
    if(event==0x43524247u){state->frontend_credits_tick=0u;state->frontend_credits_scrolling=1u;
        state->frontend_credits_y=0x12c;state->frontend_credits_line=0u;} /* CRBG */
    if(event==0x43525359u){ /* CRSY: retail credits object Y and data line */
        state->frontend_credits_y=(int)arg0;
        state->frontend_credits_line=arg1<0?0u:(unsigned)arg1;
    }
    if(event==0x4352454eu)state->frontend_credits_scrolling=0u; /* CREN */
    if(event==0x5449544cu){state->frontend_title_splash=1u;state->frontend_title_choice=0;state->frontend_title_save=0;
        state->frontend_title_reveal_tick=0u;state->frontend_title_reveal_active=1u;
        state->frontend_title_menu_dim_phase=16u;state->frontend_title_menu_pending=0u;}
    if(event==0x544d454eu){state->frontend_title_splash=0u;state->frontend_title_reveal_active=0u;
        state->frontend_title_menu_dim_phase=16u;state->frontend_title_menu_pending=1u;}
        /* TMEN starts dimming the picture before the option lettering appears. */
    if(event==0x46534644u){ /* FSFD: host-only Hall-of-Fame exit fade */
        unsigned phase=(unsigned)arg0;
        if(phase>16u)phase=16u;
        state->frontend_fame_depart_active=1u;
        state->frontend_fame_depart_phase=(unsigned char)phase;
    }
    if(event==0x54534644u){ /* TSFD: video-only title/selection transition */
        unsigned phase=(unsigned)arg0;
        if(phase>16u)phase=16u;
        state->frontend_title_depart_active=1u;
        state->frontend_title_depart_phase=(unsigned char)phase;
    }
    if(event==0x544d5550u||event==0x544d444eu){
        if(arg0>=0&&arg0<4)state->frontend_title_choice=(int)arg0;
        /* Early user navigation must remain responsive even while the
         * purely visual menu handoff is finishing. */
        state->frontend_title_menu_pending=0u;state->frontend_title_menu_dim_phase=6u;
    }
    if(event==0x53415645u){state->frontend_title_save=1;state->frontend_title_choice=0;
        state->frontend_title_menu_pending=0u;state->frontend_title_menu_dim_phase=6u;}
    if(event==0x43524544u || event==0x43524558u){
        state->frontend_title_save=0;
        if(event==0x43524558u){ /* Returning from credits is not a fresh title fade. */
            state->frontend_title_menu_pending=0u;
            state->frontend_title_menu_dim_phase=6u;
        }
    }
    if(event==0x53565854u){state->frontend_title_save=0;state->frontend_title_choice=0;} /* SVXT */
    if(event==0x534f424au){ /* SOBJ: MJP authored character-selection object */
        unsigned v=(unsigned)arg0,slot=v>>16;
        if((slot&1u)==0u&&slot<=4u){
            unsigned i=slot>>1;
            state->frontend_select_pose_species[i]=(int)(v&255u);
            state->frontend_select_pose_scale[i]=(int)((v>>8)&255u);
            state->frontend_select_pose_x[i]=(s16)((u32)arg1>>16);
            state->frontend_select_pose_y[i]=(s16)(u16)arg1;
            state->frontend_select_pose_valid[i]=1u;
        }
    }
    if(event==0x53474346u){
        state->frontend_select_brief_active=0u;
        state->frontend_select_brief_count=0u;
        state->frontend_select_visible_chars=0u;
        state->frontend_select_total_chars=0u;
        state->frontend_select_loading_ticks=0u;
        state->frontend_select_fire_prompt_active=0u;
        state->frontend_select_fire_prompt[0]=0;
        state->frontend_select_status[0]=0;
    }
    if(event==0x53454c45u){
        memset(state->frontend_select_pose_valid,0,sizeof(state->frontend_select_pose_valid));
        state->frontend_selected_species=PT_ALIEN;state->frontend_select_detail=0;
        state->frontend_fx_active=0u;state->frontend_dim_active=0u;state->frontend_dim_phase=16u;
        state->frontend_select_brief_active=0u;state->frontend_select_brief_count=0u;
        state->frontend_select_visible_chars=0u;state->frontend_select_total_chars=0u;
        state->frontend_select_loading_ticks=0u;
        state->frontend_select_fire_prompt_active=0u;
        state->frontend_select_fire_prompt[0]=0;
        state->frontend_select_status[0]=0;
    }
    if(event==0x53454c56u){if(arg0==PT_HUMAN||arg0==PT_ALIEN||arg0==PT_PREDATOR)state->frontend_selected_species=(int)arg0;}
    if(event==0x53474346u){
        if(arg0==PT_HUMAN||arg0==PT_ALIEN||arg0==PT_PREDATOR)state->frontend_selected_species=(int)arg0;
        /* Preserve the chosen front object's last published OP pose. The
         * other two objects are hidden by the original 68000 before SGCF. */
        /* Direct host selection can bypass carousel pose updates. In that
         * case use the existing full-screen cross, not an invented OP card
         * position. Normally the last authored SOBJ pose overrides this. */
        state->frontend_select_zoom_x=0;
        state->frontend_select_zoom_y=0;
        state->frontend_select_zoom_w=320;
        state->frontend_select_zoom_h=239;
        for(unsigned i=0u;i<3u;i++){
            if(!state->frontend_select_pose_valid[i]||
               state->frontend_select_pose_species[i]!=state->frontend_selected_species)continue;
            unsigned scale=(unsigned)state->frontend_select_pose_scale[i];
            if(scale==0u||scale>32u)continue;
            state->frontend_select_zoom_x=state->frontend_select_pose_x[i];
            state->frontend_select_zoom_y=state->frontend_select_pose_y[i];
            /* Match the SCBITOBJ source-line stepping used for the last
             * displayed portrait frame (including its partial final line). */
            state->frontend_select_zoom_w=(int)((320u*scale+31u)/32u);
            state->frontend_select_zoom_h=(int)((239u*scale+31u)/32u);
            break;
        }
        state->frontend_select_detail=1;
        state->frontend_fx_kind=1u;
        state->frontend_fx_active=1u;
        state->frontend_fx_phase=0u;
        state->frontend_select_cross_tick=0u;
        state->frontend_fx_from_role=(unsigned char)frontend_normal_role(state->frontend_selected_species);
        state->frontend_fx_to_role=(unsigned char)frontend_detail_role(state->frontend_selected_species);
        state->frontend_dim_active=0u;state->frontend_dim_phase=16u;
    }
    if(event==0x53474649u){
        if(arg0==PT_HUMAN||arg0==PT_ALIEN||arg0==PT_PREDATOR)state->frontend_selected_species=(int)arg0;
        state->frontend_select_detail=1;
        state->frontend_fx_kind=2u;
        state->frontend_select_cross_tick=50u;
        state->frontend_fx_active=1u;
        state->frontend_fx_phase=16u;
        state->frontend_fx_from_role=(unsigned char)frontend_detail_role(state->frontend_selected_species);
        state->frontend_fx_to_role=state->frontend_fx_from_role;
        state->frontend_dim_active=1u;state->frontend_dim_phase=16u;
    }
    if(event==0x53454e44u){state->frontend_select_detail=0;state->frontend_fx_active=0u;state->frontend_dim_active=0u;}
}

void avp_gpu_frontend_draw_text(AvpAvpGpuState *state,unsigned line,const char *text,unsigned x)
{
    size_t have,cap,n;
    if(!state||!text||line>=12u)return;
    if(state->frontend_mode==AVP_MJP_FAME && state->frontend_fame_prompt_active){
        /* Show_Tex sends individual authored glyphs (and the final blank
         * cursor cell) to this callback. Store them separately from the
         * five Fame_Tex entries; original x/timing still come from MJP. */
        if(line==0u){
            size_t used=strlen(state->frontend_fame_prompt);
            size_t cap=sizeof(state->frontend_fame_prompt)-1u;
            while(*text && used<cap){
                const char ch=*text++;
                state->frontend_fame_prompt[used++]=ch=='_'?' ':ch;
            }
            state->frontend_fame_prompt[used]=0;
        }
        return;
    }
    /* Fame_Tex draws the ten score digits at x=128, not immediately after
     * the name printed at x=45. The CPU sends separate draw_text callbacks. */
    if(line<5u&&state->frontend_text_large[line]&&x>=100u){
        size_t nscore=strlen(text);
        if(nscore>=sizeof(state->frontend_fame_score[line]))
            nscore=sizeof(state->frontend_fame_score[line])-1u;
        memcpy(state->frontend_fame_score[line],text,nscore);
        state->frontend_fame_score[line][nscore]=0;
        return;
    }
    have=strlen(state->frontend_text[line]);cap=sizeof(state->frontend_text[line])-1u;
    if(have>=cap)return;
    n=strlen(text);
    if(n>cap-have)n=cap-have;
    /* Show_Tex supplies one character at a time with its own x coordinate.
       Our line buffer represents the whole line, so its origin is the FIRST
       glyph, not the last one.  The former assignment moved the entire line
       farther right each time a letter arrived. */
    if(have==0u)state->frontend_text_x[line]=x;
    memcpy(state->frontend_text[line]+have,text,n);
    /* MJP Show_Tex sends a one-character callback at its exact authored X;
       in particular @ advances by only 3px, not the usual 7px.  Keep that
       position instead of inventing uniform spacing from the first glyph. */
    for(size_t i=0u;i<n;i++){
        const unsigned at=x+(unsigned)(i*(state->frontend_text_large[line]?11u:7u));
        state->frontend_text_glyph_x[line][have+i]=(unsigned short)(at>65535u?65535u:at);
    }
    state->frontend_text[line][have+n]=0;
    if(state->frontend_fame_active){
        state->frontend_text_large[line]=1u;
        state->frontend_text_species[line]=state->frontend_fame_species;
    }
}

/* The 68000's SELP and SELT calls describe retail text resources rather
 * than gameplay state. Keep their multi-NUL RDB strings in a separate PC
 * presentation plane: no changes to Show_Tex or selection animation. */
void avp_gpu_frontend_select_instructions(AvpAvpGpuState *state,const char *rotate,const char *fire)
{
    const char *p[2]={rotate,fire};
    if(!state)return;
    memset(state->frontend_select_instructions,0,sizeof(state->frontend_select_instructions));
    for(unsigned i=0;i<2u;i++)if(p[i]){
        size_t n=0u;
        while(n+1u<sizeof(state->frontend_select_instructions[i])&&p[i][n])++n;
        if(!p[i][n])memcpy(state->frontend_select_instructions[i],p[i],n);
    }
}
void avp_gpu_frontend_select_briefing(AvpAvpGpuState *state,const char *lines,const char *status)
{
    const char *p=lines;
    if(!state)return;
    memset(state->frontend_select_brief,0,sizeof(state->frontend_select_brief));
    state->frontend_select_brief_count=0u;
    state->frontend_select_brief_active=0u;
    state->frontend_select_visible_chars=0u;
    state->frontend_select_total_chars=0u;
        state->frontend_select_loading_ticks=0u;
    state->frontend_select_fire_prompt_active=0u;
    state->frontend_select_fire_prompt[0]=0;
    if(p)for(unsigned i=0u;i<7u;i++){
        size_t n=0u;
        while(n+1u<sizeof(state->frontend_select_brief[i]) && p[n])++n;
        if(!n || p[n])break; /* malformed/overlong source; do not overread */
        memcpy(state->frontend_select_brief[i],p,n);
        state->frontend_select_brief_count++;
        state->frontend_select_total_chars+=(unsigned short)n;
        p+=n+1u;
    }
    state->frontend_select_brief_active=1u;
    /* MJP emits SELP only after the selected character's SGCF/SGFI jobs.
     * The original retail briefing shows the matching detail bitmap
     * (FILES 213/215/209), not the earlier carousel card (214/206/210).
     * Reassert that completed handoff so stale selection-card state cannot
     * become visible behind the briefing and PRESS FIRE text. */
    state->frontend_select_detail=1;
    state->frontend_select_status[0]=0;
    if(status){
        size_t len=strlen(status);
        if(len<sizeof(state->frontend_select_status))
            memcpy(state->frontend_select_status,status,len+1u);
    }
}
void avp_gpu_frontend_select_prompt(AvpAvpGpuState *state,const char *prompt)
{
    size_t n=0u;
    if(!state)return;
    /* SELP sets LOADING SIMULATION; SELT delivers PRESS FIRE later. These
     * occupy the same bottom line in the BigPEmu cartridge recording. Do
     * not erase the first phrase when the second callback arrives. */
    state->frontend_select_fire_prompt[0]=0;
    state->frontend_select_fire_prompt_active=0u;
    if(!prompt)return;
    while(n+1u<sizeof(state->frontend_select_fire_prompt) && prompt[n])++n;
    if(prompt[n])return;
    memcpy(state->frontend_select_fire_prompt,prompt,n);
    state->frontend_select_fire_prompt[n]=0;
    state->frontend_select_fire_prompt_active=1u;
}

void avp_gpu_frontend_present(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,unsigned mode,s32 choice)
{
    const AvpGpuRgbImage *im=NULL;unsigned role=0;
    if(!state||!fb||!fb->pixels)return;
    if(mode==1u){
        int species=state->frontend_selected_species;
        if(species!=PT_HUMAN&&species!=PT_ALIEN&&species!=PT_PREDATOR)
            species=(choice==4)?PT_ALIEN:((choice==8)?PT_PREDATOR:PT_HUMAN);
        if(state->frontend_select_detail)role=frontend_detail_role(species);
        else role=frontend_normal_role(species);
        if(state->frontend_loaded[mode][role])im=&state->frontend_img[mode][role];
    }else if(mode==2u||mode==4u)role=state->frontend_page<9u?state->frontend_page:0u;
    else if(mode==3u)role=state->frontend_loaded[mode][2]?2u:0u;
    if(!im&&mode<10u&&role<9u&&state->frontend_loaded[mode][role])im=&state->frontend_img[mode][role];
    if(!im&&mode<10u)for(role=0;role<9u;role++)if(state->frontend_loaded[mode][role]){im=&state->frontend_img[mode][role];break;}

    memset(fb->pixels,0,(size_t)fb->width*fb->height*sizeof(*fb->pixels));
    if(mode==AVP_MJP_CREDITS){
        /* Exactly the same settled title base image as the menu, not black. */
        if(state->frontend_loaded[AVP_MJP_TITLE][0u])
            frontend_blit_image(fb,&state->frontend_img[AVP_MJP_TITLE][0u],1);
    }else if(state->frontend_page_cross_active &&
       (mode==AVP_MJP_INTRO||mode==AVP_MJP_ESCAPE)){
        const unsigned fr=state->frontend_page_cross_from,tr=state->frontend_page_cross_to;
        const AvpGpuRgbImage *from=fr<9u?&state->frontend_img[mode][fr]:NULL;
        const AvpGpuRgbImage *to=tr<9u?&state->frontend_img[mode][tr]:NULL;
        frontend_blit_scene_cross(fb,from,to,mode,state->frontend_picture_x,
                                  state->frontend_page_cross_phase);
    }else if(mode==AVP_MJP_SELECT&&!state->frontend_select_detail){
        /* The original OP list publishes the three carousel objects as a
         * complete selection frame. Do not fall through to a full-screen
         * decoded portrait while SELE has no SOBJ positions, or present a
         * partially populated list as the three SOBJ events arrive. */
        if(state->frontend_select_pose_valid[0] &&
           state->frontend_select_pose_valid[1] &&
           state->frontend_select_pose_valid[2])
            for(unsigned slot=0u;slot<3u;slot++)frontend_blit_select_object(state,fb,slot);
    }else if(mode==1u&&state->frontend_fx_active&&state->frontend_fx_kind==1u&&
       state->frontend_fx_from_role<9u&&state->frontend_fx_to_role<9u&&
       state->frontend_loaded[mode][state->frontend_fx_from_role]&&
       state->frontend_loaded[mode][state->frontend_fx_to_role]){
        frontend_blit_select_zoom_cross(fb,
            &state->frontend_img[mode][state->frontend_fx_from_role],
            &state->frontend_img[mode][state->frontend_fx_to_role],
            state,state->frontend_fx_phase);
    }else if(mode==1u&&im&&(state->frontend_fx_active&&state->frontend_fx_kind==2u)){
        frontend_blit_fade(fb,im,state->frontend_fx_phase);
    }else if(mode==1u&&im&&state->frontend_dim_active){
        frontend_blit_fade(fb,im,state->frontend_dim_phase);
    }else if(im && state->frontend_scene_blank &&
             (mode==AVP_MJP_INTRO||mode==AVP_MJP_ESCAPE)){
        /* Picture object currently shows the cleared $4BE18 buffer. */
    }else if(im){
        if(frontend_native_mjp_picture(mode,im))
            frontend_blit_native_mjp_picture(fb,im,state->frontend_picture_x);
        else frontend_blit_image(fb,im,mode==AVP_MJP_TITLE);
    }

    /* Recorded Jaguar Hall of Fame: the engraved cartridge backdrop fades
     * in first.  The five large-colour rows rise into their score slots
     * afterward; they are not pre-painted at their final locations.  Both
     * motions are video presentation only, keyed to original One_Tick. */
    if(mode==AVP_MJP_FAME && state->frontend_fame_intro_tick<48u){
        const unsigned phase=(state->frontend_fame_intro_tick*16u)/48u;
        for(size_t i=0u,n=(size_t)fb->width*fb->height;i<n;i++){
            const u32 c=fb->pixels[i];
            const u16 p=avp_gpu_rgb16_pack((u8)(c>>16),(u8)(c>>8),(u8)c);
            fb->pixels[i]=frontend_rgb16_to_host(avp_gpu_rgb16_fraction(p,phase));
        }
    }

    if(mode==AVP_MJP_TITLE){
        const unsigned tick=state->frontend_title_splash?state->frontend_title_reveal_tick:96u;
        const unsigned bg=state->frontend_title_splash?title_reveal_phase(tick,0u,32u):16u;
        /* Render original background/independent logo objects before fade:
         * the menu continues to use precisely this completed title artwork. */
        if(bg<16u)for(size_t i=0u,n=(size_t)fb->width*fb->height;i<n;i++){
            const uint32_t c=fb->pixels[i];
            const u16 packed=avp_gpu_rgb16_pack((u8)(c>>16),
                (u8)(c>>8),(u8)c);
            fb->pixels[i]=frontend_rgb16_to_host(avp_gpu_rgb16_fraction(packed,bg));
        }
        const int stamped=tick>=AVP_TITLE_PREDATOR_STAMP_TICK &&
                          title_predator_stamp(state,fb);
        const int alien_stamped=tick>=AVP_TITLE_ALIEN_STAMP_TICK &&
                          title_alien_stamp(state,fb);
        if(tick>=1u && state->frontend_loaded[0u][3u] &&
           !(alien_stamped && tick>AVP_TITLE_ALIEN_STAMP_TICK)){
            const AvpTitleOpPose pose=frontend_title_alien_pose(tick);
            frontend_blit_title_sprite(fb,&state->frontend_img[0u][3u],
                                       pose.x,pose.y,pose.width,pose.height,110u,pose.scale,16u);
        }
        if(tick>=33u && state->frontend_loaded[0u][2u] &&
           !(stamped && tick>AVP_TITLE_PREDATOR_STAMP_TICK)){ /* hidden one VBL after the stamp */
            const AvpTitleOpPose pose=frontend_title_predator_pose(
                frontend_title_predator_phase(tick));
            frontend_blit_title_sprite(fb,&state->frontend_img[0u][2u],
                                       pose.x,pose.y,pose.width,pose.height,82u,pose.scale,16u);
        }
        if(state->frontend_title_splash){
            /* Copyright is a distinct final title phase, not part of
             * the SELECT/RESTORE/HIGH SCORES/CREDITS menu text. */
            if(tick>=83u)title_legal_notice(state,fb);
        }else{
            /* Retail dim (RE24-S10, verified on the reference machine):
             * three cumulative in-place GPU passes 15/16, 13/16, 11/16.
             * The phase counter runs 16 down to 6 (10 fields). */
            const unsigned phase=state->frontend_title_menu_dim_phase;
            const unsigned passes=avp_gpu_title_menu_dim_passes(phase<16u?16u-phase:0u);
            if(passes)for(size_t i=0u,n=(size_t)fb->width*fb->height;i<n;i++){
                const uint32_t c=fb->pixels[i];
                const u16 packed=avp_gpu_rgb16_pack((u8)(c>>16),
                    (u8)(c>>8),(u8)c);
                fb->pixels[i]=frontend_rgb16_to_host(avp_gpu_rgb16_title_menu_dim(packed,passes));
            }
        }
    }

    if(mode==AVP_MJP_CREDITS){
        /* The native credits appear over the finished, dim title composition;
         * they are not rendered by Do_Intro's story-text/typewriter plane. */
        if(state->frontend_loaded[AVP_MJP_TITLE][3u]){
            const AvpTitleOpPose p=frontend_title_alien_pose(96u);
            if(!title_alien_stamp(state,fb))
                frontend_blit_title_sprite(fb,&state->frontend_img[AVP_MJP_TITLE][3u],
                                           p.x,p.y,p.width,p.height,110u,p.scale,16u);
        }
        if(!title_predator_stamp(state,fb) && state->frontend_loaded[AVP_MJP_TITLE][2u]){
            const AvpTitleOpPose p=frontend_title_predator_pose(
                frontend_title_predator_phase(96u));
            frontend_blit_title_sprite(fb,&state->frontend_img[AVP_MJP_TITLE][2u],
                                       p.x,p.y,p.width,p.height,82u,p.scale,16u);
        }
        /* Credits scroll over the retained, already dimmed title bitmap:
         * the retail three-pass in-place dim (RE24-S10, reference machine:
         * every non-text pixel of the credits frame matches). */
        for(size_t i=0u,n=(size_t)fb->width*fb->height;i<n;i++){
            const u32 c=fb->pixels[i];
            const u16 p=avp_gpu_rgb16_pack((u8)(c>>16),(u8)(c>>8),(u8)c);
            fb->pixels[i]=frontend_rgb16_to_host(avp_gpu_rgb16_title_menu_dim(p,3u));
        }
        if(state->frontend_credits_scrolling && state->frontend_loaded[AVP_MJP_CREDITS][0] &&
           state->frontend_img[AVP_MJP_CREDITS][0].width==320 &&
           state->frontend_img[AVP_MJP_CREDITS][0].height==1373){
            /* Retail 280-line ($118) 4-bpp object at Y=$4BE28+y (port row
             * 14+y); lines past FILES 238 are the blitter-cleared tail. */
            const AvpGpuRgbImage *im=&state->frontend_img[AVP_MJP_CREDITS][0];
            for(unsigned r=0u;r<0x118u;r++){
                const unsigned src=state->frontend_credits_line+r;
                const int py=14+state->frontend_credits_y+(int)r;
                if(src>=1373u)break;
                if(py<0||(unsigned)py>=fb->height)continue;
                for(unsigned x=0u;x<320u&&x<fb->width;x++){
                    const u8 *q=im->rgb+((size_t)src*320u+x)*3u;
                    const u16 w=avp_gpu_rgb16_pack(q[0],q[1],q[2]);
                    if(w)fb->pixels[(size_t)py*fb->width+x]=frontend_rgb16_to_host(w);
                }
            }
        }else if(state->frontend_credits_scrolling){
            /* No ROM credits bitmap (synthetic tests): legacy text plane. */
            const int shift=(int)(state->frontend_credits_tick*5u/6u);
            const unsigned count=(unsigned)(sizeof(retail_credit_lines)/sizeof(retail_credit_lines[0]));
            for(unsigned row=0u;row<count;row++){
                const char *line=retail_credit_lines[row];
                const int y=(int)fb->height+((int)row*13)-shift;
                if(y<=-13 || y>=(int)fb->height || !*line)continue;
                const int x=((int)fb->width-(int)strlen(line)*7)/2;
                frontend_credit_text(state,fb,x,y,line);
            }
        }
    }

    if(mode==AVP_MJP_SELECT && !state->frontend_select_brief_active &&
       !state->frontend_select_detail &&
       state->frontend_select_pose_valid[0] &&
       state->frontend_select_pose_valid[1] &&
       state->frontend_select_pose_valid[2]){
        /* Retail $01E952: Show_Tex X=$3E, Y=$BE, line step $F ($01E86E);
         * the authored strings' leading spaces place each line. */
        for(unsigned i=0u;i<2u;i++)if(state->frontend_select_instructions[i][0])
            frontend_font_text(state,fb,0,0x3e,AVP_MJP_BUFFER_Y(0xbe)+(int)i*15,
                               state->frontend_select_instructions[i]);
    }
    if(mode==AVP_MJP_SELECT && state->frontend_select_brief_active){
        /* BigPEmu retail capture: a common left margin, 13px baseline
         * advance, and a separate lower status line. Each callback carries
         * the whole RDB record; only the visible prefix is presented. */
        unsigned remaining=state->frontend_select_visible_chars;
        for(unsigned i=0u;i<state->frontend_select_brief_count;i++){
            const char *line=state->frontend_select_brief[i];
            const size_t len=strlen(line);
            const size_t shown=remaining<len?remaining:len;
            if(shown){
                char partial[sizeof(state->frontend_select_brief[0])];
                memcpy(partial,line,shown);
                partial[shown]=0;
                /* Retail $01EDB4: X=$28, Y=$52, step $F (BigPEmu rows 83+15n). */
                frontend_font_text(state,fb,0,0x28,AVP_MJP_BUFFER_Y(0x52)+(int)i*15,partial);
            }
            remaining-= (unsigned)shown;
        }
        if(state->frontend_select_visible_chars>=state->frontend_select_total_chars){
            /* Retail $01EE4C: LOADING SIMULATION... at X=$28,Y=$C4; $01EF10:
             * PRESS FIRE is a separate Show_Tex at X=$C2 on the same row. */
            if(state->frontend_select_status[0])
                frontend_font_text(state,fb,0,0x28,AVP_MJP_BUFFER_Y(0xc4),state->frontend_select_status);
            if(state->frontend_select_fire_prompt_active && state->frontend_select_fire_prompt[0])
                frontend_font_text(state,fb,0,0xc2,AVP_MJP_BUFFER_Y(0xc4),state->frontend_select_fire_prompt);
        }
    }
    if(mode==AVP_MJP_TITLE && !state->frontend_title_splash &&
       !state->frontend_title_menu_pending){
        /* Source-authored title labels, 11px placement, X=$28 for both the
           strings and the "a*" cursor ($01F282/$01F3DE), Y=$62+$19*n.
           The leading species marker selects the band, it is not printed. */
        static const char *items[4]={
            "a  SELECT CHARACTER","a  RESTORE GAME","a  HIGH SCORES","a  CREDITS"
        };
        if(!state->frontend_title_save){
            int i;for(i=0;i<4;i++){
                int yy=AVP_MJP_BUFFER_Y(0x62)+i*25;
                if(i==state->frontend_title_choice)title_menu_text(state,fb,40,yy,"a*");
                title_menu_text(state,fb,40,yy,items[i]);
            }
        }else{
            /* Exact authored restore-menu labels recovered from retail RDB.DTA.
               Slot availability remains CPU-owned (EncodeGa/mjp_games). */
            static const char *slots[3]={"a  RESTORE GAME 1","a  RESTORE GAME 2","a  RESTORE GAME 3"};
            int i;
            for(i=0;i<3;i++){
                int yy=AVP_MJP_BUFFER_Y(0x62)+i*25;
                if(i==state->frontend_title_choice)title_menu_text(state,fb,40,yy,"a*");
                if(mjp_games[i]=='m'||mjp_games[i]=='p'||mjp_games[i]=='a'){
                    /* Available slot uses its own source species band. */
                    fame_font_text(state,fb,40,yy,
                        fame_band_from_marker((unsigned char)mjp_games[i]),slots[i]+1);
                }else{
                    /* Retail $01F2AC: RDB $2B7B2 "a  ( NO GAME )". */
                    title_menu_text(state,fb,40,yy,"a  ( NO GAME )");
                }
            }
            if(state->frontend_title_choice==3)title_menu_text(state,fb,40,AVP_MJP_BUFFER_Y(0x62+3*0x19),"a*");
            title_menu_text(state,fb,40,AVP_MJP_BUFFER_Y(0x62+3*0x19),"a  CANCEL");
        }
    }
    for(unsigned l=0;l<12u;l++)if(state->frontend_text[l][0]){
        /* Original cartridge $2AFC3 already contains the blank authored
         * rows that place SIMULATION TERMINATED near the top of the image.
         * Do not apply an extra hosted vertical offset here; the generic
         * 14-pixel text grid plus the record's own blank rows match the
         * retail reference more closely than the earlier special case. */
        int y=14+(int)l*14;
        /* Do_Intro ($01D84C/$01D856) and Do_Escap ($01DB92/$01DB9C) start
         * the Show_Tex cursor at X=$18, Y=$A6 of the screen buffer, with
         * the $4BEA8=$E line step: the story text sits BELOW the 192x144
         * picture (screen row $A6+14=180; BigPEmu intro capture agrees). */
        if(mode==AVP_MJP_INTRO||mode==AVP_MJP_ESCAPE)y=AVP_MJP_STORY_TEXT_Y+(int)l*14;
        if(state->frontend_text_large[l]){
            unsigned code=state->frontend_text_species[l];
            unsigned band=(code==0u)?0u:((code==2u)?2u:4u);
            if(mode==AVP_MJP_FAME && l<5u){
                /* BigPEmu retail frame at 48s: numbered rows at x45,
                 * species names at x79, right-aligned scores ending x278,
                 * rows every 24 pixels starting y46. Those are Jaguar
                 * picture-space coordinates, never host window pixels. */
                char rank[3]={(char)('1'+l),'.',0};
                /* Retail recorded score rows scroll at a steady 3 native
                 * pixels per VBlank, not a quadratic ease-out.  At VBlank 0
                 * they are below the 240-line screen; they settle by 80.
                 * Keep this independent of the prompt's existing gate. */
                const int slide=frontend_fame_row_slide(state->frontend_fame_intro_tick);
                y=AVP_MJP_BUFFER_Y(0x2e)+(int)l*24+slide;
                fame_font_text(state,fb,45,y,band,rank);
                fame_font_text(state,fb,79,y,band,state->frontend_text[l]);
                if(state->frontend_fame_score[l][0]){
                    const char *digits=state->frontend_fame_score[l];
                    while(*digits==' ')++digits;
                    const int n=(int)strlen(digits);
                    fame_font_text(state,fb,278-n*11,y,band,digits);
                }
            }else{
                fame_font_text(state,fb,(int)state->frontend_text_x[l],y,band,state->frontend_text[l]);
                if(l<5u&&state->frontend_fame_score[l][0])
                    fame_font_text(state,fb,128,y,band,state->frontend_fame_score[l]);
            }
        }else if(mode==AVP_MJP_CREDITS){
            const char *line=state->frontend_text[l];
            int x=((int)fb->width-(int)strlen(line)*7)/2;
            frontend_font_text(state,fb,0,x,y,line);
        }else{
            /* Each original Show_Tex glyph has its own CPU-provided X.
               The old whole-string rendering lost half-width @ advances. */
            for(size_t i=0u;state->frontend_text[l][i];i++){
                frontend_font_glyph(fb,state->mjp_font,state->mjp_font_size,
                    224,39,7,13,
                    (int)state->frontend_text_glyph_x[l][i],y,
                    (unsigned char)state->frontend_text[l][i]);
            }
        }
    }
    /* Fade the entire finished title/menu frame using retail RGB16 fraction
     * arithmetic; this is PC-only rendering of the reference black handoff. */
    if(mode==AVP_MJP_TITLE && state->frontend_title_depart_active &&
       state->frontend_title_depart_phase<16u){
        const unsigned phase=state->frontend_title_depart_phase;
        for(size_t i=0u,n=(size_t)fb->width*(size_t)fb->height;i<n;++i){
            const uint32_t c=fb->pixels[i];
            u16 packed=avp_gpu_rgb16_pack((u8)(c>>16),(u8)(c>>8),(u8)c);
            fb->pixels[i]=frontend_rgb16_to_host(avp_gpu_rgb16_fraction(packed,phase));
        }
    }
    if(mode==AVP_MJP_FAME && state->frontend_fame_intro_tick>=96u &&
       state->frontend_fame_prompt_active && state->frontend_fame_prompt[0]){
        /* Retail $01C786: Show_Tex at X=$2D, Y=$BA of the screen buffer;
         * the record's own eleven leading spaces place "Press FIRE". */
        frontend_fame_prompt_text(state,fb,0x2d,AVP_MJP_BUFFER_Y(0xba),
                                  state->frontend_fame_prompt);
    }
    if(mode==AVP_MJP_FAME && state->frontend_fame_edit_name_valid){
        const unsigned row=state->frontend_fame_edit_slot;
        const unsigned code=state->frontend_text_species[row];
        const unsigned band=(code==0u)?0u:((code==2u)?2u:4u);
        for(unsigned col=0u;col<6u;col++){
            unsigned char ch=state->frontend_fame_edit_name[col];
            if(state->frontend_fame_editing&&col==state->frontend_fame_edit_cursor)
                ch=state->frontend_fame_edit_character;
            const int slide=frontend_fame_row_slide(state->frontend_fame_intro_tick);
            fame_font_glyph(state,fb,79+(int)col*11,46+(int)row*24+slide,band,ch);
        }
    }
    /* The recorded Hall of Fame fades its *completed* screen, including
       names, scores and PRESS FIRE, before the new title presentation. */
    if(mode==AVP_MJP_FAME && state->frontend_fame_depart_active &&
       state->frontend_fame_depart_phase<16u){
        const unsigned phase=state->frontend_fame_depart_phase;
        for(size_t i=0u,n=(size_t)fb->width*fb->height;i<n;i++){
            const u32 c=fb->pixels[i];
            const u16 p=avp_gpu_rgb16_pack((u8)(c>>16),(u8)(c>>8),(u8)c);
            fb->pixels[i]=frontend_rgb16_to_host(avp_gpu_rgb16_fraction(p,phase));
        }
    }
    if(mode==AVP_MJP_SIM_TERMINATED &&
       state->frontend_terminated_fade_phase<16u){
        const unsigned phase=state->frontend_terminated_fade_phase;
        for(size_t i=0u,n=(size_t)fb->width*fb->height;i<n;i++){
            const u32 c=fb->pixels[i];
            const u16 p=avp_gpu_rgb16_pack((u8)(c>>16),(u8)(c>>8),(u8)c);
            fb->pixels[i]=frontend_rgb16_to_host(avp_gpu_rgb16_fraction(p,phase));
        }
    }
}

int avp_gpu_frontend_transition_done(AvpAvpGpuState *state,unsigned phase)
{
    /* Retail Do_Win spins once per VBL until the OP status word reaches 2.
       The recovered hosted oracle reaches that completion after 17 status
       handshakes in each direction.  The CPU loop remains in mjp.c; this
       Jaguar-side seam owns only the hardware completion state. */
    if(!state)return 1;
    if(!state->frontend_transition_active || state->frontend_transition_phase!=phase){
        state->frontend_transition_phase=phase;
        state->frontend_transition_polls=0u;
        state->frontend_transition_active=1u;
    }
    if(state->frontend_transition_polls<17u)++state->frontend_transition_polls;
    if(state->frontend_transition_polls<17u)return 0;
    state->frontend_transition_active=0u;
    return 1;
}
