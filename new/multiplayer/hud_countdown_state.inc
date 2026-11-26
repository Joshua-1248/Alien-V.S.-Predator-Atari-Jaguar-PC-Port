/* A local co-op world has one 10-Hz countdown regardless of its actors.
 * Native HUD-world scope snapshots these two fields along with the clock. */
static u32 countdown_last_frame=UINT32_MAX;
static u16 countdown_flash_color=0x8880u;
static u32 countdown_last_cue_frame=UINT32_MAX; /* never replay on another world */
static unsigned countdown_coop_mode; /* limit co-op idempotence to the F2 session */
