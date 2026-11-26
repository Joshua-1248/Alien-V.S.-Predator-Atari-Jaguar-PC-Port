#ifndef AVP_PC_HI_RES_SPRITES_H
#define AVP_PC_HI_RES_SPRITES_H
#include "jaguar/avp_gpu/hires_sprite_pack.h"
#include <stddef.h>
#include <stdint.h>
typedef struct AvpPcHiResSprites {
    AvpGpuExternalSprites images;
    size_t decoded_bytes;
} AvpPcHiResSprites;
/* Idempotent. PNG contents remain external to the published game. */
void avp_pc_hi_res_sprites_destroy(AvpPcHiResSprites *pack);
/* AAF frame counts are read from decoded original retail AAF sheets. */
unsigned avp_pc_hi_res_sprites_load(AvpPcHiResSprites *pack,const char *executable,
                                   const uint8_t *const sheets[63],const size_t sizes[63]);
#endif
