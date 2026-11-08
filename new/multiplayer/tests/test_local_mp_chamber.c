#include "local_mp_chamber.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d\n",#x,__LINE__);return 1;}}while(0)
static uint8_t maze[8u+8u*8u*8u];
static uint8_t *cell(unsigned x,unsigned y){return maze+8u+(y*8u+x)*8u;}
static void pair(unsigned x,unsigned y,unsigned side,uint8_t type)
{
    if(side==0){cell(x,y)[0]=type;cell(x-1u,y)[2]=type;}
    if(side==1){cell(x,y)[1]=type;cell(x,y-1u)[3]=type;}
    if(side==2){cell(x,y)[2]=type;cell(x+1u,y)[0]=type;}
    if(side==3){cell(x,y)[3]=type;cell(x,y+1u)[1]=type;}
}
int main(void)
{
    AvpMpRetailMap map={maze,sizeof(maze),8u,8u,1u,1u};
    memset(maze,0,sizeof maze);
    /* An authored 2x2 room, complete with one CLOSED/OPEN door panel. */
    for(unsigned y=2;y<=3;y++){pair(2,y,0,8u);pair(3,y,2,8u);}
    for(unsigned x=2;x<=3;x++){pair(x,2,1,8u);pair(x,3,3,8u);}
    pair(3,2,2,32u); /* Original type 4 door; considered boundary even open. */
    CHECK(avp_mp_chamber_contains(&map,0x28000u,0x28000u,0x38000u,0x38000u));
    CHECK(avp_mp_chamber_contains(&map,0x38000u,0x38000u,0x28000u,0x28000u));
    CHECK(!avp_mp_chamber_contains(&map,0x28000u,0x28000u,0x48000u,0x28000u));
    CHECK(!avp_mp_chamber_contains(&map,0x18000u,0x28000u,0x48000u,0x28000u));
    CHECK(!avp_mp_chamber_contains(&map,0x28000u,0x28000u,0x88000u,0x28000u));
    pair(2,2,0,0u); /* Room opens into large unbounded hall: refuse group. */
    CHECK(!avp_mp_chamber_contains(&map,0x28000u,0x28000u,0x38000u,0x38000u));
    /* Same-chamber passengers preserve their relative 16.16 separation. */
    {
        int32_t x=0,y=0;
        CHECK(avp_mp_chamber_translate(0x220000,0x250000,0x18000,0x18000,
                                         0x28000,0x38000,&x,&y));
        CHECK(x==0x230000 && y==0x270000);
        /* A passenger that cannot land in representable map space must
         * block the entire group rather than be left behind. */
        CHECK(!avp_mp_chamber_translate(0x1000,0x1000,0x28000,0x18000,
                                          0x18000,0x18000,&x,&y));
        CHECK(!avp_mp_chamber_translate(INT32_MAX-0x4000,0x1000,
                              0x18000,0x18000,0x28000,0x18000,&x,&y));
        /* S104: crowded co-op transfers stack instead of allowing a
         * passenger offset to wrap or strand the entire party. */
        CHECK(avp_mp_chamber_translate_or_stack(0x220000,0x250000,
                              0x18000,0x18000,0x28000,0x38000,&x,&y));
        CHECK(x==0x230000 && y==0x270000); /* valid offset preserved */
        CHECK(avp_mp_chamber_translate_or_stack(0x1000,0x1000,
                              0x28000,0x18000,0x18000,0x18000,&x,&y));
        CHECK(x==0x1000 && y==0x1000); /* overflow stacks on leader */
        CHECK(avp_mp_chamber_translate_or_stack(INT32_MAX-0x4000,0x1000,
                              0x18000,0x18000,0x28000,0x18000,&x,&y));
        CHECK(x==INT32_MAX-0x4000 && y==0x1000);
        CHECK(!avp_mp_chamber_translate_or_stack(-1,0x1000,
                              0x18000,0x18000,0x28000,0x18000,&x,&y));
    }
    /* 3 co-op players enter a cramped vent at the same authored spot.
     * Their exemption is per pair and expires once enough space appears;
     * a separate fourth actor/world must never inherit it. */
    {
        AvpMpSharedArrivalGrace state={0};
        uint8_t present[4]={1,1,1,1};
        unsigned level[4]={3,3,3,4};
        uint32_t x[4]={0x28000,0x28000,0x28000,0x28000};
        uint32_t y[4]={0x28000,0x28000,0x28000,0x28000};
        avp_mp_shared_arrival_update(&state,present,level,x,y,0x7u);
        CHECK(avp_mp_shared_arrival_exempt(&state,0,1));
        CHECK(avp_mp_shared_arrival_exempt(&state,0,2));
        CHECK(avp_mp_shared_arrival_exempt(&state,1,2));
        CHECK(!avp_mp_shared_arrival_exempt(&state,0,3));
        x[1]+=0x4000;
        avp_mp_shared_arrival_update(&state,present,level,x,y,0u);
        CHECK(avp_mp_shared_arrival_exempt(&state,0,1));
        x[1]+=0x1000;
        avp_mp_shared_arrival_update(&state,present,level,x,y,0u);
        CHECK(!avp_mp_shared_arrival_exempt(&state,0,1));
        CHECK(!avp_mp_shared_arrival_exempt(&state,1,2));
        CHECK(avp_mp_shared_arrival_exempt(&state,0,2));
        /* Being close again during ordinary motion must not rearm grace. */
        x[1]=0x28000;
        avp_mp_shared_arrival_update(&state,present,level,x,y,0u);
        CHECK(!avp_mp_shared_arrival_exempt(&state,0,1));
        CHECK(avp_mp_shared_arrival_exempt(&state,0,2));
        /* A NEW valid shared arrival may restore that exact pair. */
        avp_mp_shared_arrival_update(&state,present,level,x,y,0x7u);
        CHECK(avp_mp_shared_arrival_exempt(&state,0,1));
        /* On a split-world transition, all old pair exemptions retire. */
        level[0]=5;
        avp_mp_shared_arrival_update(&state,present,level,x,y,0u);
        CHECK(!avp_mp_shared_arrival_exempt(&state,0,1));
        CHECK(!avp_mp_shared_arrival_exempt(&state,0,2));
    }
    puts("PASS: authored chamber boundaries, offsets, shared arrivals and collision grace");
    return 0;
}
