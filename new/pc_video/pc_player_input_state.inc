/* The F12 cheat is a host-only switch, rebound for each human in local MP.
 * Do not put it in the original Jaguar actor snapshot or change InitMove(). */
static int host_double_movement,host_movement_dm_fast;
void avp_player_host_movement_speed(int step,int dm_fast)
{ host_double_movement=avp_pc_move_step(step);host_movement_dm_fast=!!dm_fast; }
void avp_player_host_double_movement(int step)
{ avp_player_host_movement_speed(step,0); }
static int host_analog_motion;
static float host_analog_forward,host_analog_strafe;
static int host_keyboard_strafe_during_turn;
static int host_analog_aim;
static float host_aim_turn,host_aim_sensitivity,host_aim_speed;
/* Co-op-only digital controller tuning: 1.0 means exact retail behavior.
 * Captured/restored with the per-player engine snapshot. */
static float host_digital_turn_sensitivity=1.0f,host_digital_turn_speed=1.0f;
static int host_enhanced_analog_aim,host_mouse_aim;
static AvpPcContinuousHeading host_continuous_heading;
static int host_live_heading_enabled;
static u32 host_live_heading_value;
static int host_quick_turn_enabled;
static u32 host_quick_turn_heading;
static int host_quick_turn_preserve_heading;
void avp_player_host_quick_turn_heading(int enabled,u32 heading)
{
    host_quick_turn_enabled=enabled?1:0;
    host_quick_turn_heading=heading;
    if(enabled){
        host_quick_turn_preserve_heading=1;
        avp_pc_continuous_heading_reset(&host_continuous_heading);
    }
}
void avp_player_host_live_heading(int enabled,u32 heading)
{ host_live_heading_enabled=enabled?1:0;host_live_heading_value=heading; }
void avp_player_host_quick_turn_reset(void)
{ host_quick_turn_enabled=0;host_quick_turn_preserve_heading=0; }
void avp_player_host_aim_options(int enhanced_analog,int mouse_mode)
{
    host_enhanced_analog_aim=enhanced_analog?1:0;
    host_mouse_aim=mouse_mode?1:0;
    if(!host_enhanced_analog_aim||host_mouse_aim)
        avp_pc_continuous_heading_reset(&host_continuous_heading);
}

void avp_player_host_analog_aim(int enabled,float turn,float sensitivity,float speed)
{
    host_analog_aim=enabled?1:0;
    host_aim_turn=turn;
    host_aim_sensitivity=sensitivity;
    host_aim_speed=speed;
}
void avp_player_host_analog_motion(int enabled,float forward,float strafe)
{
    host_analog_motion=enabled?1:0;
    host_analog_forward=forward>1.0f?1.0f:(forward< -1.0f?-1.0f:forward);
    host_analog_strafe=strafe>1.0f?1.0f:(strafe< -1.0f?-1.0f:strafe);
}
void avp_player_host_keyboard_strafe_during_turn(int direction)
{
    host_keyboard_strafe_during_turn=(direction>0)-(direction<0);
}
