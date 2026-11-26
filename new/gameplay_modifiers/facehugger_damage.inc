#include "enemy_stats.h"
static unsigned host_facehugger_damage_units=4u;
void avp_mazescrn_set_facehugger_damage(unsigned units)
{host_facehugger_damage_units=avp_stat_units(units);}
