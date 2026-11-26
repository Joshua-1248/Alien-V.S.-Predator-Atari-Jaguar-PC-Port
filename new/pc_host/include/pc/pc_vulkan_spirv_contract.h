#ifndef AVP_PC_VULKAN_SPIRV_CONTRACT_H
#define AVP_PC_VULKAN_SPIRV_CONTRACT_H
/* RE46 R327. Minimal structural gate, NOT a complete SPIR-V validator.
 * Official spirv-val and a Vulkan driver are required for full validation. */
#include <stddef.h>
#include <stdint.h>
#define AVP_SPIRV_VERTEX 0u
#define AVP_SPIRV_FRAGMENT 4u
#define AVP_SPIRV_MAX_BYTES (8u * 1024u * 1024u)
int avp_spirv_stage_contract(const uint32_t *words, size_t count, uint32_t model);
/* Copies binary only after passing basic byte-order, stage and instruction
 * checks. Caller owns *out and must free it. Does not execute untrusted code. */
int avp_spirv_read_file(const char *path, uint32_t expected_model,
                        uint32_t **out, size_t *out_bytes);
#endif
