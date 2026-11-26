/* Optional host analog movement and horizontal aiming; Jaguar digital control is unchanged. */
void avp_player_host_analog_motion(int enabled,float forward,float strafe);
/* Session-only PC cheat: 0..3 = 1x..4x, not turn rate. */
void avp_player_host_double_movement(int step);
void avp_player_host_movement_speed(int step,int dm_fast);
/* Only for simultaneous PC-action keyboard sidestep plus turn. */
void avp_player_host_keyboard_strafe_during_turn(int direction);
void avp_player_host_analog_aim(int enabled,float turn,float sensitivity,float speed);
void avp_player_host_aim_options(int enhanced_analog,int mouse_mode);
