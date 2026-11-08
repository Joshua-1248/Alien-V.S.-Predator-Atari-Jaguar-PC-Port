/* Retail FONT.S on the PC host: typewriter controls belong to the original
 * CPU text interpreter, not an instant GPU-side copy of the final record. */
#include "font.h"
#include "computer.h"
#include "joypad.h"
#include "pc/pc_platform.h"
#include "avp_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned frames, drawn, sfx, previous_x, previous_y;
static void vblank(void *u){(void)u;++frames;assert(frames<256u);}
static void glyph(void *u,unsigned brush,unsigned source_x,unsigned width,
                  unsigned x,unsigned y,int clear)
{
    (void)u;(void)brush;(void)source_x;(void)width;
    if(!clear){++drawn;previous_x=x;previous_y=y;}
}
static void sound(void *u,unsigned effect){(void)u;(void)effect;++sfx;}
int main(void)
{
    unsigned char brush[9u*64u]={0};
    const char record[]={0,32,0,48,(char)0xfe,'A','B',(char)0xfd,'C',0};
    AvpRuntimeOps ops;
    for(unsigned col=0;col<170u;col+=2u)
        brush[col/4u]|=(unsigned char)(3u<<((3u-col%4u)*2u));
    memset(&ops,0,sizeof(ops));
    ops.wait_vblank=vblank;ops.draw_font_glyph=glyph;ops.play_sfx=sound;
    avp_runtime_bind(&ops);
    initpad();avp_pc_pad_set(0xffffffffu);
    avp_font_bind_brushes(brush,NULL,NULL,sizeof(brush));
    init_font();
    pr_string(record);
    assert(frames==6u); /* two source characters, 3 VBlanks each */
    assert(drawn==9u); /* 3 glyphs plus 6 flashing cursor draws */
    assert(sfx==1u); /* original alternate-character typewriter sound */
    assert(previous_y==48u&&previous_x>32u);
    puts("Retail FONT.S per-character timing and text SFX PASS.");
    return 0;
}
