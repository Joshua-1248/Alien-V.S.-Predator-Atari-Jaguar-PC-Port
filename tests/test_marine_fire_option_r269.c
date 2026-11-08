/* R269: real F12 menu and settings.cfg roundtrip for independent enhancement. */
#define _DEFAULT_SOURCE 1
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
void avp_mazescrn_set_facehug_shake_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_facehug_pain_sounds(int enabled){(void)enabled;}
void avp_player_set_pain_sounds(int enabled){(void)enabled;}
int avp_computer_active(void){return 0;}
int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s){(void)s;return 0;}
int main(void)
{
    char dir[]="/tmp/avp-marine-fire-r269-XXXXXX";
    char cfg[512],line[256];FILE*f;int found=0;
    AvpPcVideo *v=calloc(1,sizeof(*v));
    assert(v&&mkdtemp(dir));
    snprintf(fake_executable,sizeof(fake_executable),"%s/executable",dir);
    snprintf(cfg,sizeof(cfg),"%s/settings.cfg",dir);
    load_settings(v);
    assert(!v->cfg.marine_fire_interrupt && !v->cfg.marine_pulse_los_hits);
    assert(AVP_MENU_ITEMS==159);
    v->menu_page=MENU_ENHANCEMENTS;
    assert(menu_id(v,menu_count(v)-2)==157);
    assert(menu_id(v,menu_count(v)-1)==158);
    v->menu_index=158;menu_adjust(v,1);
    assert(v->cfg.marine_fire_interrupt && !v->cfg.marine_pulse_los_hits);
    f=fopen(cfg,"r");assert(f);
    while(fgets(line,sizeof(line),f))
        if(!strcmp(line,"enhancement.marine_fire_interrupt=true\n"))found=1;
    fclose(f);assert(found);
    v->cfg.marine_fire_interrupt=0;
    load_settings(v);
    assert(v->cfg.marine_fire_interrupt && !v->cfg.marine_pulse_los_hits);
    v->menu_index=157;menu_adjust(v,1);
    assert(v->cfg.marine_pulse_los_hits && v->cfg.marine_fire_interrupt);
    v->menu_index=158;menu_adjust(v,1);
    assert(!v->cfg.marine_fire_interrupt && v->cfg.marine_pulse_los_hits);
    v->cfg.marine_fire_interrupt=1;
    load_settings(v);
    assert(!v->cfg.marine_fire_interrupt && v->cfg.marine_pulse_los_hits);
    unlink(cfg);rmdir(dir);free(v);
    puts("R269 F12 Marine Fire Interrupt default, independence and persistence PASS");
    return 0;
}
