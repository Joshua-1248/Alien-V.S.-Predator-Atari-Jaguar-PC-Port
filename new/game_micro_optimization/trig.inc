/* Exact signed sine values for each original 32-angle bucket,
 * splitting the boundary from its interior for reflected quadrants. */
static const s16 exact_sine[4096]={
#include "trig_table.inc"
};
s32 __real_sin_d0(u16 a);
s32 __real_cos_d0(u16 a);
static s32 fast_sin(u16 a){return exact_sine[((unsigned)(a>>5)<<1)|((a&31u)!=0u)];}
s32 __wrap_sin_d0(u16 a){return enabled?fast_sin(a):__real_sin_d0(a);}
s32 __wrap_cos_d0(u16 a){return enabled?fast_sin((u16)(a+0x4000u)):__real_cos_d0(a);}
