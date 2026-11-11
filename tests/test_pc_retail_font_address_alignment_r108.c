/* A $FF coordinate escape aligns against the original 68000 RDB address,
 * not the malloc address of the PC's inflated resource bytes. */
#include "font.h"
#include "computer.h"
#include "joypad.h"
#include "avp_runtime.h"
#include "pc/pc_platform.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned glyphs;
static unsigned recorded_x[8],recorded_y[8];
static void draw(void *u,unsigned brush,unsigned source_x,unsigned width,
                 unsigned x,unsigned y,int clear)
{
    (void)u;(void)brush;(void)source_x;(void)width;
    if(!clear){
        assert(glyphs<8u);
        recorded_x[glyphs]=x;
        recorded_y[glyphs]=y;
        glyphs++;
    }
}
static void poll_vblank(void *u){(void)u;}

static void attempt(int source_is_odd,int malloc_is_odd)
{
    unsigned char memory[64]={0};
    unsigned char *buffer=memory;
    const u32 source=0x2651cu+(unsigned)source_is_odd;
    unsigned pos=0;
    if(((uintptr_t)buffer&1u)!=(unsigned)malloc_is_odd)buffer++;
    buffer[pos++]=0;buffer[pos++]=10;buffer[pos++]=0;buffer[pos++]=20;
    buffer[pos++]='A';buffer[pos++]=0xff;
    /* Original source address, not host allocation, decides padding. */
    if((source+pos)&1u)buffer[pos++]=0;
    buffer[pos++]=0;buffer[pos++]=72;buffer[pos++]=0;buffer[pos++]=96;
    buffer[pos++]='B';buffer[pos++]=0;
    glyphs=0;
    pr_string_retail((const char *)buffer,source);
    assert(glyphs==2);
    assert(recorded_x[0]==10u&&recorded_y[0]==20u);
    assert(recorded_x[1]==72u&&recorded_y[1]==96u);
}
int main(void)
{
    AvpRuntimeOps ops;
    unsigned char brush[9u*64u]={0};
    memset(&ops,0,sizeof(ops));
    ops.wait_vblank=poll_vblank;ops.draw_font_glyph=draw;
    avp_runtime_bind(&ops);
    initpad();avp_pc_pad_set(0xffffffffu);
    for(unsigned col=0;col<170u;col+=2u)
        brush[col/4u]|=(unsigned char)(3u<<((3u-col%4u)*2u));
    avp_font_bind_brushes(brush,NULL,NULL,sizeof(brush));
    init_font();
    attempt(0,1);attempt(1,0);attempt(0,0);attempt(1,1);
    puts("PASS: historical FONT.S source-address alignment independent of host pointer");
    return 0;
}
