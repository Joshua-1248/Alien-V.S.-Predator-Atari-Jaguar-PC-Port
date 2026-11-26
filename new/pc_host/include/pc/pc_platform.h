#ifndef AVP_PC_PLATFORM_H
#define AVP_PC_PLATFORM_H
#include "avp_types.h"
#include "avp_rom.h"
#include <stddef.h>



int avp_pc_rom_load(const char *path, AvpRomImage *rom, char *err, size_t errcap);
void avp_pc_rom_free(AvpRomImage *rom);

/* Host input is converted into the Jaguar active-low 32-bit pad image. */
void avp_pc_pad_set(u32 active_low);
u32 avp_pc_pad_get(void);
/* PC-only hook: Jaguar may poll the pad between VBLs while waiting for a
 * button release in its original synchronous terminal menus. */
void avp_pc_pad_set_poll_callback(void (*callback)(void));

/* Monotonic host timing in nanoseconds. */
u64 avp_pc_now_ns(void);
void avp_pc_sleep_ns(u64 ns);

#endif
