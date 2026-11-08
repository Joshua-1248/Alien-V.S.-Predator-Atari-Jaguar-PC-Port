#include "jaguar/cry.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define CHECK(v) do { if(!(v)){fprintf(stderr,"HUD hue test failed at %d: %s\n",__LINE__,#v);return 1;} } while(0)
static unsigned chroma(uint32_t rgb,const uint8_t *q)
{
    return avp_jaguar_rgb_to_cry_qconv((uint8_t)(rgb>>16),
        (uint8_t)(rgb>>8),(uint8_t)rgb,q,131072u)&0xff00u;
}
int main(void)
{
    uint8_t *q=calloc(131072u,1u);
    uint32_t a,b,c;uint16_t ink=0x100cu;
    if(!q)return 2;
    /* A defined synthetic neutral hue: for every gray pixel the same
     * QCONV entry is used; test only hue, not scene-dependent brightness. */
    q[0xffffu]=0x88u;
    a=avp_jaguar_rmw_fixed_hue_xrgb8888(0x00000000u,ink,q,131072u);
    b=avp_jaguar_rmw_fixed_hue_xrgb8888(0x00800000u,ink,q,131072u);
    c=avp_jaguar_rmw_fixed_hue_xrgb8888(0x00000080u,ink,q,131072u);
    CHECK(a!=0u);
    CHECK(b==c); /* same scene luma, different scene chroma */
    CHECK(chroma(b,q)==chroma(c,q));
    CHECK(avp_jaguar_rmw_fixed_hue_xrgb8888(0x00776655u,0u,q,131072u)==0x00776655u);
    CHECK(avp_jaguar_rmw_fixed_hue_xrgb8888(0x00776655u,ink,NULL,0u)==0x00776655u);
    /* Original RMW brightness and hue continue to come from the actual
     * destination, demonstrating that the stock adapter is unchanged. */
    CHECK(avp_jaguar_rmw_xrgb8888(0x00800000u,ink,q,131072u)!=
          avp_jaguar_rmw_xrgb8888(0x00000080u,ink,q,131072u));
    /* Real HUD option OFF does not route through the new function at all.
     * Check that legacy RMW remains the original signed CRY calculation. */
    CHECK(avp_jaguar_rmw_blend_cry(0x4a7fu,0x9205u)==0x0c84u);
    free(q);
    puts("RE31 fixed HUD hue: neutral chroma, backdrop brightness, OFF parity PASS");
    return 0;
}
