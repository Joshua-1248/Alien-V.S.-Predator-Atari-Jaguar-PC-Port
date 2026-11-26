/* R331: deterministic Vulkan-ready ray packet serializer. The packing is
 * self-authored; it is not a copy of a Jaguar or third-party renderer. */
#include "jaguar/avp_gpu/world_scene_gpu_wire.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

_Static_assert(sizeof(double)==8u, "The wire format requires 64-bit double");
_Static_assert(sizeof(uint64_t)==8u, "The wire format requires 64-bit uint");

static void put32(uint8_t *dst, uint32_t v)
{
    dst[0]=(uint8_t)v;
    dst[1]=(uint8_t)(v>>8);
    dst[2]=(uint8_t)(v>>16);
    dst[3]=(uint8_t)(v>>24);
}
static void put64(uint8_t *dst, double value)
{
    uint64_t bits;
    memcpy(&bits,&value,sizeof(bits));
    put32(dst,(uint32_t)bits);
    put32(dst+4,(uint32_t)(bits>>32));
}
void avp_world_gpu_wire_invalidate(AvpWorldGpuWire *dst)
{
    if (dst) dst->bytes=0u;
}
void avp_world_gpu_wire_destroy(AvpWorldGpuWire *dst)
{
    if (!dst) return;
    free(dst->data);
    memset(dst,0,sizeof(*dst));
}
int avp_world_gpu_wire_pack(AvpWorldGpuWire *dst,
                            const AvpWorldScenePacket *p)
{
    size_t hits=0u, columns_size, hits_size, required;
    uint8_t *out;
    if (!dst) return 0;
    dst->bytes=0u; /* a failed conversion never leaves a valid stale frame */
    if (!p || !avp_world_scene_complete(p) ||
        !p->width || p->width>AVP_WORLD_SCENE_MAX_WIDTH ||
        !p->height || p->height>AVP_WORLD_SCENE_MAX_HEIGHT ||
        !p->hit_capacity_per_column ||
        p->hit_capacity_per_column>AVP_WORLD_SCENE_MAX_HITS ||
        !p->map_width || !p->map_height) return 0;
    for (uint32_t x=0u; x<p->width; ++x) {
        const AvpWorldRayColumn *c=&p->columns[x];
        if (!isfinite(c->wall_depth) || c->wall_depth<0.0 ||
            c->count>p->hit_capacity_per_column ||
            hits>UINT32_MAX-c->count) return 0;
        for(uint32_t j=0u;j<c->count;++j) {
            const AvpWorldRayHit *h=&p->hits[(size_t)x*p->hit_capacity_per_column+j];
            if(!isfinite(h->distance) || h->distance<0.0 ||
               !isfinite(h->texture_fraction) ||
               h->wall>255u || h->open>255u ||
               h->vertical>1u || h->is_final>1u) return 0;
        }
        hits+=c->count;
    }
    columns_size=(size_t)p->width*AVP_WORLD_GPU_WIRE_COLUMN_BYTES;
    hits_size=hits*AVP_WORLD_GPU_WIRE_HIT_BYTES;
    if (columns_size>SIZE_MAX-AVP_WORLD_GPU_WIRE_HEADER_BYTES ||
        hits_size>SIZE_MAX-AVP_WORLD_GPU_WIRE_HEADER_BYTES-columns_size) return 0;
    required=AVP_WORLD_GPU_WIRE_HEADER_BYTES+columns_size+hits_size;
    if (required>UINT32_MAX) return 0;
    if (required>dst->capacity) {
        uint8_t *updated=realloc(dst->data,required);
        if(!updated) return 0;
        dst->data=updated;
        dst->capacity=required;
    }
    out=dst->data;
    put32(out,AVP_WORLD_GPU_WIRE_MAGIC);
    put32(out+4,AVP_WORLD_GPU_WIRE_VERSION);
    put32(out+8,p->width);
    put32(out+12,p->height);
    put32(out+16,(uint32_t)hits);
    put32(out+20,AVP_WORLD_GPU_WIRE_HEADER_BYTES);
    put32(out+24,(uint32_t)(AVP_WORLD_GPU_WIRE_HEADER_BYTES+columns_size));
    put32(out+28,0u);
    {
        uint32_t offset=0u;
        for(uint32_t x=0u;x<p->width;++x) {
            const AvpWorldRayColumn *c=&p->columns[x];
            uint8_t *d=out+AVP_WORLD_GPU_WIRE_HEADER_BYTES+
                       (size_t)x*AVP_WORLD_GPU_WIRE_COLUMN_BYTES;
            put32(d,offset);
            put32(d+4,c->count);
            put64(d+8,c->wall_depth);
            put32(d+16,c->ready);
            put32(d+20,c->overflow);
            offset+=c->count;
        }
    }
    {
        size_t next=0u, base=AVP_WORLD_GPU_WIRE_HEADER_BYTES+columns_size;
        for(uint32_t x=0u;x<p->width;++x) {
            for(uint32_t j=0u;j<p->columns[x].count;++j) {
                const AvpWorldRayHit *h=&p->hits[(size_t)x*p->hit_capacity_per_column+j];
                uint8_t *d=out+base+next*AVP_WORLD_GPU_WIRE_HIT_BYTES;
                put64(d,h->distance);
                put64(d+8,h->texture_fraction);
                put32(d+16,h->wall);
                put32(d+20,h->open);
                put32(d+24,h->vertical);
                put32(d+28,h->is_final);
                put32(d+32,(uint32_t)h->clip_lo);
                put32(d+36,(uint32_t)h->clip_hi);
                ++next;
            }
        }
    }
    dst->bytes=required;
    return 1;
}
