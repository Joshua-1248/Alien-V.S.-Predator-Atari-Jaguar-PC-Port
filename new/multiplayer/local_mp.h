#ifndef AVP_NEW_LOCAL_MP_H
#define AVP_NEW_LOCAL_MP_H

#include <stddef.h>
#include <stdint.h>

/* This is PC-only state. No original Jaguar globals, symbols or objects. */
#define AVP_LOCAL_MP_MAX_PLAYERS 4u /* human input and viewports */
#define AVP_LOCAL_MP_MAX_BOTS 12u
#define AVP_LOCAL_MP_MAX_ACTORS (AVP_LOCAL_MP_MAX_PLAYERS+AVP_LOCAL_MP_MAX_BOTS)
#define AVP_LOCAL_MP_ROWS 50u
/* UI-only F2 submenus. Existing 0..49 setting IDs are unchanged. */
#define AVP_LOCAL_MP_MENU_ROWS (AVP_LOCAL_MP_ROWS+3u)
#define AVP_LOCAL_MP_MENU_BOTS_ROW AVP_LOCAL_MP_ROWS
#define AVP_LOCAL_MP_MENU_MODIFIERS_ROW (AVP_LOCAL_MP_ROWS+1u)
#define AVP_LOCAL_MP_MENU_OPTIONS_ROW (AVP_LOCAL_MP_ROWS+2u)
enum AvpLocalMpMenuPage {
    AVP_LOCAL_MP_MENU_ROOT=0, AVP_LOCAL_MP_MENU_BOTS,
    AVP_LOCAL_MP_MENU_MODIFIERS, AVP_LOCAL_MP_MENU_OPTIONS
};
#define AVP_LOCAL_MP_SHOW_OTHER_PLAYERS_ROW 49u /* Both co-op and Deathmatch */
#define AVP_LOCAL_MP_BOT_COUNT_ROW 20u
#define AVP_LOCAL_MP_BOT_SPECIES_ROW 21u
#define AVP_LOCAL_MP_BOT_BEHAVIOR_ROW 25u
#define AVP_LOCAL_MP_RANDOM_RESPAWN_ROW 29u
#define AVP_LOCAL_MP_WHOLE_MAP_ROW 30u
#define AVP_LOCAL_MP_TRAVEL_ROW 47u /* DM: allow vents, lifts and airlocks */
#define AVP_LOCAL_MP_CLOAK_DAMAGE_ROW 48u /* DM: cloaked Predator takes 4x PvP damage */
/* R19: preserve the original 31 menu row identifiers; extend the bot
 * selectors after them so existing settings/menu navigation remain stable. */
#define AVP_LOCAL_MP_BOT_SPECIES_EXTRA_ROW 31u  /* Bot 5 through Bot 12 */
#define AVP_LOCAL_MP_BOT_BEHAVIOR_EXTRA_ROW 39u /* Bot 5 through Bot 12 */
#define AVP_LOCAL_MP_LEVEL_ROW 17u
#define AVP_LOCAL_MP_ENEMIES_ROW 18u
#define AVP_LOCAL_MP_EQUIP_ROW 19u
#define AVP_LOCAL_MP_MODE_ROW 13u
#define AVP_LOCAL_MP_SCORE_ROW 14u
#define AVP_LOCAL_MP_TIME_ROW 15u
#define AVP_LOCAL_MP_FAST_ROW 16u
#define AVP_LOCAL_MP_START_ROW 8u

enum AvpLocalMpGameMode { AVP_MP_COOP=0, AVP_MP_DEATHMATCH=1 };
enum AvpLocalMpSpecies { AVP_MP_MARINE, AVP_MP_ALIEN, AVP_MP_PREDATOR, AVP_MP_RANDOM };
enum AvpLocalMpBotBehavior { AVP_MP_BOT_BALANCED=0, AVP_MP_BOT_BERSERKER=1, AVP_MP_BOT_GUERILLA=2 };
enum AvpLocalMpLayout { AVP_MP_SIDE_BY_SIDE, AVP_MP_TOP_BOTTOM };
enum AvpLocalMpThreeLayout { AVP_MP_THREE_QUADRANTS, AVP_MP_THREE_WIDE_TOP };

typedef struct AvpLocalMpSetup {
    unsigned enabled;       /* F12 gate; zero unless explicitly enabled. */
    unsigned players;       /* 1..4 */
    unsigned campaign;      /* Marine, Alien, Predator, independently of species. */
    unsigned layout;        /* 2-player orientation; 4-player layout is fixed. */
    unsigned three_layout;  /* 3-player quadrants or full-width P1 over P2/P3. */
    unsigned game_mode; /* Co-op default; PC-local deathmatch is opt-in. */
    unsigned dm_score_limit; /* 0=None, otherwise kills. */
    unsigned dm_time_limit; /* 0=None, otherwise minutes. */
    unsigned dm_fast_movement; /* Deathmatch only; native speed/acceleration x2. */
    unsigned dm_fast_equip; /* Deathmatch only: faster original-weapon presentation. */
    unsigned dm_enemies; /* Default 0: no hostile NPCs in Deathmatch. */
    unsigned dm_level; /* 1..15 original retail levels, independent of campaign. */
    unsigned dm_level_travel; /* Deathmatch only; default off. */
    unsigned dm_cloak_damage; /* Deathmatch modifier: cloaked Predator takes 4x PvP damage. */
    unsigned bot_count; /* Separate headless native players, not P1-P4 AI flags. */
    unsigned bot_species[AVP_LOCAL_MP_MAX_BOTS];
    unsigned bot_behavior[AVP_LOCAL_MP_MAX_BOTS];
    unsigned random_species_on_respawn;
    unsigned show_whole_map;
    unsigned show_other_players_on_automap; /* Default on; affects map dots only, never radar. */
    unsigned respawn_campaign_start; /* Co-op choice: 1=campaign start, 0=own species start. */
    unsigned bot_enabled[AVP_LOCAL_MP_MAX_ACTORS]; /* PC-only per-slot bot control, including P1. */
    unsigned species[AVP_LOCAL_MP_MAX_ACTORS];
    unsigned level[AVP_LOCAL_MP_MAX_ACTORS]; /* Future runtime's independent per-player level. */
} AvpLocalMpSetup;

typedef struct AvpLocalMpRect {
    unsigned x, y, width, height;
} AvpLocalMpRect;

typedef struct AvpLocalMpWorld {
    unsigned campaign, level, member_mask;
} AvpLocalMpWorld;

void avp_local_mp_defaults(AvpLocalMpSetup *s);
void avp_local_mp_validate(AvpLocalMpSetup *s);
int avp_local_mp_adjust(AvpLocalMpSetup *s, unsigned row, int direction);
int avp_local_mp_row_visible(const AvpLocalMpSetup *s,unsigned row);
/* Ordered visible UI rows for one page. Kept separate from original
 * settings rows so save formats and gameplay routing never change. */
unsigned avp_local_mp_menu_rows(const AvpLocalMpSetup *s,unsigned page,
                                unsigned out[AVP_LOCAL_MP_MENU_ROWS]);
unsigned avp_local_mp_menu_next_row(const AvpLocalMpSetup *s,unsigned page,
                                    unsigned row,int direction);
unsigned avp_local_mp_next_row(const AvpLocalMpSetup *s,unsigned row,int direction);
const char *avp_local_mp_species_name(unsigned species);
const char *avp_local_mp_bot_behavior_name(unsigned behavior);
unsigned avp_local_mp_total_actors(const AvpLocalMpSetup *setup);
const char *avp_local_mp_campaign_name(unsigned campaign);
const char *avp_local_mp_layout_name(unsigned layout);
const char *avp_local_mp_three_layout_name(unsigned layout);
unsigned avp_local_mp_viewports(const AvpLocalMpSetup *s, unsigned width, unsigned height,
                                AvpLocalMpRect out[AVP_LOCAL_MP_MAX_PLAYERS]);
unsigned avp_local_mp_group_worlds(const AvpLocalMpSetup *s,
                                   AvpLocalMpWorld out[AVP_LOCAL_MP_MAX_PLAYERS]);

#endif
