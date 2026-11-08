#include "jaguar/avp_gpu/heading_radians.h"
#include "jaguar/avp_gpu/software_renderer.h"
#include "jaguar/avp_gpu/presentation_interp.h"
#include "jaguar/avp_gpu/bridge.h"
#include "jaguar/avp_gpu/maze2_column.h"
#include "jaguar/avp_gpu/maze2_occlusion.h"
#include "jaguar/avp_gpu/wall_raster_dda.h"
#include "jaguar/avp_gpu/sprite_sampling.h"
#include "jaguar/avp_gpu/sprite_projection.h"
#include "jaguar/avp_gpu/host_widescreen.h"
#include "jaguar/avp_gpu/host_distance.h"
#include "jaguar/avp_gpu/floor_cell_sampling.h"
#include "jaguar/avp_gpu/host_flat_shading.h"
#include "jaguar/avp_gpu/aaf_columns.h"
#include "jaguar/avp_gpu/fp_overlay.h"
#include "jaguar/avp_gpu/overlay_scale_composite.h"
#include "jaguar/avp_gpu/popup_message_color.h"
#include "jaguar/avp_gpu/tracker_brush.h"
#include "jaguar/avp_gpu/tracker_contact_math.h"
#include "jaguar/avp_gpu/hud_fade.h"
#include "jaguar/avp_gpu/hud_weapon_icons.h"
#include "jaguar/avp_gpu/pred_scope_raster.h"
#include "jaguar/cry.h"
#include "hud_message.h"
#include "player.h"
#include "weapons.h"
#include "amp.h"
#include "levels.h"
#include "mazescrn.h"
#include "maze.h"
#include "hud.h"
#include "collectables.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define ScreenOff avp_winnt_ScreenOff /* winnt.h enumerator vs game routine */
#include <windows.h>
#undef ScreenOff
#endif
#ifndef _WIN32
#include <pthread.h>
#include <unistd.h>
#endif

void avp_gpu_predator_scope_step(AvpAvpGpuState *state,s16 previous,s16 midpoint,s16 next)
{
    if(!state)return;
    if(!state->mp_waveform_enabled && (!state->hud_base[2]||
       state->hud_base_size[2]<320u*200u))return;
    if(!state->predator_scope_valid){
        if(state->mp_waveform_enabled && state->mp_waveform_species!=2u)
            memset(state->predator_scope,0,sizeof(state->predator_scope));
        else if(state->hud_base[2]&&state->hud_base_size[2]>=320u*200u)
            avp_pred_scope_initial(state->predator_scope,state->hud_base[2]);
        else memset(state->predator_scope,0,sizeof(state->predator_scope));
        state->predator_scope_valid=1;
    }
    avp_pred_scope_step(state->predator_scope,previous,midpoint,next);
}

static uint32_t rgb(unsigned r,unsigned g,unsigned b){return (r<<16)|(g<<8)|b;}
static void put(AvpJaguarFramebuffer*fb,int x,int y,uint32_t c){if(x>=0&&y>=0&&(unsigned)x<fb->width&&(unsigned)y<fb->height)fb->pixels[(size_t)y*fb->width+(unsigned)x]=c;}
static void line(AvpJaguarFramebuffer*fb,int x0,int y0,int x1,int y1,uint32_t c){int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy;for(;;){put(fb,x0,y0,c);if(x0==x1&&y0==y1)break;{int e2=2*e;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}}}

void avp_gpu_snapshot_sim(AvpAvpGpuState *state,const u8*maze,u16 mw,u16 mh){size_t n;if(!state)return;for(unsigned i=0;i<AVP_NUM_AMPS;i++){state->amp_prev_x[i]=level1amps[i].xpos;state->amp_prev_y[i]=level1amps[i].ypos;state->amp_prev_angle[i]=level1amps[i].angle;state->amp_prev_generation[i]=avp_amp_slot_generation(i);}{AvpFirstPersonOverlayState old;avp_mazescrn_get_overlay_state(&old);state->prev_overlay_id=old.overlay_visible?old.overlay_id:-1;
    state->prev_overlay_visible=old.overlay_visible;
    state->prev_overlay_species=player_type;
    state->prev_overlay_weapon=cur_wepno;
    /* Alien's first visible move_in pose is derived from the original
     * command stream.  Smooth Presentation must start its NEXT 10-Hz
     * interpolation from that displayed pose, not the invisible frame0
     * coordinates, or it reintroduces the same dip with smoothing on.
     * Preserve the previously verified Marine snapshot path unchanged. */
    state->prev_wep_x=(player_type==PT_ALIEN&&old.intro_preview)?old.intro_x:wep_x;
    state->prev_wep_y=(player_type==PT_ALIEN&&old.intro_preview)?old.intro_y:wep_y;
    }state->prev_swing_x=swing_x;state->prev_swing_y=swing_y;n=(size_t)mw*(size_t)mh*8u;if(n>sizeof(state->prev_maze))n=sizeof(state->prev_maze);if(maze&&n){memcpy(state->prev_maze,maze,n);state->prev_maze_size=n;}else state->prev_maze_size=0;state->have_prev_sim=1;}
int avp_gpu_set_other_players(AvpAvpGpuState *state,
                               const AvpGpuOtherPlayer *actors,unsigned count)
{
    if(!state || count>AVP_GPU_OTHER_PLAYERS_MAX || (count && !actors))return 0;
    for(unsigned i=0;i<count;i++){
        if(actors[i].creature!=AC_HUMAN && actors[i].creature!=AC_PREDATOR &&
           actors[i].creature!=AC_ALIEN)return 0;
        /* Original AMP animation families, not a three-state placeholder.
         * AS_HIDDEN lets a cloaked Predator disappear without removing the
         * other player's world state. Attack and recoil use original AAF. */
        if(actors[i].animation>AS_FIGHT6)return 0;
    }
    /* Full validation first; bad input must not erase an existing view. */
    if(count)memcpy(state->other_players,actors,count*sizeof(*actors));
    state->other_player_count=count;
    return 1;
}
void avp_gpu_show_map_teammates(AvpAvpGpuState *state,int enabled)
{
    if(state)state->map_hide_teammates=(unsigned char)!enabled;
}
int avp_gpu_set_map_teammates(AvpAvpGpuState *state,
    const AvpGpuMapTeammate *players,unsigned count)
{
    if(!state || count>AVP_GPU_MAP_TEAMMATES || (count && !players))return 0;
    if(count){
        memcpy(state->map_teammates,players,count*sizeof(*players));
        /* Default to original tick positions if smoothing is disabled or
         * no presentation roster was supplied this frame. */
        memcpy(state->map_teammates_draw,players,count*sizeof(*players));
    }
    state->map_teammate_count=count;
    return 1;
}
int avp_gpu_set_map_teammates_draw(AvpAvpGpuState *state,
    const AvpGpuMapTeammate *players,unsigned count)
{
    if(!state || count!=state->map_teammate_count ||
       (count && !players))return 0;
    if(count)memcpy(state->map_teammates_draw,players,count*sizeof(*players));
    return 1;
}
void avp_gpu_set_map_zoom(AvpAvpGpuState *state,int steps)
{
    if(state)state->map_zoom_steps=steps>8?8:(steps< -8?-8:steps);
}
void avp_gpu_set_render_alpha(AvpAvpGpuState *state,double a){if(!state)return;if(a<0.0)a=0.0;if(a>1.0)a=1.0;state->render_alpha=a;}

typedef struct PcRayHit { double d,tc; u8 wall,open; int hit,vertical; int clip_lo,clip_hi; } PcRayHit;
typedef struct PcBoundary { u8 wall,open; } PcBoundary;
static int wall_visible_code(u8 wall){
    unsigned t;if(!wall)return 0;t=(wall>>3)&15u;
    /* Retail MAZE2 wall_jumps renders 0..11 and 13..15.  $60/type 12 is
       the sole unknown/non-rendering slot in the ordinary game build. */
    return t!=12u;
}
/* MAZE2.GAS::Q_ENE..Q_SSE scans the directed wall in the current
 * cell.  Opposite walls in adjacent cells are *not* a fallback: both are
 * independently authored in the retail maps and frequently differ.  The
 * former neighbor fallback inserted spurious distant columns/arches and
 * incorrectly opened views when looking along some corridor walls. */
static PcBoundary boundary_info(const u8 *maze,u16 w,u16 h,
                                int cx,int cy,int nx,int ny,int vertical)
{
    const u8 *c,*n=NULL;
    PcBoundary z={0,0};
    if(cx<0||cy<0||cx>=w||cy>=h){z.wall=1;return z;}
    c=maze+((size_t)cy*w+(size_t)cx)*8u;
    if(nx>=0&&ny>=0&&nx<w&&ny<h)
        n=maze+((size_t)ny*w+(size_t)nx)*8u;
    if(vertical){
        if(nx>cx){z.wall=c[2];z.open=n?n[6]:0;}
        else{z.wall=c[0];z.open=c[6];}
    }else{
        if(ny>cy){z.wall=c[3];z.open=n?n[7]:0;}
        else{z.wall=c[1];z.open=c[7];}
    }
    if(!wall_visible_code(z.wall))z.wall=0;
    return z;
}
/* MAZE2.GAS::common_win collapses clip_top/clip_bottom when the
 * source column has no window (win_top == win_bottom).  Keep the ray
 * from crossing such columns.  The window pair belongs to the resolved
 * wall image; don't infer a generic opening from the wall's type alone. */
static int window_column_transmits(const AvpAvpGpuState *state,unsigned level,
                                   u8 wall,double tc,unsigned type)
{
    unsigned wf;
    int tx=(int)(tc*128.0);
    if(tx<0)tx=0;
    if(tx>127)tx=127;
    if(wall&0x80u)tx=127-tx;
    if(type==13u){
        /* MAZE2.GAS::singles reads the wall code through hit_ptr;
         * unlike a door handler, it never invokes OPEN_DOOR. */
        if(!avp_gpu_maze2_singles_source_col(tx,(wall&0x80u)!=0,&tx))
            return 1; /* no wall was drawn on this half */
    }
    if(!state || !avp_gpu_wall_window_pair_file(level,wall,&wf) ||
       wf<74u || wf>86u || !state->window_pair_loaded[wf-74u])
        return 0; /* no source window list: never expose geometry behind */
    return avp_gpu_maze2_window_pair_transmits(state->window_pairs[wf-74u],(unsigned)tx);
}
/* MAZE2.GAS::bup_draw/cup_draw/bub_draw/cub_draw supply an actual
 * vertical opening.  A door with no opening must terminate the room
 * scan; otherwise the host displays distant rooms behind closed doors.
 * Retain authored window-list limits for doors with bulkheads. */
static int vertical_door_source_aperture(const AvpAvpGpuState *state,
                                         unsigned level,u8 wall,u8 open,double tc,
                                         int *source_lo,int *source_hi)
{
    unsigned type=(wall>>3)&15u,wf;
    int tx=(int)(tc*128.0),wt=-64,wb=64;
    if(type<7u||type>11u||!source_lo||!source_hi)return 0;
    if(tx<0)tx=0;
    if(tx>127)tx=127;
    if(wall&0x80u)tx=127-tx;
    if(type>=9u){
        if(!state||!avp_gpu_wall_window_pair_file(level,wall,&wf)||
           wf<74u||wf>86u||!state->window_pair_loaded[wf-74u])return 0;
        wt=(int)(int8_t)state->window_pairs[wf-74u][(size_t)tx*2u];
        wb=(int)(int8_t)state->window_pairs[wf-74u][(size_t)tx*2u+1u];
        if(wt<-64)wt=-64;
        if(wt>64)wt=64;
        if(wb<-64)wb=-64;
        if(wb>64)wb=64;
        if(wb<=wt)return 0;
    }
    return avp_gpu_maze2_vertical_door_aperture(type,open,wt,wb,
                                                  source_lo,source_hi);
}
/* A centred sliding door is not an unrestricted portal.  The Jaguar
 * either draws an opaque door column, no column at all, or a windowed column.
 * Only the last case contributes a clipping aperture to subsequent rooms. */
static int centered_door_visibility(const AvpAvpGpuState *state,unsigned level,
                                    u8 wall,u8 open,double tc)
{
    unsigned type=(wall>>3)&15u,wf;
    int tx=(int)(tc*128.0);
    const u8 *pairs=NULL;
    if(tx<0)tx=0;
    if(tx>127)tx=127;
    if(wall&0x80u)tx=127-tx;
    if((type==4u||type==6u)&&state&&
       avp_gpu_wall_window_pair_file(level,wall,&wf)&&
       wf>=74u&&wf<=86u&&state->window_pair_loaded[wf-74u])
        pairs=state->window_pairs[wf-74u];
    return avp_gpu_maze2_centered_visibility(type,open,tx,pairs);
}
/* Retain every framed crossing inside the configured ray range.
 * If the scan exceeds the portal capacity, treat the last
 * crossed wall as opaque instead of accidentally exposing rooms behind a
 * window whose clip range cannot be recorded. */
static int pc_record_portal(PcRayHit *portals,unsigned *count,unsigned cap,
                            PcRayHit hit)
{
    if(!portals || !count || *count>=cap)return 0;
    portals[(*count)++]=hit;
    return 1;
}
static PcRayHit cast_ray_hit(const AvpAvpGpuState *state,unsigned logical_level,
                             const u8*maze,u16 w,u16 h,double px,double py,double dx,double dy,
                             PcRayHit *portals,unsigned *portal_count,unsigned portal_cap)
{
    /* Preserve numeric PC mode 0..4, not merely its enabled/disabled state.
     * A logical && here previously collapsed 24/32/Unlimited into 16 rooms. */
    const double max_depth=avp_host_ray_limit(state ? state->host_extend_draw_distance : 0);
    int cx=(int)floor(px),cy=(int)floor(py),stepx=dx<0?-1:1,stepy=dy<0?-1:1,n;
    double adx=fabs(dx),ady=fabs(dy),tx,ty,dtx,dty;PcRayHit miss={max_depth,0.0,0,0,0,0,0,0};
    if (portal_count) *portal_count = 0;
    if (adx < 1e-9) adx = 1e-9;
    if (ady < 1e-9) ady = 1e-9;
    dtx=1.0/adx;dty=1.0/ady;tx=(dx<0?(px-cx):(cx+1.0-px))*dtx;ty=(dy<0?(py-cy):(cy+1.0-py))*dty;
    for(n=0;n<128;n++){
        if(avp_gpu_maze2_right_wall_first(tx,ty)){double d=tx;int nx;if(d>=max_depth)break;nx=cx+stepx;PcBoundary bi=boundary_info(maze,w,h,cx,cy,nx,cy,1);if(bi.wall){double hy=py+dy*d,tc=avp_gpu_maze2_orient_fraction(hy-floor(hy),1,stepx);unsigned type=(bi.wall>>3)&15u;
            if(type==3u||type==13u||type==14u||type==15u){
                /* A zero-height source window terminates the original
                   MAZE2 scan.  Only a genuine opening admits farther rooms. */
                if(!window_column_transmits(state,logical_level,bi.wall,tc,type)){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0};return h;
                }
                {PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0};if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;}
            }else if(type>=4u&&type<=6u){
                int visibility=centered_door_visibility(state,logical_level,bi.wall,bi.open,tc);
                if(!visibility){PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0};return h;}
                if(visibility==2){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0};
                    if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;
                }
            }else if(type>=7u&&type<=11u){
                int opening_lo,opening_hi;
                if(!vertical_door_source_aperture(state,logical_level,bi.wall,
                                                   bi.open,tc,&opening_lo,&opening_hi)){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0};return h;
                }
                {PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0};if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;}
            }else{PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0};return h;}}cx=nx;tx+=dtx;}
        else{double d=ty;int ny;if(d>=max_depth)break;ny=cy+stepy;PcBoundary bi=boundary_info(maze,w,h,cx,cy,cx,ny,0);if(bi.wall){double hx=px+dx*d,tc=avp_gpu_maze2_orient_fraction(hx-floor(hx),0,stepy);unsigned type=(bi.wall>>3)&15u;
            if(type==3u||type==13u||type==14u||type==15u){
                if(!window_column_transmits(state,logical_level,bi.wall,tc,type)){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0};return h;
                }
                {PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0};if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;}
            }else if(type>=4u&&type<=6u){
                int visibility=centered_door_visibility(state,logical_level,bi.wall,bi.open,tc);
                if(!visibility){PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0};return h;}
                if(visibility==2){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0};
                    if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;
                }
            }else if(type>=7u&&type<=11u){
                int opening_lo,opening_hi;
                if(!vertical_door_source_aperture(state,logical_level,bi.wall,
                                                   bi.open,tc,&opening_lo,&opening_hi)){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0};return h;
                }
                {PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0};if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;}
            }else{PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0};return h;}}cy=ny;ty+=dty;}
        if(cx<0||cy<0||cx>=w||cy>=h)break;
    }
    return miss;
}
/* MAZE2.GAS::restart_scan retains the intersection of successive window
 * apertures.  The original GPU clips each column before walking into the
 * next room.  The host draws wall hits back-to-front, so retain the same
 * surviving clip for each recorded hit; otherwise a more distant wall may
 * be painted outside an earlier opening. */
static int pc_portal_aperture(const AvpAvpGpuState *state,unsigned level,
                              const PcRayHit *p,unsigned fb_height,
                              int *out_lo,int *out_hi)
{
    int tx,source_col,wall_h,y0;unsigned type,wf;
    int top,bottom;
    if(!p || !out_lo || !out_hi || p->d<=0.0)return 0;
    type=(p->wall>>3)&15u;
    if(type>=7u&&type<=11u){
        if(!vertical_door_source_aperture(state,level,p->wall,p->open,p->tc,
                                          &top,&bottom))return 0;
        wall_h=avp_gpu_maze2_project_wall_height(p->d,fb_height);
        {int y1;avp_gpu_maze2_centered_wall_bounds(fb_height,wall_h,&y0,&y1);wall_h=y1-y0;}
        *out_lo=avp_gpu_maze2_host_edge_y(y0,wall_h,top);
        *out_hi=avp_gpu_maze2_host_edge_y(y0,wall_h,bottom);
        return 1;
    }
    if(type!=3u && type!=4u && type!=6u &&
       type!=13u && type!=14u && type!=15u)return 0;
    if(!state || !avp_gpu_wall_window_pair_file(level,p->wall,&wf) ||
       wf<74u || wf>86u || !state->window_pair_loaded[wf-74u])return 0;
    tx=(int)(p->tc*128.0);
    if(tx<0)tx=0;
    if(tx>127)tx=127;
    if(p->wall&0x80u)tx=127-tx;
    if(type==6u){
        int shifted;
        if(!avp_gpu_maze2_centered_door_source_col(tx,p->open,&shifted))return 0;
        tx=shifted;
    }
    if(type==13u){
        source_col=tx;
        if(!avp_gpu_maze2_singles_source_col(tx,(p->wall&0x80u)!=0,&source_col))return 0;
        tx=source_col;
    }
    top=(int)(int8_t)state->window_pairs[wf-74u][(size_t)tx*2u];
    bottom=(int)(int8_t)state->window_pairs[wf-74u][(size_t)tx*2u+1u];
    wall_h=avp_gpu_maze2_project_wall_height(p->d,fb_height);
    {int y1;avp_gpu_maze2_centered_wall_bounds(fb_height,wall_h,&y0,&y1);wall_h=y1-y0;}
    *out_lo=avp_gpu_maze2_host_edge_y(y0,wall_h,top);
    *out_hi=avp_gpu_maze2_host_edge_y(y0,wall_h,bottom);
    return 1;
}
static void pc_portal_clip_column(const AvpAvpGpuState *state,unsigned level,
                                  PcRayHit *portals,unsigned count,PcRayHit *last,
                                  unsigned fb_height)
{
    unsigned k;int lo=0,hi=(int)fb_height;
    for(k=0;k<count;k++){
        int opening_lo,opening_hi;
        PcRayHit *p=&portals[k];
        p->clip_lo=lo;p->clip_hi=hi;
        if(pc_portal_aperture(state,level,p,fb_height,&opening_lo,&opening_hi))
            avp_gpu_maze2_clip_intersect(&lo,&hi,opening_lo,opening_hi);
    }
    if(last){last->clip_lo=lo;last->clip_hi=hi;}
}
static uint32_t sample_tex(const AvpGpuRgbImage*t,int x,int y)
{
    const u8*p;if(!t||!t->rgb||t->width<=0||t->height<=0)return rgb(90,70,45);
    x%=t->width;y%=t->height;if(x<0)x+=t->width;if(y<0)y+=t->height;
    p=t->rgb+((size_t)y*(size_t)t->width+(size_t)x)*3u;return rgb(p[0],p[1],p[2]);
}


/* Shading changes only the Y byte of a Jaguar CRY pixel.  Use the exact
 * existing Jaguar CRY conversion to create a once-only lookup table, rather
 * than recalculating its three chroma products for every host-resolution
 * world pixel.  This is a pixel-identical optimization, not a new palette. */
static uint32_t k_cry_rgb[65536];
static int k_cry_rgb_ready;
static void ensure_cry_rgb_table(void)
{
    if(k_cry_rgb_ready)return;
    for(unsigned i=0;i<65536u;i++){
        u8 r,g,b;
        avp_jaguar_cry16_to_rgb((u16)i,&r,&g,&b);
        k_cry_rgb[i]=rgb(r,g,b);
    }
    k_cry_rgb_ready=1;
}
static uint32_t sample_tex_shaded(const AvpGpuRgbImage *t,int x,int y,int delta)
{
    u16 cry;
    if(!t||t->width<=0||t->height<=0)return rgb(90,70,45);
    /* Both authored callers supply already-bounded texture coordinates.
     * Avoid two variable integer divisions per shaded output pixel; retain
     * exact wrapping behavior for any future out-of-range caller. */
    if(x<0||x>=t->width){x%=t->width;if(x<0)x+=t->width;}
    if(y<0||y>=t->height){y%=t->height;if(y<0)y+=t->height;}
    if(!t->cry16)return sample_tex(t,x,y);
    cry=t->cry16[(size_t)y*(size_t)t->width+(size_t)x];
    /* Identical to avp_jaguar_srcshade_cry: saturate Y, keep CR chroma.
     * Inline here because this is the hottest path at window resolution. */
    {
        int intensity=(int)(cry&0xffu)+delta;
        if(intensity<0)intensity=0;
        if(intensity>255)intensity=255;
        cry=(u16)((cry&0xff00u)|(u16)intensity);
    }
    return k_cry_rgb[cry];
}

static void draw_wall_seg_shift(AvpJaguarFramebuffer *fb,const PcRayHit *hit,unsigned sx,int y0,int y1,int base_y0,
                                int wall_h,const AvpGpuRgbImage *t,int source_tx,
                                int source_y_shift,int shade,AvpMaze2Occlusion *occlusion,float depth)
{
    int yy;
    if(!t||!t->rgb||wall_h<=0||!hit)return;
    if(y0<hit->clip_lo)y0=hit->clip_lo;
    if(y1>hit->clip_hi)y1=hit->clip_hi;
    if(y0<0)y0=0;
    if(y1>(int)fb->height)y1=(int)fb->height;
    if(y0>=y1)return;
    if(source_tx<0)source_tx=0;
    if(source_tx>127)source_tx=127;
    source_tx=source_tx*t->width/128;
    if(source_tx>=t->width)source_tx=t->width-1;
    /* The column, shade and source texture do not change inside this call.
     * At host resolutions above Jaguar native, neighboring output rows often
     * select the same source texel. Reuse its exact CRY-shaded RGB result. */
    int cached_ty=-1;
    uint32_t cached_color=0;
    if(wall_h>=128){
        AvpWallRowDda rows=avp_wall_row_dda_begin(y0-base_y0,wall_h);
        const int have_depth=occlusion && occlusion->depth &&
                             occlusion->width==fb->width &&
                             occlusion->height==fb->height;
        for(yy=y0;yy<y1;yy++){
            const size_t pixel_index=(size_t)yy*fb->width+sx;
            int source_row,ty;
            if(have_depth){
                float *owner=&occlusion->depth[pixel_index];
                if(*owner<=depth){avp_wall_row_dda_advance(&rows);continue;}
                *owner=depth;
            }
            source_row=avp_wall_row_dda_value(&rows)+source_y_shift;
            if(source_row<0)source_row=0;
            if(source_row>127)source_row=127;
            ty=source_row*t->height/128;
            if(ty<0)ty=0;
            if(ty>=t->height)ty=t->height-1;
            if(ty!=cached_ty){
                cached_color=sample_tex_shaded(t,source_tx,ty,shade);
                cached_ty=ty;
            }
            fb->pixels[pixel_index]=cached_color;
            avp_wall_row_dda_advance(&rows);
        }
    }else{
        for(yy=y0;yy<y1;yy++){
            const size_t pixel_index=(size_t)yy*fb->width+sx;
            int source_row,ty;
            if(occlusion && occlusion->depth &&
               occlusion->width==fb->width && occlusion->height==fb->height){
                float *owner=&occlusion->depth[pixel_index];
                if(*owner<=depth)continue;
                *owner=depth;
            }
            source_row=avp_gpu_maze2_host_source_row(yy-base_y0,wall_h,source_y_shift);
            ty=source_row*t->height/128;
            if(ty<0)ty=0;
            if(ty>=t->height)ty=t->height-1;
            if(ty!=cached_ty){
                cached_color=sample_tex_shaded(t,source_tx,ty,shade);
                cached_ty=ty;
            }
            fb->pixels[pixel_index]=cached_color;
        }
    }
}
static void draw_wall_seg(AvpJaguarFramebuffer *fb,const PcRayHit *hit,unsigned sx,int y0,int y1,int base_y0,
                          int wall_h,const AvpGpuRgbImage *t,int source_tx,int shade,
                          AvpMaze2Occlusion *occlusion,float depth)
{
    draw_wall_seg_shift(fb,hit,sx,y0,y1,base_y0,wall_h,t,source_tx,0,shade,occlusion,depth);
}
static int wall_edge_y(int base_y0,int wall_h,int signed_source_y)
{
    return avp_gpu_maze2_host_edge_y(base_y0,wall_h,signed_source_y);
}

static void draw_hit_column(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,unsigned sx,const PcRayHit *h,
                            unsigned logical_level,int frame_only,AvpMaze2Occlusion *occlusion)
{
    double d;
    int wall_h,y0,y1,tx,shade;
    unsigned fid=999u,type,reflected;
    const AvpGpuRgbImage *t=NULL;
    if(!h||!h->hit)return;
    d=h->d<0.001?0.001:h->d;
    wall_h=avp_gpu_maze2_project_wall_height(d,fb->height);
    if(!avp_gpu_maze2_centered_wall_bounds(fb->height,wall_h,&y0,&y1))return;
    /* Detached Map Maker camera: move the projection horizon, never the
     * Jaguar gameplay camera. Original/game output takes the unchanged path. */
    if(state && state->map_editor_world_only){
        int dy=(int)(state->map_editor_pitch*(double)fb->height*0.52);
        y0+=dy;y1+=dy;
    }
    wall_h=y1-y0;
    reflected=h->wall&0x80u;
    tx=(int)(h->tc*128.0);
    if(tx<0)tx=0;
    if(tx>127)tx=127;
    if(reflected)tx=127-tx;
    if(avp_gpu_wall_texture_file(logical_level,h->wall,&fid)&&fid<74u&&state->panel_tex[fid].rgb)
        t=&state->panel_tex[fid];
    if(!t)return;
    if(fid==16u){
        unsigned ni;
        if(avp_gpu_wall_notice_index(logical_level,h->wall,&ni) &&
           ni<18u&&state->notice_tex[ni].rgb)
            t=&state->notice_tex[ni];
    }
    shade=avp_host_fog_shade_delta(avp_gpu_retail_wall_intensity_delta(
        avp_host_fog_distance(d,state ? state->host_extend_fog_distance : 0)),
        state && state->host_disable_fog);
    type=(h->wall>>3)&15u;

    if(type==3u||type==13u||type==14u||type==15u){
        unsigned wf=0;int top_pair=0,bottom_pair=0;
        if(avp_gpu_wall_window_pair_file(logical_level,h->wall,&wf) &&
           wf>=74u&&wf<=86u&&state->window_pair_loaded[wf-74u]){
            int pair_tx=tx;
            if(type==13u){
                int source_col=tx;
                /* MAZE2.GAS::singles loads the wall code again from hit_ptr
                 * and uses its sign bit to undo the original reflection. */
                if(!avp_gpu_maze2_singles_source_col(tx,reflected!=0,&source_col))return;
                tx=source_col;
                pair_tx=source_col;
            }
            top_pair=(int)(int8_t)state->window_pairs[wf-74u][pair_tx*2];
            bottom_pair=(int)(int8_t)state->window_pairs[wf-74u][pair_tx*2+1];
            {
                int mt=wall_edge_y(y0,wall_h,top_pair);
                int mb=wall_edge_y(y0,wall_h,bottom_pair);
                draw_wall_seg(fb,h,sx,y0,mt,y0,wall_h,t,tx,shade,occlusion,(float)d);
                draw_wall_seg(fb,h,sx,mb,y1,y0,wall_h,t,tx,shade,occlusion,(float)d);
            }
        }else if(!frame_only){
            /* Fail closed as a solid column only when the authored window list
               is unavailable; canonical retail resources should always load. */
            draw_wall_seg(fb,h,sx,y0,y1,y0,wall_h,t,tx,shade,occlusion,(float)d);
        }
        return;
    }

    if(type>=4u&&type<=6u){
        int have=0,top_pair=0,bottom_pair=0,door_tx=tx;
        unsigned wf=0;
        if(type==6u){
            if(!avp_gpu_maze2_centered_door_source_col(tx,h->open,&door_tx))return;
        }
        if((type==4u||type==6u) &&
           avp_gpu_wall_window_pair_file(logical_level,h->wall,&wf) &&
           wf>=74u&&wf<=86u&&state->window_pair_loaded[wf-74u]){
            int pair_tx=(type==6u)?door_tx:tx;
            top_pair=(int)(int8_t)state->window_pairs[wf-74u][pair_tx*2];
            bottom_pair=(int)(int8_t)state->window_pairs[wf-74u][pair_tx*2+1];
            have=1;
        }
        if(have){
            int mt=wall_edge_y(y0,wall_h,top_pair);
            int mb=wall_edge_y(y0,wall_h,bottom_pair);
            int base_tx=(type==6u)?door_tx:tx;
            draw_wall_seg(fb,h,sx,y0,mt,y0,wall_h,t,base_tx,shade,occlusion,(float)d);
            draw_wall_seg(fb,h,sx,mb,y1,y0,wall_h,t,base_tx,shade,occlusion,(float)d);
            if(!frame_only&&mt<mb&&type!=6u){
                int moving=tx;
                if(avp_gpu_maze2_centered_door_source_col(tx,h->open,&moving))
                    draw_wall_seg(fb,h,sx,mt,mb,y0,wall_h,t,moving,shade,occlusion,(float)d);
            }
        }else if(!frame_only){
            int moving=(type==6u)?door_tx:tx;
            if(type==6u||avp_gpu_maze2_centered_door_source_col(tx,h->open,&moving))
                draw_wall_seg(fb,h,sx,y0,y1,y0,wall_h,t,moving,shade,occlusion,(float)d);
        }
        return;
    }

    if(type>=7u&&type<=11u){
        int open=(int)h->open;
        if(open<0)open=0;
        if(open>127)open=127;
        if(type==7u){
            /* bup_draw: source base advances by `open` rows while the lower
               screen edge rises from +64 to +64-open. */
            int edge=wall_edge_y(y0,wall_h,64-open);
            draw_wall_seg_shift(fb,h,sx,y0,edge,y0,wall_h,t,tx,+open,shade,occlusion,(float)d);
        }else if(type==8u){
            /* cup_draw: two halves retract symmetrically.  GPU source bases
               are +open for the upper half and -open for the lower half. */
            int et=wall_edge_y(y0,wall_h,-open);
            int eb=wall_edge_y(y0,wall_h,+open);
            draw_wall_seg_shift(fb,h,sx,y0,et,y0,wall_h,t,tx,+open,shade,occlusion,(float)d);
            draw_wall_seg_shift(fb,h,sx,eb,y1,y0,wall_h,t,tx,-open,shade,occlusion,(float)d);
        }else{
            /* BUB/CUB use their authored bulkhead window list plus one/two
               moving internal edges.  This is a direct source-coordinate
               translation of bub_draw/cub_draw -> like_cub in MAZE2.GAS. */
            unsigned wf=0;int wt=-64,wb=64;
            if(avp_gpu_wall_window_pair_file(logical_level,h->wall,&wf) &&
               wf>=74u&&wf<=86u&&state->window_pair_loaded[wf-74u]){
                wt=(int)(int8_t)state->window_pairs[wf-74u][tx*2];
                wb=(int)(int8_t)state->window_pairs[wf-74u][tx*2+1];
                if(wt<-64)wt=-64;
                if(wt>64)wt=64;
                if(wb<-64)wb=-64;
                if(wb>64)wb=64;
                if(wb<wt){int q=wt;wt=wb;wb=q;}
            }
            if(type==9u||type==10u){
                int xe=wb-open;
                if(xe<wt)xe=wt;
                if(xe>wb)xe=wb;
                draw_wall_seg(fb,h,sx,y0,wall_edge_y(y0,wall_h,wt),y0,wall_h,t,tx,shade,occlusion,(float)d);
                draw_wall_seg_shift(fb,h,sx,wall_edge_y(y0,wall_h,wt),wall_edge_y(y0,wall_h,xe),y0,wall_h,t,tx,+open,shade,occlusion,(float)d);
                draw_wall_seg(fb,h,sx,wall_edge_y(y0,wall_h,wb),y1,y0,wall_h,t,tx,shade,occlusion,(float)d);
            }else{
                int xt=12-open,xb=12+open;
                if(xt<wt)xt=wt;
                if(xt>wb)xt=wb;
                if(xb<wt)xb=wt;
                if(xb>wb)xb=wb;
                if(xb<xt){int q=xt;xt=xb;xb=q;}
                draw_wall_seg(fb,h,sx,y0,wall_edge_y(y0,wall_h,wt),y0,wall_h,t,tx,shade,occlusion,(float)d);
                draw_wall_seg_shift(fb,h,sx,wall_edge_y(y0,wall_h,wt),wall_edge_y(y0,wall_h,xt),y0,wall_h,t,tx,+open,shade,occlusion,(float)d);
                draw_wall_seg_shift(fb,h,sx,wall_edge_y(y0,wall_h,xb),wall_edge_y(y0,wall_h,wb),y0,wall_h,t,tx,-open,shade,occlusion,(float)d);
                draw_wall_seg(fb,h,sx,wall_edge_y(y0,wall_h,wb),y1,y0,wall_h,t,tx,shade,occlusion,(float)d);
            }
        }
        return;
    }
    draw_wall_seg(fb,h,sx,y0,y1,y0,wall_h,t,tx,shade,occlusion,(float)d);
}


static u16 pc_be16(const u8*p){return (u16)(((u16)p[0]<<8)|p[1]);}
static u32 pc_be32(const u8*p){return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3];}

typedef struct PcSpriteChoice { int aaf; int mirror; } PcSpriteChoice;
static PcSpriteChoice sprite_choice(const AvpAmp *a,double px,double py)
{
    PcSpriteChoice z={-1,0}; unsigned sidx;
    if(!a||a->animseq==AS_HIDDEN)return z;
    sidx=(unsigned)a->animseq-1u;
    switch(a->creature){
    case AC_ALIEN:{static const int t[]={8,1,5,-1,4,3,48,8};if(sidx<sizeof(t)/sizeof(t[0]))z.aaf=t[sidx];break;}
    case AC_PREDATOR:{static const int t[]={29,37,23,-1,45,29,42,32,38,36,40};if(sidx<sizeof(t)/sizeof(t[0]))z.aaf=t[sidx];break;}
    case AC_HUMAN:
        /* An observing player must not see the enemy Marine's AAF 15
         * standing/firing loop while the other Marine is simply idle.
         * For the player-only stand pose, borrow a directional source sheet
         * and hold frame zero; preserve native enemy Marine choices. */
        if(a->animseq==AS_RUN ||
           ((a->flags&(1u<<AMP_PLAYER)) &&
            (a->animseq==AS_WALK || a->animseq==AS_FIGHT1 ||
             a->animseq==AS_FIGHT2))){
            double dx=(double)a->xpos/65536.0-px,dy=(double)a->ypos/65536.0-py;
            double look=atan2(dy,dx),face=-(double)a->angle*(2.0*M_PI/65536.0),rel=look-face;
            while(rel<-M_PI)rel+=2.0*M_PI;
            while(rel>M_PI)rel-=2.0*M_PI;
            /* The dedicated rear run sheet is AAF 56; the principal Marine
             * run sheet (AAF 24) is frontal. The old host mapping selected
             * these in reverse, while its AAF 55 side/mirror path was right. */
            if((a->flags&(1u<<AMP_PLAYER)) &&
               (a->animseq==AS_FIGHT1 || a->animseq==AS_FIGHT2) &&
                fabs(rel)>3.0*M_PI/4.0){
                /* Original enemy Marine sprite_choice table maps FIGHT1
                 * (pulse burst) to AAF18 and FIGHT2 (flame) to AAF15.
                 * Other angles borrow the directional idle player sheet. */
                z.aaf=a->animseq==AS_FIGHT1?18:15;
            }else if(fabs(rel)<M_PI/4.0)z.aaf=56;
            else if(fabs(rel)>3.0*M_PI/4.0)z.aaf=24;
            else {z.aaf=55;z.mirror=rel>0.0;}
        }else{static const int t[]={15,21,-1,-1,31,18,15,25};if(sidx<sizeof(t)/sizeof(t[0]))z.aaf=t[sidx];}
        break;
    case AC_EGG:z.aaf=(sidx==0)?20:-1;break;
    case AC_FACEHUG:{static const int t[]={28,27,28,-1,-1,26};if(sidx<sizeof(t)/sizeof(t[0]))z.aaf=t[sidx];break;}
    case AC_ACRAWL:{static const int t[]={33,-1,33,-1,17,34};if(sidx<sizeof(t)/sizeof(t[0]))z.aaf=t[sidx];break;}
    case AC_SMART:z.aaf=(sidx==0)?43:-1;break;
    case AC_BOLT:z.aaf=(sidx==0)?11:-1;break;
    case AC_FIRE:z.aaf=(sidx==0)?39:-1;break;
    case AC_SPARK1:z.aaf=(sidx==0)?47:-1;break;
    case AC_AID:z.aaf=(sidx==0)?2:-1;break;
    case AC_AMMO1:z.aaf=(sidx==0)?6:-1;break;
    case AC_AMMO2:z.aaf=(sidx==0)?7:-1;break;
    case AC_CAN:z.aaf=(sidx==0)?12:-1;break;
    case AC_CART:z.aaf=(sidx==0)?13:-1;break;
    case AC_CHAIN:z.aaf=(sidx==0)?14:-1;break;
    case AC_COCOON:z.aaf=(sidx==0)?16:-1;break;
    case AC_DRUM:z.aaf=(sidx==0)?51:-1;break;
    case AC_FOOD:z.aaf=(sidx==0)?22:-1;break;
    case AC_SHELLS:z.aaf=(sidx==0)?46:-1;break;
    case AC_TABLE1:z.aaf=(sidx==0)?49:-1;break;
    case AC_TABLE2:z.aaf=(sidx==0)?50:-1;break;
    case AC_DEADOFF:z.aaf=(sidx==0)?9:-1;break;
    case AC_DEADMED:z.aaf=(sidx==0)?10:-1;break;
    case AC_ABLOOD:z.aaf=(sidx==0)?0:-1;break;
    case AC_PBLOOD:z.aaf=(sidx==0)?44:-1;break;
    case AC_MBLOOD:z.aaf=(sidx==0)?30:-1;break;
    case AC_CRYO:z.aaf=(sidx==0)?19:-1;break;
    case AC_OPTABLE:z.aaf=(sidx==0)?41:-1;break;
    case AC_BED:z.aaf=(sidx==0)?35:-1;break;
    case AC_AQUEEN:{static const int t[]={58,60,58,-1,60,61,59,62};if(sidx<sizeof(t)/sizeof(t[0]))z.aaf=t[sidx];break;}
    case AC_GEN:{static const int t[]={53,54};if(sidx<2)z.aaf=t[sidx];break;}
    case AC_AQSHIELD:{static const int t[]={52,57};if(sidx<2)z.aaf=t[sidx];break;}
    default:break;
    }
    return z;
}

static void draw_amp_sprite(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,const AvpAmp *a,double px,double py,double fx,double fy,double rx,double ry,double focal,const double *zbuf,const AvpMaze2Occlusion *occlusion)
{
    PcSpriteChoice ch=sprite_choice(a,px,py); const u8 *d,*fr; size_t n,fo,coltab; unsigned frames,fi,ncols,ci; int left,right,use_occlusion; double wx,wy,forward,side,cx,scale_x,scale_y;
    if(ch.aaf<0||ch.aaf>=63||!state->aaf[ch.aaf])return;
    d=state->aaf[ch.aaf];n=state->aaf_size[ch.aaf];if(n<4)return;frames=pc_be16(d);if(!frames||4u+(size_t)frames*4u>n)return;
    fi=(unsigned)a->animframe%frames;fo=pc_be32(d+4u+(size_t)fi*4u);if(fo+2u>n)return;fr=d+fo;left=(int)(int8_t)fr[0];right=(int)(int8_t)fr[1];if(right<left)return;ncols=(unsigned)(right-left);coltab=fo+2u;if(coltab+(size_t)ncols*2u>n)return;
    wx=(double)a->xpos/65536.0-px;wy=(double)a->ypos/65536.0-py;forward=wx*fx+wy*fy;if(forward<=0.04||forward>=avp_host_ray_limit(state ? state->host_extend_draw_distance : 0))return;side=wx*rx+wy*ry;cx=(double)fb->width*0.5+(side/forward)*focal;
    avp_gpu_sprite_viewport_scale(fb->width,fb->height,focal,forward,sprite_rescale,
                                  &scale_x,&scale_y);
    if(scale_x<0.03||scale_y<0.03)return;
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
        x0=(int)floor(cx+((double)left+(double)ci)*scale_x);x1=(int)ceil(cx+((double)left+(double)ci+1.0)*scale_x);if(x1<=x0)x1=x0+1;
        if(!avp_gpu_sprite_clip_span(&x0,&x1,(int)fb->width))continue;
        /* A single AAF source column projects to many host screen columns at
         * high resolutions. Its vertical spans and decoded CRY colors do not
         * depend on destination X. Compute these once for this column, while
         * leaving the original visibility/depth tests inside the pixel loop. */
        /* The inverse nearest-neighbour Y sample depends on this source row,
         * not on the destination X column. Its floor(affine Y) mapping is
         * monotonic for positive scale, so matching Y pixels form a single
         * contiguous interval. Trim that interval once per source column,
         * preserving the exact pixel-centre rule and paint order. */
        const double sample_y_origin=(double)fb->height*0.5+(double)a->yoffset*scale_y;
        struct { int y0,y1; u32 color; int active; } source_rows[256];
        for(syi=0;syi<npix;syi++){
            int y0=(int)floor((double)fb->height*0.5+((double)top+(double)syi+(double)a->yoffset)*scale_y);
            int y1=(int)ceil((double)fb->height*0.5+((double)top+(double)syi+1.0+(double)a->yoffset)*scale_y);
            u16 cry=pc_be16(d+co+2u+(size_t)syi*2u);
            u8 rr,gg,bb;
            source_rows[syi].active=0;
            if(cry==0)continue;
            avp_jaguar_cry16_to_rgb(cry,&rr,&gg,&bb);
            if(y1<=y0)y1=y0+1;
            if(!avp_gpu_sprite_clip_span(&y0,&y1,(int)fb->height))continue;
            while(y0<y1 && avp_gpu_sprite_sample_coord(y0,sample_y_origin,scale_y)!=top+syi)++y0;
            while(y1>y0 && avp_gpu_sprite_sample_coord(y1-1,sample_y_origin,scale_y)!=top+syi)--y1;
            if(y0>=y1)continue;
            source_rows[syi].y0=y0;
            source_rows[syi].y1=y1;
            source_rows[syi].color=rgb(rr,gg,bb);
            source_rows[syi].active=1;
        }
        for(sx=x0;sx<x1;sx++){
            if(avp_gpu_sprite_sample_coord(sx,cx,scale_x)!=(int)left+(int)ci)continue;
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
                    if(use_occlusion&&avp_gpu_maze2_occlusion_blocks(occlusion,sx,yy,(float)forward))continue;
                    fb->pixels[(size_t)yy*fb->width+(unsigned)sx]=source_rows[syi].color;
                }
            }
        }
    }
}

static void draw_amp_layer(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,double px,double py,double fx,double fy,double rx,double ry,double focal,const double *zbuf,const AvpMaze2Occlusion *occlusion,int smooth_presentation)
{
    unsigned i, j, count=0;
    double dist[AVP_NUM_AMPS+AVP_GPU_OTHER_PLAYERS_MAX];
    unsigned order[AVP_NUM_AMPS+AVP_GPU_OTHER_PLAYERS_MAX];
    for(i=0;i<AVP_NUM_AMPS;i++)if(level1amps[i].mode||level1amps[i].host_static){
        double dx=(double)level1amps[i].xpos/65536.0-px;
        double dy=(double)level1amps[i].ypos/65536.0-py;
        dist[count]=dx*dx+dy*dy;order[count++]=i;
    }
    /* Same distance sort as native AMP sprites: a nearby player must not
     * draw through a nearer enemy, or behind a farther enemy. */
    for(i=0;i<state->other_player_count&&i<AVP_GPU_OTHER_PLAYERS_MAX;i++){
        const AvpGpuOtherPlayer *a=&state->other_players[i];
        double dx=(double)a->x/65536.0-px,dy=(double)a->y/65536.0-py;
        dist[count]=dx*dx+dy*dy;order[count++]=AVP_NUM_AMPS+i;
    }
    for(i=1;i<count;i++){
        unsigned oi=order[i];double di=dist[i];j=i;
        while(j&&dist[j-1u]<di){dist[j]=dist[j-1u];order[j]=order[j-1u];--j;}
        dist[j]=di;order[j]=oi;
    }
    for(i=0;i<count;i++){
        unsigned oi=order[i];AvpAmp t;
        if(oi>=AVP_NUM_AMPS){
            const AvpGpuOtherPlayer *p=&state->other_players[oi-AVP_NUM_AMPS];
            memset(&t,0,sizeof(t));
            t.xpos=p->x;t.ypos=p->y;t.angle=p->angle;
            t.creature=p->creature;t.animseq=p->animation;t.animframe=p->frame;
            /* Host-only sprite proxy: this flag never makes a gameplay AMP
             * or mutates the shared collision/AI grid. */
            t.flags=(u16)(1u<<AMP_PLAYER);
            if(t.creature==AC_HUMAN && t.animseq==AS_WALK)
                t.animframe=0;
            if(t.creature==AC_PREDATOR && t.animseq==AS_STAND)
                t.animframe=0;
        }else{
            t=level1amps[oi];
            if(smooth_presentation&&state->have_prev_sim&&
               state->amp_prev_generation[oi]==avp_amp_slot_generation(oi)){
                double a=state->render_alpha;
                t.xpos=avp_presentation_interpolate_fixed(state->amp_prev_x[oi],t.xpos,a,2L<<16);
                t.ypos=avp_presentation_interpolate_fixed(state->amp_prev_y[oi],t.ypos,a,2L<<16);
                {u16 pa=state->amp_prev_angle[oi],ca=t.angle;
                 s16 da=(s16)(ca-pa);
                 if(da<=0x3000&&da>=-0x3000)t.angle=(u16)(pa+(s16)((double)da*a));}
            }
        }
        draw_amp_sprite(state,fb,&t,px,py,fx,fy,rx,ry,focal,zbuf,occlusion);
    }
}

static void draw_indexed_native(AvpJaguarFramebuffer*fb,const u8*pix,size_t n,int iw,int ih,const u8 pal[208][3],unsigned colors,int x,int y)
{
    int dx,dy;if(!pix||!pal||iw<=0||ih<=0||(size_t)iw*(size_t)ih>n)return;
    for(dy=0;dy<ih;dy++)for(dx=0;dx<iw;dx++){
        u8 pi=pix[(size_t)dy*(size_t)iw+(size_t)dx];int nx=x+dx,ny=y+dy;
        if(!pi||pi>=colors||nx<0||ny<0||nx>=320||ny>=228)continue;
        put(fb,nx,ny,rgb(pal[pi][0],pal[pi][1],pal[pi][2]));
    }
}

static void draw_indexed4_native(AvpJaguarFramebuffer*fb,const u8*pix,size_t n,int iw,int ih,const u8 pal[208][3],unsigned colors,int x,int y)
{
    int dx,dy;size_t rowbytes;
    if(!pix||!pal||iw<=0||ih<=0)return;
    rowbytes=((size_t)iw+1u)/2u;
    if(rowbytes*(size_t)ih>n)return;
    /* Jaguar 4-bpp phrase pixels are consumed high nibble first.  OT_USE uses
       OBJ_TRANS, so palette index 0 is transparent; its OP index selects the
       retail 208..223 CLUT range, already rebound here as a local 16-colour pal. */
    for(dy=0;dy<ih;dy++)for(dx=0;dx<iw;dx++){
        u8 b=pix[(size_t)dy*rowbytes+(size_t)dx/2u];
        u8 pi=(u8)((dx&1)?(b&0x0fu):(b>>4));
        int nx=x+dx,ny=y+dy;
        if(!pi||pi>=colors||nx<0||ny<0||nx>=320||ny>=228)continue;
        put(fb,nx,ny,rgb(pal[pi][0],pal[pi][1],pal[pi][2]));
    }
}

static void draw_fp_use(AvpAvpGpuState*state,AvpJaguarFramebuffer*fb,const AvpFirstPersonOverlayState*st,int use_id)
{
    AvpFpUseOverlayDesc d;int slot,x,y;
    if(!avp_gpu_fp_use_overlay_desc(use_id,&d))return;
    slot=d.file-133;
    if(slot<0||slot>=52||!state->fp_img[slot]||state->fp_pal_valid[slot]!=16u)return;
    x=(int)(fb->width/2u)+d.x_offset+(int)(st->x>>16)+(st->swing_enabled?st->swing_x:0);
    y=(int)(fb->height/2u)+d.y_offset+(int)(st->y>>16)+(st->swing_enabled?st->swing_y:0);
    draw_indexed4_native(fb,state->fp_img[slot],state->fp_size[slot],d.width,d.height,state->fp_pal[slot],state->fp_pal_valid[slot],x,y);
}

static void overlay_first_person(AvpAvpGpuState*state,AvpJaguarFramebuffer*fb,u64 vblank_counter,int smooth_presentation)
{
    AvpFirstPersonOverlayState st;AvpFpOverlayDesc inf;unsigned phase;int x,y,slot;
    avp_mazescrn_get_overlay_state(&st);
    /* Retail set_opic stores every weapon/use picture's authored coordinates
       relative to the screen centre. update_wep then adds the same integer
       weapon position and swing to all three OP objects. */
    if(smooth_presentation&&state->have_prev_sim&&st.overlay_visible&&
       state->prev_overlay_visible&&!st.intro_preview){
        const int same_alien_weapon=avp_presentation_alien_overlay_continuity(
            state->prev_overlay_species,state->prev_overlay_weapon,
            state->prev_overlay_visible,player_type,cur_wepno,
            st.overlay_visible);
        const int same_sprite=state->prev_overlay_id==st.overlay_id;
        if(same_alien_weapon||same_sprite){
            const s32 max_offset=same_alien_weapon?-1:(32L<<16);
            const double a=state->render_alpha;
            st.x=avp_presentation_interpolate_fixed(state->prev_wep_x,st.x,a,max_offset);
            st.y=avp_presentation_interpolate_fixed(state->prev_wep_y,st.y,a,max_offset);
            st.swing_x=avp_presentation_interpolate_small(state->prev_swing_x,st.swing_x,a,32);
            st.swing_y=avp_presentation_interpolate_small(state->prev_swing_y,st.swing_y,a,32);
        }
    }

    /* Source-authored first move_in pose for Marine (R98) and Alien (R203).
     * No gameplay commands are executed early; Predator is unaffected. */
    if(st.intro_preview){st.x=st.intro_x;st.y=st.intro_y;}

    /* Object-list order is MAZE -> USE1 -> USE2 -> OVER -> HUD.  Therefore
       flashes are composited first and the gun body can legitimately cover
       portions of them. vf_pulse alternates which use object is hidden every
       60 Hz VBL whenever both are logically shown. */
    phase=avp_gpu_fp_vblank_parity(vblank_counter);
    if(st.use1_visible&&st.use2_visible){
        if(phase==0u)draw_fp_use(state,fb,&st,st.use1_id);
        else draw_fp_use(state,fb,&st,st.use2_id);
    }else{
        if(st.use1_visible)draw_fp_use(state,fb,&st,st.use1_id);
        if(st.use2_visible)draw_fp_use(state,fb,&st,st.use2_id);
    }

    if(!st.overlay_visible||!avp_gpu_fp_overlay_desc(st.overlay_id,&inf))return;
    slot=inf.file-133;if(slot<0||slot>=52||!state->fp_img[slot]||state->fp_pal_valid[slot]!=208u)return;
    x=(int)(fb->width/2u)+inf.x_offset+(int)(st.x>>16)+(st.swing_enabled?st.swing_x:0);
    y=(int)(fb->height/2u)+inf.y_offset+(int)(st.y>>16)+(st.swing_enabled?st.swing_y:0);
    draw_indexed_native(fb,state->fp_img[slot],state->fp_size[slot],inf.width,inf.height,state->fp_pal[slot],state->fp_pal_valid[slot],x,y);
}

/* The world may use host resolution, but the original Jaguar first-person
 * OP sprites are authored for a 320x228 surface.  Composite only changed
 * native pixels back onto the high-resolution world, preserving the world
 * underneath transparent sprite pixels.  Reuse the existing HUD tiles: the
 * HUD pass runs later and may overwrite the scratch area safely. */
static void overlay_first_person_scaled(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                                        u64 vblank_counter,int smooth_presentation)
{
    const size_t count=320u*228u;
    AvpJaguarFramebuffer native;
    u32 *before,*after;
    if(fb->width==320u&&fb->height==228u){
        overlay_first_person(state,fb,vblank_counter,smooth_presentation);
        return;
    }
    if(!state->hud_native_tiles){
        state->hud_native_tiles=(u32*)malloc(2u*count*sizeof(u32));
        if(!state->hud_native_tiles)return;
    }
    before=state->hud_native_tiles;
    after=before+count;
    for(unsigned y=0;y<228u;y++){
        unsigned fy=(unsigned)(((unsigned long long)(2u*y+1u)*fb->height)/(2u*228u));
        if(fy>=fb->height)fy=fb->height-1u;
        for(unsigned x=0;x<320u;x++){
            unsigned fx=(unsigned)(((unsigned long long)(2u*x+1u)*fb->width)/(2u*320u));
            if(fx>=fb->width)fx=fb->width-1u;
            before[(size_t)y*320u+x]=fb->pixels[(size_t)fy*fb->width+fx];
        }
    }
    memcpy(after,before,count*sizeof(u32));
    native.width=320u;native.height=228u;native.pixels=after;
    overlay_first_person(state,&native,vblank_counter,smooth_presentation);
    /* Invert the same native sampling map and visit only changed OP texels.
     * Sparse first-person sprites no longer require a full high-res screen
     * comparison or a width/height division for each destination pixel. */
    avp_gpu_overlay_scaled_composite(fb->pixels,fb->width,fb->height,before,after);
}

void avp_gpu_hud_message_clear(AvpAvpGpuState *state)
{
    if(!state)return;
    state->hud_msg_valid=0;
    memset(state->hud_msg,0,sizeof(state->hud_msg));
    memset(state->hud_msg_x,0,sizeof(state->hud_msg_x));
}
void avp_gpu_hud_message_line(AvpAvpGpuState *state,unsigned line,const char *text,unsigned x_pixels)
{
    if(!state||line>=3u||!text)return;
    snprintf(state->hud_msg[line],sizeof(state->hud_msg[line]),"%s",text);
    state->hud_msg_x[line]=x_pixels;
    state->hud_msg_valid|=(1u<<line);
}



static uint32_t computer_bg(void){return rgb(5,18,12);}
static uint32_t computer_col(const AvpAvpGpuState *state,unsigned v)
{
    /* COMPUTER.S uses the 2-bit CTEXT Object Processor palette bank at
     * entries 252..255 of FILES 102 (compbg_pal).  In the retail cartridge
     * these last four slots form a separately authored four-entry group;
     * the previous PC rasterizer substituted three invented green shades.
     * CTEXT index zero is transparent even if its CLUT entry is nonzero. */
    static const uint32_t fallback[4]={0,0x00204a32u,0x004ea56bu,0x0094efacu};
    v&=3u;
    if(v!=0u&&state&&state->computer_palette_valid)
        return state->computer_palette[252u+v];
    return fallback[v];
}
static void computer_clear(AvpAvpGpuState *state)
{
    if(!state)return;
    /* The original COMPUTER.S uses independent objects for the background,
       artwork and CTEXT plane.  Keep CTEXT transparent until final compositing. */
    memset(state->computer_pixels,0,sizeof(state->computer_pixels));
    /* A CLS discards the previous cursor image as well as its pixels.
     * Otherwise the next menu's first CURS update erases a glyph-shaped
     * rectangle from fresh retail text using a stale row number. */
    state->computer_cursor=-1;
}

/* COMPUTER.S::cobj_tab recovered from retail RDB.DTA at $293B4.
   FILES record 104 is the first weapon image and 128..132 are the five maps.
   Coordinates in this table are relative to the screen center (160,120). */
typedef struct ComputerArtPlacement {u16 width,height; s16 x,y;} ComputerArtPlacement;
static const ComputerArtPlacement computer_art[29]={
    {192,127,-112,-52}, {304,114,-140,-57}, {256,72,-128,-40},
    {272,83,-132,-45}, {112,100,-159,-100}, {112,100,-159,0},
    {112,100,-159,-100}, {112,100,-159,0}, {112,100,-159,-100},
    {112,100,-159,0}, {112,100,-159,-100}, {112,100,-159,0},
    {80,43,0,0}, {64,60,-130,-70}, {64,60,-130,-70},
    {64,60,-130,-70}, {64,60,-130,-70}, {64,60,-130,-70},
    {64,60,-130,-70}, {64,60,-130,-70}, {64,60,-130,15},
    {64,60,-130,15}, {64,60,-130,15}, {64,60,-130,15},
    {272,178,-136,-89}, {304,171,-152,-85}, {304,181,-152,-90},
    {320,130,-160,-65}, {272,178,-136,-89}
};
static uint32_t computer_art_pixel(const AvpAvpGpuState *state,int file,int x,int y,uint32_t back)
{
    unsigned idx,pal;
    const ComputerArtPlacement *placement;
    const u8 *data;
    int xx,yy;
    if(!state||file<104||file>132||!state->computer_palette_valid)return back;
    idx=(unsigned)(file-104);placement=&computer_art[idx];
    data=state->computer_overlay_img[idx];
    if(!data||state->computer_overlay_size[idx]<(size_t)placement->width*placement->height)return back;
    xx=x-160-placement->x;yy=y-120-placement->y;
    if(xx<0||yy<0||xx>=(int)placement->width||yy>=(int)placement->height)return back;
    pal=data[(size_t)yy*placement->width+(unsigned)xx];
    return pal?state->computer_palette[pal]:back; /* Jaguar OP transparent index */
}
static int computer_font_bitpair(const AvpAvpGpuState *state,unsigned brush,unsigned x,unsigned y)
{
    size_t off;unsigned shift;
    if(!state||brush>=3u||!state->computer_font[brush]||y>=9u||x>=256u)return 0;
    off=(size_t)y*64u+(size_t)(x>>2);
    if(off>=state->computer_font_size[brush])return 0;
    shift=(3u-(x&3u))*2u;
    return (state->computer_font[brush][off]>>shift)&3u;
}
static int computer_column_nonblank(const AvpAvpGpuState *state,unsigned brush,unsigned x)
{
    for(unsigned y=0;y<9u;y++)if(computer_font_bitpair(state,brush,x,y))return 1;
    return 0;
}
static int computer_find_glyph(const AvpAvpGpuState *state,unsigned ch,
                               unsigned *out_brush,unsigned *out_x,unsigned *out_w)
{
    unsigned want,out=0;
    if(ch<33u)return 0;
    want=ch-33u;
    for(unsigned b=0;b<3u;b++){
        unsigned pixel=0,width=0,blank=0,start=0;
        if(!state->computer_font[b])continue;
        for(unsigned guard=0;guard<2048u&&pixel<256u;guard++){
            ++width;++pixel;
            if(computer_column_nonblank(state,b,pixel-1u)){blank=0;continue;}
            if(++blank==2u)break;
            if(out==want){if(out_brush)*out_brush=b;if(out_x)*out_x=start;if(out_w)*out_w=width;return 1;}
            ++out;start=pixel;width=0;
        }
    }
    return 0;
}
void avp_gpu_computer_draw_glyph(AvpAvpGpuState *state,unsigned brush,unsigned source_x,
                                 unsigned width,unsigned x,unsigned y,int clear)
{
    if(!state||brush>=3u)return;
    for(unsigned yy=0;yy<9u;yy++)for(unsigned xx=0;xx<width;xx++){
        unsigned dx=x+xx,dy=y+yy;int pv;
        if(dx>=320u||dy>=240u)continue;
        pv=computer_font_bitpair(state,brush,source_x+xx,yy);
        if(clear||pv==0)state->computer_pixels[(size_t)dy*320u+dx]=0;
        else state->computer_pixels[(size_t)dy*320u+dx]=0xff000000u|computer_col(state,(unsigned)pv);
    }
}
static void computer_text(AvpAvpGpuState *state,int x,int y,const char *t)
{
    if(!state||!t)return;
    while(*t){
        unsigned ch=(unsigned char)*t++,b,sx,w;
        if(ch==' '){x+=6;continue;}
        if(computer_find_glyph(state,ch,&b,&sx,&w)){
            avp_gpu_computer_draw_glyph(state,b,sx,w,(unsigned)x,(unsigned)y,0);
            x+=(int)w;
        }else x+=6;
    }
}
/* Render the actual RDB.DTA terminal menu record, rather than PC-authored
 * replacement text.  FONT.S uses BE x/y words, an optional typewriter flag,
 * and $FF followed by a word-aligned new x/y pair.  The menu's CPU timing and
 * input remain owned by the unmodified COMPUTER.S C translation.  This is a
 * presentation-only rasterization of the original cartridge's authored text. */
static void computer_retail_record(AvpAvpGpuState *state,
                                   const char *record,u32 source_address,int clear)
{
    const u8 *p=(const u8 *)record;
    unsigned off=0u;
    int x=0,y=0;
    if(!state||!state->computer_active||!p)return;
    if(clear)computer_clear(state); /* a fresh FONT.S CLS, not an overlay */
    while(off+4u<=2048u){
        x=(s16)(((u16)p[off]<<8)|p[off+1u]);
        y=(s16)(((u16)p[off+2u]<<8)|p[off+3u]);
        off+=4u;
        for(;off<2048u;){
            unsigned ch=p[off++],b,sx,w;
            if(ch==0u)return;
            if(ch==0xffu){
                /* Cartridge address, not C malloc address, controls padding. */
                if((source_address+off)&1u)++off;
                break;
            }
            if(ch==0xfeu||ch==0xfdu)continue;
            if(ch==' '){x+=6;continue;}
            if(ch<33u)continue;
            if(computer_find_glyph(state,ch,&b,&sx,&w)){
                /* FONT.S advances to the next 15-pixel line when a glyph
                   would cross the 319-pixel typewriter edge. */
                if(x+(int)w>319){y+=15;x=0;}
                if(x>=0&&y>=0&&x<320&&y<240)
                    avp_gpu_computer_draw_glyph(state,b,sx,w,(unsigned)x,(unsigned)y,0);
                x+=(int)w;
            }else x+=6;
        }
    }
}

void avp_gpu_computer_draw_retail_menu(AvpAvpGpuState *state,const char *record,u32 source_address)
{
    computer_retail_record(state,record,source_address,1);
}
void avp_gpu_computer_draw_retail_overlay(AvpAvpGpuState *state,const char *record,u32 source_address)
{
    computer_retail_record(state,record,source_address,0);
}

static void computer_line(AvpAvpGpuState *state,int x0,int y0,int x1,int y1,uint32_t c)
{
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy;
    if(!state)return;
    for(;;){
        if(x0>=0&&x0<320&&y0>=0&&y0<240)state->computer_pixels[(size_t)y0*320u+(unsigned)x0]=0xff000000u|c;
        if(x0==x1&&y0==y1)break;
        {int e2=2*e;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}
    }
}
static void computer_menu(AvpAvpGpuState *state,unsigned tag,int opts)
{
    const char *a=NULL,*b=NULL,*c=NULL,*title=NULL;
    state->computer_menu_tag=tag;state->computer_menu_opts=opts;
    if(tag==0x47454e4du){title="SECURITY TERMINAL";a="SECURITY SCHEMATIC";b="LOG";if(opts>=3)c="SELF DESTRUCT";}
    else if(tag==0x41524d4du){title="ARMOURY TERMINAL";a="ARSENAL";b="LOG";}
    else if(tag==0x4d45444cu){title="MEDICAL LOGS";a="LOG";b="SPECIES DATABASE";}
    else if(tag==0x4d45444du){title="MEDICAL TERMINAL";a="FIRST AID";b="MEDICAL LOGS";}
    else if(tag==0x504f444du){title="ESCAPE POD";a="LAUNCH ESCAPE POD";}
    /* With retail FONT.S brushes bound, CPU pr_string() renders the exact
     * cartridge stream (including timed characters).  Host English remains
     * solely as a fallback for non-retail resource-less unit contexts. */
    if(state->computer_font[0])return;
    if(title)computer_text(state,42,42,title);
    if(a)computer_text(state,52,82,a);
    if(b)computer_text(state,52,102,b);
    if(c)computer_text(state,52,122,c);
}
void avp_gpu_computer_event(AvpAvpGpuState *state,unsigned event,s32 arg0,s32 arg1)
{
    int x,y;
    if(!state)return;
    if(event==0x4350414cu){ /* CPAL */
        state->computer_active=arg0?1u:0u;
        if(arg0){state->computer_obj1=state->computer_obj2=-1;state->computer_menu_tag=0;computer_clear(state);}
        return;
    }
    if(!state->computer_active)return;
    if(event==0x434c53u){computer_clear(state);state->computer_menu_tag=0;return;} /* CLS */
    if(event==0x434f31u){state->computer_obj1=arg0;return;}
    if(event==0x434f32u){state->computer_obj2=arg0;return;}
    if(event==0x43555253u){ /* CURS */
        /* The retail prpoint record (RDB.DTA $265AE) is one '>' glyph at
         * (35,80), with menu rows 20 pixels apart.  The former host-only
         * 16x70 rectangle erase destroyed unrelated CTEXT pixels on every
         * cursor poll.  Clear only the old glyph's actual brush footprint;
         * the original FONT.S glyph rasterizer owns the same 2bpp bounds. */
        unsigned brush,source_x,width;
        int previous_y=80+state->computer_cursor*20;
        if(state->computer_cursor>=0&&state->computer_cursor<3&&
           computer_find_glyph(state,(unsigned)'>',&brush,&source_x,&width))
            avp_gpu_computer_draw_glyph(state,brush,source_x,width,35u,
                                         (unsigned)previous_y,1);
        state->computer_cursor=(int)arg0;
        if(arg0>=0&&arg0<3)
            computer_text(state,35,80+(int)arg0*20,">");
        return;
    }
    if(event==0x484cu||event==0x564cu){
        x=(s16)((u32)arg0>>16);y=(s16)((u32)arg0&0xffffu);
        if(event==0x484cu)computer_line(state,x,y,x+arg1,y,computer_col(state,3u));
        else computer_line(state,x,y,x,y+arg1,computer_col(state,3u));
        return;
    }
    if(event==0x47454e4du||event==0x41524d4du||event==0x4d45444cu||
       event==0x4d45444du||event==0x504f444du){
        computer_clear(state);
        computer_menu(state,event,(int)arg0);return;
    }
    /* All these events introduce a new retail FONT.S text record.  Unlike
     * the page-footer overlay, they replace the previous CTEXT contents.
     * Clear before the first timed glyph without clearing the artwork layer. */
    if(event==0x504f444eu||event==0x5741524eu||event==0x44455354u||
       event==0x5343484du||event==0x4152534eu||event==0x53504543u||
       event==0x46414944u||event==0x4c41554eu)
        computer_clear(state);
    if(state->computer_font[0]){
        if(event==0x504f444eu||event==0x5741524eu||event==0x44455354u||
           event==0x5343484du||event==0x4152534eu||event==0x53504543u||
           event==0x46414944u||event==0x4c41554eu)return;
    }
    if(event==0x504f444eu){computer_text(state,52,84,"ESCAPE POD OFFLINE");computer_text(state,52,104,"SELF DESTRUCT NOT ACTIVE");return;}
    if(event==0x5741524eu){computer_text(state,42,78,"SELF DESTRUCT WARNING");computer_text(state,42,98,"A: ARM   C: CANCEL");return;}
    if(event==0x44455354u){computer_text(state,42,88,"SELF DESTRUCT ARMED");return;}
    if(event==0x5343484du){
        /* COMPUTER.S::scheme prints ex3_text after clearing CTEXT.  The level
           image itself is selected by the CPU's original maplist. */
        computer_text(state,30,210,"C. EXIT");return;
    }
    if(event==0x4152534eu){
        static const char *name[4]={"FLAMETHROWER","PULSE RIFLE","SHOTGUN","SMART GUN"};
        static const int x[4]={115,120,135,125};
        if(arg0>=0&&arg0<4)computer_text(state,x[arg0],52,name[arg0]);
        computer_text(state,65,195,"A. NEXT B. PREVIOUS C. EXIT");return;
    }
    if(event==0x53504543u){
        /* Literal 1994 COMPUTER.S::creatab/xxxtext fields.  The sixth entry
           skips the commented-out Jasonite item, as in the retail code. */
        static const char *info[6][5]={
            {"SPECIES: XT","LOCATION: UNKNOWN","ATMOSPHERE: HIGHLY ACIDIC","TERRAIN: HOSTILE","INTELLIGENCE: HIGH"},
            {"SPECIES: CRAVENOR","LOCATION: PLANET BRABENSD","ATMOSPHERE: OXYGEN","TERRAIN: MOUNTAINOUS","INTELLIGENCE: LIMITED"},
            {"SPECIES: SWAMPWORM","LOCATION: PLANET CHENG IM","ATMOSPHERE: SULPHUROUS","TERRAIN: SWAMPLANDS","INTELLIGENCE: MEDIOCRE"},
            {"SPECIES: ALIEN EGG","LOCATION: UNKNOWN","ATMOSPHERE: ACIDIC","TERRAIN: HOSTILE","INTELLIGENCE: NIL"},
            {"SPECIES: FACEHUGGER","LOCATION: UNKNOWN","ATMOSPHERE: ACIDIC","TERRAIN: HOSTILE","INTELLIGENCE: HIGH"},
            {"SPECIES: SNORGETH","LOCATION: PLANET SNORGRON","ATMOSPHERE: RADIOACTIVE","TERRAIN: HUGE LAKES","INTELLIGENCE: GENIUS"}
        };
        if(arg0>=0&&arg0<6)for(int n=0;n<5;n++)computer_text(state,100,80+n*20,info[arg0][n]);
        computer_text(state,100,200,"A. NEXT  B. PREVIOUS C. EXIT");return;
    }
    if(event==0x46414944u){
        if(arg0>0)computer_text(state,52,88,"FIRST AID APPLIED");
        else if(arg0==0)computer_text(state,52,88,"NO TREATMENT REQUIRED");
        else computer_text(state,52,88,"ACCESS DENIED");
        return;
    }
    if(event==0x4c41554eu){computer_text(state,52,88,"ESCAPE POD LAUNCHED");return;}
}
/* The native COMPUTER.S/OP terminal surface is 320x240.  Multiple host
 * pixels can select the same native artwork/text sample at higher window
 * resolutions.  Composite each native row once and expand the result using
 * the exact inverse of the previous floor(x*320/width) sampling rule.
 * No terminal simulation, palette, artwork or overlay state is cached. */
void avp_gpu_computer_present(const AvpAvpGpuState *state,AvpJaguarFramebuffer *fb)
{
    uint32_t native[320];
    unsigned spans[321];
    unsigned previous_sy=240u;
    if(!state||!fb||!state->computer_active)return;
    if(!fb->width||!fb->height)return;
    for(unsigned sx=0;sx<=320u;sx++)
        spans[sx]=(unsigned)(((uint64_t)sx*fb->width+319u)/320u);
    for(unsigned y=0;y<fb->height;y++){
        unsigned sy=(unsigned)(((uint64_t)y*240u)/fb->height);
        uint32_t *row=fb->pixels+(size_t)y*fb->width;
        if(sy>239u)sy=239u;
        /* A repeated Jaguar source line has the identical terminal pixels.
         * Reuse the previous finished host row instead of expanding all
         * 320 source spans again.  Every source-line transition still uses
         * exactly the original 320:width nearest-neighbour mapping. */
        if(sy==previous_sy){
            memcpy(row,row-fb->width,(size_t)fb->width*sizeof(*row));
            continue;
        }
        if(sy!=previous_sy){
            for(unsigned sx=0;sx<320u;sx++){
                uint32_t col;
                if(spans[sx]==spans[sx+1u])continue; /* not sampled at this width */
                col=computer_bg();
                if(state->computer_palette_valid&&state->computer_bg_img&&
                   state->computer_bg_img_size>=320u*240u)
                    col=state->computer_palette[state->computer_bg_img[(size_t)sy*320u+sx]];
                col=computer_art_pixel(state,state->computer_obj1,(int)sx,(int)sy,col);
                col=computer_art_pixel(state,state->computer_obj2,(int)sx,(int)sy,col);
                {uint32_t fg=state->computer_pixels[(size_t)sy*320u+sx];
                 if(fg&0xff000000u)col=fg&0x00ffffffu;}
                native[sx]=col;
            }
            previous_sy=sy;
        }
        for(unsigned sx=0;sx<320u;sx++)
            for(unsigned x=spans[sx];x<spans[sx+1u];x++)row[x]=native[sx];
    }
}

/* MAZE5 writes the $EC palette index into the HUD layer; zero pixels in
 * that layer are transparent when the Jaguar Object Processor displays it. */
/* One HUD-only compositor bridge for the static brush, live meters,
 * tracker, weapon bars and automap. OFF follows retail Jaguar RMW bit-for-
 * bit; ON substitutes only the destination hue, not the source palette,
 * brightness, ink or underlying world scene. */
static u32 hud_rmw_pixel(const AvpAvpGpuState *state,u32 backdrop,u16 ink)
{
    return state->hud_fixed_hue ?
        avp_jaguar_rmw_fixed_hue_xrgb8888(backdrop,ink,state->qconv,state->qconv_size) :
        avp_jaguar_rmw_xrgb8888(backdrop,ink,state->qconv,state->qconv_size);
}

static void hud_map_ink(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                        int x,int y,int x0,int y0,int x1,int y1)
{
    unsigned sp;
    size_t di;
    if(x<x0||x>=x1||y<y0||y>=y1||x<0||y<0||
       (unsigned)x>=fb->width||(unsigned)y>=fb->height)return;
    sp=(unsigned)player_type>>2;
    if(sp>2u)sp=0u;
    di=(size_t)y*fb->width+(unsigned)x;
    fb->pixels[di]=hud_rmw_pixel(state,fb->pixels[di],state->hud_active_pal[sp][0xecu&31u]);
}

/* The Jaguar expands MAZE5.GAS's one-bit map_data into an 8-bit
 * temporary image before rotating it into HUD.S's per-species map window.
 * Keep this native 320x228 HUD stage independent of world 3D resolution. */
static void overlay_automap(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                            const u8 *maze,u16 mw,u16 mh,double px,double py,
                            double fx,double fy,double rx,double ry)
{
    /* HUD.S stores map_info in the 320x200 HUD bitmap's local coords.
     * MAZESCRN.S positions that Object Processor bitmap at Y=14.  The
     * map is drawn over the scene only after this OP translation. */
    int mx0=map_info.x,my0=map_info.y+14,mwpx=map_info.w,mhpx=map_info.h;
    int mcx=mx0+mwpx/2,mcy=my0+mhpx/2;
    int sy,sx;
    u32 viewpoint_x,viewpoint_y;
    s32 viewpoint_sin,viewpoint_cos;
    double zoom=1.0;
    (void)rx;(void)ry;
    /* Original output for single-player and for the default 1x setting. */
    if(state->map_zoom_steps>0)
        for(int z=0;z<state->map_zoom_steps;z++)zoom*=1.25;
    else
        for(int z=0;z>state->map_zoom_steps;z--)zoom/=1.25;
    if(!map_on||!maze||mw==0||mh==0||!state||!fb)return;
    viewpoint_x=state->map_smooth_camera?(u32)(px*65536.0):x_pos;
    viewpoint_y=state->map_smooth_camera?(u32)(py*65536.0):y_pos;
    /* sin_d0/cos_d0 are signed 1.15 fixed point (peak 32767).
     * Smooth Presentation must use the same scale: 16384 magnified the
     * automap by 2x whenever interpolation was enabled. */
    viewpoint_sin=state->map_smooth_camera?(s32)(-fy*32767.0):sin_ang;
    viewpoint_cos=state->map_smooth_camera?(s32)(fx*32767.0):cos_ang;
    for(sy=my0;sy<my0+mhpx;sy++){
        for(sx=mx0;sx<mx0+mwpx;sx++){
            int ix,iy,lit=0;
            /* Keep the MAZE5.GAS 16.16 blitter transformation unchanged.
             * Smooth Presentation substitutes only the host's interpolated
             * player viewpoint; explored map pixels remain simulation-owned. */
            avp_gpu_automap_sample(viewpoint_x,viewpoint_y,viewpoint_sin,viewpoint_cos,
                                   (unsigned)mwpx,(unsigned)mhpx,
                                   (int)lrint((double)(sx-mcx)/zoom)+(mwpx/2),
                                   (int)lrint((double)(sy-mcy)/zoom)+(mhpx/2),&ix,&iy);
            if(ix>=0&&iy>=0&&ix<512&&iy<512)
                lit=(state->automap_bits[(size_t)iy*AVP_AUTOMAP_ROW_BYTES+(unsigned)ix/8u]&
                     (0x80u>>(unsigned)(ix&7)))!=0;
            /* MAZE5.GAS writes 0xEC to the PIXEL8 HUD bitmap where a map
             * bit is set, and index zero everywhere else. In the original
             * HUD Object Processor layer zero is transparent: never paint
             * solid black over the 3D world for an absent map bit. */
            if(lit)hud_map_ink(state,fb,sx,sy,mx0,my0,mx0+mwpx,my0+mhpx);
        }
    }
    /* Project teammate positions through the INVERSE of the unchanged
     * Jaguar MAZE5 fixed-point map transform. Only living same-level
     * participants are submitted by the local multiplayer host. No fog/map reveal. */
    if(!state->map_hide_teammates && state->map_teammate_count){
        double sn=(double)viewpoint_sin,cs=(double)viewpoint_cos;
        double denom=2.0*(sn*sn+cs*cs);
        if(denom>1.0)for(unsigned i=0;i<state->map_teammate_count;i++){
            /* The camera already uses the interpolated host pose. Drawing
             * dots from raw 10-Hz scanner contacts makes them jump relative
             * to the otherwise smooth rotating/translating automap. */
            const AvpGpuMapTeammate *dots=state->map_smooth_camera?
                state->map_teammates_draw:state->map_teammates;
            double dx=((double)(s32)dots[i].x-(double)(s32)viewpoint_x)*8.0;
            double dy=((double)(s32)dots[i].y-(double)(s32)viewpoint_y)*8.0;
            int xx=mcx+(int)lrint(((sn*dx+cs*dy)/denom)*zoom);
            int yy=mcy+(int)lrint(((-cs*dx+sn*dy)/denom)*zoom);
            for(int cy=0;cy<2;cy++)for(int cx=0;cx<2;cx++)
                hud_map_ink(state,fb,xx-1+cx,yy-1+cy,
                            mx0,my0,mx0+mwpx,my0+mhpx);
        }
    }
    /* MAZE5.GAS ends with a distinct 2x2 player blit at the centre. */
    for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++)
        hud_map_ink(state,fb,mcx-1+xx,mcy-1+yy,
                    mx0,my0,mx0+mwpx,my0+mhpx);
}

/* Jaguar MAZESCRN.S places the 320x200 HUD object at screen Y=14.
 * MAZE4 tracker coordinates below are HUD-local, just like weapon bar Y.
 * Apply the offset to the whole dynamic scanner, not just its static brushes. */
enum { TRACKER_HUD_SCREEN_Y = 14 };

/* MAZE4 paints the original scanner into an indexed HUD scratch picture;
 * the Jaguar Object Processor composites that finished picture once.  R105
 * instead blended each brush, compass line, blip and digit directly into
 * already-composited world pixels.  Multiple non-opaque overlays compounded
 * the CRY RMW shade and produced bright square numeral cells. */
enum { TRACKER_TILE_X=16, TRACKER_TILE_Y=111,
       TRACKER_TILE_W=112, TRACKER_TILE_H=89 };
typedef struct AvpTrackerCanvas { u8 pixel[TRACKER_TILE_W*TRACKER_TILE_H]; } AvpTrackerCanvas;

static void tracker_put(AvpTrackerCanvas *canvas,int x,int y,u8 pixel)
{
    if(!canvas||x<TRACKER_TILE_X||x>=TRACKER_TILE_X+TRACKER_TILE_W||
       y<TRACKER_TILE_Y||y>=TRACKER_TILE_Y+TRACKER_TILE_H)return;
    canvas->pixel[(size_t)(y-TRACKER_TILE_Y)*TRACKER_TILE_W+(unsigned)(x-TRACKER_TILE_X)]=pixel;
}

static void tracker_blit_indexed(AvpTrackerCanvas *canvas,
                                 const u8 *src,size_t src_size,int sw,int sh,int dx,int dy)
{
    if(!canvas||!src||sw<=0||sh<=0||src_size<(size_t)sw*(size_t)sh)return;
    for(int y=0;y<sh;y++)for(int x=0;x<sw;x++){
        u8 pi=src[(size_t)y*(size_t)sw+(size_t)x];
        if(pi)tracker_put(canvas,dx+x,dy+y,pi);
    }
}

/* MAZE4 operates on the 320x200 *HUD bitmap*, not on the displayed frame.
 * The upper and lower scanner brushes abut at HUD-local y=175.  The Jaguar
 * OP's 14-pixel vertical offset belongs solely in tracker_composite().
 * Applying it here as well (R106) displaced both brushes and clipped the
 * meter panel, leaving the distance glyphs on top of the wrong artwork. */
static void tracker_restore_upper(const AvpAvpGpuState *state,AvpTrackerCanvas *canvas)
{
    tracker_blit_indexed(canvas,state->tracker_base1,state->tracker_base1_size,
                         96,64,16,111);
}
static void tracker_restore_lower(const AvpAvpGpuState *state,AvpTrackerCanvas *canvas)
{
    tracker_blit_indexed(canvas,state->tracker_base2,state->tracker_base2_size,
                         96,25,16,175);
    tracker_blit_indexed(canvas,state->tracker_basex,state->tracker_basex_size,
                         16,2,112,198);
}

static int tracker_bit(const u8 *src,size_t src_size,int stride_bytes,int x,int y)
{
    size_t off;
    if(!src||x<0||y<0||stride_bytes<=0)return 0;
    off=(size_t)y*(size_t)stride_bytes+(unsigned)x/8u;
    if(off>=src_size)return 0;
    return (src[off]&(u8)(0x80u>>((unsigned)x&7u)))!=0;
}

static int hud_font1_bit(const u8 *font,size_t font_size,int stride_bytes,int x,int y)
{
    size_t off;
    if(!font||x<0||y<0||stride_bytes<=0)return 0;
    off=(size_t)y*(size_t)stride_bytes+(unsigned)x/8u;
    if(off>=font_size)return 0;
    return (font[off]&(u8)(0x80u>>((unsigned)x&7u)))!=0;
}

static void hud_put_palette_index(const AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                                  unsigned sp,int x,int y,u8 pixel)
{
    size_t di;unsigned idx;
    if(!state||!fb||sp>=3u||x<0||y<0||x>=(int)fb->width||y>=(int)fb->height)return;
    idx=(unsigned)pixel&31u;di=(size_t)y*fb->width+(unsigned)x;
    fb->pixels[di]=hud_rmw_pixel(state,fb->pixels[di],state->hud_active_pal[sp][idx]);
}

static void tracker_composite(const AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                              const AvpTrackerCanvas *canvas)
{
    if(!state||!fb||!canvas)return;
    for(int y=0;y<TRACKER_TILE_H;y++)for(int x=0;x<TRACKER_TILE_W;x++){
        u8 pi=canvas->pixel[(size_t)y*TRACKER_TILE_W+(unsigned)x];
        if(pi)hud_put_palette_index(state,fb,0,TRACKER_TILE_X+x,
                                     TRACKER_TILE_Y+y+TRACKER_HUD_SCREEN_Y,pi);
    }
}

static void hud_draw_decimal_1bpp(const AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                                  unsigned sp,const u8 *font,size_t font_size,int font_stride,
                                  unsigned value,unsigned digits,int x,int y,int char_w,int char_h,
                                  int spacing,int suppress_zeroes,int draw_bg,u8 bg,u8 fg)
{
    unsigned pow10=1u;
    int found=0;
    if(!font||digits==0u)return;
    for(unsigned i=1u;i<digits;i++)pow10*=10u;
    for(unsigned pos=0;pos<digits;pos++){
        unsigned d=pow10?value/pow10:value;
        if(pow10)value%=pow10;
        if(pow10>1u)pow10/=10u;else pow10=0u;
        if(d>9u)d=9u;
        if(pos+1u==digits||d!=0u)found=1;
        if(draw_bg)for(int yy=0;yy<char_h;yy++)for(int xx=0;xx<char_w;xx++)hud_put_palette_index(state,fb,sp,x+xx,y+yy,bg);
        if(!suppress_zeroes||found){int sx=(int)d*char_w;for(int yy=0;yy<char_h;yy++)for(int xx=0;xx<char_w;xx++)if(hud_font1_bit(font,font_size,font_stride,sx+xx,yy))hud_put_palette_index(state,fb,sp,x+xx,y+yy,fg);}
        x+=char_w+spacing;
    }
}

static void hud_draw_decimal_8bpp(const AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                                  unsigned sp,const u8 *font,size_t font_size,int font_stride,
                                  unsigned value,unsigned digits,int x,int y,int char_w,int char_h,
                                  int suppress_zeroes)
{
    unsigned pow10=1u;int found=0;
    if(!font||digits==0u)return;
    for(unsigned i=1u;i<digits;i++)pow10*=10u;
    for(unsigned pos=0;pos<digits;pos++){
        unsigned d=pow10?value/pow10:value;
        if(pow10)value%=pow10;
        if(pow10>1u)pow10/=10u;else pow10=0u;
        if(d>9u)d=9u;
        if(pos+1u==digits||d!=0u)found=1;
        if(!suppress_zeroes||found){int sx=(int)d*char_w;for(int yy=0;yy<char_h;yy++)for(int xx=0;xx<char_w;xx++){size_t off=(size_t)yy*(size_t)font_stride+(size_t)(sx+xx);if(off<font_size&&font[off])hud_put_palette_index(state,fb,sp,x+xx,y+yy,font[off]);}}
        x+=char_w;
    }
}

/* OVERS.DB::tinyfont has 16 bytes per row, five rows, and 8 source bits
 * per character; the first four pixels are its actual 3px numeral + spacing.
 * MAZE4's tracker packs the two hundredths into adjoining 4px cells.
 * Do not paint overlapping eight-pixel backgrounds across the preceding digit. */
static void tracker_draw_hundredths(const AvpAvpGpuState *state,AvpTrackerCanvas *canvas,
                                    unsigned hundredths,int x,int y)
{
    if(!state||!state->hud_tiny_font||state->hud_tiny_font_size<80u)return;
    for(unsigned pos=0;pos<2u;pos++){
        unsigned digit=(pos==0u)?((hundredths/10u)%10u):(hundredths%10u);
        for(int yy=0;yy<5;yy++)for(int xx=0;xx<4;xx++){
            int sx=(int)digit*8+xx;
            u8 ink=hud_font1_bit(state->hud_tiny_font,state->hud_tiny_font_size,16,sx,yy)?0xe3u:0xebu;
            tracker_put(canvas,x+(int)pos*4+xx,y+yy,ink);
        }
    }
}

static void tracker_draw_whole(const AvpAvpGpuState *state,AvpTrackerCanvas *canvas,
                               unsigned whole,int x,int y)
{
    if(!state||!state->hud_score_font[0]||state->hud_score_font_size[0]<90u)return;
    /* The Marine HUD stores ten adjacent 8x9 1bpp numeral glyphs.  Replace
     * both complete digit cells *in the indexed picture*, retaining the
     * original $EB background and $E3 foreground pixels. */
    for(unsigned pos=0;pos<2u;pos++){
        unsigned d=(pos==0u)?((whole/10u)%10u):(whole%10u);
        for(int yy=0;yy<9;yy++)for(int xx=0;xx<8;xx++){
            u8 ink=hud_font1_bit(state->hud_score_font[0],state->hud_score_font_size[0],10,(int)d*8+xx,yy)?0xe3u:0xebu;
            tracker_put(canvas,x+(int)pos*10+xx,y+yy,ink);
        }
    }
}

static void tracker_line_indexed(AvpTrackerCanvas *canvas,
                                 int x0,int y0,int x1,int y1,u8 pixel)
{
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy;
    for(;;){
        if(x0>=16&&x0<112&&y0>=111&&y0<200)
            tracker_put(canvas,x0,y0,pixel);
        if(x0==x1&&y0==y1)break;
        {int e2=2*e;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}
    }
}

static void tracker_draw_ring(AvpAvpGpuState *state,AvpTrackerCanvas *canvas)
{
    const unsigned frame=avp_hud_tracker_frame()&7u;
    const size_t frame_bytes=16u*60u;
    if(state&&state->tracker_ring&&state->tracker_ring_size>=(size_t)(frame+1u)*frame_bytes){
        for(unsigned y=0;y<60u;y++)for(unsigned x=0;x<128u;x++)
            if(avp_gpu_tracker_ring_bit(state->tracker_ring,state->tracker_ring_size,frame,x,y))
                tracker_put(canvas,24+(int)x,115+(int)y,0xedu);
    }else{
        /* Retain a minimal ring in asset-free diagnostic geometry only. */
        int r=7+(int)frame*4;
        for(int d=0;d<360;d+=5){double q=(double)d*(M_PI/180.0);
            int x=64+(int)lrint(cos(q)*r),y=154+(int)lrint(sin(q)*r);
            if(x>=16&&x<112&&y>=111&&y<200)
                tracker_put(canvas,x,y,0xedu);
        }
    }
}

u32 avp_gpu_tracker_nearest_contact(u32 xpos,u32 ypos,u32 angle)
{
    const double px=(double)(s32)xpos/65536.0,py=(double)(s32)ypos/65536.0;
    const double radians=-(double)((u16)(angle>>16))*(2.0*M_PI/65536.0);
    const double fx=cos(radians),fy=sin(radians),rx=-fy,ry=fx;
    double nearest=15.0;
    for(unsigned i=0;i<AVP_NUM_AMPS;i++){
        const AvpAmp *a=&level1amps[i];
        double dx,dy,lat,forward,d;
        if(!(a->mode||a->host_static)||!(a->flags&(1u<<AMP_MOTION)))continue;
        dx=(double)a->xpos/65536.0-px;
        dy=(double)a->ypos/65536.0-py;
        lat=dx*rx+dy*ry;forward=dx*fx+dy*fy;
        d=avp_gpu_tracker_contact_metres(lat,forward);
        if(d<nearest)nearest=d;
    }
    return (u32)lrint(nearest*65536.0);
}

/* Multiplayer extension: living same-level participants are supplied in
 * the per-view roster, independently of the original AMP allocator.  The
 * unchanged single-player path still uses the original AMP-only scanner. */
u32 avp_gpu_tracker_nearest_contact_for_view(const AvpAvpGpuState *state,
    u32 xpos,u32 ypos,u32 angle)
{
    double nearest=(double)avp_gpu_tracker_nearest_contact(xpos,ypos,angle)/65536.0;
    const double px=(double)(s32)xpos/65536.0,py=(double)(s32)ypos/65536.0;
    const double radians=-(double)((u16)(angle>>16))*(2.0*M_PI/65536.0);
    const double fx=cos(radians),fy=sin(radians),rx=-fy,ry=fx;
    if(state)for(unsigned i=0;i<state->map_teammate_count;i++){
        const double dx=(double)(s32)state->map_teammates[i].x/65536.0-px;
        const double dy=(double)(s32)state->map_teammates[i].y/65536.0-py;
        const double d=avp_gpu_tracker_contact_metres(dx*rx+dy*ry,dx*fx+dy*fy);
        if(d<nearest)nearest=d;
    }
    return (u32)lrint(nearest*65536.0);
}

static void overlay_motion_tracker(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,double px,double py,
                                   double fx,double fy,double rx,double ry)
{
    static const int mt_lines[][4]={
        {0,-14,0,-29},{-3,-31,3,-31},{-1,-32,1,-32},{-2,-33,2,-33},{-1,-35,1,-35},{0,-34,0,-37},
        {-14,0,-33,0},{-34,-3,-35,0},{-35,0,-34,3},{-37,0,-36,-3},{-36,-4,-35,-5},{-37,0,-36,3},{-36,4,-35,5},
        {14,0,33,0},{34,-3,35,0},{35,0,34,3},{37,0,36,-3},{36,-4,35,-5},{37,0,36,3},{36,4,35,5},
        {0,14,0,33},{-3,34,0,35},{0,35,3,34},
        {10,-10,27,-27},{-10,-10,-27,-27},{10,10,27,27},{-10,10,-27,27},
        {-1,0,1,0},{0,-1,0,1}
    };
    const double max_m=15.0;
    /* MAZE4.GAS assembles MT_RESCALE with integer division: (35*256)/15.
     * The preceding double expression rounded 35.5/15 and shifted blips. */
    const double rescale=(double)(((79-8)/2)*256/15)/256.0;
    const int xc=64,yc=154;
    double nearest=max_m;
    AvpTrackerCanvas canvas={{0}};
    unsigned i;
    if(player_type!=PT_HUMAN||!show_mt)return;

    /* Restore the original upper scanner brush to the HUD-local picture
       before the compass/rings/contacts.  The screen Y offset is not applied
       until the complete tracker is composited. */
    tracker_restore_upper(state,&canvas);

    /* The compass is a world-fixed overlay seen from the turning player;
       its lines must rotate against camera heading, rather than WITH it.
       Creature dots already use the camera-relative right/forward basis and
       must not be horizontally mirrored a second time. */
    {double ca=-atan2(fy,fx),cs=cos(ca),sn=sin(ca);
    for(i=0;i<sizeof(mt_lines)/sizeof(mt_lines[0]);i++){
        double x0=mt_lines[i][0],y0=mt_lines[i][1],x1=mt_lines[i][2],y1=mt_lines[i][3];
        int ax=xc+(int)lrint(x0*cs-y0*sn),ay=yc+(int)lrint(y0*cs+x0*sn);
        int bx=xc+(int)lrint(x1*cs-y1*sn),by=yc+(int)lrint(y1*cs+x1*sn);
        tracker_line_indexed(&canvas,ax,ay,bx,by,0xeau);
    }}

    /* MAZE4.GAS draws the allscope ring *before* creature dots, so the
       latter stay visible when the two brushes overlap. */
    tracker_draw_ring(state,&canvas);

    /* All native AMPs first, then the multiplayer host's filtered roster.
     * The latter contains only living same-level participants (co-op AND
     * Deathmatch), not the visual corpse sprites or a different level's
     * actors. Both use the original BLIPS brush and the same 15-m range. */
    for(i=0;i<AVP_NUM_AMPS+(state?state->map_teammate_count:0u);i++){
        double dx,dy,lat,fwd,mx,my,len;
        if(i<AVP_NUM_AMPS){
            const AvpAmp *a=&level1amps[i];
            if(!(a->mode||a->host_static)||!(a->flags&(1u<<AMP_MOTION)))continue;
            dx=(double)a->xpos/65536.0-px;dy=(double)a->ypos/65536.0-py;
        }else{
            const AvpGpuMapTeammate *a=&state->map_teammates[i-AVP_NUM_AMPS];
            dx=(double)(s32)a->x/65536.0-px;
            dy=(double)(s32)a->y/65536.0-py;
        }
        lat=dx*rx+dy*ry; fwd=dx*fx+dy*fy;
        mx=lat*3.0; my=fwd*3.0;
        len=avp_gpu_tracker_contact_metres(lat,fwd);
        if(len>=max_m)continue;
        if(len<nearest)nearest=len;
        {int sx=xc+(int)lrint(mx*rescale),sy=yc-(int)lrint(my*rescale);
         /* BLIPS is one 64x7 1bpp brush containing eight authored 8x7 cells.
            The retail selector is deliberately position-derived; use the same
            stable position bits rather than tying dot animation to host FPS. */
         /* MAZE1.GAS packs the sprite's CAMERA-space x/z into gmp_xpos.
          * MAZE4.GAS then selects the BLIPS cell from abs(x)|abs(z).
          * Absolute world coordinates made a stationary contact change its
          * apparent animation depending on where the level was placed. */
         s16 view_x=(s16)(s32)lrint(lat*256.0);
         s16 view_z=(s16)(s32)lrint(fwd*256.0);
         unsigned phase=avp_gpu_tracker_blip_index(view_x,view_z);
         if(state&&state->tracker_blips&&state->tracker_blips_size>=56u){
             for(int yy=0;yy<7;yy++)for(int xx=0;xx<8;xx++)
                 if(tracker_bit(state->tracker_blips,state->tracker_blips_size,8,(int)phase*8+xx,yy))
                     if(sx-4+xx>=16&&sx-4+xx<112&&sy-3+yy>=111&&sy-3+yy<200)
                         tracker_put(&canvas,sx-4+xx,sy-3+yy,0xecu);
         }else{
             for(int yy=0;yy<7;yy++)for(int xx=0;xx<8;xx++)
                 if((((xx+yy+(int)phase)&3)==0)||((xx==3||xx==4)&&(yy>=2&&yy<=4)))
                     if(sx-4+xx>=16&&sx-4+xx<112&&sy-3+yy>=111&&sy-3+yy<200)
                         tracker_put(&canvas,sx-4+xx,sy-3+yy,0xecu);
         }
        }
    }
    /* The nearest-contact handoff for tracker SFX happens at the original
       HUD_EV_TRACKER/TC simulation event, not in this presentation function. */

    /* Restore the lower brush at local Y=175, directly adjoining the upper
       Y=111..174 brush, then print the numbers over its readout window. */
    tracker_restore_lower(state,&canvas);

    /* MAZE4 prints the nearest contact in two 8x9 main digits at local (48,184)
       followed by two 4x5 hundredths at local (70,188).  The main digits use the
       Marine score font; the small digits use OVERS.DB::tinyfont.  The GPU
       uses $EB as background and $E3 as foreground, whose low five bits are
       the HUD CLUT/RMW indices consumed by the hosted Jaguar compositor. */
    if(state&&state->hud_score_font[0]&&state->hud_score_font_size[0]>=90u){
        u32 nd=(u32)lrint(nearest*65536.0);unsigned whole=(nd>>16)&0xffffu;
        unsigned hundredths=(unsigned)((((u64)(nd&0xffffu))*100u)>>16);
        tracker_draw_whole(state,&canvas,whole,48,184);
        tracker_draw_hundredths(state,&canvas,hundredths,70,188);
    }
    /* One RMW pass, just as with the original Jaguar HUD bitmap/OP layer. */
    tracker_composite(state,fb,&canvas);
}

/* All HUD positions in HUD.S/MAZE4.GAS are authored for 320x228.  Draw
 * against one native surface, not partly in host pixels and partly native. */
/* The tracker is a separate overlay over the Marine's native HUD bitmap.
 * Retail HUD record 185 has the x=78..79 divider through local y=111..197;
 * the original tracker occupies that area using the separate FILES 192/193
 * brushes.  Its transparent pixels cannot erase the main HUD divider.
 *
 * NOTE: overlay_hud_native() supplies *screen-space* Y (the loop begins at
 * y=14), while the retail HUD and tracker coordinates are HUD-LOCAL.  R108
 * compared those screen coordinates directly against 111..197, leaving the
 * last 14 rows of the unwanted line visible across the meter readout.
 * Convert the bounds once; retain the bottom border at local y=198..199. */
/* In retail MAZE4 the scanner brushes replace a rectangle of the 320x200
 * indexed Marine HUD picture before the single Jaguar OP RMW composite.
 * The PC port must therefore omit ALL original HUD pixels inside that
 * rectangle, not merely the x=78..79 divider. Otherwise every pixel where
 * the base HUD border and the scanner overlap is blended twice, making the
 * left circular outline and lower frame unnaturally bright. The C4 scbasex
 * strip adds the final 16x2 pixels at HUD-local x=112..127, y=198..199.
 * Screen coordinates are exactly HUD-local Y + the OP's 14-pixel offset.
 * Tracker-off and other species HUDs must be byte-for-byte unaffected. */
static int tracker_replaces_main_hud_pixel(unsigned species,int tracker_visible,
                                           int x,int screen_y)
{
    const int y=screen_y-TRACKER_HUD_SCREEN_Y;
    if(species!=0u||!tracker_visible)return 0;
    return (x>=16&&x<112&&y>=111&&y<200) ||
           (x>=112&&x<128&&y>=198&&y<200);
}

static void overlay_hud_native(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                        const u8 *maze,u16 mw,u16 mh,double px,double py,
                        double fx,double fy,double rx,double ry,unsigned logical_level)
{
    (void)logical_level; /* Rendering now reads the per-level explored bitmap. */
    unsigned sp=(unsigned)player_type>>2;
    const u8 *base;
    size_t n;
    u16 rmwpal[32];
    if(sp>2u)sp=0u;
    for(unsigned species=0;species<3u;species++)
        avp_jaguar_build_hud_rmw_palette(state->hud_pal_cry[species],(int)hud_bright,state->hud_active_pal[species]);
    memcpy(rmwpal,state->hud_active_pal[sp],sizeof(rmwpal));
    base=state->hud_base[sp];n=state->hud_base_size[sp];
    if(base&&n>=320u*200u){
        for(int y=14;y<214;y++)for(int x=0;x<320;x++){
            u8 pi=base[(size_t)(y-14)*320u+(size_t)x];
            /* The retail HUD brushes contain demonstration numerals in
               their score/security fields.  HUD.S::print_18/print_88
               overwrites those pixels *in the HUD bitmap*, not directly
               at screen origin.  Leave their pixels transparent here and
               draw the current values at HUD-local Y plus the +14 OP Y. */
            int value_cell=(sp==0u && ((x>=151&&x<231&&y>=22&&y<31) ||
                                        (x>=43&&x<59&&y>=104&&y<113))) ||
                           (sp==1u && x>=148&&x<228&&y>=24&&y<34) ||
                           (sp==2u && x>=111&&x<221&&y>=24&&y<36);
            /* UpdtNrg clears the entire former energy strip in hud_pic
             * before copying the visible part of its animated frame.  An
             * unmodified FILES HUD brush must not restore old bar pixels. */
            int energy_cell=(x>=nrg_x&&x<nrg_x+nrg_barwidth&&
                             y>=nrg_y+14&&y<nrg_y+14+nrg_height);
            /* HP2 also clears both 43-pixel acoustic scan rows before
             * drawing the two short spans between successive samples. */
            /* The authored Predator sound trace is a retained 43x45 raster,
             * not a static illustration with only its last two rows erased. */
            int meter_cell=(sp==2u&&state->predator_scope_valid&&
                            x>=AVP_PRED_SCOPE_X&&x<AVP_PRED_SCOPE_X+AVP_PRED_SCOPE_W&&
                            y>=AVP_PRED_SCOPE_Y+14&&y<AVP_PRED_SCOPE_Y+14+AVP_PRED_SCOPE_H);
            int ammo_cell=0;
            for(int wi=0;wi<num_weps&&wi<6;wi++){
                const AvpAmmoInfo *a=&ammo_info[wi];
                if(x>=a->x&&x<a->x+44&&y>=a->y+14&&y<a->y+22){
                    ammo_cell=1;break;
                }
            }
            /* PX HUD icons are sourced from the cartridge HUD picture and
             * shaded in-place by MAZESCRN.S::PXFades/x_fade. Do not show both
             * eyes/medpak at permanent full brightness. */
            /* Original ROM MAZESCRN Blitter: $EAC4 restores/shades each
             * weapon icon from the untouched HUD source; $EB52 applies
             * the alternating-pixel unavailable-weapon mask.  Use the
             * ROM-defined rectangle, never the ammo-bar bounding box. */
            pi=avp_gpu_weapon_icon_pixel(pi,sp,x,y-14,cur_weps,
                                          inwep_no,inwep_bright,
                                          outwep_no,outwep_bright);
            if(sp==2u)pi=avp_gpu_predator_faded_icon(pi,x,y-14,
                                                      invis_bright,medpak_bright);
            /* The original indexed HUD picture is overwritten by the
             * scanner brushes before the OP composites it. Suppress the
             * underlying HUD only inside those original brush footprints. */
            if(pi&&!tracker_replaces_main_hud_pixel(sp,show_mt!=0,x,y)&&
               !value_cell&&!energy_cell&&!meter_cell&&!ammo_cell){size_t di=(size_t)y*320u+(size_t)x;
                fb->pixels[di]=hud_rmw_pixel(state,fb->pixels[di],rmwpal[pi&31u]);}
        }
    }
    if(state->hud_energy[sp]&&nrg_width>0&&nrg_height>0&&nrg_nframes>0){
        size_t frame_sz=(size_t)nrg_width*(size_t)nrg_height;
        size_t off=(size_t)(avp_hud_energy_frame()%(unsigned)nrg_nframes)*frame_sz;
        if(off+frame_sz<=state->hud_energy_size[sp]){
            const u8 *e=state->hud_energy[sp]+off;
            unsigned visible=avp_hud_energy_pixels();
            if(visible>(unsigned)nrg_barwidth)visible=(unsigned)nrg_barwidth;
            for(int y=0;y<nrg_height;y++)for(unsigned x=0;x<visible;x++){
                int dx=nrg_x+(int)x,dy=nrg_y+14+y;
                if(dx<0||dx>=320||dy<0||dy>=228)continue;
                {u8 pi=e[(size_t)y*(size_t)nrg_width+x];
                 if(pi){size_t di=(size_t)dy*320u+(unsigned)dx;
                     fb->pixels[di]=hud_rmw_pixel(state,fb->pixels[di],rmwpal[pi&31u]);}}
            }
        }
    }
    /* MAZESCRN.S::wep_bars clears 44x8 pixels per weapon before copying
       the active span from wepbar_j8. Source PIXEL8|WIDTH48 starts at (0,0),
       and uses the species HUD's palette/RMW blending. */
    if(state->hud_weapon_bar&&state->hud_weapon_bar_size>=48u*8u){
        for(int wi=0;wi<num_weps&&wi<6;wi++){
            const AvpAmmoInfo *a=&ammo_info[wi];
            unsigned count=0;
            if(a->cur>0&&a->max>0){
                count=(unsigned)((((u32)a->cur*(u32)a->rescale)>>16)+1u);
                if(count>44u)count=44u;
            }
            for(unsigned row=0;row<8u;row++)for(unsigned col=0;col<count;col++){
                int dx=a->x+(int)col,dy=a->y+14+(int)row;
                u8 pi=state->hud_weapon_bar[row*48u+col];
                if(pi&&dx>=0&&dx<320&&dy>=0&&dy<228){
                    size_t di=(size_t)dy*320u+(unsigned)dx;
                    fb->pixels[di]=hud_rmw_pixel(state,fb->pixels[di],rmwpal[pi&31u]);
                }
            }
        }
    }
    /* The original Alien HUD animates three 32x32 egg/cocoon indicators at
       the fixed positions in HUD.S::cocoon_pos and COCOON_X. */
    if(sp==1u&&state->hud_cocoon&&state->hud_cocoon_size>=18u*1024u){
        static const int ys[3]={90,130,170};
        for(unsigned k=0;k<3u;k++){
            int f=cocoon_data[k].frame;
            if(f<0||f>=18)continue;
            const u8 *frame=state->hud_cocoon+(size_t)f*1024u;
            for(int y=0;y<32;y++)for(int x=0;x<32;x++){
                u8 pi=frame[(size_t)y*32u+(size_t)x];
                if(pi){size_t di=(size_t)(ys[k]+y)*320u+(size_t)(44+x);
                    fb->pixels[di]=hud_rmw_pixel(state,fb->pixels[di],rmwpal[pi&31u]);}
            }
        }
    }
    if(state->hud_score_font[sp]){
        unsigned shown=(unsigned)(score<0?0:score);
        if(sp==0u){
            if(show_coords)shown=(unsigned)(u16)player_energy;
            hud_draw_decimal_1bpp(state,fb,sp,state->hud_score_font[sp],state->hud_score_font_size[sp],10,
                                  shown,10u,151,22,8,9,0,1,0,0,0xebu);
            hud_draw_decimal_1bpp(state,fb,sp,state->hud_score_font[sp],state->hud_score_font_size[sp],10,
                                  (unsigned)acs_level,2u,43,104,8,9,0,0,0,0,0xebu);
        }else if(sp==1u){
            hud_draw_decimal_8bpp(state,fb,sp,state->hud_score_font[sp],state->hud_score_font_size[sp],80,
                                  shown,10u,148,24,8,10,1);
        }else{
            hud_draw_decimal_8bpp(state,fb,sp,state->hud_score_font[sp],state->hud_score_font_size[sp],112,
                                  shown,10u,111,24,11,12,1);
        }
    }
    /* Show the retained Jaguar sound trace. HP2 advances its last two rows
     * at the original simulation rate via the platform Object Processor
     * event, never from a potentially 60+ FPS host presentation callback. */
    if(avp_mp_scope_visible(sp) && state->predator_scope_valid){
        /* The Jaguar sound analyzer belongs to the Predator HUD only.
         * Never paint it into a Marine or Alien HUD, even when a nearby
         * Predator can hear that player's actual synthesized sounds. */
        const int scope_x=avp_mp_scope_origin_x(sp);
        for(unsigned row=0;row<AVP_PRED_SCOPE_H;row++)
            for(unsigned col=0;col<AVP_PRED_SCOPE_W;col++){
                u8 pi=state->predator_scope[row*AVP_PRED_SCOPE_W+col];
                if(pi)hud_put_palette_index(state,fb,sp,
                    scope_x+(int)col,
                    AVP_PRED_SCOPE_Y+14+(int)row,pi);
            }
    }
    overlay_automap(state,fb,maze,mw,mh,px,py,fx,fy,rx,ry);
    overlay_motion_tracker(state,fb,px,py,fx,fy,rx,ry);
}

static int hmsg_glyph_index(unsigned char c)
{
    if(c>='A'&&c<='Z')return (int)(c-'A');
    if(c=='#')return 26;
    if(c>='0'&&c<='9')return 27+(int)(c-'0');
    if(c==':')return 37;
    return -1;
}
static int hmsg_font_bit(const AvpAvpGpuState*state,int x,int y)
{
    size_t off;unsigned char b;
    if(!state->hmsg_font||state->hmsg_font_size<64u*19u||x<0||x>=512||y<0||y>=19)return 0;
    off=(size_t)y*64u+(unsigned)x/8u;b=state->hmsg_font[off];
    return (b&(0x80u>>((unsigned)x&7u)))!=0;
}
static void draw_hud_messages_native(AvpAvpGpuState*state,AvpJaguarFramebuffer*fb)
{
    static const int line_y[3]={64,87,110};
    unsigned i;int base_x=(player_type==PT_HUMAN)?64:70;
    /* Original HUD_MSG.S still controls the on/off phases.  Colour on the
       PC presentation follows the VIEWER, not the pickup or another player. */
    const u32 message_rgb=avp_pc_popup_ink_for_step(
        player_type,avp_hudmsg_rgb24());
    if(!state->hud_msg_valid||!state->hmsg_font)return;
    /* Between illuminated warnings the Jaguar sends colour-zero timer
       records. Its HUD message pixels become invisible during those phases. */
    if(message_rgb==0u)return;
    for(i=0;i<3u;i++)if((state->hud_msg_valid&(1u<<i))&&state->hud_msg[i][0]){
        const unsigned char *p=(const unsigned char*)state->hud_msg[i];int pen=base_x+(int)state->hud_msg_x[i];
        while(*p){int g=hmsg_glyph_index(*p++);if(g>=0){int gx=g*13;for(int yy=0;yy<19;yy++)for(int xx=0;xx<12;xx++)if(hmsg_font_bit(state,gx+xx,yy))put(fb,pen+xx,line_y[i]+yy,message_rgb);}pen+=13;}
    }
}


/* Each of the 320 native HUD columns samples the same host column on
 * every native row. Keep the original centre-of-pixel integer formula,
 * including its last-pixel clamp, but compute these indices only once per
 * presentation frame rather than 228 times per column. */
static void avp_gpu_hud_sample_native(u32 *before,const AvpJaguarFramebuffer *fb)
{
    unsigned host_x[320];
    for(unsigned x=0;x<320u;x++){
        unsigned fx0=(unsigned)(((unsigned long long)(2u*x+1u)*fb->width)/(2u*320u));
        if(fx0>=fb->width)fx0=fb->width-1u;
        host_x[x]=fx0;
    }
    for(unsigned y=0;y<228u;y++){
        unsigned fy0=(unsigned)(((unsigned long long)(2u*y+1u)*fb->height)/(2u*228u));
        if(fy0>=fb->height)fy0=fb->height-1u;
        const u32 *src=fb->pixels+(size_t)fy0*fb->width;
        u32 *row=before+(size_t)y*320u;
        for(unsigned x=0;x<320u;x++)row[x]=src[host_x[x]];
    }
}

/* PC-only local Deathmatch announcement, using the cartridge's real HUD
 * message glyphs. This is drawn AFTER the player's independent death fade,
 * rather than being queued into original HUD_MSG.S (whose state/timing is
 * shared with campaign notices). The two lines never modify gameplay HUD
 * state, and rendering an already-notified frame does not advance its timer. */
static void draw_dm_notice_native(const AvpAvpGpuState *state,
    AvpJaguarFramebuffer *fb,unsigned kind,unsigned other_player,
    int viewing_type)
{
    const char *title=kind==1u?"KILLED":"KILLED BY";
    const u32 ink=avp_pc_popup_ink_for_species(viewing_type);
    char player_label[16];
    const char *lines[2]={title,player_label};
    const int line_y[2]={64,87};
    const int base_x=(viewing_type==PT_HUMAN)?64:70;
    if(!state||!state->hmsg_font||!fb||!fb->pixels||
       (kind!=1u&&kind!=2u)||other_player<1u||other_player>16u)return;
    snprintf(player_label,sizeof player_label,"PLAYER %u",other_player);
    for(unsigned i=0;i<2u;i++){
        const char *p=lines[i];
        const unsigned len=(unsigned)strlen(p);
        int pen=base_x+(192-(int)(len*13u))/2;
        while(*p){
            const int g=hmsg_glyph_index((unsigned char)*p++);
            if(g>=0){
                const int gx=g*13;
                for(int y=0;y<19;y++)for(int x=0;x<12;x++)
                    if(hmsg_font_bit(state,gx+x,y))
                        put(fb,pen+x,line_y[i]+y,ink);
            }
            pen+=13;
        }
    }
}

void avp_gpu_draw_dm_kill_notice(AvpAvpGpuState *state,
    AvpJaguarFramebuffer *fb,unsigned kind,unsigned other_player,
    int viewing_type)
{
    const size_t count=320u*228u;
    AvpJaguarFramebuffer native;
    u32 *before,*after;
    if(!state||!fb||!fb->pixels||!fb->width||!fb->height||
       (kind!=1u&&kind!=2u)||other_player<1u||other_player>16u||
       !state->hmsg_font)return;
    if(fb->width==320u&&fb->height==228u){
        draw_dm_notice_native(state,fb,kind,other_player,viewing_type);
        return;
    }
    if(!state->hud_native_tiles){
        state->hud_native_tiles=(u32*)malloc(2u*count*sizeof(u32));
        if(!state->hud_native_tiles)return;
    }
    before=state->hud_native_tiles;
    after=before+count;
    avp_gpu_hud_sample_native(before,fb);
    memcpy(after,before,count*sizeof(u32));
    native.width=320u;native.height=228u;native.pixels=after;
    draw_dm_notice_native(state,&native,kind,other_player,viewing_type);
    avp_gpu_overlay_scaled_composite(fb->pixels,fb->width,fb->height,before,after);
}

/* One native HUD image drives all scaled output.  Keep original world
 * pixels in the before tile: only pixels modified by the HUD are projected
 * back into the full-resolution framebuffer, so the rest of the scene is
 * never downsampled or overwritten by the overlay. */
static void overlay_hud(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                        const u8 *maze,u16 mw,u16 mh,double px,double py,
                        double fx,double fy,double rx,double ry,unsigned logical_level)
{
    const size_t count=320u*228u;
    AvpJaguarFramebuffer native;
    u32 *before,*after;
    if(!state||!fb||!fb->pixels||!fb->width||!fb->height)return;
    if(fb->width==320u&&fb->height==228u){
        overlay_hud_native(state,fb,maze,mw,mh,px,py,fx,fy,rx,ry,logical_level);
        draw_hud_messages_native(state,fb);
        return;
    }
    if(!state->hud_native_tiles){
        state->hud_native_tiles=(u32*)malloc(2u*count*sizeof(u32));
        if(!state->hud_native_tiles)return;
    }
    before=state->hud_native_tiles;
    after=before+count;
    avp_gpu_hud_sample_native(before,fb);
    memcpy(after,before,count*sizeof(u32));
    native.width=320u;native.height=228u;native.pixels=after;
    overlay_hud_native(state,&native,maze,mw,mh,px,py,fx,fy,rx,ry,logical_level);
    draw_hud_messages_native(state,&native);
    /* Invert the same native sampling map and visit only changed OP texels.
     * Sparse first-person sprites no longer require a full high-res screen
     * comparison or a width/height division for each destination pixel. */
    avp_gpu_overlay_scaled_composite(fb->pixels,fb->width,fb->height,before,after);
}

/* The cartridge's MazeList combines the pain ramp, temporary fades,
 * and Predator vision modifier into one Jaguar CLUT entry. Apply that
 * hardware-space RMW modifier to the scene, not to individual HUD glyphs.
 * Normal gameplay takes the zero-cost neutral path. */
typedef void (*AvpRasterSpanFn)(void *,unsigned,unsigned);
static void avp_raster_spans(AvpRasterSpanFn fn,void *context,unsigned total,unsigned pixels);
typedef struct AvpClutPass {AvpJaguarFramebuffer *fb;u16 modifier;const u8 *q;size_t n;} AvpClutPass;
static void avp_clut_rows(void *arg,unsigned first,unsigned last)
{
    const AvpClutPass *p=(const AvpClutPass*)arg;
    u32 *px=p->fb->pixels+(size_t)first*p->fb->width;
    const size_t count=(size_t)(last-first)*p->fb->width;
    for(size_t i=0;i<count;i++)px[i]=avp_jaguar_rmw_xrgb8888(px[i],p->modifier,p->q,p->n);
}
static void apply_game_clut(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb)
{
    AvpClutPass pass;
    if(!state||!fb||!fb->pixels||!state->qconv||state->qconv_size<131072u)return;
    pass.modifier=avp_jaguar_game_clut(fade_c,xtra_c,fade_level);
    if(pass.modifier==0u)return;
    /* Per-pixel pure CRY read-modify-write: rows split across the host
     * raster workers give identical output (native size stays inline). */
    pass.fb=fb;pass.q=state->qconv;pass.n=state->qconv_size;
    avp_raster_spans(avp_clut_rows,&pass,fb->height,fb->width*fb->height);
}

/* Predator multispectrum output stage: the original $44C8 RDB.DTA
 * table changes TOM VMODE as well as X_FADE.  RGB24 draws one output pixel
 * from a pair of 16-bit screen words and doubles PWIDTH; its 320-pixel
 * display therefore uses 160 source-word pairs.  The host has already
 * converted its scene to RGB; QCONV restores line-buffer words at this
 * final video-output boundary.  The original game state and normal CRY16
 * path are left untouched. */
/* A native TOM line-buffer word expands to a contiguous, possibly empty,
 * output span.  The old pixel loop used floor(x * native_words / width) for
 * every output pixel.  The inverse boundaries are ceil(i * width / words),
 * calculated once for the entire video frame.  This retains the exact
 * nearest-neighbour mapping for narrow, native, and nonintegral host widths. */
static void avp_pred_vmode_span_bounds(unsigned bounds[321],unsigned width,
                                        unsigned words)
{
    for(unsigned i=0;i<=words;i++)
        bounds[i]=(unsigned)(((uint64_t)i*width+words-1u)/words);
}

static void avp_pred_vmode_expand_row(u32 *row,const u32 *native,
                                      unsigned words,const unsigned bounds[321])
{
    for(unsigned i=0;i<words;i++)
        for(unsigned x=bounds[i];x<bounds[i+1u];x++)row[x]=native[i];
}

/* The native TOM input sample column depends only on output width and VMODE,
 * never on the output row.  Keep R128's exact 64-bit sample-index formulas
 * and clamping, but do their divisions once per video frame.  RGB24 uses
 * two consecutive word samples per native displayed pixel. */
static void avp_pred_vmode_sample_columns(unsigned columns[320],unsigned width,
                                          unsigned mode)
{
    if(mode==6u){
        for(unsigned i=0;i<320u;i++){
            unsigned sx=(unsigned)(((uint64_t)(2u*i+1u)*width)/640u);
            columns[i]=sx<width?sx:width-1u;
        }
    }else{
        for(unsigned i=0;i<160u;i++){
            unsigned x0=(unsigned)(((uint64_t)(4u*i+1u)*width)/640u);
            unsigned x1=(unsigned)(((uint64_t)(4u*i+3u)*width)/640u);
            columns[i*2u]=x0<width?x0:width-1u;
            columns[i*2u+1u]=x1<width?x1:width-1u;
        }
    }
}

typedef struct AvpVmodePass {
    AvpJaguarFramebuffer *fb;unsigned mode;const u8 *q;size_t n;
    unsigned bounds[321],sample_columns[320];
} AvpVmodePass;
static void avp_vmode_rows(void *arg,unsigned first,unsigned last)
{
    const AvpVmodePass *p=(const AvpVmodePass*)arg;
    AvpJaguarFramebuffer *fb=p->fb;
    const unsigned w=fb->width;
    if(p->mode==6u){
        /* The physical TOM line buffer is 320 words wide even when the
         * optional PC world renderer uses a higher host resolution. */
        for(unsigned y=first;y<last;y++){
            u32 native[320];
            for(unsigned i=0;i<320u;i++){
                u32 c=fb->pixels[(size_t)y*w+p->sample_columns[i]];
                u16 word=avp_jaguar_rgb_to_cry_qconv((u8)(c>>16),(u8)(c>>8),(u8)c,p->q,p->n);
                native[i]=avp_jaguar_vmode_rgb16(word);
            }
            avp_pred_vmode_expand_row(fb->pixels+(size_t)y*w,native,320u,p->bounds);
        }
    }else{
        /* Original RGB24 video uses 2 original line-buffer words per
         * picture element.  Sample at the native 320-word resolution;
         * host output resolution must not change the channel packing. */
        for(unsigned y=first;y<last;y++){
            u32 pairs[160];
            for(unsigned i=0;i<160u;i++){
                u32 c0=fb->pixels[(size_t)y*w+p->sample_columns[i*2u]];
                u32 c1=fb->pixels[(size_t)y*w+p->sample_columns[i*2u+1u]];
                u16 first_word=avp_jaguar_rgb_to_cry_qconv((u8)(c0>>16),(u8)(c0>>8),(u8)c0,p->q,p->n);
                u16 second_word=avp_jaguar_rgb_to_cry_qconv((u8)(c1>>16),(u8)(c1>>8),(u8)c1,p->q,p->n);
                pairs[i]=avp_jaguar_vmode_rgb24(first_word,second_word);
            }
            avp_pred_vmode_expand_row(fb->pixels+(size_t)y*w,pairs,160u,p->bounds);
        }
    }
}
static void present_predator_vmode(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb)
{
    AvpVmodePass pass;
    unsigned mode=(unsigned)(pred_vision_vmode&6u);
    if(!state||!fb||!fb->pixels||player_type!=PT_PREDATOR||
       invis_stat==0||!pred_vision_vmode)return;
    if(mode!=6u && mode!=2u)return;
    if(!state->qconv||state->qconv_size<131072u)return;
    if(!fb->width || !fb->height)return;
    /* Both the destination spans and the 320 native source-word sample
     * columns are invariant across rows for this host frame; each row is
     * read and rewritten only by itself, so host workers may split rows. */
    pass.fb=fb;pass.mode=mode;pass.q=state->qconv;pass.n=state->qconv_size;
    avp_pred_vmode_span_bounds(pass.bounds,fb->width,mode==6u?320u:160u);
    avp_pred_vmode_sample_columns(pass.sample_columns,fb->width,mode);
    avp_raster_spans(avp_vmode_rows,&pass,fb->height,fb->width*fb->height);
}


/* The host's optional window-resolution renderer rasterizes many more pixels
 * than Jaguar native output.  Rows of flats and columns of walls are fully
 * independent until the original sprite/HUD composition phase.  Divide only
 * those host-only raster passes between at most four workers; each worker
 * writes a disjoint region of the framebuffer (and occlusion buffer).
 * Keep native-sized frames sequential and keep all Jaguar game state on the
 * original simulation thread. */
typedef struct AvpRasterSpan {
    AvpRasterSpanFn fn;
    void *context;
    unsigned first,last;
} AvpRasterSpan;
#ifndef _WIN32
static void *avp_raster_thread(void *arg)
{
    AvpRasterSpan *s=(AvpRasterSpan*)arg;
    s->fn(s->context,s->first,s->last);
    return NULL;
}
#else
static DWORD WINAPI avp_raster_thread(LPVOID arg)
{
    AvpRasterSpan *s=(AvpRasterSpan*)arg;
    s->fn(s->context,s->first,s->last);
    return 0;
}
#endif
static void avp_raster_spans(AvpRasterSpanFn fn,void *context,
                              unsigned total,unsigned pixels)
{
#ifndef _WIN32
    /* Host-resolution raster only: rows (floor) and columns (walls) are
     * independent, so any split yields identical pixels.  Use the machine's
     * cores (up to 16) instead of a fixed 4 when the window is maximised. */
    pthread_t threads[15];
    AvpRasterSpan span[16];
    unsigned n=16,launched=0,i;
    long available;
    if(pixels<640u*480u || total<4u){fn(context,0,total);return;}
    available=sysconf(_SC_NPROCESSORS_ONLN);
    if(available<2){fn(context,0,total);return;}
    if((unsigned long)available<n)n=(unsigned)available;
    for(i=0;i<n;i++){
        span[i].fn=fn;
        span[i].context=context;
        span[i].first=(unsigned)((uint64_t)total*i/n);
        span[i].last=(unsigned)((uint64_t)total*(i+1u)/n);
    }
    /* Start N-1 workers, using the calling thread for the final range.
     * An unsuccessful creation is completed inline, not skipped. */
    for(i=0;i<n-1u;i++){
        if(pthread_create(&threads[launched],NULL,avp_raster_thread,&span[i])==0)
            launched++;
        else fn(context,span[i].first,span[i].last);
    }
    fn(context,span[n-1u].first,span[n-1u].last);
    for(i=0;i<launched;i++)(void)pthread_join(threads[i],NULL);
#else
    /* Same host-only split on Windows, with native Win32 threads. */
    HANDLE threads[15];
    AvpRasterSpan span[16];
    unsigned n=16,launched=0,i;
    SYSTEM_INFO si;
    if(pixels<640u*480u || total<4u){fn(context,0,total);return;}
    GetSystemInfo(&si);
    if(si.dwNumberOfProcessors<2u){fn(context,0,total);return;}
    if(si.dwNumberOfProcessors<n)n=(unsigned)si.dwNumberOfProcessors;
    for(i=0;i<n;i++){
        span[i].fn=fn;
        span[i].context=context;
        span[i].first=(unsigned)((uint64_t)total*i/n);
        span[i].last=(unsigned)((uint64_t)total*(i+1u)/n);
    }
    for(i=0;i<n-1u;i++){
        HANDLE h=CreateThread(NULL,0,avp_raster_thread,&span[i],0,NULL);
        if(h)threads[launched++]=h;
        else fn(context,span[i].first,span[i].last);
    }
    fn(context,span[n-1u].first,span[n-1u].last);
    if(launched)(void)WaitForMultipleObjects((DWORD)launched,threads,TRUE,INFINITE);
    for(i=0;i<launched;i++)CloseHandle(threads[i]);
#endif
}

typedef struct AvpFloorPass {
    AvpAvpGpuState *state;
    AvpJaguarFramebuffer *fb;
    const u8 *maze;
    u16 mw,mh;
    double px,py,fx,fy,rx,ry,focal;
    const AvpGpuRgbImage *flat_tex[256];
    unsigned flat_rotation[256];
} AvpFloorPass;
static void avp_floor_rows(void *arg,unsigned first,unsigned last)
{
    const AvpFloorPass *p=(const AvpFloorPass*)arg;
    AvpJaguarFramebuffer *fb=p->fb;
    unsigned x,y;
    for(y=first;y<last;y++){
        double retail_row=((double)y+0.5)*228.0/(double)fb->height -
            (p->state && p->state->map_editor_world_only
                ?p->state->map_editor_pitch*228.0*0.52:0.0);
        int ceiling=retail_row<114.0;
        double perp,lefttan,step,wx,wy,wxstep,wystep;
        int floor_shade;
        u8 row_shades[256];
        if(!avp_host_flat_row_depth(retail_row,
               p->state ? p->state->host_extend_draw_distance : 0,&perp))continue;
        floor_shade=avp_host_fog_shade_delta(avp_gpu_retail_flat_intensity_delta(
            avp_host_fog_distance(perp,p->state ? p->state->host_extend_fog_distance : 0)),
            p->state && p->state->host_disable_fog);
        avp_gpu_host_flat_row_shades(row_shades,floor_shade);
        lefttan=(0.5-(double)fb->width*0.5)/p->focal;step=1.0/p->focal;
        wx=p->px+(p->fx+p->rx*lefttan)*perp;
        wy=p->py+(p->fy+p->ry*lefttan)*perp;
        wxstep=p->rx*step*perp;wystep=p->ry*step*perp;
        for(x=0;x<fb->width;x++,wx+=wxstep,wy+=wystep){
            int cx,cy;u8 flat;
            const AvpGpuRgbImage *t;
            if(!avp_gpu_floor_valid_cell(wx,wy,p->mw,p->mh,&cx,&cy))continue;
            {const u8*c=p->maze+((size_t)cy*p->mw+(size_t)cx)*8u;flat=c[ceiling?4:5];}
            /* Resolve file IDs and loaded textures once per flat kind before
             * this host-only raster pass, not for each window-size pixel. */
            t=p->flat_tex[flat];
            if(t){
                /* The cell indices above are precisely floor(wx/wy).
                 * Reuse those results to compute the same fractional
                 * coordinates, avoiding two more libm floor operations for
                 * every high-resolution flat pixel. */
                double ux=wx-(double)cx,uy=wy-(double)cy;
                int tx=(int)(ux*t->width),ty=(int)(uy*t->height);
                if(p->flat_rotation[flat] && t->width==t->height){
                    int oldx=tx,oldy=ty;
                    switch(p->flat_rotation[flat]){
                    case 1u:tx=oldy;ty=t->height-1-oldx;break;
                    case 2u:tx=t->width-1-oldx;ty=t->height-1-oldy;break;
                    case 3u:tx=t->width-1-oldy;ty=oldx;break;
                    default:break;
                    }
                }
                /* The fraction within this validated cell is [0,1), so
                 * inbounds sample coordinates need no per-pixel modulus.
                 * Malformed zero-sized textures preserve the old fallback. */
                fb->pixels[(size_t)y*fb->width+x]=
                    t->width>0 && t->height>0
                    ? avp_gpu_host_flat_sample_inbounds(t,(unsigned)tx,(unsigned)ty,
                                                        row_shades,k_cry_rgb)
                    : sample_tex_shaded(t,tx,ty,floor_shade);
            }
        }
    }
}

typedef struct AvpWallPass {
    AvpAvpGpuState *state;
    AvpJaguarFramebuffer *fb;
    const u8 *maze;
    u16 mw,mh;
    double px,py,fx,fy,rx,ry,focal;
    unsigned logical_level;
    double *zbuf;
    AvpMaze2Occlusion *occlusion;
} AvpWallPass;
static void avp_wall_columns(void *arg,unsigned first,unsigned last)
{
    const AvpWallPass *p=(const AvpWallPass*)arg;
    unsigned x;
    for(x=first;x<last;x++){
        double tanoff=((double)x+0.5-(double)p->fb->width*0.5)/p->focal;
        double dx=p->fx+p->rx*tanoff,dy=p->fy+p->ry*tanoff;
        PcRayHit portals[AVP_MAZE2_MAX_RAY_PORTALS];unsigned pc=0,k;
        PcRayHit hit=cast_ray_hit(p->state,p->logical_level,p->maze,
                                 p->mw,p->mh,p->px,p->py,dx,dy,
                                 portals,&pc,AVP_MAZE2_MAX_RAY_PORTALS);
        pc_portal_clip_column(p->state,p->logical_level,portals,pc,&hit,p->fb->height);
        p->zbuf[x]=hit.hit?hit.d:avp_host_ray_limit(p->state ? p->state->host_extend_draw_distance : 0);
        if(p->occlusion){
            for(k=0;k<pc;k++)draw_hit_column(p->state,p->fb,x,&portals[k],
                                             p->logical_level,1,p->occlusion);
            if(hit.hit)draw_hit_column(p->state,p->fb,x,&hit,p->logical_level,0,p->occlusion);
        }else{
            if(hit.hit)draw_hit_column(p->state,p->fb,x,&hit,p->logical_level,0,NULL);
            for(k=pc;k>0;k--)draw_hit_column(p->state,p->fb,x,&portals[k-1u],
                                              p->logical_level,1,NULL);
        }
    }
}

void avp_gpu_render_maze(AvpAvpGpuState*state,AvpJaguarFramebuffer*fb,const u8*maze,u16 mw,u16 mh,u32 xpos,u32 ypos,u32 angle,int rom_ok,unsigned logical_level,u64 vblank_counter,int smooth_presentation,int debug_immediate)
{
    u8 interp_maze[64u*64u*8u];size_t maze_bytes=(size_t)mw*(size_t)mh*8u;
    if(state)state->map_smooth_camera=smooth_presentation&&state->have_prev_sim;
    unsigned x,y;double px=(double)xpos/65536.0,py=(double)ypos/65536.0;
    if(state&&state->computer_active){avp_gpu_computer_present(state,fb);return;}
    ensure_cry_rgb_table();
    if(smooth_presentation&&state->have_prev_sim&&maze&&maze_bytes<=sizeof(interp_maze)&&state->prev_maze_size==maze_bytes){memcpy(interp_maze,maze,maze_bytes);for(size_t ci=0;ci<(size_t)mw*(size_t)mh;ci++){size_t o=ci*8u;for(unsigned k=6;k<=7;k++){double q=(double)state->prev_maze[o+k]+((double)maze[o+k]-(double)state->prev_maze[o+k])*state->render_alpha;if(q<0)q=0;if(q>255)q=255;interp_maze[o+k]=(u8)(q+0.5);}}maze=interp_maze;}
    double a=avp_gpu_heading_radians(angle,state&&state->host_continuous_heading);
    double fx=cos(a),fy=sin(a),rx=-fy,ry=fx;
    /* At the original size the image-plane distance is exactly 160 pixels.
     * A wider HOST viewport preserves the 4:3 picture-plane FOV (including
     * original non-square Jaguar display pixels), and adds
     * extra rays on both sides; it does not magnify the Jaguar 320 columns.
     * Leave the byte-for-byte legacy path alone for a 4:3 screen. */
    const int host_wide=state && state->host_widescreen_fov &&
        avp_host_has_extra_columns(fb->width,fb->height);
    const double focal=host_wide ? avp_host_wide_focal(fb->height) :
                                    (double)fb->width*0.5;
    /* rgb(0,0,0) is the all-zero XRGB word: one memset instead of a
     * per-pixel loop over the (possibly window-sized) frame. */
    memset(fb->pixels,0,(size_t)fb->width*(size_t)fb->height*sizeof(*fb->pixels));

    /* Retail-style floor/ceiling projection.  The presentation surface keeps
       the original 320:228 ratio; host resolution only increases sampling. */
    if(state->textures_loaded>0 && logical_level>=1u&&logical_level<=15u){
        AvpFloorPass pass;
        pass.state=state;pass.fb=fb;pass.maze=maze;pass.mw=mw;pass.mh=mh;
        pass.px=px;pass.py=py;pass.fx=fx;pass.fy=fy;pass.rx=rx;pass.ry=ry;pass.focal=focal;
        /* Resolve level texture indices only once, before worker dispatch. */
        for(unsigned f=0;f<256u;f++){
            unsigned id=0;
            /* The panel image set is immutable during the raster workers.
             * A missing/unloaded flat remains transparent to this pass. */
            pass.flat_tex[f]=avp_gpu_flat_texture_file(logical_level,(u8)f,&id)&&
                             id<74u&&state->panel_tex[id].rgb
                             ?&state->panel_tex[id]:NULL;
            pass.flat_rotation[f]=avp_gpu_flat_texture_rotation(logical_level,(u8)f);
        }
        avp_raster_spans(avp_floor_rows,&pass,fb->height,fb->width*fb->height);
    }else{
        for(y=0;y<fb->height;y++){uint32_t c=y<fb->height/2?rgb(20,22,24):rgb(38,36,32);for(x=0;x<fb->width;x++)fb->pixels[(size_t)y*fb->width+x]=c;}
    }

    /* Source-backed wall/door presentation.  Door state bytes are mutated by
       translated MoveDoors at 10 Hz; this renderer only visualizes them. */
    {double *zbuf=(double*)malloc((size_t)fb->width*sizeof(double));AvpMaze2Occlusion *occlusion=NULL;
    if(!zbuf)return;
    if(avp_gpu_maze2_occlusion_ensure(&state->maze_occlusion,fb->width,fb->height)){
        avp_gpu_maze2_occlusion_clear(&state->maze_occlusion);
        occlusion=&state->maze_occlusion;
    }
    {
        AvpWallPass pass;
        pass.state=state;pass.fb=fb;pass.maze=maze;pass.mw=mw;pass.mh=mh;
        pass.px=px;pass.py=py;pass.fx=fx;pass.fy=fy;pass.rx=rx;pass.ry=ry;
        pass.focal=focal;pass.logical_level=logical_level;
        pass.zbuf=zbuf;pass.occlusion=occlusion;
        avp_raster_spans(avp_wall_columns,&pass,fb->width,fb->width*fb->height);
    }
    if(!state->map_editor_world_only)
        draw_amp_layer(state,fb,px,py,fx,fy,rx,ry,focal,zbuf,occlusion,smooth_presentation);
    free(zbuf);
    }

    if(state->map_editor_world_only)return; /* The same wall/flat raster, without active game entities or HUD. */

    /* An authored Jaguar HUD and first-person weapon are always 320x228,
     * irrespective of expanded world FOV.  For a wide host view composite
     * those unchanged 4:3 OP layers over a CENTERED picture.  The side
     * columns remain 3D scene, not scaled HUD or weapon pixels. */
    AvpJaguarFramebuffer op_view={0};
    unsigned op_left=0;
    if(host_wide){
        unsigned center_w=avp_host_wide_center_width(fb->height);
        if(center_w && center_w<fb->width &&
           (size_t)center_w <= SIZE_MAX/sizeof(u32)/fb->height){
            op_view.width=center_w;op_view.height=fb->height;
            op_left=(fb->width-center_w)/2u;
            op_view.pixels=(u32*)malloc((size_t)center_w*fb->height*sizeof(u32));
        }
    }
    if(op_view.pixels){
        for(unsigned row=0;row<fb->height;row++)
            memcpy(op_view.pixels+(size_t)row*op_view.width,
                   fb->pixels+(size_t)row*fb->width+op_left,
                   (size_t)op_view.width*sizeof(u32));
        overlay_first_person_scaled(state,&op_view,vblank_counter,smooth_presentation);
        for(unsigned row=0;row<fb->height;row++)
            memcpy(fb->pixels+(size_t)row*fb->width+op_left,
                   op_view.pixels+(size_t)row*op_view.width,
                   (size_t)op_view.width*sizeof(u32));
    }else overlay_first_person_scaled(state,fb,vblank_counter,smooth_presentation);
    apply_game_clut(state,fb);
    if(op_view.pixels){
        for(unsigned row=0;row<fb->height;row++)
            memcpy(op_view.pixels+(size_t)row*op_view.width,
                   fb->pixels+(size_t)row*fb->width+op_left,
                   (size_t)op_view.width*sizeof(u32));
        overlay_hud(state,&op_view,maze,mw,mh,px,py,fx,fy,rx,ry,logical_level);
        for(unsigned row=0;row<fb->height;row++)
            memcpy(fb->pixels+(size_t)row*fb->width+op_left,
                   op_view.pixels+(size_t)row*op_view.width,
                   (size_t)op_view.width*sizeof(u32));
        free(op_view.pixels);
    }else overlay_hud(state,fb,maze,mw,mh,px,py,fx,fy,rx,ry,logical_level);
    present_predator_vmode(state,fb);
    /* The translated MAZESCRN fade is owned by the 68000, but the PC
     * renderer composites the HUD after the 3D CLUT pass.  At the completed
     * fade-to-black endpoint the final video signal must be black, including
     * the separately composited HUD.  Do not alter the original fade timer,
     * rate or intermediate Jaguar color-modifier behavior. */
    if(fade_level>=0x80 && fb->pixels)
        memset(fb->pixels,0,(size_t)fb->width*(size_t)fb->height*sizeof(*fb->pixels));
    /* Developer diagnostics are never drawn during the normal retail path. */
    if(debug_immediate){int ox=8,oy=8,cy,cx;int maxw=(int)(fb->width/3u);int maxh=(int)(fb->height/3u);int msx=mw?maxw/(int)mw:1,msy=mh?maxh/(int)mh:1;int ms=msx<msy?msx:msy;if(ms<1)ms=1;for(cy=0;cy<(int)mh;cy++)for(cx=0;cx<(int)mw;cx++){const u8*c=maze+((size_t)cy*mw+(size_t)cx)*8u;int xx=ox+cx*ms,yy=oy+cy*ms;if(c[0])line(fb,xx,yy,xx,yy+ms,rgb(100,100,100));if(c[1])line(fb,xx,yy,xx+ms,yy,rgb(100,100,100));if(c[2])line(fb,xx+ms,yy,xx+ms,yy+ms,rgb(100,100,100));if(c[3])line(fb,xx,yy+ms,xx+ms,yy+ms,rgb(100,100,100));}put(fb,ox+(int)px*ms,oy+(int)py*ms,rom_ok?rgb(80,255,100):rgb(255,190,60));}
}

