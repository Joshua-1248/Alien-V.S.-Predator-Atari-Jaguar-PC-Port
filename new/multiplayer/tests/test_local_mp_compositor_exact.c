/* RE31: compare the optimized PC-only compositor against the exact R2
 * 64-bit integer nearest-neighbor sampling formula (independent reference).
 * Use odd/even layouts, small and large partitions, padding and overlay. */
#include "local_mp_compositor.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t seed=0x31415926u;
static uint32_t random32(void)
{
    seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
    return seed;
}
static void reference_rectangle(uint32_t *dest,unsigned pitch,
                                const AvpLocalMpRect *rect,
                                const AvpLocalMpSourceFrame *frame)
{
    for(unsigned y=0;y<rect->height;y++){
        const unsigned sy=rect->height==1u ? frame->height-1u :
            (unsigned)((uint64_t)y*(frame->height-1u)/(rect->height-1u));
        for(unsigned x=0;x<rect->width;x++){
            const unsigned sx=rect->width==1u ? 0u :
                (unsigned)((uint64_t)x*(frame->width-1u)/(rect->width-1u));
            dest[(size_t)(rect->y+y)*pitch+rect->x+x]=
                frame->pixels[(size_t)sy*frame->pitch+sx];
        }
    }
}
static void require(int condition,unsigned case_id,const char *what)
{
    if(!condition){fprintf(stderr,"compositor exactness case %u: %s\n",case_id,what);exit(1);}
}
static void run_case(unsigned case_id,unsigned players,unsigned layout,
                     unsigned three,unsigned ow,unsigned oh,
                     unsigned sw,unsigned sh)
{
    AvpLocalMpSetup setup;
    AvpLocalMpRect rect[AVP_LOCAL_MP_MAX_PLAYERS];
    AvpLocalMpSourceFrame frames[AVP_LOCAL_MP_MAX_PLAYERS]={{0}},overlay;
    uint32_t *dest,*expected,*extra;
    const unsigned opitch=ow+3u,spitch=sw+2u;
    const size_t on=(size_t)opitch*oh,sn=(size_t)spitch*sh;
    const uint32_t color=0x00467389u^(case_id<<8);
    unsigned count;
    avp_local_mp_defaults(&setup);
    setup.players=players;setup.layout=layout;setup.three_layout=three;
    dest=malloc(on*sizeof(*dest));expected=malloc(on*sizeof(*expected));
    extra=malloc(sn*sizeof(*extra));
    require(dest!=NULL && expected!=NULL && extra!=NULL,case_id,"allocation");
    for(unsigned p=0;p<players;p++){
        uint32_t *pixels=malloc(sn*sizeof(*pixels));
        require(pixels!=NULL,case_id,"frame allocation");
        for(size_t j=0;j<sn;j++)pixels[j]=random32()^(p<<24);
        frames[p]=(AvpLocalMpSourceFrame){pixels,sw,sh,spitch};
    }
    for(size_t j=0;j<sn;j++)extra[j]=0x00df1200u^(uint32_t)j^(case_id<<6);
    overlay=(AvpLocalMpSourceFrame){extra,sw,sh,spitch};
    for(size_t j=0;j<on;j++)dest[j]=expected[j]=0xccbcadadu;
    count=avp_local_mp_viewports(&setup,ow,oh,rect);
    require(count==players,case_id,"viewport layout");
    /* The R2 reference clears every visible output pixel (not padding). */
    for(unsigned y=0;y<oh;y++)
        for(unsigned x=0;x<ow;x++)expected[(size_t)y*opitch+x]=color;
    for(unsigned p=0;p<players;p++){
        require(rect[p].width>0u&&rect[p].height>0u,case_id,"partition empty");
        reference_rectangle(expected,opitch,&rect[p],&frames[p]);
    }
    const AvpLocalMpOutputFrame output={dest,ow,oh,opitch};
    require(avp_local_mp_compose(&setup,frames,&output,color)==players,
            case_id,"compose failure");
    require(memcmp(dest,expected,on*sizeof(*dest))==0,
            case_id,"composed pixels disagree with R2 reference");
    /* The original synchronous terminal must only update its owner's view. */
    const unsigned owner=case_id%players;
    reference_rectangle(expected,opitch,&rect[owner],&overlay);
    require(avp_local_mp_overlay_player(&setup,owner,&overlay,&output),
            case_id,"overlay failure");
    require(memcmp(dest,expected,on*sizeof(*dest))==0,
            case_id,"terminal pixels disagree with R2 reference");
    for(unsigned p=0;p<players;p++)free((void *)frames[p].pixels);
    free(extra);free(expected);free(dest);
}
int main(void)
{
    unsigned count=0;
    for(unsigned p=1u;p<=4u;p++){
        for(unsigned layout=0u;layout<=1u;layout++){
            for(unsigned three=0u;three<=1u;three++){
                /* Include width/height 1 and exact copy, down- and upscale. */
                for(unsigned i=0u;i<80u;i++){
                    const unsigned ow=2u+random32()%97u;
                    const unsigned oh=2u+random32()%71u;
                    const unsigned sw=1u+random32()%111u;
                    const unsigned sh=1u+random32()%109u;
                    run_case(count++,p,layout,three,ow,oh,sw,sh);
                }
            }
        }
    }
    /* High-resolution per-pixel check, including three-player black gap. */
    run_case(count++,3u,AVP_MP_SIDE_BY_SIDE,AVP_MP_THREE_QUADRANTS,
             1920u,1080u,320u,228u);
    run_case(count++,4u,AVP_MP_TOP_BOTTOM,AVP_MP_THREE_WIDE_TOP,
             1920u,1080u,1920u,540u);
    /* RE31 R6: exercise the precomputed horizontal-index fast path and
     * the >4096-pixel fallback against independent 64-bit reference math.
     * Include 1440p 1/3/4-player layouts and one-row terminal overlays. */
    run_case(count++,1u,AVP_MP_SIDE_BY_SIDE,AVP_MP_THREE_QUADRANTS,
             2560u,1440u,320u,228u);
    run_case(count++,3u,AVP_MP_SIDE_BY_SIDE,AVP_MP_THREE_QUADRANTS,
             2560u,1440u,320u,228u);
    run_case(count++,3u,AVP_MP_TOP_BOTTOM,AVP_MP_THREE_WIDE_TOP,
             2560u,1440u,320u,228u);
    run_case(count++,4u,AVP_MP_SIDE_BY_SIDE,AVP_MP_THREE_QUADRANTS,
             2560u,1440u,320u,228u);
    run_case(count++,4u,AVP_MP_TOP_BOTTOM,AVP_MP_THREE_WIDE_TOP,
             2560u,1440u,320u,228u);
    run_case(count++,1u,AVP_MP_SIDE_BY_SIDE,AVP_MP_THREE_QUADRANTS,
             4096u,7u,347u,5u);
    run_case(count++,1u,AVP_MP_SIDE_BY_SIDE,AVP_MP_THREE_QUADRANTS,
             4097u,7u,347u,5u);
    run_case(count++,1u,AVP_MP_SIDE_BY_SIDE,AVP_MP_THREE_QUADRANTS,
             5001u,1u,1u,1u);
    run_case(count++,1u,AVP_MP_SIDE_BY_SIDE,AVP_MP_THREE_QUADRANTS,
             17u,1u,243u,7u);
    printf("PC compositor exact reference pixels/terminal/padding: PASS (%u layouts)\n",count);
    return 0;
}
