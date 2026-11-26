/* RE38 R143: the TOM DSTENZ bit requests destination Z reads independently
 * of ZMODE, DSTWRZ, or a preceding comparator inhibiting the color write.
 * Byte bus observations test functional read enable, not bus cycles/timing. */
#include "jaguar/blitter_trm.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SRC 0x1000u
#define DST 0x2000u
#define DSTA2 (1u<<11)
#define DSTENZ (1u<<4)
#define DSTWRZ (1u<<5)
#define ZMODE_LT (1u<<18)
#define BCOMPEN (1u<<26)
#define PATDSEL (1u<<16)
#define PS16 (4u<<3)
#define WIDTH8 (3u<<11)
#define ZOFF1 (1u<<6)
#define PITCH2 1u
#define XADDPIX (1u<<16)

static uint8_t mem[0x4000];
static unsigned z_reads,color_reads,z_writes,color_writes;
static uint8_t read_byte(void *ctx,uint32_t addr)
{
    (void)ctx;
    assert(addr<sizeof mem);
    if(addr>=DST+8u && addr<DST+16u)z_reads++;
    if(addr>=DST && addr<DST+8u)color_reads++;
    return mem[addr];
}
static void write_byte(void *ctx,uint32_t addr,uint8_t value)
{
    (void)ctx;
    assert(addr<sizeof mem);
    if(addr>=DST+8u && addr<DST+16u)z_writes++;
    if(addr>=DST && addr<DST+8u)color_writes++;
    mem[addr]=value;
}
static void W(JagBlitter *b,uint32_t off,uint32_t val)
{ assert(jag_blitter_write(b,off,val)); }
static void check(int reversed,int phrase,int enable_z,int z_compare,
                  int bit_inhibit,int write_z)
{
    JagBlitter b;
    JagBlitBus bus={NULL,read_byte,write_byte};
    uint32_t dstflags=PS16|WIDTH8|ZOFF1|PITCH2|(phrase?0u:XADDPIX);
    uint32_t srcflags=dstflags;
    uint32_t cmd=PATDSEL|(reversed?DSTA2:0u)|
        (enable_z?DSTENZ:0u)|(z_compare?ZMODE_LT:0u)|
        (bit_inhibit?BCOMPEN:0u)|(write_z?DSTWRZ:0u);
    memset(mem,0,sizeof mem);
    z_reads=color_reads=z_writes=color_writes=0u;
    for(unsigned j=0;j<4u;j++){
        mem[DST+2u*j]=0x77u;
        mem[DST+2u*j+1u]=0x77u;
        mem[DST+8u+2u*j]=0x55u;
        mem[DST+8u+2u*j+1u]=0x55u;
    }
    jag_blitter_init(&b,bus);
    W(&b,0x00,reversed?SRC:DST);
    W(&b,0x04,reversed?srcflags:dstflags);
    W(&b,0x0C,0u);
    W(&b,0x24,reversed?DST:SRC);
    W(&b,0x28,reversed?dstflags:srcflags);
    W(&b,0x30,0u);
    W(&b,0x40,0u);W(&b,0x44,0u); /* all bit-comparator bits clear */
    W(&b,0x58,0x33333333u);W(&b,0x5C,0x33333333u);
    W(&b,0x50,0x55555555u);W(&b,0x54,0x55555555u);
    W(&b,0x68,0xABCDABCDu);W(&b,0x6C,0xABCDABCDu);
    W(&b,0x3C,(1u<<16)|4u);
    W(&b,0x38,cmd);
    /* Every enabled read is independent of whether a comparator subsequently
     * rejects the pixel or Z writing is selected. Four 16-bit Z pixels. */
    assert(z_reads==(enable_z?8u:0u));
    assert(z_writes==(write_z&&!bit_inhibit&&!z_compare?8u:0u));
    assert(color_reads==0u);
    if(!bit_inhibit&&!z_compare)assert(color_writes==8u);
    if(bit_inhibit&&!phrase)assert(color_writes==0u);
    assert(b.blits==1u&&!b.stopped);
}
int main(void)
{
    unsigned cases=0u;
    for(int reverse=0;reverse<2;reverse++)
        for(int phrase=0;phrase<2;phrase++)
            for(int z=0;z<2;z++)
                for(int compare=0;compare<2;compare++)
                    for(int inhibit=0;inhibit<2;inhibit++)
                        for(int write_z=0;write_z<2;write_z++){
                            check(reverse,phrase,z,compare,inhibit,write_z);
                            cases++;
                        }
    printf("RE38 R143 DSTENZ independent destination-Z read passed (%u cases)\n",cases);
    return 0;
}
