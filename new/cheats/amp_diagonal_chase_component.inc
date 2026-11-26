/* The original AMP.S adds the entire per-axis speed along BOTH axes when
 * moving diagonally, producing ~sqrt(2) more travel per tick. This optional
 * PC-only fixed-point correction applies to the successful diagonal chase
 * candidate ONLY. Straight-axis fallback, collision checks, timers, heading,
 * animation, projectiles, player movement and the OFF path stay original.
 * 46341/65536 is the nearest Q16 approximation to 1/sqrt(2).
 * Do not use a floating-point calculation in the original tick loop. */
static s32 amp_diagonal_chase_component(s16 speed)
{
    return (s32)(((s64)speed*INT64_C(46341))/INT64_C(65536));
}

