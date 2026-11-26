/* F12 Cheats > New: independent session-only Invincibility, per human. */
AvpNewHealthMode avp_pc_video_new_health_mode(const AvpPcVideo *v,unsigned player)
{
    if(!v||player>=4u)return AVP_NEW_HEALTH_OFF;
    if(v->cheat_invincibility[player])return AVP_NEW_HEALTH_INVINCIBLE;
    if(player==0u)return v->infinite_health?AVP_NEW_HEALTH_INFINITE:AVP_NEW_HEALTH_OFF;
#ifdef AVP_LOCAL_MULTIPLAYER
    if(v->local_mp_controls.player[player].cheat_infinite_health)return AVP_NEW_HEALTH_INFINITE;
#endif
    return AVP_NEW_HEALTH_OFF;
}
void avp_pc_video_new_health_reset(AvpPcVideo *v)
{
    if(v)memset(v->cheat_invincibility,0,sizeof(v->cheat_invincibility));
}
static int avp_new_invincibility_menu_adjust(AvpPcVideo *v)
{
    if(v->menu_page!=MENU_CHEATS_NEW || v->menu_index!=198)return 0;
    if(v->cheat_target<4u)v->cheat_invincibility[v->cheat_target]^=1;
    return 1;
}
