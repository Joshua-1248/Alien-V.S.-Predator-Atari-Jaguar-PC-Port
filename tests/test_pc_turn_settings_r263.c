/* R263: loading malformed PC control settings must not poison the
 * host-clock angle accumulator or the original 10 Hz turn handoff. */
#define _DEFAULT_SOURCE 1
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static char fake_executable[512];
static ssize_t avp_test_readlink(const char *path,char *target,size_t count)
{
    size_t n=strlen(fake_executable);
    assert(!strcmp(path,"/proc/self/exe"));
    if(n>count)n=count;
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

static void write_and_load(AvpPcVideo *v,const char *turn,const char *speed,const char *deadzone)
{
    char path[512];FILE *f;
    snprintf(path,sizeof(path),"%s",fake_executable);
    char *slash=strrchr(path,'/');assert(slash);strcpy(slash+1,"settings.cfg");
    f=fopen(path,"w");assert(f);
    fprintf(f,"controls.turn_sensitivity=%s\ncontrols.turn_speed=%s\ngamepad.deadzone=%s\n",turn,speed,deadzone);
    fclose(f);
    load_settings(v);
    assert(!strcmp(v->settings_path,path));
}
int main(void)
{
    char dir[]="/tmp/avp-turn-settings-r263-XXXXXX";
    char cfg[512]; AvpPcVideo *v=calloc(1,sizeof(*v));
    assert(v&&mkdtemp(dir));
    snprintf(fake_executable,sizeof(fake_executable),"%s/avp-test-binary",dir);
    snprintf(cfg,sizeof(cfg),"%s/settings.cfg",dir);
    write_and_load(v,"nan","inf","nan");
    assert(isfinite(v->cfg.turn_sensitivity)&&v->cfg.turn_sensitivity==1.0f);
    assert(isfinite(v->cfg.turn_speed)&&v->cfg.turn_speed==1.0f);
    assert(isfinite(v->cfg.deadzone)&&v->cfg.deadzone==0.20f);
    for(int i=0;i<AVP_JS_MAX_AXES;i++)assert(isfinite(v->cfg.axis_deadzone[i]));
    write_and_load(v,"-inf","nan","inf");
    assert(v->cfg.turn_sensitivity==1.0f&&v->cfg.turn_speed==1.0f);
    assert(v->cfg.deadzone==0.20f);
    write_and_load(v,"0.125","999","-1");
    assert(v->cfg.turn_sensitivity==0.25f&&v->cfg.turn_speed==5.0f);
    assert(v->cfg.deadzone==0.0f);
    write_and_load(v,"1.75","3.25","0.30");
    assert(v->cfg.turn_sensitivity==1.75f&&v->cfg.turn_speed==3.25f);
    assert(v->cfg.deadzone==0.30f);
    unlink(cfg);rmdir(dir);free(v);
    puts("R263 finite joystick settings and bounds PASS");return 0;
}
