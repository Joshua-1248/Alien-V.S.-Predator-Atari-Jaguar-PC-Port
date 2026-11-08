#ifndef AVP_NEW_LOCAL_MP_RETAIL_MAPS_H
#define AVP_NEW_LOCAL_MP_RETAIL_MAPS_H

#include <stddef.h>
#include <stdint.h>

/* PC extension only. Real decoded Jaguar FILES 87..101, not generated mazes.
 * A level is shared within the chosen campaign and independent across levels.
 * This layer owns original maze bytes and mutable per-campaign copies, but DOES
 * NOT pretend to run original AI, doors, collision or player simulation.
 * Existing game globals, maze bindings, or quicksave areas are never swapped.
 */
#define AVP_MP_RETAIL_LEVELS 15u
#define AVP_MP_RETAIL_CAMPAIGNS 3u
#define AVP_MP_RETAIL_MAZE_FIRST_FILE 87u

typedef struct AvpMpRetailMapSource {
    unsigned file_index;         /* Source-proven retail FILES index 87..101. */
    const uint8_t *decoded;      /* Includes original eight-byte maze header. */
    size_t decoded_size;
    unsigned width, height;      /* Must match header, 1..64 each. */
} AvpMpRetailMapSource;

typedef struct AvpMpRetailMap {
    uint8_t *bytes;              /* Owned mutable level bytes, header included. */
    size_t size;
    unsigned width, height, level;
    unsigned leases;             /* Zero means dormant, NOT reset. */
} AvpMpRetailMap;

typedef struct AvpMpRetailMaps {
    uint8_t *original[AVP_MP_RETAIL_LEVELS];
    size_t original_size[AVP_MP_RETAIL_LEVELS];
    unsigned width[AVP_MP_RETAIL_LEVELS], height[AVP_MP_RETAIL_LEVELS];
    AvpMpRetailMap saved[AVP_MP_RETAIL_CAMPAIGNS][AVP_MP_RETAIL_LEVELS];
    int ready;
} AvpMpRetailMaps;

/* Initialize a zeroed store by copying all 15 *unmodified* retail decoded maps.
 * Full success or no owned allocations. No ROM or filesystem access here. */
int avp_mp_retail_maps_init(AvpMpRetailMaps *store,
                            const AvpMpRetailMapSource *sources, unsigned count);
/* Lease one persistent per-(campaign,level) map.  No frame swapping. */
AvpMpRetailMap *avp_mp_retail_maps_acquire(AvpMpRetailMaps *store,
                                          unsigned campaign, unsigned level);
/* Reject mismatched/double releases. A dormant map keeps its exact bytes. */
int avp_mp_retail_maps_release(AvpMpRetailMaps *store, unsigned campaign,
                               unsigned level, const AvpMpRetailMap *map);
/* Reset a campaign to the actual retail initial bytes. Refuse while leased. */
int avp_mp_retail_maps_reset_campaign(AvpMpRetailMaps *store, unsigned campaign);
/* Refuse to destroy a store with live leases. Callers must stop sessions first. */
int avp_mp_retail_maps_destroy(AvpMpRetailMaps *store);

/* Bounds-checked editable cell bytes. Original Jaguar maze cells are 8 bytes;
 * do NOT invent door/physics rules or assume a byte's semantics here. */
uint8_t *avp_mp_retail_map_cell(AvpMpRetailMap *map, unsigned x, unsigned y);
const uint8_t *avp_mp_retail_map_cell_const(const AvpMpRetailMap *map,
                                            unsigned x, unsigned y);
#endif
