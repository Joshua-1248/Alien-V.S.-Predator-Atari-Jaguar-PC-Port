/* R41: one PvP event -> independent human HUD notices, bots numbered #5..16. */
#include "local_mp_kill_notice.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    AvpMpKillNotice notices[AVP_LOCAL_MP_MAX_PLAYERS]={0};
    unsigned other=0u;
    /* Both human participants get separate, correctly directed messages. */
    avp_mp_kill_notice_record(notices,4u,16u,0u,3u,10u);
    assert(avp_mp_kill_notice_active(&notices[0],10u,&other)==AVP_MP_NOTICE_KILLED);
    assert(other==4u);
    assert(avp_mp_kill_notice_active(&notices[3],10u,&other)==AVP_MP_NOTICE_KILLED_BY);
    assert(other==1u);
    assert(avp_mp_kill_notice_active(&notices[0],39u,&other)==AVP_MP_NOTICE_KILLED);
    assert(avp_mp_kill_notice_active(&notices[0],40u,&other)==AVP_MP_NOTICE_NONE);
    assert(avp_mp_kill_notice_active(&notices[3],40u,&other)==AVP_MP_NOTICE_NONE);
    /* Same five-tick illuminated/blank rhythm as the native pickup messages.
       Run by game ticks, NOT presentation frames, and independent per HUD. */
    for(unsigned age=0;age<30u;age++){
        const unsigned expected=(age/5u)%2u?AVP_MP_NOTICE_NONE:AVP_MP_NOTICE_KILLED;
        assert(avp_mp_kill_notice_visible(&notices[0],10u+age,&other)==expected);
        if(expected)assert(other==4u);
    }
    assert(avp_mp_kill_notice_visible(&notices[0],40u,&other)==AVP_MP_NOTICE_NONE);
    assert(avp_mp_kill_notice_visible(&notices[0],9u,&other)==AVP_MP_NOTICE_NONE);
    /* A second render at the same tick must never consume the notice. */
    assert(avp_mp_kill_notice_visible(&notices[3],10u,&other)==AVP_MP_NOTICE_KILLED_BY);
    assert(avp_mp_kill_notice_visible(&notices[3],10u,&other)==AVP_MP_NOTICE_KILLED_BY);
    /* The same event can credit a bot #16 as killer or victim; only humans
       have HUD viewports, but the displayed roster number remains #16. */
    memset(notices,0,sizeof notices);
    avp_mp_kill_notice_record(notices,4u,16u,15u,0u,100u);
    assert(avp_mp_kill_notice_active(&notices[0],100u,&other)==AVP_MP_NOTICE_KILLED_BY);
    assert(other==16u);
    avp_mp_kill_notice_record(notices,4u,16u,1u,15u,101u);
    assert(avp_mp_kill_notice_active(&notices[1],101u,&other)==AVP_MP_NOTICE_KILLED);
    assert(other==16u);
    /* Human roster slots are not limited to a specific shooter/victim ID. */
    for(unsigned i=0;i<4u;i++){
        memset(notices,0,sizeof notices);
        avp_mp_kill_notice_record(notices,4u,16u,i,(i+1u)%4u,300u);
        assert(avp_mp_kill_notice_active(&notices[i],300u,&other)==AVP_MP_NOTICE_KILLED);
        assert(other==(i+1u)%4u+1u);
    }
    /* Invalid, uncredited or self-inflicted deaths do not announce. */
    memset(notices,0,sizeof notices);
    avp_mp_kill_notice_record(notices,4u,16u,0u,0u,500u);
    avp_mp_kill_notice_record(notices,4u,16u,16u,0u,500u);
    avp_mp_kill_notice_record(notices,4u,16u,0u,16u,500u);
    avp_mp_kill_notice_record(notices,4u,16u,0u,17u,500u);
    avp_mp_kill_notice_record(notices,4u,16u,17u,0u,500u);
    for(unsigned i=0;i<4u;i++)assert(!avp_mp_kill_notice_active(&notices[i],500u,0));
    avp_mp_kill_notice_record(notices,0u,16u,3u,15u,510u);
    for(unsigned i=0;i<4u;i++)assert(!avp_mp_kill_notice_active(&notices[i],510u,0));
    /* New eliminations overwrite the player's previous notice (not stack). */
    avp_mp_kill_notice_record(notices,4u,16u,2u,5u,600u);
    assert(avp_mp_kill_notice_active(&notices[2],600u,&other)==AVP_MP_NOTICE_KILLED);
    assert(other==6u);
    avp_mp_kill_notice_record(notices,4u,16u,15u,2u,605u);
    assert(avp_mp_kill_notice_active(&notices[2],605u,&other)==AVP_MP_NOTICE_KILLED_BY);
    assert(other==16u);
    assert(avp_mp_kill_notice_active(&notices[2],634u,&other)==AVP_MP_NOTICE_KILLED_BY);
    assert(avp_mp_kill_notice_active(&notices[2],635u,&other)==AVP_MP_NOTICE_NONE);
    avp_mp_kill_notice_reset(&notices[2]);
    assert(avp_mp_kill_notice_active(&notices[2],1u,&other)==AVP_MP_NOTICE_NONE);
    puts("R42 local deathmatch 2-way flashing HUD notifications: PASS");
    return 0;
}
