#ifndef AVP_NEW_MAP_MAKER_HIGHLIGHT_H
#define AVP_NEW_MAP_MAKER_HIGHLIGHT_H

#include "map_maker.h"
#include <stdint.h>

/* Draw a thin outline around the currently picked, projected wall/floor/
 * ceiling tile in the detached Map Maker framebuffer. This is an editor-only
 * overlay: no authored cell, ROM image or Jaguar rasterizer state is changed.
 * Returns nonzero when one or more visible outline pixels were drawn. */
int avp_mm_draw_3d_highlight(const AvpMapMaker *m,uint32_t *pixels,
                             unsigned width,unsigned height);
/* 2D cell-selection footprint carried into 3D as a cyan wireframe. */
int avp_mm_draw_3d_cell_selection(const AvpMapMaker *m,uint32_t *pixels,
                                  unsigned width,unsigned height);

/* Editor-only 3D markers for staged AMP objects; clicking their marker
 * selects the object, independent from face/cell selection. */
int avp_mm_3d_object_at(const AvpMapMaker *m,unsigned width,unsigned height,
                        int px,int py);
int avp_mm_draw_3d_objects(const AvpMapMaker *m,uint32_t *pixels,
                           unsigned width,unsigned height);

#endif
