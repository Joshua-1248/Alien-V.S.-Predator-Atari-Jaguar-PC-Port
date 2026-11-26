#include "local_mp_spycam.h"
#include "local_mp_life.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    uint32_t held=0;
    const uint32_t next=1u<<AVP_MP_SPY_NEXT_PLAYER;
    const uint32_t prev=1u<<AVP_MP_SPY_PREVIOUS_PLAYER;
    assert(avp_mp_spycam_cycle(0u,0u,4u,0xfu,1)==1u);
    assert(avp_mp_spycam_cycle(0u,1u,4u,0xfu,1)==2u);
    assert(avp_mp_spycam_cycle(0u,3u,4u,0xfu,1)==0u);
    assert(avp_mp_spycam_cycle(0u,0u,4u,0xfu,-1)==3u);
    assert(avp_mp_spycam_cycle(0u,1u,4u,0xfu,-1)==0u);
    assert(avp_mp_spycam_cycle(1u,1u,4u,0x9u,1)==3u);
    assert(avp_mp_spycam_cycle(1u,3u,4u,0x9u,1)==0u);
    assert(avp_mp_spycam_cycle(1u,0u,4u,0x9u,1)==1u);
    assert(avp_mp_spycam_cycle(1u,1u,4u,0u,1)==1u);
    /* Bots P5-P16 are valid spy targets, with cyclic return to self. */
    assert(avp_mp_spycam_cycle(0u,0u,16u,(1u<<0)|(1u<<5)|(1u<<15),1)==5u);
    assert(avp_mp_spycam_cycle(0u,5u,16u,(1u<<0)|(1u<<5)|(1u<<15),1)==15u);
    assert(avp_mp_spycam_cycle(0u,15u,16u,(1u<<0)|(1u<<5)|(1u<<15),1)==0u);
    assert(avp_mp_spycam_cycle(0u,0u,16u,(1u<<0)|(1u<<5)|(1u<<15),-1)==15u);
    /* 16 headless bots: P20 must be reachable from both directions and
     * the next/previous bindings must still cycle back to the viewer. */
    assert(avp_mp_spycam_cycle(0u,0u,20u,(1u<<0)|(1u<<19),1)==19u);
    assert(avp_mp_spycam_cycle(0u,19u,20u,(1u<<0)|(1u<<19),1)==0u);
    assert(avp_mp_spycam_cycle(0u,0u,20u,(1u<<0)|(1u<<19),-1)==19u);
    assert(avp_mp_spycam_cycle(0u,19u,20u,(1u<<0)|(1u<<19),-1)==0u);
    /* R131: the last of 32 bots is P36 (bit 35), beyond a 32-bit mask. */
    {const AvpMpActorMask viewers=AVP_MP_ACTOR_BIT(0u)|AVP_MP_ACTOR_BIT(35u);
     assert(avp_mp_spycam_cycle(0u,0u,36u,viewers,1)==35u);
     assert(avp_mp_spycam_cycle(0u,35u,36u,viewers,1)==0u);
     assert(avp_mp_spycam_cycle(0u,0u,36u,viewers,-1)==35u);
     assert(avp_mp_spycam_retarget(0u,35u,36u,viewers)==35u);
    }
    /* This optional helper only handles a missing/disconnected actor.
     * Death is NOT a disconnect: the host includes the dead actor in the
     * ring and never auto-retargets until a real next/previous press. */
    assert(avp_mp_spycam_retarget(0u,1u,4u,0xfu)==1u);
    /* A dying P2 remains in the selectable ring, and that SUBJECT's native
     * death FadeDown must blacken the composite view without retargeting. */
    {
        AvpMpLife dying={0};
        uint32_t pixels[4]={0x00ff8080u,0x00402010u,0x00ffffffu,0x00112233u};
        const uint32_t all_actors=(1u<<0)|(1u<<1)|(1u<<19);
        unsigned subject=avp_mp_spycam_cycle(0u,0u,20u,all_actors,1);
        assert(subject==1u);
        avp_mp_life_begin(&dying);
        assert(avp_mp_life_busy(&dying));
        for(unsigned i=0;i<64u;i++)(void)avp_mp_life_display_step(&dying);
        assert(avp_mp_life_busy(&dying) && dying.fade==128u);
        avp_mp_life_apply(&dying,pixels,2u,2u,2u);
        for(unsigned i=0;i<4u;i++)assert((pixels[i]&0x00ffffffu)==0u);
        assert(subject==1u); /* No automatic camera action on death. */
        subject=avp_mp_spycam_cycle(0u,subject,20u,all_actors,1);
        assert(subject==19u); /* Explicit press still works while dead. */
    }
    assert(avp_mp_spycam_retarget(0u,1u,4u,0xdu)==2u);
    assert(avp_mp_spycam_retarget(0u,2u,4u,0xdu)==2u);
    assert(avp_mp_spycam_retarget(0u,3u,4u,1u)==0u);
    assert(avp_mp_spycam_retarget(0u,19u,20u,(1u<<0)|(1u<<16))==0u);
    assert(avp_mp_spycam_retarget(0u,17u,20u,(1u<<0)|(1u<<19))==19u);
    assert(avp_mp_spycam_retarget(0u,0u,20u,0u)==0u);
    assert(avp_mp_spycam_edge(next,&held)==next);
    assert(avp_mp_spycam_edge(next,&held)==0u); /* no auto-repeat */
    assert(avp_mp_spycam_edge(0u,&held)==0u);
    assert(avp_mp_spycam_edge(prev,&held)==prev);
    assert(avp_mp_spycam_edge(prev,&held)==0u);
    puts("spycam ring, manual death fade, self-return, bots and key rising edges: PASS");
    return 0;
}
