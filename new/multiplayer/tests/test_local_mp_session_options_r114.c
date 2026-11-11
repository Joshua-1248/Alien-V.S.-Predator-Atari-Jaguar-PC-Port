/* R114: A new session must preserve all F2 options, and each player's
 * Reveal Entire Map cheat must remain private (unlike session Show Map). */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "local_mp.h"
#include "local_mp_reveal_policy.h"
int main(void)
{
    AvpLocalMpSetup menu,session;
    AvpMpInputConfig input;
    avp_local_mp_defaults(&menu);
    avp_local_mp_defaults(&session);
    memset(&input,0,sizeof(input));
    menu.players=3u;menu.campaign=AVP_MP_PREDATOR;
    menu.player_collisions=0u;
    menu.show_other_players_on_automap=0u;
    menu.three_layout=AVP_MP_THREE_WIDE_TOP;
    menu.game_mode=AVP_MP_DEATHMATCH;
    menu.dm_fast_movement=1u;menu.dm_enemies=1u;
    menu.dm_cloak_damage=1u;menu.dm_score_limit=35u;
    menu.dm_level_travel=1u;menu.bot_count=7u;
    menu.bot_species[3]=AVP_MP_ALIEN;
    menu.bot_behavior[3]=AVP_MP_BOT_GUERILLA;
    menu.random_species_on_respawn=0u;
    menu.respawn_campaign_start=0u;
    menu.level_explicit=1u;
    strcpy(menu.custom_map_stem,"selected_map");
    avp_local_mp_copy_session_options(&session,&menu);
    assert(session.players==2u && session.campaign==AVP_MP_MARINE);
    assert(session.player_collisions==0u);
    assert(session.show_other_players_on_automap==0u);
    assert(session.three_layout==AVP_MP_THREE_WIDE_TOP);
    assert(session.game_mode==AVP_MP_DEATHMATCH);
    assert(session.dm_fast_movement==1u && session.dm_enemies==1u);
    assert(session.dm_cloak_damage==1u && session.dm_score_limit==35u);
    assert(session.dm_level_travel==1u && session.bot_count==7u);
    assert(session.bot_species[3]==AVP_MP_ALIEN);
    assert(session.bot_behavior[3]==AVP_MP_BOT_GUERILLA);
    assert(session.random_species_on_respawn==0u);
    assert(session.respawn_campaign_start==0u);
    assert(session.level_explicit==1u);
    assert(!strcmp(session.custom_map_stem,"selected_map"));
    input.player[2].cheat_reveal_map=1;
    for(unsigned p=0;p<4u;p++)
        assert(avp_mp_reveal_for_player(&input,p,0)==(p==2u));
    assert(!avp_mp_reveal_for_player(&input,4u,0));
    /* Session-wide Show Whole Map is a separate explicit F2 option. */
    for(unsigned p=0;p<4u;p++)assert(avp_mp_reveal_for_player(&input,p,1));
    input.player[2].cheat_reveal_map=0;
    for(unsigned p=0;p<4u;p++)assert(!avp_mp_reveal_for_player(&input,p,0));
    puts("PASS: multiplayer options copied to runtime; private map cheats");
    return 0;
}
