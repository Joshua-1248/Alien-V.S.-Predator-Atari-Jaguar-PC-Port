#ifndef AVP_NEW_DAMAGE_GATE_H
#define AVP_NEW_DAMAGE_GATE_H
/* PC-only protection at native damage assignments. No healing or pain-state
 * changes. Non-PC/reference builds execute the unmodified expression. */
void avp_new_damage_gate_set(int blocked);
int avp_new_damage_gate_blocked(void);
#ifdef AVP_NEW_INVINCIBILITY_GATE
#define AVP_NEW_DAMAGE_RESULT(current, damaged) (avp_new_damage_gate_blocked() ? (current) : (damaged))
#else
#define AVP_NEW_DAMAGE_RESULT(current, damaged) (damaged)
#endif
#endif
