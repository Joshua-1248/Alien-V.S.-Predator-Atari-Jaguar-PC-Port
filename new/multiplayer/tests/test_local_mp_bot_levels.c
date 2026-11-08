#include "local_mp_bot_levels.h"
#include <stdlib.h>
#define CHECK(X) do { if(!(X)){fprintf(stderr,"FAIL %s line %d\n",#X,__LINE__);exit(1);} }while(0)
#include <stdio.h>
#include <string.h>

#define C(x,s) ((x)*8u+(s))
static uint8_t main_floor[5u*8u],duct_floor[5u*8u],wall_table[256];
static AvpMpBotLevelLayout layouts[AVP_MAX_LEVEL];
static AvpDoorAccessEntry jam[]={{C(2u,6u),AVP_ACC_JAMMED}};
static AvpDoorExitEntry main_exit[]={{C(0u,1u),0u,0u,8u}};
static AvpDoorExitEntry duct_exit[]={{C(4u,1u),4u,0u,3u}};
static AvpDoorLevelMeta main_meta={jam,1u,main_exit,1u,NULL,0u};
static AvpDoorLevelMeta duct_meta={NULL,0u,duct_exit,1u,NULL,0u};

static void setup(void)
{
    memset(layouts,0,sizeof layouts);
    memset(main_floor,0,sizeof main_floor);
    memset(duct_floor,0,sizeof duct_floor);
    memset(wall_table,0,sizeof wall_table);
    for(unsigned i=0;i<5u;i++){
        main_floor[C(i,5u)]=1u;
        duct_floor[C(i,5u)]=1u;
    }
    /* A JAMMED door separates x=0,1 from x=2,3,4. No direct route. */
    wall_table[4u]=AVP_WB_DOOR;
    main_floor[C(2u,0u)]=4u;
    main_floor[C(1u,2u)]=4u;
    main_floor[C(0u,1u)]=1u;
    duct_floor[C(4u,1u)]=1u;
    layouts[2].map=(AvpMpBotMap){main_floor,5u,1u,wall_table,&main_meta,0,NULL,NULL};
    layouts[7].map=(AvpMpBotMap){duct_floor,5u,1u,wall_table,&duct_meta,0,NULL,NULL};
    layouts[2].meta=&main_meta;
    layouts[7].meta=&duct_meta;
    layouts[2].panel_list[0]=5u;
    layouts[7].panel_list[0]=6u;
    layouts[2].panel_ids=(AvpDoorPanelIds){0xff,0xff,5u,0xff,0xff,0xff,0xff};
    layouts[7].panel_ids=(AvpDoorPanelIds){0xff,0xff,0xff,6u,0xff,0xff,0xff};
}
int main(void)
{
    AvpMpBotLevelPlan plan={0};
    uint16_t groups[AVP_MP_BOT_MAP_CELLS];
    unsigned count;
    setup();
    count=avp_mp_bot_components(&layouts[2].map,groups);
    CHECK(count==2u);
    CHECK(groups[0] && groups[4] && groups[0]!=groups[4]);
    /* This is the actual new use case: SAME level, separated by jammed
     * door. An authored two-hop loop through a vent reunites the bot. */
    CHECK(avp_mp_bot_plan_levels(layouts,0u,3u,0u,0u,3u,4u,0u,1,&plan));
    CHECK(plan.from_level==3u&&plan.to_level==8u);
    CHECK(plan.x==0u&&plan.y==0u&&plan.side==1u);
    CHECK(plan.kind==AVP_MP_BOT_TRANS_DUCT&&plan.hops==2u);
    CHECK(avp_mp_bot_plan_levels(layouts,1u,3u,0u,0u,3u,4u,0u,1,&plan));
    CHECK(!avp_mp_bot_plan_levels(layouts,2u,3u,0u,0u,3u,4u,0u,1,&plan));
    /* The return hop must lead into the OTHER component, not whichever
     * vent happens to be geometrically nearest. */
    CHECK(avp_mp_bot_plan_levels(layouts,0u,8u,0u,0u,3u,4u,0u,1,&plan));
    CHECK(plan.from_level==8u&&plan.to_level==3u&&plan.x==4u);
    CHECK(plan.hops==1u);
    /* A source-open door removes the partition; direct traversal wins. */
    main_floor[C(2u,6u)]=48u;
    count=avp_mp_bot_components(&layouts[2].map,groups);
    CHECK(count==1u);
    CHECK(!avp_mp_bot_plan_levels(layouts,0u,3u,0u,0u,3u,4u,0u,1,&plan));
    /* No invented alternative if the return vent is missing or jammed. */
    main_floor[C(2u,6u)]=0u;
    duct_floor[C(4u,1u)]=0u;
    CHECK(!avp_mp_bot_plan_levels(layouts,0u,3u,0u,0u,3u,4u,0u,1,&plan));
    /* For campaign progression, arriving at ANY valid component of a
     * different level is enough; the target cell is not fabricated. */
    duct_floor[C(4u,1u)]=1u;
    CHECK(avp_mp_bot_plan_levels(layouts,0u,3u,0u,0u,8u,0u,0u,0,&plan));
    CHECK(plan.to_level==8u&&plan.hops==1u);
    puts("PASS: jammed same-level storage detour via authored ducts, landing components, species restrictions, open-door direct route and no fake exits");
    return 0;
}
