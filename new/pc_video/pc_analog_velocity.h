#ifndef AVP_PC_ANALOG_VELOCITY_H
#define AVP_PC_ANALOG_VELOCITY_H
#include <stdint.h>
#include <math.h>
/* Same speed metric as PLAYER.S (3*max+min), in native velocity units. */
static inline double avp_pc_analog_metric(double x,double y)
{
    x=fabs(x);y=fabs(y);
    return 3.0*(x>y?x:y)+(x>y?y:x);
}
static inline int32_t avp_pc_analog_approach(int32_t now,int32_t target,int divisor)
{
    const int64_t d=(int64_t)target-now;
    /* Round toward the target: a held stick converges exactly, including
     * negative directions, instead of sticking a few units short. */
    return (int32_t)((int64_t)now+(d>=0?(d+divisor-1)/divisor:(d-divisor+1)/divisor));
}
static inline void avp_pc_analog_velocity(int32_t *vx,int32_t *vy,
    int32_t ca,int32_t sa,double f,double s,
    int32_t dfx,int32_t dfy,int32_t dsx,int32_t dsy,
    uint32_t maximum,int shift)
{
    const double cap=3.0*maximum;
    const double basis=avp_pc_analog_metric(ca,sa);
    const int divisor=1<<(shift>=0&&shift<=15?shift:2);
    const double scale=basis>0.0?cap/basis:0.0;
    /* A nonzero proportional axis owns its component. Digital input on the
     * other axis remains available; a duplicate binding on this axis does
     * not add a second contribution. Clamp external command inputs too. */
    if(f>1.0)f=1.0;else if(f< -1.0)f=-1.0;
    if(s>1.0)s=1.0;else if(s< -1.0)s=-1.0;
    double x=(f!=0.0?ca*f*scale:dfx*(double)divisor)+
             (s!=0.0?sa*s*scale:dsx*(double)divisor);
    double y=(f!=0.0?-sa*f*scale:dfy*(double)divisor)+
             (s!=0.0?ca*s*scale:dsy*(double)divisor);
    const double mag=avp_pc_analog_metric(x,y);
    if(mag>cap && mag>0.0){x*=cap/mag;y*=cap/mag;}
    *vx=avp_pc_analog_approach(*vx,(int32_t)x,divisor);
    *vy=avp_pc_analog_approach(*vy,(int32_t)y,divisor);
}
#endif
