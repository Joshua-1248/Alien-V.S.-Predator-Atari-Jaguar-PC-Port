#ifndef AVP_PC_HYBRID_TEXTURE_CACHE_H
#define AVP_PC_HYBRID_TEXTURE_CACHE_H
/* RE47 R362: byte-exact, SDK-independent policy for retaining GPU-resident
 * original source textures between hybrid frames. This is not an image cache
 * keyed only by a hash: source words and slot metadata MUST compare exactly.
 * Cache is scoped to one Vulkan device; call invalidate on reset/teardown. */
#include "pc/pc_vulkan_layered_frame.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct AvpHybridTextureCacheSlot {
    uint32_t *words;
    size_t bytes;
    uint32_t width,height,encoding;
} AvpHybridTextureCacheSlot;

typedef struct AvpHybridTextureCache {
    AvpHybridTextureCacheSlot slots[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t order[AVP_WORLD_SCENE_IMAGE_SLOTS];
    uint32_t used_count;
    uint64_t bytes;
    int valid;
} AvpHybridTextureCache;

/* Returns 1 only for an EXACT source/data/slot-plan match. A changed
 * texture, encoding, texture size, used-slot set, or corrupt frame misses. */
int avp_hybrid_texture_cache_matches(const AvpHybridTextureCache *cache,
                                    const AvpVkLayeredFrame *frame);
/* Transactional replacement. A failed allocation or invalid scene does not
 * mutate an existing snapshot; caller must still rebuild rather than reuse. */
int avp_hybrid_texture_cache_capture(AvpHybridTextureCache *cache,
                                    const AvpVkLayeredFrame *frame);
/* R376: independent exact-byte per-slot cache. A slot that goes out of
 * view stays eligible for GPU reuse when the player turns back. */
int avp_hybrid_texture_cache_slot_matches(const AvpHybridTextureCache *cache,
    const AvpVkLayeredFrame *frame,uint32_t slot);
int avp_hybrid_texture_cache_slot_capture(AvpHybridTextureCache *cache,
    const AvpVkLayeredFrame *frame,uint32_t slot);
void avp_hybrid_texture_cache_invalidate(AvpHybridTextureCache *cache);
#ifdef __cplusplus
}
#endif
#endif
