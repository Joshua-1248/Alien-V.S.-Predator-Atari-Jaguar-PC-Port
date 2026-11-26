/* Host equivalents of the Jaguar BLITTER_exact_sourcefit.s transfer helpers.
 * TOM is big-endian. Blits complete synchronously on PC, so WaitBlit need not
 * spin, but byte lanes and the source's word-count truncation/rounding still
 * matter when a blitter routine is called from the translated 68000 code. */
#include "blitter.h"
#include <stdint.h>
#include <string.h>

void WaitBlit(void) {}

void ByteMove(const void *src, void *dst, u32 count)
{
    memmove(dst, src, count);
}

void ByteSet(void *dst, u32 count, u32 pattern)
{
    u8 *d=(u8 *)dst;
    const u8 bytes[4]={ (u8)(pattern>>24), (u8)(pattern>>16),
                        (u8)(pattern>>8), (u8)pattern };
    u32 i;
    for(i=0;i<count;i++)d[i]=bytes[i&3u];
}

/* BLITTER_exact_sourcefit.s::QuickMove rounds the Jaguar transfer count UP
 * to a full eight-byte block before writing its two-longword B_COUNT width. 
 * As on the original hardware, callers must provide that much source and
 * destination storage; this is not the byte-exact ByteMove interface. */
void QuickMove(const void *src, void *dst, u32 count)
{
    size_t bytes=(size_t)((count+7u)&~7u);
    memmove(dst,src,bytes);
}

/* BLITTER_exact_sourcefit.s::QuickSet uses (count>>3)<<1 longwords, not a
 * byte count. Jaguar stores the long pattern big-endian regardless of host
 * byte order. Use byte writes to avoid unaligned u32 access and strict-alias
 * issues, and do not modify the original's truncated partial blocks. */
void QuickSet(void *dst, u32 count, u32 pattern)
{
    u8 *d=(u8 *)dst;
    const u8 bytes[4]={ (u8)(pattern>>24), (u8)(pattern>>16),
                        (u8)(pattern>>8), (u8)pattern };
    size_t n=((size_t)count>>3)<<3;
    size_t i;
    for(i=0;i<n;i++)d[i]=bytes[i&3u];
}

void QuickClear(void *dst, u32 count)
{
    QuickSet(dst,count,0u);
}
