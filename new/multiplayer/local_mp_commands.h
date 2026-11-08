#ifndef AVP_LOCAL_MP_COMMANDS_H
#define AVP_LOCAL_MP_COMMANDS_H

#include "local_mp_worlds.h"
#include "local_mp_input.h"
#include <stdint.h>

/* A platform-neutral, fixed-size input snapshot for one 10-Hz Jaguar tick.
 * X11/Win32 supply active KeySyms, while evdev/Raw Input supply independent
 * mouse deltas and buttons. Controllers must be identified by path, never
 * pooled across all players. No operating-system polling occurs in here. */
#define AVP_MP_COMMAND_KEY_SLOTS 256u
#define AVP_MP_COMMAND_AXES 16u
#define AVP_MP_COMMAND_PADS 4u
typedef struct AvpMpControllerSample {
    char path[AVP_MP_DEVICE_PATH];
    uint32_t buttons;
    int16_t axes[AVP_MP_COMMAND_AXES];
    unsigned axis_count;
} AvpMpControllerSample;

typedef struct AvpMpCommandInput {
    unsigned long keysyms[AVP_MP_COMMAND_KEY_SLOTS]; /* 0 == up */
    AvpMpMouseState mice[AVP_MP_INPUT_PLAYERS];
    AvpMpControllerSample gamepads[AVP_MP_COMMAND_PADS];
} AvpMpCommandInput;

/* Builds all requested commands atomically and consumes just the mouse
 * impulses, not buttons or keys. Inactive player commands are always zero.
 * Failure neither mutates input nor publishes partially built commands.
 * Player 1 keyboard/mouse *legacy* bindings remain in the existing host;
 * this function only uses the dedicated optional physical mouse assignment
 * for P1, avoiding accidental duplication of its normal X11 pointer. */
int avp_mp_commands_take(const AvpMpInputConfig *profiles,
                         AvpMpCommandInput *input, unsigned players,
                         AvpMpPlayerCommand commands[AVP_LOCAL_MP_MAX_PLAYERS]);

/* Clear all transient input on focus loss, disconnect, or menu entry.
 * Does not modify configured bindings. */
void avp_mp_commands_clear(AvpMpCommandInput *input);
#endif
