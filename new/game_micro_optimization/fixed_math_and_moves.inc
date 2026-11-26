/* R403: PC-only alternatives to the existing original-game entry points.
 * The caller must opt in. No state, approximation, cache or reordered RNG. */
void __real_Vector(const AvpXY *from,const AvpXY *to,s32 *vx,s32 *vy);
u16 __real_NMoves(u16 x,u16 y,u8 dir,u16 wanted);
void __wrap_Vector(const AvpXY *from,const AvpXY *to,s32 *vx,s32 *vy)
{
    if (!enabled) { __real_Vector(from,to,vx,vy); return; }
    /* The generated R402 collision.c uses wrapping 68000 SUB.L. */
    u32 dx=(u32)to->x-(u32)from->x, dy=(u32)to->y-(u32)from->y;
    const int nx=(dx>>31)!=0, ny=(dy>>31)!=0;
    u32 x=nx?0u-dx:dx, y=ny?0u-dy:dy;
    const u32 bits=x|y;
    if (!bits) { *vx=0x4000; *vy=0; return; }
    /* The original loop stops when either bit 30 becomes set. For
     * magnitudes below bit 31, its count is exactly clz(bits)-1.
     * INT32_MIN magnitudes retain the original unusual shift behavior. */
    if (bits&0x80000000u) { __real_Vector(from,to,vx,vy); return; }
    const unsigned shift=(unsigned)__builtin_clz(bits)-1u;
    x=(x<<shift)>>1; y<<=shift;
    const u32 xx=x>>16, yy=y>>16;
    const u32 den=(((xx*xx)+(yy*yy))>>16)+0x3000u;
    x=(x>>2)/den; y=(y>>2)/den;
    *vx=nx?-(s32)(u16)x:(s32)(u16)x;
    *vy=ny?-(s32)(u16)y:(s32)(u16)y;
}

u16 __wrap_NMoves(u16 x,u16 y,u8 dir,u16 wanted)
{
    if (!enabled) return __real_NMoves(x,y,dir,wanted);
    if (!wanted) return 0;
    const unsigned w=micro_width, h=micro_height;
    const u8 *map=micro_maze;
    if (!map || x>=w || y>=h) return 0;
    /* Bound the proof to ordinary Jaguar-size maps. Larger custom maps
     * still use the existing implementation. No cached wall state. */
    if (w>64u || h>64u) return __real_NMoves(x,y,dir,wanted);
    const unsigned d=dir&3u;
    unsigned limit;
    int step;
    switch(d) {
    case 0: limit=x;   step=-8; break;
    case 1: limit=y;   step=-(int)(w*8u); break;
    case 2: limit=w-x; step=8; break;
    default:limit=h-y; step=(int)(w*8u); break;
    }
    /* Left/top do not count a step out of cell 0; right/bottom do count
     * the final outgoing step. Preserve that asymmetric original rule. */
    if (limit>wanted) limit=wanted;
    if (!limit) return 0;
    const u8 *p=map+((unsigned)y*w+x)*8u+d;
    unsigned n=0;
    for (;;) {
        const unsigned wall=*p;
        if (wall>=1u && wall<=31u) break;
        if (++n==limit) break;
        p+=step;
    }
    return (u16)n;
}
