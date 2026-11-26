#include "pc/pc_original_cheats.h"
#include "pc/pc_video.h"
#include "player.h"
#include "collectables.h"
#include "weapons.h"
#include "levels.h"
#include "joypad.h"
#include "dev_cheats.h"
#include <stdio.h>

void avp_pc_original_cheat_apply(unsigned actions)
{
    if(actions&AVP_PC_ACTION_ORIG_TRACKER_TOGGLE){
        if(player_type==PT_HUMAN){
            show_mt=show_mt?0:-1;
            /* This is the original direct tracker toggle, not the temporary
             * ownership/borrowing policy used by New > Motion Tracker. */
            avp_dev_cheats_tracker_collected();
        }
    }
    for(unsigned slot=1;slot<=4;slot++){
        if(!(actions&(AVP_PC_ACTION_ORIG_WEAPON1<<(slot-1))))continue;
        if(player_type==PT_ALIEN && slot==4)continue;
        if(player_type!=PT_HUMAN && player_type!=PT_ALIEN && player_type!=PT_PREDATOR)continue;
        cur_weps=(u8)(cur_weps^(1u<<slot));
        if(!(cur_weps&(1u<<slot)) && cur_wepno==(s16)slot){
            /* Select a remaining owned weapon using the existing Jaguar
             * withdrawal/equip animation; never teleport the weapon sprite. */
            unsigned maxslot=player_type==PT_ALIEN?3u:4u;
            for(unsigned next=1;next<=maxslot;next++)
                if(cur_weps&(1u<<next)){new_wepno=(s16)next;break;}
        }
    }
    if(actions&AVP_PC_ACTION_ORIG_LEVEL_NEXT){
        unsigned level=(cur_level>=1&&cur_level<=15)?(unsigned)cur_level:1u;
        new_level=(s16)(level==15u?1u:level+1u);
        new_x=-1;new_y=-1;
    }else if(actions&AVP_PC_ACTION_ORIG_LEVEL_PREVIOUS){
        unsigned level=(cur_level>=1&&cur_level<=15)?(unsigned)cur_level:1u;
        new_level=(s16)(level==1u?15u:level-1u);
        new_x=-1;new_y=-1;
    }
}

u32 avp_pc_original_cheat_alien_pad(u32 pad)
{
    /* When the user actively removes an Alien attack, mask only that attack
     * input for the current original 10-Hz game tick. With all three weapons
     * owned (retail start), the input is bit-for-bit unchanged. */
    if(player_type!=PT_ALIEN)return pad;
    if(!(cur_weps&(1u<<1)))pad|=1u<<FIRE_C; /* tail */
    if(!(cur_weps&(1u<<2)))pad|=1u<<FIRE_A; /* claw/swipe */
    if(!(cur_weps&(1u<<3)))pad|=1u<<FIRE_B; /* bite */
    return pad;
}
void avp_pc_original_cheat_gps_format(char *out,unsigned capacity,
    unsigned level,u32 x,u32 y)
{
    if(!out || !capacity)return;
    /* Original 16.16 position: integer cell as decimal and 8-bit-subcell
     * fine position as hex. Level IDs remain original 1..15. */
    (void)snprintf(out,capacity,"dec(%02u,%02u)  hex(%04X,%04X)  l(%02u)",
        (unsigned)(x>>16),(unsigned)(y>>16),
        (unsigned)((x>>8)&0xffffu),(unsigned)((y>>8)&0xffffu),level);
}
