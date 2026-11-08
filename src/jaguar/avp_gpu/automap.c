/* GPU_AUTOMAP exploration is a state update at the simulation boundary.
   Its 64x64 observed-cell bitmap is visual state only, independent of the
   interpolated viewport refresh rate and never used by collision/AI. */
#include "jaguar/avp_gpu/state.h"
#include <string.h>

static void rasterize_mask(const u8 *mask,const u8 *maze,u16 mw,u16 mh,
                           u8 pixels[AVP_AUTOMAP_RASTER_BYTES]);
static int wall_visible_code(u8 wall)
{
    /* MAZE5.GAS calls check_left/top/right/bottom and consults
       DOORS.S::fullbits: only SOLID (types 0..3) and DOOR (4..11)
       stop the trace.  Transparent/edge geometry 12..15 must not
       truncate map discovery.  The zero-numbered panel is empty. */
    return wall!=0u && ((wall>>3)&15u)<12u;
}
static int automap_dir_blocked(const u8 *maze,u16 mw,u16 mh,int x,int y,unsigned dir)
{
    const u8 *c;
    u8 wall;
    if(!maze||x<0||y<0||x>=(int)mw||y>=(int)mh)return 1;
    c=maze+((size_t)y*mw+(size_t)x)*8u;
    wall=c[dir&3u];
    return wall_visible_code(wall);
}

static int automap_step(int *x,int *y,unsigned dir,u16 mw,u16 mh)
{
    int nx=*x,ny=*y;
    if(dir==0u)--nx;
    else if(dir==1u)--ny;
    else if(dir==2u)++nx;
    else ++ny;
    if(nx<0||ny<0||nx>=(int)mw||ny>=(int)mh)return 0;
    *x=nx;*y=ny;return 1;
}

static void automap_mark(AvpAvpGpuState *state,int x,int y,u16 mw,u16 mh)
{
    if(!state||x<0||y<0||x>=(int)mw||y>=(int)mh||x>=64||y>=64)return;
    state->automap_seen[(unsigned)y*64u+(unsigned)x]=1u;
    state->automap_touched[(unsigned)y*64u+(unsigned)x]=1u;
}

/* GPU_AUTOMAP traces four squares along the view-facing cardinal corridor and
 * up to three squares down each visible side branch.  It accumulates into the
 * persistent one-bit map buffer until ResetMap/level load clears it.
 * The trace's touched cells are retained separately from prior discoveries. */
static void automap_reveal_side(AvpAvpGpuState *state,const u8 *maze,u16 mw,u16 mh,
                                int x,int y,unsigned dir)
{
    unsigned n;
    for(n=0;n<3u;n++){
        automap_mark(state,x,y,mw,mh);
        if(automap_dir_blocked(maze,mw,mh,x,y,dir))break;
        if(!automap_step(&x,&y,dir,mw,mh))break;
        automap_mark(state,x,y,mw,mh);
    }
}
static void automap_reveal(AvpAvpGpuState *state,const u8 *maze,u16 mw,u16 mh,
                           int px,int py,unsigned dir,unsigned logical_level)
{
    unsigned left,right,n;
    int x=px,y=py;
    if(!state||!maze||mw==0||mh==0)return;
    if(px<0||py<0||px>=(int)mw||py>=(int)mh)return;
    if(state->automap_level!=logical_level){
        avp_gpu_automap_reset(state);
        state->automap_level=logical_level;
    }
    left=(dir+3u)&3u; right=(dir+1u)&3u;
    for(n=0;n<4u;n++){
        automap_mark(state,x,y,mw,mh);
        if(!automap_dir_blocked(maze,mw,mh,x,y,left))
            automap_reveal_side(state,maze,mw,mh,x,y,left);
        if(!automap_dir_blocked(maze,mw,mh,x,y,right))
            automap_reveal_side(state,maze,mw,mh,x,y,right);
        if(automap_dir_blocked(maze,mw,mh,x,y,dir))break;
        if(!automap_step(&x,&y,dir,mw,mh))break;
    }
}


void avp_gpu_automap_observe(AvpAvpGpuState *state,const u8 *maze,u16 mw,u16 mh,
                              u32 xpos,u32 ypos,u32 angle,unsigned logical_level)
{
    /* MAZE5.GAS::automapping performs a logical >>29 of the full
       32-bit centre angle, rounds the octant, clears bit 2, and
       dispatches map_jumps={right,up,left,down}.  No trigonometric
       approximation or floating-point tie handling is involved. */
    static const unsigned jump_dir[4]={2u,1u,0u,3u};
    unsigned octant,sector;
    u8 strokes[AVP_AUTOMAP_RASTER_BYTES];
    if(!state||!maze||!mw||!mh)return;
    /* MAZE5.GAS draws a new trace even when all squares are already seen.
       Display redraws never update the persistent one-bit surface. */
    memset(state->automap_touched,0,sizeof(state->automap_touched));
    octant=angle>>29;
    sector=((octant+1u)>>1)&3u;
    automap_reveal(state,maze,mw,mh,(int)(xpos>>16),(int)(ypos>>16),
                   jump_dir[sector],logical_level);
    rasterize_mask(state->automap_touched,maze,mw,mh,strokes);
    for(size_t i=0;i<AVP_AUTOMAP_RASTER_BYTES;i++)
        state->automap_bits[i]|=strokes[i];
}

/* PC multiplayer map-visibility modifier: expose authored geometry through
 * the original Jaguar automap rasterizer, without changing the collision or
 * exploration algorithm. Keep each viewport's state private. */
void avp_gpu_automap_reveal_all(AvpAvpGpuState *state,const u8 *maze,u16 mw,u16 mh,
                               unsigned logical_level)
{
    if(!state||!maze||!mw||!mh||mw>64u||mh>64u)return;
    if(state->automap_level!=logical_level){
        avp_gpu_automap_reset(state);
        state->automap_level=logical_level;
    }
    for(unsigned y=0;y<mh;y++)for(unsigned x=0;x<mw;x++){
        const u8 *cell=maze+((size_t)y*mw+x)*8u;
        if(cell[0]||cell[1]||cell[2]||cell[3]||cell[5])
            state->automap_seen[y*64u+x]=1u;
    }
    rasterize_mask(state->automap_seen,maze,mw,mh,state->automap_bits);
}

/* MAZE5.GAS::check_left/right/top/bottom writes into HUD.S::map_data, a
 * one-bit 512 x 512 image with a 64-byte stride.  This projection retains
 * the already-discovered cells saved by the native port; no exploration
 * state or game collision data changes as a side effect of drawing. */
static void raster_pixel(u8 *bits,int x,int y)
{
    if(x<0||y<0||x>=512||y>=512)return;
    bits[(size_t)y*AVP_AUTOMAP_ROW_BYTES+(unsigned)x/8u] |=
        (u8)(0x80u>>(unsigned)(x&7));
}

static void raster_row_byte(u8 *bits,int x,int y,u8 pattern)
{
    unsigned bit;
    for(bit=0;bit<8u;bit++)
        if(pattern&(0x80u>>bit))raster_pixel(bits,x+(int)bit,y);
}

/* MAZE5.GAS::blank_line uses pat_blank=$11111111 and rotates left by
 * one bit on each of eight rows, OR-ing the pattern into the adjacent
 * square whenever the far side of a solid wall is not also solid. */
static void raster_blank_tile(u8 *pixels,int tx,int ty)
{
    static const u8 pattern[4]={0x11u,0x22u,0x44u,0x88u};
    for(int row=0;row<8;row++)
        raster_row_byte(pixels,tx*8,ty*8+row,pattern[row&3]);
}

static void rasterize_mask(const u8 *mask,const u8 *maze,u16 mw,u16 mh,
                           u8 pixels[AVP_AUTOMAP_RASTER_BYTES])
{
    int x,y;
    if(!pixels)return;
    memset(pixels,0,AVP_AUTOMAP_RASTER_BYTES);
    if(!mask||!maze||mw==0||mh==0)return;
    for(y=0;y<(int)mh&&y<64;y++)for(x=0;x<(int)mw&&x<64;x++){
        const u8 *cell;
        unsigned side;
        if(!mask[(unsigned)y*64u+(unsigned)x])continue;
        cell=maze+((size_t)y*mw+(unsigned)x)*8u;
        for(side=0;side<4u;side++){
            u8 wall=cell[side];
            unsigned type;
            int base_x=x*8,base_y=y*8;
            if(!wall)continue;
            type=(wall>>3)&15u;
            if(type>=12u)continue; /* neither solid nor door in fullbits */
            if(side==1u||side==3u){
                /* check_top/bottom store two adjacent byte rows.  The
                 * exact door masks are pat_topdoor=$B3, pat_botdoor=$CD. */
                int top=base_y+(side==1u?-1:7);
                raster_row_byte(pixels,base_x,top,type<4u?0xffu:0xb3u);
                raster_row_byte(pixels,base_x,top+1,type<4u?0xffu:0xcdu);
            }else{
                /* l_wall / r_wall write one bit on either side of the
                 * eight-pixel tile border, per each of eight scanlines.
                 * The door stair-step is the authored sequence of
                 * l_wall,r_right,l_down,l_left,r_down,r_right,l_down,l_wall. */
                /* Source scripts left_door and right_door both run:
                   l_wall, r_right, l_down, l_left, r_down, r_right,
                   l_down, l_wall.  Each operation moves a full byte, not
                   one pixel, before stepping to the following row. */
                static const u8 left_door[8]={3u,1u,2u,2u,1u,1u,2u,3u};
                static const u8 right_door[8]={3u,2u,1u,1u,2u,2u,1u,3u};
                int outer=base_x+(side==0u?-1:8);
                int inner=base_x+(side==0u?0:7);
                for(int row=0;row<8;row++){
                    u8 mask=(type<4u)?3u:
                        (side==0u?left_door[row]:right_door[row]);
                    if(mask&1u)raster_pixel(pixels,outer,base_y+row);
                    if(mask&2u)raster_pixel(pixels,inner,base_y+row);
                }
            }
            if(type<4u){
                static const int nxoff[4]={-1,0,1,0};
                static const int nyoff[4]={0,-1,0,1};
                static const unsigned opposite[4]={2u,3u,0u,1u};
                int nx=x+nxoff[side],ny=y+nyoff[side];
                if(nx>=0&&ny>=0&&nx<(int)mw&&ny<(int)mh){
                    const u8 *neighbor=maze+((size_t)ny*mw+(unsigned)nx)*8u;
                    u8 far=neighbor[opposite[side]];
                    if(!far||((far>>3)&15u)>=4u)
                        raster_blank_tile(pixels,nx,ny);
                }
            }
        }
    }
}


/* Retained as a pure projection for regression comparisons; the active HUD
 * reads state->automap_bits, which MAZE5-style observations accumulate. */
void avp_gpu_automap_rasterize(const AvpAvpGpuState *state,
                               const u8 *maze,u16 mw,u16 mh,
                               u8 pixels[AVP_AUTOMAP_RASTER_BYTES])
{
    rasterize_mask(state?state->automap_seen:NULL,maze,mw,mh,pixels);
}

/* MAZE5.GAS: source start = player*8 + (h*cos-w*sin,
 * -h*sin-w*cos).  Each output pixel adds (2*sin,2*cos), and each row
 * advances (-2*cos,2*sin).  The blitter's integer/fractional coordinate
 * registers implement signed 16.16 sampling with the fractional half
 * preserved.  No host trigonometry or rounding-to-nearest is involved. */
static int floor_fixed_16(int64_t value)
{
    int64_t q=value/65536;
    if(value<0 && value%65536)q--;
    return (int)q;
}
void avp_gpu_automap_sample(u32 xpos,u32 ypos,s32 sine,s32 cosine,
                             unsigned width,unsigned height,int pixel_x,int pixel_y,
                             int *src_x,int *src_y)
{
    int64_t xs=(int64_t)(int32_t)xpos*8+(int64_t)height*cosine-(int64_t)width*sine;
    int64_t ys=(int64_t)(int32_t)ypos*8-(int64_t)height*sine-(int64_t)width*cosine;
    xs+=(int64_t)pixel_x*2*sine-(int64_t)pixel_y*2*cosine;
    ys+=(int64_t)pixel_x*2*cosine+(int64_t)pixel_y*2*sine;
    if(src_x)*src_x=floor_fixed_16(xs);
    if(src_y)*src_y=floor_fixed_16(ys);
}
