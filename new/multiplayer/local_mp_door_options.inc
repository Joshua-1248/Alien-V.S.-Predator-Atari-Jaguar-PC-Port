/* Local multiplayer adapter only; does not modify original Jaguar behavior. */
static int mp_dm_no_terminal,mp_dm_no_travel;
/* Scoped to the autonomous actor's native tick; never affects humans/SP. */
static int mp_bot_airlock_interlock;
void avp_doors_mp_bot_airlock_interlock(int enabled)
{mp_bot_airlock_interlock=!!enabled;}
static int host_predator_jammed_melee;
static int host_jammed_forced_ever;
void avp_doors_predator_jammed_attack_enabled(int enabled){host_predator_jammed_melee=!!enabled;}
void avp_doors_mp_restrict_interactions(int deathmatch,int level_travel)
{
    mp_dm_no_terminal=!!deathmatch;
    mp_dm_no_travel=!!deathmatch && !level_travel;
}
