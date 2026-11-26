#ifndef AVP_PC_HYBRID_SHADER_CACHE_H
#define AVP_PC_HYBRID_SHADER_CACHE_H
/* RE47 R365: exact validated shader bytes are owned by a persistent game
 * Vulkan context, not read and reallocated three times per frame. A changed
 * file stat always triggers a full SPIR-V structural revalidation. Only the
 * opt-in Linux Vulkan target includes this SDK-independent source module. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_HYBRID_SHADER_COUNT 3u
#define AVP_HYBRID_SHADER_PATH_MAX 1024u
typedef struct AvpHybridShaderStamp {
    uint64_t device, inode, bytes;
    int64_t mtime_seconds, mtime_nanos, ctime_seconds, ctime_nanos;
} AvpHybridShaderStamp;
typedef struct AvpHybridShaderEntry {
    char path[AVP_HYBRID_SHADER_PATH_MAX];
    AvpHybridShaderStamp stamp;
    uint32_t *words;
    size_t bytes;
} AvpHybridShaderEntry;
typedef struct AvpHybridShaderCache {
    AvpHybridShaderEntry entries[AVP_HYBRID_SHADER_COUNT];
    uint64_t updates;
} AvpHybridShaderCache;
/* The three paths are vertex, world fragment, foreground fragment.
 * Refresh is transactional. A missing, changed-in-flight or invalid shader
 * fails the entire frame without replacing any previously validated bytes.
 * File content on disk is treated as immutable while its identity and
 * nanosecond modification/change timestamps remain unchanged; rebuild or
 * touch shaders to trigger a hot reload. */
int avp_hybrid_shader_cache_refresh(AvpHybridShaderCache *cache,
    const char *vertex, const char *world, const char *foreground);
void avp_hybrid_shader_cache_dispose(AvpHybridShaderCache *cache);
#ifdef __cplusplus
}
#endif
#endif
