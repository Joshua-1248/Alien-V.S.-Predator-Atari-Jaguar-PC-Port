#ifndef AVP_PC_VULKAN_GEOMETRY_POLICY_H
#define AVP_PC_VULKAN_GEOMETRY_POLICY_H
#include <stddef.h>
#include <stdint.h>
/* Complete, bounded little-endian R331 wire validation; no Vulkan SDK. */
int avp_vk_geometry_wire_valid(const uint8_t *wire, size_t bytes);
/* Memory selection for the two-buffer upload, no driver involved. */
int avp_vk_geometry_choose_memory(uint32_t supported,uint32_t required,
    const uint32_t *flags,uint32_t count,uint32_t *index);
#endif
