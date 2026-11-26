#ifndef AVP_GPU_FLOOR_ROW_CELL_CACHE_H
#define AVP_GPU_FLOOR_ROW_CELL_CACHE_H

#include <stdint.h>
#include <stddef.h>

/* R389: per-output-row lookup. A projected floor ray stays in the same
 * maze cell for many adjacent host-resolution pixels. Keep the exact
 * ceiling/floor source byte for that cell until cx or cy changes. No ray
 * coordinates, rounding, interpolation or map contents are altered. */
typedef struct AvpFloorRowCellCache {
    const uint8_t *maze;
    unsigned width;
    unsigned ceiling;
    int cx,cy;
    uint8_t flat;
    unsigned valid;
} AvpFloorRowCellCache;

static inline void avp_floor_row_cell_cache_init(AvpFloorRowCellCache *c,
    const uint8_t *maze,unsigned width,unsigned ceiling)
{
    c->maze=maze;c->width=width;c->ceiling=ceiling?1u:0u;
    c->cx=0;c->cy=0;c->flat=0;c->valid=0u;
}

/* Returns 1 if a cell changed, so the caller can update its texture, mip,
 * and rotation pointers once rather than at every output pixel. Input cells
 * are already validated by avp_gpu_floor_valid_cell. */
static inline unsigned avp_floor_row_cell_cache_update(AvpFloorRowCellCache *c,
    int cx,int cy)
{
    if(c->valid && c->cx==cx && c->cy==cy)return 0u;
    c->cx=cx;c->cy=cy;c->valid=1u;
    c->flat=c->maze[((size_t)cy*c->width+(size_t)cx)*8u+(c->ceiling?4u:5u)];
    return 1u;
}
#endif
