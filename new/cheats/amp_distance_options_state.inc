/* Host-only optional distance filter. Zero-initialized, so retail square
 * comparisons remain the default even before the F12 settings are loaded. */
static int host_normalize_diagonal_contact_range;
/* Independent PC-only Alien death spray radius. Jaguar square remains default. */
static int host_normalize_diagonal_alien_death_spray_range;
/* R297: independent, opt-in diagonal enemy chase speed correction. */
static int host_normalize_diagonal_enemy_movement_speed;
/* PC-only pursuit wake distance: 0 original, 1..3 2x..4x, 4 unlimited. */
static unsigned host_enemy_activation_distance;
