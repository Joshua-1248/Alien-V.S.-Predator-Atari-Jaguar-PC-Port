/* RE24 S30: host-independent native port of the original 68000 FILES
 * record locator ($BD22) and codec dispatcher ($BD44-$BD86).
 *
 * Uses the caller's Jaguar bus and explicit archive bounds.  The dispatcher
 * table is READ from Jaguar DRAM: it is NOT a guessed or hard-coded codec
 * mapping.  No ROM bytes or decoded assets are contained in this module.
 * The live game's existing loader is not changed. */
#ifndef AVP_JAG_FILES_DISPATCH_H
#define AVP_JAG_FILES_DISPATCH_H
#include "jaguar/avp_gpu/view3d.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AVP_JAG_FILES_ARCHIVE_POINTER 0x815C0Cu
#define AVP_JAG_FILES_DISPATCH_TABLE 0x022B94u
#define AVP_JAG_FILES_PREVIOUS_TAG   0x02EF50u

typedef struct AvpJagFilesConfig {
    /* Address storing the first record's Jaguar address (retail $815C0C). */
    uint32_t first_pointer;
    /* Inclusive start and EXCLUSIVE end of the validated file archive. */
    uint32_t archive_begin, archive_end;
    /* Original table: {four-byte tag, initializer PC, expander PC},
     * terminated by a zero tag. */
    uint32_t dispatch_table, previous_tag;
    /* Only used for safe bounded traversal, not an original 68000 limit. */
    unsigned max_entries;
} AvpJagFilesConfig;

typedef struct AvpJagFilesPlan {
    uint32_t index, header, payload, tag;
    uint32_t table_entry, init_pc, expand_pc;
    /* The original calls an initializer ONLY on a tag change and ONLY if
     * the entry has a nonzero init PC.  The previous tag is updated only
     * after that initializer returns. */
    int call_init;
} AvpJagFilesPlan;

enum {
    AVP_JAGFILES_OK=0,
    AVP_JAGFILES_INVALID=-1,
    AVP_JAGFILES_NOT_FOUND=-2,
    AVP_JAGFILES_OUT_OF_RANGE=-3,
    AVP_JAGFILES_NO_HANDLER=-4
};

/* Read-only.  Does not change the bus, game state, or cached JPEG quality.
 * The 68000's 16-bit DBRA index is enforced; any larger input is rejected.
 * The caller MUST set authentic safe bounds for the ROM file region. */
int avp_jagfiles_plan(const AvpView3DBus *bus, const AvpJagFilesConfig *config,
                      uint32_t index, AvpJagFilesPlan *result);

/* An adapter for the original A0/A1/PC calls.  For init, record_addr is
 * the file HEADER; for expand it is the PAYLOAD (header + 8).  The original
 * 68000 saves D0/A0/A1/A3 around the init call before jumping to expand.
 * An adapter may reject unsupported native handlers (e.g. unported GPU
 * loaders); a zero return must be treated as failure, not success. */
typedef int (*AvpJagFilesInvoke)(void *ctx, uint32_t routine_pc,
                                 uint32_t record_addr, uint32_t dest_addr);
int avp_jagfiles_execute(const AvpView3DBus *bus,const AvpJagFilesConfig *config,
                         const AvpJagFilesPlan *plan,uint32_t dest,
                         AvpJagFilesInvoke invoke,void *ctx);

/* Original NONE handler $BD88: copies the record's payload verbatim.
 * The explicit destination end must be within the caller's RAM allocation.
 * Rejects aliasing and short buffers before writing any bytes. */
int avp_jagfiles_expand_none(const AvpView3DBus *bus,const AvpJagFilesConfig *config,
                             const AvpJagFilesPlan *plan,uint32_t dest,
                             uint32_t dest_end,uint32_t *written);
#ifdef __cplusplus
}
#endif
#endif
