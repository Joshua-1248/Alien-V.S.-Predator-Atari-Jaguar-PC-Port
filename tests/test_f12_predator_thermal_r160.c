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
#include "../new/pc_host/src/pc_video_x11.c"
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
    char folder[]="/tmp/avp-r160-thermal-XXXXXX";
    AvpPcVideo *v=calloc(1,sizeof *v),*loaded=calloc(1,sizeof *loaded);
    char line[256]; FILE *f;int wrote=0;
    assert(v && loaded && mkdtemp(folder));
    snprintf(fake_executable,sizeof fake_executable,"%s/avp_pc",folder);
    settings_defaults(&v->cfg);make_settings_path(v);
    v->menu_page=MENU_ENHANCEMENTS;v->menu_open=1;
    assert(!v->cfg.enhanced_predator_thermal);
    /* The page acquired additional enhancements after R223. Validate
     * individual IDs and persistence below, not a stale absolute row count. */
    assert(menu_count(v)>9);
    /* R223 extends (not renumbers) the legacy enhancements page. */
    {int found=0;for(int row=0;row<menu_count(v);row++)
        found+=(menu_id(v,row)==188);
     assert(found==1);}
    assert(!v->cfg.fill_automap_hatch_gaps); /* Jaguar default */
    v->menu_index=188;
    menu_adjust(v,1);
    load_settings(loaded);
    assert(v->cfg.fill_automap_hatch_gaps && loaded->cfg.fill_automap_hatch_gaps);
    menu_adjust(v,-1);
    load_settings(loaded);
    assert(!v->cfg.fill_automap_hatch_gaps && !loaded->cfg.fill_automap_hatch_gaps);
    {int count=0;for(int i=0;i<menu_count(v);i++)
        count+=(menu_id(v,i)==178 || menu_id(v,i)==179 || menu_id(v,i)==180 || menu_id(v,i)==181 || menu_id(v,i)==182 || menu_id(v,i)==183 || menu_id(v,i)==184 || menu_id(v,i)==185 || menu_id(v,i)==186);
     assert(count==9);}
    v->menu_index=178;
    menu_adjust(v,1);
    assert(v->cfg.enhanced_predator_electromagnetic);
    assert(!v->cfg.enhanced_predator_thermal);
    f=fopen(v->settings_path,"r");assert(f);
    while(fgets(line,sizeof line,f))
        if(!strcmp(line,"enhancement.enhanced_predator_electromagnetic_vision=true\n"))wrote=1;
    fclose(f);assert(wrote);
    load_settings(loaded);assert(loaded->cfg.enhanced_predator_electromagnetic);
    menu_adjust(v,-1);assert(!v->cfg.enhanced_predator_electromagnetic);
    v->menu_index=179;menu_adjust(v,1);assert(v->cfg.enhanced_predator_thermal);
    load_settings(loaded);assert(loaded->cfg.enhanced_predator_thermal);
    assert(!loaded->cfg.enhanced_predator_electromagnetic);
    v->menu_index=179;menu_adjust(v,-1);
    load_settings(loaded);assert(!loaded->cfg.enhanced_predator_thermal);
    /* Old R160 key migrates to Electromagnetic; it must not silently enable Thermal. */
    f=fopen(v->settings_path,"w");assert(f);
    fputs("enhancement.enhanced_predator_thermal_vision=true\n",f);fclose(f);
    load_settings(loaded);assert(loaded->cfg.enhanced_predator_electromagnetic);
    assert(!loaded->cfg.enhanced_predator_thermal);
    /* The newly added Infrared setting has its own saved key and does not
     * toggle existing Thermal/Electromagnetic options. */
    assert(!v->cfg.enhanced_predator_infrared);
    v->menu_index=180;menu_adjust(v,1);
    assert(v->cfg.enhanced_predator_infrared);
    load_settings(loaded);
    assert(loaded->cfg.enhanced_predator_infrared);
    assert(!loaded->cfg.enhanced_predator_thermal);
    assert(!loaded->cfg.enhanced_predator_electromagnetic);
    v->menu_index=180;menu_adjust(v,-1);
    load_settings(loaded);
    assert(!loaded->cfg.enhanced_predator_infrared);
    /* An intermediate R161 legacy key still belongs to Electromagnetic.
     * Do not migrate it to the genuinely NEW Infrared toggle. */
    f=fopen(v->settings_path,"w");assert(f);
    fputs("enhancement.enhanced_predator_infrared_vision=true\n",f);fclose(f);
    load_settings(loaded);
    assert(loaded->cfg.enhanced_predator_electromagnetic);
    assert(!loaded->cfg.enhanced_predator_infrared);
    /* R166: the scenery toggle is independent of all three character toggles. */
    assert(!v->cfg.enhanced_predator_background);
    v->menu_index=181;menu_adjust(v,1);
    assert(v->cfg.enhanced_predator_background);
    load_settings(loaded);
    assert(loaded->cfg.enhanced_predator_background);
    assert(!loaded->cfg.enhanced_predator_thermal);
    assert(!loaded->cfg.enhanced_predator_infrared);
    v->menu_index=181;menu_adjust(v,-1);
    load_settings(loaded);
    assert(!loaded->cfg.enhanced_predator_background);
    assert(!v->cfg.predator_cloaked_kills_no_honor_loss);
    assert(!v->cfg.predator_cloaked_kills_half_value);
    v->menu_index=182;menu_adjust(v,1);
    load_settings(loaded);
    assert(loaded->cfg.predator_cloaked_kills_no_honor_loss);
    assert(!loaded->cfg.predator_cloaked_kills_half_value);
    v->menu_index=183;menu_adjust(v,1);
    load_settings(loaded);
    assert(loaded->cfg.predator_cloaked_kills_no_honor_loss);
    assert(loaded->cfg.predator_cloaked_kills_half_value);
    v->menu_index=182;menu_adjust(v,-1);
    load_settings(loaded);
    assert(!loaded->cfg.predator_cloaked_kills_no_honor_loss);
    assert(loaded->cfg.predator_cloaked_kills_half_value);
    v->menu_index=183;menu_adjust(v,-1);
    load_settings(loaded);
    assert(!loaded->cfg.predator_cloaked_kills_no_honor_loss);
    assert(!loaded->cfg.predator_cloaked_kills_half_value);
    /* R193: added Predator jammed-door enhancement appears once in the
     * existing Enhancements list and saves independently of honor toggles. */
    assert(!v->cfg.predator_attack_opens_jammed_doors);
    v->menu_index=184;menu_adjust(v,1);
    load_settings(loaded);
    assert(loaded->cfg.predator_attack_opens_jammed_doors);
    assert(!loaded->cfg.predator_cloaked_kills_no_honor_loss);
    assert(!loaded->cfg.predator_cloaked_kills_half_value);
    v->menu_index=184;menu_adjust(v,-1);
    load_settings(loaded);
    assert(!loaded->cfg.predator_attack_opens_jammed_doors);
    /* R196: two independent enhancements default off and persist.
     * Melee-only does not imply ranged; all-attacks overrides melee-only. */
    assert(!v->cfg.player_melee_center_aim && !v->cfg.player_all_center_aim);
    assert(menu_count(v)>9); /* other enhancements may be added later */
    v->menu_index=185;menu_adjust(v,1);
    load_settings(loaded);
    assert(loaded->cfg.player_melee_center_aim && !loaded->cfg.player_all_center_aim);
    v->menu_index=186;menu_adjust(v,1);
    load_settings(loaded);
    assert(loaded->cfg.player_melee_center_aim && loaded->cfg.player_all_center_aim);
    v->menu_index=185;menu_adjust(v,1);
    load_settings(loaded);
    assert(!loaded->cfg.player_melee_center_aim && loaded->cfg.player_all_center_aim);
    v->menu_index=186;menu_adjust(v,1);
    load_settings(loaded);
    assert(!loaded->cfg.player_melee_center_aim && !loaded->cfg.player_all_center_aim);
    unlink(v->settings_path);rmdir(folder);
    free(loaded);free(v);
    puts("R161 F12 Electromagnetic/Thermal independent toggles, legacy migration PASS");
    return 0;
}

#include "../new/audio/tests/sound_channels_menu_stub.inc"
