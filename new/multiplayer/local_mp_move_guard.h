#ifndef AVP_NEW_LOCAL_MP_MOVE_GUARD_H
#define AVP_NEW_LOCAL_MP_MOVE_GUARD_H
#include <stdint.h>
/* Multiplayer-only validation layered over, NEVER replacing, the original
 * COLLIDE.S SafePos.  Original Jaguar movement is unmodified. */
typedef int (*AvpMpMovementSample)(int32_t x,int32_t y,void *user);
/* Test all corners of a player-sized footprint against the actual eight-byte
 * retail floor records.  Coordinates and radius use Jaguar signed 16.16. */
int avp_mp_move_guard_floor(const uint8_t *cells,unsigned width,unsigned height,
                            int32_t x,int32_t y,unsigned radius);
/* Fast Movement's doubled displacement must not jump over intermediate
 * original wall/door tests or an unplayable cell.  The callback returns the
 * original 0=clear / nonzero=blocked collision result.  Intermediate checks
 * are observation-only; this does NOT perform additional movement ticks. */
int avp_mp_move_guard_sweep(int32_t from_x,int32_t from_y,
                             int32_t to_x,int32_t to_y,unsigned step,
                             AvpMpMovementSample sample,void *user);
#endif
