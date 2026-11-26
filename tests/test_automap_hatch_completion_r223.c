/* RE41 R223: PC-only automap diagonal hatch completion. The original
 * 512x512 raster must not be modified, and navigable floors must stay clear. */
#include "jaguar/avp_gpu/state.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void hatch_tile(AvpAvpGpuState *s,unsigned x,unsigned y)
{
    static const u8 pat[4]={0x11u,0x22u,0x44u,0x88u};
    for(unsigned row=0;row<8u;row++)
        s->automap_bits[(y*8u+row)*AVP_AUTOMAP_ROW_BYTES+x]=pat[row&3u];
}
static void all_floors(u8 *maze,unsigned w,unsigned h,u8 floor_id)
{
    memset(maze,0,w*h*8u);
    for(unsigned i=0;i<w*h;i++)maze[i*8u+5u]=floor_id;
}
static void set_floor(u8 *maze,unsigned w,unsigned x,unsigned y,u8 floor_id)
{
    maze[((size_t)y*w+x)*8u+5u]=floor_id;
}
#define AT(x,y) mask[(y)*64u+(x)]
int main(void)
{
    AvpAvpGpuState *s=(AvpAvpGpuState *)calloc(1,sizeof(*s));
    u8 maze[8u*8u*8u],mask[64u*64u],previous[AVP_AUTOMAP_RASTER_BYTES];
    assert(s);
    /* 3x3 void island inside already drawn hatch ring (9-tile hole). */
    all_floors(maze,8,8,1u);
    for(unsigned y=1;y<=5;y++)for(unsigned x=1;x<=5;x++){
        set_floor(maze,8,x,y,0u);
        if(x==1u||x==5u||y==1u||y==5u)hatch_tile(s,x,y);
    }
    memcpy(previous,s->automap_bits,sizeof previous);
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    for(unsigned y=2;y<=4;y++)for(unsigned x=2;x<=4;x++)assert(AT(x,y));
    assert(!AT(0,0)&&!AT(6,6));
    assert(!AT(1,1)); /* Already painted; no completion flag needed. */
    assert(!memcmp(previous,s->automap_bits,sizeof previous));
    /* Traced walking corridor through the apparent hole stays CLEAR. */
    set_floor(maze,8,3,3,1u);
    s->automap_seen[3u*64u+3u]=1u;
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(!AT(3,3));
    assert(!memcmp(previous,s->automap_bits,sizeof previous));
    s->automap_seen[3u*64u+3u]=0u;
    /* Missing outer corner: two original perpendicular hatch neighbors. */
    all_floors(maze,8,8,1u);memset(s->automap_bits,0,sizeof s->automap_bits);
    set_floor(maze,8,4,4,0u);
    hatch_tile(s,3,4);hatch_tile(s,4,3);
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(AT(4,4));
    /* One lonely stripe cannot reveal the otherwise undiscovered exterior. */
    memset(s->automap_bits,0,sizeof s->automap_bits);
    hatch_tile(s,3,4);
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(!AT(4,4));
    /* Two visible diagonal neighbors without a cardinal side still cannot
     * cause a false corner: only source-confirmed structural void may fill. */
    memset(s->automap_bits,0,sizeof s->automap_bits);
    hatch_tile(s,3,3);hatch_tile(s,5,5);
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(!AT(4,4));
    /* Boundary handling is bounded and never extends beyond maze width. */
    all_floors(maze,8,8,1u);memset(s->automap_bits,0,sizeof s->automap_bits);
    set_floor(maze,8,7,7,0u);hatch_tile(s,6,7);hatch_tile(s,7,6);
    avp_gpu_automap_hatch_completion(s,maze,8,8,mask);
    assert(AT(7,7));assert(!AT(8,7));
    avp_gpu_automap_hatch_completion(s,maze,0,8,mask);
    for(size_t j=0;j<sizeof mask;j++)assert(mask[j]==0);
    free(s);
    return 0;
}
