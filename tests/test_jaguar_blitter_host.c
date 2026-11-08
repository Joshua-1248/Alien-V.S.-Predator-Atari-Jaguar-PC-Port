/* BLITTER_exact_sourcefit.s, QuickSet/QuickMove big-endian transfer parity.
 * Buffers deliberately include the documented original rounded extent. */
#include "blitter.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    uint8_t b[40],s[40];
    unsigned i;
    memset(b,0x77,sizeof b);
    QuickSet(b+1,16u,0x12345678u);
    for(i=0;i<16u;i++){
        const uint8_t expected[]={0x12,0x34,0x56,0x78};
        assert(b[1u+i]==expected[i&3u]);
    }
    assert(b[0]==0x77&&b[17]==0x77);
    memset(b,0x77,sizeof b);
    QuickSet(b,15u,0xa1b2c3d4u);
    for(i=0;i<8u;i++){
        const uint8_t expected[]={0xa1,0xb2,0xc3,0xd4};
        assert(b[i]==expected[i&3u]);
    }
    assert(b[8]==0x77); /* Jaguar floors to one 8-byte block. */
    memset(b,0x77,sizeof b);
    ByteSet(b+3,9u,0x12345678u);
    for(i=0;i<9u;i++){
        const uint8_t expected[]={0x12,0x34,0x56,0x78};
        assert(b[3u+i]==expected[i&3u]);
    }
    memset(b,0x77,sizeof b);
    QuickClear(b+1,15u);
    for(i=1;i<9u;i++)assert(b[i]==0);
    assert(b[9]==0x77);
    for(i=0;i<sizeof s;i++)s[i]=(uint8_t)i;
    memset(b,0xff,sizeof b);
    QuickMove(s,b,9u); /* Original rounded transfer is 16 bytes. */
    assert(memcmp(b,s,16u)==0&&b[16]==0xff);
    memset(b,0xff,sizeof b);
    ByteMove(s,b,9u); /* Original ByteMove transfers 9 bytes exactly. */
    assert(memcmp(b,s,9u)==0&&b[9]==0xff);
    return 0;
}
