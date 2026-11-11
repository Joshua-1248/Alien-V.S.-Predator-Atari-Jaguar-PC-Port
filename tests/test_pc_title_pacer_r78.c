/* Clock arithmetic only: R78 title-only 60-Hz deadline pacing. */
#include "pc/pc_title_reference.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    const uint64_t t=1000000000ull;
    uint64_t d=avp_pc_title_next_deadline(0u,t);
    assert(d==t+16666667ull);
    d=avp_pc_title_next_deadline(d,t+5000000ull);
    assert(d==t+33333334ull); /* rendering 5ms doesn't add 5ms to field */
    d=avp_pc_title_next_deadline(d,t+40000000ull);
    assert(d==t+50000001ull); /* behind schedule: no frame discarded */
    d=avp_pc_title_next_deadline(d,t+2000000000ull);
    assert(d==t+2016666667ull); /* stale time base re-synchronizes */
    puts("R78 title-only video pacing deadline arithmetic PASS");
    return 0;
}
