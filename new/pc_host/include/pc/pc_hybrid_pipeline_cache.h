#ifndef AVP_PC_HYBRID_PIPELINE_CACHE_H
#define AVP_PC_HYBRID_PIPELINE_CACHE_H
/* RE47 R363: SDK-independent ownership policy for persistent hybrid Vulkan
 * pipelines. Never trust a path, timestamp, shader hash or descriptor handle
 * when the actual SPIR-V bytes/presentation region have changed. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AvpHybridPipelineKey {
    uint32_t target_width, target_height, target_format;
    uint32_t region_x, region_y, region_width, region_height;
    uint32_t push_bytes;
    const uint32_t *vertex, *world, *foreground;
    size_t vertex_bytes, world_bytes, foreground_bytes;
} AvpHybridPipelineKey;
typedef struct AvpHybridPipelineCache {
    AvpHybridPipelineKey key;
    uint32_t *shader_copies[3];
    int valid;
} AvpHybridPipelineCache;
/* Exact metadata and byte comparison; rejects incomplete or oversized inputs. */
int avp_hybrid_pipeline_key_valid(const AvpHybridPipelineKey *key);
int avp_hybrid_pipeline_cache_matches(const AvpHybridPipelineCache *cache,
                                      const AvpHybridPipelineKey *key);
/* Atomic snapshot: an allocation failure never corrupts the old snapshot. */
int avp_hybrid_pipeline_cache_capture(AvpHybridPipelineCache *cache,
                                      const AvpHybridPipelineKey *key);
void avp_hybrid_pipeline_cache_invalidate(AvpHybridPipelineCache *cache);
#ifdef __cplusplus
}
#endif
#endif
