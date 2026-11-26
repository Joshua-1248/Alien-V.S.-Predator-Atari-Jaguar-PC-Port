static int host_attacks_disabled;
/* RE39 R177: opt-in Predator SINGLE-PLAYER campaign cloaked-kill honor rules.
 * Defaults OFF; never changes the Jaguar scoring path unless enabled. */
static int host_predator_cloak_no_honor_loss;
static int host_predator_cloak_half_honor;
void avp_amp_set_predator_cloak_honor_options(int no_loss,int half_value)
{
    host_predator_cloak_no_honor_loss=no_loss!=0;
    host_predator_cloak_half_honor=half_value!=0;
}

/* PC-only optional Marine pulse-rifle damage check at the authored final frame. */
static int host_marine_pulse_los_hits;
/* Optional PC-only fatal-hit interruption of the Marine pulse attack. */
static int host_marine_fire_interrupt;
/* The native co-op target scope is NOT a Jaguar campaign. Apply the
 * host enhancements to the currently bound actor of any species there. */
static int host_marine_multiplayer_target;
static int host_facehug_attach_disabled;
static int host_dev_death;
static char host_enemy_bolt_explosion_tag;
static AvpAmpMpProjectileSpawn mp_projectile_spawn;
static AvpAmpMpProjectileHit mp_projectile_hit;
static AvpAmpMpExplosionSpawn mp_explosion_spawn;
static AvpAmpMpExplosionDamage mp_explosion_damage;
static void *mp_projectile_user;
void avp_amp_set_mp_projectile_hooks(AvpAmpMpProjectileSpawn spawn,
    AvpAmpMpProjectileHit hit,AvpAmpMpExplosionSpawn explosion_spawn,
    AvpAmpMpExplosionDamage explosion_damage,void *user)
{
    mp_projectile_spawn=spawn;
    mp_projectile_hit=hit;
    mp_explosion_spawn=explosion_spawn;
    mp_explosion_damage=explosion_damage;
    mp_projectile_user=user;
}

