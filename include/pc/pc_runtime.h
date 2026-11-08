#ifndef AVP_PC_RUNTIME_H
#define AVP_PC_RUNTIME_H
#include "pc/pc_platform.h"
void avp_pc_bind_runtime(void *user);
void avp_pc_runtime_set_rom(const AvpRomImage *rom);
void avp_pc_runtime_set_noclip(int enabled);
#ifdef AVP_LOCAL_MULTIPLAYER
void avp_pc_runtime_set_mp_bot_floor_guard(int enabled);
void avp_pc_runtime_set_mp_player_floor_guard(int enabled);
void avp_pc_runtime_set_mp_fast_sweep_guard(int enabled);
#endif
int avp_pc_runtime_get_noclip(void);
void avp_pc_runtime_set_gameplay_vblank(int active);
/* Optional host-only modal input ownership; -1 disables multiplayer routing. */
void avp_pc_runtime_set_mp_terminal(int player,unsigned players);
#endif
