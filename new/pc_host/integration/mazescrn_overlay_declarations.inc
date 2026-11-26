typedef struct AvpFirstPersonOverlayState {
    s16 overlay_id;
    u8 overlay_visible;
    s16 use1_id;
    s16 use2_id;
    u8 use1_visible;
    u8 use2_visible;
    u8 swing_enabled;
    s32 x,y;
    s16 swing_x,swing_y;
    /* Marine/Alien/Predator Combi-Stick first move_in pose, read-only. */
    u8 intro_preview;
    s32 intro_x,intro_y;
} AvpFirstPersonOverlayState;
void avp_mazescrn_get_overlay_state(AvpFirstPersonOverlayState *out);

