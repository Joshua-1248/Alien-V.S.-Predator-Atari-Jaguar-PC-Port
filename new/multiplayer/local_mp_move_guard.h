#ifndef AVP_NEW_LOCAL_MP_MOVE_GUARD_H
#define AVP_NEW_LOCAL_MP_MOVE_GUARD_H
#include <stdint.h>
/* Multiplayer-only validation layered over, NEVER replacing, the original
 * COLLIDE.S SafePos.  Original Jaguar movement is unmodified. */
typedef int (*AvpMpMovementSample)(int32_t x,int32_t y,void *user);
/* Source player half-width is 40<<9 (0x5000). Two player footprints
 * overlap iff their centres are closer than 2 * that width on BOTH axes. */
#define AVP_MP_PEER_CLEARANCE 0xa000u
#define AVP_MP_PEER_COUNT 16u
typedef struct AvpMpPeerCollisionView {
    uint32_t x[AVP_MP_PEER_COUNT],y[AVP_MP_PEER_COUNT];
    uint8_t active[AVP_MP_PEER_COUNT]; /* only living, same-world opponents */
    unsigned count;
    int32_t from_x,from_y; /* Bound actor's current original 16.16 pose. */
} AvpMpPeerCollisionView;
int avp_mp_move_guard_peers(const AvpMpPeerCollisionView *view,
                            int32_t x,int32_t y);
/* Arrival/respawn exemptions are retained only until a pair has enough
 * independent room. World identity is a host address, not just a level ID. */
typedef struct AvpMpPeerGrace {
    uint16_t ignore[AVP_MP_PEER_COUNT];
} AvpMpPeerGrace;
void avp_mp_peer_grace_update(AvpMpPeerGrace *grace,
    uint32_t present_mask,const uintptr_t worlds[AVP_MP_PEER_COUNT],
    const uint32_t x[AVP_MP_PEER_COUNT],const uint32_t y[AVP_MP_PEER_COUNT],
    uint32_t arriving_mask);
int avp_mp_peer_grace_exempt(const AvpMpPeerGrace *grace,
                             unsigned p,unsigned q);
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
