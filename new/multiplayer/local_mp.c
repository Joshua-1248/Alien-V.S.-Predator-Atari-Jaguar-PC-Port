#include "local_mp.h"
#include <string.h>

void avp_local_mp_defaults(AvpLocalMpSetup *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->players = 2;
    s->bot_count=0u;
    s->random_species_on_respawn=1u;
    s->show_other_players_on_automap=1u; /* Preserve R22 map markers by default. */
    s->game_mode=AVP_MP_COOP;
    s->dm_score_limit=20u;
    s->dm_time_limit=10u;
    s->dm_level=3u; /* Marine campaign starting level by default. */
    s->respawn_campaign_start = 1u; /* Default: return to the chosen campaign start. */
    s->species[0] = AVP_MP_MARINE;
    s->species[1] = AVP_MP_PREDATOR;
    s->species[2] = AVP_MP_ALIEN;
    s->species[3] = AVP_MP_MARINE;
}

void avp_local_mp_validate(AvpLocalMpSetup *s)
{
    unsigned i;
    if (!s) return;
    s->enabled = !!s->enabled;
    if(s->players<1u||s->players>AVP_LOCAL_MP_MAX_PLAYERS)s->players=AVP_LOCAL_MP_MAX_PLAYERS;
    if(s->game_mode>AVP_MP_DEATHMATCH)s->game_mode=AVP_MP_COOP;
    if(s->dm_score_limit>100u)s->dm_score_limit=100u;
    if(s->dm_time_limit>60u)s->dm_time_limit=60u;
    s->dm_fast_movement=!!s->dm_fast_movement;
    s->dm_fast_equip=!!s->dm_fast_equip;
    s->dm_enemies=!!s->dm_enemies;
    s->dm_level_travel=!!s->dm_level_travel;
    s->dm_cloak_damage=!!s->dm_cloak_damage;
    if(s->dm_level<1u||s->dm_level>15u)s->dm_level=3u;
    s->respawn_campaign_start = !!s->respawn_campaign_start;
    s->show_whole_map=!!s->show_whole_map;
    s->show_other_players_on_automap=!!s->show_other_players_on_automap;
    s->random_species_on_respawn=!!s->random_species_on_respawn;
    if(s->bot_count>AVP_LOCAL_MP_MAX_BOTS)s->bot_count=AVP_LOCAL_MP_MAX_BOTS;
    if (s->campaign > AVP_MP_PREDATOR) s->campaign = AVP_MP_MARINE;
    if (s->layout > AVP_MP_TOP_BOTTOM) s->layout = AVP_MP_SIDE_BY_SIDE;
    if (s->three_layout > AVP_MP_THREE_WIDE_TOP) s->three_layout = AVP_MP_THREE_QUADRANTS;
    for (i = 0; i < AVP_LOCAL_MP_MAX_PLAYERS; ++i) {
        s->bot_enabled[i] = !!s->bot_enabled[i];
        if (s->species[i] > AVP_MP_RANDOM) s->species[i] = AVP_MP_MARINE;
    }
    for(i=0;i<AVP_LOCAL_MP_MAX_BOTS;i++){
        if(s->bot_species[i]>AVP_MP_RANDOM)s->bot_species[i]=AVP_MP_MARINE;
        if(s->bot_behavior[i]>AVP_MP_BOT_GUERILLA)s->bot_behavior[i]=AVP_MP_BOT_BALANCED;
        s->species[s->players+i]=s->bot_species[i];
        s->bot_enabled[s->players+i]=(i<s->bot_count);
    }
}

/* Hide inactive F2 selectors from BOTH keyboard and joystick navigation.
 * Keep the raw row IDs stable for saved bindings and existing menus. */
int avp_local_mp_row_visible(const AvpLocalMpSetup *s,unsigned row)
{
    if(!s || row>=AVP_LOCAL_MP_ROWS)return 0;
    if(row>=2u && row<=5u && row-2u>=s->players)return 0;
    if(row==7u && s->game_mode==AVP_MP_DEATHMATCH)return 0;
    if(row>=AVP_LOCAL_MP_BOT_SPECIES_ROW && row<AVP_LOCAL_MP_BOT_SPECIES_ROW+4u &&
       row-AVP_LOCAL_MP_BOT_SPECIES_ROW>=s->bot_count)return 0;
    if(row>=AVP_LOCAL_MP_BOT_BEHAVIOR_ROW && row<AVP_LOCAL_MP_BOT_BEHAVIOR_ROW+4u &&
       row-AVP_LOCAL_MP_BOT_BEHAVIOR_ROW>=s->bot_count)return 0;
    if(row>=AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW &&
       row<AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW &&
       row-AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW+4u>=s->bot_count)return 0;
    if(row>=AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW && row<AVP_LOCAL_MP_ROWS &&
       row<AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW+AVP_LOCAL_MP_MAX_BOTS-4u &&
       row-AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW+4u>=s->bot_count)return 0;
    if((row==AVP_LOCAL_MP_TRAVEL_ROW || row==AVP_LOCAL_MP_CLOAK_DAMAGE_ROW) &&
       s->game_mode!=AVP_MP_DEATHMATCH)return 0;
    if(row>=AVP_LOCAL_MP_SCORE_ROW && row<20u &&
       s->game_mode!=AVP_MP_DEATHMATCH)return 0;
    return 1;
}
/* R56 F2 layout: root concentrates session configuration; each submenu
 * filters the same stable underlying row IDs. No gameplay data is copied. */
unsigned avp_local_mp_menu_rows(const AvpLocalMpSetup *s,unsigned page,
                                unsigned out[AVP_LOCAL_MP_MENU_ROWS])
{
    static const unsigned root[]={
        0u,AVP_LOCAL_MP_BOT_COUNT_ROW,1u,2u,3u,4u,5u,6u,7u,
        AVP_LOCAL_MP_MODE_ROW,AVP_LOCAL_MP_SCORE_ROW,AVP_LOCAL_MP_TIME_ROW,
        AVP_LOCAL_MP_LEVEL_ROW,AVP_LOCAL_MP_MENU_BOTS_ROW,
        AVP_LOCAL_MP_MENU_MODIFIERS_ROW,AVP_LOCAL_MP_MENU_OPTIONS_ROW,
        AVP_LOCAL_MP_START_ROW};
    static const unsigned bots[]={9u,10u,11u,12u,
        21u,22u,23u,24u,31u,32u,33u,34u,35u,36u,37u,38u,
        25u,26u,27u,28u,39u,40u,41u,42u,43u,44u,45u,46u};
    static const unsigned modifiers[]={AVP_LOCAL_MP_RANDOM_RESPAWN_ROW,
        AVP_LOCAL_MP_FAST_ROW,AVP_LOCAL_MP_EQUIP_ROW,AVP_LOCAL_MP_CLOAK_DAMAGE_ROW};
    static const unsigned options[]={AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW,
        AVP_LOCAL_MP_WHOLE_MAP_ROW,AVP_LOCAL_MP_TRAVEL_ROW,
        AVP_LOCAL_MP_ENEMIES_ROW};
    const unsigned *items=NULL;unsigned size=0,count=0,i;
    if(!s||!out)return 0u;
    switch(page){
    case AVP_LOCAL_MP_MENU_ROOT:items=root;size=sizeof(root)/sizeof(root[0]);break;
    case AVP_LOCAL_MP_MENU_BOTS:items=bots;size=sizeof(bots)/sizeof(bots[0]);break;
    case AVP_LOCAL_MP_MENU_MODIFIERS:items=modifiers;size=sizeof(modifiers)/sizeof(modifiers[0]);break;
    case AVP_LOCAL_MP_MENU_OPTIONS:items=options;size=sizeof(options)/sizeof(options[0]);break;
    default:return 0u;
    }
    for(i=0;i<size;i++){
        unsigned row=items[i];
        if(row>=AVP_LOCAL_MP_ROWS || avp_local_mp_row_visible(s,row))
            out[count++]=row;
    }
    return count;
}
unsigned avp_local_mp_menu_next_row(const AvpLocalMpSetup *s,unsigned page,
                                    unsigned row,int direction)
{
    unsigned items[AVP_LOCAL_MP_MENU_ROWS];
    unsigned i,n=avp_local_mp_menu_rows(s,page,items);
    if(!n)return AVP_LOCAL_MP_START_ROW;
    for(i=0;i<n;i++)if(items[i]==row)break;
    if(i==n)return items[0];
    return items[(i+(direction<0?n-1u:1u))%n];
}

unsigned avp_local_mp_next_row(const AvpLocalMpSetup *s,unsigned row,int direction)
{
    for(unsigned i=0;i<AVP_LOCAL_MP_ROWS;i++){
        row=(row+(direction<0?AVP_LOCAL_MP_ROWS-1u:1u))%AVP_LOCAL_MP_ROWS;
        if(avp_local_mp_row_visible(s,row))return row;
    }
    return AVP_LOCAL_MP_START_ROW;
}

/* Selectors only; this module does not start or simulate a second player. */
int avp_local_mp_adjust(AvpLocalMpSetup *s, unsigned row, int direction)
{
    unsigned *value = NULL, limit = 0;
    if (!s || !direction || !avp_local_mp_row_visible(s,row)) return 0;
    if(row>=AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW &&
       row<AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW){
        value=&s->bot_species[row-AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW+4u];
        limit=4u;
    }else if(row>=AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW &&
             row<AVP_LOCAL_MP_TRAVEL_ROW){
        value=&s->bot_behavior[row-AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW+4u];
        limit=3u;
    }else switch (row) {
    case 0: value = &s->players; limit = AVP_LOCAL_MP_MAX_PLAYERS; break;
    case 1: value = &s->campaign; limit = 3u; break;
    case 2: case 3: case 4: case 5:
        if (row - 2u >= s->players) return 0;
        value = &s->species[row - 2u]; limit = 4u; break;
    case 6: value = &s->layout; limit = 2u; break;
    case 7: value = &s->respawn_campaign_start; limit = 2u; break;
    case AVP_LOCAL_MP_BOT_COUNT_ROW:
        s->bot_count=(s->bot_count+(direction>0?1u:AVP_LOCAL_MP_MAX_BOTS))%(AVP_LOCAL_MP_MAX_BOTS+1u);
        return 1;
    case 21:case 22:case 23:case 24:
        value=&s->bot_species[row-AVP_LOCAL_MP_BOT_SPECIES_ROW];limit=4u;break;
    case 25:case 26:case 27:case 28:
        value=&s->bot_behavior[row-AVP_LOCAL_MP_BOT_BEHAVIOR_ROW];limit=3u;break;
    case AVP_LOCAL_MP_RANDOM_RESPAWN_ROW:
        s->random_species_on_respawn=!s->random_species_on_respawn;return 1;
    case AVP_LOCAL_MP_WHOLE_MAP_ROW:
        s->show_whole_map=!s->show_whole_map;return 1;
    case AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW:
        s->show_other_players_on_automap=!s->show_other_players_on_automap;return 1;
    case AVP_LOCAL_MP_TRAVEL_ROW:
        s->dm_level_travel=!s->dm_level_travel;return 1;
    case AVP_LOCAL_MP_CLOAK_DAMAGE_ROW:
        s->dm_cloak_damage=!s->dm_cloak_damage;return 1;
    case AVP_LOCAL_MP_MODE_ROW: value=&s->game_mode;limit=2u;break;
    case AVP_LOCAL_MP_SCORE_ROW:
        if(s->game_mode!=AVP_MP_DEATHMATCH)return 0;
        s->dm_score_limit=(s->dm_score_limit+(direction>0?5u:100u))%105u;return 1;
    case AVP_LOCAL_MP_TIME_ROW:
        if(s->game_mode!=AVP_MP_DEATHMATCH)return 0;
        s->dm_time_limit=(s->dm_time_limit+(direction>0?5u:60u))%65u;return 1;
    case AVP_LOCAL_MP_FAST_ROW:
        s->dm_fast_movement=!s->dm_fast_movement;return 1;
    case AVP_LOCAL_MP_EQUIP_ROW:
        s->dm_fast_equip=!s->dm_fast_equip;return 1;
    case AVP_LOCAL_MP_ENEMIES_ROW:
        s->dm_enemies=!s->dm_enemies;return 1;
    case AVP_LOCAL_MP_LEVEL_ROW:
        s->dm_level=(s->dm_level+(direction>0?0u:13u))%15u+1u;
        /* selected 1..15, avoid accidentally skipping the first level */
        return 1;
    case 9: case 10: case 11: case 12:
        value = &s->bot_enabled[row - 9u]; limit = 2u; break;
    default: return 0;
    }
    if (row == 0u) {
        unsigned old = *value - 1u;
        *value = (old + (direction > 0 ? 1u : limit - 1u)) % limit + 1u;
    } else {
        *value = (unsigned)(((int)*value + (direction > 0 ? 1 : (int)limit - 1)) % (int)limit);
    }
    return 1;
}

const char *avp_local_mp_three_layout_name(unsigned layout)
{
    return layout == AVP_MP_THREE_QUADRANTS ? "Three Quadrants" :
           layout == AVP_MP_THREE_WIDE_TOP ? "Wide P1 + Two Below" : "Invalid";
}

const char *avp_local_mp_species_name(unsigned species)
{
    static const char *const names[] = {"Marine", "Alien", "Predator", "Random"};
    return species <= AVP_MP_RANDOM ? names[species] : "Invalid";
}
const char *avp_local_mp_bot_behavior_name(unsigned behavior)
{
    static const char *const names[]={"Balanced","Berserker","Guerilla"};
    return behavior<=AVP_MP_BOT_GUERILLA?names[behavior]:"Invalid";
}
unsigned avp_local_mp_total_actors(const AvpLocalMpSetup *setup)
{
    if(!setup)return 0;
    return setup->players+setup->bot_count;
}
const char *avp_local_mp_campaign_name(unsigned campaign)
{
    return avp_local_mp_species_name(campaign);
}
const char *avp_local_mp_layout_name(unsigned layout)
{
    return layout == AVP_MP_SIDE_BY_SIDE ? "Side by Side" :
           layout == AVP_MP_TOP_BOTTOM ? "Top and Bottom" : "Invalid";
}

unsigned avp_local_mp_viewports(const AvpLocalMpSetup *s, unsigned w, unsigned h,
                                AvpLocalMpRect out[AVP_LOCAL_MP_MAX_PLAYERS])
{
    unsigned i, n, halfw, halfh;
    if (!s || !out || !w || !h || s->players < 1 || s->players > 4) return 0;
    memset(out, 0, sizeof(*out) * AVP_LOCAL_MP_MAX_PLAYERS);
    n = s->players;
    if (n == 1) { out[0] = (AvpLocalMpRect){0u, 0u, w, h}; return n; }
    if (n == 2) {
        if (s->layout == AVP_MP_TOP_BOTTOM) {
            halfh = h / 2u;
            out[0] = (AvpLocalMpRect){0u, 0u, w, halfh};
            out[1] = (AvpLocalMpRect){0u, halfh, w, h - halfh};
        } else {
            halfw = w / 2u;
            out[0] = (AvpLocalMpRect){0u, 0u, halfw, h};
            out[1] = (AvpLocalMpRect){halfw, 0u, w - halfw, h};
        }
        return n;
    }
    halfw = w / 2u; halfh = h / 2u;
    if (n == 3u && s->three_layout == AVP_MP_THREE_WIDE_TOP) {
        if (w < 2u || h < 2u) return 0;
        out[0] = (AvpLocalMpRect){0u, 0u, w, halfh};
        out[1] = (AvpLocalMpRect){0u, halfh, halfw, h - halfh};
        out[2] = (AvpLocalMpRect){halfw, halfh, w - halfw, h - halfh};
        return n;
    }
    for (i = 0; i < n; ++i) {
        out[i].x = (i % 2u) ? halfw : 0u;
        out[i].y = (i / 2u) ? halfh : 0u;
        out[i].width = (i % 2u) ? w - halfw : halfw;
        out[i].height = (i / 2u) ? h - halfh : halfh;
    }
    return n;
}

/* Future level-independent simulation: same campaign+level shares one world,
 * other levels remain distinct. No original game state is duplicated here. */
unsigned avp_local_mp_group_worlds(const AvpLocalMpSetup *s,
                                   AvpLocalMpWorld out[AVP_LOCAL_MP_MAX_PLAYERS])
{
    unsigned p, j, n = 0;
    if (!s || !out || s->players < 1 || s->players > 4) return 0;
    memset(out, 0, sizeof(*out) * AVP_LOCAL_MP_MAX_PLAYERS);
    for (p = 0; p < s->players; ++p) {
        for (j = 0; j < n; ++j)
            if (out[j].campaign == s->campaign && out[j].level == s->level[p]) break;
        if (j == n) {
            out[n].campaign = s->campaign;
            out[n].level = s->level[p];
            ++n;
        }
        out[j].member_mask |= 1u << p;
    }
    return n;
}
