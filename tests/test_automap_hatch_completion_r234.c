/* RE41 R234: holes and interrupted rims can be floor-textured and
 * one-step corner repairs must precede the enclosure flood. */
#include "jaguar/avp_gpu/state.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define SLOT(x,y) mask[(unsigned)(y)*64u+(unsigned)(x)]
static void floor_grid(u8 *maze,unsigned w,unsigned h,u8 floor)
{
    memset(maze,0,(size_t)w*h*8u);
    for(unsigned i=0;i<w*h;i++)maze[(size_t)i*8u+5u]=floor;
}
static void hatch(AvpAvpGpuState *s,unsigned x,unsigned y)
{
    static const u8 pat[4]={0x11u,0x22u,0x44u,0x88u};
    for(unsigned row=0;row<8u;row++)
        s->automap_bits[((size_t)y*8u+row)*AVP_AUTOMAP_ROW_BYTES+x]=pat[row&3u];
}
static void ring(AvpAvpGpuState *s,unsigned x0,unsigned y0,unsigned x1,unsigned y1)
{
    for(unsigned y=y0;y<=y1;y++)for(unsigned x=x0;x<=x1;x++)
        if(x==x0||x==x1||y==y0||y==y1)hatch(s,x,y);
}
int main(void)
{
    AvpAvpGpuState *s=calloc(1,sizeof *s);
    u8 maze[8u*8u*8u],mask[64u*64u],previous[AVP_AUTOMAP_RASTER_BYTES];
    assert(s);

    /* The author's textured (nonzero) floor byte is not proof that
     * an enclosed *unseen* tile is a walkable/observed corridor. */
    floor_grid(maze,8,8,1u); ring(s,1,1,5,5);
    memcpy(previous,s->automap_bits,sizeof previous);
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    for(unsigned y=2;y<=4;y++)for(unsigned x=2;x<=4;x++)assert(SLOT(x,y));
    assert(!SLOT(0,0)&&!SLOT(6,6));
    assert(!memcmp(previous,s->automap_bits,sizeof previous));

    /* Explored actual floor must not disappear just because hatch surrounds it. */
    s->automap_seen[3u*64u+3u]=1u;
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(!SLOT(3,3));

    /* An interrupted edge of the enclosing rim must be repaired BEFORE
     * determining whether the interior region leaks to the exterior. */
    memset(s->automap_bits,0,sizeof s->automap_bits);
    memset(s->automap_seen,0,sizeof s->automap_seen);
    ring(s,1,1,5,5);
    memset(s->automap_bits+((size_t)1u*8u)*AVP_AUTOMAP_ROW_BYTES+3u,0,1u);
    for(unsigned row=1;row<8u;row++)
        s->automap_bits[((size_t)1u*8u+row)*AVP_AUTOMAP_ROW_BYTES+3u]=0u;
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(SLOT(3,1));
    for(unsigned y=2;y<=4;y++)for(unsigned x=2;x<=4;x++)assert(SLOT(x,y));

    /* Two original hatch anchors and visible parallel support can bridge
     * a short, one-cell-wide interruption without changing raster bytes. */
    memset(s->automap_bits,0,sizeof s->automap_bits);
    hatch(s,1,4);hatch(s,5,4);hatch(s,3,3);
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(SLOT(2,4)&&SLOT(3,4)&&SLOT(4,4));
    /* But a lone pair with no parallel/rim support must never fill
     * an undiscovered open corridor. */
    memset(s->automap_bits,0,sizeof s->automap_bits);
    hatch(s,1,4);hatch(s,5,4);
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(!SLOT(2,4)&&!SLOT(3,4)&&!SLOT(4,4));

    memset(s->automap_bits,0,sizeof s->automap_bits);
    hatch(s,3,4);hatch(s,4,3);
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(SLOT(4,4)); /* Original R223 missing outer corner on textured tile. */
    memset(s->automap_bits,0,sizeof s->automap_bits);
    hatch(s,3,4);
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(!SLOT(4,4));
    free(s);
    return 0;
}
