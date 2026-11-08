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
    char folder[]="/tmp/avp-r71-options-XXXXXX";
    AvpPcVideo *v=calloc(1,sizeof *v),*loaded=calloc(1,sizeof *loaded);
    FILE *f;
    char line[256];
    int wrote=0;
    assert(v && loaded && mkdtemp(folder));
    snprintf(fake_executable,sizeof fake_executable,"%s/avp_pc",folder);
    settings_defaults(&v->cfg);
    make_settings_path(v);
    v->menu_open=1;v->menu_page=MENU_ENHANCEMENTS;
    assert(v->cfg.enemy_activation_distance==0);
    assert(menu_count(v)==15);
    {int n=0;for(int i=0;i<menu_count(v);i++)n+=(menu_id(v,i)==177);assert(n==1);}
    v->menu_index=177;
    for(int i=1;i<=4;i++){menu_adjust(v,1);assert(v->cfg.enemy_activation_distance==i);}
    menu_adjust(v,1);assert(v->cfg.enemy_activation_distance==0);
    menu_adjust(v,-1);assert(v->cfg.enemy_activation_distance==4);
    menu_adjust(v,-1);assert(v->cfg.enemy_activation_distance==3);
    f=fopen(v->settings_path,"r");assert(f);
    while(fgets(line,sizeof line,f))if(!strcmp(line,"enhancement.enemy_activation_distance=3\n"))wrote=1;
    fclose(f);assert(wrote);
    load_settings(loaded);assert(loaded->cfg.enemy_activation_distance==3);
    /* Invalid config must revert to Jaguar default, not create infinite AI. */
    f=fopen(v->settings_path,"w");assert(f);
    fputs("enhancement.enemy_activation_distance=99\n",f);fclose(f);
    load_settings(loaded);assert(loaded->cfg.enemy_activation_distance==0);
    unlink(v->settings_path);rmdir(folder);
    free(loaded);free(v);
    puts("R71 F12 enemy activation 2x/3x/4x/unlimited cycling and persistence PASS");
    return 0;
}
