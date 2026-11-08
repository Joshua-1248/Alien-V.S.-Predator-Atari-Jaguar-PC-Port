#ifndef AVP_MP_TRACKER_PEERS_H
#define AVP_MP_TRACKER_PEERS_H

/* Original player scopes/levels, not AMP proxy actors. The same check is
 * used in co-op and Deathmatch. Corpses/death fades, other levels, and the
 * viewer itself never become live tracker or automap contacts. */
static inline int avp_mp_tracker_peer_eligible(unsigned viewer,unsigned peer,
    const void *viewer_world,const void *peer_world,int peer_fading,
    int peer_health,int peer_game_over)
{
    return viewer!=peer && viewer_world && viewer_world==peer_world &&
           !peer_fading && peer_health>0 && !peer_game_over;
}
#endif
