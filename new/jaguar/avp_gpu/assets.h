#ifndef AVP_JAGUAR_AVP_GPU_ASSETS_H
#define AVP_JAGUAR_AVP_GPU_ASSETS_H

#include "avp_rom.h"
#include "jaguar/avp_gpu/state.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bind presentation resources from the user's verified retail cartridge. */
int avp_gpu_assets_load_retail(AvpAvpGpuState *state,const AvpRomImage *rom);
int avp_gpu_assets_frontend_load(AvpAvpGpuState *state,const AvpRomImage *rom,unsigned mode,unsigned file_index,unsigned role);

#ifdef __cplusplus
}
#endif

#endif
