#ifndef AVP_LOCAL_MP_BOT_LEVELS_H
#define AVP_LOCAL_MP_BOT_LEVELS_H
#include "local_mp_bot.h"
#include "doors.h"
/* Read-only original maze/LEVDOOR panel graph; all maps retain the real
 * player-specific access restrictions and their currently saved door bytes.
 * Empty layouts are intentionally skipped rather than fabricated. */
typedef struct AvpMpBotLevelLayout {
    AvpMpBotMap map;
    const AvpDoorLevelMeta *meta;
    AvpDoorPanelIds panel_ids;
    uint8_t panel_list[AVP_MAX_PANELS];
} AvpMpBotLevelLayout;

typedef struct AvpMpBotLevelPlan {
    unsigned from_level,to_level;
    unsigned x,y,side;
    enum AvpMpBotTransitionKind kind;
    unsigned landing_x,landing_y;
    unsigned hops;
} AvpMpBotLevelPlan;

/* Finds the FIRST actual source-authored vent, airlock or lift to operate.
 * Can leave and re-enter the SAME level via ducts to bypass jammed doors.
 * goal_known=0 seeks ANY reachable portion of the goal level (campaign hop).
 * It never modifies world state and cannot teleport or grant access. */
int avp_mp_bot_plan_levels(const AvpMpBotLevelLayout layouts[AVP_MAX_LEVEL],
    unsigned species,unsigned current,unsigned sx,unsigned sy,
    unsigned goal_level,unsigned gx,unsigned gy,int goal_known,
    AvpMpBotLevelPlan *out);
#endif
