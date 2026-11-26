#ifndef AVP_ENEMY_STATS_H
#define AVP_ENEMY_STATS_H
/* Quarter units, 4 = native 1x. No float state or RNG changes. */
enum { AVP_STAT_ALIEN, AVP_STAT_CRAWLER, AVP_STAT_FACEHUGGER, AVP_STAT_EGG,
       AVP_STAT_MARINE, AVP_STAT_PREDATOR, AVP_STAT_QUEEN, AVP_STAT_ENEMIES };
enum { AVP_STAT_DAMAGE, AVP_STAT_HEALTH, AVP_STAT_SPEED, AVP_STAT_FIELDS };
static inline unsigned avp_stat_units(unsigned n){return n>=1u&&n<=16u?n:4u;}
static inline int avp_stat_scale(int base,unsigned units)
{
    units=avp_stat_units(units);
    if(units==4u || base<=0)return base;
    int result=(base*(int)units+2)/4;
    return result>32767?32767:result<1?1:result;
}
void avp_amp_set_facehugger_instant_death(int enabled);
void avp_amp_set_enemy_stat(unsigned enemy,unsigned field,unsigned units);
void avp_mazescrn_set_facehugger_damage(unsigned units);
#endif
