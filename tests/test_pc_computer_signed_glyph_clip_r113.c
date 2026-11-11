/* R113: computer FONT.S draws from signed 16-bit Jaguar coordinates.
 * The host callback transports those words in unsigned parameters.  A brush
 * crossing x=0/y=0 must be clipped rather than discarded or wrapped.
 * Synthetic font data only: no cartridge bytes required. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void filled_columns(u8 *brush,unsigned count,unsigned period)
{
    memset(brush,0,9u*64u);
    for(unsigned y=0;y<9u;y++)for(unsigned x=0;x<count;x++){
        if(x%period==period-1u)continue;
        brush[y*64u+x/4u]|=(u8)(3u<<((3u-x%4u)*2u));
    }
}
static unsigned pix(const AvpAvpGpuState *s,unsigned x,unsigned y)
{
    return s->computer_pixels[(size_t)y*320u+x];
}
int main(void)
{
    AvpAvpGpuState *state=calloc(1,sizeof(*state));
    u8 brush[9u*64u];
    assert(state);
    state->computer_active=1;
    state->computer_font[0]=brush;
    state->computer_font_size[0]=sizeof(brush);
    filled_columns(brush,4u,5u);

    /* The original Jaguar signed X=-2 leaves only two of four columns. */
    avp_gpu_computer_draw_glyph(state,0u,0u,4u,(unsigned)(u16)(s16)-2,20u,0);
    assert(pix(state,0u,20u)!=0u && pix(state,1u,20u)!=0u);
    assert(pix(state,2u,20u)==0u && pix(state,319u,20u)==0u);

    computer_clear(state);
    avp_gpu_computer_draw_glyph(state,0u,0u,4u,319u,20u,0);
    assert(pix(state,319u,20u)!=0u && pix(state,0u,20u)==0u);

    /* Signed Y=-3 clips the top three brush rows, retaining six. */
    computer_clear(state);
    avp_gpu_computer_draw_glyph(state,0u,0u,4u,30u,(unsigned)(u16)(s16)-3,0);
    assert(pix(state,30u,0u)!=0u && pix(state,30u,5u)!=0u);
    assert(pix(state,30u,6u)==0u && pix(state,30u,239u)==0u);

    /* The complete retail coordinate-stream parser must use identical
     * clipping even when its record begins just left of the CTEXT viewport.
     * Make 33 synthetic four-column brushes, so 'A' (index 32) exists. */
    filled_columns(brush,132u,4u);
    {
        const char record[]={(char)0xff,(char)0xff,0,20,'A',0};
        computer_clear(state);
        avp_gpu_computer_draw_retail_menu(state,record,0x2651cu);
        assert(pix(state,0u,20u)!=0u && pix(state,1u,20u)!=0u);
        assert(pix(state,2u,20u)==0u && pix(state,319u,20u)==0u);
    }
    free(state);
    puts("PASS: signed Jaguar FONT.S glyph clipping in live and resource paths");
    return 0;
}
