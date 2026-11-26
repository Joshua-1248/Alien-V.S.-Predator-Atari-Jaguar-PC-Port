/* RE39 R189: link-only fixtures for X11 F12 menu tests that compile the
 * real pc_video_x11.c in one translation unit.  None of these unrelated
 * branches is exercised by the controls/settings assertions; keep the
 * test's production menu code intact rather than importing the entire
 * game, GPU renderer, local MP input pipeline, and audio host.
 */
#include "jaguar/framebuffer.h"
#include "pc/pc_prototype_textures.h"
#include "pc/pc_audio.h"
#include "local_mp_input.h"
#include "local_mp_mice.h"
#include "local_mp_gamepads.h"
#include "game_micro_optimization.h"
#include "map_maker.h"
#include "levels.h"
#include <string.h>
#include <stdlib.h>

s16 cur_level=0;
int avp_jaguar_framebuffer_init(AvpJaguarFramebuffer *fb,unsigned width,unsigned height)
{ (void)fb;(void)width;(void)height;return 0; }
void avp_jaguar_framebuffer_destroy(AvpJaguarFramebuffer *fb){(void)fb;}
unsigned avp_pc_prototype_textures_apply_search(AvpPcPrototypeTextures *swap,
 AvpAvpGpuState *state,const char *override_directory,const char *executable_path,
 const char *working_pack_directory,int enabled,char *selected_directory,
 size_t selected_capacity)
{ (void)swap;(void)state;(void)override_directory;(void)executable_path;
  (void)working_pack_directory;(void)enabled;(void)selected_directory;
  (void)selected_capacity;return 0; }
void avp_pc_audio_set_mix(unsigned music,unsigned sfx,unsigned ambience,
 unsigned voice,unsigned hissing,int music_on,int ambience_on,int hissing_on)
{ (void)music;(void)sfx;(void)ambience;(void)voice;(void)hissing;
  (void)music_on;(void)ambience_on;(void)hissing_on; }
void avp_pc_audio_set_speaker_mode(unsigned mode,unsigned subwoofers)
{ (void)mode;(void)subwoofers; }
void avp_mm_init(AvpMapMaker *m){(void)m;}
void avp_mm_destroy(AvpMapMaker *m){(void)m;}
int avp_mm_bind(AvpMapMaker *m,unsigned level,const u8 *decoded,size_t len)
{ (void)m;(void)level;(void)decoded;(void)len;return 0; }
int avp_mm_load(AvpMapMaker *m,const char *path){(void)m;(void)path;return 0;}
int avp_mm_select(AvpMapMaker *m,unsigned level){(void)m;(void)level;return 0;}
int avp_mm_blank(AvpMapMaker *m){(void)m;return 0;}
int avp_game_micro_opt_available(void){return 0;}
void avp_game_micro_opt_set(int enabled){(void)enabled;}
void avp_mp_input_write_settings(const AvpMpInputConfig *cfg,FILE *out)
{ (void)cfg;(void)out; }
void avp_mp_input_validate(AvpMpInputConfig *cfg){(void)cfg;}
int avp_mp_input_cycle_gamepad(AvpMpInputConfig *cfg,unsigned player,
 const char *const *devices,unsigned count,const char *p1_device,int direction)
{ (void)cfg;(void)player;(void)devices;(void)count;(void)p1_device;
  (void)direction;return 0; }
unsigned avp_mp_mice_discover(AvpMpPhysicalMouse out[AVP_MP_DISCOVER_MICE])
{ (void)out;return 0; }
int avp_mp_input_assign_mouse(AvpMpInputConfig *cfg,unsigned player,const char *path)
{ (void)cfg;(void)player;(void)path;return 0; }
int avp_mp_pad_reader_open(AvpMpPadReader *reader,const char *path)
{ (void)reader;(void)path;return -1; }
int avp_mp_pad_reader_poll(AvpMpPadReader *reader,AvpMpControllerSample *out)
{ (void)reader;(void)out;return -1; }
void avp_mp_pad_reader_close(AvpMpPadReader *reader){(void)reader;}
/* The focus tests exercise these trivial shutdown actions. Preserve the
 * actual clear/reset effects, rather than leaving the buffer uncleared. */
void avp_mp_commands_clear(AvpMpCommandInput *input)
{ if(input)memset(input,0,sizeof(*input)); }
void avp_mm_history_commit(AvpMapMaker *m)
{ /* In these tests edit_history is unallocated; production also returns. */
  if(m && m->edit_history){abort();} }
void avp_mm_wall_drag_end(AvpMapMaker *m)
{ if(m){m->wall_drag_lock=0;m->wall_last_valid=0;} }

#include "../new/audio/tests/sound_channels_menu_stub.inc"
