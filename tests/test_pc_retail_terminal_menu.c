/* PC-only renderer test: original coordinate-stream menus replace placeholder text. */
#include "jaguar/avp_gpu/software_renderer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    AvpAvpGpuState state;
    unsigned char brush[9u*64u];
    const char sample[]={0,35,0,80,'A','B',(char)0xff,0,0,41,0,100,'C',0};
    memset(&state,0,sizeof(state));
    memset(brush,0,sizeof(brush));
    /* Minimal test-only proportional FONT.S-style bitmap: every printable
     * glyph is one 2bpp ink column separated by one clear column. */
    for(unsigned col=0;col<170u;col+=2u){
        unsigned off=col/4u,shift=(3u-col%4u)*2u;
        brush[off]|=(unsigned char)(3u<<shift);
    }
    state.computer_active=1;
    state.computer_font[0]=brush;
    state.computer_font_size[0]=sizeof(brush);
    /* R99 displayed completed PC fallback text before the first original
     * typewriter glyph.  With retail font assets, event metadata must not
     * stamp a second (and wrongly timed) textual image into the CTEXT layer. */
    avp_gpu_computer_event(&state,0x47454e4du,2,0);
    assert(state.computer_menu_tag==0x47454e4du);
    assert(state.computer_pixels[42u*320u+42u]==0u);
    avp_gpu_computer_event(&state,0x46414944u,1,750);
    assert(state.computer_pixels[88u*320u+52u]==0u);
    state.computer_pixels[0]=0xffffffffu; /* overwritten by authentic menu */
    state.computer_menu_tag=0x47454e4du;
    avp_gpu_computer_draw_retail_menu(&state,sample,0x2651cu);
    assert(state.computer_menu_tag==0x47454e4du);
    assert(state.computer_pixels[0]==0u);
    assert(state.computer_pixels[80u*320u+35u]);
    assert(state.computer_pixels[80u*320u+37u]);
    assert(state.computer_pixels[100u*320u+41u]);
    assert(!state.computer_pixels[101u*320u+41u]);
    /* The source glyph record at RDB.DTA $265AE begins at (35,80),
     * not the former PC-only (32,80).  A cursor change must erase the old
     * marker and draw the new one on the next 20-pixel menu row. */
    /* Cartridge glyph clearing must not wipe arbitrary neighbouring CTEXT.
     * R101 cleared x30..45 on all rows 75..144 even if those pixels belonged
     * to a line/overlay rather than the cursor. */
    state.computer_pixels[90u*320u+40u]=0xff112233u;
    state.computer_pixels[120u*320u+32u]=0xff445566u;
    avp_gpu_computer_event(&state,0x43555253u,0,0);
    assert(state.computer_pixels[90u*320u+40u]==0xff112233u);
    assert(state.computer_pixels[120u*320u+32u]==0xff445566u);
    assert(state.computer_pixels[80u*320u+35u]);
    assert(state.computer_pixels[80u*320u+32u]==0u);
    avp_gpu_computer_event(&state,0x43555253u,1,0);
    assert(state.computer_pixels[80u*320u+35u]==0u);
    assert(state.computer_pixels[100u*320u+35u]);
    /* FILES 102 ends with the 4-entry CTEXT CLUT group (252..255).
     * Pixel values 1,2,3 index 253,254,255; zero remains transparent. */
    state.computer_palette_valid=1;
    state.computer_palette[253]=0x00112233u;
    state.computer_palette[254]=0x00445566u;
    state.computer_palette[255]=0x00778899u;
    brush[0]=(unsigned char)((1u<<6)|(2u<<4)|(3u<<2));
    avp_gpu_computer_draw_glyph(&state,0,0,4,80,160,0);
    assert(state.computer_pixels[160u*320u+80u]==0xff112233u);
    assert(state.computer_pixels[160u*320u+81u]==0xff445566u);
    assert(state.computer_pixels[160u*320u+82u]==0xff778899u);
    assert(state.computer_pixels[160u*320u+83u]==0u);
    {
        const char top[]={0,24,0,150,'A',0};
        const char footer[]={0,100,0,(char)200,'B',0};
        avp_gpu_computer_draw_retail_menu(&state,top,0x25ea0u);
        assert(state.computer_pixels[150u*320u+24u]);
        avp_gpu_computer_draw_retail_overlay(&state,footer,0x259feu);
        assert(state.computer_pixels[150u*320u+24u]);
        assert(state.computer_pixels[200u*320u+100u]);
    }
    /* R102 regression: CLS already erased the old cursor.  A new screen may
     * legitimately contain source text at the old cursor's coordinates.
     * Its first CURS event must not erase that new text. */
    avp_gpu_computer_event(&state,0x43555253u,2,0);
    assert(state.computer_pixels[120u*320u+35u]);
    avp_gpu_computer_event(&state,0x434c53u,0,0);
    assert(state.computer_cursor==-1);
    state.computer_pixels[120u*320u+35u]=0xffaabbccu;
    state.computer_pixels[120u*320u+36u]=0xff112244u;
    avp_gpu_computer_event(&state,0x43555253u,0,0);
    assert(state.computer_pixels[120u*320u+35u]==0xffaabbccu);
    assert(state.computer_pixels[120u*320u+36u]==0xff112244u);
    /* A menu event also starts a new CTEXT record; it must discard the
     * previous cursor's footprint before the next selection draw. */
    avp_gpu_computer_event(&state,0x47454e4du,2,0);
    assert(state.computer_cursor==-1);
    state.computer_pixels[80u*320u+35u]=0xffeecc99u;
    avp_gpu_computer_event(&state,0x43555253u,1,0);
    assert(state.computer_pixels[80u*320u+35u]==0xffeecc99u);
    assert(state.computer_pixels[100u*320u+35u]!=0u);
    /* Repeated updates and genuine movements still retire a drawn cursor. */
    avp_gpu_computer_event(&state,0x43555253u,2,0);
    assert(state.computer_pixels[100u*320u+35u]==0u);
    assert(state.computer_pixels[120u*320u+35u]!=0u);
    puts("Original terminal coordinate-stream rasterization and overlays PASS.");
    return 0;
}
