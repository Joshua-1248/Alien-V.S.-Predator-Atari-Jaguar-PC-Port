/* R283: the host-resolution Predator vision (TOM VMODE) and MazeList CLUT
 * passes are split across raster workers.  Each row is read and rewritten
 * only by itself, so the threaded output must equal one sequential pass of
 * the same row worker, for every retail vision mode and the CLUT RMW. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

s16 player_type=PT_PREDATOR;
u8 invis_stat=2;
u16 pred_vision_vmode;
u16 fade_c,xtra_c;
s16 fade_level;

int main(void)
{
    enum{W=1280,H=720};
    AvpAvpGpuState state={0};
    AvpJaguarFramebuffer fb={0};
    u8 *q=(u8*)malloc(131072u);
    u32 *a=(u32*)malloc((size_t)W*H*4u),*b=(u32*)malloc((size_t)W*H*4u),*src=(u32*)malloc((size_t)W*H*4u);
    static const u16 modes[5]={0x06c7u,0x0ec3u,0x0ec3u,0x06c7u,0x06c7u}; /* RDB $44C8 */
    unsigned seed=12345u;
    assert(q&&a&&b&&src);
    for(size_t i=0;i<131072u;i++){seed=seed*1103515245u+12345u;q[i]=(u8)(seed>>16);}
    for(size_t i=0;i<(size_t)W*H;i++){seed=seed*1103515245u+12345u;src[i]=(seed>>8)&0xffffffu;}
    state.qconv=q;state.qconv_size=131072u;
    fb.width=W;fb.height=H;
    for(unsigned m=0;m<5u;m++){
        AvpVmodePass pass;
        pred_vision_vmode=modes[m];
        memcpy(a,src,(size_t)W*H*4u);memcpy(b,src,(size_t)W*H*4u);
        fb.pixels=a;present_predator_vmode(&state,&fb);          /* threaded */
        pass.fb=&fb;pass.mode=pred_vision_vmode&6u;pass.q=q;pass.n=131072u;
        avp_pred_vmode_span_bounds(pass.bounds,W,pass.mode==6u?320u:160u);
        avp_pred_vmode_sample_columns(pass.sample_columns,W,pass.mode);
        fb.pixels=b;avp_vmode_rows(&pass,0u,H);                  /* sequential */
        assert(memcmp(a,b,(size_t)W*H*4u)==0);
        assert(memcmp(a,src,(size_t)W*H*4u)!=0);
    }
    {
        AvpClutPass pass;
        fade_c=0x0000u;xtra_c=0x9558u;fade_level=0;
        memcpy(a,src,(size_t)W*H*4u);memcpy(b,src,(size_t)W*H*4u);
        fb.pixels=a;apply_game_clut(&state,&fb);
        pass.fb=&fb;pass.modifier=avp_jaguar_game_clut(fade_c,xtra_c,fade_level);pass.q=q;pass.n=131072u;
        assert(pass.modifier!=0u);
        fb.pixels=b;avp_clut_rows(&pass,0u,H);
        assert(memcmp(a,b,(size_t)W*H*4u)==0);
        assert(memcmp(a,src,(size_t)W*H*4u)!=0);
    }
    puts("R283 threaded Predator VMODE/CLUT passes == sequential PASS");
    free(q);free(a);free(b);free(src);
    return 0;
}
