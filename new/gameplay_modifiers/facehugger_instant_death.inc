/* PC-only modifier: resolve lethal damage at successful native attachment.
 * Keep the native hug flag, actor release and ordinary death handling. */
static int host_facehugger_instant_death;
void avp_amp_set_facehugger_instant_death(int enabled)
{host_facehugger_instant_death=!!enabled;}
static void facehugger_attachment_modifier(void)
{
    if(host_facehugger_instant_death && player_energy>0)
        player_energy=AVP_NEW_DAMAGE_RESULT(player_energy,0);
}
