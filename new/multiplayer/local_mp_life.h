#ifndef AVP_LOCAL_MP_LIFE_H
#define AVP_LOCAL_MP_LIFE_H
#include <stddef.h>
#include <stdint.h>
/* Co-op-specific player-local fade. The fade rate 2/60 Hz and 0x80 opaque
 * endpoint follow the original MAZE.S FadeDown / MAZESCRN.S MazeList; the
 * independence of each player is new multiplayer behavior. */
typedef enum AvpMpLifePhase {
    AVP_MP_LIFE_ALIVE=0, AVP_MP_LIFE_FADE_OUT,
    AVP_MP_LIFE_FADE_IN, AVP_MP_LIFE_WAIT_LEVEL
} AvpMpLifePhase;
typedef struct AvpMpLife {
    AvpMpLifePhase phase;
    unsigned fade;
} AvpMpLife;
int avp_mp_life_busy(const AvpMpLife *life);
void avp_mp_life_begin(AvpMpLife *life);
/* Returns 1 exactly when black has been reached and a respawn is required. */
int avp_mp_life_display_step(AvpMpLife *life);
void avp_mp_life_resumed(AvpMpLife *life);
void avp_mp_life_wait(AvpMpLife *life);
void avp_mp_life_apply(const AvpMpLife *life,uint32_t *pixels,
                       unsigned width,unsigned height,unsigned pitch);
#endif
