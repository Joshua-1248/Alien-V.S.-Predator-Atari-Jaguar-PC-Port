/* Planner -> real continuous heading and proportional velocity helpers.
 * No world positions are written by the planner. This isolated fixture
 * simulates free-space response, not complete multiplayer gameplay. */
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include "pc/pc_analog_aim.h"
#include "../../pc_video/pc_analog_velocity.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#define PI 3.14159265358979323846
#define A(x) (1u<<(x))
static uint32_t angle(double degrees){return (uint32_t)(uint64_t)llround(degrees*4294967296.0/360.0);}
static double degrees(uint32_t a,uint32_t b){return (double)(int32_t)(a-b)*360.0/4294967296.0;}
static void view_init(AvpMpBotObservation *v,unsigned species){
    memset(v,0,sizeof(*v));v->x=v->y=10u<<16;v->player_species=species;
    v->player_energy=v->max_energy=1000;v->has_target=1;v->goal=AVP_MP_BOT_PATROL;
    v->tick=1;v->current_level=1;
}
static void aim_at(AvpMpBotObservation *v,double a,double radius){
    v->target_x=(uint32_t)((int64_t)v->x+llround(cos(a*PI/180.0)*radius*65536.0));
    v->target_y=(uint32_t)((int64_t)v->y-llround(sin(a*PI/180.0)*radius*65536.0));
}
int main(void){
    unsigned angles=0;
    const double aims[]={0.125,31.25,90.75,179.875,270.0625,359.875};
    for(unsigned species=0;species<3;species++)for(unsigned i=0;i<6;i++)for(unsigned h=0;h<32;h++){
        AvpMpBotState s={0};AvpMpBotObservation v;AvpMpPlayerCommand c;
        AvpPcContinuousHeading state={0};int maximum=species==1?0x900:0x600;
        view_init(&v,species);aim_at(&v,aims[i],5.0);v.heading=angle(h*11.25);
        for(unsigned t=0;t<45;t++){
            /* Keep the isolated angular test out of navigation-stall recovery. */
            memset(&s,0,sizeof s);avp_mp_bot_step_precise(&s,&v,maximum,&c);
            assert(c.analog_enabled && c.enhanced_analog_enabled && (c.analog_axis_mask&4));
            assert(!(c.digital_actions&(A(AVP_MP_TURN_LEFT)|A(AVP_MP_TURN_RIGHT))));
            assert(fabsf(c.analog_turn)<=1.0f);
            double old=fabs(degrees(angle(aims[i]),v.heading));
            v.heading=avp_pc_continuous_heading_tick(&state,v.heading,c.analog_turn,1,1,maximum);
            assert(fabs(degrees(angle(aims[i]),v.heading))<=old+0.001);
        }
        assert(fabs(degrees(angle(aims[i]),v.heading))<0.001);
        uint32_t stop=v.heading;
        for(unsigned t=0;t<100;t++){
            memset(&s,0,sizeof s);avp_mp_bot_step_precise(&s,&v,maximum,&c);
            v.heading=avp_pc_continuous_heading_tick(&state,v.heading,c.analog_turn,1,1,maximum);
        }
        assert(fabs(degrees(stop,v.heading))<0.00001);angles++;
    }
    for(unsigned species=0;species<3;species++){
        AvpMpBotState s={0};AvpMpBotObservation v;AvpMpPlayerCommand c,far;
        uint8_t cells[8*16*16]={0},flags[256]={0};
        for(unsigned i=0;i<16*16;i++)cells[8*i+5]=1;
        view_init(&v,species);v.x=v.y=(10u<<16)|0x8000;
        v.map=(AvpMpBotMap){cells,16,16,flags,NULL,0,NULL,NULL,0};
        v.goal=AVP_MP_BOT_AIRLOCK_SEAT;v.airlock_face_side=2;
        aim_at(&v,0,1.5);avp_mp_bot_step_precise(&s,&v,0,&far);
        assert((far.analog_axis_mask&3)==3 && far.analog_forward>0);
        memset(&s,0,sizeof s);aim_at(&v,0,0.24);avp_mp_bot_step_precise(&s,&v,0,&c);
        fprintf(stderr,"species %u near actions=%x digital=%x mask=%x forward=%f strafe=%f far=%f\n",species,c.actions,c.digital_actions,c.analog_axis_mask,c.analog_forward,c.analog_strafe,far.analog_forward);
        assert((c.analog_axis_mask&3)==3 && c.analog_forward>0 && c.analog_forward<far.analog_forward);
        assert(!(c.digital_actions&A(AVP_MP_FORWARD)));
        int32_t vx=0,vy=0,fx=0,fy=0;unsigned cap=species==1?17000:11900;
        for(unsigned t=0;t<100;t++){
            avp_pc_analog_velocity(&vx,&vy,32767,0,c.analog_forward,c.analog_strafe,0,0,0,0,cap,2);
            avp_pc_analog_velocity(&fx,&fy,32767,0,far.analog_forward,far.analog_strafe,0,0,0,0,cap,2);
        }
        assert(vx>0 && vx<fx && vy==0 && fy==0);
        printf("species %u: approach %.4f vs cruise %.4f; velocity %d vs %d\n",species,c.analog_forward,far.analog_forward,vx,fx);
        memset(&s,0,sizeof s);v.facehugged=1;v.facehug_direction=-1;
        avp_mp_bot_step_precise(&s,&v,0,&c);assert(!c.actions&&!c.analog_enabled);
        avp_mp_bot_step_precise(&s,&v,0,&c);assert(c.actions==A(AVP_MP_STRAFE_RIGHT)&&!c.analog_enabled);
        avp_mp_bot_step_precise(&s,&v,0,&c);assert(!c.actions&&!c.analog_enabled);
    }
    printf("PASS %u heading convergence/hold scenarios; three species approach and Facehugger inputs\n",angles);
}
