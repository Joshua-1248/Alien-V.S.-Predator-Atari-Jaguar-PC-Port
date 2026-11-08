/* RE33 R59: real F12 Enhancements toggle and settings.cfg roundtrip. */
#define _DEFAULT_SOURCE 1
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
static char fake_executable[512];
static ssize_t avp_test_readlink(const char *path,char *target,size_t cap)
{
    size_t n=strlen(fake_executable);
    assert(!strcmp(path,"/proc/self/exe"));
    if(n>cap)n=cap;
    memcpy(target,fake_executable,n);
    return (ssize_t)n;
}
#define readlink avp_test_readlink
#include "../src/platform/pc/pc_video_x11.c"
#undef readlink
s16 cur_level=1;
int avp_mm_select(AvpMapMaker *m,unsigned level){(void)m;(void)level;return 1;}
int avp_mm_blank(AvpMapMaker *m){(void)m;return 1;}
int avp_jaguar_framebuffer_init(AvpJaguarFramebuffer *fb,unsigned w,unsigned h)
{(void)fb;(void)w;(void)h;return 1;}
void avp_jaguar_framebuffer_destroy(AvpJaguarFramebuffer *fb){(void)fb;}
void avp_pc_audio_set_speaker_mode(unsigned mode,unsigned sub)
{(void)mode;(void)sub;}
void avp_pc_audio_set_mix(unsigned m,unsigned s,unsigned a,unsigned v,unsigned h,int mo,int ao,int ho)
{(void)m;(void)s;(void)a;(void)v;(void)h;(void)mo;(void)ao;(void)ho;}
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int avp_computer_active(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s){(void)s;return 0;}
int avp_game_micro_opt_available(void){return 0;}
void avp_game_micro_opt_set(int enabled){(void)enabled;}
/* No game world or ROM is needed to exercise the host config/menu. */
void avp_local_mp_defaults(AvpLocalMpSetup *s){memset(s,0,sizeof(*s));}
void avp_local_mp_validate(AvpLocalMpSetup *s){(void)s;}
void avp_mp_input_defaults(AvpMpInputConfig *s){memset(s,0,sizeof(*s));}
void avp_mp_input_validate(AvpMpInputConfig *s){(void)s;}
int avp_mp_input_parse_setting(AvpMpInputConfig *s,const char *a,const char *b)
{(void)s;(void)a;(void)b;return 0;}
void avp_mp_input_write_settings(const AvpMpInputConfig *s,FILE *f)
{(void)s;(void)f;}
unsigned avp_pc_prototype_textures_apply_search(AvpPcPrototypeTextures *s,
    AvpAvpGpuState *g,const char *a,const char *b,const char *c,int d,
    char *e,size_t f)
{(void)s;(void)g;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;return 0;}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));
    assert(v);
    v->menu_open=1;
    v->menu_page=MENU_ROOT;
    assert(menu_count(v)==8);
    v->menu_cursor=5;  /* Debug appears on F12 root. */
    menu_choose(v);
    assert(v->menu_page==MENU_DEBUG);
    assert(menu_count(v)==1);
    assert(!avp_pc_video_take_refresh_level(v));
    menu_choose(v);
    assert(avp_pc_video_take_refresh_level(v));
    assert(!avp_pc_video_take_refresh_level(v));
    v->pending_actions=AVP_PC_ACTION_RESTORE_HEALTH;
    menu_choose(v);
    assert(avp_pc_video_take_refresh_level(v));
    assert(v->pending_actions==AVP_PC_ACTION_RESTORE_HEALTH);
    menu_back(v);
    assert(v->menu_page==MENU_ROOT);
    v->menu_cursor=6; /* Miscellaneous remains a separate root page. */
    menu_choose(v);
    assert(v->menu_page==MENU_MISC);
    v->menu_index=166; /* Existing Return To Title action. */
    menu_adjust(v,1);
    assert(avp_pc_video_take_restart_title(v));
    assert(!avp_pc_video_take_restart_title(v));
    assert(v->menu_open==0);
    v->pending_actions=AVP_PC_ACTION_REFRESH_LEVEL;
    v->pad_buffered=1;v->use_button_buffered=1;
    avp_pc_video_prepare_title_return(v);
    assert(v->title_return_wait_release);
    assert(v->pending_actions==0);
    assert(v->pad_buffered==0 && v->use_button_buffered==0);
    free(v);
    puts("R68 F12 Debug routing and Return To Title state PASS");
    return 0;
}
