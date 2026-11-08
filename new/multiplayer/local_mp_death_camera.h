#ifndef AVP_LOCAL_MP_DEATH_CAMERA_H
#define AVP_LOCAL_MP_DEATH_CAMERA_H
#include <stdint.h>
/* Preserve the last *simulation-owned* pose at the death event. In the
 * 60-Hz presentation path never interpolate back to the pre-death 10-Hz pose,
 * or continue a pending host quick turn, while the original world, enemies,
 * projectiles and surviving players continue their normal simulation. */
typedef struct AvpMpDeathCamera {
    uint32_t x,y,angle;
    unsigned active;
} AvpMpDeathCamera;
static inline void avp_mp_death_camera_begin(AvpMpDeathCamera *camera,
    uint32_t x,uint32_t y,uint32_t angle)
{
    if(camera && !camera->active){
        camera->x=x;camera->y=y;camera->angle=angle;camera->active=1;
    }
}
static inline void avp_mp_death_camera_end(AvpMpDeathCamera *camera)
{if(camera)camera->active=0;}
static inline void avp_mp_death_camera_apply(const AvpMpDeathCamera *camera,
    uint32_t *x,uint32_t *y,uint32_t *angle)
{
    if(camera&&camera->active){
        if(x)*x=camera->x;
        if(y)*y=camera->y;
        if(angle)*angle=camera->angle;
    }
}
#endif
