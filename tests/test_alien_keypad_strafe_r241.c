/* RE41 R241: direct Jaguar keypad strafe derived from original ROM traces. */
/* R240 retail-ROM-verified D-pad modifier and fixed-point movement regression. */
#define main old_continuous_aim_main
#include "test_pc_continuous_aim_r261.c"
#undef main
#include <stdlib.h>
#include "pc/pc_input_routing.h"

static void prep(int species, u32 active_pad){
  player_type=(s16)species;InitMove();ResetMove();
  centre_angle=0xc0000000u;
  x_pos=0x00188000u;y_pos=0x00135000u;
  sin_ang=-32767;cos_ang=0;joy_cur=0xffffffffu & ~active_pad;
  lift_key=0;hugkill=0;
  avp_player_host_keyboard_strafe_during_turn(0);
  avp_player_host_analog_motion(0,0,0);
  avp_player_host_analog_aim(0,0,1,1);
  avp_player_host_aim_options(0,0);
  avp_player_host_live_heading(0,0);
  avp_player_host_quick_turn_reset();
}

u8 cur_weps=0; s16 new_wepno=0;
u32 TFX(unsigned id){(void)id;return 0;}
void StopLoop(u32 handle){(void)handle;}

static void check_three(u32 key, u32 x1, u32 x2, u32 x3){
  const u32 up=(1u<<JOY_UP);
  prep(PT_ALIEN,up|key);
  MovePlayer(); assert(centre_angle==0xc0000000u && x_pos==x1 && y_pos==0x00136000u);
  MovePlayer(); assert(centre_angle==0xc0000000u && x_pos==x2 && y_pos==0x00137c00u);
  MovePlayer(); assert(centre_angle==0xc0000000u && x_pos==x3 && y_pos==0x0013a100u);
}
int main(void){
  check_three(1u<<KEY_7,0x00188800u,0x00189600u,0x0018a880u);
  check_three(1u<<KEY_9,0x00187800u,0x00186a00u,0x00185780u);
  for(unsigned n=0;n<3;n++){
    int species=n==0?PT_ALIEN:n==1?PT_HUMAN:PT_PREDATOR;
    prep(species,1u<<KEY_7);MovePlayer();assert(x_pos>0x00188000u && centre_angle==0xc0000000u);
    prep(species,1u<<KEY_9);MovePlayer();assert(x_pos<0x00188000u && centre_angle==0xc0000000u);
    prep(species,(1u<<KEY_7)|(1u<<KEY_9));MovePlayer();
    assert(x_pos==0x00188000u && centre_angle==0xc0000000u);
  }
  /* Alien C is Tail, not the sideways modifier. Original Jaguar Option
     + D-pad provides Alien sidestep, while Predator/Marine use C + D-pad. */
  prep(PT_ALIEN,(1u<<FIRE_C)|(1u<<JOY_RIGHT));MovePlayer();
  assert(centre_angle!=0xc0000000u);
  prep(PT_ALIEN,(1u<<OPTION)|(1u<<JOY_RIGHT));MovePlayer();
  assert(centre_angle==0xc0000000u);
  {u32 p=avp_pc_digit_pad(0xffffffffu,7u);
   assert(!(p&(1u<<KEY_7)) && (p&(1u<<KEY_9)) && (p&(1u<<KEY_5)));
   p=avp_pc_digit_pad(0xffffffffu,9u);
   assert(!(p&(1u<<KEY_9)) && (p&(1u<<KEY_7)) && (p&(1u<<KEY_5)));
  }
  prep(PT_PREDATOR,1u<<KEY_7);joy_edge=0xffffffffu;PlayerWeapons();
  assert(medpak_act==0); /* keypad 7 never activates Predator health pack */
  prep(PT_PREDATOR,1u<<KEY_5);joy_edge=0xffffffffu;PlayerWeapons();
  assert(medpak_act!=0); /* keypad 5 is the original Predator health pack */
  puts("R241 retail keypad 7/9, species chord and Predator medpak PASS");
  return 0;
}
