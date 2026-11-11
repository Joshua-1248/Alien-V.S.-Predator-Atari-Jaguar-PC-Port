/* RE35 R93 — original-ROM FILES 235 live MJP cursor validation.
 * Requires a local user-owned AVP.jag at runtime; no ROM bytes are shipped. */
#include "avp_resources.h"
#include "jaguar/avp_gpu/frontend_renderer.h"
#include "jaguar/avp_gpu/frontend_fade.h"
#include "jaguar/framebuffer.h"
#include "mjp.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char mjp_games[4]="xxx";
static u32 pixel(const AvpJaguarFramebuffer *fb,unsigned x,unsigned y)
{ return fb->pixels[(size_t)y*fb->width+x]; }
int main(int argc,char **argv)
{
    AvpRomImage rom={0};AvpAvpGpuState state={0};AvpJaguarFramebuffer fb={0};
    FILE *f;long len;unsigned char *font=NULL;size_t font_len=0;char tag[5];
    uint8_t rr,gg,bb;u32 green;unsigned mode;
    if(argc!=2){fprintf(stderr,"usage: test_mjp_live_cursor_rom_r93 /path/to/AVP.jag\n");return 2;}
    f=fopen(argv[1],"rb");if(!f)return 2;
    if(fseek(f,0,SEEK_END)||((len=ftell(f))!=4194304L)||fseek(f,0,SEEK_SET)){fclose(f);return 2;}
    rom.size=(size_t)len;rom.data=(u8*)malloc(rom.size);assert(rom.data);
    if(fread(rom.data,1,rom.size,f)!=rom.size){fclose(f);return 2;}fclose(f);
    assert(avp_resources_decode_file(&rom,235u,tag,&font,&font_len));
    assert(font_len==224u*39u*2u);
    /* Decode the exact cartridge font, not the synthetic underscore mistake. */
    for(unsigned y=0;y<13u;y++)for(unsigned x=0;x<7u;x++){
        const unsigned idx='_'-'!',gx=(idx&31u)*7u,gy=(idx>>5)*13u;
        const size_t off=2u*((size_t)(gy+y)*224u+gx+x);
        assert(font[off]==0u && font[off+1u]==0u);
    }
    {const unsigned idx=0x7fu-'!',gx=(idx&31u)*7u,gy=(idx>>5)*13u;
     for(unsigned y=1u;y<13u;y++)for(unsigned x=0;x<7u;x++){
         const size_t off=2u*((size_t)(gy+y)*224u+gx+x);
         assert(font[off]==0x5au && font[off+1u]==0xffu);
     }}
    assert(avp_jaguar_framebuffer_init(&fb,320u,254u));
    avp_gpu_rgb16_unpack(0x5affu,&rr,&gg,&bb);
    green=((u32)rr<<16)|((u32)gg<<8)|bb;
    for(unsigned m=0;m<4u;m++){
        static const unsigned modes[4]={AVP_MJP_INTRO,AVP_MJP_ESCAPE,
            AVP_MJP_SIM_TERMINATED,AVP_MJP_BASE_EXPLODES};
        mode=modes[m];avp_gpu_frontend_enter_mode(&state,mode);
        state.mjp_font=font;state.mjp_font_size=font_len;
        avp_gpu_frontend_event(&state,0x4d4a5043u,32,0);
        avp_gpu_frontend_present(&state,&fb,mode,0);
        const unsigned y=(mode==AVP_MJP_INTRO||mode==AVP_MJP_ESCAPE||
            mode==AVP_MJP_BASE_EXPLODES)?180u:14u;
        assert(pixel(&fb,32u,y)==0u); /* transparent first cursor row */
        assert((pixel(&fb,32u,y+1u)&0xffffffu)==green);
        avp_gpu_frontend_event(&state,0x4d4a5043u,39,0);
        avp_gpu_frontend_present(&state,&fb,mode,0);
        assert(pixel(&fb,32u,y+1u)==0u);
        assert((pixel(&fb,39u,y+1u)&0xffffffu)==green);
        /* The final text hold MUST preserve the printed glyphs and hide the
         * square without a scene-clear event. The old test only cleared
         * CLR1 and therefore failed to catch R93's stuck square. */
        avp_gpu_frontend_draw_text(&state,0,"A",24);
        avp_gpu_frontend_event(&state,0x4d4a505au,0,0); /* MJPZ */
        avp_gpu_frontend_present(&state,&fb,mode,0);
        assert(pixel(&fb,39u,y+1u)==0u);
        assert(state.frontend_text_cursor_active==0u);
        assert(state.frontend_text[0][0]=='A');
        if(mode==AVP_MJP_BASE_EXPLODES){
            /* R95: original $01DAAE/$01DAB8 place the Marine timer-failure
             * report below the picture at X=$18,Y=$A6 (+14 OP origin).
             * Sample its actual retail glyph pixels, NOT a synthetic font. */
            unsigned ink=0u;
            for(unsigned py=180u;py<193u;py++)for(unsigned px=24u;px<31u;px++)
                ink+=(pixel(&fb,px,py)!=0u);
            assert(ink!=0u);
            for(unsigned py=14u;py<27u;py++)for(unsigned px=24u;px<31u;px++)
                assert(pixel(&fb,px,py)==0u);
        }
        /* Skip uses the exact same cursor-retirement event. */
        avp_gpu_frontend_event(&state,0x4d4a5043u,39,0);
        avp_gpu_frontend_event(&state,0x4d4a505au,0,0);
        avp_gpu_frontend_present(&state,&fb,mode,0);
        assert(pixel(&fb,39u,y+1u)==0u);
        avp_gpu_frontend_event(&state,0x434c5231u,0,0); /* next scene */
    }
    free(font);font=NULL;state.mjp_font=NULL;
    avp_jaguar_framebuffer_destroy(&fb);free(rom.data);
    puts("PASS: original-ROM MJP 0x7F green cursor visible during progressive text in four scenes");
    return 0;
}
