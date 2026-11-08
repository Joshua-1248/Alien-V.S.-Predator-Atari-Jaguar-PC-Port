#ifndef AVP_JAGREF_VIEW3D_SNAPSHOT_H
#define AVP_JAGREF_VIEW3D_SNAPSHOT_H
/* Local, ROM-gated reproducibility fixture for a Jaguar GPU view entry.
 * Captures memory and architectural state, not host pointers or Musashi state.
 * Fixtures contain proprietary game data and MUST NEVER be redistributed. */
#include "jaguar/jag_machine.h"
#include <stddef.h>
#include <stdint.h>
uint64_t avp_view3d_rom_fingerprint(const uint8_t *rom,size_t size);
int avp_view3d_snapshot_write(const char *path,const JagMachine *m,
                              uint64_t rom_fingerprint);
int avp_view3d_snapshot_read(const char *path,JagMachine *m,
                             uint64_t rom_fingerprint);
/* S29: private JagPEG GPU-entry images.  Distinct file magic prevents
 * confusing a 3D view capture with a JPEG decode/convert capture. */
int avp_jagpeg_snapshot_write(const char *path,const JagMachine *m,
                              uint64_t rom_fingerprint,uint32_t entry);
int avp_jagpeg_snapshot_read(const char *path,JagMachine *m,
                             uint64_t rom_fingerprint,uint32_t entry);
#endif
