#include "local_mp_dm_loot.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    AvpAmmoInfo a[4];AvpMpDmLoot out;
    uint8_t player=0x30,weapon=0x30;
    unsigned counts[3]={0,0,0};
    avp_mp_dm_predator_weapons(&player,&weapon);
    assert(player==0x3e && weapon==0x3e); /* no unsolicited medkit */
    avp_mp_dm_predator_weapons(&player,&weapon);
    assert(player==0x3e && weapon==0x3e);
    avp_mp_dm_predator_weapons(NULL,NULL);
    memset(a,0,sizeof(a));
    for(unsigned i=0;i<4;i++){a[i].max=100;a[i].cur=0;}
    /* Exhaust all possible 16-bit chance draws; probabilities must be exact. */
    for(unsigned stage=0;stage<3u;stage++){
        unsigned owned=(1u<<1);
        if(stage>0u)owned|=1u<<2;
        if(stage>1u)owned|=1u<<3;
        for(unsigned roll=0;roll<65536u;roll++){
            assert(avp_mp_dm_corpse_loot((uint8_t)owned,a,(uint16_t)roll,0,&out));
            if(out.type==COLL_PULSERIFLE||out.type==COLL_FLAMETHROWER||
               out.type==COLL_SMARTGUN){
                assert(out.type==(stage==0u?COLL_PULSERIFLE:
                                 stage==1u?COLL_FLAMETHROWER:COLL_SMARTGUN));
                assert(out.amount==(stage==0u?60:stage==1u?25:20));
                counts[stage]++;
            }else{
                assert(out.type==COLL_SHOTAMMO&&out.amount==15);
            }
        }
    }
    assert(counts[0]==32768u && counts[1]==16384u && counts[2]==8192u);
    /* The independent third draw must succeed for exactly 1/16 of the
     * possible outcomes, regardless of weapon ownership or ammo capacity.
     * An already-owned motion tracker must never generate a duplicate. */
    {
        unsigned tracker_hits=0;
        for(unsigned roll=0;roll<65536u;roll++){
            const int hit=avp_mp_dm_tracker_bonus((uint16_t)roll,0);
            assert(hit==((roll&15u)==0u));
            assert(!avp_mp_dm_tracker_bonus((uint16_t)roll,1));
            tracker_hits+=(unsigned)hit;
        }
        assert(tracker_hits==4096u);
        for(unsigned i=0;i<4u;i++)a[i].cur=a[i].max;
        assert(avp_mp_dm_tracker_bonus(0u,0));
        assert(!avp_mp_dm_tracker_bonus(1u,0));
        assert(!avp_mp_dm_tracker_bonus(0u,1));
        for(unsigned i=0;i<4u;i++)a[i].cur=0;
        for(unsigned stage=0;stage<3u;stage++){
            unsigned owned=(1u<<1);
            if(stage>0u)owned|=1u<<2;
            if(stage>1u)owned|=1u<<3;
            /* Baseline weapon rolls are identical for both tracker results:
             * tracker is a bonus, not a mutually exclusive item table. */
            for(unsigned roll=0;roll<65536u;roll++){
                int is_weapon;
                assert(avp_mp_dm_corpse_loot((uint8_t)owned,a,
                      (uint16_t)roll,0,&out));
                is_weapon=out.type==COLL_PULSERIFLE||
                          out.type==COLL_FLAMETHROWER||
                          out.type==COLL_SMARTGUN;
                assert(is_weapon==((roll&((1u<<(stage+1u))-1u))==0u));
            }
        }
    }
    for(unsigned i=0;i<4u;i++)a[i].cur=0;
    /* Random ammo selection never yields ammo for a gun we do not possess. */
    for(unsigned roll=0;roll<256u;roll++){
        assert(avp_mp_dm_corpse_loot(0x1eu,a,65535u,(uint16_t)roll,&out));
        assert(out.type==COLL_SHOTAMMO||out.type==COLL_PULSEAMMO||
               out.type==COLL_FLAMEAMMO||out.type==COLL_SMARTAMMO);
    }
    a[0].cur=a[0].max;a[1].cur=a[1].max;a[3].cur=a[3].max;
    for(unsigned roll=0;roll<256u;roll++){
        assert(avp_mp_dm_corpse_loot(0x1eu,a,65535u,(uint16_t)roll,&out));
        assert(out.type==COLL_FLAMEAMMO&&out.amount==25);
    }
    assert(!avp_mp_dm_corpse_loot(0x1eu,NULL,0,0,&out));
    a[2].cur=a[2].max;
    assert(!avp_mp_dm_corpse_loot(0x1eu,a,65535u,0,&out));
    assert(!avp_mp_dm_corpse_loot(0u,a,1u,0u,&out));
    assert(avp_mp_dm_corpse_loot(0u,a,0u,0u,&out));
    assert(out.type==COLL_PULSERIFLE);
    puts("PASS: Predator Deathmatch loadout; Marine 1/2,1/4,1/8 weapon odds, owned ammo and independent 1/16 motion tracker");
    return 0;
}
