/* R279: retail title logo and legal notice.
 * (1) Clear_Bu $1FA82-$1FAE0 lets the GPU advance $7FB4 for $23 (35) steps;
 *     Update $1E7B4: scale=$2C-$7FB4, x=($20-$7FB4)*8+$2A -> final scale 9,
 *     x 18 (R278 stopped at phase 32: scale 12, x 42).
 * (2) Legal_Pic = FILES 233 (320x40 RGB16) blitted to picture row $A8+14,
 *     shown at screen row 196 with colour 0 transparent (not transcribed text).
 * Synthetic pixels only; no retail data. */
#define _DEFAULT_SOURCE 1
#include "../src/jaguar/avp_gpu/frontend_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
char mjp_games[4]="xxx";
int main(void)
{
    AvpTitleOpPose p=frontend_title_alien_pose(35u);
    assert(p.scale==9u&&p.x==18);
    p=frontend_title_alien_pose(80u);
    assert(p.scale==9u&&p.x==18);          /* settles at the GPU step count */
    p=frontend_title_alien_pose(0u);
    assert(p.scale==44u&&p.x==298);        /* unchanged start pose */
    {
        static AvpAvpGpuState st;static uint32_t px[320u*240u];
        AvpJaguarFramebuffer fb;u8 *rgb=(u8*)calloc(320u*40u,3u);
        assert(rgb);
        memset(&fb,0,sizeof fb);fb.width=320u;fb.height=240u;fb.pixels=px;
        for(size_t i=0;i<320u*240u;i++)px[i]=0x123456u;
        rgb[(5u*320u+10u)*3u]=0xf8u;               /* one red pixel, row 5 col 10 */
        st.frontend_img[AVP_MJP_TITLE][4].width=320;st.frontend_img[AVP_MJP_TITLE][4].height=40;
        st.frontend_img[AVP_MJP_TITLE][4].rgb=rgb;st.frontend_loaded[AVP_MJP_TITLE][4]=1u;
        assert(title_legal_bitmap(&st,&fb));
        assert(px[(182u+5u)*320u+10u]!=0x123456u);  /* drawn at row 196+5 */
        assert(px[(182u+5u)*320u+11u]==0x123456u);  /* black = transparent */
        assert(px[(181u)*320u+10u]==0x123456u);
        free(rgb);
    }
    {   /* R280: MJP is RGB16 (VMODE $6C7): FILES 235/234 glyph words decode
         * as RGB16, word 0 transparent.  $5AFF story green, $AEEA menu text. */
        static AvpAvpGpuState st;static uint32_t px[320u*240u];
        static u8 f235[224u*39u*2u],f234[320u*96u*2u];
        AvpJaguarFramebuffer fb;u8 r,g,b;
        memset(&fb,0,sizeof fb);fb.width=320u;fb.height=240u;fb.pixels=px;
        f235[0]=0x5au;f235[1]=0xffu;                 /* '!' cell (0,0) */
        frontend_font_glyph(&fb,f235,sizeof f235,224,39,7,13,10,20,'!');
        avp_gpu_rgb16_unpack(0x5affu,&r,&g,&b);
        assert(px[20u*320u+10u]==rgb(r,g,b));
        assert(r<128u && g>=248u);                   /* green, not CRY cyan */
        assert(b<128u);
        assert(px[20u*320u+11u]==0u);                /* word 0 transparent */
        f234[0]=0xaeu;f234[1]=0xeau;                 /* '!' band 0 (0,0) */
        st.avp_font=f234;st.avp_font_size=sizeof f234;
        fame_font_glyph(&st,&fb,40,60,0u,'!');
        avp_gpu_rgb16_unpack(0xaeeau,&r,&g,&b);
        assert(px[60u*320u+40u]==rgb(r,g,b) && b>r && b>g);
        assert(px[60u*320u+41u]==0u);
    }
    {   /* R280: retail credits ($01F5BC) = FILES 238 bitmap in a 280-line
         * object at port row 14+Y, data starting at the CRSY line. */
        static AvpAvpGpuState st;static uint32_t px[320u*228u];
        AvpJaguarFramebuffer fb;u8 *img=(u8*)calloc(320u*1373u,3u);
        assert(img);
        memset(&fb,0,sizeof fb);fb.width=320u;fb.height=228u;fb.pixels=px;
        img[(7u*320u+100u)*3u]=0xf8u;img[(7u*320u+100u)*3u+1u]=0xf8u;img[(7u*320u+100u)*3u+2u]=0xf8u;
        st.frontend_img[AVP_MJP_CREDITS][0]=(AvpGpuRgbImage){320,1373,img,NULL};
        st.frontend_loaded[AVP_MJP_CREDITS][0]=1u;
        st.frontend_mode=AVP_MJP_CREDITS;
        avp_gpu_frontend_event(&st,0x43524247u,0,0);          /* CRBG */
        avp_gpu_frontend_event(&st,0x43525359u,10,0);         /* CRSY y=10 */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_CREDITS,0);
        assert(px[(14u+10u+7u)*320u+100u]!=0u);
        assert(px[(14u+10u+7u)*320u+101u]==0u);              /* index 0 transparent */
        avp_gpu_frontend_event(&st,0x43525359u,1,5);          /* data line 5 */
        avp_gpu_frontend_present(&st,&fb,AVP_MJP_CREDITS,0);
        assert(px[(14u+1u+2u)*320u+100u]!=0u);
        st.frontend_img[AVP_MJP_CREDITS][0].rgb=NULL;free(img);
    }
    {   /* R282: retail $01F554/$01FD0A stamp FILES 205 (96x40 at 15,33) and
         * FILES 232 (160x40 at 153,33) opaquely into the title buffer, then
         * hide the scaled ALIEN/PREDATOR objects one VBL later. */
        static AvpAvpGpuState st;static uint32_t px[320u*254u];
        static u8 a205[96u*40u*3u],p232[160u*40u*3u];
        AvpJaguarFramebuffer fb;
        memset(&fb,0,sizeof fb);fb.width=320u;fb.height=254u;fb.pixels=px;
        for(size_t i=0;i<96u*40u;i++){a205[3u*i]=8u;a205[3u*i+1u]=200u;a205[3u*i+2u]=8u;}
        for(size_t i=0;i<160u*40u;i++){p232[3u*i]=200u;p232[3u*i+1u]=8u;p232[3u*i+2u]=8u;}
        st.frontend_img[AVP_MJP_TITLE][5]=(AvpGpuRgbImage){96,40,a205,NULL};
        st.frontend_img[AVP_MJP_TITLE][6]=(AvpGpuRgbImage){160,40,p232,NULL};
        st.frontend_loaded[AVP_MJP_TITLE][5]=st.frontend_loaded[AVP_MJP_TITLE][6]=1u;
        assert(title_alien_stamp(&st,&fb) && title_predator_stamp(&st,&fb));
        assert(px[(33u)*320u+15u]!=0u && px[(33u)*320u+14u]==0u);
        assert(px[(72u)*320u+110u]!=0u && px[(73u)*320u+110u]==0u);
        assert(px[(33u)*320u+153u]!=0u && px[(33u)*320u+312u]!=0u);
        assert(px[(33u)*320u+313u]==0u);
        st.frontend_loaded[AVP_MJP_TITLE][5]=st.frontend_loaded[AVP_MJP_TITLE][6]=0u;
        assert(!title_alien_stamp(&st,&fb) && !title_predator_stamp(&st,&fb));
    }
    puts("R279-R282 title logos/stamps, Legal_Pic, RGB16 MJP fonts, retail credits bitmap PASS");
    return 0;
}
