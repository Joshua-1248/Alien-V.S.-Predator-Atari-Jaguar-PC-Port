#include "local_mp_input.h"
#include "joypad.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef _WIN32
#include <sys/stat.h>
#endif

static const char *const action_names[AVP_MP_INPUT_ACTIONS] = {
    "Forward", "Backward", "Turn Left", "Turn Right", "Strafe Left", "Strafe Right",
    "Fire", "Use", "Map", "Previous Weapon", "Next Weapon", "Alien Claw",
    "Alien Bite", "Alien Tail", "Predator Medkit", "Predator Cloak", "Predator Vision",
    "Quick Turn", "Pause", "Option", "Terminal Next", "Terminal Previous",
    "Map Zoom In", "Map Zoom Out", "Terminal Exit"
};

/* The original terminal is A/NEXT, B/SELECT/PREVIOUS and C/EXIT.
 * P2-P4 have independent mapped buttons; preserve all Jaguar menu rules.
 * Do not synthesize a key edge here: the original readpad() owns edges. */
int avp_mp_input_weapon_cycle_edge(uint32_t actions,uint32_t *prior_buttons)
{
    const uint32_t mask=(1u<<AVP_MP_PREV_WEAPON)|(1u<<AVP_MP_NEXT_WEAPON);
    uint32_t held,edge;
    if(!prior_buttons)return 0;
    held=actions&mask;
    edge=held&~*prior_buttons;
    *prior_buttons=held;
    return (edge&(1u<<AVP_MP_NEXT_WEAPON))?1:
           (edge&(1u<<AVP_MP_PREV_WEAPON))?-1:0;
}

uint32_t avp_mp_terminal_actions_pad(uint32_t actions)
{
    uint32_t pad=UINT32_MAX;
    if(actions&(1u<<AVP_MP_FORWARD))pad&=~(1u<<JOY_UP);
    if(actions&(1u<<AVP_MP_BACKWARD))pad&=~(1u<<JOY_DOWN);
    if(actions&((1u<<AVP_MP_TERMINAL_NEXT)|(1u<<AVP_MP_FIRE)))pad&=~(1u<<FIRE_A);
    if(actions&((1u<<AVP_MP_TERMINAL_PREVIOUS)|(1u<<AVP_MP_USE)))pad&=~(1u<<FIRE_B);
    if(actions&((1u<<AVP_MP_TERMINAL_EXIT)|(1u<<AVP_MP_OPTION)))pad&=~(1u<<FIRE_C);
    return pad;
}

uint32_t avp_mp_terminal_initial_use_guard(uint32_t pad,int *waiting_for_release)
{
    if(waiting_for_release && *waiting_for_release){
        if(pad&(1u<<FIRE_B))*waiting_for_release=0;
        pad|=1u<<FIRE_B;
    }
    return pad;
}

const char *avp_mp_input_action_name(unsigned action)
{ return action < AVP_MP_INPUT_ACTIONS ? action_names[action] : "Invalid"; }

static int same_mouse(const char *a,const char *b);

void avp_mp_input_defaults(AvpMpInputConfig *cfg)
{
    unsigned p, a;
    if(!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    for(p=0;p<AVP_MP_INPUT_PLAYERS;p++) {
        AvpMpInputProfile *v=&cfg->player[p];
        for(a=0;a<AVP_MP_INPUT_ACTIONS;a++) {
            v->mouse_button[a]=-1; v->gamepad_button[a]=-1;
        }
        /* Match P1's modern controller defaults once the secondary player
         * explicitly chooses their own physical gamepad. No implicit pad
         * ownership or keyboard/gamepad action binding is created. */
        v->gamepad_axis[0]=1; /* forward/back (physical Y) */
        v->gamepad_axis[1]=0; /* strafe (physical X) */
        v->gamepad_axis[2]=2; /* turn (right stick X) */
        v->gamepad_axis_invert[0]=1;
        v->gamepad_deadzone=0.20f;
        v->gamepad_analog_movement=0;
        v->gamepad_enhanced_analog_controls=0;
        v->gamepad_turn_sensitivity=1.0f;
        v->gamepad_turn_speed=1.0f;
        v->mouse_enabled=1; v->gamepad_enabled=1;
        v->audio_motion_tracker=1;v->audio_player_pain=1;
        v->audio_facehug_pain=1;v->audio_facehug_shake=1;
        v->remember_automap=0; /* original level-change behavior by default */
        v->mouse_sensitivity=1.0f;
        /* No default keyboard/gamepad bindings for secondary players:
         * avoid claiming input that still belongs to Player 1. */
    }
    /* P1 host-only automap zoom works in both single-player and co-op.
     * X11 keysyms: XK_equal ('=' / '+') and XK_minus ('-' / '_').
     * Never turn these into original Jaguar keypad or player actions. */
    cfg->player[0].key[AVP_MP_MAP_ZOOM_IN]=0x003dul;
    cfg->player[0].key[AVP_MP_MAP_ZOOM_OUT]=0x002dul;
}
void avp_mp_input_validate(AvpMpInputConfig *cfg)
{
    unsigned p,a;
    if(!cfg) return;
    for(p=0;p<AVP_MP_INPUT_PLAYERS;p++) {
        AvpMpInputProfile *v=&cfg->player[p];
        for(a=0;a<3;a++){
            if(v->gamepad_axis[a]<-1||v->gamepad_axis[a]>15)v->gamepad_axis[a]=-1;
            v->gamepad_axis_invert[a]=!!v->gamepad_axis_invert[a];
        }
        if(!isfinite(v->gamepad_deadzone)||v->gamepad_deadzone<0.0f)v->gamepad_deadzone=0.0f;
        if(v->gamepad_deadzone>0.95f)v->gamepad_deadzone=0.95f;
        v->cheat_infinite_health=!!v->cheat_infinite_health;
        v->cheat_infinite_ammo=!!v->cheat_infinite_ammo;
        v->cheat_tracker=!!v->cheat_tracker;
        v->cheat_noclip=!!v->cheat_noclip;
        v->cheat_no_facehug=!!v->cheat_no_facehug;
        v->cheat_no_enemy_attack=!!v->cheat_no_enemy_attack;
        v->audio_motion_tracker=!!v->audio_motion_tracker;
        v->audio_player_pain=!!v->audio_player_pain;
        v->audio_facehug_pain=!!v->audio_facehug_pain;
        v->audio_facehug_shake=!!v->audio_facehug_shake;
        v->remember_automap=!!v->remember_automap;
        v->cheat_reveal_map=!!v->cheat_reveal_map;
        v->gamepad_analog_movement=!!v->gamepad_analog_movement;
        v->gamepad_enhanced_analog_controls=!!v->gamepad_enhanced_analog_controls;
        if(!isfinite(v->gamepad_turn_sensitivity))v->gamepad_turn_sensitivity=1.0f;
        if(v->gamepad_turn_sensitivity<0.25f)v->gamepad_turn_sensitivity=0.25f;
        if(v->gamepad_turn_sensitivity>5.0f)v->gamepad_turn_sensitivity=5.0f;
        if(!isfinite(v->gamepad_turn_speed))v->gamepad_turn_speed=1.0f;
        if(v->gamepad_turn_speed<0.25f)v->gamepad_turn_speed=0.25f;
        if(v->gamepad_turn_speed>5.0f)v->gamepad_turn_speed=5.0f;
        v->mouse_path[sizeof(v->mouse_path)-1]='\0';
        v->gamepad_path[sizeof(v->gamepad_path)-1]='\0';
        v->mouse_enabled=!!v->mouse_enabled;v->gamepad_enabled=!!v->gamepad_enabled;
        if(!isfinite(v->mouse_sensitivity) || v->mouse_sensitivity<0.1f)v->mouse_sensitivity=0.1f;
        if(v->mouse_sensitivity>5.0f)v->mouse_sensitivity=5.0f;
        if(!isfinite(v->mouse_acceleration) || v->mouse_acceleration<0.0f)v->mouse_acceleration=0.0f;
        if(v->mouse_acceleration>3.0f)v->mouse_acceleration=3.0f;
        for(a=0;a<AVP_MP_INPUT_ACTIONS;a++){
            if(v->mouse_button[a]<1||v->mouse_button[a]>15)v->mouse_button[a]=-1;
            if(v->gamepad_button[a]<-33||v->gamepad_button[a]>=32)v->gamepad_button[a]=-1;
        }
    }
    /* Never allow two players to claim the same physical input device,
     * including Linux /dev/input symlink aliases for one event node. */
    for(p=1;p<AVP_MP_INPUT_PLAYERS;p++) {
        unsigned q;
        if(cfg->player[p].mouse_path[0])
            for(q=0;q<p;q++)
                if(cfg->player[q].mouse_path[0] &&
                   same_mouse(cfg->player[q].mouse_path,cfg->player[p].mouse_path)){
                    cfg->player[p].mouse_path[0]='\0';break;
                }
        if(cfg->player[p].gamepad_path[0])
            for(q=0;q<p;q++)
                if(cfg->player[q].gamepad_path[0] &&
                   same_mouse(cfg->player[q].gamepad_path,cfg->player[p].gamepad_path)){
                    cfg->player[p].gamepad_path[0]='\0';break;
                }
    }
}

/* Require complete, range-checked integers, and do not accept trailing junk. */
static int number(const char *s, long *out)
{
    char *end;long n;
    if(!s || !*s || *s==' '||*s=='\t')return 0;
    errno=0;n=strtol(s,&end,0);
    if(errno || end==s || *end)return 0;
    *out=n;return 1;
}
static int decimal_suffix(const char *s,unsigned *idx)
{
    unsigned long n=0;
    if(!s || !*s)return 0;
    while(*s){if(*s<'0'||*s>'9')return 0;n=n*10u+(unsigned)(*s-'0');if(n>=AVP_MP_INPUT_ACTIONS)return 0;s++;}
    *idx=(unsigned)n;return 1;
}
int avp_mp_input_mouse_owner(const AvpMpInputConfig *cfg,const char *path)
{
    unsigned p;
    if(!cfg||!path||!*path)return -1;
    for(p=0;p<AVP_MP_INPUT_PLAYERS;p++)
        if(!strcmp(cfg->player[p].mouse_path,path))return (int)p;
    return -1;
}
static int same_mouse(const char *a,const char *b)
{
    if(!strcmp(a,b))return 1;
#ifndef _WIN32
    {struct stat sa,sb;
     if(!stat(a,&sa)&&!stat(b,&sb) && sa.st_dev==sb.st_dev && sa.st_ino==sb.st_ino)return 1;
    }
#endif
    return 0;
}
int avp_mp_input_assign_mouse(AvpMpInputConfig *cfg,unsigned player,const char *path)
{
    unsigned p;
    size_t len;
    if(!cfg||player>=AVP_MP_INPUT_PLAYERS||!path)return 0;
    len=strlen(path);
    if(len>=AVP_MP_DEVICE_PATH || strchr(path,'\n') || strchr(path,'\r'))return 0;
    for(p=0;p<AVP_MP_INPUT_PLAYERS;p++)
        if(p!=player && len && cfg->player[p].mouse_path[0] && same_mouse(cfg->player[p].mouse_path,path))return 0;
    memcpy(cfg->player[player].mouse_path,path,len+1);return 1;
}
/* Return one step in the ordered unique-device ring. Unassigned is always
 * reachable, including when all discovered pads are claimed already. Do not
 * let validation erase a rejected choice: skip it BEFORE committing it. */
int avp_mp_input_cycle_gamepad(AvpMpInputConfig *cfg,unsigned player,
                               const char *const *devices,unsigned count,
                               const char *p1_device,int direction)
{
    unsigned i,step,slots;
    unsigned current;
    const char *assigned;
    if(!cfg || (count && !devices) || player==0 || player>=AVP_MP_INPUT_PLAYERS ||
       count>64u || direction==0)return 0;
    assigned=cfg->player[player].gamepad_path;
    current=count; /* explicit unassigned state at end of device ring */
    for(i=0;i<count;i++)
        if(devices[i] && !strcmp(assigned,devices[i])){current=i;break;}
    slots=count+1u;
    for(step=1;step<=slots;step++){
        unsigned next=(current + (direction>0?step:slots-(step%slots)))%slots;
        const char *candidate=next<count?devices[next]:"";
        int taken=0;
        size_t len;
        if(next==count){
            cfg->player[player].gamepad_path[0]='\0';
            return 1;
        }
        if(!candidate || !(len=strlen(candidate)) || len>=AVP_MP_DEVICE_PATH ||
           strchr(candidate,'\n') || strchr(candidate,'\r'))continue;
        if(p1_device && *p1_device && same_mouse(candidate,p1_device))continue;
        for(unsigned q=1;q<AVP_MP_INPUT_PLAYERS;q++){
            const char *other=cfg->player[q].gamepad_path;
            if(q!=player && *other && same_mouse(candidate,other)){
                taken=1;break;
            }
        }
        if(taken)continue;
        memcpy(cfg->player[player].gamepad_path,candidate,len+1);
        return 1;
    }
    return 0;
}

int avp_mp_input_parse_setting(AvpMpInputConfig *cfg,const char *key,const char *value)
{
    static const char prefix[]="local_multiplayer.input.p";
    const char *s,*f;unsigned p,a;long n;AvpMpInputProfile *profile;
    if(!cfg||!key||!value||strncmp(key,prefix,sizeof(prefix)-1))return 0;
    s=key+sizeof(prefix)-1;
    if(s[0]<'1'||s[0]>'4'||s[1]!='.')return 0;
    p=(unsigned)(s[0]-'1');f=s+2;profile=&cfg->player[p];
    /* Accept ONLY complete boolean values. One player's configuration can
     * never modify any other player's state, even on malformed input. */
    {
        static const char *const names[]={
            "cheat_infinite_health","cheat_infinite_ammo","cheat_tracker","cheat_noclip",
            "audio_motion_tracker","audio_player_pain","audio_facehug_pain","audio_facehug_shake",
            "remember_automap","cheat_no_facehug","cheat_no_enemy_attack","cheat_reveal_map"};
        int *const values[]={&profile->cheat_infinite_health,&profile->cheat_infinite_ammo,
            &profile->cheat_tracker,&profile->cheat_noclip,&profile->audio_motion_tracker,
            &profile->audio_player_pain,&profile->audio_facehug_pain,
            &profile->audio_facehug_shake,&profile->remember_automap,
            &profile->cheat_no_facehug,&profile->cheat_no_enemy_attack,&profile->cheat_reveal_map};
        for(unsigned i=0;i<sizeof(names)/sizeof(names[0]);i++){
            if(strcmp(f,names[i]))continue;
            if(!number(value,&n)||(n!=0&&n!=1))return 0;
            *values[i]=(int)n;return 1;
        }
    }
    if(!strcmp(f,"mouse_device"))return avp_mp_input_assign_mouse(cfg,p,value);
    if(!strcmp(f,"gamepad_device")){
        size_t len=strlen(value);
        if(len>=sizeof(profile->gamepad_path)||strchr(value,'\n')||strchr(value,'\r'))return 0;
        memcpy(profile->gamepad_path,value,len+1);return 1;
    }
    if(!strcmp(f,"gamepad_analog_movement") || !strcmp(f,"gamepad_enhanced_analog_controls")){
        if(!number(value,&n)||(n!=0&&n!=1))return 0;
        if(!strcmp(f,"gamepad_analog_movement"))profile->gamepad_analog_movement=(int)n;
        else profile->gamepad_enhanced_analog_controls=(int)n;
        return 1;
    }
    if(!strcmp(f,"gamepad_turn_sensitivity") || !strcmp(f,"gamepad_turn_speed")){
        char *end;float v;errno=0;v=strtof(value,&end);
        if(errno||end==value||*end||!isfinite(v)||v<0.25f||v>5.0f)return 0;
        if(!strcmp(f,"gamepad_turn_sensitivity"))profile->gamepad_turn_sensitivity=v;
        else profile->gamepad_turn_speed=v;
        return 1;
    }
    if(!strcmp(f,"gamepad_deadzone")){
        char *end;float v;errno=0;v=strtof(value,&end);
        if(errno||end==value||*end||!isfinite(v)||v<0||v>0.95f)return 0;
        profile->gamepad_deadzone=v;return 1;
    }
    if(!strncmp(f,"gamepad_axis_",13) && f[13]>='0' && f[13]<='2'){
        if(f[14]=='\0'){
            if(!number(value,&n)||(n<-1||n>15))return 0;
            profile->gamepad_axis[f[13]-'0']=(int)n;return 1;
        }
        if(!strcmp(f+14,"_invert")){
            if(!number(value,&n)||(n!=0&&n!=1))return 0;
            profile->gamepad_axis_invert[f[13]-'0']=(int)n;return 1;
        }
    }
    if(!strcmp(f,"mouse_sensitivity")||!strcmp(f,"mouse_acceleration")){
        char *end;float v;errno=0;v=strtof(value,&end);
        if(errno||end==value||*end||!isfinite(v))return 0;
        if(!strcmp(f,"mouse_sensitivity"))profile->mouse_sensitivity=v;
        else profile->mouse_acceleration=v;
        return 1;
    }
    if(!strcmp(f,"mouse_enabled")||!strcmp(f,"gamepad_enabled")){
        if(!number(value,&n)||(n!=0&&n!=1))return 0;
        if(!strcmp(f,"mouse_enabled"))profile->mouse_enabled=(int)n;
        else profile->gamepad_enabled=(int)n;
        return 1;
    }
    if(!strncmp(f,"key_",4)){
        char *end;unsigned long k;
        if(!decimal_suffix(f+4,&a)||!*value||*value=='-')return 0;
        errno=0;k=strtoul(value,&end,0);
        if(errno||end==value||*end||k>UINT32_MAX)return 0;
        profile->key[a]=k;return 1;
    }
    if(!strncmp(f,"mouse_button_",13)){
        if(!decimal_suffix(f+13,&a)||!number(value,&n)||(n!=-1&&(n<1||n>15)))return 0;
        profile->mouse_button[a]=(int)n;return 1;
    }
    if(!strncmp(f,"gamepad_button_",15)){
        if(!decimal_suffix(f+15,&a)||!number(value,&n)||(n< -33||n>31))return 0;
        profile->gamepad_button[a]=(int)n;return 1;
    }
    return 0;
}
void avp_mp_input_write_settings(const AvpMpInputConfig *cfg,FILE *out)
{
    unsigned p,a;
    if(!cfg||!out)return;
    fputs("# Isolated co-op profiles: p1 keys/buttons remain bound to legacy controls.\n",out);
    fprintf(out,"local_multiplayer.input.p1.mouse_device=%s\n",cfg->player[0].mouse_path);
    /* P1 uses the original joystick for these two co-op-only bindings;
     * persist them without claiming the legacy gamepad a second time. */
    fprintf(out,"local_multiplayer.input.p1.gamepad_button_%u=%d\n",
            (unsigned)AVP_MP_MAP_ZOOM_IN,cfg->player[0].gamepad_button[AVP_MP_MAP_ZOOM_IN]);
    fprintf(out,"local_multiplayer.input.p1.gamepad_button_%u=%d\n",
            (unsigned)AVP_MP_MAP_ZOOM_OUT,cfg->player[0].gamepad_button[AVP_MP_MAP_ZOOM_OUT]);
    fprintf(out,"local_multiplayer.input.p1.key_%u=0x%lx\n",
            (unsigned)AVP_MP_MAP_ZOOM_IN,cfg->player[0].key[AVP_MP_MAP_ZOOM_IN]);
    fprintf(out,"local_multiplayer.input.p1.key_%u=0x%lx\n",
            (unsigned)AVP_MP_MAP_ZOOM_OUT,cfg->player[0].key[AVP_MP_MAP_ZOOM_OUT]);
    /* Persist the four independent profiles; P1 gameplay still uses legacy
     * single-player settings as authoritative values. */
    for(p=0;p<AVP_MP_INPUT_PLAYERS;p++){
        const AvpMpInputProfile *v=&cfg->player[p];
        fprintf(out,"local_multiplayer.input.p%u.cheat_infinite_health=%d\n",p+1,v->cheat_infinite_health);
        fprintf(out,"local_multiplayer.input.p%u.cheat_infinite_ammo=%d\n",p+1,v->cheat_infinite_ammo);
        fprintf(out,"local_multiplayer.input.p%u.cheat_tracker=%d\n",p+1,v->cheat_tracker);
        fprintf(out,"local_multiplayer.input.p%u.cheat_noclip=%d\n",p+1,v->cheat_noclip);
        fprintf(out,"local_multiplayer.input.p%u.cheat_no_facehug=%d\n",p+1,v->cheat_no_facehug);
        fprintf(out,"local_multiplayer.input.p%u.cheat_no_enemy_attack=%d\n",p+1,v->cheat_no_enemy_attack);
        fprintf(out,"local_multiplayer.input.p%u.audio_motion_tracker=%d\n",p+1,v->audio_motion_tracker);
        fprintf(out,"local_multiplayer.input.p%u.audio_player_pain=%d\n",p+1,v->audio_player_pain);
        fprintf(out,"local_multiplayer.input.p%u.audio_facehug_pain=%d\n",p+1,v->audio_facehug_pain);
        fprintf(out,"local_multiplayer.input.p%u.audio_facehug_shake=%d\n",p+1,v->audio_facehug_shake);
        fprintf(out,"local_multiplayer.input.p%u.remember_automap=%d\n",p+1,v->remember_automap);
        fprintf(out,"local_multiplayer.input.p%u.cheat_reveal_map=%d\n",p+1,v->cheat_reveal_map);
    }
    for(p=1;p<AVP_MP_INPUT_PLAYERS;p++){
        const AvpMpInputProfile *v=&cfg->player[p];
        fprintf(out,"local_multiplayer.input.p%u.mouse_device=%s\n",p+1,v->mouse_path);
        fprintf(out,"local_multiplayer.input.p%u.gamepad_device=%s\n",p+1,v->gamepad_path);
        fprintf(out,"local_multiplayer.input.p%u.mouse_enabled=%d\n",p+1,v->mouse_enabled);
        fprintf(out,"local_multiplayer.input.p%u.gamepad_enabled=%d\n",p+1,v->gamepad_enabled);
        fprintf(out,"local_multiplayer.input.p%u.mouse_sensitivity=%.3f\n",p+1,v->mouse_sensitivity);
        fprintf(out,"local_multiplayer.input.p%u.mouse_acceleration=%.3f\n",p+1,v->mouse_acceleration);
        fprintf(out,"local_multiplayer.input.p%u.gamepad_deadzone=%.3f\n",p+1,v->gamepad_deadzone);
        fprintf(out,"local_multiplayer.input.p%u.gamepad_analog_movement=%d\n",p+1,v->gamepad_analog_movement);
        fprintf(out,"local_multiplayer.input.p%u.gamepad_enhanced_analog_controls=%d\n",p+1,v->gamepad_enhanced_analog_controls);
        fprintf(out,"local_multiplayer.input.p%u.gamepad_turn_sensitivity=%.3f\n",p+1,v->gamepad_turn_sensitivity);
        fprintf(out,"local_multiplayer.input.p%u.gamepad_turn_speed=%.3f\n",p+1,v->gamepad_turn_speed);
        for(a=0;a<3;a++){
            fprintf(out,"local_multiplayer.input.p%u.gamepad_axis_%u=%d\n",p+1,a,v->gamepad_axis[a]);
            fprintf(out,"local_multiplayer.input.p%u.gamepad_axis_%u_invert=%d\n",p+1,a,v->gamepad_axis_invert[a]);
        }
        for(a=0;a<AVP_MP_INPUT_ACTIONS;a++){
            fprintf(out,"local_multiplayer.input.p%u.key_%u=0x%lx\n",p+1,a,v->key[a]);
            fprintf(out,"local_multiplayer.input.p%u.mouse_button_%u=%d\n",p+1,a,v->mouse_button[a]);
            fprintf(out,"local_multiplayer.input.p%u.gamepad_button_%u=%d\n",p+1,a,v->gamepad_button[a]);
        }
    }
}
static int32_t saturated_add(int32_t old,int add)
{
    int64_t sum=(int64_t)old+add;
    return sum>INT32_MAX?INT32_MAX:sum<INT32_MIN?INT32_MIN:(int32_t)sum;
}
int avp_mp_input_mouse_motion(const AvpMpInputConfig *cfg,AvpMpMouseState states[AVP_MP_INPUT_PLAYERS],
                               const char *device,int dx,int dy)
{
    int p=avp_mp_input_mouse_owner(cfg,device);
    if(p<0||!states||!cfg->player[p].mouse_enabled)return 0;
    states[p].dx=saturated_add(states[p].dx,dx);
    states[p].dy=saturated_add(states[p].dy,dy);
    return 1;
}
int avp_mp_input_mouse_button(const AvpMpInputConfig *cfg,AvpMpMouseState states[AVP_MP_INPUT_PLAYERS],
                               const char *device,unsigned button,int down)
{
    int p=avp_mp_input_mouse_owner(cfg,device);
    if(p<0||!states||!cfg->player[p].mouse_enabled||button<1||button>AVP_MP_MOUSE_BUTTONS)return 0;
    if(down)states[p].buttons|=(uint16_t)(1u<<(button-1));
    else states[p].buttons&=(uint16_t)~(1u<<(button-1));
    return 1;
}
