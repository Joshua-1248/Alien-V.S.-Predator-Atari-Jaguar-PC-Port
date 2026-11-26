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
int main(void){
  const u32 UP=(1u<<JOY_UP),RIGHT=(1u<<JOY_RIGHT);
  prep(PT_ALIEN,UP|RIGHT);
  MovePlayer(); assert(centre_angle==0xbd000000u); /* exact retail ROM after UP+RIGHT */
  prep(PT_ALIEN,UP|RIGHT|(1u<<OPTION));
  MovePlayer(); assert(centre_angle==0xc0000000u);
  assert(x_pos==0x00187800u&&y_pos==0x00136000u); /* ROM field 2259 */
  MovePlayer(); assert(x_pos==0x00186a00u&&y_pos==0x00137c00u); /* ROM field 2264 */
  MovePlayer(); assert(x_pos==0x00185780u&&y_pos==0x0013a100u); /* ROM field 2269 */
  for(unsigned i=0;i<3;i++){
    int species=i==0?PT_ALIEN:i==1?PT_HUMAN:PT_PREDATOR;
    u32 modifier=1u<<(species==PT_ALIEN?OPTION:FIRE_C);
    prep(species,RIGHT);
    MovePlayer();assert(centre_angle!=0xc0000000u);
    prep(species,RIGHT|modifier);
    MovePlayer();assert(centre_angle==0xc0000000u);
    assert(x_pos<0x00188000u);
  }
  prep(PT_ALIEN,UP);
  avp_player_host_analog_motion(1,0,1);
  MovePlayer();
  assert(x_pos<0x00188000u); /* analog independent sidestep even while ordinary D-pad is in turn mode */
  prep(PT_ALIEN,UP|RIGHT);
  avp_player_host_keyboard_strafe_during_turn(-1);
  MovePlayer();
  assert(centre_angle==0xbd000000u&&x_pos>0x00188000u); /* signed LEFT strafe at angle c000 --> +X */
  /* Host PC-action mapping must preserve the original turn/strafe semantics
     despite independent remappable left/right strafe keys and turn keys. */
  for(unsigned species=0;species<3;species++){
    unsigned modifier=species==1?OPTION:FIRE_C;
    u32 p=avp_pc_keyboard_action_horizontal_pad(0xffffffffu,0,1,0,0,modifier);
    assert(!(p&(1u<<JOY_RIGHT)) && (p&(1u<<modifier))); /* D: unmodified turn */
    p=avp_pc_keyboard_action_horizontal_pad(0xffffffffu,0,0,0,1,modifier);
    assert(!(p&(1u<<JOY_RIGHT)) && !(p&(1u<<modifier))); /* E: modifier+strafe */
    p=avp_pc_keyboard_action_horizontal_pad(0xffffffffu,0,1,1,0,modifier);
    assert(!(p&(1u<<JOY_RIGHT)) && (p&(1u<<JOY_LEFT)) && (p&(1u<<modifier)));
    assert(avp_pc_keyboard_chord_strafe(0,1,1,0)==-1);
    p=avp_pc_strafe_chord_pad(0xffffffffu,1,0,modifier);
    assert(!(p&(1u<<JOY_LEFT)) && !(p&(1u<<modifier))); /* dedicated gamepad strafe */
  }
  puts("R240 ROM-verified Alien/Marine/Predator controls and PC input chords PASS");
  return 0;
}
