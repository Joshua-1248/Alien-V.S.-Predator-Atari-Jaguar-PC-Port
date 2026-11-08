#include "local_mp_compositor.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Distinct per-player world colors and character HUD bands prove the whole
 * source frame travels together; changing a layout never shuffles elements. */
#define SRC_W 7u
#define SRC_H 5u
#define OUT_W 13u
#define OUT_H 9u
#define STRIDE 17u
static uint32_t src[4][SRC_H][SRC_W];
static uint32_t out[OUT_H][STRIDE];
static uint32_t original[OUT_H][STRIDE];
static const uint32_t hud[4]={0x00aa0021u,0x00bb0032u,0x00cc0043u,0x00dd0054u};
static AvpLocalMpSourceFrame inputs[4];
static const AvpLocalMpOutputFrame dest={&out[0][0],OUT_W,OUT_H,STRIDE};
static const uint32_t blank=0x00303030u;

static void setup_inputs(void)
{
    unsigned p,y,x;
    for(p=0;p<4;p++){
        for(y=0;y<SRC_H;y++)for(x=0;x<SRC_W;x++)
            src[p][y][x]=y==SRC_H-1?hud[p]:
                (0x00100000u*(p+1u)) | (0x00010000u*y) | x;
        inputs[p]=(AvpLocalMpSourceFrame){&src[p][0][0],SRC_W,SRC_H,SRC_W};
    }
}
static void check_rects(const AvpLocalMpSetup *setup)
{
    AvpLocalMpRect r[4];
    unsigned n=avp_local_mp_viewports(setup,OUT_W,OUT_H,r),p,x,y,k;
    assert(n==setup->players);
    /* Destination padding belongs to the hosting framebuffer. */
    for(y=0;y<OUT_H;y++)for(x=OUT_W;x<STRIDE;x++)assert(out[y][x]==0xfeedefffu);
    for(y=0;y<OUT_H;y++)for(x=0;x<OUT_W;x++){
        unsigned match=0;
        for(p=0;p<n;p++)if(x>=r[p].x&&x<r[p].x+r[p].width&&y>=r[p].y&&y<r[p].y+r[p].height){
            unsigned sx=r[p].width==1u?0u:(unsigned)((uint64_t)(x-r[p].x)*(SRC_W-1u)/(r[p].width-1u));
            unsigned sy=r[p].height==1u?SRC_H-1u:(unsigned)((uint64_t)(y-r[p].y)*(SRC_H-1u)/(r[p].height-1u));
            assert(out[y][x]==src[p][sy][sx]);
            match++;
        }
        assert(match<=1u);
        if(!match)assert(out[y][x]==blank);
    }
    /* Verify no frame overlaps across the right/top/bottom region. */
    for(k=0;k<n;k++){
        assert(r[k].width&&r[k].height);
        /* The full original bottom-of-screen HUD remains visible even in
         * four-player quadrants with fewer rows than the source picture. */
        assert(out[r[k].y+r[k].height-1u][r[k].x]==hud[k]);
    }
}
static void check_case(unsigned players,unsigned layout)
{
    AvpLocalMpSetup s;
    unsigned x,y;
    avp_local_mp_defaults(&s);s.players=players;s.layout=layout;
    for(y=0;y<OUT_H;y++)for(x=0;x<STRIDE;x++)out[y][x]=0xfeedefffu;
    assert(avp_local_mp_compose(&s,inputs,&dest,blank)==players);
    check_rects(&s);
}
static void reject_bad_sources(void)
{
    AvpLocalMpSetup s;
    AvpLocalMpSourceFrame bad[4];
    AvpLocalMpOutputFrame destbad=dest;
    avp_local_mp_defaults(&s);
    memcpy(bad,inputs,sizeof(bad));
    memcpy(original,out,sizeof(out));
    bad[1].pixels=NULL;
    assert(avp_local_mp_compose(&s,bad,&dest,blank)==0);
    assert(memcmp(out,original,sizeof(out))==0);
    memcpy(bad,inputs,sizeof(bad));bad[1].pitch=SRC_W-1;
    assert(avp_local_mp_compose(&s,bad,&dest,blank)==0);
    assert(memcmp(out,original,sizeof(out))==0);
    bad[1]=inputs[1];bad[0].pixels=dest.pixels;
    assert(avp_local_mp_compose(&s,bad,&dest,blank)==0);
    assert(memcmp(out,original,sizeof(out))==0);
    destbad.pitch=OUT_W-1;
    assert(avp_local_mp_compose(&s,inputs,&destbad,blank)==0);
    assert(memcmp(out,original,sizeof(out))==0);
    s.players=4;destbad=dest;destbad.width=1;destbad.height=1;
    assert(avp_local_mp_compose(&s,inputs,&destbad,blank)==0);
    assert(memcmp(out,original,sizeof(out))==0);
}
/* S116: the synchronous original terminal is NOT a full-window screen.
 * Paint its original complete 320x228 raster into only the owning player
 * viewport; every teammate's previous world+HUD picture stays byte-identical.
 * A second menu page must replace only that owner again after the host
 * restores the frozen composite, and returning from the modal must restore
 * the old gameplay image without any leftover terminal pixels. */
static void check_terminal_owner_overlay(unsigned players,unsigned layout,
                                         unsigned three_layout)
{
    AvpLocalMpSetup s;
    AvpLocalMpRect rect[4];
    unsigned p,x,y,owner,n;
    uint32_t terminal[3][4];
    AvpLocalMpSourceFrame frame={&terminal[0][0],4u,3u,4u};
    avp_local_mp_defaults(&s);
    s.players=players;s.layout=layout;s.three_layout=three_layout;
    for(y=0;y<3u;y++)for(x=0;x<4u;x++)terminal[y][x]=0x007a0000u+y*16u+x;
    n=avp_local_mp_viewports(&s,OUT_W,OUT_H,rect);
    assert(n==players);
    for(owner=0;owner<players;owner++){
        for(y=0;y<OUT_H;y++)for(x=0;x<STRIDE;x++)out[y][x]=0xfeedefffu;
        assert(avp_local_mp_compose(&s,inputs,&dest,blank)==players);
        memcpy(original,out,sizeof(out));
        assert(avp_local_mp_overlay_player(&s,owner,&frame,&dest));
        for(y=0;y<OUT_H;y++)for(x=0;x<STRIDE;x++){
            const AvpLocalMpRect *r=&rect[owner];
            int in=(x>=r->x&&x<r->x+r->width&&y>=r->y&&y<r->y+r->height);
            if(!in)assert(out[y][x]==original[y][x]);
            else{
                unsigned sx=r->width==1u?0u:(unsigned)((uint64_t)(x-r->x)*3u/(r->width-1u));
                unsigned sy=r->height==1u?2u:(unsigned)((uint64_t)(y-r->y)*2u/(r->height-1u));
                assert(out[y][x]==terminal[sy][sx]);
            }
        }
        /* Subsequent computer menu redraws must not spill into teammates. */
        memcpy(out,original,sizeof(out));
        for(y=0;y<3u;y++)for(x=0;x<4u;x++)terminal[y][x]^=0x00001234u;
        assert(avp_local_mp_overlay_player(&s,owner,&frame,&dest));
        for(p=0;p<players;p++)if(p!=owner){
            const AvpLocalMpRect *r=&rect[p];
            for(y=r->y;y<r->y+r->height;y++)
                for(x=r->x;x<r->x+r->width;x++)
                    assert(out[y][x]==original[y][x]);
        }
        memcpy(out,original,sizeof(out));
        assert(!memcmp(out,original,sizeof(out)));
        for(y=0;y<3u;y++)for(x=0;x<4u;x++)terminal[y][x]^=0x00001234u;
    }
}
static void reject_bad_terminal_overlay(void)
{
    AvpLocalMpSetup s;
    AvpLocalMpSourceFrame bad=inputs[0];
    AvpLocalMpOutputFrame shortout=dest;
    avp_local_mp_defaults(&s);s.players=2;
    memcpy(original,out,sizeof(out));
    assert(!avp_local_mp_overlay_player(&s,2u,&bad,&dest));
    assert(!memcmp(out,original,sizeof(out)));
    bad.pitch=SRC_W-1u;
    assert(!avp_local_mp_overlay_player(&s,1u,&bad,&dest));
    assert(!memcmp(out,original,sizeof(out)));
    bad=inputs[0];bad.pixels=dest.pixels;
    assert(!avp_local_mp_overlay_player(&s,1u,&bad,&dest));
    assert(!memcmp(out,original,sizeof(out)));
    bad=inputs[0];shortout.pitch=OUT_W-1u;
    assert(!avp_local_mp_overlay_player(&s,1u,&bad,&shortout));
    assert(!memcmp(out,original,sizeof(out)));
}
int main(void)
{
    setup_inputs();
    check_case(1,AVP_MP_SIDE_BY_SIDE);
    check_case(2,AVP_MP_SIDE_BY_SIDE);
    check_case(2,AVP_MP_TOP_BOTTOM);
    check_case(3,AVP_MP_SIDE_BY_SIDE);
    {AvpLocalMpSetup s;avp_local_mp_defaults(&s);s.players=3;s.three_layout=AVP_MP_THREE_WIDE_TOP;
     assert(avp_local_mp_compose(&s,inputs,&dest,blank)==3);check_rects(&s);}
    check_case(4,AVP_MP_SIDE_BY_SIDE);
    reject_bad_sources();
    check_terminal_owner_overlay(1u,AVP_MP_SIDE_BY_SIDE,0u);
    check_terminal_owner_overlay(2u,AVP_MP_SIDE_BY_SIDE,0u);
    check_terminal_owner_overlay(2u,AVP_MP_TOP_BOTTOM,0u);
    check_terminal_owner_overlay(3u,AVP_MP_SIDE_BY_SIDE,0u);
    check_terminal_owner_overlay(3u,AVP_MP_SIDE_BY_SIDE,AVP_MP_THREE_WIDE_TOP);
    check_terminal_owner_overlay(4u,AVP_MP_SIDE_BY_SIDE,0u);
    reject_bad_terminal_overlay();
    puts("local multiplayer full-frame/HUD and owner-only terminal: PASS (1..4, layouts, safety)");
    return 0;
}
