/* RE47 R342: legacy thermal-sprite test supplies an intentionally fixed
 * avp_jaguar_cry16_to_rgb() fixture. The current texture filter also needs
 * the real Jaguar RGB->CRY qconv conversion. Compile the actual source with
 * just its conflicting RGB fixture entry renamed, not a fake qconv routine.
 * This is TEST-ONLY and never linked into avp_pc. */
#define avp_jaguar_cry16_to_rgb avp_jaguar_cry16_to_rgb_unused_r342
#include "../new/jaguar/hw/cry.c"
