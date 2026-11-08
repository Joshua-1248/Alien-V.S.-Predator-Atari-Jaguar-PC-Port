#include "local_mp.h"
/* Keep assertions active in Release builds too. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    AvpLocalMpSetup s;
    AvpLocalMpRect rect[4];
    AvpLocalMpWorld world[4];
    unsigned i, j;
    avp_local_mp_defaults(&s);
    /* R56: Root menu shows Number of Bots immediately under players; all
     * legacy row numbers and their adjustment semantics remain unchanged. */
    {
        unsigned rows[AVP_LOCAL_MP_MENU_ROWS],n;
        n=avp_local_mp_menu_rows(&s,AVP_LOCAL_MP_MENU_ROOT,rows);
        assert(n>8u && rows[0]==0u && rows[1]==AVP_LOCAL_MP_BOT_COUNT_ROW);
        assert(rows[n-1u]==AVP_LOCAL_MP_START_ROW);
        assert(rows[n-4u]==AVP_LOCAL_MP_MENU_BOTS_ROW);
        assert(rows[n-3u]==AVP_LOCAL_MP_MENU_MODIFIERS_ROW);
        assert(rows[n-2u]==AVP_LOCAL_MP_MENU_OPTIONS_ROW);
        assert(avp_local_mp_menu_next_row(&s,AVP_LOCAL_MP_MENU_ROOT,0u,+1)==
               AVP_LOCAL_MP_BOT_COUNT_ROW);
        assert(avp_local_mp_menu_next_row(&s,AVP_LOCAL_MP_MENU_ROOT,
               AVP_LOCAL_MP_BOT_COUNT_ROW,-1)==0u);
        assert(avp_local_mp_menu_next_row(&s,AVP_LOCAL_MP_MENU_ROOT,
               AVP_LOCAL_MP_START_ROW,+1)==0u);
        assert(avp_local_mp_menu_next_row(&s,AVP_LOCAL_MP_MENU_ROOT,0u,-1)==
               AVP_LOCAL_MP_START_ROW);
        n=avp_local_mp_menu_rows(&s,AVP_LOCAL_MP_MENU_BOTS,rows);
        assert(n==4u); /* P1–P4 AI controls, no inactive Bot 1+ selectors */
        for(unsigned i=0;i<n;i++)assert(rows[i]==9u+i);
        n=avp_local_mp_menu_rows(&s,AVP_LOCAL_MP_MENU_MODIFIERS,rows);
        assert(n==1u && rows[0]==AVP_LOCAL_MP_RANDOM_RESPAWN_ROW);
        n=avp_local_mp_menu_rows(&s,AVP_LOCAL_MP_MENU_OPTIONS,rows);
        assert(n==2u && rows[0]==AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW &&
               rows[1]==AVP_LOCAL_MP_WHOLE_MAP_ROW);
        assert(!avp_local_mp_menu_rows(&s,999u,rows));
        s.game_mode=AVP_MP_DEATHMATCH;s.bot_count=12u;
        n=avp_local_mp_menu_rows(&s,AVP_LOCAL_MP_MENU_BOTS,rows);
        assert(n==28u && rows[4]==AVP_LOCAL_MP_BOT_SPECIES_ROW &&
               rows[15]==AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW+7u &&
               rows[16]==AVP_LOCAL_MP_BOT_BEHAVIOR_ROW &&
               rows[27]==AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW+7u);
        n=avp_local_mp_menu_rows(&s,AVP_LOCAL_MP_MENU_MODIFIERS,rows);
        assert(n==4u && rows[0]==AVP_LOCAL_MP_RANDOM_RESPAWN_ROW &&
               rows[1]==AVP_LOCAL_MP_FAST_ROW &&
               rows[2]==AVP_LOCAL_MP_EQUIP_ROW &&
               rows[3]==AVP_LOCAL_MP_CLOAK_DAMAGE_ROW);
        n=avp_local_mp_menu_rows(&s,AVP_LOCAL_MP_MENU_OPTIONS,rows);
        assert(n==4u && rows[0]==AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW &&
               rows[1]==AVP_LOCAL_MP_WHOLE_MAP_ROW &&
               rows[2]==AVP_LOCAL_MP_TRAVEL_ROW &&
               rows[3]==AVP_LOCAL_MP_ENEMIES_ROW);
        s.game_mode=AVP_MP_COOP;s.bot_count=0u;
    }
    assert(s.enabled == 0 && s.players == 2 && s.campaign == 0);
    assert(s.respawn_campaign_start==1);
    assert(s.show_other_players_on_automap==1u);
    assert(avp_local_mp_row_visible(&s,AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW));
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW,+1));
    assert(s.show_other_players_on_automap==0u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW,-1));
    assert(s.show_other_players_on_automap==1u);
    for(unsigned p=0;p<4u;p++){
        assert(!s.bot_enabled[p]);
        assert(avp_local_mp_adjust(&s,9u+p,+1));
        assert(s.bot_enabled[p]==1);
        assert(avp_local_mp_adjust(&s,9u+p,-1));
        assert(!s.bot_enabled[p]);
    }
    assert(avp_local_mp_adjust(&s,7,+1) && s.respawn_campaign_start==0);
    assert(avp_local_mp_adjust(&s,7,-1) && s.respawn_campaign_start==1);
    assert(s.species[0] == AVP_MP_MARINE && s.species[1] == AVP_MP_PREDATOR);
    assert(avp_local_mp_adjust(&s, 0, +1) && s.players == 3);
    assert(avp_local_mp_adjust(&s, 0, +1) && s.players == 4);
    assert(avp_local_mp_adjust(&s, 0, +1) && s.players == 1);
    assert(avp_local_mp_adjust(&s, 0, -1) && s.players == 4);
    assert(avp_local_mp_adjust(&s, 1, -1) && s.campaign == AVP_MP_PREDATOR);
    assert(avp_local_mp_adjust(&s, 5, +1) && s.species[3] == AVP_MP_ALIEN);
    assert(!avp_local_mp_adjust(&s, AVP_LOCAL_MP_START_ROW, +1));
    s.players = 2;
    assert(!avp_local_mp_adjust(&s, 4, +1));
    assert(avp_local_mp_viewports(&s, 801, 601, rect) == 2);
    assert(rect[0].width == 400 && rect[1].x == 400 && rect[1].width == 401);
    assert(rect[0].height == 601 && rect[1].height == 601);
    s.layout = AVP_MP_TOP_BOTTOM;
    assert(avp_local_mp_viewports(&s, 801, 601, rect) == 2);
    assert(rect[0].height == 300 && rect[1].y == 300 && rect[1].height == 301);
    s.players = 3;
    s.three_layout = AVP_MP_THREE_WIDE_TOP;
    assert(avp_local_mp_viewports(&s, 801, 601, rect) == 3);
    assert(rect[0].x==0 && rect[0].y==0 && rect[0].width==801 && rect[0].height==300);
    assert(rect[1].x==0 && rect[1].y==300 && rect[1].width==400 && rect[1].height==301);
    assert(rect[2].x==400 && rect[2].y==300 && rect[2].width==401 && rect[2].height==301);
    assert(!strcmp(avp_local_mp_three_layout_name(s.three_layout),"Wide P1 + Two Below"));
    s.players = 4;
    assert(avp_local_mp_viewports(&s, 801, 601, rect) == 4);
    for (i = 0; i < 4; ++i) {
        assert(rect[i].width && rect[i].height);
        for (j = i + 1; j < 4; ++j)
            assert(rect[i].x + rect[i].width <= rect[j].x ||
                   rect[j].x + rect[j].width <= rect[i].x ||
                   rect[i].y + rect[i].height <= rect[j].y ||
                   rect[j].y + rect[j].height <= rect[i].y);
    }
    /* F2 Game Mode and deathmatch-only modifiers: no invisible rows can
     * capture keyboard/joystick input, and both limits allow None. */
    s.players=2u;
    assert(s.game_mode==AVP_MP_COOP);
    assert(!avp_local_mp_row_visible(&s,AVP_LOCAL_MP_FAST_ROW));
    assert(avp_local_mp_next_row(&s,AVP_LOCAL_MP_MODE_ROW,+1)==AVP_LOCAL_MP_BOT_COUNT_ROW);
    assert(avp_local_mp_next_row(&s,0u,-1)==AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW);
    assert(!avp_local_mp_adjust(&s,AVP_LOCAL_MP_FAST_ROW,+1));
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_MODE_ROW,+1));
    assert(s.game_mode==AVP_MP_DEATHMATCH);
    assert(!avp_local_mp_row_visible(&s,7u));
    assert(avp_local_mp_next_row(&s,6u,+1)==AVP_LOCAL_MP_START_ROW);
    assert(avp_local_mp_row_visible(&s,AVP_LOCAL_MP_FAST_ROW));
    assert(avp_local_mp_next_row(&s,AVP_LOCAL_MP_FAST_ROW,+1)==AVP_LOCAL_MP_LEVEL_ROW);
    assert(avp_local_mp_next_row(&s,AVP_LOCAL_MP_EQUIP_ROW,+1)==AVP_LOCAL_MP_BOT_COUNT_ROW);
    assert(s.dm_enemies==0u && s.dm_fast_equip==0u && s.dm_level==3u);
    assert(!s.dm_level_travel);
    assert(avp_local_mp_row_visible(&s,AVP_LOCAL_MP_TRAVEL_ROW));
    assert(avp_local_mp_next_row(&s,0u,-1)==AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW);
    assert(avp_local_mp_next_row(&s,AVP_LOCAL_MP_CLOAK_DAMAGE_ROW,+1)==AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_CLOAK_DAMAGE_ROW,+1));
    assert(s.dm_cloak_damage==1u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_CLOAK_DAMAGE_ROW,-1));
    assert(s.dm_cloak_damage==0u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_TRAVEL_ROW,+1));
    assert(s.dm_level_travel==1u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_TRAVEL_ROW,-1));
    assert(s.dm_level_travel==0u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_ENEMIES_ROW,+1));
    assert(s.dm_enemies==1u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_ENEMIES_ROW,-1));
    assert(s.dm_enemies==0u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_EQUIP_ROW,+1));
    assert(s.dm_fast_equip==1u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_EQUIP_ROW,+1));
    assert(s.dm_fast_equip==0u);
    s.dm_level=1u;
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_LEVEL_ROW,-1));
    assert(s.dm_level==15u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_LEVEL_ROW,+1));
    assert(s.dm_level==1u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_LEVEL_ROW,+1));
    assert(s.dm_level==2u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_FAST_ROW,+1));
    assert(s.dm_fast_movement==1u);
    s.dm_score_limit=5u;
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_SCORE_ROW,-1));
    assert(s.dm_score_limit==0u);
    s.dm_time_limit=5u;
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_TIME_ROW,-1));
    assert(s.dm_time_limit==0u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_MODE_ROW,-1));
    assert(s.game_mode==AVP_MP_COOP);
    assert(!avp_local_mp_row_visible(&s,AVP_LOCAL_MP_FAST_ROW));
    assert(!avp_local_mp_row_visible(&s,AVP_LOCAL_MP_TRAVEL_ROW));
    assert(!avp_local_mp_row_visible(&s,AVP_LOCAL_MP_CLOAK_DAMAGE_ROW));
    assert(!avp_local_mp_adjust(&s,AVP_LOCAL_MP_TRAVEL_ROW,+1));
    s.players=4u;
    /* Four independent locations may have three active level worlds. */
    s.campaign = AVP_MP_ALIEN;
    s.level[0] = 7; s.level[1] = 12; s.level[2] = 7; s.level[3] = 9;
    assert(avp_local_mp_group_worlds(&s, world) == 3);
    assert(world[0].level == 7 && world[0].member_mask == 0x5);
    assert(world[1].level == 12 && world[1].member_mask == 0x2);
    assert(world[2].level == 9 && world[2].member_mask == 0x8);
    /* R17 standalone roster stays independent from four human viewports. */
    s.players=2u;
    assert(s.bot_count==0u && s.random_species_on_respawn==1u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_BOT_COUNT_ROW,+1));
    assert(s.bot_count==1u);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_BOT_SPECIES_ROW,+1));
    assert(s.bot_species[0]==AVP_MP_ALIEN);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_BOT_BEHAVIOR_ROW,+1));
    assert(s.bot_behavior[0]==AVP_MP_BOT_BERSERKER);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_RANDOM_RESPAWN_ROW,+1));
    assert(!s.random_species_on_respawn);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_WHOLE_MAP_ROW,+1));
    assert(s.show_whole_map);
    assert(avp_local_mp_adjust(&s,2u,+1));
    avp_local_mp_validate(&s);
    assert(avp_local_mp_total_actors(&s)==3u);
    assert(s.bot_enabled[2u] && s.species[2u]==AVP_MP_ALIEN);
    assert(s.bot_enabled[0u]==0u && s.bot_enabled[1u]==0u);
    /* R19: twelve independent headless bots, while P1-P4 remain the
     * only display/input seats. The original 31 F2 row IDs stay stable. */
    assert(AVP_LOCAL_MP_MAX_PLAYERS==4u && AVP_LOCAL_MP_MAX_BOTS==12u);
    assert(AVP_LOCAL_MP_MAX_ACTORS==16u);
    memset(s.bot_species,0,sizeof(s.bot_species));
    memset(s.bot_behavior,0,sizeof(s.bot_behavior));
    s.players=4u;s.bot_count=12u;
    avp_local_mp_validate(&s);
    assert(avp_local_mp_total_actors(&s)==16u);
    for(unsigned bot=0;bot<12u;bot++){
        unsigned species_row=bot<4u?AVP_LOCAL_MP_BOT_SPECIES_ROW+bot:
            AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW+(bot-4u);
        unsigned behavior_row=bot<4u?AVP_LOCAL_MP_BOT_BEHAVIOR_ROW+bot:
            AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW+(bot-4u);
        assert(avp_local_mp_row_visible(&s,species_row));
        assert(avp_local_mp_row_visible(&s,behavior_row));
        assert(avp_local_mp_adjust(&s,species_row,+1));
        assert(avp_local_mp_adjust(&s,behavior_row,+1));
        assert(s.bot_species[bot]==AVP_MP_ALIEN);
        assert(s.bot_behavior[bot]==AVP_MP_BOT_BERSERKER);
        assert(s.species[bot+4u]==AVP_MP_MARINE); /* refreshed on validation */
    }
    avp_local_mp_validate(&s);
    for(unsigned bot=0;bot<12u;bot++){
        assert(s.species[bot+4u]==AVP_MP_ALIEN);
        assert(s.bot_enabled[bot+4u]);
    }
    assert(avp_local_mp_next_row(&s,AVP_LOCAL_MP_WHOLE_MAP_ROW,+1)==
           AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW);
    assert(avp_local_mp_row_visible(&s,AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW));
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW,+1));
    assert(!s.show_other_players_on_automap);
    avp_local_mp_validate(&s);
    assert(!s.show_other_players_on_automap);
    assert(avp_local_mp_adjust(&s,AVP_LOCAL_MP_BOT_COUNT_ROW,+1));
    assert(s.bot_count==0u);
    avp_local_mp_validate(&s);
    assert(!avp_local_mp_row_visible(&s,AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW));
    assert(!avp_local_mp_row_visible(&s,AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW));
    assert(avp_local_mp_total_actors(&s)==4u);
    s.players = 99; s.campaign = 10; s.species[3] = 99; s.respawn_campaign_start=77; s.bot_enabled[0]=17;s.bot_enabled[3]=99;
    avp_local_mp_validate(&s);
    assert(s.players == 4 && s.campaign == 0 && s.species[3] == 0);
    assert(s.respawn_campaign_start==1);
    assert(s.bot_enabled[0]==1&&s.bot_enabled[3]==1);
    assert(strcmp(avp_local_mp_species_name(AVP_MP_ALIEN), "Alien") == 0);
    puts("local multiplayer setup / viewport / world grouping: PASS");
    return 0;
}
