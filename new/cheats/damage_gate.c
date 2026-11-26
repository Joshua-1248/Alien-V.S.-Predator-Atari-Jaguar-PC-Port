#include "damage_gate.h"
static int damage_blocked;
void avp_new_damage_gate_set(int blocked){damage_blocked=!!blocked;}
int avp_new_damage_gate_blocked(void){return damage_blocked;}
