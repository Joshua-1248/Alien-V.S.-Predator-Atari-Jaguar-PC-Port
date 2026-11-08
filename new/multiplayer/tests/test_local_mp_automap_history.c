/* Integration regression: the authentic Jaguar automap observer is used;
 * only snapshots of already revealed cells may survive a level change. */
#include "jaguar/avp_gpu/state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do {if(!(e)){fprintf(stderr,"automap check failed line %d: %s\n",__LINE__,#e);return 1;}}while(0)
static void visit(AvpGpuAutomapHistory *h,AvpAvpGpuState *s,unsigned level,int enabled,
                  const u8 *maze,unsigned x,unsigned y)
{
    avp_gpu_automap_history_enter(h,s,level,enabled);
    avp_gpu_automap_observe(s,maze,64u,64u,
        ((u32)x<<16)|0x8000u,((u32)y<<16)|0x8000u,0u,level);
}
int main(void)
{
    AvpGpuAutomapHistory h[4]={0};
    AvpAvpGpuState *s[4]={0};
    const size_t a=12u*64u+12u,b=35u*64u+35u,c=41u*64u+41u;
    unsigned char *maze=calloc(64u*64u,8u);
    CHECK(maze);
    for(unsigned i=0;i<4;i++){s[i]=calloc(1,sizeof(*s[i]));CHECK(s[i]);}
    visit(&h[0],s[0],1u,1,maze,12u,12u);
    CHECK(s[0]->automap_seen[a]);
    CHECK(!s[0]->automap_seen[b]);
    visit(&h[0],s[0],2u,1,maze,35u,35u);
    CHECK(s[0]->automap_level==2u && s[0]->automap_seen[b]);
    CHECK(!s[0]->automap_seen[a]);
    visit(&h[1],s[1],1u,1,maze,41u,41u);
    CHECK(s[1]->automap_seen[c] && !s[1]->automap_seen[a]);
    visit(&h[0],s[0],1u,1,maze,12u,12u);
    CHECK(s[0]->automap_level==1u && s[0]->automap_seen[a]);
    CHECK(!s[0]->automap_seen[b] && !s[0]->automap_seen[c]);
    /* A fresh viewer never borrows discoveries from another actor. */
    visit(&h[2],s[2],1u,1,maze,35u,35u);
    CHECK(s[2]->automap_seen[b] && !s[2]->automap_seen[a]);
    /* Opt-out retains original automap reset-on-level-change semantics. */
    visit(&h[3],s[3],1u,0,maze,12u,12u);
    visit(&h[3],s[3],2u,0,maze,35u,35u);
    visit(&h[3],s[3],1u,0,maze,41u,41u);
    CHECK(s[3]->automap_seen[c] && !s[3]->automap_seen[a] && !s[3]->automap_seen[b]);
    CHECK(!h[3].level[0] && !h[3].level[1]);
    /* R17: Reveal Whole Map exposes authored geometry to this player ONLY.
     * Another human has an independent map and separate level histories. */
    maze[b*8u+5u]=1u;
    CHECK(!s[0]->automap_seen[b]);
    avp_gpu_automap_reveal_all(s[0],maze,64u,64u,1u);
    CHECK(s[0]->automap_seen[b]);
    CHECK(!s[1]->automap_seen[b]);
    CHECK(!s[2]->automap_seen[a]);
    visit(&h[0],s[0],2u,1,maze,35u,35u);
    visit(&h[0],s[0],1u,1,maze,12u,12u);
    CHECK(s[0]->automap_seen[b]);
    /* Each return must preserve native 1-bit discovery strokes, not a full map. */
    CHECK(h[0].level[1] && h[0].level[1]->level==2u);
    for(unsigned i=0;i<4;i++){avp_gpu_automap_history_destroy(&h[i]);free(s[i]);}
    free(maze);
    puts("PASS automap: original observer, 4 isolated player histories, level return, opt-out");
    return 0;
}
