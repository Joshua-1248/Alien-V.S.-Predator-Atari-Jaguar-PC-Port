/* R303: integer Jaguar map sampling is unchanged; high-res views gain
 * subpixel access to the SAME 512x512 explored-map raster. */
#include "jaguar/avp_gpu/hires_automap_sample.h"
#include "jaguar/avp_gpu/state.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static unsigned rng=0x1234abcd;
static unsigned next(void){ rng=rng*1664525u+1013904223u;return rng; }

static void check_integer_jaguar_parity(void)
{
    /* All four camera quadrants, signed cos/sine, varied map viewport sizes,
     * zoom 1x. Double can represent all 16.16 blitter products exactly at
     * the tested real-world magnitude. */
    for(unsigned i=0;i<40000u;i++){
        const uint32_t xpos=(uint32_t)((int32_t)((int)(next()%58u)-12)*65536+(int)(next()&65535u));
        const uint32_t ypos=(uint32_t)((int32_t)((int)(next()%58u)-12)*65536+(int)(next()&65535u));
        const int32_t sine=(int32_t)(next()%65536u)-32768;
        const int32_t cosine=(int32_t)(next()%65536u)-32768;
        const unsigned w=100u+next()%30u, h=100u+next()%30u;
        const int px=(int)(next()%200u)-50,py=(int)(next()%200u)-50;
        int original_x,original_y,host_x,host_y;
        avp_gpu_automap_sample(xpos,ypos,sine,cosine,w,h,px,py,&original_x,&original_y);
        avp_gpu_hires_automap_sample(xpos,ypos,sine,cosine,w,h,
                                    (double)px,(double)py,&host_x,&host_y);
        assert(host_x==original_x&&host_y==original_y);
    }
}

static void check_spans(void)
{
    static const unsigned dims[][2]={{320,228},{640,456},{1280,912},{1279,911},{120,120},{1920,1080}};
    for(unsigned j=0;j<sizeof dims/sizeof dims[0];j++){
        for(unsigned axis=0;axis<2;axis++){
            unsigned host=dims[j][axis],native=axis?228u:320u;
            unsigned prev=0;
            assert(avp_gpu_hires_automap_edge(0u,host,native)==0u);
            assert(avp_gpu_hires_automap_edge(native,host,native)==host);
            for(unsigned n=0;n<native;n++){
                unsigned a=avp_gpu_hires_automap_edge(n,host,native);
                unsigned b=avp_gpu_hires_automap_edge(n+1u,host,native);
                assert(a==prev&&b>=a&&b<=host);
                for(unsigned x=a;x<b;x++)assert((uint64_t)x*native/host==n);
                prev=b;
            }
        }
    }
}

static void check_subpixel_improvement(void)
{
    const uint32_t xpos=0x00118000u,ypos=0x00178000u;
    const int32_t sn=23170,cs=23170; /* rotated 45 degrees */
    unsigned distinctions=0,zoom_distinctions=0;
    for(int y=25;y<90;y++)for(int x=25;x<90;x++){
        int ox,oy,a0,b0,a1,b1;
        avp_gpu_automap_sample(xpos,ypos,sn,cs,120u,128u,x,y,&ox,&oy);
        avp_gpu_hires_automap_sample(xpos,ypos,sn,cs,120u,128u,
                                    (double)x-0.25,(double)y-0.25,&a0,&b0);
        avp_gpu_hires_automap_sample(xpos,ypos,sn,cs,120u,128u,
                                    (double)x+0.25,(double)y+0.25,&a1,&b1);
        /* One original HUD output pixel is replicated at 2x. But source
         * map addresses must be free to differ at real host pixel centres. */
        if(a0!=a1||b0!=b1)distinctions++;
        /* Zoomed map should also retain fractional transforms. */
        avp_gpu_hires_automap_sample(xpos,ypos,sn,cs,120u,128u,
                                    ((double)x-0.25)/1.25,
                                    ((double)y-0.25)/1.25,&a0,&b0);
        avp_gpu_hires_automap_sample(xpos,ypos,sn,cs,120u,128u,
                                    ((double)x+0.25)/1.25,
                                    ((double)y+0.25)/1.25,&a1,&b1);
        if(a0!=a1||b0!=b1)zoom_distinctions++;
    }
    assert(distinctions>100u);
    assert(zoom_distinctions>100u);
}


static void check_fast_scanlines(void)
{
    for(unsigned i=0;i<1000u;i++){
        uint32_t px=(next()%61u)*65536u+(next()&65535u);
        uint32_t py=(next()%61u)*65536u+(next()&65535u);
        int32_t sn=(int32_t)(next()%65536u)-32768;
        int32_t cs=(int32_t)(next()%65536u)-32768;
        unsigned w=100u+next()%35u,h=100u+next()%40u;
        const double sx=(double)(next()%124u)+0.375;
        const double sy=(double)(next()%122u)+0.625;
        const double step=(next()%4u+1u)/8.0;
        AvpGpuHiresAutomapRow row;
        avp_gpu_hires_automap_row_begin(&row,px,py,sn,cs,w,h,sx,sy,step);
        for(int j=0;j<320;j++){
            int x,y,reference_x,reference_y;
            avp_gpu_hires_automap_row_next(&row,&x,&y);
            avp_gpu_hires_automap_sample(px,py,sn,cs,w,h,
                sx+(double)j*step,sy,&reference_x,&reference_y);
            assert(x==reference_x&&y==reference_y);
        }
    }
}

int main(void)
{
    check_integer_jaguar_parity();
    check_spans();
    check_subpixel_improvement();
    check_fast_scanlines();
    puts("R303 automap: native blitter parity, HUD span mapping, fractional hires + zoom PASS");
    return 0;
}
