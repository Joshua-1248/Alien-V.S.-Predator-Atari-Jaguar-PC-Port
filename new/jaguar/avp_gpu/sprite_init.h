/* RE25 S49: isolated native reconstruction of the retail 68000 sprite
 * initialization subroutines. Original ROM is authoritative. Not linked
 * into playable PC input/renderer/loader paths. */
#ifndef AVP_JAG_SPRITE_INIT_H
#define AVP_JAG_SPRITE_INIT_H
#include "jaguar/avp_gpu/sprite_sets.h"
#ifdef __cplusplus
extern "C" {
#endif
/* $DB92-$DBAC: reset FILES archive state and sprite allocation base. */
int avp_jagsprite_driver_prepare(const AvpJagFilesJpegDispatch *d);
/* $DC1E Alien, $DCA8 Predator, $DD24 Marine. Includes direct CLUT FILES. */
int avp_jagsprite_character_init(const AvpJagFilesJpegDispatch *d);
/* $DBFE-$DC1C: two shared FILES resources, second selected at $3108A. */
int avp_jagsprite_common_init(const AvpJagFilesJpegDispatch *d);
#ifdef __cplusplus
}
#endif
#endif
