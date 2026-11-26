/* Hosted SELECT briefing shortcut. The cartridge's confirmation and weapon
 * code are unchanged; the PC bridge may consume a NEW A/B/C press while
 * presenting the decoded RDB description instead of waiting 450 host fields. */
static int select_brief_skip;
void avp_mjp_select_request_briefing_skip(void){select_brief_skip=1;}
