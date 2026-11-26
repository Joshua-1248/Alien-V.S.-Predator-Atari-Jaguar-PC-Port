/* Native PC presentation preference only.  Damage, pain flashes, moan timing,
 * and the retail random-number sequence remain unchanged. */
static int pc_player_pain_sounds_enabled=1;
static int pc_facehug_pain_sounds_enabled=1;
void avp_player_set_facehug_pain_sounds(int enabled){pc_facehug_pain_sounds_enabled=enabled!=0;}
void avp_player_set_pain_sounds(int enabled)
{
    pc_player_pain_sounds_enabled=(enabled!=0);
}

static unsigned host_last_pain_sfx;
void avp_player_clear_last_pain_sfx(void){host_last_pain_sfx=0u;}
unsigned avp_player_last_pain_sfx(void){return host_last_pain_sfx;}
