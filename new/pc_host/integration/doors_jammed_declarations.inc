/* R193 optional PC Predator-campaign melee override. Never toggles a jammed
 * door closed; the original access message remains DOOR JAMMED. */
void avp_doors_predator_jammed_attack_enabled(int enabled);
int avp_doors_predator_melee_jammed(s16 weapon,u32 max_distance);
