#include "pc/pc_quickstate.h"
/* Windows: the in-session F5/F9 snapshot (pc_quickstate.c) uses the PE game
   state section from cmake/avp_game_state_pe.ld.  The durable cross-session
   archive relocates stale native pointers using the Linux address-space
   layout; that relocation rule is not valid for Windows heap/image placement,
   so the archive is not written or read here rather than risk restoring a
   stale pointer.  Original Jaguar EEPROM save slots are fully supported. */
int avp_pc_quickstate_disk_save(const AvpPcQuickState*q,const AvpRomImage *rom,unsigned level,const void *map,size_t n){(void)q;(void)rom;(void)level;(void)map;(void)n;return 0;}
int avp_pc_quickstate_disk_load(AvpPcQuickState*q,const AvpRomImage *rom,const AvpMazeCandidate *m,unsigned count,void *map,size_t n){(void)q;(void)rom;(void)m;(void)count;(void)map;(void)n;return 0;}
