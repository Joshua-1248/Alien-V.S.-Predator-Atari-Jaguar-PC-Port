/* Original FONT.S line command packing: signed 16-bit coordinates become
 * two's-complement 32-bit longword bits, including negative x/y. */
#include "font.h"
#include "avp_runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned emitted;
static unsigned observed_tag;
static u32 observed_xy;
static s32 observed_extent;
static void capture(void *u,unsigned event,s32 xy,s32 extent)
{
    (void)u; ++emitted;observed_tag=event;
    observed_xy=(u32)xy;observed_extent=extent;
}
static void expect(unsigned tag, s16 x, s16 y, s16 length)
{
    unsigned before=emitted;
    u32 packed=((u32)(u16)x<<16)|(u32)(u16)y;
    if(tag==0x484cu)horiz_line(length);
    else if(tag==0x564cu)vert_line(length);
    else assert(0);
    assert(emitted==before+1u);
    assert(observed_tag==tag);
    assert(observed_xy==packed);
    assert(observed_extent==length);
}
int main(void)
{
    static const s16 pairs[][2]={
        {0,0},{1,2},{320,228},{-1,0},{-2,-3},{-32768,32767},
        {32767,-32768},{-300,500},{0,-1},{-1,-1}
    };
    AvpRuntimeOps ops={0};ops.frontend_event=capture;
    avp_runtime_bind(&ops);
    for(unsigned i=0;i<sizeof pairs/sizeof pairs[0];++i){
        x_line_pos=pairs[i][0];y_line_pos=pairs[i][1];
        expect(0x484cu,pairs[i][0],pairs[i][1],(s16)(i-5));
        expect(0x564cu,pairs[i][0],pairs[i][1],(s16)(5-(int)i));
    }
    /* box_draw() uses the same four source-sized line events and must retain
     * 16-bit wraparound across sides. Observe all four events independently. */
    {
        const s16 box[4]={-1,-2,6,7};
        unsigned before=emitted;
        box_draw(box);
        assert(emitted==before+4u);
        assert(observed_tag==0x564cu);
        assert(observed_xy==(((u32)(u16)5<<16)|(u32)(u16)(-2)));
        assert(observed_extent==7);
    }
    puts("FONT.S line-event signed-word packing PASS (24 events).");
    return 0;
}
