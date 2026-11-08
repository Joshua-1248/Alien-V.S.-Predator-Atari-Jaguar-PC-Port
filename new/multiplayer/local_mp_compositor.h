#ifndef AVP_NEW_LOCAL_MP_COMPOSITOR_H
#define AVP_NEW_LOCAL_MP_COMPOSITOR_H

#include <stdint.h>
#include "local_mp.h"

/* A source frame is a COMPLETE original PC frame: world + weapon + HUD +
 * messages + character-specific overlays. The compositor never extracts,
 * relocates or redraws individual HUD elements. Pixels are host XRGB32.
 * Each source must be independently rendered for its designated player.
 * Source/destination pixel buffers must not overlap. */
typedef struct AvpLocalMpSourceFrame {
    const uint32_t *pixels;
    unsigned width, height, pitch; /* pitch in uint32_t pixels */
} AvpLocalMpSourceFrame;

typedef struct AvpLocalMpOutputFrame {
    uint32_t *pixels;
    unsigned width, height, pitch; /* pitch in uint32_t pixels */
} AvpLocalMpOutputFrame;

/* Scale each player's ENTIRE frame into its viewport with separate X/Y
 * nearest-neighbor factors. The classic three-player layout leaves the fourth
 * quadrant at clear_color; the wide-top option uses the full output.
 * Returns zero without modifying output on bad
 * arguments; returns the active player count on success. No game state,
 * sprites, simulation or input routing is performed here. */
unsigned avp_local_mp_compose(const AvpLocalMpSetup *setup,
                              const AvpLocalMpSourceFrame frames[AVP_LOCAL_MP_MAX_PLAYERS],
                              const AvpLocalMpOutputFrame *output,
                              uint32_t clear_color);

/* Paint ONE complete source frame into its existing co-op viewport without
 * altering any other player, HUD, padding, or the unused 3-player quadrant.
 * Intended for original synchronous modal overlays such as COMPUTER.S.
 * Does not clear the destination. Validates bounds before writing. */
int avp_local_mp_overlay_player(const AvpLocalMpSetup *setup,unsigned owner,
                                const AvpLocalMpSourceFrame *frame,
                                const AvpLocalMpOutputFrame *output);

#endif
