#include "jaguar/avp_gpu/frontend_renderer.h"
#include "jaguar/avp_gpu/state.h"
#include "jaguar/avp_gpu/frontend_fade.h"
#include "jaguar/framebuffer.h"
#include "mjp.h"
#include "player.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

char mjp_games[4]="xxx";

static u32 expected_rgb16(u8 r,u8 g,u8 b)
{
    u8 rr,gg,bb;
    avp_gpu_rgb16_unpack(avp_gpu_rgb16_pack(r,g,b),&rr,&gg,&bb);
    return ((u32)rr<<16)|((u32)gg<<8)|bb;
}

static void set_cell(u8 *font,unsigned band,unsigned char ch,u16 cry)
{
    unsigned idx=(ch==' ' ? 0x5cu : ch)-0x21u;
    unsigned gx=(idx&31u)*11u,gy=(band+((idx>>5)&1u))*16u;
    if(gx+10u>320u)return;
    for(unsigned y=0;y<16u;y++)for(unsigned x=0;x<10u;x++){
        size_t o=2u*((size_t)(gy+y)*320u+(gx+x));
        font[o]=(u8)(cry>>8);font[o+1]=(u8)cry;
    }
}

static void set_mjp_cell(u8 *font,unsigned char ch,u16 cry)
{
    unsigned idx=(unsigned)ch-0x21u;
    unsigned gx=(idx&31u)*7u,gy=(idx>>5)*13u;
    for(unsigned y=0;y<13u;y++)for(unsigned x=0;x<7u;x++){
        size_t o=2u*((size_t)(gy+y)*224u+(gx+x));
        font[o]=(u8)(cry>>8);font[o+1]=(u8)cry;
    }
}

int main(void)
{
    AvpAvpGpuState st;AvpJaguarFramebuffer fb={0};
    memset(&st,0,sizeof(st));

    avp_gpu_frontend_event(&st,0x4d4a5700u,0,0);
    for(unsigned i=0;i<16u;i++)assert(!avp_gpu_frontend_transition_done(&st,0u));
    assert(avp_gpu_frontend_transition_done(&st,0u));
    avp_gpu_frontend_event(&st,0x4d4a5702u,0,0);
    for(unsigned i=0;i<16u;i++)assert(!avp_gpu_frontend_transition_done(&st,1u));
    assert(avp_gpu_frontend_transition_done(&st,1u));

    assert(avp_jaguar_framebuffer_init(&fb,320,228));
    st.avp_font=(u8*)calloc(320u*96u*2u,1u);assert(st.avp_font);st.avp_font_size=320u*96u*2u;
    /* Existing tests below verify settled Fame score placement. */
    st.frontend_fame_intro_tick=96u;
    /* Distinct intensity in the same glyph across m/p/a bands proves the
       leading authored marker selects 0/2/4 and is not itself rendered. */
    set_cell(st.avp_font,0u,'A',0x1180u);
    set_cell(st.avp_font,2u,'A',0x11a0u);
    set_cell(st.avp_font,4u,'A',0x11c0u);
    /* R180 and R207 original-ROM visual proof: FILES 234 has 10px ink
       glyphs with 11px source origins and 11px output advance. */
    set_cell(st.avp_font,4u,'S',0x11c0u);
    set_cell(st.avp_font,4u,'*',0x11c0u);
    set_cell(st.avp_font,4u,']',0x11c0u); /* right-bracket menu glyph */
    /* FILES 235 starts at ASCII '!' (not the space at 0x20).  Verify
       both correct glyph selection and preserving the first x coordinate
       when CPU Show_Tex sends successive one-character callbacks. */
    st.mjp_font=(u8*)calloc(224u*39u*2u,1u);assert(st.mjp_font);
    st.mjp_font_size=224u*39u*2u;
    set_mjp_cell(st.mjp_font,'S',0x11c0u);
    set_mjp_cell(st.mjp_font,'E',0x11c0u);
    /* The retail B glyph starts at SOURCE x=11, not x=10. A source pixel
       at x=10 must NOT be substituted for the actual B origin. */
    {
        const unsigned idx=(unsigned)'B'-0x21u;
        const unsigned gx=(idx&31u)*11u,gy=(4u+(idx>>5))*16u;
        const size_t off=2u*((size_t)gy*320u+gx);
        st.avp_font[off]=0x11u;st.avp_font[off+1u]=0xc0u;
        avp_gpu_frontend_event(&st,0x464d5350u,0,1); /* Alien large glyph */
        avp_gpu_frontend_draw_text(&st,0u,"B",12u);
        avp_gpu_frontend_present(&st,&fb,3u,0);
        assert(fb.pixels[60u*fb.width+79u]!=0u); /* Fame row uses captured picture-space layout */
        avp_gpu_frontend_event(&st,0x434c5231u,0,0);
    }
    /* A non-glyph source x=10 byte cannot be used for B. Keep the real
     * source x=11 pixel and verify the High Scores row reads that position;
     * this test FAILED under the R205 10px renderer. */
    {
        const unsigned idx=(unsigned)'B'-0x21u;
        const unsigned gy=(4u+(idx>>5))*16u;
        const unsigned real_x=(idx&31u)*11u;
        const size_t real=2u*((size_t)gy*320u+real_x);
        const size_t incorrect=real-2u;
        st.avp_font[real]=0x11u;st.avp_font[real+1u]=0xc0u;
        st.avp_font[incorrect]=0;st.avp_font[incorrect+1u]=0;
        avp_gpu_frontend_event(&st,0x434c5231u,0,0);
        avp_gpu_frontend_event(&st,0x464d5350u,0,1);
        avp_gpu_frontend_draw_text(&st,0u,"B",79u);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
        assert(fb.pixels[60u*fb.width+79u]!=0u);
        avp_gpu_frontend_event(&st,0x434c5231u,0,0);
        /* Rightmost glyph whose real source pixels fit: index 28 at x=308. */
        set_cell(st.avp_font,4u,']',0x11c0u);
        avp_gpu_frontend_event(&st,0x464d5350u,0,1);
        avp_gpu_frontend_draw_text(&st,0u,"]",79u);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
        assert(fb.pixels[60u*fb.width+79u]!=0u);
        avp_gpu_frontend_event(&st,0x434c5231u,0,0);
        /* Reestablish labels for later menu/scoreboard assertions. */
        set_cell(st.avp_font,4u,'S',0x11c0u);
        set_cell(st.avp_font,4u,'*',0x11c0u);
        set_cell(st.avp_font,4u,']',0x11c0u);
    }
    st.frontend_mode=3u;
    avp_gpu_frontend_draw_text(&st,0u,"S",50u);
    avp_gpu_frontend_draw_text(&st,0u,"E",57u);
    assert(st.frontend_text_x[0]==50u);
    avp_gpu_frontend_present(&st,&fb,3u,0);
    assert(fb.pixels[14u*fb.width+50u]!=0u);
    assert(fb.pixels[14u*fb.width+57u]!=0u);
    assert(fb.pixels[14u*fb.width+64u]==0u);
    /* A literal space advances by one 7px cell but has no glyph in the
       cartridge font.  It must not render an underscore/fallback character. */
    avp_gpu_frontend_event(&st,0x434c5231u,0,0);
    avp_gpu_frontend_draw_text(&st,0u,"S E",50u);
    avp_gpu_frontend_present(&st,&fb,3u,0);
    assert(fb.pixels[14u*fb.width+50u]!=0u);
    assert(fb.pixels[14u*fb.width+60u]==0u);
    assert(fb.pixels[14u*fb.width+64u]!=0u);
    /* Explicit Show_Tex positions must survive its @ half-width spacing.
       In the original stream the following letter starts at x=53, not 57. */
    avp_gpu_frontend_event(&st,0x434c5231u,0,0);
    avp_gpu_frontend_draw_text(&st,0u,"S",50u);
    avp_gpu_frontend_draw_text(&st,0u,"E",53u);
    assert(st.frontend_text_glyph_x[0][0]==50u && st.frontend_text_glyph_x[0][1]==53u);
    avp_gpu_frontend_present(&st,&fb,3u,0);
    assert(fb.pixels[14u*fb.width+53u]!=0u);
    assert(fb.pixels[14u*fb.width+60u]==0u);
    avp_gpu_frontend_event(&st,0x434c5231u,0,0);
    assert(!st.frontend_text[0][0]);
    avp_gpu_frontend_draw_text(&st,0u,"E",12u);
    assert(st.frontend_text_x[0]==12u);
    avp_gpu_frontend_event(&st,0x4d4a5042u,0,0);
    assert(!st.frontend_text[0][0]);
    free(st.mjp_font);st.mjp_font=NULL;st.mjp_font_size=0;
    /* FINS/FNCH/FNAM: the editor retains committed six-character name cells
     * and the active cursor independently of the scoreboard's stored name. */
    avp_gpu_frontend_event(&st,0x46494e53u,12000,1);
    assert(st.frontend_fame_editing && st.frontend_fame_edit_slot==1u);
    avp_gpu_frontend_event(&st,0x464e4348u,0,'A');
    avp_gpu_frontend_event(&st,0x464e414du,1,'B');
    assert(st.frontend_fame_edit_name[0]=='A');
    assert(st.frontend_fame_edit_cursor==1u && st.frontend_fame_edit_character=='B');
    avp_gpu_frontend_event(&st,0x464e444eu,0,0);
    assert(!st.frontend_fame_editing && st.frontend_fame_edit_name_valid);
    st.frontend_mode=0u;
    avp_gpu_frontend_present(&st,&fb,0u,0);
    /* The authored title starts at x=40,y=98, after two blank cells.
       The title S and the selected row's cartridge cursor must both draw. */
    assert(fb.pixels[112u*fb.width+(40u+22u)]!=0u);
    /* R280: retail draws the "a*" cursor string at X=$28 ($01F282). */
    assert(fb.pixels[112u*fb.width+40u]!=0u);
    assert(fb.pixels[112u*fb.width+29u]==0u);
    /* The captured retail title keeps its artwork behind the menu, then
     * fades it down rather than replacing it with a black frame. */
    {
        u8 backdrop[3u]={67u,88u,109u};
        st.frontend_img[0u][0u].rgb=backdrop;
        st.frontend_img[0u][0u].width=1;
        st.frontend_img[0u][0u].height=1;
        st.frontend_loaded[0u][0u]=1u;
        avp_gpu_frontend_event(&st,0x5449544cu,0,0); /* TITL splash */
        for(unsigned tick=0u;tick<201u;tick++)
            avp_gpu_frontend_event(&st,0x5449434bu,0,0); /* TICK */
        /* The title plane remains through the CPU's original 141-VBL wait. */
        assert(st.frontend_title_splash==1u);
        assert(st.frontend_title_reveal_tick==128u);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        const u32 bright=fb.pixels[225u*fb.width+310u];
        assert(bright!=0u);
        u32 menu_ink;
        {u8 r,g,b;avp_gpu_rgb16_unpack(0x11c0u,&r,&g,&b);menu_ink=((u32)r<<16)|((u32)g<<8)|b;}
        avp_gpu_frontend_event(&st,0x544d454eu,0,0); /* TMEN */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(fb.pixels[225u*fb.width+310u]==bright); /* same artwork */
        assert(st.frontend_title_menu_pending==1u);
        /* The reference's 13.6 s fading frame has no menu lettering yet.
         * Do not paint the selected cursor until the backdrop is dim. */
        assert(fb.pixels[112u*fb.width+40u]!=menu_ink);
        for(unsigned tick=0u;tick<9u;tick++)
            avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(st.frontend_title_menu_pending==1u);
        assert(fb.pixels[112u*fb.width+40u]!=menu_ink);
        avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(st.frontend_title_menu_dim_phase==6u);
        assert(st.frontend_title_menu_pending==0u);
        assert(fb.pixels[225u*fb.width+310u]!=0u);
        assert(fb.pixels[225u*fb.width+310u]<bright);
        /* R280: menu glyphs are FILES 234 band-4 RGB16 words (the retail
         * lavender gradient), cursor at X=$28; no hand-tuned white luma. */
        assert(fb.pixels[112u*fb.width+40u]==menu_ink);
        /* Video-derived new-game handoff: the completed artwork and its
         * menu fade together; by phase zero the entire framebuffer is black. */
        avp_gpu_frontend_event(&st,0x54534644u,16,0); /* TSFD start */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        const u32 unchanged=fb.pixels[225u*fb.width+310u];
        assert(unchanged!=0u);
        avp_gpu_frontend_event(&st,0x54534644u,8,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(fb.pixels[225u*fb.width+310u]<unchanged);
        assert(fb.pixels[225u*fb.width+310u]!=0u);
        avp_gpu_frontend_event(&st,0x54534644u,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        for(size_t pos=0;pos<(size_t)fb.width*fb.height;++pos)
            assert(fb.pixels[pos]==0u);
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_SELECT);
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
        assert(st.frontend_title_depart_active==0u);

        avp_gpu_frontend_event(&st,0x5449544cu,0,0); /* reenter title */
        for(unsigned tick=0u;tick<83u;tick++)
            avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        /* Letter cells for the legal overlay come from cartridge FILES 235;
           no literal PNG/font pixels are compiled into the PC port. */
        assert(st.frontend_title_splash==1u);
        avp_gpu_frontend_event(&st,0x544d454eu,0,0); /* return to menu */
        st.frontend_img[0u][0u].rgb=NULL;
        st.frontend_loaded[0u][0u]=0u;
    }
    /* Retail title slot 0 is an unscaled 320x280 BITOBJ. Make_TLi +
     * Make_New encodes YPOS=2*Zero_Line=28, equal to VDB=28. The first
     * picture row is native row ZERO; rows 0..227 remain unscaled. */
    {
        u8 *backdrop=(u8*)calloc(320u*280u*3u,1u);
        assert(backdrop);
        for(unsigned y=0u;y<280u;y++)for(unsigned x=0u;x<320u;x++){
            const size_t o=((size_t)y*320u+x)*3u;
            backdrop[o]=(u8)(y%240u);
            backdrop[o+1u]=(u8)(x%240u);
            backdrop[o+2u]=24u;
        }
        st.frontend_img[0u][0u]=(AvpGpuRgbImage){320,280,backdrop,NULL};
        st.frontend_loaded[0u][0u]=1u;
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        /* No logos, menu glyphs or fades interfere with these samples. */
        const u16 first=avp_gpu_rgb16_pack(0u,70u,24u);
        u8 r,g,b;
        /* The retail title BITOBJ begins at the first active row. */
        avp_gpu_rgb16_unpack(first,&r,&g,&b);
        assert(fb.pixels[0u*fb.width+310u]==
               (((u32)r<<16)|((u32)g<<8)|(u32)b));
        avp_gpu_rgb16_unpack(avp_gpu_rgb16_pack(100u,70u,24u),&r,&g,&b);
        assert(fb.pixels[100u*fb.width+310u]==
               (((u32)r<<16)|((u32)g<<8)|(u32)b));
        /* 228px host output line 227 maps to source row 227. */
        avp_gpu_rgb16_unpack(avp_gpu_rgb16_pack(227u,70u,24u),&r,&g,&b);
        assert(fb.pixels[227u*fb.width+310u]==
               (((u32)r<<16)|((u32)g<<8)|(u32)b));
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_CREDITS);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_CREDITS,0);
        /* CREDITS shares this unscaled title origin and dims at 6/16. */
        const u16 faded=avp_gpu_rgb16_fraction(first,6u);
        avp_gpu_rgb16_unpack(faded,&r,&g,&b);
        assert(fb.pixels[0u*fb.width+310u]==
               (((u32)r<<16)|((u32)g<<8)|(u32)b));
        st.frontend_loaded[0u][0u]=0u;
        st.frontend_img[0u][0u].rgb=NULL;
        free(backdrop);
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
    }

    /* The legal notice is an artwork-phase overlay using small FILES 235
     * pixels, never a title-menu or copyright-image replacement. */
    {
        st.mjp_font=(u8*)calloc(224u*39u*2u,1u);assert(st.mjp_font);
        st.mjp_font_size=224u*39u*2u;
        set_mjp_cell(st.mjp_font,'A',0x11e0u);
        avp_gpu_frontend_event(&st,0x5449544cu,0,0);
        for(unsigned i=0u;i<114u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        for(unsigned x=0u;x<fb.width;x++)assert(fb.pixels[182u*fb.width+x]==0u);
        avp_gpu_frontend_event(&st,0x5449434bu,0,0); /* 115: copyright after logo reveal */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        int has_letter=0;
        for(unsigned x=0u;x<fb.width;x++)
            if(fb.pixels[182u*fb.width+x]!=0u)has_letter=1;
        assert(has_letter);
        avp_gpu_frontend_event(&st,0x544d454eu,0,0);
        free(st.mjp_font);st.mjp_font=NULL;st.mjp_font_size=0;
    }
    /* The reference credits scroll over the settled, dimmed title art.
     * No custom host font, video pixels, or menu-page art is embedded. */
    {
        u8 backdrop[3]={72u,93u,114u};
        st.frontend_img[AVP_MJP_TITLE][0].rgb=backdrop;
        st.frontend_img[AVP_MJP_TITLE][0].width=1;
        st.frontend_img[AVP_MJP_TITLE][0].height=1;
        st.frontend_loaded[AVP_MJP_TITLE][0]=1u;
        st.mjp_font=(u8*)calloc(224u*39u*2u,1u);assert(st.mjp_font);
        st.mjp_font_size=224u*39u*2u;
        set_mjp_cell(st.mjp_font,'P',0x11c0u);
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_CREDITS);
        avp_gpu_frontend_event(&st,0x43524247u,0,0); /* CRBG */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_CREDITS,0);
        assert(fb.pixels[220u*fb.width+310u]!=0u); /* backdrop, not black */
        const u32 bg=fb.pixels[220u*fb.width+310u];
        for(unsigned i=0;i<200u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        assert(st.frontend_credits_tick==200u);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_CREDITS,0);
        const int first_x=(int)(fb.width-strlen("Produced by:")*7u)/2;
        const int first_y=(int)fb.height-(200*5)/6;
        assert(fb.pixels[(unsigned)first_y*fb.width+(unsigned)first_x]!=bg);
        avp_gpu_frontend_event(&st,0x4352454eu,0,0); /* CREN */
        assert(!st.frontend_credits_scrolling);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_CREDITS,0);
        assert(fb.pixels[(unsigned)first_y*fb.width+(unsigned)first_x]==bg);
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
        assert(!st.frontend_credits_scrolling);
        st.frontend_img[AVP_MJP_TITLE][0].rgb=NULL;
        st.frontend_loaded[AVP_MJP_TITLE][0]=0u;
        free(st.mjp_font);st.mjp_font=NULL;st.mjp_font_size=0;
    }
    /* Per-species restore slots use each game's m/p/a band, not 'a'
     * for all three slots. Pixel color is determined by the authored art. */
    {
        set_cell(st.avp_font,0u,'R',0x1180u);
        set_cell(st.avp_font,2u,'R',0x11a0u);
        set_cell(st.avp_font,4u,'R',0x11c0u);
        mjp_games[0]='m';mjp_games[1]='p';mjp_games[2]='a';
        avp_gpu_frontend_event(&st,0x53415645u,0,0); /* SAVE */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(fb.pixels[112u*fb.width+62u]!=0u);
        assert(fb.pixels[123u*fb.width+62u]!=0u);
        assert(fb.pixels[148u*fb.width+62u]!=0u);
        mjp_games[0]=mjp_games[1]=mjp_games[2]='x';
        avp_gpu_frontend_event(&st,0x53565854u,0,0); /* SVXT */
    }
    memset(st.frontend_text,0,sizeof(st.frontend_text));
    st.frontend_fame_active=1u;st.frontend_fame_species=2u;
    st.frontend_fame_intro_tick=96u; /* settled-frame legacy test */
    avp_gpu_frontend_draw_text(&st,0u,"A",10u);
    memset(fb.pixels,0,(size_t)fb.width*fb.height*sizeof(*fb.pixels));
    avp_gpu_frontend_present(&st,&fb,3u,0);
    assert(fb.pixels[60u*fb.width+79u]!=0u); /* Fame rows use 24px spacing */


    /* The three FILES portrait images are separate OP objects.  Position,
     * scale and slot order come from the CPU's exact select_track, never a
     * static hero image chosen by the host. */
    {
        u8 predator[32u*32u*3u],human[32u*32u*3u],alien[32u*32u*3u];
        for(unsigned i=0u;i<32u*32u;i++){
            predator[i*3u]=248u;predator[i*3u+1u]=predator[i*3u+2u]=0u;
            human[i*3u+1u]=252u;human[i*3u]=human[i*3u+2u]=0u;
            alien[i*3u+2u]=248u;alien[i*3u]=alien[i*3u+1u]=0u;
        }
        AvpGpuRgbImage *a=&st.frontend_img[1][1];
        AvpGpuRgbImage *b=&st.frontend_img[1][3];
        AvpGpuRgbImage *c=&st.frontend_img[1][5];
        a->rgb=predator;b->rgb=human;c->rgb=alien;
        a->width=b->width=c->width=32;
        a->height=b->height=c->height=32;
        st.frontend_loaded[1][1]=st.frontend_loaded[1][3]=st.frontend_loaded[1][5]=1u;
        avp_gpu_frontend_event(&st,0x53454c45u,0,0);
        /* R282: OP pose Y is the frontend row (row 14 = VDB), Y=32 -> row 32.
         * SELE must not accidentally display the unscaled full-screen
         * portrait while waiting for the three authored SOBJ phrases. */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,0);
        assert(fb.pixels[32u*fb.width+40u]==0u);
        avp_gpu_frontend_event(&st,0x534f424au,(s32)((0u<<16)|(16u<<8)|PT_PREDATOR),(s32)((40u<<16)|32u));
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,0);
        assert(fb.pixels[32u*fb.width+40u]==0u);
        avp_gpu_frontend_event(&st,0x534f424au,(s32)((2u<<16)|(32u<<8)|PT_HUMAN),(s32)((56u<<16)|32u));
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,0);
        assert(fb.pixels[32u*fb.width+56u]==0u);
        avp_gpu_frontend_event(&st,0x534f424au,(s32)((4u<<16)|(16u<<8)|PT_ALIEN),(s32)((80u<<16)|32u));
        avp_gpu_frontend_present(&st,&fb,1u,0);
        assert(fb.pixels[32u*fb.width+40u]==0xf80000u);
        assert(fb.pixels[32u*fb.width+56u]==0x00fc00u);
        assert(fb.pixels[32u*fb.width+80u]==0x0000f8u);
        assert(fb.pixels[32u*fb.width+95u]==0x0000f8u);
        assert(fb.pixels[32u*fb.width+96u]==0u);
        /* Changed ordering and scale take effect on the very next frame. */
        avp_gpu_frontend_event(&st,0x534f424au,(s32)((4u<<16)|(32u<<8)|PT_PREDATOR),(s32)((56u<<16)|32u));
        avp_gpu_frontend_present(&st,&fb,1u,0);
        assert(fb.pixels[32u*fb.width+56u]==0xf80000u);
        a->rgb=b->rgb=c->rgb=NULL;
        st.frontend_loaded[1][1]=st.frontend_loaded[1][3]=st.frontend_loaded[1][5]=0u;
        st.frontend_select_pose_valid[0]=st.frontend_select_pose_valid[1]=st.frontend_select_pose_valid[2]=0u;
    }

    /* Jaguar SELECT programs Ob_Height=239 for its 320x240 portrait.
     * A half-size portrait addresses 239 source rows and displays 120
     * rows: the OP's initial REMAINDER=VSCALE retains the last partial
     * repetition. Row 239 of the decoded image is never fetched. The prior
     * 119-row test rounded the rectangle instead of stepping SCBITOBJ. */
    {
        u8 *portrait=(u8*)malloc(320u*240u*3u);
        AvpGpuRgbImage *im=&st.frontend_img[1][3];
        assert(portrait);
        for(unsigned i=0u;i<320u*240u;i++){
            portrait[i*3u]=248u;
            portrait[i*3u+1u]=portrait[i*3u+2u]=0u;
        }
        im->rgb=portrait;im->width=320;im->height=240;
        st.frontend_loaded[1][3]=1u;
        avp_gpu_frontend_event(&st,0x53454c45u,0,0); /* clear SELECT poses */
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((0u<<16)|(16u<<8)|PT_HUMAN),
            (s32)((10u<<16)|30u));
        /* Remaining two valid OP slots can have zero size: they must still
         * be present for the completed list to become drawable. */
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((2u<<16)|(0u<<8)|PT_HUMAN),0);
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((4u<<16)|(0u<<8)|PT_HUMAN),0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,0);
        assert(fb.pixels[32u*fb.width+25u]==0xf80000u);
        assert(fb.pixels[148u*fb.width+169u]==0xf80000u);
        assert(fb.pixels[149u*fb.width+10u]==0xf80000u); /* partial 120th row */
        assert(fb.pixels[150u*fb.width+10u]==0u); /* exclusive end */
        assert(fb.pixels[30u*fb.width+170u]==0u); /* exclusive 160px end */
        im->rgb=NULL;st.frontend_loaded[1][3]=0u;
        st.frontend_select_pose_valid[0]=st.frontend_select_pose_valid[1]=
            st.frontend_select_pose_valid[2]=0u;
        free(portrait);
    }

    /* Fractional SCBITOBJ source stepping (HSCALE=VSCALE=21/32): the
     * destination is seven pixels high for a ten-line object, not six.
     * The source index advances 0,1,3,4,6,7,9 and must not be re-ratioed
     * against the rounded output rectangle. No retail image is needed. */
    {
        u8 fixture[10u*10u*3u];
        AvpGpuRgbImage *im=&st.frontend_img[1][3];
        for(unsigned y=0;y<10u;y++)for(unsigned x=0;x<10u;x++){
            const unsigned at=3u*(y*10u+x);
            fixture[at]=(u8)(x*20u+1u);
            fixture[at+1u]=(u8)(y*20u+1u);
            fixture[at+2u]=0u;
        }
        im->rgb=fixture;im->width=10;im->height=10;
        st.frontend_loaded[1][3]=1u;
        avp_gpu_frontend_event(&st,0x53454c45u,0,0);
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((0u<<16)|(21u<<8)|PT_HUMAN),(s32)((30u<<16)|35u));
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((2u<<16)|(0u<<8)|PT_HUMAN),0);
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((4u<<16)|(0u<<8)|PT_HUMAN),0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,0);
        for(unsigned y=0;y<7u;y++)for(unsigned x=0;x<7u;x++){
            /* OP source pixels go through RGB16, losing RGB888 low bits. */
            const u32 want=((((x*32u/21u)*20u+1u)&0xf8u)<<16) |
                           ((((y*32u/21u)*20u+1u)&0xfcu)<<8);
            assert(fb.pixels[(35u+y)*fb.width+30u+x]==want); /* pose Y 35 = row 35 */
        }
        assert(fb.pixels[42u*fb.width+30u]==0u);
        assert(fb.pixels[35u*fb.width+37u]==0u);
        im->rgb=NULL;st.frontend_loaded[1][3]=0u;
        memset(st.frontend_select_pose_valid,0,sizeof(st.frontend_select_pose_valid));
    }

    avp_gpu_frontend_event(&st,0x53415645u,0,0);
    assert(st.frontend_title_save==1);
    avp_gpu_frontend_event(&st,0x53565854u,0,0);
    assert(st.frontend_title_save==0&&st.frontend_title_choice==0);

    /* Native-size transmission art follows the original MJP $01DF2C OP
     * object coordinates: X=$12+4*D1 and screen Y=30 for NTSC.  At scene
     * start ($4BED4=0, D1=10) the picture is centred at X=58 (R280). */
    {
        u8 still[192u*144u*3u];memset(still,0,sizeof(still));
        for(unsigned k=0;k<192u*144u;k++)still[k*3u]=248u;
        AvpGpuRgbImage *im=&st.frontend_img[AVP_MJP_INTRO][0];
        im->rgb=still;im->width=192;im->height=144;
        st.frontend_loaded[AVP_MJP_INTRO][0]=1;
        /* Clear previously rendered test messages: $01D846 invokes Clear
         * before publishing the first Intro display object. */
        avp_gpu_frontend_event(&st,0x434c5231u,0,0);
        avp_gpu_frontend_event(&st,0x4d4a5042u,0,0); /* MJPB scene start */
        st.frontend_page=0;
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_INTRO,0);
        assert(fb.pixels[0]==0u); /* 14px text top stays unobstructed. */
        assert(fb.pixels[29u*fb.width+58u]==0u);
        assert(fb.pixels[30u*fb.width+57u]==0u);
        assert(fb.pixels[30u*fb.width+58u]==0xf80000u);
        assert(fb.pixels[173u*fb.width+249u]==0xf80000u);
        assert(fb.pixels[174u*fb.width+58u]==0u);
        assert(fb.pixels[30u*fb.width+250u]==0u);
        im->rgb=NULL;st.frontend_loaded[AVP_MJP_INTRO][0]=0;
    }

    /* Hall-of-Fame names and decimal scores are separate retail columns.
     * A single concatenated text buffer previously drew the score at x=111. */
    avp_gpu_frontend_event(&st,0x434c5231u,0,0);
    avp_gpu_frontend_event(&st,0x464d5350u,0,1);
    avp_gpu_frontend_draw_text(&st,0u,"ABCDEF",45u);
    avp_gpu_frontend_draw_text(&st,0u,"   1220000",128u);
    assert(strcmp(st.frontend_text[0],"ABCDEF")==0);
    assert(strcmp(st.frontend_fame_score[0],"   1220000")==0);
    assert(st.frontend_text_x[0]==45u);
    avp_gpu_frontend_event(&st,0x434c5231u,0,0);
    assert(!st.frontend_text[0][0]&&!st.frontend_fame_score[0][0]);

    /* SGCF must preserve the last SOBJ portrait's exact SCBITOBJ source
     * scanlines at motion tick zero.  A 239-line image scaled by 16/32
     * produces 120 displayed rows, with source rows 0,2,...,238; the old
     * rounded-rectangle sampler incorrectly used rows 0,1,...,237. */
    {
        u8 *normal_pixels=(u8*)calloc(320u*240u*3u,1u);
        static u8 detail_rgb[3]={0u,252u,0u};
        AvpGpuRgbImage *normal=&st.frontend_img[1][5];
        AvpGpuRgbImage *detail=&st.frontend_img[1][6];
        assert(normal_pixels);
        for(unsigned y=0u;y<240u;y++)for(unsigned x=0u;x<320u;x++)
            normal_pixels[3u*(y*320u+x)]=(u8)(8u+8u*(y%30u));
        normal->rgb=normal_pixels;normal->width=320;normal->height=240;
        detail->rgb=detail_rgb;detail->width=detail->height=1;
        st.frontend_loaded[1][5]=st.frontend_loaded[1][6]=1u;
        avp_gpu_frontend_event(&st,0x53454c45u,0,0);
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((0u<<16)|(16u<<8)|PT_ALIEN),(s32)((32u<<16)|24u));
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((2u<<16)|(0u<<8)|PT_HUMAN),0);
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((4u<<16)|(0u<<8)|PT_PREDATOR),0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,0);
        u32 last_card_scanlines[120];
        for(unsigned y=0u;y<120u;y++)
            last_card_scanlines[y]=fb.pixels[(24u+y)*fb.width+32u];
        assert(last_card_scanlines[1]==0x180000u);
        avp_gpu_frontend_event(&st,0x53474346u,PT_ALIEN,0);
        assert(st.frontend_select_cross_tick==0u && st.frontend_fx_phase==0u);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,0);
        for(unsigned y=0u;y<120u;y++)
            assert(fb.pixels[(24u+y)*fb.width+32u]==last_card_scanlines[y]);
        assert(fb.pixels[(24u+120u)*fb.width+32u]==0u);
        normal->rgb=detail->rgb=NULL;
        st.frontend_loaded[1][5]=st.frontend_loaded[1][6]=0u;
        free(normal_pixels);
        avp_gpu_frontend_event(&st,0x53454c45u,0,0);
    }

    /* R194: compare the observed 2026-10-21 BigPEmu SGCF frames against
     * discrete 320x240 scale/position states, rather than a 50-tick
     * floating-point zoom. Original screen captures at 27.133/27.167 and
     * 27.833 seconds show steps 18/32, 19/32 and full-screen respectively. */
    {
        AvpJaguarFramebuffer zoomfb={0};
        static u8 red[320u*240u*3u];
        static u8 green[320u*240u*3u];
        AvpGpuRgbImage *normal=&st.frontend_img[1][5];
        AvpGpuRgbImage *detail=&st.frontend_img[1][6];
        assert(avp_jaguar_framebuffer_init(&zoomfb,320u,254u) /* R282 frontend target: title row 0 = row 14 */);
        for(size_t i=0u;i<320u*240u;i++){
            red[i*3u]=248u;green[i*3u+1u]=252u;
        }
        normal->rgb=red;normal->width=320;normal->height=240;
        detail->rgb=green;detail->width=320;detail->height=240;
        st.frontend_loaded[1][5]=st.frontend_loaded[1][6]=1u;
        avp_gpu_frontend_event(&st,0x53454c45u,0,0);
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((4u<<16)|(21u<<8)|PT_ALIEN),(s32)((60u<<16)|18u));
        avp_gpu_frontend_event(&st,0x53474346u,PT_ALIEN,0);
        avp_gpu_frontend_present(&st,&zoomfb,1u,4);
        assert(zoomfb.pixels[18u*320u+60u]!=0u);
        for(unsigned t=1;t<=4;t++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&zoomfb,1u,4);
        assert(st.frontend_select_cross_tick==4u);
        assert(zoomfb.pixels[42u*320u+69u]==0u);
        assert(zoomfb.pixels[42u*320u+70u]!=0u);
        assert(zoomfb.pixels[41u*320u+70u]==0u);
        for(unsigned t=5;t<=7;t++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&zoomfb,1u,4);
        assert(zoomfb.pixels[40u*320u+64u]==0u);
        assert(zoomfb.pixels[40u*320u+65u]!=0u);
        for(unsigned t=8;t<=46;t++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&zoomfb,1u,4);
        assert(st.frontend_select_cross_tick==46u);
        assert(zoomfb.pixels[14u*320u]!=0u);
        for(unsigned t=47;t<=50;t++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&zoomfb,1u,4);
        assert(zoomfb.pixels[14u*320u]==0x00fc00u);
        assert(zoomfb.pixels[13u*320u]==0u); /* nothing above VDB */
        normal->rgb=detail->rgb=NULL;
        st.frontend_loaded[1][5]=st.frontend_loaded[1][6]=0u;
        avp_jaguar_framebuffer_destroy(&zoomfb);
        avp_gpu_frontend_event(&st,0x53454c45u,0,0);
    }

    /* Regression for R194's actual visible double portrait: a detail
     * pixel at source (0,0) must stay attached to the moving SCBITOBJ's
     * top-left, regardless of its screen x/y.  R194 sampled the incoming
     * detail picture from the *screen* (x,y) and lost this texel. */
    {
        AvpJaguarFramebuffer picture={0};
        u8 *old=(u8*)calloc(320u*240u*3u,1u);
        u8 *incoming=(u8*)calloc(320u*240u*3u,1u);
        assert(old && incoming);
        for(unsigned p=0;p<320u*240u;p++){
            old[p*3u]=248u;
            incoming[p*3u+1u]=252u;
        }
        incoming[0]=248u;incoming[1]=0u;
        AvpGpuRgbImage *a=&st.frontend_img[1][5];
        AvpGpuRgbImage *b=&st.frontend_img[1][6];
        a->rgb=old;a->width=320;a->height=240;
        b->rgb=incoming;b->width=320;b->height=240;
        st.frontend_loaded[1][5]=st.frontend_loaded[1][6]=1u;
        assert(avp_jaguar_framebuffer_init(&picture,320u,254u));
        avp_gpu_frontend_event(&st,0x53454c45u,0,0);
        avp_gpu_frontend_event(&st,0x534f424au,
            (s32)((4u<<16)|(21u<<8)|PT_ALIEN),(s32)((60u<<16)|18u));
        avp_gpu_frontend_event(&st,0x53474346u,PT_ALIEN,0);
        for(unsigned t=0;t<12u;t++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&picture,1u,4);
        assert(st.frontend_fx_phase==16u);
        /* At tick 12 the selected object is 20/32, top left (60,23) in title rows = port row 37.
         * Both colors are addressed by source texel 0,0, not screen 60,23. */
        assert(picture.pixels[37u*picture.width+60u]==0xf80000u);
        assert(picture.pixels[37u*picture.width+61u]==0x00fc00u);
        assert(picture.pixels[37u*picture.width+59u]==0u);
        avp_jaguar_framebuffer_destroy(&picture);
        a->rgb=b->rgb=NULL;
        st.frontend_loaded[1][5]=st.frontend_loaded[1][6]=0u;
        free(old);free(incoming);
        avp_gpu_frontend_event(&st,0x53454c45u,0,0);
    }

    /* The original 50-VBL CPU hold advances portrait geometry.  The
       incoming art must finish changing during the first eight updated
       frames, not remain a 50-frame double-image dissolve.  SGFI still
       performs the separate original terminal dim. */
    {
        static u8 normal_rgb[320u*240u*3u];
        static u8 detail_rgb[320u*240u*3u];
        AvpGpuRgbImage *normal=&st.frontend_img[1][5];
        AvpGpuRgbImage *detail=&st.frontend_img[1][6];
        for(size_t i=0u;i<320u*240u;i++){
            normal_rgb[i*3u]=248u;
            detail_rgb[i*3u+1u]=252u;
        }
        normal->width=detail->width=320;
        normal->height=detail->height=240;
        normal->rgb=normal_rgb;detail->rgb=detail_rgb;
        st.frontend_loaded[1][5]=1u;st.frontend_loaded[1][6]=1u;
        st.frontend_mode=1u;st.frontend_selected_species=PT_ALIEN;

        /* A completed OP card at x=32, y=24 starts small, then grows
         * toward the full picture as the original GPU CROSS executes. */
        st.frontend_select_pose_valid[2]=1u;
        st.frontend_select_pose_species[2]=PT_ALIEN;
        st.frontend_select_pose_scale[2]=16;
        st.frontend_select_pose_x[2]=32;
        st.frontend_select_pose_y[2]=24;
        avp_gpu_frontend_event(&st,0x53474346u,PT_ALIEN,0);
        assert(st.frontend_fx_active&&st.frontend_fx_phase==0u);
        assert(st.frontend_select_zoom_x==32 && st.frontend_select_zoom_y==24);
        assert(st.frontend_select_zoom_w==160 && st.frontend_select_zoom_h==120);
        avp_gpu_frontend_present(&st,&fb,1u,4);
        assert(fb.pixels[0]==0u);
        assert(fb.pixels[40u*fb.width+48u]==0xf80000u);
        /* Multiple host redraws of one Jaguar frame cannot advance fades. */
        assert(st.frontend_fx_phase==0u);
        avp_gpu_frontend_present(&st,&fb,1u,4);
        assert(st.frontend_fx_phase==0u);
        avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        assert(st.frontend_select_cross_tick==1u);
        assert(st.frontend_fx_phase==0u);
        for(unsigned i=1u;i<50u;i++){
            avp_gpu_frontend_present(&st,&fb,1u,4);
            if(i==25u){
                /* The OP rectangle still grows while the art handoff is
                 * already complete; no synthetic red portrait ghosting. */
                const uint32_t mid=fb.pixels[40u*fb.width+50u];
                assert(mid==0x00fc00u);
                assert(st.frontend_fx_phase==16u);
                /* The detailed image may not leak past the moving card. */
                assert(fb.pixels[0]==0u);
            }
            avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        }
        avp_gpu_frontend_present(&st,&fb,1u,4);
        assert(!st.frontend_fx_active&&st.frontend_fx_phase==16u);
        assert(st.frontend_select_cross_tick==50u);
        assert(fb.pixels[14u*fb.width]==0x00fc00u /* full picture from VDB row 14 */);

        avp_gpu_frontend_event(&st,0x53474649u,PT_ALIEN,0);
        assert(st.frontend_fx_active&&st.frontend_fx_phase==16u);
        avp_gpu_frontend_present(&st,&fb,1u,4);
        assert(fb.pixels[14u*fb.width]==0x00fc00u /* full picture from VDB row 14 */);
        assert(st.frontend_fx_phase==16u);
        avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        assert(st.frontend_fx_phase==15u);
        for(unsigned i=0u;i<9u;i++){
            avp_gpu_frontend_present(&st,&fb,1u,4);
            avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        }
        assert(!st.frontend_fx_active&&st.frontend_dim_active&&st.frontend_dim_phase==7u);
        {uint32_t held=fb.pixels[14u*fb.width];
         avp_gpu_frontend_present(&st,&fb,1u,4);
         assert(fb.pixels[14u*fb.width]==held);}

        /* Static arrays are test-owned; prevent state destruction paths from
           treating them as heap-backed decoded images. */
        normal->rgb=NULL;detail->rgb=NULL;
        st.frontend_loaded[1][5]=st.frontend_loaded[1][6]=0u;
    }

    /* Retail $01DF2C (and both later 192x144 object-building phases) writes
     * X=$12+4*D1 and Y=High_Line+$10 (NTSC screen Y=30).  R280: X and the
     * CROSS stage are published by the CPU per VBL (MJPX); the first eight
     * VBLs show the outgoing picture, then GPU stages 1..16 blend.  This is
     * Do_Intro page 1 ($4BED4=-1, $4BED8=10): X=58 for eight VBLs and stages
     * 1..6, then 54..18. */
    {
        u8 before[192u*144u*3u],after[192u*144u*3u];
        uint32_t mixed;
        for(unsigned i=0u;i<192u*144u;i++){
            before[i*3u]=248u;before[i*3u+1u]=before[i*3u+2u]=0u;
            after[i*3u]=0u;after[i*3u+1u]=252u;after[i*3u+2u]=0u;
        }
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_INTRO);
        st.frontend_img[AVP_MJP_INTRO][0]=(AvpGpuRgbImage){192,144,before,NULL};
        st.frontend_img[AVP_MJP_INTRO][1]=(AvpGpuRgbImage){192,144,after,NULL};
        st.frontend_loaded[AVP_MJP_INTRO][0]=1;
        st.frontend_loaded[AVP_MJP_INTRO][1]=1;
        avp_gpu_frontend_event(&st,0x4d4a5050u,0,1); /* MJPP */
        assert(st.frontend_page_cross_active);
        assert(st.frontend_page==1u);
        for(unsigned d5=0u;d5<8u;d5++){
            avp_gpu_frontend_event(&st,0x4d4a5058u,58,0); /* MJPX */
            avp_gpu_frontend_event(&st,0x5449434bu,0,0);  /* host ticks are inert */
            avp_gpu_frontend_present(&st,&fb,AVP_MJP_INTRO,0);
            assert(st.frontend_page_cross_phase==0u);
            assert(fb.pixels[30u*fb.width+58u]==0xf80000u);
            assert(fb.pixels[29u*fb.width+58u]==0u);
            assert(fb.pixels[30u*fb.width+57u]==0u);
            assert(fb.pixels[30u*fb.width+250u]==0u);
            assert(fb.pixels[174u*fb.width+58u]==0u);
        }
        avp_gpu_frontend_event(&st,0x4d4a5058u,58,1);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_INTRO,0);
        mixed=fb.pixels[30u*fb.width+58u];
        assert(mixed!=0xf80000u&&mixed!=0x00fc00u);
        for(unsigned p=2u;p<=16u;p++){
            const int x=18+4*(int)(16u-p<10u?16u-p:10u);
            avp_gpu_frontend_event(&st,0x4d4a5058u,x,(s32)p);
            avp_gpu_frontend_present(&st,&fb,AVP_MJP_INTRO,0);
            assert(fb.pixels[30u*fb.width+(unsigned)x]!=0u);
            assert(fb.pixels[30u*fb.width+(unsigned)x-1u]==0u);
        }
        assert(fb.pixels[30u*fb.width+18u]==0x00fc00u);
        assert(fb.pixels[173u*fb.width+209u]==0x00fc00u);
        assert(fb.pixels[30u*fb.width+210u]==0u);
        assert(fb.pixels[174u*fb.width+18u]==0u);
        avp_gpu_frontend_event(&st,0x4d4a5053u,0,1); /* MJPS */
        assert(!st.frontend_page_cross_active);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_INTRO,0);
        assert(fb.pixels[30u*fb.width+18u]==0x00fc00u);
        assert(fb.pixels[30u*fb.width+58u+192u]==0u);
        /* Cleared $4BE18 operand: closing CROSS fades to black and the
         * picture stays blank after GPU completion. */
        avp_gpu_frontend_event(&st,0x4d4a5050u,1,AVP_MJP_PAGE_BLANK);
        avp_gpu_frontend_event(&st,0x4d4a5058u,58,8);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_INTRO,0);
        mixed=fb.pixels[30u*fb.width+58u];
        assert(mixed!=0u&&mixed!=0x00fc00u);
        avp_gpu_frontend_event(&st,0x4d4a5058u,58,16);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_INTRO,0);
        assert(fb.pixels[30u*fb.width+58u]==0u);
        avp_gpu_frontend_event(&st,0x4d4a5053u,1,AVP_MJP_PAGE_BLANK);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_INTRO,0);
        assert(fb.pixels[30u*fb.width+58u]==0u);
        st.frontend_img[AVP_MJP_INTRO][0].rgb=NULL;
        st.frontend_img[AVP_MJP_INTRO][1].rgb=NULL;
        st.frontend_loaded[AVP_MJP_INTRO][0]=0;
        st.frontend_loaded[AVP_MJP_INTRO][1]=0;
    }

    /* R211: The MJP picture-object builder at retail $01DF2C is SHARED by
     * Do_Intro, Do_Escap and Do_Lose. Each 192x144 FILES bitmap therefore
     * occupies the same OP aperture, rather than being
     * stretched to the host 320x228 viewport in Escape/Game Over.
     * Do_Escap/Do_Lose run $01DF2C with $4BED4=0, so D1=10 and the
     * aperture is (58,30)..(249,173) (R280; R211 assumed X=$12). */
    {
        const unsigned modes[]={AVP_MJP_ESCAPE,AVP_MJP_SIM_TERMINATED,
                                AVP_MJP_BASE_EXPLODES};
        static u8 pixels[192u*144u*3u];
        for(size_t n=0;n<sizeof(pixels);n+=3u){
            pixels[n]=248u;pixels[n+1u]=0u;pixels[n+2u]=0u;
        }
        for(unsigned m=0;m<sizeof(modes)/sizeof(modes[0]);m++){
            const unsigned mode=modes[m];
            avp_gpu_frontend_enter_mode(&st,mode);
            st.frontend_img[mode][0]=(AvpGpuRgbImage){192,144,pixels,NULL};
            st.frontend_loaded[mode][0]=1u;
            avp_gpu_frontend_present(&st,&fb,mode,0);
            assert(fb.pixels[30u*fb.width+58u]==0xf80000u);
            assert(fb.pixels[173u*fb.width+249u]==0xf80000u);
            assert(fb.pixels[29u*fb.width+58u]==0u);
            assert(fb.pixels[30u*fb.width+57u]==0u);
            assert(fb.pixels[30u*fb.width+250u]==0u);
            assert(fb.pixels[174u*fb.width+58u]==0u);
            st.frontend_img[mode][0].rgb=NULL;
            st.frontend_loaded[mode][0]=0u;
        }
        /* The ESCAPE transition shares the picture aperture as well.
         * In R210 this code reached the generic scaled picture sampler. */
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_ESCAPE);
        st.frontend_img[AVP_MJP_ESCAPE][0]=(AvpGpuRgbImage){192,144,pixels,NULL};
        st.frontend_img[AVP_MJP_ESCAPE][1]=(AvpGpuRgbImage){192,144,pixels,NULL};
        st.frontend_loaded[AVP_MJP_ESCAPE][0]=1u;
        st.frontend_loaded[AVP_MJP_ESCAPE][1]=1u;
        avp_gpu_frontend_event(&st,0x4d4a5050u,0,1); /* MJPP */
        avp_gpu_frontend_event(&st,0x4d4a5058u,58,0); /* MJPX, D1=10 */
        assert(st.frontend_page_cross_active);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_ESCAPE,0);
        assert(fb.pixels[30u*fb.width+58u]==0xf80000u);
        assert(fb.pixels[29u*fb.width+58u]==0u);
        assert(fb.pixels[30u*fb.width+57u]==0u);
        assert(fb.pixels[30u*fb.width+250u]==0u);
        assert(fb.pixels[174u*fb.width+58u]==0u);
        st.frontend_img[AVP_MJP_ESCAPE][0].rgb=NULL;
        st.frontend_img[AVP_MJP_ESCAPE][1].rgb=NULL;
        st.frontend_loaded[AVP_MJP_ESCAPE][0]=0u;
        st.frontend_loaded[AVP_MJP_ESCAPE][1]=0u;
        avp_gpu_frontend_event(&st,0x4d4a5053u,0,1);
    }

    /* The retail ROM supplies different normal/detail JPEGs for each
     * SELECT species (Marine FILES 214 versus 213). SELP must present the
     * completed detail image and green FILES-235 lettering, even if a stale
     * carousel flag survives the SGCF/SGFI host handoff. */
    {
        AvpAvpGpuState *q=(AvpAvpGpuState*)calloc(1u,sizeof(*q));
        AvpJaguarFramebuffer shot={0};
        static const char text[]="S\0\0";
        assert(q && avp_jaguar_framebuffer_init(&shot,320u,228u));
        avp_gpu_frontend_enter_mode(q,AVP_MJP_SELECT);
        q->frontend_selected_species=PT_HUMAN;
        q->frontend_loaded[AVP_MJP_SELECT][3]=1u;
        q->frontend_loaded[AVP_MJP_SELECT][4]=1u;
        q->frontend_img[AVP_MJP_SELECT][3].rgb=(u8*)calloc(3u,1u);
        q->frontend_img[AVP_MJP_SELECT][4].rgb=(u8*)calloc(3u,1u);
        assert(q->frontend_img[AVP_MJP_SELECT][3].rgb &&
               q->frontend_img[AVP_MJP_SELECT][4].rgb);
        q->frontend_img[AVP_MJP_SELECT][3].width=q->frontend_img[AVP_MJP_SELECT][4].width=1;
        q->frontend_img[AVP_MJP_SELECT][3].height=q->frontend_img[AVP_MJP_SELECT][4].height=1;
        q->frontend_img[AVP_MJP_SELECT][3].rgb[0]=160u;
        q->frontend_img[AVP_MJP_SELECT][4].rgb[1]=144u;
        q->mjp_font=(u8*)calloc(224u*39u*2u,1u);
        assert(q->mjp_font);
        q->mjp_font_size=224u*39u*2u;
        set_mjp_cell(q->mjp_font,'S',0x5affu);
        q->frontend_select_detail=0u;
        avp_gpu_frontend_select_briefing(q,text,NULL);
        assert(q->frontend_select_detail==1u);
        avp_gpu_frontend_event(q,0x5449434bu,0,0); /* first authored glyph */
        avp_gpu_frontend_present(q,&shot,AVP_MJP_SELECT,0);
        assert(shot.pixels[130u*320u+160u]==0x009000u); /* FILES 213 role */
        /* R280: FILES 235 word $5AFF is native RGB16 (BigPEmu (90,255,90)),
         * not the former hand-tuned pure green rgb(0,low byte,0). */
        {
            u8 r,g,b;
            avp_gpu_rgb16_unpack(0x5affu,&r,&g,&b);
            assert(shot.pixels[96u*320u+40u]==(((u32)r<<16)|((u32)g<<8)|b)); /* $01EDB4: X=$28, Y=$52+14 */
            assert(r>80u && b>80u && g>=248u);
        }
        free(q->frontend_img[AVP_MJP_SELECT][3].rgb);
        free(q->frontend_img[AVP_MJP_SELECT][4].rgb);
        free(q->mjp_font);
        avp_jaguar_framebuffer_destroy(&shot);
        free(q);
    }

    /* SELECT text obeys cartridge RDB string order and original CPU TICKs:
     * never reveal the whole briefing from a single SELP callback. A later
     * SELT appends PRESS FIRE to the still-present LOADING SIMULATION line. */
    {
        static const char lines[]="ALIEN\0CLAW\0\0";
        st.mjp_font=(u8*)calloc(224u*39u*2u,1u);assert(st.mjp_font);
        st.mjp_font_size=224u*39u*2u;
        for(const char *q="ALIENCLWPFRSOGMTU";*q;q++)
            set_mjp_cell(st.mjp_font,(unsigned char)*q,0x11c0u);
        st.frontend_mode=AVP_MJP_SELECT;
        st.frontend_select_detail=0;
        /* Instructions accompany the committed three-object carousel,
         * not the intermediate SELE/SOBJ construction frames. */
        st.frontend_select_pose_valid[0]=st.frontend_select_pose_valid[1]=
            st.frontend_select_pose_valid[2]=1u;
        avp_gpu_frontend_select_instructions(&st,
            "LEFT and RIGHT to rotate","FIRE to select");
        assert(strcmp(st.frontend_select_instructions[0],
                      "LEFT and RIGHT to rotate")==0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,4);
        /* R282 retail $01E952: X=$3E, Y=$BE (+14), not centred at row 176. */
        assert(fb.pixels[204u*fb.width+62u]!=0u);
        assert(fb.pixels[176u*fb.width+((320u-24u*7u)/2u)]==0u);
        avp_gpu_frontend_select_briefing(&st,lines,"LOADING SIMULATION...");
        assert(st.frontend_select_brief_count==2u);
        assert(!strcmp(st.frontend_select_brief[0],"ALIEN"));
        assert(!strcmp(st.frontend_select_brief[1],"CLAW"));
        assert(st.frontend_select_total_chars==9u);
        assert(st.frontend_select_visible_chars==0u);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,4);
        assert(fb.pixels[96u*fb.width+40u]==0u);
        avp_gpu_frontend_event(&st,0x5449434bu,0,0); /* One_Tick */
        assert(st.frontend_select_visible_chars==1u);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,4);
        assert(fb.pixels[96u*fb.width+40u]!=0u);   /* $01EDB4: X=$28, Y=$52+14 */
        assert(fb.pixels[96u*fb.width+47u]==0u);
        /* Host repaint alone must not advance another character. */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,4);
        assert(st.frontend_select_visible_chars==1u);
        /* SELT might be dispatched immediately from End_Sele: it must not
         * clear LOADING nor leak PRESS FIRE before the text is complete. */
        avp_gpu_frontend_select_prompt(&st,"PRESS FIRE");
        assert(!strcmp(st.frontend_select_status,"LOADING SIMULATION..."));
        assert(!strcmp(st.frontend_select_fire_prompt,"PRESS FIRE"));
        for(unsigned tick=1;tick<9u;tick++)
            avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        assert(st.frontend_select_visible_chars==9u);
        assert(st.frontend_select_loading_ticks==0u);
        assert(!avp_gpu_frontend_select_loading_ready(&st));
        /* The cartridge video shows a distinct loading interval after the
         * entire description appears; repaint must not advance this clock. */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,4);
        assert(st.frontend_select_loading_ticks==0u);
        for(unsigned i=0;i<449u;i++)
            avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        assert(!avp_gpu_frontend_select_loading_ready(&st));
        avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        assert(avp_gpu_frontend_select_loading_ready(&st));
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,4);
        assert(fb.pixels[96u*fb.width+40u]!=0u);
        assert(fb.pixels[111u*fb.width+40u]!=0u);  /* line step $F */
        assert(fb.pixels[210u*fb.width+40u]!=0u);  /* $01EE4C: X=$28, Y=$C4+14 */
        /* $01EF10: PRESS FIRE is its own Show_Tex at X=$C2. */
        assert(fb.pixels[210u*fb.width+194u]!=0u);
        avp_gpu_frontend_event(&st,0x53454c45u,0,0);
        assert(!st.frontend_select_brief_active);
        assert(!avp_gpu_frontend_select_loading_ready(&st));
        assert(!st.frontend_select_status[0]);
        assert(!st.frontend_select_fire_prompt_active);
        free(st.mjp_font);st.mjp_font=NULL;st.mjp_font_size=0;
    }

    /* Retail TITLE uses the original bitmap background and the two
     * already-expanded OP RGB16 buffers (FILES 236/237), not JPEG 205/207.
     * Preserve the original Update / Update2 integer trajectories
     * while the Jaguar CPU owns input and title music. */
    {
        static u8 background[320u*280u*3u], alien[320u*122u*3u],pred[320u*76u*3u];
        for(size_t i=0;i<sizeof(background);i++)background[i]=60u;
        for(size_t i=0;i<sizeof(alien);i+=3u){alien[i]=240u;alien[i+1u]=40u;alien[i+2u]=25u;}
        for(size_t i=0;i<sizeof(pred);i+=3u){pred[i]=50u;pred[i+1u]=220u;pred[i+2u]=80u;}
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
        st.frontend_img[AVP_MJP_TITLE][0]=(AvpGpuRgbImage){320,280,background,NULL};
        st.frontend_img[AVP_MJP_TITLE][3]=(AvpGpuRgbImage){320,122,alien,NULL};
        st.frontend_img[AVP_MJP_TITLE][2]=(AvpGpuRgbImage){320,76,pred,NULL};
        st.frontend_loaded[AVP_MJP_TITLE][0]=1u;
        st.frontend_loaded[AVP_MJP_TITLE][3]=1u;
        st.frontend_loaded[AVP_MJP_TITLE][2]=1u;
        avp_gpu_frontend_event(&st,0x5449544cu,0,0); /* TITL */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(fb.pixels[20u*fb.width+10u]==0u);
        /* Half-revealed title background uses the actual RGB16 fade
         * arithmetic, not RGB888 channel multiplication.  This deliberately
         * samples a pixel outside both moving title objects. */
        for(unsigned i=0u;i<16u;i++)
            avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        {
            u8 r,g,b;
            u16 pixel=avp_gpu_rgb16_fraction(
                avp_gpu_rgb16_pack(60u,60u,60u),8u);
            avp_gpu_rgb16_unpack(pixel,&r,&g,&b);
            assert(fb.pixels[70u*fb.width+10u]==
                   (((u32)r<<16)|((u32)g<<8)|b));
            assert(fb.pixels[70u*fb.width+10u]!=0x1e1e1eu);
        }
        for(unsigned i=16u;i<32u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(fb.pixels[70u*fb.width+10u]==expected_rgb16(60u,60u,60u));
        /* R77: the first 32 VBLs show the backdrop, before the PC's
         * scaled logo animation begins. The exact original delay remains
         * unverified without the owner's ROM and reference capture. */
        assert(fb.pixels[32u*fb.width+42u]==expected_rgb16(60u,60u,60u));
        for(unsigned i=32u;i<64u;i++)
            avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        /* Retail model: Alien YPOS=$5c, NTSC VDB=$1c -> row 32. */
        assert(fb.pixels[32u*fb.width+42u]==0xf02818u); /* Update phase 32 */
        assert(fb.pixels[31u*fb.width+42u]!=0xf02818u);
        for(unsigned i=64u;i<72u;i++){
            avp_gpu_frontend_event(&st,0x5449434bu,0,0);
            if(i==64u||i==65u||i==66u){
                avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
                if(i<=65u){ /* same original phase 1 for two ticks */
                    assert(fb.pixels[106u*fb.width+12u]==0x30dc50u);
                    assert(fb.pixels[101u*fb.width+21u]==expected_rgb16(60u,60u,60u));
                }else{ /* original phase 3 after the increment */
                    assert(fb.pixels[101u*fb.width+21u]==0x30dc50u);
                    assert(fb.pixels[106u*fb.width+12u]==expected_rgb16(60u,60u,60u));
                }
            }
        }
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(fb.pixels[32u*fb.width+42u]==0xf02818u);
        /* Update2 phase 7 after two ticks at each odd phase. */
        assert(fb.pixels[92u*fb.width+40u]==0x30dc50u);
        assert(fb.pixels[20u*fb.width+220u]==expected_rgb16(60u,60u,60u));
        for(unsigned i=72u;i<112u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        /* R279: the GPU advances the ALIEN strip $23 times (Clear_Bu
         * $1FA82-$1FAE0), so it rests at Update phase 35: x=(32-35)*8+42=18. */
        assert(fb.pixels[32u*fb.width+18u]==0xf02818u);
        assert(fb.pixels[46u*fb.width+17u]==expected_rgb16(60u,60u,60u));
        /* Predator phase 33: YPOS=28+216-148=96 -> row 34. */
        assert(fb.pixels[34u*fb.width+155u]==0x30dc50u); /* phase 33 */
        assert(fb.pixels[34u*fb.width+314u]==0x30dc50u);
        assert(fb.pixels[34u*fb.width+315u]==expected_rgb16(60u,60u,60u));
        assert(st.frontend_title_reveal_tick==112u);
        /* FILES 237 is 320x76 although the OP aperture says 82 lines.
         * A safe source read must not wrap into other allocation pixels. */
        /* At x=200 we are outside the now two-scanline-taller ALIEN
         * object, so only PREDATOR's source-allocation guard is tested. */
        assert(fb.pixels[88u*fb.width+200u]==expected_rgb16(60u,60u,60u));
        /* For the resting retail Update phase 35, HSCALE=VSCALE=9/32 and
         * REMAINDER=9+32. Source row 21 is displayed at destination line 7
         * and source column 14 at destination column 4 (x=18+4=22). */
        memset(alien,0,sizeof(alien));
        {
            const size_t source_at=3u*((size_t)21u*320u+14u);
            alien[source_at]=240u;alien[source_at+1u]=40u;alien[source_at+2u]=25u;
        }
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(fb.pixels[39u*fb.width+22u]==0xf02818u);
        assert(fb.pixels[39u*fb.width+23u]==expected_rgb16(60u,60u,60u));
        /* Original SCBITOBJ vertical REMAINDER is VSCALE+0x20, so at
         * scale 9 the first two *displayed* rows share source row 0,
         * source rows 1-2 are skipped, displayed row 2 samples source
         * row 3, and the 32-line object ends on source row 106 (row 109
         * is never displayed). Validate exact original OP writeback. */
        memset(alien,0,sizeof(alien));
        alien[0]=240u;alien[1]=40u;alien[2]=25u;  /* row 0 */
        alien[3u*320u]=50u;alien[3u*320u+1u]=220u;
        alien[3u*320u+2u]=80u;               /* row 1: skipped */
        alien[3u*640u]=50u;alien[3u*640u+1u]=220u;
        alien[3u*640u+2u]=80u;               /* row 2: skipped */
        alien[3u*960u]=50u;alien[3u*960u+1u]=220u;
        alien[3u*960u+2u]=80u;               /* row 3 */
        alien[3u*106u*320u]=240u;
        alien[3u*106u*320u+1u]=40u;
        alien[3u*106u*320u+2u]=25u;         /* final displayed row */
        alien[3u*109u*320u]=240u;
        alien[3u*109u*320u+1u]=40u;
        alien[3u*109u*320u+2u]=25u;         /* never displayed */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(fb.pixels[32u*fb.width+18u]==0xf02818u);
        assert(fb.pixels[33u*fb.width+18u]==0xf02818u);
        assert(fb.pixels[34u*fb.width+18u]==0x30dc50u);
        assert(fb.pixels[35u*fb.width+18u]==expected_rgb16(60u,60u,60u));
        assert(fb.pixels[63u*fb.width+18u]==0xf02818u);
        assert(fb.pixels[64u*fb.width+18u]==expected_rgb16(60u,60u,60u));
        /* Each of the 35 original ALIEN Update phases must use the exact
         * OP phrase remainder, not proportional nearest-neighbor sampling.
         * Give each source row a unique native-RGB16 marker at X=0, walk an
         * independent SCBITOBJ 3.5 accumulator, and compare every output
         * line (including phases above/below 1:1).  No developer tracing
         * or production test hooks are required to inspect final pixels. */
        memset(alien,0,sizeof(alien));
        for(unsigned row=0u;row<110u;row++){
            alien[3u*row*320u]  =(u8)((row&31u)*8u);
            alien[3u*row*320u+1u]=(u8)((1u+(row>>5))*48u);
            alien[3u*row*320u+2u]=32u;
        }
        for(unsigned tick=1u;tick<=35u;tick++){
            const unsigned scale=44u-tick;
            const unsigned width=(320u*scale)/32u;
            const unsigned height=(110u*scale)/32u+2u;
            const unsigned x=(32u-tick)*8u+42u;
            unsigned srcrow=0u,remainder=scale+32u;
            assert(width>0u && x<320u);
            st.frontend_title_reveal_tick=tick+32u;
            st.frontend_title_splash=1u;
            avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
            for(unsigned yy=0u; yy<height;yy++){
                const u32 expected=expected_rgb16(
                    (u8)((srcrow&31u)*8u),
                    (u8)((1u+(srcrow>>5))*48u),32u);
                assert(srcrow<110u);
                assert(fb.pixels[(32u+yy)*fb.width+x]==expected);
                while(remainder<32u){remainder+=scale;srcrow++;}
                remainder-=32u;
            }
            assert(srcrow>=110u);
        }
        st.frontend_title_reveal_tick=112u;
        /* Restore fully covered synthetic title before the keyed test. */
        for(size_t i=0;i<sizeof(alien);i+=3u){alien[i]=240u;alien[i+1u]=40u;alien[i+2u]=25u;}
        /* Retail OP TRANS keys exactly on RGB16 color zero. Low-intensity
         * nonzero RGB16 title detail must NOT disappear just because the
         * total 24-bit channel brightness is below an arbitrary threshold. */
        {
            static u8 keyed_sprite[320u*122u*3u]={24u,0u,0u};
            st.frontend_img[AVP_MJP_TITLE][3]=(AvpGpuRgbImage){320,122,keyed_sprite,NULL};
            avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
            assert(fb.pixels[32u*fb.width+18u]==0x180000u);
            assert(fb.pixels[32u*fb.width+120u]==expected_rgb16(60u,60u,60u));
        }
        /* R188: the unscaled title background is an original RGB16 Object
         * Processor bitmap too. R187 kept RGB888 decoder low bits only for
         * the fully revealed background (scaled objects were quantized). */
        {
            u8 rr,gg,bb;
            u16 packed;
            background[0]=123u;background[1]=101u;background[2]=37u;
            avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
            packed=avp_gpu_rgb16_pack(123u,101u,37u);
            avp_gpu_rgb16_unpack(packed,&rr,&gg,&bb);
            assert(fb.pixels[0]==(((u32)rr<<16)|((u32)gg<<8)|bb)); /* title OP starts at Y=0 */
            assert(fb.pixels[14u*fb.width]==expected_rgb16(60u,60u,60u));
            assert(fb.pixels[0]!=0x7b6525u);
            background[0]=60u;background[1]=60u;background[2]=60u;
        }
        st.frontend_loaded[AVP_MJP_TITLE][0]=0u;
        st.frontend_loaded[AVP_MJP_TITLE][2]=0u;
        st.frontend_loaded[AVP_MJP_TITLE][3]=0u;
        st.frontend_img[AVP_MJP_TITLE][0].rgb=NULL;
        st.frontend_img[AVP_MJP_TITLE][2].rgb=NULL;
        st.frontend_img[AVP_MJP_TITLE][3].rgb=NULL;
    }

    /* R148: Fame's five record rows and its RDB $2A925 prompt are two
     * separate Jaguar presentation layers. CPU FMSP/Show_Tex ordering and
     * the saved scores remain untouched. Coordinates match the user-provided
     * BigPEmu High Scores capture, in 320-pixel picture space. */
    avp_gpu_frontend_enter_mode(&st,AVP_MJP_FAME);
    st.mjp_font=(u8*)calloc(224u*39u*2u,1u);assert(st.mjp_font);
    st.mjp_font_size=224u*39u*2u;
    set_mjp_cell(st.mjp_font,'P',0x11f0u);
    set_cell(st.avp_font,0u,'1',0x11c0u);
    set_cell(st.avp_font,0u,'.',0x11c0u);
    set_cell(st.avp_font,0u,'S',0x11c0u);
    avp_gpu_frontend_event(&st,0x464d5350u,0,0); /* FMSP row 1: Marine */
    avp_gpu_frontend_draw_text(&st,0u,"S",45u);
    avp_gpu_frontend_draw_text(&st,0u,"   1220000",128u);
    /* Original recorded reveal: image fades in with no prepainted rows;
     * the ranking glyphs rise from below frame and settle by tick 96. */
    {
        static u8 bg[3]={88u,100u,116u};
        st.frontend_img[AVP_MJP_FAME][2]=(AvpGpuRgbImage){1,1,bg,NULL};
        st.frontend_loaded[AVP_MJP_FAME][2]=1u;
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
        assert(st.frontend_fame_intro_tick==0u);
        assert(fb.pixels[200u*fb.width+300u]==0u);
        assert(fb.pixels[60u*fb.width+45u]==0u);
        for(unsigned i=0u;i<32u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
        assert(fb.pixels[200u*fb.width+300u]!=0u);
        assert(fb.pixels[60u*fb.width+45u]!=0u /* background now lit */);
        /* R176: measured retail scroll is 3 picture-space pixels per VBlank.
         * At tick 32 rank 1 is Y=190, at tick 48 Y=142, at tick 79 Y=49;
         * it must stop at Y=46 on tick 80 without quadratic easing. The
         * uniform synthetic image lets us distinguish the coloured rank
         * glyphs from their background at those exact raster positions. */
        assert(fb.pixels[204u*fb.width+45u]!=fb.pixels[204u*fb.width+300u]);
        for(unsigned i=32u;i<48u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
        assert(fb.pixels[156u*fb.width+45u]!=fb.pixels[156u*fb.width+300u]);
        for(unsigned i=48u;i<79u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
        assert(fb.pixels[63u*fb.width+45u]!=fb.pixels[63u*fb.width+300u]);
        avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
        assert(fb.pixels[60u*fb.width+45u]!=fb.pixels[60u*fb.width+300u]);
        for(unsigned i=80u;i<96u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        assert(st.frontend_fame_intro_tick==96u);
        st.frontend_loaded[AVP_MJP_FAME][2]=0u;
        st.frontend_img[AVP_MJP_FAME][2].rgb=NULL;
    }
    avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
    assert(fb.pixels[60u*fb.width+45u]!=0u); /* rank 1 */
    assert(fb.pixels[60u*fb.width+79u]!=0u); /* first letter */
    assert(fb.pixels[60u*fb.width+201u]!=0u); /* 1220000 score */
    assert(fb.pixels[14u*fb.width+45u]==0u); /* old crowded top row */
    avp_gpu_frontend_event(&st,0x4d4a5054u,0x0002A925u,0); /* MJPT */
    avp_gpu_frontend_draw_text(&st,0u,"P",0u);
    avp_gpu_frontend_draw_text(&st,0u,"_",7u); /* Show_Tex blank */
    for(unsigned i=0u;i<8u;i++)avp_gpu_frontend_draw_text(&st,0u,"P",14u+i*7u);
    assert(strcmp(st.frontend_text[0],"S")==0);
    assert(strcmp(st.frontend_fame_score[0],"   1220000")==0);
    assert(st.frontend_fame_prompt_active);
    assert(st.frontend_fame_prompt[1]==' ');
    avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
    {   /* R280: prompt glyph word $11F0 shown as native RGB16, not rgb(0,$F0,0). */
        u8 r,g,b;
        avp_gpu_rgb16_unpack(0x11f0u,&r,&g,&b);
        assert(fb.pixels[200u*fb.width+45u]==(((u32)r<<16)|((u32)g<<8)|b));
    }
    assert(fb.pixels[60u*fb.width+79u]!=0u); /* score rows retained */
    /* The High Scores exit fades the *finished* score/text composition.
       The title-side load must not reveal title assets until phase zero. */
    avp_gpu_frontend_event(&st,0x46534644u,16,0); /* FSFD, first frame */
    avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
    const u32 before_fame_fade=fb.pixels[60u*fb.width+79u];
    assert(before_fame_fade!=0u);
    avp_gpu_frontend_event(&st,0x46534644u,8,0);
    avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
    assert(fb.pixels[60u*fb.width+79u]!=0u);
    assert(fb.pixels[60u*fb.width+79u]!=before_fame_fade);
    avp_gpu_frontend_event(&st,0x46534644u,0,0);
    avp_gpu_frontend_present(&st,&fb,AVP_MJP_FAME,0);
    assert(fb.pixels[60u*fb.width+79u]==0u);
    assert(fb.pixels[200u*fb.width+45u]==0u);
    avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
    assert(!st.frontend_fame_depart_active);
    assert(!st.frontend_fame_prompt_active && !st.frontend_fame_prompt[0]);
    free(st.mjp_font);st.mjp_font=NULL;st.mjp_font_size=0;

    /* The retail $2AFC3 SIMULATION TERMINATED record carries its own
     * blank rows.  The authored text therefore sits on the ordinary MJP
     * grid: with line 1 and glyph X=63, the first visible S appears at
     * native X=63,Y=28 rather than at a hosted late-screen offset. */
    {
        st.mjp_font=(u8*)calloc(224u*39u*2u,1u);assert(st.mjp_font);
        st.mjp_font_size=224u*39u*2u;
        set_mjp_cell(st.mjp_font,'S',0x11c0u);
        set_mjp_cell(st.mjp_font,'_',0x11c0u);
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_SIM_TERMINATED);
        avp_gpu_frontend_draw_text(&st,1u,"S",63u);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SIM_TERMINATED,0);
        assert(fb.pixels[28u*fb.width+63u]!=0u);
        assert(fb.pixels[178u*fb.width+91u]==0u);
        /* R92: advancing CPU text moves a visible cursor brush at the
         * next glyph position without moving already printed characters. */
        avp_gpu_frontend_event(&st,0x4d4a5043u,70,1);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SIM_TERMINATED,0);
        assert(fb.pixels[28u*fb.width+70u]!=0u);
        avp_gpu_frontend_event(&st,0x4d4a5043u,77,1);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SIM_TERMINATED,0);
        assert(fb.pixels[28u*fb.width+77u]!=0u);
        assert(fb.pixels[28u*fb.width+70u]==0u);
        avp_gpu_frontend_event(&st,0x4d4a5050u,0,0); /* first same-page MJPP */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SIM_TERMINATED,0);
        assert(fb.pixels[28u*fb.width+63u]==0u);
        for(unsigned i=0u;i<8u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SIM_TERMINATED,0);
        assert(fb.pixels[28u*fb.width+63u]!=0u);
        avp_gpu_frontend_event(&st,0x4d4a5050u,0,0); /* outbound MJPP */
        for(unsigned i=0u;i<8u;i++)avp_gpu_frontend_event(&st,0x5449434bu,0,0);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SIM_TERMINATED,0);
        assert(fb.pixels[178u*fb.width+91u]==0u);
        /* R280: Do_Intro's Show_Tex cursor starts at Y=$A6 of the screen
         * buffer (port row 180) with a 14-row line step, below the picture. */
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_INTRO);
        avp_gpu_frontend_draw_text(&st,1u,"S",24u);
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_INTRO,0);
        assert(fb.pixels[194u*fb.width+24u]!=0u);
        assert(fb.pixels[28u*fb.width+24u]==0u);
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_FAME);
        assert(st.frontend_terminated_transitions==0u);
        assert(st.frontend_fame_intro_tick==0u);
        free(st.mjp_font);st.mjp_font=NULL;st.mjp_font_size=0;
    }

    /* Jaguar RGB16 OP precision: title and carousel must not display the
     * extra low bits left by the hosted JagPEG RGB888 decompressor. This
     * includes a real scaled TITLE role and a native SCBITOBJ card. */
    {
        u8 *title=(u8*)malloc(96u*40u*3u);
        u8 *portrait=(u8*)malloc(320u*240u*3u);
        assert(title && portrait);
        for(size_t i=0u;i<96u*40u;i++){
            title[3u*i]=131u;title[3u*i+1u]=151u;title[3u*i+2u]=191u;
        }
        for(size_t i=0u;i<320u*240u;i++){
            portrait[3u*i]=67u;portrait[3u*i+1u]=99u;portrait[3u*i+2u]=131u;
        }
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
        st.frontend_title_splash=1u;
        st.frontend_title_reveal_tick=32u;
        /* Keep an independent real title background so fallback scene
         * selection cannot substitute JPEG 205 as the backdrop. */
        u8 title_bg[3]={25u,33u,41u};
        st.frontend_img[AVP_MJP_TITLE][0]=(AvpGpuRgbImage){1,1,title_bg,NULL};
        st.frontend_loaded[AVP_MJP_TITLE][0]=1u;
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        const u32 original_bg_pixel=fb.pixels[46u*fb.width+42u];
        st.frontend_img[AVP_MJP_TITLE][5]=(AvpGpuRgbImage){96,40,title,NULL};
        st.frontend_loaded[AVP_MJP_TITLE][5]=1u;
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_TITLE,0);
        assert(fb.pixels[46u*fb.width+42u]==original_bg_pixel);
        st.frontend_img[AVP_MJP_TITLE][5].rgb=NULL;
        st.frontend_loaded[AVP_MJP_TITLE][5]=0u;
        st.frontend_img[AVP_MJP_TITLE][0].rgb=NULL;
        st.frontend_loaded[AVP_MJP_TITLE][0]=0u;
        avp_gpu_frontend_enter_mode(&st,AVP_MJP_SELECT);
        st.frontend_img[AVP_MJP_SELECT][5]=(AvpGpuRgbImage){320,240,portrait,NULL};
        st.frontend_loaded[AVP_MJP_SELECT][5]=1u;
        st.frontend_selected_species=PT_ALIEN;
        for(unsigned i=0u;i<3u;i++){
            st.frontend_select_pose_valid[i]=1u;
            st.frontend_select_pose_species[i]=PT_ALIEN;
            st.frontend_select_pose_scale[i]=16;
            st.frontend_select_pose_x[i]=(i==0u)?10:-300;
            st.frontend_select_pose_y[i]=(i==0u)?10:-300;
        }
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_SELECT,0);
        assert(fb.pixels[10u*fb.width+10u]==0x00406080u);
        free(title);free(portrait);
        st.frontend_img[AVP_MJP_SELECT][5].rgb=NULL;
        st.frontend_loaded[AVP_MJP_SELECT][5]=0u;
    }
    free(st.avp_font);
    avp_jaguar_framebuffer_destroy(&fb);
    return 0;
}
