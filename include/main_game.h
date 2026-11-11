#ifndef AVP_MAIN_GAME_H
#define AVP_MAIN_GAME_H
#include "avp_types.h"
void start(void);void main_start(void);void re_start(void);void reset(void);void stop(void);
void avp_post_game_frontend(void);
/* Host multiplayer outcome: reuse original ending without Fame/EEPROM insertion. */
void avp_post_game_frontend_multiplayer(void);
/* PC cold-boot music startup runs while the first title screen is black.
 * Original MAIN.S still starts music normally on every later NewTitle. */
#if defined(AVP_PC_HOST) || defined(AVP_PC_REFERENCE_TITLE_TIMING)
void avp_pc_title_music_prestart(void);
#endif
void Version(void);void NewTitle(void);void Title(void);void SetupGpu(void);void SetFixGpu(void);void SetGpuBin(void);void ClearStack(void);
extern u8 in_title,in_select,allow_reset;
#endif
