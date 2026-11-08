#ifndef AVP_RESOURCES_H
#define AVP_RESOURCES_H

#include "avp_rom.h"
#include "avp_types.h"
#include "doors.h"
#include <stddef.h>

typedef struct AvpMazeCandidate {
    unsigned file_index;
    size_t record_offset;
    u16 width,height;
    u8 *decoded;
    size_t decoded_size;
} AvpMazeCandidate;

int avp_resources_find_files_archive(const AvpRomImage *rom,size_t *start,unsigned *records);
unsigned avp_resources_find_mazes(const AvpRomImage *rom,AvpMazeCandidate *out,unsigned cap);
void avp_resources_free_mazes(AvpMazeCandidate *m,unsigned count);
int avp_resources_file_payload(const AvpRomImage *rom,unsigned file_index,char tag[5],const u8 **payload,size_t *payload_size);
int avp_resources_decode_file(const AvpRomImage *rom,unsigned file_index,char tag[5],u8 **decoded,size_t *decoded_size);
int avp_resources_aaf_payload(const AvpRomImage *rom,unsigned aaf_index,const u8 **payload,size_t *payload_size);
unsigned avp_resources_aaf_count(const AvpRomImage *rom);
int avp_resources_decode_aaf(const AvpRomImage *rom,unsigned aaf_index,u8 **decoded,size_t *decoded_size);
int avp_resources_bind_retail_amp_placements(const AvpRomImage *rom,unsigned *out_lists,unsigned *out_entries);
struct AvpAmpPlacement;
/* Read-only PC resource view for isolated Map Maker test-process overlays. */
const struct AvpAmpPlacement *avp_resources_retail_amp_list(unsigned kind,unsigned level);
int avp_resources_bind_retail_level_info(const AvpRomImage *rom);
/* Read-only per-level table query for bot route graph; no game-global writes. */
int avp_resources_retail_panel_info(const AvpRomImage *rom,unsigned level,AvpDoorPanelIds *out_ids,u8 out_list[AVP_MAX_PANELS],unsigned *out_count);
int avp_resources_bind_retail_panel_info(const AvpRomImage *rom,unsigned level,AvpDoorPanelIds *out_ids);
int avp_resources_decode_rdb(const AvpRomImage *rom,u8 **decoded,size_t *decoded_size);
int avp_resources_decode_rdb_text(const AvpRomImage *rom,u8 **decoded,size_t *decoded_size);
const char *avp_resources_rdb_address(const u8 *rdb,size_t rdb_size,u32 address);
int avp_resources_rom_descriptor(const AvpRomImage *rom,u32 descriptor_addr,const u8 **data,size_t *size);

#endif
