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
    char dir[]="/tmp/avp-diagonal-contact-r59-XXXXXX", cfg[512], line[256];
    AvpPcVideo *v=calloc(1,sizeof(*v));
    FILE *f;int found=0;
    assert(v&&mkdtemp(dir));
    snprintf(fake_executable,sizeof(fake_executable),"%s/avp_pc",dir);
    snprintf(cfg,sizeof(cfg),"%s/settings.cfg",dir);
    load_settings(v);
    assert(!v->cfg.normalize_diagonal_contact_range);
    assert(!v->cfg.normalize_diagonal_alien_death_spray_range);
    assert(!v->cfg.normalize_diagonal_enemy_movement_speed);
    assert(avp_pc_video_normalize_diagonal_enemy_movement_speed(v)==0);
    assert(avp_pc_video_normalize_diagonal_contact_range(v)==0);
    assert(avp_pc_video_normalize_diagonal_alien_death_spray_range(v)==0);
    v->menu_page=MENU_ENHANCEMENTS;
    { int contact_seen=0,spray_seen=0,movement_seen=0;
      for(int i=0;i<menu_count(v);i++){
        if(menu_id(v,i)==174)contact_seen++;
        if(menu_id(v,i)==190)spray_seen++;
        if(menu_id(v,i)==191)movement_seen++;
      }
      assert(contact_seen==1&&spray_seen==1&&movement_seen==1);
    }
    /* Existing R59 setting remains an independent toggle. */
    v->menu_index=174;
    menu_adjust(v,1);
    assert(avp_pc_video_normalize_diagonal_contact_range(v)==1);
    assert(avp_pc_video_normalize_diagonal_alien_death_spray_range(v)==0);
    f=fopen(cfg,"r");assert(f);
    while(fgets(line,sizeof(line),f))
        if(!strcmp(line,"enhancement.normalize_diagonal_contact_range=true\n"))found=1;
    fclose(f);assert(found);
    /* New R294 option is separately persisted and has no contact side effects. */
    v->menu_index=190;
    menu_adjust(v,1);
    assert(avp_pc_video_normalize_diagonal_contact_range(v)==1);
    assert(avp_pc_video_normalize_diagonal_alien_death_spray_range(v)==1);
    f=fopen(cfg,"r");assert(f);found=0;
    while(fgets(line,sizeof(line),f))
        if(!strcmp(line,"enhancement.normalize_diagonal_alien_death_spray_range=true\n"))found=1;
    fclose(f);assert(found);
    /* R297 movement setting has its own saved flag and independent toggle. */
    v->menu_index=191;
    menu_adjust(v,1);
    assert(avp_pc_video_normalize_diagonal_enemy_movement_speed(v)==1);
    f=fopen(cfg,"r");assert(f);found=0;
    while(fgets(line,sizeof(line),f))
        if(!strcmp(line,"enhancement.normalize_diagonal_enemy_movement_speed=true\n"))found=1;
    fclose(f);assert(found);
    v->cfg.normalize_diagonal_contact_range=0;
    v->cfg.normalize_diagonal_alien_death_spray_range=0;
    v->cfg.normalize_diagonal_enemy_movement_speed=0;
    load_settings(v);
    assert(v->cfg.normalize_diagonal_contact_range);
    assert(v->cfg.normalize_diagonal_alien_death_spray_range);
    assert(v->cfg.normalize_diagonal_enemy_movement_speed);
    v->menu_index=190;
    menu_adjust(v,-1);
    assert(!v->cfg.normalize_diagonal_alien_death_spray_range);
    assert(v->cfg.normalize_diagonal_contact_range);
    assert(v->cfg.normalize_diagonal_enemy_movement_speed);
    v->menu_index=191;
    menu_adjust(v,-1);
    assert(!v->cfg.normalize_diagonal_enemy_movement_speed);
    v->menu_index=174;
    menu_adjust(v,-1);
    assert(!v->cfg.normalize_diagonal_contact_range);
    v->cfg.normalize_diagonal_contact_range=1;
    v->cfg.normalize_diagonal_alien_death_spray_range=1;
    v->cfg.normalize_diagonal_enemy_movement_speed=1;
    load_settings(v);
    assert(!v->cfg.normalize_diagonal_contact_range);
    assert(!v->cfg.normalize_diagonal_alien_death_spray_range);
    assert(!v->cfg.normalize_diagonal_enemy_movement_speed);
    unlink(cfg);rmdir(dir);free(v);
    puts("R59 F12 Normalize Diagonal Contact Range default + persistence PASS");
    return 0;
}
