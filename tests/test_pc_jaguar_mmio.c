/* Host-only 68000 MMIO bus tests, with a synthetic monotonic clock.
 * No Jaguar retail ROM or gameplay instrumentation is required. */
#include "jaguar_hw.h"
#include "mjp.h"
#include <assert.h>
#include <time.h>
static unsigned reads;
int avp_test_clock_gettime(clockid_t unused,struct timespec *ts)
{
    (void)unused;
    ts->tv_sec=0;
    ts->tv_nsec=(reads++==0u)?0x0001ffffu:0x00020000u;
    return 0;
}
void initpad(void){}
void avp_mjp_set_lines(s32 a,s32 b){(void)a;(void)b;}
void avp_mjp_bind_objects(AvpMjpObject *p,unsigned n){(void)p;(void)n;}
int main(void)
{
    assert(avp_host_mmio_read8(JAG_CONFIG)==0x00u);
    assert(avp_host_mmio_read8(JAG_CONFIG+1u)==0x10u);
    assert(avp_host_mmio_read16(JAG_CONFIG)==0x0010u);
    reads=0;
    /* 68000 MOVE.L at TOM HC: first 16 bits are HC, second VC; one
     * transaction must not change its timestamp between the two words. */
    assert(avp_host_mmio_read32(0x00f00004u)==0xffff0001u);
    assert(reads==1u);
    assert(avp_host_mmio_read32(JAG_CONFIG-2u)==0x00000010u);
    return 0;
}
