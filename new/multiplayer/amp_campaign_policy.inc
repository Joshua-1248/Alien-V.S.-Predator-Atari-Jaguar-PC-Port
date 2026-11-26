/* The retail Alien Level 15 ending is normally installed by entering the
 * level. In mixed co-op, a Predator's death can put them directly on that
 * level as a RESPAWN, without a campaign exit/entry. Defer the encounter
 * ONLY for that abnormal co-op entrance; normal retail initialization is
 * unchanged. The host arms the original make_end_queen on a real transfer. */
static int mp_defer_end_queen;
static s16 mp_campaign_type=-1;
/* Co-op Predator campaign shares the retail AMP score routine. Deathmatch
 * does not: it must never inherit these campaign-only honor options. */
static int host_predator_honor_mp_coop;
void avp_amp_mp_defer_end_queen(int enabled){mp_defer_end_queen=enabled!=0;}
void avp_amp_mp_set_campaign_type(s16 type)
{
    mp_campaign_type=type;
    if(type<0)host_predator_honor_mp_coop=0;
}
void avp_amp_mp_set_predator_honor_coop(int enabled)
{
    host_predator_honor_mp_coop=enabled!=0;
}
/* Query only for the Queen's endgame branch: all ordinary combat/score/egg
 * target checks still use that individual actor's original player_type. */
int avp_amp_mp_queen_predator_ending(s16 acting_player_type)
{
    return (mp_campaign_type>=0?mp_campaign_type:acting_player_type)==PT_PREDATOR;
}

