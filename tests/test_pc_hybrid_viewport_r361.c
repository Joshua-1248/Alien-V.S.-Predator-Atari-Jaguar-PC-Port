#include "pc/pc_hybrid_viewport.h"
#include <stdio.h>
#include <stdint.h>
#define CHECK(expr) do { if(!(expr)) { \
    fprintf(stderr,"R361 viewport test failed: %s at %d\n",#expr,__LINE__); \
    return 1; \
} } while(0)
int main(void)
{
    AvpHybridViewport v={0};uint32_t x=0,y=0;
    CHECK(avp_hybrid_viewport_make(2560,1440,1920,1440,320,228,&v));
    CHECK(v.x==320&&v.y==0&&v.width==1920&&v.height==1440);
    CHECK(!avp_hybrid_viewport_source_pixel(&v,319,0,&x,&y));
    CHECK(avp_hybrid_viewport_source_pixel(&v,320,0,&x,&y));
    CHECK(x==0&&y==0);
    CHECK(avp_hybrid_viewport_source_pixel(&v,2239,1439,&x,&y));
    CHECK(x==319&&y==227);
    CHECK(!avp_hybrid_viewport_source_pixel(&v,2240,1439,&x,&y));
    /* Every GPU destination pixel uses the same unsigned integer source
     * division as avp_pc_scale_nearest32, with no floating-point rounding. */
    for(uint32_t py=0;py<1440;py+=13u)
        for(uint32_t px=320;px<2240;px+=7u){
            CHECK(avp_hybrid_viewport_source_pixel(&v,px,py,&x,&y));
            CHECK(x==(px-320u)*320u/1920u);
            CHECK(y==py*228u/1440u);
        }
    CHECK(avp_hybrid_viewport_make(501,333,501,333,501,333,&v));
    CHECK(avp_hybrid_viewport_source_pixel(&v,250,125,&x,&y));
    CHECK(x==250&&y==125);
    CHECK(!avp_hybrid_viewport_make(300,200,301,200,320,228,&v));
    CHECK(!avp_hybrid_viewport_make(0,200,1,200,320,228,&v));
    CHECK(!avp_hybrid_viewport_make(500,500,320,228,0,228,&v));
    puts("R361 X11/Vulkan center-and-nearest viewport parity PASS");
    return 0;
}
