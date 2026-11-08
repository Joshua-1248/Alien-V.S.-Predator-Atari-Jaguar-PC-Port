#include "jaguar/cry.h"
#include <assert.h>
int main(void)
{
    assert(avp_jaguar_srcshade_cry(0x5a80u,10)==0x5a8au);
    assert(avp_jaguar_srcshade_cry(0x5afau,20)==0x5affu);
    assert(avp_jaguar_srcshade_cry(0x5a05u,-20)==0x5a00u);
    return 0;
}
