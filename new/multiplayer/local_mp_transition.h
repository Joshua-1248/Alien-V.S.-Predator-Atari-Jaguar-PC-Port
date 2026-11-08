#ifndef AVP_LOCAL_MP_TRANSITION_H
#define AVP_LOCAL_MP_TRANSITION_H
#include <stdint.h>
#include "local_mp.h"

/* Serialized original LEVELS.S exit requests.  One destination is owned by
 * the actor that actually pressed the door/lift, not the shared world.
 * 0 means no pending request. -1 in new_x has the ORIGINAL start-fallback
 * semantics; it is never normalized to arbitrary coordinates here.
 * This is a request queue, NOT a substitute for LoadLevel/LeaveLevel. */
typedef struct AvpMpTransitionRequest {
    int16_t target_level;
    int32_t new_x,new_y;
    uint8_t use_cocoon;
    uint8_t valid;
} AvpMpTransitionRequest;

typedef struct AvpMpTransitionQueue {
    AvpMpTransitionRequest player[AVP_LOCAL_MP_MAX_ACTORS];
} AvpMpTransitionQueue;

/* Refuse to overwrite an unconsumed request, and refuse impossible levels.
 * A player may request the current level for Alien cocoon respawn. */
int avp_mp_transition_queue(AvpMpTransitionQueue *queue,unsigned player,
                             int16_t level,int32_t new_x,int32_t new_y,
                             int use_cocoon);
/* A passenger who independently used the same authored lift/airlock
 * on this world tick is still part of the shared chamber transfer.
 * Geometry, original exit kind and room occupancy are checked by the host.
 * Coordinates are intentionally not compared: source DOORS.S can preserve
 * each actor's different fractional position in its own request. */
int avp_mp_transition_same_destination(const AvpMpTransitionRequest *leader,
                                       const AvpMpTransitionRequest *other);
int avp_mp_transition_peek(const AvpMpTransitionQueue *queue,unsigned player,
                            AvpMpTransitionRequest *out);
int avp_mp_transition_commit(AvpMpTransitionQueue *queue,unsigned player,
                              AvpMpTransitionRequest *out);
#endif
