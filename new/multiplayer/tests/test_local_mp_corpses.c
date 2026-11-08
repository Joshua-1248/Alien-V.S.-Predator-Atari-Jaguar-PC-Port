#include "local_mp_corpses.h"
#include "local_mp_sprite_pose.h"
#include "amp.h"
#include "avp_sfx_ids.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(e) do {if(!(e)){fprintf(stderr,"FAILED at line %d: %s\n",__LINE__,#e);exit(1);}}while(0)
int main(void)
{
    AvpMpCorpses *c=calloc(1,sizeof(*c));
    AvpGpuOtherPlayer *draw=calloc(AVP_GPU_OTHER_PLAYERS_MAX,sizeof(*draw));
    CHECK(c && draw);
    CHECK(avp_mp_death_sfx(0)==AVP_FX_DEATH);
    CHECK(avp_mp_death_sfx(1)==AVP_FX_AHIT);
    CHECK(avp_mp_death_sfx(2)==AVP_FX_PD2);
    CHECK(avp_mp_death_sfx(3)==0);
    /* A just-killed actor remains rendered in death frame zero until its
     * corpse is registered on the next source player tick. */
    CHECK(!avp_mp_sprite_pending_death_pose(1).active);
    CHECK(avp_mp_sprite_pending_death_pose(0).active);
    CHECK(avp_mp_sprite_pending_death_pose(0).sequence==AS_DEATH);
    CHECK(avp_mp_sprite_pending_death_pose(-200).frame==0);

    CHECK(!avp_mp_corpse_add(c,0,0,0,0,0,0));
    CHECK(!avp_mp_corpse_add(c,1,3,0,0,0,0));
    CHECK(avp_mp_corpse_add(c,14,0,0x12340000,0x56780000,0x9100,10));
    CHECK(avp_mp_corpse_add(c,14,1,0x22340000,0x66780000,0x9200,10));
    CHECK(avp_mp_corpse_add(c,15,2,0x32340000,0x76780000,0x9300,10));
    CHECK(!avp_mp_corpse_claim_at(c,14,0u,0x22340000,0x66780000));
    CHECK(!avp_mp_corpse_claim_at(c,15,0u,0x32340000,0x76780000));
    CHECK(avp_mp_corpse_claim_at(c,14,0u,0x12340000,0x56780000));
    CHECK(!avp_mp_corpse_claim_at(c,14,0u,0x12340000,0x56780000));
    CHECK(avp_mp_corpse_claim_at(c,14,1u,0x12340000,0x56780000));
    CHECK(avp_mp_corpse_claim_at(c,14,8u,0x12340000,0x56780000));
    CHECK(!avp_mp_corpse_claim_at(c,14,8u,0x12340000,0x56780000));
    CHECK(avp_mp_corpse_claim_at(c,14,15u,0x12340000,0x56780000));
    CHECK(!avp_mp_corpse_claim_at(c,14,15u,0x12340000,0x56780000));
    CHECK(!avp_mp_corpse_claim_at(c,14,16u,0x12340000,0x56780000));
    CHECK(!avp_mp_corpse_claim_at(c,15,0u,0x12340000,0x56780000));
    CHECK(avp_mp_corpses_for_level(c,14,10,draw,AVP_GPU_OTHER_PLAYERS_MAX)==2);
    CHECK(draw[0].creature==AC_HUMAN && draw[0].animation==AS_DEATH && draw[0].frame==0);
    CHECK(draw[1].creature==AC_ALIEN && draw[1].frame==0);
    CHECK(avp_mp_corpses_for_level(c,14,12,draw,AVP_GPU_OTHER_PLAYERS_MAX)==2);
    CHECK(draw[0].frame==1 && draw[1].frame==2);
    CHECK(avp_mp_corpses_for_level(c,14,60,draw,AVP_GPU_OTHER_PLAYERS_MAX)==2);
    CHECK(draw[0].frame==2 && draw[1].frame==3);
    CHECK(draw[0].x==0x12340000 && draw[0].y==0x56780000 && draw[0].angle==0x9100);
    CHECK(avp_mp_corpses_for_level(c,15,20,draw,AVP_GPU_OTHER_PLAYERS_MAX)==1);
    CHECK(draw[0].creature==AC_PREDATOR && draw[0].frame==4);
    CHECK(avp_mp_corpses_for_level(c,3,20,draw,AVP_GPU_OTHER_PLAYERS_MAX)==0);
    CHECK(avp_mp_corpses_for_level(c,14,60,draw,1)==1);
    for(unsigned i=3;i<AVP_MP_CORPSE_LIMIT;i++)CHECK(avp_mp_corpse_add(c,15,i%3,0x55550000+i,0x44440000,0,100+i));
    CHECK(c->evictions==0);
    CHECK(avp_mp_corpse_add(c,14,2,0x55550000,0x44440000,0,400));
    CHECK(c->evictions==1); /* Capacity-bounded: oldest body evicted. */
    CHECK(avp_mp_corpses_for_level(c,14,400,draw,AVP_GPU_OTHER_PLAYERS_MAX)==2);
    free(draw);free(c);
    puts("PASS: native per-species death sounds, source sprite timelines, non-colliding corpse retention and cross-world separation");
    return 0;
}
