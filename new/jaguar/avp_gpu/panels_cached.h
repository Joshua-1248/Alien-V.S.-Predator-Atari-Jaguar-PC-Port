/* S40: verified cached-only subset of original retail $F1F6 panel walker.
 * Not a substitute for the original FILES, notice or Blitter paths.
 * No use by the live PC renderer. */
#ifndef AVP_JAG_PANELS_CACHED_H
#define AVP_JAG_PANELS_CACHED_H
#include "jaguar/avp_gpu/files_dispatch.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_JAG_PANEL_SLOT_BASE 0x0DD000u
#define AVP_JAG_PANEL_CACHE_BASE 0x0309BEu
#define AVP_JAG_PANEL_INDEX_ADDR 0x02F5B6u
#define AVP_JAG_PANEL_SLOT_BYTES 32768u
#define AVP_JAG_PANEL_NOTICE_NEXT_ADDR 0x02F5BAu
#define AVP_JAG_PANEL_NOTICE_MAP_BASE 0x023FACu
/* The caller provides the exact DRAM extent containing the panel-list bytes.
 * Each entry has {texture record pointer, notice pointer, byte IDs, FF padded
 * to word}, terminated by a negative long. The start contains an initial
 * signed slot index (negative means leave the index unchanged).
 * Only cached IDs (not $80) with NULL notice pointers are supported.
 * On an unsupported entry, previous original-style writes are retained.
 * For genuine full FILES loading use the unported original routine instead.
 * `next_list` is the first byte after the terminating long on success. */
int avp_jagpanel_walk_cached(const AvpView3DBus *bus,
                             uint32_t list_begin,uint32_t list_end,
                             uint32_t *next_list);
/* Extends the verified cached path with original NONE FILES ($BD44/$BD88)
 * cache misses. Still rejects JPEG/GZIP/special files and notice overlays.
 * cfg must describe the authenticated caller's live FILES table and archive. */
int avp_jagpanel_walk_cached_or_none(const AvpView3DBus *bus,
                                     const AvpJagFilesConfig *cfg,
                                     uint32_t list_begin,uint32_t list_end,
                                     uint32_t *next_list);
/* S41: isolated original $F218/$F296 notice publication and FILES/NONE
 * or zero-fill (256-byte) notice slot. The caller must provide authentic
 * DRAM mapping table and file archive state. Special $80, compressed FILES,
 * other hardware-dependent panel/Blitter paths are still unsupported.
 * No live game calls this API. */
int avp_jagpanel_walk_cached_or_none_notice(const AvpView3DBus *bus,
                                            const AvpJagFilesConfig *cfg,
                                            uint32_t list_begin,uint32_t list_end,
                                            uint32_t *next_list);
#ifdef __cplusplus
}
#endif
#endif
