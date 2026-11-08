#ifndef AVP_NEW_MAP_MAKER_OVERHEAD_H
#define AVP_NEW_MAP_MAKER_OVERHEAD_H
#include "map_maker.h"
#include <stdint.h>
/* Host-only top-down editor image; no live Jaguar renderer or ROM mutation. */
typedef struct AvpMmOverheadLayout {
    int x0,y0;
    unsigned cell_px;
    int clip_x0,clip_y0,clip_x1,clip_y1; /* editable canvas, excluding HUD */
} AvpMmOverheadLayout;
typedef struct AvpMmOverheadTexture {
    const uint8_t *rgb; /* original decoded Jaguar panel RGB, row-major */
    unsigned width,height,rotation;
} AvpMmOverheadTexture;
typedef int (*AvpMmOverheadLookup)(void *context,unsigned level,unsigned flat,
                                    AvpMmOverheadTexture *texture);
typedef struct AvpMmOverheadSprite {
    /* Original Jaguar AAF frame-zero pixels, with alpha=0 for empty texels.
     * The pointer is borrowed for the duration of the callback only. */
    const uint32_t *rgba;
    unsigned width,height;
    unsigned x0,y0,x1,y1; /* nontransparent bounds, exclusive end */
} AvpMmOverheadSprite;
typedef int (*AvpMmOverheadSpriteLookup)(void *context,unsigned amp_definition,
                                         AvpMmOverheadSprite *sprite);
/* Mouse-centered wheel zoom and frame-rate-independent visual easing. The
 * anchor, zoom steps and animation are UI-only; no save-file changes. */
void avp_mm_overhead_zoom_at(AvpMapMaker *m,unsigned width,unsigned height,
                             int px,int py,int direction);
void avp_mm_overhead_zoom_tick(AvpMapMaker *m,double delta_seconds);
/* Middle-button dragging shifts the map image only; never selects cells. */
void avp_mm_overhead_pan(AvpMapMaker *m,unsigned width,unsigned height,
                         int dx,int dy);
int avp_mm_overhead_layout(const AvpMapMaker *m,unsigned width,unsigned height,
                           AvpMmOverheadLayout *layout);
int avp_mm_overhead_hit(const AvpMapMaker *m,unsigned width,unsigned height,
                        int px,int py,unsigned *cell_x,unsigned *cell_y);
/* During a Shift+left-drag range selection, keep selecting when the pointer
 * reaches a viewport edge at close zoom. This is selection-only: no writes,
 * undo history, or changes to the Jaguar runtime. Returns 1 only when the
 * canvas actually pans and its new target cell is valid. */
int avp_mm_overhead_selection_auto_pan(AvpMapMaker *m,unsigned width,unsigned height,
                                       int px,int py,int step,
                                       unsigned *cell_x,unsigned *cell_y);
/* Detect a panel edge near a mouse click. Returns 0..3 (L,T,R,B),
 * or 4 when the click is in the cell interior. */
unsigned avp_mm_overhead_edge(const AvpMapMaker *m,unsigned width,unsigned height,
                              int px,int py);
void avp_mm_render_overhead_with_sprites(const AvpMapMaker *m,uint32_t *pixels,
                            unsigned width,unsigned height,
                            AvpMmOverheadLookup lookup,void *context,
                            AvpMmOverheadSpriteLookup sprite_lookup);
/* Existing callers retain the original marker-only behavior. */
void avp_mm_render_overhead(const AvpMapMaker *m,uint32_t *pixels,
                            unsigned width,unsigned height,
                            AvpMmOverheadLookup lookup,void *context);
#endif
