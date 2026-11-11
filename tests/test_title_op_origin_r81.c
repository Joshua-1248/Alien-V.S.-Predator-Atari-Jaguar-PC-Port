/* RE34 R81: verify original retail NTSC title OP YPOS against synthetic
 * frontend buffers. Jaguar original GPU/OP reference model fields 330/506:
 *   VDB=$1c; title YPOS=$1c; ALIEN YPOS=$5c;
 *   PREDATOR phase31 YPOS=$69. No ROM bytes/assets in this test. */
#define _DEFAULT_SOURCE 1
#define AVP_PC_REFERENCE_TITLE_TIMING 1
#include "../src/jaguar/avp_gpu/frontend_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>
char mjp_games[4]="xxx";
int main(void)
{
    static AvpAvpGpuState st;
    static u32 px[320u*240u];
    static u8 bg[320u*280u*3u];
    static u8 alien[320u*122u*3u],predator[320u*76u*3u];
    static u8 mark_a[96u*40u*3u],mark_p[160u*40u*3u];
    static u8 legal[320u*40u*3u];
    AvpJaguarFramebuffer fb={0};
    AvpTitleOpPose p;
    fb.width=320u;fb.height=240u;fb.pixels=px;
    assert(frontend_title_op_y(0x1cu)==0);
    assert(frontend_title_op_y(0x5cu)==32);
    p=frontend_title_alien_pose(7u);
    assert(p.x==242 && p.y==32 && p.scale==37u);
    p=frontend_title_predator_pose(31u);
    assert(p.y==(105-28)/2 && p.scale==17u);
    p=frontend_title_predator_pose(33u);
    assert(p.y==34 && p.x==155 && p.scale==16u);
    /* The Jaguar title background OP shows its first source row at active
     * output row 0; R80 left that row blank until output row 14. */
    for(size_t k=0;k<sizeof(bg);k+=3u){bg[k]=48u;bg[k+1u]=80u;bg[k+2u]=112u;}
    st.frontend_img[AVP_MJP_TITLE][0]=(AvpGpuRgbImage){320,280,bg,NULL};
    st.frontend_loaded[AVP_MJP_TITLE][0]=1u;
    avp_gpu_frontend_enter_mode(&st,AVP_MJP_TITLE);
    frontend_blit_image(&fb,&st.frontend_img[AVP_MJP_TITLE][0],1);
    assert(px[0]==0x305070u);
    assert(px[14u*320u]==0x305070u);
    for(size_t k=0;k<sizeof(alien);k+=3u){alien[k]=240u;alien[k+1u]=40u;alien[k+2u]=24u;}
    for(size_t k=0;k<sizeof(predator);k+=3u){predator[k]=48u;predator[k+1u]=216u;predator[k+2u]=80u;}
    st.frontend_img[AVP_MJP_TITLE][3]=(AvpGpuRgbImage){320,122,alien,NULL};
    st.frontend_img[AVP_MJP_TITLE][2]=(AvpGpuRgbImage){320,76,predator,NULL};
    st.frontend_loaded[AVP_MJP_TITLE][2]=st.frontend_loaded[AVP_MJP_TITLE][3]=1u;
    /* Direct scaled OP draw: first source row is at Y=32 (not 46). */
    memset(px,0,sizeof px);
    p=frontend_title_alien_pose(7u);
    frontend_blit_title_sprite(&fb,&st.frontend_img[AVP_MJP_TITLE][3],
                               p.x,p.y,p.width,p.height,110u,p.scale,16u);
    assert(px[32u*320u+242u]==0xf02818u);
    assert(px[46u*320u+242u]==0xf02818u);
    assert(px[31u*320u+242u]==0u);
    /* The static stamp is drawn into the original screen buffer at Y=33. */
    for(size_t k=0;k<sizeof(mark_a);k+=3u){mark_a[k]=240u;mark_a[k+1u]=40u;mark_a[k+2u]=24u;}
    for(size_t k=0;k<sizeof(mark_p);k+=3u){mark_p[k]=48u;mark_p[k+1u]=216u;mark_p[k+2u]=80u;}
    st.frontend_img[AVP_MJP_TITLE][5]=(AvpGpuRgbImage){96,40,mark_a,NULL};
    st.frontend_img[AVP_MJP_TITLE][6]=(AvpGpuRgbImage){160,40,mark_p,NULL};
    st.frontend_loaded[AVP_MJP_TITLE][5]=st.frontend_loaded[AVP_MJP_TITLE][6]=1u;
    memset(px,0,sizeof px);
    assert(title_alien_stamp(&st,&fb)&&title_predator_stamp(&st,&fb));
    assert(px[33u*320u+15u]==0xf02818u);
    assert(px[33u*320u+153u]==0x30d850u);
    assert(px[32u*320u+15u]==0u);
    legal[3u*((size_t)5u*320u+8u)]=240u;
    st.frontend_img[AVP_MJP_TITLE][4]=(AvpGpuRgbImage){320,40,legal,NULL};
    st.frontend_loaded[AVP_MJP_TITLE][4]=1u;
    memset(px,0,sizeof px);
    assert(title_legal_bitmap(&st,&fb));
    assert(px[187u*320u+8u]==0xf00000u);
    assert(px[201u*320u+8u]==0u);
    puts("R81 verified retail OP title vertical origin PASS");
    return 0;
}
