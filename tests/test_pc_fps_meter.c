#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "pc/pc_fps_meter.h"

static void check_rate(unsigned expected)
{
    AvpPcFpsMeter m={0};
    const uint64_t t0=UINT64_C(10000000000);
    avp_pc_fps_meter_present(&m,t0);
    assert(!m.sample_valid);
    for(unsigned i=1;i<expected;i++)
        avp_pc_fps_meter_present(&m,t0+(UINT64_C(1000000000)*i)/expected);
    avp_pc_fps_meter_present(&m,t0+UINT64_C(1000000000));
    assert(m.sample_valid);
    assert(m.display_fps==expected);
    avp_pc_fps_meter_reset(&m);
    assert(!m.sample_valid && !m.frames && !m.start_ns);
}
int main(void)
{
    static const unsigned rates[]={24,30,50,60,75,85,90,100,120,144,165,180,
        200,240,280,300,360,480,500,540,1000};
    for(unsigned i=0;i<sizeof(rates)/sizeof(rates[0]);i++)check_rate(rates[i]);
    {
        AvpPcFpsMeter m={0};
        avp_pc_fps_meter_present(&m,2000000000u);
        avp_pc_fps_meter_present(&m,1000000000u); /* backward clock reset */
        assert(!m.sample_valid && m.frames==0u);
        avp_pc_fps_meter_present(&m,2500000000u);
        assert(m.sample_valid && m.display_fps==1u);
    }
    puts("PC host FPS meter tests PASS (21 presentation rates)");
    return 0;
}
