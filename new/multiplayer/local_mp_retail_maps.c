#include "local_mp_retail_maps.h"
#include <stdlib.h>
#include <string.h>

static unsigned read_be16(const uint8_t *p)
{
    return ((unsigned)p[0] << 8) | (unsigned)p[1];
}
static int valid_source(const AvpMpRetailMapSource *s)
{
    size_t need;
    if (!s || !s->decoded || s->width < 1u || s->height < 1u ||
        s->width > 64u || s->height > 64u) return 0;
    need = 8u + (size_t)s->width * s->height * 8u;
    return s->decoded_size == need &&
           read_be16(s->decoded) == s->width &&
           read_be16(s->decoded + 2u) == s->height;
}

static void free_store(AvpMpRetailMaps *store)
{
    unsigned c, i;
    for (c = 0; c < AVP_MP_RETAIL_CAMPAIGNS; ++c)
        for (i = 0; i < AVP_MP_RETAIL_LEVELS; ++i)
            free(store->saved[c][i].bytes);
    for (i = 0; i < AVP_MP_RETAIL_LEVELS; ++i)
        free(store->original[i]);
    memset(store, 0, sizeof(*store));
}

int avp_mp_retail_maps_init(AvpMpRetailMaps *store,
                            const AvpMpRetailMapSource *sources, unsigned count)
{
    unsigned i;
    unsigned found[AVP_MP_RETAIL_LEVELS] = {0};
    if (!store || !sources || count != AVP_MP_RETAIL_LEVELS || store->ready) return 0;
    /* A fresh store is zeroed. A failed prior initialization cleans itself. */
    for (i = 0; i < AVP_MP_RETAIL_LEVELS; ++i) {
        unsigned file, idx;
        const AvpMpRetailMapSource *src = &sources[i];
        if (!valid_source(src)) goto fail;
        file = src->file_index;
        if (file < AVP_MP_RETAIL_MAZE_FIRST_FILE ||
            file >= AVP_MP_RETAIL_MAZE_FIRST_FILE + AVP_MP_RETAIL_LEVELS)
            goto fail;
        idx = file - AVP_MP_RETAIL_MAZE_FIRST_FILE;
        if (found[idx]) goto fail;
        found[idx] = 1;
        store->original[idx] = malloc(src->decoded_size);
        if (!store->original[idx]) goto fail;
        memcpy(store->original[idx], src->decoded, src->decoded_size);
        store->original_size[idx] = src->decoded_size;
        store->width[idx] = src->width;
        store->height[idx] = src->height;
    }
    store->ready = 1;
    return 1;
fail:
    free_store(store);
    return 0;
}

AvpMpRetailMap *avp_mp_retail_maps_acquire(AvpMpRetailMaps *store,
                                          unsigned campaign, unsigned level)
{
    AvpMpRetailMap *map;
    unsigned idx;
    if (!store || !store->ready || campaign >= AVP_MP_RETAIL_CAMPAIGNS ||
        level < 1u || level > AVP_MP_RETAIL_LEVELS) return NULL;
    idx = level - 1u;
    map = &store->saved[campaign][idx];
    if (!map->bytes) {
        map->bytes = malloc(store->original_size[idx]);
        if (!map->bytes) return NULL;
        memcpy(map->bytes, store->original[idx], store->original_size[idx]);
        map->size = store->original_size[idx];
        map->width = store->width[idx];
        map->height = store->height[idx];
        map->level = level;
    }
    if (map->leases == ~0u) return NULL;
    ++map->leases;
    return map;
}

int avp_mp_retail_maps_release(AvpMpRetailMaps *store, unsigned campaign,
                               unsigned level, const AvpMpRetailMap *map)
{
    AvpMpRetailMap *slot;
    if (!store || !store->ready || campaign >= AVP_MP_RETAIL_CAMPAIGNS ||
        level < 1u || level > AVP_MP_RETAIL_LEVELS || !map) return 0;
    slot = &store->saved[campaign][level - 1u];
    if (map != slot || !slot->leases) return 0;
    --slot->leases;
    return 1;
}

int avp_mp_retail_maps_reset_campaign(AvpMpRetailMaps *store, unsigned campaign)
{
    unsigned i;
    if (!store || !store->ready || campaign >= AVP_MP_RETAIL_CAMPAIGNS) return 0;
    for (i = 0; i < AVP_MP_RETAIL_LEVELS; ++i)
        if (store->saved[campaign][i].leases) return 0;
    for (i = 0; i < AVP_MP_RETAIL_LEVELS; ++i) {
        AvpMpRetailMap *slot = &store->saved[campaign][i];
        /* Copy into existing storage: no allocation, no failure half-way. */
        if (slot->bytes) memcpy(slot->bytes, store->original[i], slot->size);
    }
    return 1;
}

int avp_mp_retail_maps_destroy(AvpMpRetailMaps *store)
{
    unsigned c, i;
    if (!store) return 0;
    for (c = 0; c < AVP_MP_RETAIL_CAMPAIGNS; ++c)
        for (i = 0; i < AVP_MP_RETAIL_LEVELS; ++i)
            if (store->saved[c][i].leases) return 0;
    free_store(store);
    return 1;
}

uint8_t *avp_mp_retail_map_cell(AvpMpRetailMap *map, unsigned x, unsigned y)
{
    size_t off;
    if (!map || !map->bytes || x >= map->width || y >= map->height) return NULL;
    off = 8u + ((size_t)y * map->width + x) * 8u;
    return off + 8u <= map->size ? map->bytes + off : NULL;
}

const uint8_t *avp_mp_retail_map_cell_const(const AvpMpRetailMap *map,
                                            unsigned x, unsigned y)
{
    return avp_mp_retail_map_cell((AvpMpRetailMap *)map, x, y);
}
