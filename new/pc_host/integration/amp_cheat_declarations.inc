/* Multiplayer extension only. Reuses original acid contact math and damage
 * against the currently scoped player; never invoked by single-player. */
int avp_amp_acid_contact_player(const AvpAmp *puddle);
/* Exact native acid mode; false during the preceding death animation. */
int avp_amp_is_acid_puddle(const AvpAmp *puddle);
/* PC-only developer option: suspend hostile AMP state-machine updates. */
/* PC development cheats, inactive by default. All normal Jaguar paths are retained. */
void avp_amp_set_attacks_disabled(int enabled);
int avp_amp_attacks_disabled(void);
/* Optional PC-only Marine pulse-rifle final-frame line-of-sight damage gate. */
void avp_amp_set_marine_pulse_los_hits(int enabled);
int avp_amp_marine_pulse_los_hits(void);
/* Optional PC-only fatal-hit interruption of enemy Marine pulse bursts. */
/* PC-only opt-in: contact, latch and melee hits use radial reach instead
 * of Jaguar's world-axis-aligned square range. Facing/turning never changes
 * the retail hitbox. Default OFF preserves all original comparisons. */
void avp_amp_set_normalize_diagonal_contact_range(int enabled);
/* Independently opt into a 96-unit circular Alien death spray, not its
 * 30-unit corpse acid or any other contact damage. Default OFF. */
void avp_amp_set_normalize_diagonal_alien_death_spray_range(int enabled);
int avp_amp_normalize_diagonal_alien_death_spray_range(void);
/* Independent opt-in enemy AMP diagonal chase speed correction; OFF is Jaguar. */
void avp_amp_set_normalize_diagonal_enemy_movement_speed(int enabled);
int avp_amp_normalize_diagonal_enemy_movement_speed(void);
/* PC-only enemy pursuit wake distance: 0 original, 1=2x, 2=3x, 3=4x, 4 unlimited. */
void avp_amp_set_enemy_activation_distance(unsigned mode);
unsigned avp_amp_enemy_activation_distance(void);
int avp_amp_normalize_diagonal_contact_range(void);
void avp_amp_set_marine_fire_interrupt(int enabled);
int avp_amp_marine_fire_interrupt(void);
/* Bound only during a native multiplayer AMP-target callback; default off. */
void avp_amp_set_marine_multiplayer_target(int enabled);
/* type==-1 kills all living hostiles; otherwise exact AvpCreatureType. */
unsigned avp_amp_dev_kill_enemies(int type);
/* PC cheat only: remove all hostile AMP actors from the currently bound
 * level without invoking deaths, scoring, pickups, or Queen progression. */
unsigned avp_amp_dev_clear_enemies(void);
/* PC cheat: despawn only damaging level hazards (explosive drums, Queen
 * electric shields/generators, and active Alien acid), no death triggers. */
unsigned avp_amp_dev_clear_hazards(void);
void avp_amp_set_facehug_attach_disabled(int enabled);
int avp_amp_facehug_attach_disabled(void);
void avp_amp_set_enemies_frozen(int enabled);
int avp_amp_enemies_frozen(void);
