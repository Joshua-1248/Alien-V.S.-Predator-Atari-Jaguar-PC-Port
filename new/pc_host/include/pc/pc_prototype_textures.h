#ifndef AVP_PC_PROTOTYPE_TEXTURES_H
#define AVP_PC_PROTOTYPE_TEXTURES_H
#include "jaguar/avp_gpu/state.h"
#include <stdint.h>

/* Presentation-only, user-generated external RGB replacement pack.
 * All buffers owned by the original GPU state remain at stable addresses,
 * including when its assets are borrowed by independent co-op views. */
#define AVP_PC_PROTOTYPE_SLOTS 56u
#define AVP_PC_PROTOTYPE_SIDE 128u
#define AVP_PC_PROTOTYPE_RGB_BYTES (AVP_PC_PROTOTYPE_SIDE * AVP_PC_PROTOTYPE_SIDE * 3u)
#define AVP_PC_PROTOTYPE_CRY_BYTES (AVP_PC_PROTOTYPE_SIDE * AVP_PC_PROTOTYPE_SIDE * sizeof(uint16_t))
typedef struct AvpPcPrototypeTextures {
    uint8_t *retail_rgb[AVP_PC_PROTOTYPE_SLOTS];
    uint16_t *retail_cry[AVP_PC_PROTOTYPE_SLOTS];
    unsigned applied;
} AvpPcPrototypeTextures;
void avp_pc_prototype_textures_destroy(AvpPcPrototypeTextures *swap);
void avp_pc_prototype_textures_capture(AvpPcPrototypeTextures *swap,const AvpAvpGpuState *state);
unsigned avp_pc_prototype_textures_apply(AvpPcPrototypeTextures *swap,AvpAvpGpuState *state,
                                         const char *directory,int enabled);
/* S108: search user-local packs beside the executable first, then its parent
 * (source-tree build layout), then the working directory. Explicit overrides
 * are exclusive. Does not load or embed any texture artwork in the binary. */
unsigned avp_pc_prototype_textures_apply_search(AvpPcPrototypeTextures *swap,
    AvpAvpGpuState *state,const char *override_directory,const char *executable_path,
    const char *working_pack_directory,int enabled,char *selected_directory,
    size_t selected_capacity);
#endif
