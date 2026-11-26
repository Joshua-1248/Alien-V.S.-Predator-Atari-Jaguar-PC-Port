#ifndef AVP_PC_HYBRID_STREAM_POLICY_H
#define AVP_PC_HYBRID_STREAM_POLICY_H
/* R364. Hardware-independent, bounded reusable Vulkan frame allocation policy.
 * No Jaguar game state, SDK or framebuffer pixels are owned here. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* R378: the source-tape builder already supports 64 MiB. Match it so
 * native 2560x1440 (56.25 MiB) does not needlessly fall back to Software. */
#define AVP_HYBRID_GEOMETRY_MAX_BYTES (64u * 1024u * 1024u)
#define AVP_HYBRID_FOREGROUND_MAX_BYTES (64u * 1024u * 1024u)
/* 1 = valid; *resize is set iff current allocation cannot hold required.
 * Geometric growth reduces buffer churn on fluctuating camera geometry. */
int avp_hybrid_stream_capacity(size_t required, size_t current,
    size_t maximum, size_t *planned, int *resize);
/* Pixel counts must fit R32_UINT, be within Vulkan's expected size gates,
 * and never overflow the width*height*4 calculation. */
int avp_hybrid_stream_foreground_size(uint32_t width,uint32_t height,
    size_t *bytes);

/* R367: exact foreground-content reuse decision. The GPU image may be
 * reused only after a completed, shader-readable upload. Compare every
 * source byte, never a hash or the address of the caller's pixel buffer.
 * Returns -1 for invalid input, 0 to reuse, 1 to upload/copy. */
int avp_hybrid_stream_foreground_upload_needed(const uint32_t *staging,
    const uint32_t *pixels,uint32_t width,uint32_t height,
    size_t staging_bytes,int image_uploaded);

#ifdef __cplusplus
}
#endif
#endif
