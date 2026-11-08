#include "local_mp_enhancement_buffer.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    AvpMpEnhancementBuffer buf={0};
    AvpMpPlayerCommand sample[AVP_LOCAL_MP_MAX_PLAYERS]={{0}};
    AvpMpPlayerCommand tick[AVP_LOCAL_MP_MAX_PLAYERS]={{0}};
    int checks=0;

    /* Capture a press/release between Jaguar ticks for P2, independent of
     * P1, and a short stick pulse for P3. Neither creates extra game ticks. */
    sample[0].digital_actions=1u<<AVP_MP_FORWARD;
    sample[0].mouse_dx=2;
    sample[1].digital_actions=1u<<AVP_MP_FIRE;
    sample[1].mouse_dx=9;
    sample[1].mouse_dy=-7;
    sample[2].analog_enabled=1;
    sample[2].analog_forward=0.75f;
    sample[2].analog_turn=-0.5f;
    sample[3].digital_actions=1u<<AVP_MP_USE;
    avp_mp_enhancement_buffer_poll(&buf,sample,4,1,1);
    memset(sample,0,sizeof(sample));
    sample[2].analog_enabled=1;
    sample[1].mouse_dx=3;
    sample[1].mouse_dy=-2;
    avp_mp_enhancement_buffer_poll(&buf,sample,4,1,1);
    tick[1].mouse_dx=4;
    tick[2].analog_enabled=1;
    avp_mp_enhancement_buffer_take(&buf,tick,4,1,1);
    assert(!(tick[0].actions&(1u<<AVP_MP_FORWARD))); checks++;
    assert(tick[0].mouse_dx==2); checks++;
    assert(tick[1].actions==(1u<<AVP_MP_FIRE)); checks++;
    assert(tick[1].digital_actions==(1u<<AVP_MP_FIRE)); checks++;
    assert(tick[1].mouse_dx==16&&tick[1].mouse_dy==-9); checks+=2;
    assert(tick[2].analog_forward==0.75f&&tick[2].analog_turn==-0.5f); checks+=2;
    assert(tick[3].actions==(1u<<AVP_MP_USE)); checks++;

    /* A second game tick must never replay an already-consumed impulse. */
    memset(tick,0,sizeof(tick));
    tick[2].analog_enabled=1;
    avp_mp_enhancement_buffer_take(&buf,tick,4,1,1);
    assert(tick[1].actions==0&&tick[1].mouse_dx==0); checks++;
    assert(tick[2].analog_forward==0&&tick[2].analog_turn==0); checks++;
    assert(tick[3].actions==0); checks++;

    /* Held live input wins over previous analog impulse, as in P1. */
    memset(sample,0,sizeof(sample));
    sample[2].analog_enabled=1;
    sample[2].analog_strafe=0.6f;
    avp_mp_enhancement_buffer_poll(&buf,sample,4,0,1);
    memset(tick,0,sizeof(tick));
    tick[2].analog_enabled=1;
    tick[2].analog_strafe=-0.8f;
    avp_mp_enhancement_buffer_take(&buf,tick,4,0,1);
    assert(tick[2].analog_strafe==-0.8f); checks++;

    /* No analogue buffering when the individual player has disabled it. */
    sample[1].analog_enabled=0;
    sample[1].analog_forward=0.9f;
    avp_mp_enhancement_buffer_poll(&buf,sample,4,0,1);
    memset(tick,0,sizeof(tick));
    tick[1].analog_enabled=0;
    avp_mp_enhancement_buffer_take(&buf,tick,4,0,1);
    assert(tick[1].analog_forward==0.0f); checks++;

    /* Focus-loss / F12 / pause cancels every pending player impulse. */
    memset(sample,0,sizeof(sample));
    sample[1].digital_actions=1u<<AVP_MP_FIRE;
    sample[2].analog_enabled=1;
    sample[2].analog_forward=0.5f;
    sample[3].mouse_dx=1234;
    avp_mp_enhancement_buffer_poll(&buf,sample,4,1,1);
    avp_mp_enhancement_buffer_reset(&buf);
    memset(tick,0,sizeof(tick));
    tick[2].analog_enabled=1;
    avp_mp_enhancement_buffer_take(&buf,tick,4,1,1);
    assert(tick[1].actions==0&&tick[2].analog_forward==0&&tick[3].mouse_dx==0); checks++;

    /* Saturation must be defined even after extreme raw-mouse motion. */
    assert(avp_mp_buffer_add_clamped(INT32_MAX,4)==INT32_MAX); checks++;
    assert(avp_mp_buffer_add_clamped(INT32_MIN,-4)==INT32_MIN); checks++;
    assert(avp_mp_buffer_add_clamped(17,-9)==8); checks++;

    puts("local co-op P2-P4 enhancement input buffering: PASS");
    printf("%d checks\n",checks);
    return 0;
}
