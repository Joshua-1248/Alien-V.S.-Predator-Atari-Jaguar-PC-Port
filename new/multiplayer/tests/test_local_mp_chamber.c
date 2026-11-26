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
    /* R299: FOUR actual ship transitions, modeled only as completion
     * notifications. Every reverse panel must remain inhibited while the
     * arriving party is still in its chamber, regardless of how many
     * original game ticks pass or whether FOLLOW replans back to origin. */
    {
        static const unsigned from[4]={1u,15u,5u,14u};
        static const unsigned to[4]={15u,1u,14u,5u};
        for(unsigned i=0;i<4u;i++){
            AvpMpAirlockCycle bot[3]={{0}};
            for(unsigned q=0;q<3u;q++)
                avp_mp_airlock_cycle_arrived(&bot[q],from[i],to[i],500u);
            for(unsigned tick=501u;tick<1050u;tick+=7u)
                for(unsigned q=0;q<3u;q++)
                    CHECK(avp_mp_airlock_cycle_reverse_block(&bot[q],
                        to[i],from[i],1,0,1,tick));
            /* One bot can leave physically, but a human still in the
             * airlock or stranded back at the origin isn't a new trip. */
            CHECK(avp_mp_airlock_cycle_reverse_block(&bot[0],
                 to[i],from[i],0,0,1,1100u));
            CHECK(bot[0].arrived_level==to[i]);
            /* Human is outside but bot is still inside: follow the human
             * first, don't reach for the pressure control. */
            CHECK(avp_mp_airlock_cycle_reverse_block(&bot[1],
                 to[i],from[i],1,1,1,1100u));
            /* Only AFTER both are outside may a genuine second passage
             * be planned. An unrelated level target was never blocked. */
            CHECK(!avp_mp_airlock_cycle_reverse_block(&bot[0],
                 to[i],from[i],0,1,1,1101u));
            CHECK(!bot[0].arrived_level);
            CHECK(!avp_mp_airlock_cycle_reverse_block(&bot[1],
                 to[i],from[i],0,1,1,1101u));
            CHECK(!avp_mp_airlock_cycle_reverse_block(&bot[2],
                 to[i],99u,1,0,1,1101u));
            /* Without any living human, autonomous movement must still
             * become possible, but never while standing in the chamber. */
            CHECK(avp_mp_airlock_cycle_reverse_block(&bot[2],
                 to[i],from[i],1,0,0,2000u));
            CHECK(!avp_mp_airlock_cycle_reverse_block(&bot[2],
                 to[i],from[i],0,0,0,2001u));
            /* A later authentic transfer in the opposite direction sets
             * a fresh arrival guard rather than resurrecting stale data. */
            avp_mp_airlock_cycle_arrived(&bot[0],to[i],from[i],2200u);
            CHECK(avp_mp_airlock_cycle_reverse_block(&bot[0],
                 from[i],to[i],1,0,1,2300u));
            CHECK(!avp_mp_airlock_cycle_reverse_block(&bot[0],
                 from[i],to[i],0,1,1,2301u));
        }
        puts("R299: four original ship portal pairs, no automatic reverse cycle: PASS");
    }
    puts("PASS: authored chamber boundaries, offsets, shared arrivals and collision grace");
    return 0;
}
