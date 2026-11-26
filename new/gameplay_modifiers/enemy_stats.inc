#include "enemy_stats.h"
static unsigned char host_enemy_stats[AVP_STAT_ENEMIES][AVP_STAT_FIELDS];
void avp_amp_set_enemy_stat(unsigned enemy,unsigned field,unsigned units)
{
    if(enemy<AVP_STAT_ENEMIES && field<AVP_STAT_FIELDS)
        host_enemy_stats[enemy][field]=(unsigned char)avp_stat_units(units);
}
static unsigned enemy_stat(unsigned enemy,unsigned field)
{return enemy<AVP_STAT_ENEMIES?avp_stat_units(host_enemy_stats[enemy][field]):4u;}
static unsigned enemy_stat_category(u16 creature)
{
    switch(creature){
    case AC_ALIEN:return AVP_STAT_ALIEN;
    case AC_ACRAWL:return AVP_STAT_CRAWLER;
    case AC_FACEHUG:return AVP_STAT_FACEHUGGER;
    case AC_EGG:return AVP_STAT_EGG;
    case AC_HUMAN:return AVP_STAT_MARINE;
    case AC_PREDATOR:return AVP_STAT_PREDATOR;
    case AC_AQUEEN:return AVP_STAT_QUEEN;
    default:return AVP_STAT_ENEMIES;
    }
}
static int enemy_stat_damage(unsigned enemy,int base)
{return avp_stat_scale(base,enemy_stat(enemy,AVP_STAT_DAMAGE));}
static int enemy_actor_damage(const AvpAmp *a,int base)
{return enemy_stat_damage(enemy_stat_category(a->creature),base);}
static int enemy_projectile_damage(const AvpAmp *a,int base)
{
    if(a->flags&(1u<<AMP_PLAYER))return base;
    return enemy_stat_damage(a->creature==AC_FIRE?AVP_STAT_MARINE:AVP_STAT_PREDATOR,base);
}
static s16 enemy_actor_speed(const AvpAmp *a,s16 base)
{return (s16)avp_stat_scale(base,enemy_stat(enemy_stat_category(a->creature),AVP_STAT_SPEED));}
static void enemy_stat_spawn(AvpAmp *a)
{
    unsigned type=enemy_stat_category(a->creature);
    unsigned units=enemy_stat(type,AVP_STAT_HEALTH);
    if(units==4u || !(a->flags&(1u<<AMP_KILLABLE)) || a->energy<=0)return;
    a->energy=(s16)avp_stat_scale(a->energy,units);
    a->oldenergy=(s16)avp_stat_scale(a->oldenergy,units);
    /* Egg damage detection compares health against the template's ldir. */
    if(type==AVP_STAT_EGG)a->ldir=(s16)avp_stat_scale(a->ldir,units);
}
