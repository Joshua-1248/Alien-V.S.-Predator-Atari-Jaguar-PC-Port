#include "local_mp_scoreboard_layout.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

static void verify_pages(unsigned width,unsigned height,unsigned cell_width,
    unsigned players)
{
    AvpMpDmScoreLayout layout=avp_mp_dm_score_layout(width,height,cell_width,players,0u);
    unsigned seen[AVP_LOCAL_MP_MAX_ACTORS]={0};
    assert(layout.pages>=1u);
    for(unsigned p=0;p<layout.pages;p++){
        AvpMpDmScoreLayout page=avp_mp_dm_score_layout(width,height,cell_width,players,
                                                        (uint64_t)p*50u);
        assert(page.page==p);
        assert(page.columns>=1u);
        assert(page.panel_height<=height);
        assert(page.rows*page.columns>=page.last-page.first);
        for(unsigned n=page.first;n<page.last;n++){
            assert(n<players);
            assert(!seen[n]++);
        }
    }
    for(unsigned n=0;n<players;n++)assert(seen[n]==1u);
}
int main(void)
{
    AvpMpDmScoreLayout a;
    verify_pages(320,228,92,36);
    a=avp_mp_dm_score_layout(320,228,92,36,0);
    assert(a.pages==1u && a.columns==3u && a.rows==12u);
    verify_pages(640,480,92,36);
    verify_pages(1280,720,92,36);
    verify_pages(2560,1440,92,36);
    verify_pages(320,228,150,36); /* long scores -> all pages accessible */
    verify_pages(160,120,92,36); /* tiny window -> rotate */
    verify_pages(320,228,92,1);
    verify_pages(320,228,92,4);
    a=avp_mp_dm_score_layout(320,228,92,36,50u);
    assert(a.page==0u); /* non-paged overlay never rotates */
    puts("R204 responsive full-roster Deathmatch scoreboard layout PASS");
    return 0;
}
