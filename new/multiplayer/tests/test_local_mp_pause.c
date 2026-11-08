#include "local_mp_pause.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"pause failure line %d: %s\n",__LINE__,#x);return 1;}} while(0)
int main(void)
{
    AvpMpPause p={0};
    CHECK(!avp_mp_pause_step(&p,0u)&&!p.paused);
    CHECK(avp_mp_pause_step(&p,1u)&&p.paused);
    CHECK(!avp_mp_pause_step(&p,1u)&&p.paused);
    CHECK(!avp_mp_pause_step(&p,0u)&&p.paused);
    CHECK(avp_mp_pause_step(&p,1u<<3)&&!p.paused);
    CHECK(!avp_mp_pause_step(&p,1u<<3)&&!p.paused);
    CHECK(!avp_mp_pause_step(&p,0u));
    CHECK(avp_mp_pause_step(&p,3u)&&p.paused);
    CHECK(!avp_mp_pause_step(&p,3u)&&p.paused);
    CHECK(!avp_mp_pause_step(&p,0u));
    CHECK(avp_mp_pause_step(&p,1u<<2)&&!p.paused);
    puts("PASS: global co-op pause rising-edge toggle, all participants, no held-key retrigger");
    return 0;
}
