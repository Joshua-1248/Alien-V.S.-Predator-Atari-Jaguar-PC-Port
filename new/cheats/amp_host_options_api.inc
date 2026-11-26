/* Original enemy proximity/contact is a world-aligned square, unchanged
 * by player rotation. An optional PC-only radial contact filter removes its
 * diagonal corner reach for harmful contact, without altering pursuit,
 * ambient range, line-of-sight, world solidity, or player movement. */
void avp_amp_set_normalize_diagonal_contact_range(int enabled)
{ host_normalize_diagonal_contact_range=enabled!=0; }
void avp_amp_set_normalize_diagonal_alien_death_spray_range(int enabled)
{ host_normalize_diagonal_alien_death_spray_range=enabled!=0; }
int avp_amp_normalize_diagonal_alien_death_spray_range(void)
{ return host_normalize_diagonal_alien_death_spray_range; }
void avp_amp_set_normalize_diagonal_enemy_movement_speed(int enabled)
{ host_normalize_diagonal_enemy_movement_speed=enabled!=0; }
int avp_amp_normalize_diagonal_enemy_movement_speed(void)
{ return host_normalize_diagonal_enemy_movement_speed; }
void avp_amp_set_enemy_activation_distance(unsigned mode)
{ host_enemy_activation_distance=mode<=4u?mode:0u; }
unsigned avp_amp_enemy_activation_distance(void)
{ return host_enemy_activation_distance; }
int avp_amp_normalize_diagonal_contact_range(void)
{ return host_normalize_diagonal_contact_range; }
static int host_enemies_frozen;
void avp_amp_set_attacks_disabled(int enabled){host_attacks_disabled=enabled!=0;}
int avp_amp_attacks_disabled(void){return host_attacks_disabled;}
void avp_amp_set_marine_pulse_los_hits(int enabled){host_marine_pulse_los_hits=enabled!=0;}
int avp_amp_marine_pulse_los_hits(void){return host_marine_pulse_los_hits;}
void avp_amp_set_marine_fire_interrupt(int enabled){host_marine_fire_interrupt=enabled!=0;}
int avp_amp_marine_fire_interrupt(void){return host_marine_fire_interrupt;}
void avp_amp_set_marine_multiplayer_target(int enabled)
{ host_marine_multiplayer_target=enabled!=0; }
void avp_amp_set_facehug_attach_disabled(int enabled){host_facehug_attach_disabled=enabled!=0;}
int avp_amp_facehug_attach_disabled(void){return host_facehug_attach_disabled;}
void avp_amp_set_enemies_frozen(int enabled){host_enemies_frozen=enabled!=0;}
int avp_amp_enemies_frozen(void){return host_enemies_frozen;}
