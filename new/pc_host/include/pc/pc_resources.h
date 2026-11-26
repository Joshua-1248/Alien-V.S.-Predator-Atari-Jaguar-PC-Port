#ifndef AVP_PC_RESOURCES_H
#define AVP_PC_RESOURCES_H
#include "avp_resources.h"
#include <stddef.h>

/* PC-only derived-cache writer. Resource parsing itself is host-independent. */
int avp_pc_prepare_rom_cache(const AvpRomImage *rom,const char *exe_path,char *out_dir,size_t out_dir_cap);
/* Start of a new campaign: restore the mutable decoded MAZE bytes from the
 * user's original ROM. Normal level travel preserves its saved doors. */
int avp_pc_restore_new_campaign_mazes(const AvpRomImage *rom,AvpMazeCandidate *mazes,unsigned count);
/* Debug refresh: replace just this writable maze with ROM original bytes. */
int avp_pc_restore_single_maze(const AvpRomImage *rom,AvpMazeCandidate *maze);

#endif
