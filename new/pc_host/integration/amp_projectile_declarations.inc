/* Optional PC-local Deathmatch projectile callbacks. All default to NULL
 * and are never installed for single-player or co-op. Original AMP modes,
 * damage tables and collision handling remain the authorities for enemies. */
typedef void (*AvpAmpMpProjectileSpawn)(void *user,const AvpAmp *projectile);
typedef int (*AvpAmpMpProjectileHit)(void *user,const AvpAmp *projectile);
typedef void (*AvpAmpMpExplosionSpawn)(void *user,const AvpAmp *bolt,
                                        const AvpAmp *explosion_amp);
typedef int (*AvpAmpMpExplosionDamage)(void *user,const AvpAmp *explosion_amp);
void avp_amp_set_mp_projectile_hooks(AvpAmpMpProjectileSpawn spawn,
    AvpAmpMpProjectileHit hit,AvpAmpMpExplosionSpawn explosion_spawn,
    AvpAmpMpExplosionDamage explosion_damage,void *user);
