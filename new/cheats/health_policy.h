#ifndef AVP_NEW_HEALTH_POLICY_H
#define AVP_NEW_HEALTH_POLICY_H
#include <stdint.h>
#include <limits.h>
/* Preserve the original Pain() delta across a PC-only refill.  The pending
 * loss travels in the actor-owned old_energy, including across MP scopes.
 * Saturate before converting to the Jaguar signed word. */
static inline void avp_new_refill_with_pain(int16_t *energy,int16_t *previous,int16_t maximum)
{
    int32_t loss=(int32_t)*previous-(int32_t)*energy;
    int32_t pending=(int32_t)maximum+(loss>0?loss:0);
    if(pending>INT16_MAX)pending=INT16_MAX;
    *energy=maximum;
    *previous=(int16_t)pending;
}
typedef enum AvpNewHealthMode {
    AVP_NEW_HEALTH_OFF=0, AVP_NEW_HEALTH_INFINITE=1, AVP_NEW_HEALTH_INVINCIBLE=2
} AvpNewHealthMode;
static inline void avp_new_health_apply(AvpNewHealthMode mode,int16_t *energy,int16_t *previous,int16_t maximum)
{
    if(mode==AVP_NEW_HEALTH_INFINITE)avp_new_refill_with_pain(energy,previous,maximum);
    /* Invincibility blocks damage at its source; never refill health here. */
}
/* Only Infinite Health refills before the native actor phase. */
static inline void avp_new_health_before_actor(AvpNewHealthMode mode,int16_t *energy,int16_t *previous,int16_t maximum)
{
    avp_new_health_apply(mode,energy,previous,maximum);
}
struct AvpPcVideo;
AvpNewHealthMode avp_pc_video_new_health_mode(const struct AvpPcVideo *v,unsigned player);
void avp_pc_video_new_health_reset(struct AvpPcVideo *v);
void avp_dev_cheats_set_health_mode(AvpNewHealthMode mode);
void avp_dev_cheats_health_tick(void);
void avp_dev_cheats_before_actor(void);
#endif
