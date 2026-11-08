/* The five Predator retail modes include both TOM RGB16 and RGB24.
 * A 24-bit pixel consumes two original 16-bit line-buffer words; host
 * resolution must not turn that into independent 16-bit pixels. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

s16 player_type=PT_PREDATOR;
u8 invis_stat=2;
u16 pred_vision_vmode;

int main(void)
{
    AvpAvpGpuState state={0};
    AvpJaguarFramebuffer fb={0};
    u8 *q=(u8*)calloc(131072u,1u);
    u32 pixels[320u]={0};
    assert(q);
    state.qconv=q;state.qconv_size=131072u;
    fb.pixels=pixels;fb.width=320;fb.height=1;
    pixels[0]=0x00ff0000u;pixels[1]=0x0000ff00u;
    pred_vision_vmode=0x0ec3u;
    present_predator_vmode(&state,&fb);
    assert(pixels[0]==0x00ff00ffu && pixels[1]==0x00ff00ffu);
    /* With full native 320x228 geometry the same pair occupies two
     * output columns; no arbitrary host-channel overlay is applied. */
    memset(pixels,0,sizeof(pixels));pixels[0]=0x00ff0000u;
    pred_vision_vmode=0x06c7u;
    present_predator_vmode(&state,&fb);
    assert(pixels[0]==avp_jaguar_vmode_rgb16(0x00ffu));
    assert(pixels[1]==0u);
    /* Original normal and invisible-off rendering must stay unchanged. */
    pixels[0]=0x00112233u;pred_vision_vmode=0;
    present_predator_vmode(&state,&fb);assert(pixels[0]==0x00112233u);
    pred_vision_vmode=0x06c7u;invis_stat=0;
    present_predator_vmode(&state,&fb);assert(pixels[0]==0x00112233u);
    player_type=PT_HUMAN;invis_stat=2;
    present_predator_vmode(&state,&fb);assert(pixels[0]==0x00112233u);
    free(q);
    puts("Jaguar Predator RGB16/RGB24 video-output regression PASS");
    return 0;
}
