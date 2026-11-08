#include "local_mp_commands.h"
#include "pc/pc_axis_deadzone.h"
#include "pc/pc_input_routing.h"
#include <string.h>

static int key_down(const AvpMpCommandInput *in, unsigned long key)
{
    unsigned i;
    if (!key) return 0;
    for (i=0;i<AVP_MP_COMMAND_KEY_SLOTS;i++)
        if (in->keysyms[i]==key) return 1;
    return 0;
}

static const AvpMpControllerSample *find_pad(const AvpMpCommandInput *in,
                                             const char *name)
{
    unsigned i;
    if (!name[0]) return NULL; /* Never share an unidentified controller. */
    for (i=0;i<AVP_MP_COMMAND_PADS;i++)
        if (!strcmp(name,in->gamepads[i].path)) return &in->gamepads[i];
    return NULL;
}

/* Match Player 1's original -2/-3... axis-as-button encoding.
 * A digital binding must not inherit movement deadzones: its threshold is
 * deliberately 60%% deflection, as in avp_pc_binding_down(). */
static int action_pad_down(const AvpMpControllerSample *pad,int token)
{
    int64_t n,index;
    if(!pad || token==-1)return 0;
    if(token>=0)return token<32 && !!(pad->buttons&(1u<<(unsigned)token));
    n=-(int64_t)token-2;
    if(n<0)return 0;
    index=n/2;
    if(index>=(int64_t)pad->axis_count || index>=AVP_MP_COMMAND_AXES)return 0;
    return (n&1) ? pad->axes[index]>=19660 : pad->axes[index]<=-19660;
}

static float axis_sample(const AvpMpInputProfile *profile,
                         const AvpMpControllerSample *pad,unsigned index)
{
    int n;
    if (!pad || index>=3) return 0.0f;
    n=profile->gamepad_axis[index];
    if (n<0 || n>=(int)AVP_MP_COMMAND_AXES || (unsigned)n>=pad->axis_count)
        return 0.0f;
    /* Exactly the original PC's normalized axis/deadzone semantics, but
     * independently evaluated from each player's selected physical pad. */
    return avp_pc_axis_deadzone(pad->axes[n],profile->gamepad_deadzone,
                                profile->gamepad_axis_invert[index]);
}

void avp_mp_commands_clear(AvpMpCommandInput *in)
{
    if (in) memset(in,0,sizeof(*in));
}

int avp_mp_commands_take(const AvpMpInputConfig *cfg,
                         AvpMpCommandInput *in,unsigned players,
                         AvpMpPlayerCommand result[AVP_LOCAL_MP_MAX_PLAYERS])
{
    AvpMpPlayerCommand staged[AVP_LOCAL_MP_MAX_PLAYERS]={{0}};
    unsigned p,a,q;
    if (!cfg || !in || !result || players<1 || players>AVP_LOCAL_MP_MAX_PLAYERS)
        return 0;
    /* Reject ambiguous physical controller assignment before any mutations.
     * Mice are already deduplicated by the existing input config validator. */
    for (p=0;p<players;p++) {
        const AvpMpInputProfile *v=&cfg->player[p];
        if (v->gamepad_enabled && v->gamepad_path[0])
            for (q=0;q<p;q++)
                if (cfg->player[q].gamepad_enabled &&
                    !strcmp(cfg->player[q].gamepad_path,v->gamepad_path)) return 0;
        if (v->gamepad_deadzone<0.0f || v->gamepad_deadzone>0.95f ||
            !(v->gamepad_deadzone>=0.0f)) return 0;
    }
    for (p=0;p<players;p++) {
        const AvpMpInputProfile *v=&cfg->player[p];
        const AvpMpControllerSample *pad=v->gamepad_enabled?find_pad(in,v->gamepad_path):NULL;
        uint32_t actions=0;
        float axis[3]={0.0f};
        if (pad) {
            if (pad->axis_count>AVP_MP_COMMAND_AXES) return 0;
            for (a=0;a<3;a++) {
                axis[a]=axis_sample(v,pad,a);
                if(v->gamepad_axis[a]>=0 &&
                   (unsigned)v->gamepad_axis[a]<pad->axis_count)
                    staged[p].analog_axis_mask|=1u<<a;
            }
        }
        for (a=0;a<AVP_MP_INPUT_ACTIONS;a++) {
            int down=0;
            /* P1's original host keyboard and controller mappings remain
             * authoritative; never silently double-poll them. */
            if (p!=0 || a==AVP_MP_MAP_ZOOM_IN || a==AVP_MP_MAP_ZOOM_OUT) {
                down=key_down(in,v->key[a]);
                if (action_pad_down(pad,v->gamepad_button[a])) down=1;
            }
            if (v->mouse_enabled && v->mouse_path[0] &&
                v->mouse_button[a]>=1 && v->mouse_button[a]<=16 &&
                (in->mice[p].buttons & (1u<<(unsigned)(v->mouse_button[a]-1))))
                down=1;
            if (down) actions|=1u<<a;
        }
        staged[p].digital_actions=actions;
        /* P1 promotes >=90% stick travel to full Jaguar acceleration.
         * Apply the SAME response to each other player. */
        staged[p].analog_forward=avp_pc_axis_motion_full_travel(axis[0]);
        staged[p].analog_strafe=avp_pc_axis_motion_full_travel(axis[1]);
        staged[p].analog_turn=axis[2];
        /* Make the per-player analog settings reach the ORIGINAL native
         * PLAYER.S host bridge. Previously these command fields stayed zero,
         * so the native input layer silently disabled all proportional axes. */
        staged[p].analog_enabled=v->gamepad_analog_movement && v->gamepad_enabled;
        staged[p].enhanced_analog_enabled=v->gamepad_enhanced_analog_controls;
        staged[p].analog_turn_sensitivity=v->gamepad_turn_sensitivity;
        staged[p].analog_turn_speed=v->gamepad_turn_speed;
        if(p!=0 && !staged[p].analog_enabled){
            /* P1 Jaguar/D-pad fallback is a SWITCH, not any nonzero stick
             * deflection. Analog ON uses the proportional host bridge only;
             * never send a second digital step for the same axis. */
            if(avp_pc_axis_digital_positive(axis[0]))actions|=1u<<AVP_MP_FORWARD;
            if(avp_pc_axis_digital_negative(axis[0]))actions|=1u<<AVP_MP_BACKWARD;
            if(avp_pc_axis_digital_negative(axis[1]))actions|=1u<<AVP_MP_STRAFE_LEFT;
            if(avp_pc_axis_digital_positive(axis[1]))actions|=1u<<AVP_MP_STRAFE_RIGHT;
            if(axis[2]<-0.5f)actions|=1u<<AVP_MP_TURN_LEFT;
            if(axis[2]>0.5f)actions|=1u<<AVP_MP_TURN_RIGHT;
        }
        staged[p].actions=actions;
        if (v->mouse_enabled && v->mouse_path[0]) {
            staged[p].mouse_dx=in->mice[p].dx;
            staged[p].mouse_dy=in->mice[p].dy;
        }
    }
    memcpy(result,staged,sizeof(staged));
    for(p=0;p<players;p++) {
        in->mice[p].dx=0;
        in->mice[p].dy=0;
    }
    return 1;
}
