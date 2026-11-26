#include "player.h"
#include "avp_runtime.h"
#include "joypad.h"
#include "../health_policy.h"
#include "../damage_gate.h"
#include <assert.h>
#include <stdio.h>
volatile u32 frame_counter;
s16 hugkill;
int main(void){
    AvpRuntimeOps o={0};avp_runtime_bind(&o);
    for(int s=PT_HUMAN;s<=PT_PREDATOR;s+=4){
        player_type=(s16)s;max_energy=800;player_energy=700;old_energy=800;pain=0;fade_c=0x8880u;destruct_flag=0;frame_counter+=25;
        avp_new_health_apply(AVP_NEW_HEALTH_INFINITE,&player_energy,&old_energy,max_energy);
        assert(player_energy==800);Pain();assert(pain==99&&fade_c!=0x8880u&&player_energy==800&&old_energy==800);
        pain=0;fade_c=0x8880u;player_energy=700;old_energy=700;frame_counter+=25;
        avp_new_health_before_actor(AVP_NEW_HEALTH_INVINCIBLE,&player_energy,&old_energy,max_energy);
        avp_new_damage_gate_set(1);
        player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,(s16)(player_energy-100));Pain();assert(pain==0&&fade_c==0x8880u);
        avp_new_health_apply(AVP_NEW_HEALTH_INVINCIBLE,&player_energy,&old_energy,max_energy);
        assert(player_energy==700&&old_energy==700);
        avp_new_damage_gate_set(0);
    }
    puts("PASS native Pain() integration: all three species, Infinite Health visual pain, Invincibility suppresses new pain");return 0;
}
