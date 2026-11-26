#ifdef AVP_LOCAL_MULTIPLAYER_WEAPON_BRIDGE
/* Optional native private-state seam, with no change to PredAvail(). */
unsigned avp_mp_weapons_native_get_index(void) { return pred_index; }
int avp_mp_weapons_native_set_index(unsigned index)
{
    if (index>=sizeof(awards)/sizeof(awards[0])) return 0;
    pred_index=index;
    return 1;
}
#endif
