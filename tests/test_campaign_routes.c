#include "doors.h"
#include "levels.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/* Retail LEVDOOR.S connections used by all three campaigns.  This verifies
 * that the complete 15-level connection table remains bound and well-formed
 * without changing the player species' restrictions on lift/duct usage. */
static const AvpDoorLevelMeta *meta;
static unsigned nmeta;
void avp_doors_bind_level_meta(const AvpDoorLevelMeta *p,unsigned n)
{meta=p;nmeta=n;}

int main(void)
{
    unsigned seen[AVP_MAX_LEVEL]={0}, changed=1, total_exits=0;
    avp_doors_bind_retail_meta();
    assert(meta && nmeta==AVP_MAX_LEVEL);
    for(unsigned level=1;level<=AVP_MAX_LEVEL;++level){
        const AvpDoorLevelMeta *m=&meta[level-1];
        assert(m->exits && m->exit_count);
        for(size_t i=0;i<m->exit_count;++i){
            const AvpDoorExitEntry *e=&m->exits[i];
            assert(e->level>=1 && e->level<=AVP_MAX_LEVEL);
            assert(e->x<64 && e->y<64);
            assert(e->panel_offset<64u*64u*8u);
            ++total_exits;
        }
        for(size_t i=0;i<m->access_count;++i)
            assert(m->access[i].door_offset<64u*64u*8u);
        for(size_t i=0;i<m->interlock_count;++i){
            assert(m->interlocks[i].door_offset<64u*64u*8u);
            for(size_t j=0;j<m->interlocks[i].required_count;++j)
                assert(m->interlocks[i].required_offsets[j]<64u*64u*8u);
        }
    }
    assert(total_exits>=30);
    /* Global route graph, including the vertical lifts on sublevels 1..5.
     * Species-specific lift/duct restrictions are tested in the door logic. */
    seen[2]=1;
    while(changed){
        changed=0;
        for(unsigned level=1;level<=AVP_MAX_LEVEL;++level){
            const AvpDoorLevelMeta *m=&meta[level-1];
            if(!seen[level-1])continue;
            if(level<=5){
                if(level>1 && !seen[level-2]){seen[level-2]=1;changed=1;}
                if(level<5 && !seen[level]){seen[level]=1;changed=1;}
            }
            for(size_t i=0;i<m->exit_count;++i){
                unsigned dst=m->exits[i].level-1u;
                if(!seen[dst]){seen[dst]=1;changed=1;}
            }
        }
    }
    for(unsigned i=0;i<AVP_MAX_LEVEL;++i)assert(seen[i]);
    assert(meta[13].exit_count && meta[13].exits[0].level==5);
    assert(meta[14].exit_count && meta[14].exits[0].level==1);
    return 0;
}
