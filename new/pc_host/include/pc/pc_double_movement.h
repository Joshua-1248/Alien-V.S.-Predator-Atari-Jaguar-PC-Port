#ifndef AVP_PC_DOUBLE_MOVEMENT_H
#define AVP_PC_DOUBLE_MOVEMENT_H
/* Host-only movement multiplier: steps 0..3 represent 1x..4x.
 * Retain the native 1x/2x shift order and the legacy helper ABI. */
#include <stdint.h>
static inline int avp_pc_move_step(int step){return step<0?0:step>3?3:step;}
static inline int avp_pc_move_menu_step(int step,int dir)
{return (avp_pc_move_step(step)+(dir>0?1:3))%4;}
static inline int avp_pc_double_move_shift(int original_shift,int step)
{return step && original_shift>0 ? original_shift-1 : original_shift;}
static inline uint32_t avp_pc_double_move_speed(uint32_t original_max,int step)
{ const unsigned n=(unsigned)avp_pc_move_step(step)+1u;
  return original_max<=UINT32_MAX/n?original_max*n:original_max; }
/* Deathmatch Fast Movement already doubled native max and halved the
 * acceleration divisor. Interpret the cheat as an absolute multiplier;
 * an existing 2x match rule is a floor, never a second multiplier. */
static inline int avp_pc_move_effective_step(int step,int dm_fast)
{step=avp_pc_move_step(step);return dm_fast&&step<1?1:step;}
static inline uint32_t avp_pc_move_max(uint32_t native_max,int step,int dm_fast)
{ const unsigned n=(unsigned)avp_pc_move_effective_step(step,dm_fast)+1u;
  const uint64_t result=(uint64_t)native_max*n/(dm_fast?2u:1u);
  return result<=UINT32_MAX?(uint32_t)result:native_max; }
static inline int32_t avp_pc_move_accel(int32_t raw,int shift,int step,int dm_fast)
{ const int n=avp_pc_move_effective_step(step,dm_fast)+1;
  const int base_shift=shift+(dm_fast?1:0);
  if(n==1)return raw>>base_shift;
  if(n==2)return raw>>(base_shift>0?base_shift-1:0);
  if(n==4)return raw>>(base_shift>1?base_shift-2:0);
  return (int32_t)(((int64_t)raw*3)>>base_shift); }
#endif
