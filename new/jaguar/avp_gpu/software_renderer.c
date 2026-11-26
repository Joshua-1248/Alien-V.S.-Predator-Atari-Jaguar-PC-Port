#include "jaguar/avp_gpu/heading_radians.h"
#include "jaguar/avp_gpu/software_renderer.h"
#include "jaguar/avp_gpu/automap_enemy_marker.h"
#include "jaguar/avp_gpu/presentation_interp.h"
#include "jaguar/avp_gpu/overlay_presentation.h"
#include "jaguar/avp_gpu/bridge.h"
#include "jaguar/avp_gpu/maze2_column.h"
#include "jaguar/avp_gpu/maze2_occlusion.h"
#if AVP_MAZE2_MAX_RAY_PORTALS != AVP_WORLD_SCENE_MAX_PORTALS
#error "GPU scene packet portal limit must follow the original CPU raycaster"
#endif
#include "jaguar/avp_gpu/wall_raster_dda.h"
#include "jaguar/avp_gpu/wall_shade_cache.h"
#include "jaguar/avp_gpu/fp_column_uv.h"
#include "jaguar/avp_gpu/sprite_sampling.h"
#include "jaguar/avp_gpu/thermal_vision.h"
#include "jaguar/avp_gpu/sprite_projection.h"
#include "jaguar/avp_gpu/host_widescreen.h"
#include "jaguar/avp_gpu/fp_wide_blit.h"
#include "jaguar/avp_gpu/host_distance.h"
#include "jaguar/avp_gpu/floor_cell_sampling.h"
#include "jaguar/avp_gpu/floor_row_cell_cache.h"
#include "jaguar/avp_gpu/flat_usage.h"
#include "jaguar/avp_gpu/host_flat_shading.h"
#include "jaguar/avp_gpu/aaf_columns.h"
#include "jaguar/avp_gpu/fp_overlay.h"
#include "jaguar/avp_gpu/overlay_scale_composite.h"
#include "jaguar/avp_gpu/hires_wall_sample.h"
#include "jaguar/avp_gpu/hires_window_mask.h"
#include "jaguar/avp_gpu/hires_alpha_blend.h"
#include "jaguar/avp_gpu/texture_filter.h"
#include "jaguar/avp_gpu/hud_rmw_host_composite.h"
#include "jaguar/avp_gpu/hires_automap_sample.h"
#include "jaguar/avp_gpu/hires_hud_details.h"
#include "jaguar/avp_gpu/popup_message_color.h"
#include "jaguar/avp_gpu/tracker_brush.h"
#include "jaguar/avp_gpu/tracker_contact_math.h"
#include "jaguar/avp_gpu/hud_fade.h"
#include "jaguar/avp_gpu/hud_weapon_icons.h"
#include "jaguar/avp_gpu/pred_scope_raster.h"
#include "jaguar/cry.h"
#include "../../game_micro_optimization/cry_rmw_fast.h"
#include "../../game_micro_optimization/damage_clut_fast.h"
#include "../../game_micro_optimization/point_floor_row.h"
#include "../../game_micro_optimization/hud_replay_fast.h"
#if defined(__linux__) && defined(__SSE2__) && defined(__GNUC__)
#include "../../game_micro_optimization/hud_replay_sparse_reset.h"
#define AVP_HUD_SPARSE_GCC 1
#endif
#if defined(__linux__) /* R447 experiment: GCC and Clang */
#include "../../game_micro_optimization/hud_brush_fast.h"
#endif
#include "../../game_micro_optimization/depth_clear_fast.h"
#include "../../game_micro_optimization/frame_copy_fast.h"
#if !defined(__clang__) /* R427: fullscreen-tested GCC path only */
#include "../../game_micro_optimization/fp_wide_fast.h"
#endif
#include "../../game_micro_optimization/opaque_wall_fast.h"
#if defined(__linux__) || (defined(__clang__) && !defined(_WIN32)) || defined(AVP_R434_TEST_SPRITE_SPANS)
#include "../../game_micro_optimization/sprite_span_fast.h"
#endif
#if defined(__clang__) && !defined(_WIN32) /* R422: measured Clang-only fast path */
#include "../../game_micro_optimization/maze_interp_fast.h"
#endif
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
#if defined(__clang__) && !defined(_WIN32)
#include <float.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef AVP_HUD_SPARSE_GCC

/* R443: keep the ordinary full reset and untracked recorder when the
 * Micro-Optimization setting is off. The pass mode is latched at begin. */
static inline void *avp_hud_replay_allocate(size_t count)
{
#ifdef AVP_HUD_SPARSE_GCC
    (void)count;
    return avp_hud_sparse_create();
#else
    return malloc(count*sizeof(AvpHudRmwOps));
#endif
}
static inline AvpHudRmwOps *avp_hud_replay_begin(void *storage,size_t count,int enabled)
{
#ifdef AVP_HUD_SPARSE_GCC
    (void)count;
    return avp_hud_sparse_begin((AvpHudSparseReplay*)storage,enabled);
#else
    (void)enabled;
    AvpHudRmwOps *ops=(AvpHudRmwOps*)storage;
    if(ops)memset(ops,0,count*sizeof(*ops));
    return ops;
#endif
}
static inline void avp_hud_replay_record_pass(void *storage,size_t offset,uint16_t ink)
{
#ifdef AVP_HUD_SPARSE_GCC
    avp_hud_sparse_record((AvpHudSparseReplay*)storage,offset,ink);
#else
    avp_gpu_hud_replay_record((AvpHudRmwOps*)storage,offset,ink);
#endif
}

#endif
#ifndef _WIN32
#include <time.h>
#endif
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
#include "../../game_micro_optimization/raster_row_queue.h"
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
        memset(state->predator_scope_seg_valid,0,sizeof(state->predator_scope_seg_valid));
    }
    /* Keep the exact native 43x45 Jaguar bitmap; separate PC display-only
     * spans can subdivide the original three samples without inventing
     * audio measurements or updating at host presentation FPS. */
    memmove(state->predator_scope_seg_a,state->predator_scope_seg_a+2,
            (AVP_PRED_SCOPE_H-2u)*sizeof(state->predator_scope_seg_a[0]));
    memmove(state->predator_scope_seg_b,state->predator_scope_seg_b+2,
            (AVP_PRED_SCOPE_H-2u)*sizeof(state->predator_scope_seg_b[0]));
    memmove(state->predator_scope_seg_valid,state->predator_scope_seg_valid+2,
            (AVP_PRED_SCOPE_H-2u)*sizeof(state->predator_scope_seg_valid[0]));
    state->predator_scope_seg_a[AVP_PRED_SCOPE_H-2u]=(s16)avp_pred_scope_clamp(previous);
    state->predator_scope_seg_b[AVP_PRED_SCOPE_H-2u]=(s16)avp_pred_scope_clamp(midpoint);
    state->predator_scope_seg_a[AVP_PRED_SCOPE_H-1u]=(s16)avp_pred_scope_clamp(midpoint);
    state->predator_scope_seg_b[AVP_PRED_SCOPE_H-1u]=(s16)avp_pred_scope_clamp(next);
    state->predator_scope_seg_valid[AVP_PRED_SCOPE_H-2u]=1u;
    state->predator_scope_seg_valid[AVP_PRED_SCOPE_H-1u]=1u;
    avp_pred_scope_step(state->predator_scope,previous,midpoint,next);
}

static uint32_t rgb(unsigned r,unsigned g,unsigned b){return (r<<16)|(g<<8)|b;}
static void put(AvpJaguarFramebuffer*fb,int x,int y,uint32_t c){if(x>=0&&y>=0&&(unsigned)x<fb->width&&(unsigned)y<fb->height)fb->pixels[(size_t)y*fb->width+(unsigned)x]=c;}
static void line(AvpJaguarFramebuffer*fb,int x0,int y0,int x1,int y1,uint32_t c){int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy;for(;;){put(fb,x0,y0,c);if(x0==x1&&y0==y1)break;{int e2=2*e;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}}}

void avp_gpu_snapshot_sim(AvpAvpGpuState *state,const u8*maze,u16 mw,u16 mh){size_t n;if(!state)return;state->prev_damage=avp_damage_sample(fade_c,xtra_c,player_type,cur_level,fade_level==0 && !destruct_flag && !player_dead && player_energy>0);for(unsigned i=0;i<AVP_NUM_AMPS;i++){state->amp_prev_x[i]=level1amps[i].xpos;state->amp_prev_y[i]=level1amps[i].ypos;state->amp_prev_angle[i]=level1amps[i].angle;state->amp_prev_generation[i]=avp_amp_slot_generation(i);}{AvpFirstPersonOverlayState old;avp_mazescrn_get_overlay_state(&old);state->prev_overlay_id=old.overlay_visible?old.overlay_id:-1;
    state->prev_overlay_visible=old.overlay_visible;
    state->prev_overlay_snapshot_valid=1u;
    state->prev_overlay_species=player_type;
    state->prev_overlay_weapon=cur_wepno;
    /* Alien and Predator Combi-Stick first visible move_in poses are
     * derived from the original
     * command stream.  Smooth Presentation must start its NEXT 10-Hz
     * interpolation from that displayed pose, not the invisible frame0
     * coordinates, or it reintroduces the same dip with smoothing on.
     * Preserve the previously verified Marine snapshot path unchanged. */
    state->prev_wep_x=(old.intro_preview &&
        (player_type==PT_ALIEN || (player_type==PT_PREDATOR&&cur_wepno==1)))?
        old.intro_x:wep_x;
    state->prev_wep_y=(old.intro_preview &&
        (player_type==PT_ALIEN || (player_type==PT_PREDATOR&&cur_wepno==1)))?
        old.intro_y:wep_y;
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

/* R428: retain the R427 ray structure and dispatch on GCC/Windows. */
#if defined(__clang__) && !defined(_WIN32)
#define AVP_R428_FRESH_DEPTH_FIELD unsigned fresh_depth;
#define AVP_R428_FRESH_DEPTH_INIT ,0
#else
#define AVP_R428_FRESH_DEPTH_FIELD
#define AVP_R428_FRESH_DEPTH_INIT
#endif
typedef struct PcRayHit { double d,tc; u8 wall,open; int hit,vertical; int clip_lo,clip_hi; AVP_R428_FRESH_DEPTH_FIELD } PcRayHit;
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
    double adx=fabs(dx),ady=fabs(dy),tx,ty,dtx,dty;PcRayHit miss={max_depth,0.0,0,0,0,0,0,0 AVP_R428_FRESH_DEPTH_INIT};
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
                    PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0 AVP_R428_FRESH_DEPTH_INIT};return h;
                }
                {PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0 AVP_R428_FRESH_DEPTH_INIT};if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;}
            }else if(type>=4u&&type<=6u){
                int visibility=centered_door_visibility(state,logical_level,bi.wall,bi.open,tc);
                if(!visibility){PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0 AVP_R428_FRESH_DEPTH_INIT};return h;}
                if(visibility==2){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0 AVP_R428_FRESH_DEPTH_INIT};
                    if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;
                }
            }else if(type>=7u&&type<=11u){
                int opening_lo,opening_hi;
                if(!vertical_door_source_aperture(state,logical_level,bi.wall,
                                                   bi.open,tc,&opening_lo,&opening_hi)){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0 AVP_R428_FRESH_DEPTH_INIT};return h;
                }
                {PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0 AVP_R428_FRESH_DEPTH_INIT};if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;}
            }else{PcRayHit h={d,tc,bi.wall,bi.open,1,1,0,0 AVP_R428_FRESH_DEPTH_INIT};return h;}}cx=nx;tx+=dtx;}
        else{double d=ty;int ny;if(d>=max_depth)break;ny=cy+stepy;PcBoundary bi=boundary_info(maze,w,h,cx,cy,cx,ny,0);if(bi.wall){double hx=px+dx*d,tc=avp_gpu_maze2_orient_fraction(hx-floor(hx),0,stepy);unsigned type=(bi.wall>>3)&15u;
            if(type==3u||type==13u||type==14u||type==15u){
                if(!window_column_transmits(state,logical_level,bi.wall,tc,type)){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0 AVP_R428_FRESH_DEPTH_INIT};return h;
                }
                {PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0 AVP_R428_FRESH_DEPTH_INIT};if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;}
            }else if(type>=4u&&type<=6u){
                int visibility=centered_door_visibility(state,logical_level,bi.wall,bi.open,tc);
                if(!visibility){PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0 AVP_R428_FRESH_DEPTH_INIT};return h;}
                if(visibility==2){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0 AVP_R428_FRESH_DEPTH_INIT};
                    if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;
                }
            }else if(type>=7u&&type<=11u){
                int opening_lo,opening_hi;
                if(!vertical_door_source_aperture(state,logical_level,bi.wall,
                                                   bi.open,tc,&opening_lo,&opening_hi)){
                    PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0 AVP_R428_FRESH_DEPTH_INIT};return h;
                }
                {PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0 AVP_R428_FRESH_DEPTH_INIT};if(!pc_record_portal(portals,portal_count,portal_cap,h))return h;}
            }else{PcRayHit h={d,tc,bi.wall,bi.open,1,0,0,0 AVP_R428_FRESH_DEPTH_INIT};return h;}}cy=ny;ty+=dty;}
        if(cx<0||cy<0||cx>=w||cy>=h)break;
    }
    return miss;
}
/* MAZE2.GAS::restart_scan retains the intersection of successive window
 * apertures.  The original GPU clips each column before walking into the
 * next room.  The host draws wall hits back-to-front, so retain the same
 * surviving clip for each recorded hit; otherwise a more distant wall may
 * be painted outside an earlier opening. */
static int hires_window_enabled(const AvpAvpGpuState *s,unsigned level,u8 wall,unsigned height)
{
    unsigned fid,ni;
    if(!s||height<=228u||!s->host_world_tex_enabled||
       !*s->host_world_tex_enabled||!s->host_world_tex||
       !avp_gpu_wall_texture_file(level,wall,&fid)||fid>=74u)return 0;
    if(s->host_world_tex[fid].rgb)return 1;
    return fid==16u && s->host_notice_tex &&
        avp_gpu_wall_notice_index(level,wall,&ni) && ni<18u &&
        s->host_notice_tex[ni].rgb;
}
static double hires_mask_subcolumn(double tc,int reflect,int unreflect)
{
    double f=tc*128.0;
    f-=floor(f);
    if(reflect)f=1.0-f;
    if(unreflect)f=1.0-f;
    if(f<0.0)f=0.0;
    if(f>1.0)f=1.0;
    return f;
}
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
    if(hires_window_enabled(state,level,p->wall,fb_height) && bottom>top){
        double ft,fb;
        avp_gpu_hires_window_pair(state->window_pairs[wf-74u],tx,
            hires_mask_subcolumn(p->tc,(p->wall&0x80u)!=0,type==13u),
            1,&ft,&fb);
        *out_lo=avp_gpu_hires_edge_y(y0,wall_h,ft);
        *out_hi=avp_gpu_hires_edge_y(y0,wall_h,fb);
    }else{
        *out_lo=avp_gpu_maze2_host_edge_y(y0,wall_h,top);
        *out_hi=avp_gpu_maze2_host_edge_y(y0,wall_h,bottom);
    }
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

static void draw_wall_seg_shift(const AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,const PcRayHit *hit,unsigned sx,int y0,int y1,int base_y0,
                                int wall_h,const AvpGpuRgbImage *t,int source_tx,
                                int source_y_shift,int shade,AvpMaze2Occlusion *occlusion,float depth)
{
    int yy;
    const int high_y=t && t->height>128;
    double sample_y=0.0,step_y=0.0;
    if(!t||!t->rgb||wall_h<=0||!hit)return;
    if(y0<hit->clip_lo)y0=hit->clip_lo;
    if(y1>hit->clip_hi)y1=hit->clip_hi;
    if(y0<0)y0=0;
    if(y1>(int)fb->height)y1=(int)fb->height;
    if(y0>=y1)return;
    if(source_tx<0)source_tx=0;
    if(source_tx>127)source_tx=127;
    if(t->width>128){
        const unsigned wall_type=((unsigned)hit->wall>>3)&15u;
        source_tx=avp_gpu_hires_wall_x(source_tx,hit->tc,
                                      (hit->wall&0x80u)!=0,wall_type==13u,
                                      (unsigned)t->width);
    }else source_tx=source_tx*t->width/128;
    if(source_tx>=t->width)source_tx=t->width-1;
    /* The column, shade and source texture do not change inside this call.
     * At host resolutions above Jaguar native, neighboring output rows often
     * select the same source texel. Reuse its exact CRY-shaded RGB result. */
    if(high_y){
        step_y=(double)t->height/(double)wall_h;
        sample_y=((double)(y0-base_y0)+0.5)*step_y+
                 (double)source_y_shift*(double)t->height/128.0;
    }
    /* R308: a separate opt-in sampler. Never disturb the unchanged Jaguar
     * point path, collision masks or depth comparison when filtering is off. */
    if(state && (state->host_filter_mask & (1u<<AVP_FILTER_GEOMETRY)) &&
       (state->host_filter_mode || state->host_aniso>1u)){
        const AvpFilterImage *mips=(state->host_filter_mode==AVP_TEXFILTER_NEAREST_MIP ||
                                     state->host_aniso>1u) ?
            avp_filter_cache_lookup_ready(state->host_filter_cache,t) : NULL;
        /* Projected depth and vertical pixel footprint determine minification.
         * The horizontal footprint is estimated from the wall ray's depth;
         * the floor pass below uses full perspective row derivatives. */
        AvpFilterDerivatives dd={0};
        dd.dudx=(hit->d<0.001?0.001:hit->d)*2.0/(double)fb->width;
        dd.dvdy=1.0/(double)wall_h;
        const unsigned mode=state->host_filter_mode;
        const unsigned aniso=state->host_aniso;
        double fu=hit->tc*(double)t->width;
        double rem=fu-floor(fu);
        if((hit->wall&0x80u)!=0u)rem=1.0-rem;
        double u=((double)source_tx+rem)/(double)t->width;
        if(u<0.0)u=0.0;
        if(u>1.0)u=1.0;
        const AvpFilterPlan plan=avp_filter_plan(t,mips,dd,mode,aniso);
        /* Fog shade is invariant for a projected wall column. Give the
         * identical row-shade CRY lookup to the source-texture sampler and
         * resolve opaque/CRY/clamped state outside the AF tap loop. */
        /* R388: adjacent wall columns usually share a fog shade. Reuse
         * its identical 256-entry CLUT locally to the calling raster thread;
         * no global state or locks are touched by parallel wall workers.
         * C99 compilers without thread-local storage retain the old path. */
#if defined(_MSC_VER)
        static __declspec(thread) AvpWallShadeCache shade_cache;
        const uint8_t *wall_shade_lut=avp_wall_shade_cache_get(&shade_cache,shade);
#elif defined(__GNUC__) || defined(__clang__)
        static __thread AvpWallShadeCache shade_cache;
        const uint8_t *wall_shade_lut=avp_wall_shade_cache_get(&shade_cache,shade);
#else
        uint8_t wall_shade_lut_storage[256];
        avp_filter_make_shade_lut(wall_shade_lut_storage,shade);
        const uint8_t *wall_shade_lut=wall_shade_lut_storage;
#endif
        AvpFilterCryWallColumn filtered_column;
        const int fast_cry_column=avp_filter_cry_wall_column_prepare(
            &filtered_column,t,mips,u,plan,0,k_cry_rgb,wall_shade_lut);
        for(yy=y0;yy<y1;yy++){
            size_t at=(size_t)yy*fb->width+sx;
            if(occlusion&&occlusion->depth&&occlusion->width==fb->width&&
               occlusion->height==fb->height&&occlusion->depth[at]<=depth)continue;
            double v=((double)(yy-base_y0)+0.5)/(double)wall_h+
                     (double)source_y_shift/128.0;
            if(v<0.0)v=0.0;
            if(v>1.0)v=1.0;
            unsigned alpha=255u;
            uint32_t c=fast_cry_column ?
                avp_filter_cry_wall_column_sample(&filtered_column,v) :
                avp_filter_sample_prepared_shaded(t,mips,u,v,plan,
                                          0,shade,k_cry_rgb,&alpha,wall_shade_lut);
            if(!alpha)continue;
            if(occlusion&&occlusion->depth&&occlusion->width==fb->width&&
               occlusion->height==fb->height)occlusion->depth[at]=depth;
            fb->pixels[at]=alpha==255u?c:avp_gpu_hires_alpha_blend(fb->pixels[at],c,alpha);
        }
        return;
    }
#if defined(__clang__) && !defined(_WIN32)
    /* R428: no portal has written this column, and the terminal wall has
     * exactly one opaque span. The depth surface was cleared to FLT_MAX. */
    if(hit->fresh_depth && state && state->host_game_micro_optimization &&
       wall_h>=128 && t->width==128 && t->height==128 && t->cry16 && !t->alpha &&
       occlusion && occlusion->depth && occlusion->width==fb->width &&
       occlusion->height==fb->height && depth<FLT_MAX){
        avp_micro_opaque_wall128_fresh(fb->pixels,occlusion->depth,fb->width,sx,
            y0,y1,base_y0,wall_h,t->cry16,(unsigned)source_tx,source_y_shift,
            shade,k_cry_rgb,depth);
        return;
    }
#endif
#if defined(__GNUC__) && !defined(__clang__)
    /* R419 measured a GCC benefit but a Clang regression; retain the
     * previous renderer for Clang until a better implementation exists. */
    if(state && state->host_game_micro_optimization && wall_h>=128 &&
       t->width==128 && t->height==128 && t->cry16 && !t->alpha &&
       occlusion && occlusion->depth && occlusion->width==fb->width &&
       occlusion->height==fb->height){
        avp_micro_opaque_wall128(fb->pixels,occlusion->depth,fb->width,sx,
            y0,y1,base_y0,wall_h,t->cry16,(unsigned)source_tx,source_y_shift,
            shade,k_cry_rgb,depth);
        return;
    }
#endif
    int cached_ty=-1;
    uint32_t cached_color=0;
    unsigned cached_alpha=255u;
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
                if(!t->alpha)*owner=depth;
            }
            source_row=avp_wall_row_dda_value(&rows)+source_y_shift;
            if(source_row<0)source_row=0;
            if(source_row>127)source_row=127;
            ty=high_y?(int)sample_y:source_row*t->height/128;
            if(high_y)sample_y+=step_y;
            if(ty<0)ty=0;
            if(ty>=t->height)ty=t->height-1;
            if(ty!=cached_ty){
                cached_color=sample_tex_shaded(t,source_tx,ty,shade);
                cached_alpha=t->alpha?t->alpha[(size_t)ty*t->width+(unsigned)source_tx]:255u;
                cached_ty=ty;
            }
            if(t->alpha){
                if(cached_alpha==0u){avp_wall_row_dda_advance(&rows);continue;}
                if(have_depth)occlusion->depth[pixel_index]=depth;
                fb->pixels[pixel_index]=avp_gpu_hires_alpha_blend(
                    fb->pixels[pixel_index],cached_color,cached_alpha);
            }else fb->pixels[pixel_index]=cached_color;
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
                if(!t->alpha)*owner=depth;
            }
            source_row=avp_gpu_maze2_host_source_row(yy-base_y0,wall_h,source_y_shift);
            ty=high_y?(int)sample_y:source_row*t->height/128;
            if(high_y)sample_y+=step_y;
            if(ty<0)ty=0;
            if(ty>=t->height)ty=t->height-1;
            if(ty!=cached_ty){
                cached_color=sample_tex_shaded(t,source_tx,ty,shade);
                cached_alpha=t->alpha?t->alpha[(size_t)ty*t->width+(unsigned)source_tx]:255u;
                cached_ty=ty;
            }
            if(t->alpha){
                if(cached_alpha==0u){continue;}
                if(occlusion && occlusion->depth &&
                   occlusion->width==fb->width && occlusion->height==fb->height)
                    occlusion->depth[pixel_index]=depth;
                fb->pixels[pixel_index]=avp_gpu_hires_alpha_blend(
                    fb->pixels[pixel_index],cached_color,cached_alpha);
            }else fb->pixels[pixel_index]=cached_color;
        }
    }
}
static void draw_wall_seg(const AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,const PcRayHit *hit,unsigned sx,int y0,int y1,int base_y0,
                          int wall_h,const AvpGpuRgbImage *t,int source_tx,int shade,
                          AvpMaze2Occlusion *occlusion,float depth)
{
    draw_wall_seg_shift(state,fb,hit,sx,y0,y1,base_y0,wall_h,t,source_tx,0,shade,occlusion,depth);
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
    if(fid!=16u && state->host_world_tex && state->host_world_tex_enabled &&
       *state->host_world_tex_enabled && state->host_world_tex[fid].rgb)
        t=&state->host_world_tex[fid];
    if(fid==16u){
        unsigned ni;
        if(avp_gpu_wall_notice_index(logical_level,h->wall,&ni) && ni<18u){
            /* High-resolution signs are separate display assets: the retail
             * 128x128 composites remain untouched when the option is OFF. */
            if(state->host_world_tex_enabled && *state->host_world_tex_enabled &&
               state->host_notice_tex && state->host_notice_tex[ni].rgb)
                t=&state->host_notice_tex[ni];
            else if(state->notice_tex[ni].rgb)t=&state->notice_tex[ni];
        }else if(state->host_world_tex_enabled && *state->host_world_tex_enabled &&
                 state->host_world_tex && state->host_world_tex[16u].rgb)
            t=&state->host_world_tex[16u];
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
                if(t->width>128 && fb->height>228u && bottom_pair>top_pair){
                    double ft,fbm;
                    avp_gpu_hires_window_pair(state->window_pairs[wf-74u],pair_tx,
                        hires_mask_subcolumn(h->tc,reflected!=0,type==13u),1,&ft,&fbm);
                    mt=avp_gpu_hires_edge_y(y0,wall_h,ft);
                    mb=avp_gpu_hires_edge_y(y0,wall_h,fbm);
                }
                draw_wall_seg(state,fb,h,sx,y0,mt,y0,wall_h,t,tx,shade,occlusion,(float)d);
                draw_wall_seg(state,fb,h,sx,mb,y1,y0,wall_h,t,tx,shade,occlusion,(float)d);
            }
        }else if(!frame_only){
            /* Fail closed as a solid column only when the authored window list
               is unavailable; canonical retail resources should always load. */
            draw_wall_seg(state,fb,h,sx,y0,y1,y0,wall_h,t,tx,shade,occlusion,(float)d);
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
            if(t->width>128 && fb->height>228u && bottom_pair>top_pair){
                double ft,fbm;
                avp_gpu_hires_window_pair(state->window_pairs[wf-74u],
                    type==6u?door_tx:tx,hires_mask_subcolumn(h->tc,reflected!=0,0),
                    1,&ft,&fbm);
                mt=avp_gpu_hires_edge_y(y0,wall_h,ft);
                mb=avp_gpu_hires_edge_y(y0,wall_h,fbm);
            }
            int base_tx=(type==6u)?door_tx:tx;
            draw_wall_seg(state,fb,h,sx,y0,mt,y0,wall_h,t,base_tx,shade,occlusion,(float)d);
            draw_wall_seg(state,fb,h,sx,mb,y1,y0,wall_h,t,base_tx,shade,occlusion,(float)d);
            if(!frame_only&&mt<mb&&type!=6u){
                int moving=tx;
                if(avp_gpu_maze2_centered_door_source_col(tx,h->open,&moving))
                    draw_wall_seg(state,fb,h,sx,mt,mb,y0,wall_h,t,moving,shade,occlusion,(float)d);
            }
        }else if(!frame_only){
            int moving=(type==6u)?door_tx:tx;
            if(type==6u||avp_gpu_maze2_centered_door_source_col(tx,h->open,&moving))
                draw_wall_seg(state,fb,h,sx,y0,y1,y0,wall_h,t,moving,shade,occlusion,(float)d);
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
            draw_wall_seg_shift(state,fb,h,sx,y0,edge,y0,wall_h,t,tx,+open,shade,occlusion,(float)d);
        }else if(type==8u){
            /* cup_draw: two halves retract symmetrically.  GPU source bases
               are +open for the upper half and -open for the lower half. */
            int et=wall_edge_y(y0,wall_h,-open);
            int eb=wall_edge_y(y0,wall_h,+open);
            draw_wall_seg_shift(state,fb,h,sx,y0,et,y0,wall_h,t,tx,+open,shade,occlusion,(float)d);
            draw_wall_seg_shift(state,fb,h,sx,eb,y1,y0,wall_h,t,tx,-open,shade,occlusion,(float)d);
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
                draw_wall_seg(state,fb,h,sx,y0,wall_edge_y(y0,wall_h,wt),y0,wall_h,t,tx,shade,occlusion,(float)d);
                draw_wall_seg_shift(state,fb,h,sx,wall_edge_y(y0,wall_h,wt),wall_edge_y(y0,wall_h,xe),y0,wall_h,t,tx,+open,shade,occlusion,(float)d);
                draw_wall_seg(state,fb,h,sx,wall_edge_y(y0,wall_h,wb),y1,y0,wall_h,t,tx,shade,occlusion,(float)d);
            }else{
                int xt=12-open,xb=12+open;
                if(xt<wt)xt=wt;
                if(xt>wb)xt=wb;
                if(xb<wt)xb=wt;
                if(xb>wb)xb=wb;
                if(xb<xt){int q=xt;xt=xb;xb=q;}
                draw_wall_seg(state,fb,h,sx,y0,wall_edge_y(y0,wall_h,wt),y0,wall_h,t,tx,shade,occlusion,(float)d);
                draw_wall_seg_shift(state,fb,h,sx,wall_edge_y(y0,wall_h,wt),wall_edge_y(y0,wall_h,xt),y0,wall_h,t,tx,+open,shade,occlusion,(float)d);
                draw_wall_seg_shift(state,fb,h,sx,wall_edge_y(y0,wall_h,xb),wall_edge_y(y0,wall_h,wb),y0,wall_h,t,tx,-open,shade,occlusion,(float)d);
                draw_wall_seg(state,fb,h,sx,wall_edge_y(y0,wall_h,wb),y1,y0,wall_h,t,tx,shade,occlusion,(float)d);
            }
        }
        return;
    }
    draw_wall_seg(state,fb,h,sx,y0,y1,y0,wall_h,t,tx,shade,occlusion,(float)d);
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

/* R328: high-resolution AAF PNGs use ORIGINAL sheet/frame IDs and the
 * ORIGINAL projected AAF rectangle. Never change sprite timing or placement.
 * Each replacement has alpha and must have uniform integer scale. */
static int hires_image_matches(const AvpGpuRgbImage *im,int w,int h)
{
    return im&&im->rgb&&im->alpha&&w>0&&h>0&&im->width>=w&&im->height>=h&&
           im->width%w==0&&im->height%h==0&&im->width/w==im->height/h;
}
static const AvpGpuRgbImage *host_sprite_image(const AvpAvpGpuState *state,
                                               int enemy,unsigned sheet,unsigned frame)
{
    if(!state||!state->host_external_sprites||
       !state->host_world_tex_enabled||!*state->host_world_tex_enabled||
       sheet>=AVP_HIRES_AAF_SHEETS||frame>=AVP_HIRES_AAF_FRAMES)return NULL;
    return enemy?state->host_external_sprites->enemies[sheet][frame]:
                 state->host_external_sprites->props[sheet][frame];
}
/* Native source AAF bounds, not guessed PNG padding, anchor replacements. */
static int aaf_frame_bounds(const u8 *d,size_t n,unsigned fi,
                            int *left,int *right,int *top,int *bottom)
{
    unsigned frames,cols;size_t fo,tab;
    if(!d||n<4||!left||!right||!top||!bottom)return 0;
    frames=pc_be16(d);if(!frames||fi>=frames||4u+(size_t)frames*4u>n)return 0;
    fo=pc_be32(d+4u+(size_t)fi*4u);if(fo+2u>n)return 0;
    *left=(int)(int8_t)d[fo];*right=(int)(int8_t)d[fo+1u];
    if(*right<=*left)return 0;
    cols=(unsigned)(*right-*left);tab=fo+2u;
    if(tab+(size_t)cols*2u>n)return 0;
    *top=128;*bottom=-128;
    for(unsigned ci=0;ci<cols;ci++){
        size_t rel=pc_be16(d+tab+(size_t)ci*2u),co=fo+rel;
        if(!avp_gpu_aaf_column_offset_valid(rel,cols)||co+2u>n)continue;
        int t=(int)(int8_t)d[co],b=(int)(int8_t)d[co+1u];
        if(b<=t||co+2u+(size_t)(b-t)*2u>n)continue;
        if(t<*top)*top=t;
        if(b>*bottom)*bottom=b;
    }
    return *bottom>*top && *bottom-*top<=256;
}
static int draw_amp_sprite_replacement(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
    const AvpAmp *a,PcSpriteChoice ch,const AvpGpuRgbImage *source,
    double px,double py,double fx,double fy,double rx,double ry,double focal,
    const double *zbuf,const AvpMaze2Occlusion *occlusion,
    u32 *thermal_pixels,unsigned heat_style,unsigned target)
{
    int left,right,top,bottom,x0,x1,y0,y1;double sx_scale,sy_scale;
    const u8 *d=state->aaf[ch.aaf];size_t n=state->aaf_size[ch.aaf];
    unsigned frames=n>=4u?pc_be16(d):0;
    if(!frames||!aaf_frame_bounds(d,n,(unsigned)a->animframe%frames,
                                   &left,&right,&top,&bottom) ||
       !hires_image_matches(source,right-left,bottom-top))return 0;
    double wx=(double)a->xpos/65536.0-px,wy=(double)a->ypos/65536.0-py;
    double forward=wx*fx+wy*fy;
    if(forward<=0.04||forward>=avp_host_ray_limit(state->host_extend_draw_distance))return 1;
    double side=wx*rx+wy*ry;
    double cx=fb->width*0.5+(side/forward-state->host_stereo_projection_shift)*focal;
    avp_gpu_sprite_viewport_scale(fb->width,fb->height,focal,forward,sprite_rescale,
                                  &sx_scale,&sy_scale);
    if(sx_scale<0.03||sy_scale<0.03)return 1;
    double oy=(double)fb->height*0.5+a->yoffset*sy_scale;
    x0=(int)floor(cx+left*sx_scale);x1=(int)ceil(cx+right*sx_scale);
    y0=(int)floor(oy+top*sy_scale);y1=(int)ceil(oy+bottom*sy_scale);
    if(!avp_gpu_sprite_clip_span(&x0,&x1,(int)fb->width)||
       !avp_gpu_sprite_clip_span(&y0,&y1,(int)fb->height))return 1;
    const unsigned mode=(state->host_filter_mask&(1u<<target))?
                         state->host_filter_mode:AVP_TEXFILTER_NEAREST;
    const AvpFilterDerivatives dd={1.0/((right-left)*sx_scale),0.0,0.0,
                                   1.0/((bottom-top)*sy_scale)};
    const AvpFilterImage *mips=NULL;
    if(mode==AVP_TEXFILTER_NEAREST_MIP && state->host_filter_cache)
        mips=avp_filter_cache_lookup(state->host_filter_cache,source);
    const AvpFilterPlan plan=avp_filter_plan(source,mips,dd,mode,0u);
    const int use_occlusion=occlusion&&occlusion->depth&&
        occlusion->width==fb->width&&occlusion->height==fb->height;
    const int living=thermal_pixels&&avp_thermal_is_living(a);
    const double dist=living?hypot(wx,wy):0;
    /* R390: identical V expression once per source output row rather than
     * once for each x column. If allocation fails use the exact old path. */
    double *sample_v=avp_gpu_sprite_scratch_rows(
        &state->host_aaf_sprite_scratch,(size_t)(y1-y0));
    if(sample_v)for(int y=y0;y<y1;y++)
        sample_v[y-y0]=(((double)y+0.5-oy)/sy_scale-top)/(bottom-top);
    for(int x=x0;x<x1;x++){
        if(zbuf&&forward>=zbuf[x])continue;
        double u=(((double)x+0.5-cx)/sx_scale-left)/(right-left);
        if(ch.mirror)u=1.0-u;
        for(int y=y0;y<y1;y++){
            if(use_occlusion&&avp_gpu_maze2_occlusion_blocks(occlusion,x,y,(float)forward))continue;
            const double v=sample_v?sample_v[y-y0]:
                (((double)y+0.5-oy)/sy_scale-top)/(bottom-top);
            unsigned alpha=0;
            const u32 c=avp_filter_sample_prepared(source,mips,u,v,plan,0,0,k_cry_rgb,&alpha);
            if(!alpha)continue;
            size_t at=(size_t)y*fb->width+(unsigned)x;
            fb->pixels[at]=alpha==255u?c:avp_gpu_hires_alpha_blend(fb->pixels[at],c,alpha);
            if(living)thermal_pixels[at]=alpha>=128u?avp_thermal_record(
                ((heat_style==AVP_VISION_ENHANCED_THERMAL||
                   heat_style==AVP_VISION_ENHANCED_INFRARED)?
                   avp_thermal_heat_intensity(a,dist,c):avp_thermal_intensity(a,dist,c)),c):0u;
        }
    }
    return 1;
}

/* R308: decode exactly one requested original AAF frame into a transient
 * covered image for source-space sampling. Empty AAF columns and cry==0
 * remain transparent, rather than blurring arbitrary framebuffer contents.
 * The unchanged column raster below is used for every disabled target. */
static int draw_amp_sprite_filtered(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
    const AvpAmp *a,PcSpriteChoice ch,double px,double py,double fx,double fy,
    double rx,double ry,double focal,const double *zbuf,
    const AvpMaze2Occlusion *occlusion,u32 *thermal_pixels,unsigned heat_style)
{
    const u8 *d=state->aaf[ch.aaf];
    size_t n=state->aaf_size[ch.aaf];
    if(n<4)return 0;
    unsigned frames=pc_be16(d);
    if(!frames||4u+(size_t)frames*4u>n)return 0;
    unsigned fi=(unsigned)a->animframe%frames;
    size_t fo=pc_be32(d+4u+(size_t)fi*4u);
    if(fo+2u>n)return 0;
    int left=(int)(int8_t)d[fo],right=(int)(int8_t)d[fo+1u];
    if(right<=left)return 0;
    unsigned cols=(unsigned)(right-left);
    size_t table=fo+2u;
    if(table+(size_t)cols*2u>n)return 0;
    /* R389: visibility is independent of the decoded AAF pixels. Reject
     * actors behind the camera/outside the draw limit before decoding and
     * allocating their whole source image on every presentation frame. */
    double wx=(double)a->xpos/65536.0-px,wy=(double)a->ypos/65536.0-py;
    double forward=wx*fx+wy*fy;
    if(forward<=0.04||forward>=avp_host_ray_limit(state->host_extend_draw_distance))
        return 1;
    double side=wx*rx+wy*ry;
    double cx=(double)fb->width*0.5+
        (side/forward-state->host_stereo_projection_shift)*focal;
    double sx_scale,sy_scale;
    avp_gpu_sprite_viewport_scale(fb->width,fb->height,focal,forward,sprite_rescale,
                                  &sx_scale,&sy_scale);
    if(sx_scale<0.03||sy_scale<0.03)return 1;
    int x0=(int)floor(cx+left*sx_scale),x1=(int)ceil(cx+right*sx_scale);
    if(!avp_gpu_sprite_clip_span(&x0,&x1,(int)fb->width))return 1;
    int top=128,bottom=-128;
    for(unsigned ci=0;ci<cols;ci++){
        size_t rel=pc_be16(d+table+(size_t)ci*2u),co=fo+rel;
        if(!avp_gpu_aaf_column_offset_valid(rel,cols)||co+2u>n)continue;
        int a0=(int)(int8_t)d[co],a1=(int)(int8_t)d[co+1u];
        if(a1<=a0||co+2u+(size_t)(a1-a0)*2u>n)continue;
        if(a0<top)top=a0;
        if(a1>bottom)bottom=a1;
    }
    if(top>=bottom||bottom-top>256)return 0;
    unsigned height=(unsigned)(bottom-top);
    size_t pixels=(size_t)cols*height;
    if(!pixels||pixels>256u*256u)return 0;
    const double origin_y=(double)fb->height*0.5+(double)a->yoffset*sy_scale;
    int y0=(int)floor(origin_y+top*sy_scale),y1=(int)ceil(origin_y+bottom*sy_scale);
    if(!avp_gpu_sprite_clip_span(&y0,&y1,(int)fb->height))return 1;
    /* R389: each viewport owns its filtered AAF decode buffers. Retain
     * their capacity across actors/frames; clear exactly as the three old
     * calloc() calls did before filling the currently selected frame. */
    if(!avp_gpu_sprite_scratch_prepare(&state->host_aaf_sprite_scratch,pixels))
        return 0;
    AvpGpuRgbImage sprite={0};
    sprite.width=(int)cols;sprite.height=(int)height;
    sprite.rgb=state->host_aaf_sprite_scratch.rgb;
    sprite.cry16=state->host_aaf_sprite_scratch.cry16;
    sprite.alpha=state->host_aaf_sprite_scratch.alpha;
    for(unsigned ci=0;ci<cols;ci++){
        unsigned sci=ch.mirror?cols-1u-ci:ci;
        size_t rel=pc_be16(d+table+(size_t)sci*2u),co=fo+rel;
        if(!avp_gpu_aaf_column_offset_valid(rel,cols)||co+2u>n)continue;
        int a0=(int)(int8_t)d[co],a1=(int)(int8_t)d[co+1u];
        if(a1<=a0||co+2u+(size_t)(a1-a0)*2u>n)continue;
        for(int row=a0;row<a1;row++){
            u16 cry=pc_be16(d+co+2u+(size_t)(row-a0)*2u);
            if(!cry)continue;
            size_t offset=(size_t)(row-top)*cols+ci;
            u8 *rgb8=sprite.rgb+offset*3u;
            uint32_t c=k_cry_rgb[cry];
            rgb8[0]=(u8)(c>>16u);rgb8[1]=(u8)(c>>8u);rgb8[2]=(u8)c;
            sprite.cry16[offset]=cry;sprite.alpha[offset]=255u;
        }
    }
    AvpFilterImage *mips=&state->host_aaf_sprite_scratch.mip_chain;
    if(state->host_filter_mode==AVP_TEXFILTER_NEAREST_MIP)
        (void)avp_filter_image_rebuild(mips,&sprite,state->qconv,state->qconv_size);
    AvpFilterDerivatives dd={1.0/((double)cols*sx_scale),0.0,0.0,
                             1.0/((double)height*sy_scale)};
    /* R310: the source dimensions and projected size are constant over this
     * AAF sprite. Resolve the exact same mip/filter plan once per sprite,
     * never once per destination pixel. In crowded MP this avoids thousands
     * of repeated hypot/log2 evaluations for each sprite. */
    const AvpFilterPlan sprite_plan=avp_filter_plan(&sprite,
        (state->host_filter_mode==AVP_TEXFILTER_NEAREST_MIP && mips->levels)?mips:NULL,dd,state->host_filter_mode,0u);
    const int use_occlusion=occlusion&&occlusion->depth&&
        occlusion->width==fb->width&&occlusion->height==fb->height;
    const int living=thermal_pixels&&avp_thermal_is_living(a);
    const double dist=living?hypot(wx,wy):0.0;
    double *sample_v=avp_gpu_sprite_scratch_rows(
        &state->host_aaf_sprite_scratch,(size_t)(y1-y0));
    if(sample_v)for(int y=y0;y<y1;y++)
        sample_v[y-y0]=(((double)y+0.5-origin_y)/sy_scale-(double)top)/(double)height;
    for(int x=x0;x<x1;x++){
        if(zbuf&&forward>=zbuf[x])continue;
        double u=(((double)x+0.5-cx)/sx_scale-(double)left)/(double)cols;
        for(int y=y0;y<y1;y++){
            if(use_occlusion&&avp_gpu_maze2_occlusion_blocks(occlusion,x,y,(float)forward))continue;
            double v=sample_v?sample_v[y-y0]:
                (((double)y+0.5-origin_y)/sy_scale-(double)top)/(double)height;
            unsigned alpha=255u;
            uint32_t c=avp_filter_sample_prepared(&sprite,
                (state->host_filter_mode==AVP_TEXFILTER_NEAREST_MIP && mips->levels)?mips:NULL,
                u,v,sprite_plan,0,0,k_cry_rgb,&alpha);
            if(!alpha)continue;
            size_t at=(size_t)y*fb->width+(unsigned)x;
            fb->pixels[at]=alpha==255u?c:avp_gpu_hires_alpha_blend(fb->pixels[at],c,alpha);
            if(living){
                thermal_pixels[at]=alpha>=128u?avp_thermal_record(
                    ((heat_style==AVP_VISION_ENHANCED_THERMAL||
                      heat_style==AVP_VISION_ENHANCED_INFRARED)?
                    avp_thermal_heat_intensity(a,dist,c):avp_thermal_intensity(a,dist,c)),c):0u;
            }
        }
    }
    /* Mip level zero borrows sprite (a stack-local descriptor), not its
     * storage. Never retain a dangling descriptor between sprite draws. */
    mips->source=NULL;
    return 1;
}

static void draw_amp_sprite(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,const AvpAmp *a,double px,double py,double fx,double fy,double rx,double ry,double focal,const double *zbuf,const AvpMaze2Occlusion *occlusion,u32 *thermal_pixels,unsigned heat_style)
{
    PcSpriteChoice ch=sprite_choice(a,px,py); const u8 *d,*fr; size_t n,fo,coltab; unsigned frames,fi,ncols,ci; int left,right,use_occlusion; double wx,wy,forward,side,cx,scale_x,scale_y;
    if(ch.aaf<0||ch.aaf>=63||!state->aaf[ch.aaf])return;
    {
        const unsigned enemy=(a->creature==AC_ALIEN||a->creature==AC_PREDATOR||
            a->creature==AC_HUMAN||a->creature==AC_EGG||a->creature==AC_FACEHUG||
            a->creature==AC_ACRAWL||a->creature==AC_AQUEEN);
        const unsigned frame=state->aaf_size[ch.aaf]>=4u && pc_be16(state->aaf[ch.aaf])?
            (unsigned)a->animframe%pc_be16(state->aaf[ch.aaf]):0u;
        const AvpGpuRgbImage *rep=host_sprite_image(state,(int)enemy,(unsigned)ch.aaf,frame);
        if(rep && draw_amp_sprite_replacement(state,fb,a,ch,rep,px,py,fx,fy,rx,ry,
            focal,zbuf,occlusion,thermal_pixels,heat_style,
            enemy?AVP_FILTER_ENEMIES:AVP_FILTER_PROPS))return;
    }
    if(state->host_filter_mode){
        const unsigned enemy=(a->creature==AC_ALIEN||a->creature==AC_PREDATOR||
            a->creature==AC_HUMAN||a->creature==AC_EGG||a->creature==AC_FACEHUG||
            a->creature==AC_ACRAWL||a->creature==AC_AQUEEN);
        const unsigned target=enemy?AVP_FILTER_ENEMIES:AVP_FILTER_PROPS;
        if((state->host_filter_mask&(1u<<target))&&
           draw_amp_sprite_filtered(state,fb,a,ch,px,py,fx,fy,rx,ry,focal,
                                     zbuf,occlusion,thermal_pixels,heat_style))return;
    }
    d=state->aaf[ch.aaf];n=state->aaf_size[ch.aaf];if(n<4)return;frames=pc_be16(d);if(!frames||4u+(size_t)frames*4u>n)return;
    fi=(unsigned)a->animframe%frames;fo=pc_be32(d+4u+(size_t)fi*4u);if(fo+2u>n)return;fr=d+fo;left=(int)(int8_t)fr[0];right=(int)(int8_t)fr[1];if(right<left)return;ncols=(unsigned)(right-left);coltab=fo+2u;if(coltab+(size_t)ncols*2u>n)return;
    wx=(double)a->xpos/65536.0-px;wy=(double)a->ypos/65536.0-py;forward=wx*fx+wy*fy;if(forward<=0.04||forward>=avp_host_ray_limit(state ? state->host_extend_draw_distance : 0))return;side=wx*rx+wy*ry;cx=(double)fb->width*0.5+(side/forward-(state?state->host_stereo_projection_shift:0.0))*focal;
    avp_gpu_sprite_viewport_scale(fb->width,fb->height,focal,forward,sprite_rescale,
                                  &scale_x,&scale_y);
    if(scale_x<0.03||scale_y<0.03)return;
    use_occlusion=occlusion&&occlusion->depth&&occlusion->width==fb->width&&occlusion->height==fb->height;
    /* The same original sprite visibility, clipping and depth logic decides
     * which heat texels can actually be seen. Other actors/props overwrite
     * older coverage as the existing far-to-near layer is drawn. */
    const int living=thermal_pixels && avp_thermal_is_living(a);
    const double heat_distance=living?hypot(wx,wy):0.0;
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
        struct { int y0,y1; u32 color,heat; int active; } source_rows[256];
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
            source_rows[syi].heat=living ? avp_thermal_record(
                ((heat_style==AVP_VISION_ENHANCED_THERMAL ||
                  heat_style==AVP_VISION_ENHANCED_INFRARED) ?
                 avp_thermal_heat_intensity(a,heat_distance,source_rows[syi].color) :
                 avp_thermal_intensity(a,heat_distance,source_rows[syi].color)),
                source_rows[syi].color) : 0u;
            source_rows[syi].active=1;
        }
#if defined(__linux__) || (defined(__clang__) && !defined(_WIN32)) || defined(AVP_R434_TEST_SPRITE_SPANS)
        /* R434: group the same visible source-column samples into contiguous
         * runs and paint row-major. Keep the thermal path and small spans on
         * the original loop. X ownership is still tested with the exact
         * inverse projection, and the wall column test is unchanged. */
        if(state->host_game_micro_optimization && !thermal_pixels && x1-x0>=4){
            sx=x0;
            while(sx<x1){
                while(sx<x1 && (avp_gpu_sprite_sample_coord(sx,cx,scale_x)!=(int)left+(int)ci ||
                    (zbuf&&forward>=zbuf[sx])))++sx;
                int start=sx;
                while(sx<x1 && avp_gpu_sprite_sample_coord(sx,cx,scale_x)==(int)left+(int)ci &&
                    !(zbuf&&forward>=zbuf[sx]))++sx;
                if(start==sx)continue;
                for(syi=0;syi<npix;syi++)if(source_rows[syi].active)
                    avp_micro_sprite_span(fb->pixels,use_occlusion?occlusion->depth:NULL,
                        fb->width,start,sx,source_rows[syi].y0,source_rows[syi].y1,
                        source_rows[syi].color,(float)forward);
            }
            continue;
        }
#endif
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
                    const size_t at=(size_t)yy*fb->width+(unsigned)sx;
                    fb->pixels[at]=source_rows[syi].color;
                    if(thermal_pixels)thermal_pixels[at]=source_rows[syi].heat;
                }
            }
        }
    }
}

static void draw_amp_layer(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,double px,double py,double fx,double fy,double rx,double ry,double focal,const double *zbuf,const AvpMaze2Occlusion *occlusion,int smooth_presentation,u32 *thermal_pixels,unsigned heat_style)
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
        draw_amp_sprite(state,fb,&t,px,py,fx,fy,rx,ry,focal,zbuf,occlusion,thermal_pixels,heat_style);
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
    avp_gpu_overlay_present_state(state,&st,smooth_presentation,player_type,cur_wepno);

    /* Source-authored first move_in pose for Marine, Alien and Predator
     * Combi-Stick. No gameplay commands are executed early. */
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
    if(state->host_filter_mode==AVP_TEXFILTER_BILINEAR &&
       (state->host_filter_mask & (1u<<AVP_FILTER_WEAPONS)))
        avp_filter_native_overlay(fb->pixels,fb->width,fb->height,before,after);
    else avp_gpu_overlay_scaled_composite(fb->pixels,fb->width,fb->height,before,after);
}

/* RE40 R197: render only first-person OPs directly across the expanded
 * host world.  The former 4:3 op_view scratch clipped the Predator right
 * arm/Combi-Stick and other original OP sprites at x=320 before the wider
 * host frame could see them.  Keep the original weapon offsets, retail
 * palettes, 60-Hz USE alternation and interpolated sway; clip only at the
 * REAL framebuffer edge.  The Jaguar 4:3 path remains byte-for-byte the
 * original overlay_first_person_scaled() implementation above. */
static const AvpGpuRgbImage *host_fp_image(const AvpAvpGpuState *state,unsigned file)
{
    if(!state||!state->host_external_sprites||!state->host_world_tex_enabled||
       !*state->host_world_tex_enabled||file<AVP_HIRES_FILES_FIRST||
       file>=AVP_HIRES_FILES_FIRST+AVP_HIRES_FILES_COUNT)return NULL;
    return state->host_external_sprites->weapons[file-AVP_HIRES_FILES_FIRST];
}
/* The replacement PNG is mapped to the original retail OP screen rectangle,
 * regardless of source resolution. The original screen footprint, sway and
 * animation selection are invariant. PNGs with a different aspect ratio are
 * stretched to that original rectangle; authors should preserve the native
 * aspect ratio to avoid intentional/unintentional distortion. */
static int draw_fp_rgba_wide(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                             const AvpGpuRgbImage *img,int sw,int sh,int nx,int ny)
{
    unsigned cw;double left,scale_x,scale_y;int x0,x1,y0,y1;
    if(!avp_gpu_hires_fp_image_usable(img,sw,sh)||!fb||!fb->pixels)return 0;
    cw=avp_host_wide_center_width(fb->height);
    if(!cw||cw>fb->width)return 0;
    left=((double)fb->width-(double)cw)*0.5;
    scale_x=(double)cw/320.0;scale_y=(double)fb->height/228.0;
    x0=(int)floor(left+nx*scale_x);x1=(int)ceil(left+(nx+sw)*scale_x);
    y0=(int)floor(ny*scale_y);y1=(int)ceil((ny+sh)*scale_y);
    if(!avp_gpu_sprite_clip_span(&x0,&x1,(int)fb->width)||
       !avp_gpu_sprite_clip_span(&y0,&y1,(int)fb->height))return 1;
    const unsigned mode=(state->host_filter_mode==AVP_TEXFILTER_BILINEAR&&
        (state->host_filter_mask&(1u<<AVP_FILTER_WEAPONS)))?
        AVP_TEXFILTER_BILINEAR:AVP_TEXFILTER_NEAREST;
    const AvpFilterDerivatives dd={1.0/(sw*scale_x),0.0,0.0,1.0/(sh*scale_y)};
    const AvpFilterPlan plan=avp_filter_plan(img,NULL,dd,mode,0u);
    /* R388: the weapon's U projection is invariant across all output rows.
     * Retain the exact old expression per X; do not introduce incremental
     * floating-point UV drift. Oversized viewports use the original path. */
    double column_uv[AVP_FP_COLUMN_UV_CACHE_MAX];
    const int have_column_uv=avp_gpu_fp_column_uv_fill(column_uv,
        AVP_FP_COLUMN_UV_CACHE_MAX,x0,x1,left,scale_x,nx,sw);
    for(int y=y0;y<y1;y++){
        /* Every pixel on this scanline shares the same vertically projected
         * source coordinate. Keep the original floating-point expression and
         * evaluation order; do not alter the actual texture sampler. */
        const double v=(((double)y+0.5)/scale_y-ny)/sh;
        for(int x=x0;x<x1;x++){
        const double u=have_column_uv?column_uv[x-x0]:
            (((double)x+0.5-left)/scale_x-nx)/sw;
        unsigned alpha=0;
        u32 color=avp_filter_sample_prepared(img,NULL,u,v,plan,0,0,k_cry_rgb,&alpha);
        if(!alpha)continue;
        size_t at=(size_t)y*fb->width+(unsigned)x;
        fb->pixels[at]=alpha==255u?color:
            avp_gpu_hires_alpha_blend(fb->pixels[at],color,alpha);
        }
    }
    return 1;
}

static void draw_fp_use_wide(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                             const AvpFirstPersonOverlayState *st,int use_id)
{
    AvpFpUseOverlayDesc d;int slot,x,y;
    if(!avp_gpu_fp_use_overlay_desc(use_id,&d))return;
    slot=d.file-133;
    if(slot<0||slot>=52||!state->fp_img[slot]||state->fp_pal_valid[slot]!=16u)return;
    x=160+d.x_offset+(int)(st->x>>16)+(st->swing_enabled?st->swing_x:0);
    y=114+d.y_offset+(int)(st->y>>16)+(st->swing_enabled?st->swing_y:0);
    if(draw_fp_rgba_wide(state,fb,host_fp_image(state,d.file),d.width,d.height,x,y))return;
#if defined(__clang__) /* Retain R426 after fullscreen regression. */
    (state->host_filter_mode==AVP_TEXFILTER_BILINEAR &&
     (state->host_filter_mask & (1u<<AVP_FILTER_WEAPONS))?
     avp_gpu_fp_wide_blit_bilinear:avp_gpu_fp_wide_blit)(fb->pixels,fb->width,fb->height,
        state->fp_img[slot],state->fp_size[slot],d.width,d.height,
        state->fp_pal[slot],state->fp_pal_valid[slot],x,y,1);
#else
    (state->host_filter_mode==AVP_TEXFILTER_BILINEAR &&
     (state->host_filter_mask & (1u<<AVP_FILTER_WEAPONS))?
     avp_gpu_fp_wide_blit_bilinear:
     (state->host_game_micro_optimization && fb->height>=1080u
      ?avp_micro_fp_wide_blit:avp_gpu_fp_wide_blit))(fb->pixels,fb->width,fb->height,
        state->fp_img[slot],state->fp_size[slot],d.width,d.height,
        state->fp_pal[slot],state->fp_pal_valid[slot],x,y,1);
#endif
}

static void overlay_first_person_wide(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                                      u64 vblank_counter,int smooth_presentation)
{
    AvpFirstPersonOverlayState st;AvpFpOverlayDesc inf;unsigned phase;int x,y,slot;
    avp_mazescrn_get_overlay_state(&st);
    avp_gpu_overlay_present_state(state,&st,smooth_presentation,player_type,cur_wepno);
    if(st.intro_preview){st.x=st.intro_x;st.y=st.intro_y;}
    phase=avp_gpu_fp_vblank_parity(vblank_counter);
    if(st.use1_visible&&st.use2_visible){
        if(phase==0u)draw_fp_use_wide(state,fb,&st,st.use1_id);
        else draw_fp_use_wide(state,fb,&st,st.use2_id);
    }else{
        if(st.use1_visible)draw_fp_use_wide(state,fb,&st,st.use1_id);
        if(st.use2_visible)draw_fp_use_wide(state,fb,&st,st.use2_id);
    }
    if(!st.overlay_visible||!avp_gpu_fp_overlay_desc(st.overlay_id,&inf))return;
    slot=inf.file-133;
    if(slot<0||slot>=52||!state->fp_img[slot]||state->fp_pal_valid[slot]!=208u)return;
    x=160+inf.x_offset+(int)(st.x>>16)+(st.swing_enabled?st.swing_x:0);
    y=114+inf.y_offset+(int)(st.y>>16)+(st.swing_enabled?st.swing_y:0);
    if(draw_fp_rgba_wide(state,fb,host_fp_image(state,inf.file),inf.width,inf.height,x,y))return;
#if defined(__clang__) /* Retain R426 after fullscreen regression. */
    (state->host_filter_mode==AVP_TEXFILTER_BILINEAR &&
     (state->host_filter_mask & (1u<<AVP_FILTER_WEAPONS))?
     avp_gpu_fp_wide_blit_bilinear:avp_gpu_fp_wide_blit)(fb->pixels,fb->width,fb->height,
        state->fp_img[slot],state->fp_size[slot],inf.width,inf.height,
        state->fp_pal[slot],state->fp_pal_valid[slot],x,y,0);
#else
    (state->host_filter_mode==AVP_TEXFILTER_BILINEAR &&
     (state->host_filter_mask & (1u<<AVP_FILTER_WEAPONS))?
     avp_gpu_fp_wide_blit_bilinear:
     (state->host_game_micro_optimization && fb->height>=1080u
      && !(inf.file>=179 && inf.file<=182)
      ?avp_micro_fp_wide_blit:avp_gpu_fp_wide_blit))(fb->pixels,fb->width,fb->height,
        state->fp_img[slot],state->fp_size[slot],inf.width,inf.height,
        state->fp_pal[slot],state->fp_pal_valid[slot],x,y,0);
#endif
}

/* Do not change the original native OP raster merely because some *other*
 * weapon record has a replacement. The currently displayed IDs alone decide
 * whether we need the direct, host-resolution replacement compositor. */
static int fp_has_active_replacement(AvpAvpGpuState *state)
{
    AvpFirstPersonOverlayState st;
    AvpFpOverlayDesc body;
    AvpFpUseOverlayDesc use;
    if(!state||!state->host_external_sprites||!state->host_world_tex_enabled||
       !*state->host_world_tex_enabled||!state->host_external_sprites->weapon_count)return 0;
    avp_mazescrn_get_overlay_state(&st);
    if(st.overlay_visible&&avp_gpu_fp_overlay_desc(st.overlay_id,&body)&&
       avp_gpu_hires_fp_image_usable(host_fp_image(state,body.file),body.width,body.height))return 1;
    if(st.use1_visible&&avp_gpu_fp_use_overlay_desc(st.use1_id,&use)&&
       avp_gpu_hires_fp_image_usable(host_fp_image(state,use.file),use.width,use.height))return 1;
    if(st.use2_visible&&avp_gpu_fp_use_overlay_desc(st.use2_id,&use)&&
       avp_gpu_hires_fp_image_usable(host_fp_image(state,use.file),use.width,use.height))return 1;
    return 0;
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
    /* FONT.S positions are Jaguar signed 16-bit coordinates.  The generic
     * runtime callback transports their raw 16-bit representation as an
     * unsigned argument: convert it back before clipping the Blitter brush.
     * In particular an X of -2 is a partly visible glyph, not a glyph at
     * unsigned X=65534 that should disappear entirely.  Signed wider
     * arithmetic also avoids accidental wraparound at the right edge. */
    const int origin_x=(int)(s16)(u16)x;
    const int origin_y=(int)(s16)(u16)y;
    for(unsigned yy=0;yy<9u;yy++)for(unsigned xx=0;xx<width;xx++){
        const int64_t dx=(int64_t)origin_x+(int64_t)xx;
        const int64_t dy=(int64_t)origin_y+(int64_t)yy;
        int pv;
        if(dx<0||dx>=320||dy<0||dy>=240)continue;
        pv=computer_font_bitpair(state,brush,source_x+xx,yy);
        if(clear||pv==0)state->computer_pixels[(size_t)dy*320u+(size_t)dx]=0;
        else state->computer_pixels[(size_t)dy*320u+(size_t)dx]=0xff000000u|computer_col(state,(unsigned)pv);
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
                /* The Jaguar clips glyph brushes against the CTEXT plane;
                 * don't discard a whole glyph when its signed origin is
                 * just outside the screen and some columns remain visible. */
                avp_gpu_computer_draw_glyph(state,b,sx,w,
                                            (unsigned)(u16)x,(unsigned)(u16)y,0);
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

/* R328: static background PNGs cannot overwrite live HUD information.
 * The rectangles below are the same authored native HUD coordinates already
 * used by the original HUD compositor, plus the weapon icon table. */
static int hud_dynamic_pixel(unsigned sp,int x,int sy)
{
    const int local_y=sy-14;
    if((sp==0u && ((x>=151&&x<231&&sy>=22&&sy<31)||
                   (x>=43&&x<59&&sy>=104&&sy<113))) ||
       (sp==1u&&x>=148&&x<228&&sy>=24&&sy<34) ||
       (sp==2u&&x>=111&&x<221&&sy>=24&&sy<36))return 1;
    if(x>=nrg_x&&x<nrg_x+nrg_barwidth &&
       sy>=nrg_y+14&&sy<nrg_y+14+nrg_height)return 1;
    if(sp==2u&&x>=AVP_PRED_SCOPE_X&&x<AVP_PRED_SCOPE_X+AVP_PRED_SCOPE_W&&
       sy>=AVP_PRED_SCOPE_Y+14&&sy<AVP_PRED_SCOPE_Y+14+AVP_PRED_SCOPE_H)return 1;
    if(sp==0u&&show_mt&&x>=16&&x<128&&sy>=125&&sy<214)return 1;
    if(map_on&&x>=map_info.x&&x<map_info.x+map_info.w&&
       sy>=map_info.y+14&&sy<map_info.y+14+map_info.h)return 1;
    for(int i=0;i<num_weps&&i<6;i++){
        const AvpAmmoInfo *a=&ammo_info[i];
        if(x>=a->x&&x<a->x+44&&sy>=a->y+14&&sy<a->y+22)return 1;
    }
    const unsigned slots=sp==0u?4u:sp==1u?3u:6u;
    for(unsigned slot=1;slot<=slots;slot++){
        AvpHudWeaponIconRect r=avp_hud_weapon_rect(sp,slot);
        /* Checkerboard clear can touch the right/bottom neighbour. */
        if(x>=r.x&&x<=r.x+r.width&&local_y>=r.y&&
           local_y<=r.y+r.height)return 1;
    }
    /* All HUD message cells should remain native, including temporary
     * notices appearing over otherwise static artwork. */
    if(sy>=14&&sy<50&&x>=20&&x<300)return 1;
    return 0;
}
static const AvpGpuRgbImage *host_hud_static_image(const AvpAvpGpuState *state,
                                                   unsigned sp)
{
    if(!state||!state->host_external_sprites||!state->host_world_tex_enabled||
       !*state->host_world_tex_enabled||sp>=AVP_HIRES_HUD_COUNT)return NULL;
    const AvpGpuRgbImage *im=state->host_external_sprites->hud[sp];
    return hires_image_matches(im,320,200)?im:NULL;
}
/* Composite ONLY static HUD art in screen space. The native dynamic HUD
 * renderer still supplies exact score, bars, tracker, fading icons and text.
 * Skip those regions even if a PNG contains opaque pixels there. */
static int draw_hud_static_png(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                              u32 *mask,unsigned mask_stride)
{
    unsigned sp=(unsigned)player_type>>2;
    if(sp>2u)sp=0u;
    const AvpGpuRgbImage *im=host_hud_static_image(state,sp);
    if(!im||!fb||!fb->pixels||!fb->width||!fb->height)return 0;
    /* A fixed 320x200 mask avoids re-evaluating weapon/map/HUD rectangles
     * for every one of the million-plus output pixels at 1080p. Local to
     * this render call, safe for split-screen/parallel host presentation. */
    unsigned char dynamic[320u*200u];
    for(int sy=14;sy<214;sy++)for(int sx=0;sx<320;sx++)
        dynamic[(size_t)(sy-14)*320u+(unsigned)sx]=(unsigned char)
            hud_dynamic_pixel(sp,sx,sy);
    const unsigned mode=(state->host_filter_mode==AVP_TEXFILTER_BILINEAR &&
        (state->host_filter_mask&(1u<<AVP_FILTER_HUD)))?
        AVP_TEXFILTER_BILINEAR:AVP_TEXFILTER_NEAREST;
    const AvpFilterDerivatives dd={1.0/fb->width,0,0,1.0/fb->height};
    const AvpFilterPlan plan=avp_filter_plan(im,NULL,dd,mode,0u);
    for(unsigned y=0;y<fb->height;y++){
        const double native_y=((double)y+0.5)*228.0/fb->height;
        if(native_y<14.0||native_y>=214.0)continue;
        const double v=(native_y-14.0)/200.0;
        for(unsigned x=0;x<fb->width;x++){
            const double native_x=((double)x+0.5)*320.0/fb->width;
            if(dynamic[((unsigned)native_y-14u)*320u+(unsigned)native_x])continue;
            const double u=native_x/320.0;
            unsigned alpha=0;
            u32 c=avp_filter_sample_prepared(im,NULL,u,v,plan,0,0,k_cry_rgb,&alpha);
            if(!alpha)continue;
            size_t at=(size_t)y*fb->width+x;
            fb->pixels[at]=alpha==255u?c:
                avp_gpu_hires_alpha_blend(fb->pixels[at],c,alpha);
            if(mask)mask[(size_t)y*mask_stride+x]=1u;
        }
    }
    return 1;
}

/* MAZE5 writes the $EC palette index into the HUD layer; zero pixels in
 * that layer are transparent when the Jaguar Object Processor displays it. */
/* One HUD-only compositor bridge for the static brush, live meters,
 * tracker, weapon bars and automap. OFF follows retail Jaguar RMW bit-for-
 * bit; ON substitutes only the destination hue, not the source palette,
 * brightness, ink or underlying world scene. */
static u32 hud_rmw_pixel(const AvpAvpGpuState *state,u32 backdrop,u16 ink)
{
    if(state->host_game_micro_optimization && !state->hud_fixed_hue && k_cry_rgb_ready)
        return avp_micro_cry_rmw(backdrop,ink,state->qconv,state->qconv_size,k_cry_rgb);
    return state->hud_fixed_hue ?
        avp_jaguar_rmw_fixed_hue_xrgb8888(backdrop,ink,state->qconv,state->qconv_size) :
        avp_jaguar_rmw_xrgb8888(backdrop,ink,state->qconv,state->qconv_size);
}

/* Capture every original HUD ink operation while rendering its unchanged
 * native brush. Replay those operations on each host pixel later; unlike
 * enlarging already blended native pixels, this preserves 3D detail visible
 * behind the translucent HUD. */
static void hud_rmw_apply_native(const AvpAvpGpuState *state,
                                  AvpJaguarFramebuffer *fb,size_t offset,u16 ink)
{
    if(state && state->host_hud_replay_active &&
       fb->width==320u && fb->height==228u)
#ifdef AVP_HUD_SPARSE_GCC
        avp_hud_replay_record_pass(state->host_hud_replay_active,
#else
        avp_gpu_hud_replay_record((AvpHudRmwOps*)state->host_hud_replay_active,
#endif
                                  offset,ink);
    fb->pixels[offset]=hud_rmw_pixel(state,fb->pixels[offset],ink);
}
static u32 hud_rmw_host_callback(void *context,u32 backdrop,u16 ink)
{
    return hud_rmw_pixel((const AvpAvpGpuState*)context,backdrop,ink);
}

/* The Jaguar map's teammate/player/cheat glyphs retain their authored HUD
 * pixel sizes. When drawing the map at output resolution, expand only these
 * tiny glyphs to their exact native-HUD spans; wall strokes are sampled at
 * host pixel centres separately below. */
static void hud_map_ink(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                        int x,int y,int x0,int y0,int x1,int y1,
                        u32 *background_mask,unsigned background_stride)
{
    unsigned sp;
    size_t di;
    if(x<x0||x>=x1||y<y0||y>=y1||x<0||y<0||!fb||!fb->pixels)return;
    sp=(unsigned)player_type>>2;
    if(sp>2u)sp=0u;
    if(fb->width!=320u||fb->height!=228u){
        unsigned sx0=avp_gpu_hires_automap_edge((unsigned)x,fb->width,320u);
        unsigned sx1=avp_gpu_hires_automap_edge((unsigned)x+1u,fb->width,320u);
        unsigned sy0=avp_gpu_hires_automap_edge((unsigned)y,fb->height,228u);
        unsigned sy1=avp_gpu_hires_automap_edge((unsigned)y+1u,fb->height,228u);
        for(unsigned yy=sy0;yy<sy1;yy++)for(unsigned xx=sx0;xx<sx1;xx++){
            di=(size_t)yy*fb->width+xx;
            fb->pixels[di]=hud_rmw_pixel(state,fb->pixels[di],state->hud_active_pal[sp][0xecu&31u]);
            if(background_mask)background_mask[(size_t)yy*background_stride+xx]=1u;
        }
        return;
    }
    if((unsigned)x>=fb->width||(unsigned)y>=fb->height)return;
    di=(size_t)y*fb->width+(unsigned)x;
    hud_rmw_apply_native(state,fb,di,state->hud_active_pal[sp][0xecu&31u]);
}

/* The Jaguar expands MAZE5.GAS's one-bit map_data into an 8-bit
 * temporary image before rotating it into HUD.S's per-species map window.
 * Keep this native 320x228 HUD stage independent of world 3D resolution. */
static void overlay_automap(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                            const u8 *maze,u16 mw,u16 mh,double px,double py,
                            double fx,double fy,double rx,double ry,
                            u32 *background_mask,unsigned background_stride)
{
    /* HUD.S stores map_info in the 320x200 HUD bitmap's local coords.
     * MAZESCRN.S positions that Object Processor bitmap at Y=14.  The
     * map is drawn over the scene only after this OP translation. */
    int mx0=map_info.x,my0=map_info.y+14,mwpx=map_info.w,mhpx=map_info.h;
    int mcx=mx0+mwpx/2,mcy=my0+mhpx/2;
    int sy,sx;
    u8 completed_hatch[64u*64u];
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
    if(state->host_fill_automap_hatch_gaps)
        avp_gpu_automap_hatch_completion(state,maze,mw,mh,completed_hatch);
    viewpoint_x=state->map_smooth_camera?(u32)(px*65536.0):x_pos;
    viewpoint_y=state->map_smooth_camera?(u32)(py*65536.0):y_pos;
    /* sin_d0/cos_d0 are signed 1.15 fixed point (peak 32767).
     * Smooth Presentation must use the same scale: 16384 magnified the
     * automap by 2x whenever interpolation was enabled. */
    viewpoint_sin=state->map_smooth_camera?(s32)(-fy*32767.0):sin_ang;
    viewpoint_cos=state->map_smooth_camera?(s32)(fx*32767.0):cos_ang;
    /* Window-Resolution 3D: draw each *output* map pixel from the original
     * 512x512 explored bitmap. The native 320x228 pass below remains exact
     * when the enhancement is disabled, or when the view is too small. */
    if(state->host_hires_automap && fb->width>320u && fb->height>228u){
        unsigned mx_begin=avp_gpu_hires_automap_edge((unsigned)mx0,fb->width,320u);
        unsigned mx_end=avp_gpu_hires_automap_edge((unsigned)(mx0+mwpx),fb->width,320u);
        unsigned my_begin=avp_gpu_hires_automap_edge((unsigned)my0,fb->height,228u);
        unsigned my_end=avp_gpu_hires_automap_edge((unsigned)(my0+mhpx),fb->height,228u);
        const double xstep=320.0/(double)fb->width/zoom;
        const double xstart=((double)mx_begin+0.5)*320.0/(double)fb->width;
        unsigned sp=(unsigned)player_type>>2;
        u16 ink;
        if(sp>2u)sp=0u;
        ink=state->hud_active_pal[sp][0xecu&31u];
        for(unsigned hy=my_begin;hy<my_end;hy++){
            const double ny=((double)hy+0.5)*228.0/(double)fb->height-0.5;
            const double sample_y=(ny-(double)mcy)/zoom+(double)(mhpx/2);
            const double sample_x=(xstart-0.5-(double)mcx)/zoom+(double)(mwpx/2);
            AvpGpuHiresAutomapRow row;
            avp_gpu_hires_automap_row_begin(&row,viewpoint_x,viewpoint_y,
                viewpoint_sin,viewpoint_cos,(unsigned)mwpx,(unsigned)mhpx,
                sample_x,sample_y,xstep);
            for(unsigned hx=mx_begin;hx<mx_end;hx++){
                int ix,iy,lit=0;
                avp_gpu_hires_automap_row_next(&row,&ix,&iy);
                if(ix>=0&&iy>=0&&ix<512&&iy<512){
                    lit=(state->automap_bits[(size_t)iy*AVP_AUTOMAP_ROW_BYTES+(unsigned)ix/8u]&
                         (0x80u>>(unsigned)(ix&7)))!=0;
                    if(!lit&&state->host_fill_automap_hatch_gaps&&
                       completed_hatch[(unsigned)(iy>>3)*64u+(unsigned)(ix>>3)]){
                        static const u8 hatch[4]={0x11u,0x22u,0x44u,0x88u};
                        lit=(hatch[iy&3]&(0x80u>>(ix&7)))!=0;
                    }
                }
                if(lit){
                    size_t di=(size_t)hy*fb->width+hx;
                    fb->pixels[di]=hud_rmw_pixel(state,fb->pixels[di],ink);
                    if(background_mask)background_mask[(size_t)hy*background_stride+hx]=1u;
                }
            }
        }
    }else for(sy=my0;sy<my0+mhpx;sy++){
        for(sx=mx0;sx<mx0+mwpx;sx++){
            int ix,iy,lit=0;
            /* Keep the MAZE5.GAS 16.16 blitter transformation unchanged.
             * Smooth Presentation substitutes only the host's interpolated
             * player viewpoint; explored map pixels remain simulation-owned. */
            avp_gpu_automap_sample(viewpoint_x,viewpoint_y,viewpoint_sin,viewpoint_cos,
                                   (unsigned)mwpx,(unsigned)mhpx,
                                   (int)lrint((double)(sx-mcx)/zoom)+(mwpx/2),
                                   (int)lrint((double)(sy-mcy)/zoom)+(mhpx/2),&ix,&iy);
            if(ix>=0&&iy>=0&&ix<512&&iy<512){
                lit=(state->automap_bits[(size_t)iy*AVP_AUTOMAP_ROW_BYTES+(unsigned)ix/8u]&
                     (0x80u>>(unsigned)(ix&7)))!=0;
                if(!lit&&state->host_fill_automap_hatch_gaps&&
                   completed_hatch[(unsigned)(iy>>3)*64u+(unsigned)(ix>>3)]){
                    static const u8 hatch[4]={0x11u,0x22u,0x44u,0x88u};
                    lit=(hatch[iy&3]&(0x80u>>(ix&7)))!=0;
                }
            }
            /* MAZE5.GAS writes 0xEC to the PIXEL8 HUD bitmap where a map
             * bit is set, and index zero everywhere else. In the original
             * HUD Object Processor layer zero is transparent: never paint
             * solid black over the 3D world for an absent map bit. */
            if(lit)hud_map_ink(state,fb,sx,sy,mx0,my0,mx0+mwpx,my0+mhpx,
                               background_mask,background_stride);
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
                            mx0,my0,mx0+mwpx,my0+mhpx,background_mask,background_stride);
        }
    }
    /* R227 cheat: the POSITION rotates with the Jaguar map, but the glyph
     * never does. Every label is an upright 5x7 HUD raster centered on its
     * actor's actual 16.16 position, not on its tile. Smooth presentation
     * interpolates the SAME AMP snapshot/generation as the 3D sprite pass. */
    if(state->host_show_enemies_on_map){
        static const u8 rows[6][7]={
          {14,17,17,31,17,17,17}, /* A */
          {14,17,17,17,21,18,13}, /* Q */
          {31,16,16,30,16,16,16}, /* F */
          {31,16,16,30,16,16,31}, /* E */
          {17,27,21,21,17,17,17}, /* M */
          {30,17,17,30,16,16,16}  /* P */
        };
        for(unsigned ai=0;ai<AVP_NUM_AMPS;ai++){
            const AvpAmp *a=&level1amps[ai];
            s32 ex=a->xpos,ey=a->ypos;
            int gx,gy,gi;
            char letter=avp_gpu_automap_enemy_letter(a);
            if(!letter)continue;
            if(state->map_smooth_camera && state->have_prev_sim &&
               state->amp_prev_generation[ai]==avp_amp_slot_generation(ai)){
                ex=avp_presentation_interpolate_fixed(state->amp_prev_x[ai],ex,
                    state->render_alpha,2L<<16);
                ey=avp_presentation_interpolate_fixed(state->amp_prev_y[ai],ey,
                    state->render_alpha,2L<<16);
            }
            switch(letter){case 'A':gi=0;break;case 'Q':gi=1;break;
                case 'F':gi=2;break;case 'E':gi=3;break;
                case 'M':gi=4;break;default:gi=5;break;}
            if(!avp_gpu_automap_enemy_screen((s32)viewpoint_x,(s32)viewpoint_y,
                viewpoint_sin,viewpoint_cos,zoom,ex,ey,mcx,mcy,&gx,&gy))continue;
            /* Center-coordinate clipping also bounds the glyph draw work. */
            if(gx<mx0-2||gx>=mx0+mwpx+2||gy<my0-3||gy>=my0+mhpx+3)continue;
            for(int row=0;row<7;row++)for(int col=0;col<5;col++)
                if(rows[gi][row] & (1u<<(4-col)))
                    hud_map_ink(state,fb,gx+col-2,gy+row-3,
                                mx0,my0,mx0+mwpx,my0+mhpx,background_mask,background_stride);
        }
    }
    /* MAZE5.GAS ends with a distinct 2x2 player blit at the centre. */
    for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++)
        hud_map_ink(state,fb,mcx-1+xx,mcy-1+yy,
                    mx0,my0,mx0+mwpx,my0+mhpx,background_mask,background_stride);
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
    hud_rmw_apply_native(state,fb,di,state->hud_active_pal[sp][idx]);
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
         if(!(state&&state->host_hires_tracker_blips_active)){
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
                        double fx,double fy,double rx,double ry,unsigned logical_level,
                        int defer_map_and_tracker)
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
#if defined(__linux__)
        if(state->host_game_micro_optimization){
            u8 excluded[320u*200u];
            avp_micro_hud_exclusions(excluded,sp,show_mt!=0,state->predator_scope_valid,
                nrg_x,nrg_y,nrg_barwidth,nrg_height,ammo_info,num_weps);
            for(int y=14;y<214;y++)for(int x=0;x<320;x++){
                size_t i=(size_t)(y-14)*320u+(size_t)x;
                u8 pi=base[i];
                if(!pi||excluded[i])continue;
                if(state->host_replace_hud_static&&!hud_dynamic_pixel(sp,x,y))continue;
                pi=avp_gpu_weapon_icon_pixel(pi,sp,x,y-14,cur_weps,
                    inwep_no,inwep_bright,outwep_no,outwep_bright);
                if(sp==2u)pi=avp_gpu_predator_faded_icon(pi,x,y-14,invis_bright,medpak_bright);
                if(pi)hud_rmw_apply_native(state,fb,(size_t)y*320u+(size_t)x,rmwpal[pi&31u]);
            }
        }else
#endif
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
            if(pi&&(!state->host_replace_hud_static||hud_dynamic_pixel(sp,x,y))&&
               !tracker_replaces_main_hud_pixel(sp,show_mt!=0,x,y)&&
               !value_cell&&!energy_cell&&!meter_cell&&!ammo_cell){size_t di=(size_t)y*320u+(size_t)x;
                hud_rmw_apply_native(state,fb,di,rmwpal[pi&31u]);}
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
                     hud_rmw_apply_native(state,fb,di,rmwpal[pi&31u]);}}
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
                    hud_rmw_apply_native(state,fb,di,rmwpal[pi&31u]);
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
                    hud_rmw_apply_native(state,fb,di,rmwpal[pi&31u]);}
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
    if(avp_mp_scope_visible(sp) && state->predator_scope_valid &&
       !state->host_hires_predator_awa_active){
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
    if(!defer_map_and_tracker){
        overlay_automap(state,fb,maze,mw,mh,px,py,fx,fy,rx,ry,NULL,0u);
        overlay_motion_tracker(state,fb,px,py,fx,fy,rx,ry);
    }
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
       (kind!=1u&&kind!=2u)||other_player<1u||other_player>AVP_GPU_DM_NOTICE_MAX_PLAYERS)return;
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
       (kind!=1u&&kind!=2u)||other_player<1u||other_player>AVP_GPU_DM_NOTICE_MAX_PLAYERS||
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

/* R304 independently selectable Marine tracker contact enhancement.
 * Only the dots use floating sub-HUD-pixel positions. The ring, brush and
 * numbers remain source Jaguar artwork, and gameplay range is unchanged.
 * Coverage of four fixed supersamples anti-aliases original 8x7 BLIPS ink. */
static int hires_tracker_brush_at(const AvpAvpGpuState *state,unsigned phase,
                                  int x,int y)
{
    if(x<0||x>=8||y<0||y>=7)return 0;
    if(state->tracker_blips&&state->tracker_blips_size>=56u)
        return tracker_bit(state->tracker_blips,state->tracker_blips_size,8,
                           (int)phase*8+x,y);
    return (((x+y+(int)phase)&3)==0)||((x==3||x==4)&&(y>=2&&y<=4));
}
static void overlay_tracker_blips_hires(AvpAvpGpuState *state,
    AvpJaguarFramebuffer *fb,double px,double py,double fx,double fy,
    double rx,double ry,u32 *mask,unsigned stride)
{
    const double sxscale=(double)fb->width/320.0,syscale=(double)fb->height/228.0;
    const u16 ink=state->hud_active_pal[0][0xecu&31u];
    if(player_type!=PT_HUMAN||!show_mt||fb->width<=320u||fb->height<=228u)return;
    for(unsigned i=0;i<AVP_NUM_AMPS+state->map_teammate_count;i++){
        double dx,dy,lat,forward,cx,cy;
        if(i<AVP_NUM_AMPS){
            const AvpAmp *a=&level1amps[i];
            if(!(a->mode||a->host_static)||!(a->flags&(1u<<AMP_MOTION)))continue;
            dx=(double)a->xpos/65536.0-px;dy=(double)a->ypos/65536.0-py;
        }else{
            const AvpGpuMapTeammate *a=&state->map_teammates[i-AVP_NUM_AMPS];
            dx=(double)(s32)a->x/65536.0-px;
            dy=(double)(s32)a->y/65536.0-py;
        }
        lat=dx*rx+dy*ry;forward=dx*fx+dy*fy;
        if(avp_gpu_tracker_contact_metres(lat,forward)>=15.0)continue;
        avp_gpu_hires_tracker_xy(lat,forward,&cx,&cy);
        cy+=TRACKER_HUD_SCREEN_Y;
        const unsigned phase=avp_gpu_tracker_blip_index(
            (s16)(s32)lrint(lat*256.0),(s16)(s32)lrint(forward*256.0));
        int x0=(int)floor((cx-4.0)*sxscale)-1;
        int x1=(int)ceil((cx+4.0)*sxscale)+1;
        int y0=(int)floor((cy-3.0)*syscale)-1;
        int y1=(int)ceil((cy+4.0)*syscale)+1;
        /* The original radar clipping box is in HUD-local pixels. */
        const int clipx0=(int)ceil(16.0*sxscale),clipx1=(int)floor(112.0*sxscale);
        const int clipy0=(int)ceil((111.0+TRACKER_HUD_SCREEN_Y)*syscale);
        const int clipy1=(int)floor((200.0+TRACKER_HUD_SCREEN_Y)*syscale);
        if(x0<clipx0)x0=clipx0;
        if(x1>clipx1)x1=clipx1;
        if(y0<clipy0)y0=clipy0;
        if(y1>clipy1)y1=clipy1;
        if(x1<0||y1<0||x0>=(int)fb->width||y0>=(int)fb->height)continue;
        if(x0<0)x0=0;
        if(y0<0)y0=0;
        if(x1>(int)fb->width)x1=(int)fb->width;
        if(y1>(int)fb->height)y1=(int)fb->height;
        for(int hy=y0;hy<y1;hy++)for(int hx=x0;hx<x1;hx++){
            unsigned covered=0;
            for(unsigned yy=0;yy<2u;yy++)for(unsigned xx=0;xx<2u;xx++){
                double nx=((double)hx+((double)xx+0.5)/2.0)/sxscale;
                double ny=((double)hy+((double)yy+0.5)/2.0)/syscale;
                int bx=(int)floor(nx-(cx-4.0)),by=(int)floor(ny-(cy-3.0));
                if(hires_tracker_brush_at(state,phase,bx,by))covered++;
            }
            if(covered){size_t di=(size_t)hy*fb->width+(unsigned)hx;
                u32 old=fb->pixels[di],full=hud_rmw_pixel(state,old,ink);
                if(covered==4u)fb->pixels[di]=full;
                else{
                    unsigned a=covered,b=4u-a;
                    unsigned r=(((old>>16)&255u)*b+((full>>16)&255u)*a+2u)/4u;
                    unsigned g=(((old>>8)&255u)*b+((full>>8)&255u)*a+2u)/4u;
                    unsigned blue=((old&255u)*b+(full&255u)*a+2u)/4u;
                    fb->pixels[di]=rgb(r,g,blue);
                }
                if(mask)mask[(size_t)hy*stride+(unsigned)hx]=1u;
            }
        }
    }
}
/* High-resolution Predator audio waveform raster. With detail disabled,
 * retain every original 43x45 occupied texel but anti-alias edges at output
 * resolution. Detail 2x/4x/8x uses additional DISPLAY positions between
 * the same three original HP2 samples per 10-Hz update, not new sound data. */
static void overlay_predator_awa_hires(AvpAvpGpuState *state,
    AvpJaguarFramebuffer *fb,u32 *mask,unsigned stride)
{
    const double sx=(double)fb->width/320.0,sy=(double)fb->height/228.0;
    const double basey=(double)AVP_PRED_SCOPE_Y+14.0;
    const unsigned detail=state->host_predator_awa_detail<=3u?
        state->host_predator_awa_detail:0u;
    const u16 ink=state->hud_active_pal[2][AVP_PRED_SCOPE_INK&31u];
    if(player_type!=PT_PREDATOR||!state->predator_scope_valid)return;
    unsigned beginx=avp_gpu_hires_automap_edge(AVP_PRED_SCOPE_X,fb->width,320u);
    unsigned endx=avp_gpu_hires_automap_edge(AVP_PRED_SCOPE_X+AVP_PRED_SCOPE_W,fb->width,320u);
    unsigned beginy=avp_gpu_hires_automap_edge(AVP_PRED_SCOPE_Y+14u,fb->height,228u);
    unsigned endy=avp_gpu_hires_automap_edge(AVP_PRED_SCOPE_Y+14u+AVP_PRED_SCOPE_H,fb->height,228u);
    for(unsigned y=beginy;y<endy;y++){
        double ny=((double)y+0.5)/sy-basey;
        int row=(int)floor(ny);
        if(row<0||row>=AVP_PRED_SCOPE_H)continue;
        for(unsigned x=beginx;x<endx;x++){
            double nx=((double)x+0.5)/sx-(double)AVP_PRED_SCOPE_X;
            double coverage=0.0;
            if(detail&&state->predator_scope_seg_valid[row]){
                /* The original HP2 inputs are exactly the row's endpoints.
                 * Quantization sublevels increase only VISUAL amplitude
                 * resolution, and keep the trace within its authored area. */
                double t=ny-(double)row;
                double a=(double)state->predator_scope_seg_a[row];
                double b=(double)state->predator_scope_seg_b[row];
                double position=(double)(AVP_PRED_SCOPE_W/2)+
                    avp_gpu_awa_detail_position((int)a,(int)b,t,detail);
                double d=fabs(nx-position);
                coverage=d<=0.30?1.0:(d<0.90?(0.90-d)/0.60:0.0);
            }else{
                /* Piecewise-linear source-bit coverage, not stretching a
                 * pre-blended low-resolution HUD image. */
                const double fx=nx-0.5,fy=ny-0.5;
                const int ix=(int)floor(fx),iy=(int)floor(fy);
                double tx=fx-ix,ty=fy-iy;
                for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++){
                    int ox=ix+xx,oy=iy+yy;
                    if(ox>=0&&ox<AVP_PRED_SCOPE_W&&oy>=0&&oy<AVP_PRED_SCOPE_H &&
                       state->predator_scope[(size_t)oy*AVP_PRED_SCOPE_W+(unsigned)ox])
                        coverage+=(xx?tx:1.0-tx)*(yy?ty:1.0-ty);
                }
            }
            if(coverage>0.0){size_t i=(size_t)y*fb->width+x;
                u32 old=fb->pixels[i],col=hud_rmw_pixel(state,old,ink);
                if(coverage>=1.0)fb->pixels[i]=col;
                else{
                    double inv=1.0-coverage;
                    fb->pixels[i]=rgb((unsigned)lrint(((old>>16)&255u)*inv+((col>>16)&255u)*coverage),
                        (unsigned)lrint(((old>>8)&255u)*inv+((col>>8)&255u)*coverage),
                        (unsigned)lrint((old&255u)*inv+(col&255u)*coverage));
                }
                if(mask)mask[(size_t)y*stride+x]=1u;
            }
        }
    }
}

/* One native HUD image drives all scaled output.  Keep original world
 * pixels in the before tile: only pixels modified by the HUD are projected
 * back into the full-resolution framebuffer, so the rest of the scene is
 * never downsampled or overwritten by the overlay. */
typedef void (*AvpRasterSpanFn)(void *,unsigned,unsigned);
static void avp_raster_spans(AvpRasterSpanFn fn,void *context,unsigned total,unsigned pixels);
static void avp_micro_hud_composite(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
    const u32 *before,const u32 *after,const AvpHudRmwOps *replay,u32 *mask,unsigned stride)
{
    AvpMicroHudReplay pass={0};
    pass.host=fb->pixels;pass.mask=mask;pass.before=before;pass.after=after;
    pass.rgb=k_cry_rgb;pass.ops=replay;pass.q=state->qconv;pass.qsize=state->qconv_size;
    pass.width=fb->width;pass.stride=stride;
    avp_micro_hud_replay_init(&pass,fb->height);
    avp_raster_spans(avp_micro_hud_replay_rows,&pass,228u,fb->width*fb->height);
}

static void overlay_hud(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
                        const u8 *maze,u16 mw,u16 mh,double px,double py,
                        double fx,double fy,double rx,double ry,unsigned logical_level,
                        u32 *background_mask,unsigned background_stride)
{
    const size_t count=320u*228u;
    AvpJaguarFramebuffer native;
    u32 *before,*after;
    AvpHudRmwOps *replay=NULL;
    if(!state||!fb||!fb->pixels||!fb->width||!fb->height)return;
    state->host_replace_hud_static=(unsigned char)
        draw_hud_static_png(state,fb,background_mask,background_stride);
    state->host_hires_tracker_blips_active=0u;
    state->host_hires_predator_awa_active=0u;
    if(fb->width==320u&&fb->height==228u){
        u32 *original=NULL;
        if(background_mask){
            if(!state->hud_native_tiles)
                state->hud_native_tiles=(u32*)malloc(2u*count*sizeof(u32));
            original=state->hud_native_tiles;
            if(original)memcpy(original,fb->pixels,count*sizeof(u32));
            else for(unsigned y=0;y<228u;y++)
                for(unsigned x=0;x<320u;x++)background_mask[(size_t)y*background_stride+x]=1u;
        }
        overlay_hud_native(state,fb,maze,mw,mh,px,py,fx,fy,rx,ry,logical_level,0);
        draw_hud_messages_native(state,fb);
        if(original)for(unsigned y=0;y<228u;y++)
            for(unsigned x=0;x<320u;x++)
                if(original[(size_t)y*320u+x]!=fb->pixels[(size_t)y*320u+x])
                    background_mask[(size_t)y*background_stride+x]=1u;
        state->host_replace_hud_static=0u;
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
    if(state->host_hud_full_resolution){
        if(!state->host_hud_replay_storage)
#ifdef AVP_HUD_SPARSE_GCC
            state->host_hud_replay_storage=avp_hud_replay_allocate(count);
        replay=avp_hud_replay_begin(state->host_hud_replay_storage,count,state->host_game_micro_optimization);
#else
            state->host_hud_replay_storage=malloc(count*sizeof(AvpHudRmwOps));
        replay=(AvpHudRmwOps*)state->host_hud_replay_storage;
#endif
        if(replay){
#ifdef AVP_HUD_SPARSE_GCC
            state->host_hud_replay_active=state->host_hud_replay_storage;
#else
            memset(replay,0,count*sizeof(*replay));
            state->host_hud_replay_active=replay;
#endif
        }
    }
    /* Only an actual high-resolution viewport with a visible map can defer
     * the native map + scanner. At other sizes, retain R302 unchanged. */
    const int hires_output=replay && state->host_hud_full_resolution &&
                           fb->width>320u && fb->height>228u;
    const int hires_map=hires_output && state->host_hires_automap && map_on && maze && mw && mh;
    const int hires_scope=hires_output && player_type==PT_PREDATOR &&
        state->predator_scope_valid &&
        (state->host_hires_predator_awa||state->host_predator_awa_detail);
    const int hires_blips=hires_output && state->host_hires_tracker_blips &&
                          player_type==PT_HUMAN && show_mt;
    const int defer=hires_map||hires_scope;
    state->host_hires_tracker_blips_active=(unsigned char)!!hires_blips;
    state->host_hires_predator_awa_active=(unsigned char)!!hires_scope;
    overlay_hud_native(state,&native,maze,mw,mh,px,py,fx,fy,rx,ry,
                       logical_level,defer);
    state->host_hud_replay_active=NULL;
    if(!defer)draw_hud_messages_native(state,&native);
    /* Invert the same native sampling map and visit only changed OP texels.
     * Sparse first-person sprites no longer require a full high-res screen
     * comparison or a width/height division for each destination pixel. */
    if(replay){
        if(state->host_filter_mode==AVP_TEXFILTER_BILINEAR &&
           (state->host_filter_mask & (1u<<AVP_FILTER_HUD)))
            avp_filter_hud_replay(fb->pixels,fb->width,fb->height,
                before,after,replay,hud_rmw_host_callback,state,
                background_mask,background_stride);
        else if(state->host_game_micro_optimization && !state->hud_fixed_hue && k_cry_rgb_ready)
            avp_micro_hud_composite(state,fb,before,after,replay,background_mask,background_stride);
        else avp_gpu_hud_replay_scaled_composite(fb->pixels,fb->width,fb->height,
            before,after,replay,hud_rmw_host_callback,state,
            background_mask,background_stride);
    }else{
        /* Jaguar-native and allocation-failure paths retain original output. */
        avp_gpu_overlay_scaled_composite(fb->pixels,fb->width,fb->height,before,after);
        if(background_mask)
            avp_predator_background_mark_scaled_overlay(background_mask,background_stride,
                                                         fb->width,fb->height,before,after);
    }
    if(hires_scope)overlay_predator_awa_hires(state,fb,background_mask,background_stride);
    if(defer){
        /* Native HUD -> Predator AWA -> map -> Marine radar -> text. If only
         * AWA is enhanced, the ORIGINAL map still overlays the analyzer. */
        if(hires_map)overlay_automap(state,fb,maze,mw,mh,px,py,fx,fy,rx,ry,
                                      background_mask,background_stride);
        avp_gpu_hud_sample_native(before,fb);
        memcpy(after,before,count*sizeof(u32));
#ifdef AVP_HUD_SPARSE_GCC
        replay=avp_hud_replay_begin(state->host_hud_replay_storage,count,state->host_game_micro_optimization);
        state->host_hud_replay_active=state->host_hud_replay_storage;
#else
        memset(replay,0,count*sizeof(*replay));
        state->host_hud_replay_active=replay;
#endif
        if(!hires_map)overlay_automap(state,&native,maze,mw,mh,px,py,fx,fy,rx,ry,NULL,0u);
        overlay_motion_tracker(state,&native,px,py,fx,fy,rx,ry);
        state->host_hud_replay_active=NULL;
        draw_hud_messages_native(state,&native);
        if(state->host_filter_mode==AVP_TEXFILTER_BILINEAR &&
           (state->host_filter_mask & (1u<<AVP_FILTER_HUD)))
            avp_filter_hud_replay(fb->pixels,fb->width,fb->height,
                before,after,replay,hud_rmw_host_callback,state,
                background_mask,background_stride);
        else if(state->host_game_micro_optimization && !state->hud_fixed_hue && k_cry_rgb_ready)
            avp_micro_hud_composite(state,fb,before,after,replay,background_mask,background_stride);
        else avp_gpu_hud_replay_scaled_composite(fb->pixels,fb->width,fb->height,
            before,after,replay,hud_rmw_host_callback,state,
            background_mask,background_stride);
    }
    if(hires_blips)overlay_tracker_blips_hires(state,fb,px,py,fx,fy,rx,ry,
                                                background_mask,background_stride);
    state->host_hires_tracker_blips_active=0u;
    state->host_hires_predator_awa_active=0u;
    state->host_replace_hud_static=0u;
}

/* R421: avoid the two widescreen crop transfers for the ordinary point HUD.
 * All other HUD modes retain the existing crop compositor. Allocate before
 * modifying any state, so allocation failure can take the original path. */
static int avp_micro_direct_wide_hud(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
    unsigned width,unsigned left,const u8 *maze,u16 mw,u16 mh,
    double px,double py,double fx,double fy,double rx,double ry,
    unsigned level,u32 *mask)
{
    const size_t count=320u*228u;
    unsigned sp=(unsigned)player_type>>2;
    if(sp>2u)sp=0u;
    if(!state->host_game_micro_optimization || !state->host_hud_full_resolution ||
       state->hud_fixed_hue || !k_cry_rgb_ready ||
       (state->host_filter_mode==AVP_TEXFILTER_BILINEAR &&
        (state->host_filter_mask&(1u<<AVP_FILTER_HUD))) ||
       state->host_hires_automap || state->host_hires_tracker_blips ||
       state->host_hires_predator_awa || state->host_predator_awa_detail ||
       host_hud_static_image(state,sp) || width<=320u || fb->height<=228u ||
       (uint64_t)width*fb->height<1920u*1080u)return 0;
    if(!state->hud_native_tiles)state->hud_native_tiles=malloc(2u*count*sizeof(u32));
#ifdef AVP_HUD_SPARSE_GCC
    if(!state->host_hud_replay_storage)state->host_hud_replay_storage=avp_hud_replay_allocate(count);
#else
    if(!state->host_hud_replay_storage)state->host_hud_replay_storage=malloc(count*sizeof(AvpHudRmwOps));
#endif
    if(!state->hud_native_tiles||!state->host_hud_replay_storage)return 0;
    u32 *before=state->hud_native_tiles,*after=before+count;
#ifdef AVP_HUD_SPARSE_GCC
    AvpHudRmwOps *ops=avp_hud_replay_begin(state->host_hud_replay_storage,count,state->host_game_micro_optimization);
#else
    AvpHudRmwOps *ops=state->host_hud_replay_storage;
#endif
    AvpJaguarFramebuffer native={0};
    native.width=320u;native.height=228u;native.pixels=after;
    avp_micro_hud_sample_strided(before,fb->pixels+left,width,fb->height,fb->width);
    memcpy(after,before,count*sizeof(u32));
#ifdef AVP_HUD_SPARSE_GCC
#else
    memset(ops,0,count*sizeof(*ops));
#endif
    state->host_replace_hud_static=0u;
    state->host_hires_tracker_blips_active=0u;
    state->host_hires_predator_awa_active=0u;
#ifdef AVP_HUD_SPARSE_GCC
    state->host_hud_replay_active=state->host_hud_replay_storage;
#else
    state->host_hud_replay_active=ops;
#endif
    overlay_hud_native(state,&native,maze,mw,mh,px,py,fx,fy,rx,ry,level,0);
    state->host_hud_replay_active=NULL;
    draw_hud_messages_native(state,&native);
    AvpMicroHudReplay pass={0};
    pass.host=fb->pixels+left;pass.mask=mask?mask+left:NULL;
    pass.before=before;pass.after=after;pass.rgb=k_cry_rgb;pass.ops=ops;
    pass.q=state->qconv;pass.qsize=state->qconv_size;
    pass.width=width;pass.stride=fb->width;pass.host_stride=fb->width;
    avp_micro_hud_replay_init(&pass,fb->height);
    avp_raster_spans(avp_micro_hud_replay_rows,&pass,228u,width*fb->height);
    return 1;
}

/* The cartridge's MazeList combines the pain ramp, temporary fades,
 * and Predator vision modifier into one Jaguar CLUT entry. Apply that
 * hardware-space RMW modifier to the scene, not to individual HUD glyphs.
 * Normal gameplay takes the zero-cost neutral path. */

typedef struct AvpClutPass {AvpJaguarFramebuffer *fb;AvpDamagePresentationBlend blend;const u8 *q;size_t n;int micro;} AvpClutPass;
static void avp_clut_rows(void *arg,unsigned first,unsigned last)
{
    const AvpClutPass *p=(const AvpClutPass*)arg;
    u32 *px=p->fb->pixels+(size_t)first*p->fb->width;
    const size_t count=(size_t)(last-first)*p->fb->width;
    if(p->blend.weight==256u || p->blend.previous==p->blend.current){
        if(p->blend.current){
            if(p->micro)for(size_t i=0;i<count;i++)
                px[i]=avp_micro_cry_rmw(px[i],p->blend.current,p->q,p->n,k_cry_rgb);
            else for(size_t i=0;i<count;i++)
                px[i]=avp_jaguar_rmw_xrgb8888(px[i],p->blend.current,p->q,p->n);
        }
    }else if(p->micro){
        for(size_t i=0;i<count;i++)
            px[i]=avp_micro_damage_blend(px[i],p->blend,p->q,p->n,k_cry_rgb);
    }else for(size_t i=0;i<count;i++)
        px[i]=avp_damage_blend_pixel(px[i],p->blend,p->q,p->n);
}
static void apply_game_clut(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,int smooth)
{
    AvpClutPass pass;
    if(!state||!fb||!fb->pixels||!state->qconv||state->qconv_size<131072u)return;
    const AvpDamagePresentationSample current=avp_damage_sample(fade_c,xtra_c,
        player_type,cur_level,fade_level==0 && !destruct_flag && !player_dead && player_energy>0);
    pass.blend=avp_damage_blend(state->prev_damage,current,fade_level,
        smooth && state->have_prev_sim,state->render_alpha);
    if(!pass.blend.current && (!pass.blend.previous || pass.blend.weight==256u))return;
    /* Per-pixel pure CRY read-modify-write: rows split across the host
     * raster workers give identical output (native size stays inline). */
    pass.fb=fb;pass.q=state->qconv;pass.n=state->qconv_size;
    pass.micro=state->host_game_micro_optimization && k_cry_rgb_ready;
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


/* R383: Window-resolution 3D and the Software fallback share this raster
 * dispatcher. Previously every floor/wall/CLUT/VMODE pass created and joined
 * up to fifteen kernel threads. That is costly and can cause scheduler spikes
 * in Software *and* in Vulkan's Software-reference pass.
 *
 * On POSIX, keep one small pool per calling render thread (pthread TLS), so
 * simultaneous multiplayer viewports remain independent. Every dispatch is
 * synchronous: workers own disjoint row/column spans, and all are complete
 * before the caller returns. No Jaguar simulation state is accessed by the
 * pool itself. The old direct invocation remains the failure fallback.
 */
typedef struct AvpRasterSpan {
    AvpRasterSpanFn fn;
    void *context;
    unsigned first,last;
} AvpRasterSpan;
#ifndef _WIN32
enum { AVP_RASTER_MAX_THREADS = 8 };
typedef struct AvpRasterPool AvpRasterPool;
typedef struct AvpRasterWorker {
    AvpRasterPool *pool;
    unsigned index;
} AvpRasterWorker;
struct AvpRasterPool {
    pthread_mutex_t mutex;
    pthread_cond_t start;
    pthread_cond_t finished;
    pthread_t threads[AVP_RASTER_MAX_THREADS-1u];
    AvpRasterWorker workers[AVP_RASTER_MAX_THREADS-1u];
    AvpRasterSpan jobs[AVP_RASTER_MAX_THREADS];
    unsigned worker_count;
    unsigned done;
    unsigned generation;
    int stop;
};
static pthread_key_t avp_raster_pool_key;
static pthread_once_t avp_raster_pool_once=PTHREAD_ONCE_INIT;
static int avp_raster_pool_key_valid;
static void *avp_raster_worker_thread(void *arg)
{
    AvpRasterWorker *w=(AvpRasterWorker*)arg;
    AvpRasterPool *p=w->pool;
    unsigned seen=0;
    pthread_mutex_lock(&p->mutex);
    for(;;){
        AvpRasterSpan job;
        while(!p->stop && p->generation==seen)
            pthread_cond_wait(&p->start,&p->mutex);
        if(p->stop)break;
        seen=p->generation;
        job=p->jobs[w->index];
        pthread_mutex_unlock(&p->mutex);
        job.fn(job.context,job.first,job.last);
        pthread_mutex_lock(&p->mutex);
        p->done++;
        if(p->done==p->worker_count)
            pthread_cond_signal(&p->finished);
    }
    pthread_mutex_unlock(&p->mutex);
    return NULL;
}
static void avp_raster_pool_destroy(void *ptr)
{
    AvpRasterPool *p=(AvpRasterPool*)ptr;
    if(!p)return;
    pthread_mutex_lock(&p->mutex);
    p->stop=1;
    pthread_cond_broadcast(&p->start);
    pthread_mutex_unlock(&p->mutex);
    for(unsigned i=0;i<p->worker_count;i++)
        (void)pthread_join(p->threads[i],NULL);
    pthread_cond_destroy(&p->finished);
    pthread_cond_destroy(&p->start);
    pthread_mutex_destroy(&p->mutex);
    free(p);
}
static void avp_raster_pool_make_key(void)
{
    avp_raster_pool_key_valid=
        pthread_key_create(&avp_raster_pool_key,avp_raster_pool_destroy)==0;
}
static AvpRasterPool *avp_raster_pool_get(unsigned desired)
{
    AvpRasterPool *p;
    if(pthread_once(&avp_raster_pool_once,avp_raster_pool_make_key)!=0 ||
       !avp_raster_pool_key_valid)return NULL;
    p=(AvpRasterPool*)pthread_getspecific(avp_raster_pool_key);
    if(p)return p;
    p=(AvpRasterPool*)calloc(1,sizeof(*p));
    if(!p)return NULL;
    if(pthread_mutex_init(&p->mutex,NULL)!=0){free(p);return NULL;}
    if(pthread_cond_init(&p->start,NULL)!=0){
        pthread_mutex_destroy(&p->mutex);free(p);return NULL;
    }
    if(pthread_cond_init(&p->finished,NULL)!=0){
        pthread_cond_destroy(&p->start);
        pthread_mutex_destroy(&p->mutex);free(p);return NULL;
    }
    /* Workers may begin waiting as soon as they are created. Generation 0
     * represents no dispatched job; partial thread creation is safe. */
    for(unsigned i=0;i+1u<desired;i++){
        p->workers[i].pool=p;
        p->workers[i].index=i;
        if(pthread_create(&p->threads[i],NULL,avp_raster_worker_thread,
                          &p->workers[i])!=0)break;
        p->worker_count++;
    }
    if(!p->worker_count || pthread_setspecific(avp_raster_pool_key,p)!=0){
        avp_raster_pool_destroy(p);
        return NULL;
    }
    return p;
}
static void avp_raster_spans(AvpRasterSpanFn fn,void *context,
                             unsigned total,unsigned pixels)
{
    AvpRasterPool *p;
    unsigned n;
    long available;
    if(pixels<640u*480u || total<4u){fn(context,0,total);return;}
    available=sysconf(_SC_NPROCESSORS_ONLN);
    if(available<2){fn(context,0,total);return;}
    n=(unsigned long)available<AVP_RASTER_MAX_THREADS ?
        (unsigned)available:AVP_RASTER_MAX_THREADS;
    if(n>total)n=total;
    p=avp_raster_pool_get(n);
    if(!p){fn(context,0,total);return;}
    n=p->worker_count+1u;
    /* One TLS pool is owned by this caller, so there is no cross-viewport
     * contention and no need to allocate or create threads per pass. */
    pthread_mutex_lock(&p->mutex);
    p->done=0u;
    for(unsigned i=0;i<n;i++){
        p->jobs[i].fn=fn;
        p->jobs[i].context=context;
        p->jobs[i].first=(unsigned)((uint64_t)total*i/n);
        p->jobs[i].last=(unsigned)((uint64_t)total*(i+1u)/n);
    }
    p->generation++;
    pthread_cond_broadcast(&p->start);
    pthread_mutex_unlock(&p->mutex);
    fn(context,p->jobs[n-1u].first,p->jobs[n-1u].last);
    pthread_mutex_lock(&p->mutex);
    while(p->done<p->worker_count)
        pthread_cond_wait(&p->finished,&p->mutex);
    pthread_mutex_unlock(&p->mutex);
}
#else
/* Original Win32 threading path preserved pending a separate native pool
 * implementation and platform-side verification. */
static DWORD WINAPI avp_raster_thread(LPVOID arg)
{
    AvpRasterSpan *s=(AvpRasterSpan*)arg;
    s->fn(s->context,s->first,s->last);
    return 0;
}
static void avp_raster_spans(AvpRasterSpanFn fn,void *context,
                             unsigned total,unsigned pixels)
{
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
}
#endif

#ifndef _WIN32
void avp_micro_frame_copy_parallel(const uint32_t *src,uint32_t *dst,unsigned w,unsigned h)
{
    AvpMicroFrameCopy pass={src,dst,w};
#if defined(__linux__) && defined(__SSE2__)
    /* R448/R449: the 1440p full-frame transfer is the measured Linux hot path.
     * The caller already checks Game Micro-Optimization and resolution. */
    if(w>=1920u && h>=1080u){
        avp_raster_spans(avp_r448_copy_stream,&pass,h,w*h);return;
    }
#endif
    avp_raster_spans(avp_micro_frame_copy_rows,&pass,h,w*h);
}
#endif

typedef struct AvpFloorPass {
    AvpAvpGpuState *state;
    AvpJaguarFramebuffer *fb;
    const u8 *maze;
    u16 mw,mh;
    double px,py,fx,fy,rx,ry,focal;
    const AvpGpuRgbImage *flat_tex[256];
    const AvpFilterImage *flat_mip[256];
    unsigned flat_rotation[256];
    AvpMaze2Occlusion *clear_depth;
    int clear_color_holes;
} AvpFloorPass;
static void avp_floor_rows(void *arg,unsigned first,unsigned last)
{
    const AvpFloorPass *p=(const AvpFloorPass*)arg;
    AvpJaguarFramebuffer *fb=p->fb;
    unsigned x,y;
    if(p->clear_depth)avp_micro_depth_clear_rows(p->clear_depth,first,last);
    for(y=first;y<last;y++){
        double retail_row=((double)y+0.5)*228.0/(double)fb->height -
            (p->state && p->state->map_editor_world_only
                ?p->state->map_editor_pitch*228.0*0.52:0.0);
        int ceiling=retail_row<114.0;
        double perp,lefttan,step,wx,wy,wxstep,wystep;
        int floor_shade;
        if(!avp_host_flat_row_depth(retail_row,
               p->state ? p->state->host_extend_draw_distance : 0,&perp)){
            if(p->clear_color_holes)memset(fb->pixels+(size_t)y*fb->width,0,
                                         (size_t)fb->width*sizeof(*fb->pixels));
            continue;
        }
        floor_shade=avp_host_fog_shade_delta(avp_gpu_retail_flat_intensity_delta(
            avp_host_fog_distance(perp,p->state ? p->state->host_extend_fog_distance : 0)),
            p->state && p->state->host_disable_fog);
        /* R389: floor rows and successive frames often share the exact
         * signed fog delta. Reuse the 256-byte CRY Y lookup within each
         * raster worker, just as R388 already does for wall columns. */
#if defined(_MSC_VER)
        static __declspec(thread) AvpWallShadeCache flat_shade_cache;
        const u8 *row_shades=avp_wall_shade_cache_get(&flat_shade_cache,floor_shade);
#elif defined(__GNUC__) || defined(__clang__)
        static __thread AvpWallShadeCache flat_shade_cache;
        const u8 *row_shades=avp_wall_shade_cache_get(&flat_shade_cache,floor_shade);
#else
        u8 row_shades_storage[256];
        avp_gpu_host_flat_row_shades(row_shades_storage,floor_shade);
        const u8 *row_shades=row_shades_storage;
#endif
        lefttan=(0.5-(double)fb->width*0.5)/p->focal+
            (p->state?p->state->host_stereo_projection_shift:0.0);step=1.0/p->focal;
        wx=p->px+(p->fx+p->rx*lefttan)*perp;
        wy=p->py+(p->fy+p->ry*lefttan)*perp;
        wxstep=p->rx*step*perp;wystep=p->ry*step*perp;
        /* Next-row depth depends on the output ROW, not column or texture.
         * R308 recalculated it for every single filtered floor pixel. */
        double row_depth_delta=0.0;
        if(p->state &&
           (p->state->host_filter_mask & (1u<<AVP_FILTER_GEOMETRY)) &&
           (p->state->host_filter_mode||p->state->host_aniso>1u)){
            double next_perp=perp;
            if(avp_host_flat_row_depth(retail_row+228.0/(double)fb->height,
                p->state->host_extend_draw_distance,&next_perp))
                row_depth_delta=next_perp-perp;
        }
        /* A bilinear-only pass never needs derivatives, and a mip/AF
         * footprint changes smoothly across a floor scanline. Cache its
         * plan for up to four adjacent screen pixels on one flat texture;
         * do not perform hypot/log2 for every rendered pixel. The sample UV
         * remains exact for each pixel. This is display-only filtering. */
        const unsigned filter_active=p->state &&
            (p->state->host_filter_mask & (1u<<AVP_FILTER_GEOMETRY)) &&
            (p->state->host_filter_mode||p->state->host_aniso>1u);
        if(!filter_active && p->state && p->state->host_game_micro_optimization){
            avp_micro_point_floor_row_impl(fb->pixels+(size_t)y*fb->width,fb->width,
                p->maze,p->mw,p->mh,(unsigned)ceiling,wx,wy,wxstep,wystep,
                p->flat_tex,p->flat_rotation,row_shades,k_cry_rgb,p->clear_color_holes);
            continue;
        }
        /* R315: the original nearest-row shading LUT already has precisely
         * the same saturating CRY-intensity mapping as the filtered LUT.
         * Reuse it instead of constructing a duplicate per scanline. */
        const uint8_t *shade_lut=row_shades;
        const unsigned filter_bilinear_only=filter_active &&
            p->state->host_filter_mode==AVP_TEXFILTER_BILINEAR &&
            p->state->host_aniso<=1u;
        AvpFilterPlan cached_plan={0u,1u,0u,0.0,0.0};
        const AvpGpuRgbImage *cached_texture=NULL;
        int cached_block=-1;
        u8 cached_flat=0xffu;
        /* No heap/cache mutation in workers. Bound filter data belongs to
         * this one scanline and is replaced only as its plan changes. */
        AvpFilterCryFloorBlock floor_block;
        const AvpGpuRgbImage *floor_bound_texture=NULL;
        unsigned floor_block_ready=0u;
        /* R389: avoid repeatedly indexing maze cells and flat bindings
         * while several adjacent output pixels occupy the same cell. */
        AvpFloorRowCellCache cell;
        avp_floor_row_cell_cache_init(&cell,p->maze,p->mw,(unsigned)ceiling);
        const AvpGpuRgbImage *t=NULL;
        const AvpFilterImage *flat_mip=NULL;
        unsigned flat_rotation=0u;
        for(x=0;x<fb->width;x++,wx+=wxstep,wy+=wystep){
            int cx,cy;u8 flat;
            if(!avp_gpu_floor_valid_cell(wx,wy,p->mw,p->mh,&cx,&cy))continue;
            if(avp_floor_row_cell_cache_update(&cell,cx,cy)){
                const u8 id=cell.flat;
                t=p->flat_tex[id];
                flat_mip=p->flat_mip[id];
                flat_rotation=p->flat_rotation[id];
            }
            flat=cell.flat;
            if(t){
                /* The cell indices above are precisely floor(wx/wy).
                 * Reuse those results to compute the same fractional
                 * coordinates, avoiding two more libm floor operations for
                 * every high-resolution flat pixel. */
                double ux=wx-(double)cx,uy=wy-(double)cy;
                /* The fraction within this validated cell is [0,1), so
                 * inbounds sample coordinates need no per-pixel modulus.
                 * Malformed zero-sized textures preserve the old fallback. */
                if(filter_active){
                    double u=ux,v=uy;
                    switch(t->width==t->height?(flat_rotation&3u):0u){
                    case 1u:u=uy;v=1.0-ux;break;
                    case 2u:u=1.0-ux;v=1.0-uy;break;
                    case 3u:u=1.0-uy;v=ux;break;
                    default:break;
                    }
                    AvpFilterPlan plan={0u,1u,1u,0.0,0.0};
                    unsigned new_plan=0u;
                    if(!filter_bilinear_only){
                        const int block=(int)(x>>2);
                        if(cached_texture!=t || cached_flat!=flat || cached_block!=block){
                            new_plan=1u;
                            /* Derivative footprints are sampled once per
                             * four pixels; original UVs are not quantized. */
                            double tan=lefttan+((double)x+1.5)*step;
                            double d_yx=(p->fx+p->rx*tan)*row_depth_delta;
                            double d_yy=(p->fy+p->ry*tan)*row_depth_delta;
                            AvpFilterDerivatives dd={wxstep,wystep,d_yx,d_yy};
                            switch(t->width==t->height?(flat_rotation&3u):0u){
                            case 1u:dd=(AvpFilterDerivatives){wystep,-wxstep,d_yy,-d_yx};break;
                            case 2u:dd=(AvpFilterDerivatives){-wxstep,-wystep,-d_yx,-d_yy};break;
                            case 3u:dd=(AvpFilterDerivatives){-wystep,wxstep,-d_yy,d_yx};break;
                            default:break;
                            }
                            cached_plan=avp_filter_plan(t,flat_mip,dd,
                                p->state->host_filter_mode,p->state->host_aniso);
                            cached_flat=flat;cached_texture=t;cached_block=block;
                        }
                        plan=cached_plan;
                    }
                    /* Bilinear-only has a constant plan for the full row;
                     * mip/AF mode changes at most once per 4-pixel block
                     * while the same flat texture remains bound. */
                    if(new_plan || floor_bound_texture!=t){
                        floor_block_ready=(unsigned)avp_filter_cry_floor_block_prepare(
                            &floor_block,t,flat_mip,plan,1,k_cry_rgb,shade_lut);
                        floor_bound_texture=t;
                    }
                    unsigned alpha=255u;
                    uint32_t c=floor_block_ready ?
                        avp_filter_cry_floor_block_sample(&floor_block,u,v) :
                        avp_filter_sample_prepared_shaded(t,flat_mip,u,v,plan,
                            1,floor_shade,k_cry_rgb,&alpha,shade_lut);
                    size_t off=(size_t)y*fb->width+x;
                    if(alpha==255u)fb->pixels[off]=c;
                    else if(alpha)fb->pixels[off]=avp_gpu_hires_alpha_blend(fb->pixels[off],c,alpha);
                }else{
                    /* R315: integer texel coordinates and Jaguar rotation
                     * are needed only by the unfiltered point sampler. The
                     * filtered path works in exact fractional UVs above. */
                    int tx=(int)(ux*t->width),ty=(int)(uy*t->height);
                    if(flat_rotation && t->width==t->height){
                        const int oldx=tx,oldy=ty;
                        switch(flat_rotation){
                        case 1u:tx=oldy;ty=t->height-1-oldx;break;
                        case 2u:tx=t->width-1-oldx;ty=t->height-1-oldy;break;
                        case 3u:tx=t->width-1-oldy;ty=oldx;break;
                        default:break;
                        }
                    }
                    fb->pixels[(size_t)y*fb->width+x]=
                        t->width>0 && t->height>0
                        ? avp_gpu_host_flat_sample_inbounds(t,(unsigned)tx,(unsigned)ty,
                                                            row_shades,k_cry_rgb)
                        : sample_tex_shaded(t,tx,ty,floor_shade);
                }
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
    AvpWorldScenePacket *capture; /* NULL for all production Software frames */
} AvpWallPass;
static void avp_wall_columns(void *arg,unsigned first,unsigned last)
{
    const AvpWallPass *p=(const AvpWallPass*)arg;
    unsigned x;
    for(x=first;x<last;x++){
        double tanoff=((double)x+0.5-(double)p->fb->width*0.5)/p->focal+
            (p->state?p->state->host_stereo_projection_shift:0.0);
        double dx=p->fx+p->rx*tanoff,dy=p->fy+p->ry*tanoff;
        PcRayHit portals[AVP_MAZE2_MAX_RAY_PORTALS];unsigned pc=0,k;
        PcRayHit hit=cast_ray_hit(p->state,p->logical_level,p->maze,
                                 p->mw,p->mh,p->px,p->py,dx,dy,
                                 portals,&pc,AVP_MAZE2_MAX_RAY_PORTALS);
        pc_portal_clip_column(p->state,p->logical_level,portals,pc,&hit,p->fb->height);
        const double depth=hit.hit?hit.d:avp_host_ray_limit(
            p->state ? p->state->host_extend_draw_distance : 0);
        if(p->zbuf)p->zbuf[x]=depth;
        if(p->capture){
            /* Write only this worker-owned column. Portal aperture clipping
             * comes from the SAME native-visible ray sequence as Software;
             * no second raycast, no completed framebuffer transfer. */
            for(k=0;k<pc;k++){
                const PcRayHit *q=&portals[k];
                (void)avp_world_scene_hit(p->capture,x,q->d,q->tc,q->wall,
                    q->open,q->vertical,0u,q->clip_lo,q->clip_hi);
            }
            if(hit.hit)
                (void)avp_world_scene_hit(p->capture,x,hit.d,hit.tc,hit.wall,
                    hit.open,hit.vertical,1u,hit.clip_lo,hit.clip_hi);
            avp_world_scene_end_column(p->capture,x,depth);
        }
        /* R334: ray-only preparation for Vulkan. When pixels is NULL, this
         * worker runs the identical portal/solid-wall raycaster and capture
         * callbacks WITHOUT rasterizing a single CPU framebuffer pixel. */
        if (!p->fb->pixels) continue;
#if defined(__clang__) && !defined(_WIN32)
        /* No portal or overlapping door/window segment may take the fresh
         * depth shortcut. Set flags here, after visibility/capture work. */
        hit.fresh_depth=0u;
        for(k=0;k<pc;k++)portals[k].fresh_depth=0u;
#endif
        if(p->occlusion){
#if defined(__clang__) && !defined(_WIN32)
            const unsigned wall_type=(hit.wall>>3)&15u;
            hit.fresh_depth=hit.hit && pc==0u &&
                (wall_type<=2u || wall_type==12u);
#endif
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

/* R312: The host raster dispatches wall COLUMNS concurrently. A cold
 * avp_filter_cache_lookup() allocates and builds mip levels; if two wall
 * workers encounter the same texture together, they can otherwise write the
 * same mip chain or destroy an in-progress build (a real data race).
 * Resolve every wall source ID represented by this immutable frame's maze
 * on the owning thread BEFORE launching any parallel raster workers. Do not
 * prebuild the whole 92-texture catalog: that would waste memory and startup
 * time on high-resolution packs containing assets this level never uses.
 * All visible wall codes come from maze cell wall bytes 0..3; door motion
 * alters open bytes 6..7 only. Source notices share the same 18-item cache.
 */
static void avp_filter_preheat_wall_mips(AvpAvpGpuState *state,const u8 *maze,
                                        u16 mw,u16 mh,unsigned logical_level)
{
    if(!state || !state->host_filter_cache || !maze || !mw || !mh ||
       logical_level<1u || logical_level>15u)return;
    if(!(state->host_filter_mask & (1u<<AVP_FILTER_GEOMETRY)) ||
       (state->host_filter_mode!=AVP_TEXFILTER_NEAREST_MIP &&
        state->host_aniso<=1u))return;
    u8 seen_wall[256]={0};
    u8 seen_image[74u+18u]={0};
    const size_t cells=(size_t)mw*(size_t)mh;
    for(size_t i=0;i<cells;i++){
        const u8 *c=maze+i*8u;
        seen_wall[c[0]]=seen_wall[c[1]]=seen_wall[c[2]]=seen_wall[c[3]]=1u;
    }
    seen_wall[1]=1u; /* Outer map edge is an implicit solid wall. */
    for(unsigned code=0;code<256u;code++){
        unsigned fid=0;
        const AvpGpuRgbImage *t=NULL;
        unsigned slot;
        if(!seen_wall[code] || !avp_gpu_wall_texture_file(logical_level,(u8)code,&fid) ||
           fid>=74u || !state->panel_tex[fid].rgb)continue;
        slot=fid;
        t=&state->panel_tex[fid];
        if(fid!=16u && state->host_world_tex && state->host_world_tex_enabled &&
           *state->host_world_tex_enabled && state->host_world_tex[fid].rgb)
            t=&state->host_world_tex[fid];
        if(fid==16u){
            unsigned ni=0;
            if(avp_gpu_wall_notice_index(logical_level,(u8)code,&ni) && ni<18u){
                slot=74u+ni;
                if(state->host_world_tex_enabled && *state->host_world_tex_enabled &&
                   state->host_notice_tex && state->host_notice_tex[ni].rgb)
                    t=&state->host_notice_tex[ni];
                else if(state->notice_tex[ni].rgb)t=&state->notice_tex[ni];
                else slot=16u; /* Keep original panel fallback identity. */
            }else if(state->host_world_tex_enabled && *state->host_world_tex_enabled &&
                     state->host_world_tex && state->host_world_tex[16u].rgb)
                t=&state->host_world_tex[16u];
        }
        if(!seen_image[slot]){
            /* Serialized build: all subsequent wall-worker lookups for this
             * source are read-only. Shared local-MP viewports reuse it. */
            (void)avp_filter_cache_lookup(state->host_filter_cache,t);
            seen_image[slot]=1u;
        }
    }
}

/* R383: opt-in Software-stage spike attribution. The Vulkan hybrid invokes
 * this same path, so its report also detects shared CPU-side stalls.
 * Timers are read only when AVP_SOFTWARE_TIMINGS=1; no per-frame printing
 * unless the full Software render exceeds 100ms. */
static double avp_render_clock_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency,ticks;
    if(!QueryPerformanceFrequency(&frequency)||!frequency.QuadPart||
       !QueryPerformanceCounter(&ticks))return 0.0;
    return (double)ticks.QuadPart*1000.0/(double)frequency.QuadPart;
#else
    struct timespec t;
    if(clock_gettime(CLOCK_MONOTONIC,&t)!=0)return 0.0;
    return (double)t.tv_sec*1000.0+(double)t.tv_nsec/1000000.0;
#endif
}

static void avp_gpu_render_maze_impl(AvpAvpGpuState*state,AvpJaguarFramebuffer*fb,const u8*maze,u16 mw,u16 mh,u32 xpos,u32 ypos,u32 angle,int rom_ok,unsigned logical_level,u64 vblank_counter,int smooth_presentation,int debug_immediate,AvpWorldScenePacket *capture
#ifdef AVP_VK_GAMEPLAY_WALL_PROBE_BUILD
    ,uint32_t *hybrid_world_snapshot,size_t hybrid_world_capacity
#endif
)
{
    const char *software_timing=getenv("AVP_SOFTWARE_TIMINGS");
    const int profile=software_timing && software_timing[0]=='1' &&
                      software_timing[1]=='\0';
    double stages[7]={0};
    if(profile)stages[0]=avp_render_clock_ms();
    u8 interp_maze[64u*64u*8u];size_t maze_bytes=(size_t)mw*(size_t)mh*8u;
    if(state)state->map_smooth_camera=smooth_presentation&&state->have_prev_sim;
    unsigned x,y;double px=(double)xpos/65536.0,py=(double)ypos/65536.0;
    if(capture)avp_world_scene_invalidate(capture);
    if(state&&state->computer_active){avp_gpu_computer_present(state,fb);return;}
    ensure_cry_rgb_table();
    if(state && state->host_filter_cache &&
       (state->host_filter_mode==AVP_TEXFILTER_NEAREST_MIP||state->host_aniso>1u) &&
       (state->host_filter_mask & (1u<<AVP_FILTER_GEOMETRY))){
        avp_filter_cache_prepare(state->host_filter_cache,
            state->host_world_tex,state->host_notice_tex,
            state->panel_tex,state->notice_tex,
            state->host_world_tex_enabled&&*state->host_world_tex_enabled,
            state->qconv,state->qconv_size);
    }
#if defined(__clang__) && !defined(_WIN32) /* R422: measured Clang-only fast path */
    if(smooth_presentation&&state->have_prev_sim&&maze&&maze_bytes<=sizeof(interp_maze)&&state->prev_maze_size==maze_bytes){if(state->host_game_micro_optimization && isfinite(state->render_alpha))avp_micro_maze_interp(interp_maze,maze,state->prev_maze,maze_bytes,state->render_alpha);else {memcpy(interp_maze,maze,maze_bytes);for(size_t ci=0;ci<(size_t)mw*(size_t)mh;ci++){size_t o=ci*8u;for(unsigned k=6;k<=7;k++){double q=(double)state->prev_maze[o+k]+((double)maze[o+k]-(double)state->prev_maze[o+k])*state->render_alpha;if(q<0)q=0;if(q>255)q=255;interp_maze[o+k]=(u8)(q+0.5);}}}maze=interp_maze;}
#else
    if(smooth_presentation&&state->have_prev_sim&&maze&&maze_bytes<=sizeof(interp_maze)&&state->prev_maze_size==maze_bytes){memcpy(interp_maze,maze,maze_bytes);for(size_t ci=0;ci<(size_t)mw*(size_t)mh;ci++){size_t o=ci*8u;for(unsigned k=6;k<=7;k++){double q=(double)state->prev_maze[o+k]+((double)maze[o+k]-(double)state->prev_maze[o+k])*state->render_alpha;if(q<0)q=0;if(q>255)q=255;interp_maze[o+k]=(u8)(q+0.5);}}maze=interp_maze;}
#endif
    avp_filter_preheat_wall_mips(state,maze,mw,mh,logical_level);
    if(profile)stages[1]=avp_render_clock_ms();
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
    if(capture && (!avp_world_scene_begin(capture,state,maze,mw,mh,
        logical_level,px,py,fx,fy,rx,ry,focal) ||
        capture->width!=fb->width || capture->height!=fb->height)){
        avp_world_scene_invalidate(capture);
        capture=NULL; /* never leak a stale/partial world packet */
    }
    /* RE39 R164: preserve the screenshot-verified RGB16 selectors for Thermal
     * and Electromagnetic; optionally enhance the separate retail Infrared
     * pair {RGB16,8880}. RGB24 Night Vision and all unselected modes remain
     * untouched, including original rendering when every toggle is off. */
    u32 *thermal_pixels=NULL;
    u32 *background_mask=NULL;
    const unsigned background_style=state ? (unsigned)avp_predator_background_style(
        state->host_enhanced_predator_background,player_type,invis_stat,
        pred_vision_vmode,xtra_c) : 0u;
    const unsigned heat_style=state ? (unsigned)avp_predator_vision_style_ex(
        state->host_enhanced_predator_thermal,state->host_enhanced_predator_electromagnetic,
        state->host_enhanced_predator_infrared,
        player_type,invis_stat,pred_vision_vmode,xtra_c) : 0u;
    const size_t frame_pixels=(size_t)fb->width*(size_t)fb->height;
    if(state && !state->map_editor_world_only && frame_pixels &&
       frame_pixels<=SIZE_MAX/sizeof(u32) &&
       heat_style){
        if(state->host_thermal_capacity<frame_pixels){
            u32 *resized=(u32*)realloc(state->host_thermal_pixels,
                                       frame_pixels*sizeof(u32));
            if(resized){
                state->host_thermal_pixels=resized;
                state->host_thermal_capacity=frame_pixels;
            }
        }
        if(state->host_thermal_capacity>=frame_pixels){
            thermal_pixels=state->host_thermal_pixels;
            memset(thermal_pixels,0,frame_pixels*sizeof(u32));
        }
    }
    if(state && !state->map_editor_world_only && frame_pixels &&
       frame_pixels<=SIZE_MAX/sizeof(u32) && background_style){
        if(state->host_predator_background_capacity<frame_pixels){
            u32 *resized=(u32*)realloc(state->host_predator_background_mask,
                                       frame_pixels*sizeof(u32));
            if(resized){
                state->host_predator_background_mask=resized;
                state->host_predator_background_capacity=frame_pixels;
            }
        }
        if(state->host_predator_background_capacity>=frame_pixels)
            background_mask=state->host_predator_background_mask;
    }
    /* rgb(0,0,0) is the all-zero XRGB word: one memset instead of a
     * per-pixel loop over the (possibly window-sized) frame. */
    const int textured_floor=state->textures_loaded>0 && logical_level>=1u && logical_level<=15u;
    const int filtered_floor=(state->host_filter_mask & (1u<<AVP_FILTER_GEOMETRY)) &&
        (state->host_filter_mode || state->host_aniso>1u);
    const int defer_color_clear=avp_micro_floor_defer_color_clear(fb->width,fb->height,
        state->host_game_micro_optimization,textured_floor,filtered_floor);
    if(!defer_color_clear)
        memset(fb->pixels,0,(size_t)fb->width*(size_t)fb->height*sizeof(*fb->pixels));

    /* Retail-style floor/ceiling projection.  The presentation surface keeps
       the original 320:228 ratio; host resolution only increases sampling. */
    /* Optional: initialize depth in the existing synchronous floor job,
     * avoiding a serial full-resolution clear or a second worker wakeup. */
    AvpMaze2Occlusion *prepared_depth=NULL;
    if(textured_floor){
        AvpFloorPass pass;
        pass.clear_color_holes=defer_color_clear;
        if(state->host_game_micro_optimization && fb->width>=1280u && fb->height>=720u &&
            avp_gpu_maze2_occlusion_ensure(&state->maze_occlusion,fb->width,fb->height))
            prepared_depth=&state->maze_occlusion;
        pass.clear_depth=prepared_depth;
        pass.state=state;pass.fb=fb;pass.maze=maze;pass.mw=mw;pass.mh=mh;
        pass.px=px;pass.py=py;pass.fx=fx;pass.fy=fy;pass.rx=rx;pass.ry=ry;pass.focal=focal;
        /* R386: only maze flat IDs used on this level may cause mip builds.
         * The original renderer allocated mips for all 256 flat codes,
         * even those absent from the loaded maze. Both ceiling and floor
         * bytes must be included, including tiles outside the current view. */
        unsigned char used_flats[256];
        avp_gpu_collect_used_flats(maze,mw,mh,used_flats);
        /* Explicit NULL for unused entries; workers never see an
         * uninitialized binding. The point/filtered sampling code is
         * untouched and sees the same binding for every reachable cell. */
        memset(pass.flat_tex,0,sizeof(pass.flat_tex));
        memset(pass.flat_mip,0,sizeof(pass.flat_mip));
        memset(pass.flat_rotation,0,sizeof(pass.flat_rotation));
        for(unsigned f=0;f<256u;f++){
            if(!used_flats[f])continue;
            unsigned id=0;
            /* The panel image set is immutable during the raster workers.
             * A missing/unloaded flat remains transparent to this pass. */
            pass.flat_tex[f]=avp_gpu_flat_texture_file(logical_level,(u8)f,&id)&&
                             id<74u&&state->panel_tex[id].rgb
                             ?(state->host_world_tex && state->host_world_tex_enabled &&
                               *state->host_world_tex_enabled && state->host_world_tex[id].rgb
                               ?&state->host_world_tex[id]:&state->panel_tex[id]):NULL;
            pass.flat_rotation[f]=avp_gpu_flat_texture_rotation(logical_level,(u8)f);
            pass.flat_mip[f]=(state->host_filter_mode==AVP_TEXFILTER_NEAREST_MIP ||
                              state->host_aniso>1u) ?
                avp_filter_cache_lookup(state->host_filter_cache,pass.flat_tex[f]):NULL;
        }
#if defined(__linux__) || (!defined(_WIN32) && defined(__GNUC__) && !defined(__clang__))
        /* R435: combine oblique scaled sampling with dynamic floor bands.
         * Smaller bands on Linux GCC reduce the remaining tail imbalance;
         * Clang uses the earlier 32-row scheduling. */
#if defined(__linux__) && defined(__GNUC__) && !defined(__clang__)
        const unsigned floor_band=16u;
#else
        const unsigned floor_band=32u;
#endif
        if(state->host_game_micro_optimization && fb->height>=720u && fb->width>=1280u){
            AvpRasterRowQueue queue={avp_floor_rows,&pass,fb->height,floor_band,ATOMIC_VAR_INIT(0u)};
            avp_raster_spans(avp_raster_row_queue_run,&queue,fb->height,fb->width*fb->height);
        }else
#endif
        avp_raster_spans(avp_floor_rows,&pass,fb->height,fb->width*fb->height);
    }else{
        for(y=0;y<fb->height;y++){uint32_t c=y<fb->height/2?rgb(20,22,24):rgb(38,36,32);for(x=0;x<fb->width;x++)fb->pixels[(size_t)y*fb->width+x]=c;}
    }

    if(profile)stages[2]=avp_render_clock_ms();
    /* Source-backed wall/door presentation.  Door state bytes are mutated by
       translated MoveDoors at 10 Hz; this renderer only visualizes them. */
    {double *zbuf=NULL;AvpMaze2Occlusion *occlusion=NULL;
    /* Same logical depth buffer as before; grow only on window resize. */
    if(state->host_wall_depth_capacity<fb->width){
        double *newbuf=(double*)realloc(state->host_wall_depth,
                                       (size_t)fb->width*sizeof(double));
        if(newbuf){state->host_wall_depth=newbuf;
                   state->host_wall_depth_capacity=fb->width;}
    }
    if(state->host_wall_depth_capacity>=fb->width)
        zbuf=state->host_wall_depth;
    if(!zbuf)return;
    if(prepared_depth){occlusion=prepared_depth;}
    else if(avp_gpu_maze2_occlusion_ensure(&state->maze_occlusion,fb->width,fb->height)){
        avp_gpu_maze2_occlusion_clear(&state->maze_occlusion);
        occlusion=&state->maze_occlusion;
    }
    {
        AvpWallPass pass;
        pass.state=state;pass.fb=fb;pass.maze=maze;pass.mw=mw;pass.mh=mh;
        pass.px=px;pass.py=py;pass.fx=fx;pass.fy=fy;pass.rx=rx;pass.ry=ry;
        pass.focal=focal;pass.logical_level=logical_level;
        pass.zbuf=zbuf;pass.occlusion=occlusion;
        pass.capture=capture;
        if(capture)capture->occlusion_enabled=(unsigned)(occlusion!=NULL);
#ifndef _WIN32
        /* Disjoint 64-column bands limit cache-line boundary sharing while
         * balancing ray lengths and wall heights. Capture-only and hybrid
         * scene-packet paths retain their original fixed partition. */
        if(state->host_game_micro_optimization && !capture &&
           fb->width>=1280u && fb->height>=720u){
            AvpRasterRowQueue queue={avp_wall_columns,&pass,fb->width,64u,ATOMIC_VAR_INIT(0u)};
            avp_raster_spans(avp_raster_row_queue_run,&queue,fb->width,fb->width*fb->height);
        }else
#endif
        avp_raster_spans(avp_wall_columns,&pass,fb->width,fb->width*fb->height);
    }
    if(profile)stages[3]=avp_render_clock_ms();
#ifdef AVP_VK_GAMEPLAY_WALL_PROBE_BUILD
    /* R360: same original CPU world pass, BEFORE any actors/effects/HUD.
     * Opt-in only: the normal Software entry point provides no destination.
     * The GPU uses its own scene source tape, NOT these world RGB pixels. */
    if(hybrid_world_snapshot && frame_pixels &&
       hybrid_world_capacity>=frame_pixels)
        memcpy(hybrid_world_snapshot,fb->pixels,frame_pixels*sizeof(uint32_t));
#endif
    /* Capture the world before any living/nonliving sprites or HUD. The
     * scratch becomes a foreground protection mask immediately after the
     * first-person overlay; no extra CLUT/world rendering pass is needed. */
    if(background_mask)
        memcpy(background_mask,fb->pixels,frame_pixels*sizeof(u32));
    if(!state->map_editor_world_only)
        draw_amp_layer(state,fb,px,py,fx,fy,rx,ry,focal,zbuf,occlusion,smooth_presentation,thermal_pixels,heat_style);
    /* zbuf belongs to the viewport and is reused on subsequent frames. */
    }

    if(state->map_editor_world_only)return; /* The same wall/flat raster, without active game entities or HUD. */

    /* Widescreen: a first-person sprite may protrude beyond the original
     * centered 320 columns.  Paint it over the whole host world at the
     * identical Jaguar-native texel scale; HUD remains 4:3 below. */
    if(host_wide || fp_has_active_replacement(state))
        overlay_first_person_wide(state,fb,vblank_counter,smooth_presentation);
    else
        overlay_first_person_scaled(state,fb,vblank_counter,smooth_presentation);
    /* Flag all original-pixel changes from the actor/weapon passes BEFORE
     * the Jaguar CLUT; the HUD pass separately marks its modified pixels. */
    if(background_mask)
        for(size_t k=0;k<frame_pixels;k++)
            background_mask[k]=(background_mask[k]!=fb->pixels[k])?1u:0u;
    /* Original first-person weapons paint over the scene. Discard heat
     * records covered by those pixels BEFORE the retail full-frame CLUT;
     * otherwise a sprite behind the weapon could shine through it. */
    if(thermal_pixels)
        avp_thermal_remove_covered(thermal_pixels,fb->pixels,frame_pixels);
    apply_game_clut(state,fb,smooth_presentation);
    if(profile)stages[4]=avp_render_clock_ms();
    /* False-color compositing comes after the original blue scene filter,
     * but before the HUD, so only living character pixels become hot and
     * original HUD colors remain unchanged. The native TOM VMODE mapping
     * still processes the final image exactly as it did before. */
    /* Keep the heat mask, but composite its false colors AFTER TOM VMODE.
     * Converting already-red RGB pixels back to CRY and through RGB16
     * would modify their red/yellow appearance. Protect the HUD below. */
    if(thermal_pixels){
        for(size_t k=0;k<frame_pixels;k++)if(thermal_pixels[k])
            thermal_pixels[k]=(thermal_pixels[k]&0xff000000u)|
                              (fb->pixels[k]&0x00ffffffu);
    }
    /* Presentation-only stereo camera rotation and offsets must not shift
     * the scanner/automap: those belong to the authoritative player camera.
     * Neither side can influence gameplay or exploration state. */
    double hud_px=px,hud_py=py,hud_fx=fx,hud_fy=fy,hud_rx=rx,hud_ry=ry;
    if(state && state->host_vr_hud_anchor_enabled){
        double ha=avp_gpu_heading_radians(state->host_vr_hud_anchor_angle,
                                         state->host_continuous_heading);
        hud_px=(double)state->host_vr_hud_anchor_x/65536.0;
        hud_py=(double)state->host_vr_hud_anchor_y/65536.0;
        hud_fx=cos(ha);hud_fy=sin(ha);hud_rx=-hud_fy;hud_ry=hud_fx;
    }
    /* The retail HUD itself stays centered and 4:3 on a wide host view.
     * Allocate this crop only for the HUD, AFTER the full-width weapon pass. */
    AvpJaguarFramebuffer op_view={0};
    unsigned op_left=0;
    int direct_hud=0;
    if(host_wide){
        unsigned center_w=avp_host_wide_center_width(fb->height);
        if(center_w && center_w<fb->width &&
           (size_t)center_w<=SIZE_MAX/sizeof(u32)/fb->height){
            op_view.width=center_w;op_view.height=fb->height;
            op_left=(fb->width-center_w)/2u;
            direct_hud=avp_micro_direct_wide_hud(state,fb,center_w,op_left,maze,mw,mh,
                hud_px,hud_py,hud_fx,hud_fy,hud_rx,hud_ry,logical_level,background_mask);
            const size_t count=(size_t)center_w*fb->height;
            if(!direct_hud && state->host_hud_crop_capacity<count){
                u32 *newbuf=(u32*)realloc(state->host_hud_crop_pixels,
                                          count*sizeof(u32));
                if(newbuf){state->host_hud_crop_pixels=newbuf;
                           state->host_hud_crop_capacity=count;}
            }
            if(!direct_hud && state->host_hud_crop_capacity>=count)
                op_view.pixels=state->host_hud_crop_pixels;
        }
    }
    if(direct_hud){ /* HUD already composited in place. */ }
    else if(op_view.pixels){
        const int parallel_crop=state->host_game_micro_optimization &&
            (uint64_t)op_view.width*op_view.height>=1920u*1080u;
        if(parallel_crop){
            AvpMicroStridedCopy pass={fb->pixels+op_left,op_view.pixels,
                op_view.width,fb->width,op_view.width};
            avp_raster_spans(avp_micro_strided_copy_rows,&pass,op_view.height,
                             op_view.width*op_view.height);
        }else for(unsigned row=0;row<fb->height;row++)
            memcpy(op_view.pixels+(size_t)row*op_view.width,
                   fb->pixels+(size_t)row*fb->width+op_left,
                   (size_t)op_view.width*sizeof(u32));
        overlay_hud(state,&op_view,maze,mw,mh,hud_px,hud_py,hud_fx,hud_fy,hud_rx,hud_ry,
                    logical_level,background_mask?background_mask+op_left:NULL,fb->width);
        if(parallel_crop){
            AvpMicroStridedCopy pass={op_view.pixels,fb->pixels+op_left,
                op_view.width,op_view.width,fb->width};
            avp_raster_spans(avp_micro_strided_copy_rows,&pass,op_view.height,
                             op_view.width*op_view.height);
        }else for(unsigned row=0;row<fb->height;row++)
            memcpy(fb->pixels+(size_t)row*fb->width+op_left,
                   op_view.pixels+(size_t)row*op_view.width,
                   (size_t)op_view.width*sizeof(u32));
        /* Viewport owns the crop scratch; no per-frame heap free. */
    }else overlay_hud(state,fb,maze,mw,mh,hud_px,hud_py,hud_fx,hud_fy,hud_rx,hud_ry,
                      logical_level,background_mask,fb->width);
    if(profile)stages[5]=avp_render_clock_ms();
    /* Original HUD draws before VMODE. Remove heat records that the HUD
     * covered, then map surviving heat to the native 320-word output. */
    if(thermal_pixels)avp_thermal_remove_covered(thermal_pixels,fb->pixels,frame_pixels);
    present_predator_vmode(state,fb);
    /* Native RGB16 video line-buffer output has finished. Recolor ONLY
     * scenery samples, leaving HUD, weapon, pickups and all sprites protected.
     * The independent living-character compositor remains last and unchanged. */
    if(background_mask){
        if(background_style==AVP_VISION_ENHANCED_INFRARED){
            /* RE39 R175: wall depth is already cached by the normal renderer;
             * untouched flat pixels use their projected row depth. Never infer
             * geometry proximity from texture brightness again. */
            const AvpMaze2Occlusion *o=state?&state->maze_occlusion:NULL;
            const float *wall_depth=o&&o->depth&&o->width==fb->width&&
                 o->height==fb->height?o->depth:NULL;
            avp_predator_infrared_background_composite_depth_rgb16(
                fb->pixels,background_mask,wall_depth,fb->width,fb->height,
                state?state->host_extend_draw_distance:0);
        }else{
            avp_predator_background_composite_rgb16(fb->pixels,background_mask,
                fb->width,fb->height,background_style);
        }
    }
    if(thermal_pixels)
        avp_thermal_composite_vmode(thermal_pixels,fb->pixels,fb->width,fb->height,
                                    (unsigned)(pred_vision_vmode&6u),heat_style);
    /* The translated MAZESCRN fade is owned by the 68000, but the PC
     * renderer composites the HUD after the 3D CLUT pass.  At the completed
     * fade-to-black endpoint the final video signal must be black, including
     * the separately composited HUD.  Do not alter the original fade timer,
     * rate or intermediate Jaguar color-modifier behavior. */
    if(fade_level>=0x80 && fb->pixels)
        memset(fb->pixels,0,(size_t)fb->width*(size_t)fb->height*sizeof(*fb->pixels));
    if(profile){
        stages[6]=avp_render_clock_ms();
        if(stages[6]-stages[0]>=100.0){
            fprintf(stderr,"[Software frame spike] %ux%u total=%.1fms "
                "preheat=%.1f flats=%.1f walls=%.1f "
                "actors+CLUT=%.1f HUD=%.1f post=%.1f\n",
                fb->width,fb->height,stages[6]-stages[0],
                stages[1]-stages[0],stages[2]-stages[1],
                stages[3]-stages[2],stages[4]-stages[3],
                stages[5]-stages[4],stages[6]-stages[5]);
        }
    }
    /* Developer diagnostics are never drawn during the normal retail path. */
    if(debug_immediate){int ox=8,oy=8,cy,cx;int maxw=(int)(fb->width/3u);int maxh=(int)(fb->height/3u);int msx=mw?maxw/(int)mw:1,msy=mh?maxh/(int)mh:1;int ms=msx<msy?msx:msy;if(ms<1)ms=1;for(cy=0;cy<(int)mh;cy++)for(cx=0;cx<(int)mw;cx++){const u8*c=maze+((size_t)cy*mw+(size_t)cx)*8u;int xx=ox+cx*ms,yy=oy+cy*ms;if(c[0])line(fb,xx,yy,xx,yy+ms,rgb(100,100,100));if(c[1])line(fb,xx,yy,xx+ms,yy,rgb(100,100,100));if(c[2])line(fb,xx+ms,yy,xx+ms,yy+ms,rgb(100,100,100));if(c[3])line(fb,xx,yy+ms,xx+ms,yy+ms,rgb(100,100,100));}put(fb,ox+(int)px*ms,oy+(int)py*ms,rom_ok?rgb(80,255,100):rgb(255,190,60));}
}


/* R334 Vulkan preparation: cast only CPU-authored visibility rays, not the
 * Software framebuffer.  Unlike avp_gpu_render_maze_capture(), this function
 * does NOT run floor/ceiling, walls, actors, HUD, CLUT or presentation.
 * It is a source-geometry producer, NOT an active Vulkan renderer.
 * It intentionally uses the same original cast_ray_hit and portal-clipping
 * code that the normal Software wall worker uses. */
int avp_gpu_trace_world_rays(AvpAvpGpuState *state,
    const u8 *maze,u16 mw,u16 mh,u32 xpos,u32 ypos,u32 angle,
    unsigned logical_level,int smooth_presentation,
    uint32_t width,uint32_t height,AvpWorldScenePacket *packet)
{
    AvpJaguarFramebuffer virtual_fb={0};
    AvpWallPass pass={0};
    u8 interp_maze[64u*64u*8u];
    const size_t maze_bytes=(size_t)mw*(size_t)mh*8u;
    const double px=(double)xpos/65536.0,py=(double)ypos/65536.0;
    double a,fx,fy,rx,ry,focal;
    int wide;
    if (!packet)return 0;
    avp_world_scene_invalidate(packet);
    if (!state || !maze || !mw || !mh || mw>64u || mh>64u ||
        !width || !height || width>AVP_WORLD_SCENE_MAX_WIDTH ||
        height>AVP_WORLD_SCENE_MAX_HEIGHT ||
        packet->width!=width || packet->height!=height ||
        logical_level<1u || logical_level>15u || state->computer_active)
        return 0;
    if(smooth_presentation && state->have_prev_sim &&
       maze_bytes<=sizeof(interp_maze) && state->prev_maze_size==maze_bytes){
#if defined(__clang__) && !defined(_WIN32) /* R422: measured Clang-only fast path */
        if(state->host_game_micro_optimization && isfinite(state->render_alpha))
            avp_micro_maze_interp(interp_maze,maze,state->prev_maze,maze_bytes,state->render_alpha);
        else {
#endif
        memcpy(interp_maze,maze,maze_bytes);
        for(size_t ci=0;ci<(size_t)mw*mh;ci++) {
            const size_t o=ci*8u;
            for(unsigned k=6u;k<=7u;k++) {
                double q=(double)state->prev_maze[o+k]+
                    ((double)maze[o+k]-(double)state->prev_maze[o+k])*state->render_alpha;
                if(q<0.0)q=0.0;
                if(q>255.0)q=255.0;
                interp_maze[o+k]=(u8)(q+0.5);
            }
#if defined(__clang__) && !defined(_WIN32) /* R422: measured Clang-only fast path */
        }
#endif
        }
        maze=interp_maze;
    }
    a=avp_gpu_heading_radians(angle,state->host_continuous_heading);
    fx=cos(a);fy=sin(a);rx=-fy;ry=fx;
    wide=state->host_widescreen_fov && avp_host_has_extra_columns(width,height);
    focal=wide?avp_host_wide_focal(height):(double)width*0.5;
    if (!avp_world_scene_begin(packet,state,maze,mw,mh,logical_level,
        px,py,fx,fy,rx,ry,focal))return 0;
    /* Correct portal-clipped ray endpoints need only dimensions. No pixels,
     * occlusion surface, texture filtering or framebuffer allocation. */
    virtual_fb.width=width;virtual_fb.height=height;
    pass.state=state;pass.fb=&virtual_fb;pass.maze=maze;
    pass.mw=mw;pass.mh=mh;pass.px=px;pass.py=py;
    pass.fx=fx;pass.fy=fy;pass.rx=rx;pass.ry=ry;pass.focal=focal;
    pass.logical_level=logical_level;pass.zbuf=NULL;
    pass.occlusion=NULL;pass.capture=packet;
    packet->occlusion_enabled=0u; /* No CPU occlusion raster is constructed. */
    avp_raster_spans(avp_wall_columns,&pass,width,width*height);
    if(!avp_world_scene_complete(packet)){
        avp_world_scene_invalidate(packet);return 0;
    }
    return 1;
}

int avp_gpu_trace_world_wire(AvpAvpGpuState *state,
    const u8 *maze,u16 mw,u16 mh,u32 xpos,u32 ypos,u32 angle,
    unsigned logical_level,int smooth_presentation,
    uint32_t width,uint32_t height,AvpWorldScenePacket *packet,
    AvpWorldGpuWire *wire)
{
    if (!wire)return 0;
    avp_world_gpu_wire_invalidate(wire);
    if (!avp_gpu_trace_world_rays(state,maze,mw,mh,xpos,ypos,angle,
            logical_level,smooth_presentation,width,height,packet))return 0;
    if (!avp_world_gpu_wire_pack(wire,packet)){
        avp_world_scene_invalidate(packet);
        return 0;
    }
    return 1;
}

/* The public Software entry point remains unchanged, with NO allocated
 * capture or GPU work. An opt-in caller can request the same scene ray data
 * alongside software output for a future pixel-parity harness. */
void avp_gpu_render_maze(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
    const u8 *maze,u16 mw,u16 mh,u32 xpos,u32 ypos,u32 angle,
    int rom_ok,unsigned logical_level,u64 vblank_counter,
    int smooth_presentation,int debug_immediate)
{
    avp_gpu_render_maze_impl(state,fb,maze,mw,mh,xpos,ypos,angle,
        rom_ok,logical_level,vblank_counter,smooth_presentation,debug_immediate,NULL
#ifdef AVP_VK_GAMEPLAY_WALL_PROBE_BUILD
        ,NULL,0u
#endif
        );
}

int avp_gpu_render_maze_capture(AvpAvpGpuState *state,AvpJaguarFramebuffer *fb,
    const u8 *maze,u16 mw,u16 mh,u32 xpos,u32 ypos,u32 angle,
    int rom_ok,unsigned logical_level,u64 vblank_counter,
    int smooth_presentation,int debug_immediate,AvpWorldScenePacket *packet)
{
    if(!packet || !state || !fb)return 0;
    avp_gpu_render_maze_impl(state,fb,maze,mw,mh,xpos,ypos,angle,
        rom_ok,logical_level,vblank_counter,smooth_presentation,debug_immediate,packet
#ifdef AVP_VK_GAMEPLAY_WALL_PROBE_BUILD
        ,NULL,0u
#endif
        );
    return avp_world_scene_complete(packet);
}
#ifdef AVP_VK_GAMEPLAY_WALL_PROBE_BUILD
/* Two exact captures from ONE Software render: the original ray packet and
 * the world XRGB pixels at the actor/weapon boundary. The latter is solely an
 * overlay-difference reference: the Vulkan world NEVER samples this image. */
int avp_gpu_render_maze_capture_hybrid(AvpAvpGpuState *state,
    AvpJaguarFramebuffer *fb,const u8 *maze,u16 mw,u16 mh,
    u32 xpos,u32 ypos,u32 angle,int rom_ok,unsigned logical_level,
    u64 vblank_counter,int smooth_presentation,int debug_immediate,
    AvpWorldScenePacket *packet,uint32_t *world,size_t world_pixels)
{
    if(!packet||!state||!fb||!world||!fb->width||!fb->height||
       fb->height>SIZE_MAX/sizeof(uint32_t)/fb->width||
       world_pixels<(size_t)fb->width*fb->height)return 0;
    avp_gpu_render_maze_impl(state,fb,maze,mw,mh,xpos,ypos,angle,
        rom_ok,logical_level,vblank_counter,smooth_presentation,
        debug_immediate,packet,world,world_pixels);
    return avp_world_scene_complete(packet);
}
#endif
