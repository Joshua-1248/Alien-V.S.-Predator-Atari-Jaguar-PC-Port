/* The original mode pointer is authoritative: a dying Alien body does
 * not emit damaging floor acid until amp_finish_death() installs amp_acid.
 * Read-only co-op bot hazard discovery must not mistake death frames or
 * decorative sprites for live acid contact. */
int avp_amp_is_acid_puddle(const AvpAmp *a)
{
    return a && !a->host_static && a->mode==amp_acid;
}

/* The original acid contact/damage operation, extracted without changing
 * its square-range test or 3-energy decrement. Single-player continues to
 * use amp_acid() below. The co-op world tick invokes this once per eligible
 * player, under that player's original PLAYER.S scope. */
int avp_amp_acid_contact_player(const AvpAmp *a)
{
    if(!a || player_type==PT_ALIEN || !amp_contact_in_range(a,30))return 0;
    player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,amp_word_add(player_energy,-3));
    return 1;
}
