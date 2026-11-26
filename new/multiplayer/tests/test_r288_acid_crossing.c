/* RE44 R288: long-horizon bot-command proxy for original-width passages.
 * Never changes or substitutes the game's collision or acid physics.
 * Each scenario feeds ACTUAL observed position back into the planner after
 * collision-limited translation; test fails unless the bot reaches the far
 * side of the acid, not merely because it emitted one Forward command. */
#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#define CELL(x,y,w) ((y)*(w)*8u+(x)*8u)
static uint8_t map_cells[64u*64u*8u],flags[256],risk[4096];
static const double pi=3.141592653589793;
static int run(unsigned position,unsigned offset,unsigned count,
               unsigned vertical,unsigned mode){
    AvpMpBotState bot={0};
    AvpMpBotObservation v={0};
    AvpMpPlayerCommand cmd={0};
    memset(map_cells,0,sizeof map_cells);
    memset(risk,0,sizeof risk);
    for(unsigned k=0;k<8;k++)
        map_cells[CELL(vertical?0u:k,vertical?k:0u,vertical?1u:8u)+5u]=1u;
    v.map=(AvpMpBotMap){map_cells,vertical?1u:8u,vertical?8u:1u,
                        flags,NULL,0u,risk,NULL,0u};
    v.player_species=AVP_MP_MARINE;
    v.player_index=2u;
    v.player_collision_disabled=1u;
    v.player_energy=v.max_energy=100u;
    v.x=(uint32_t)((vertical?(double)position/100.0:1.2)*65536.0);
    v.y=(uint32_t)((vertical?1.2:(double)position/100.0)*65536.0);
    v.heading=vertical?0xc0000000u:0u;
    v.target_x=(uint32_t)((vertical?0.5:7.5)*65536.0);
    v.target_y=(uint32_t)((vertical?7.5:0.5)*65536.0);
    v.goal=mode==0u?AVP_MP_BOT_SUPPLY:AVP_MP_BOT_HOSTILE;
    v.has_target=1u;
    if(mode){
        v.hostile_target=1u;
        v.target_slot=7u;
        v.target_is_alien=1u;
        if(mode==1u)v.tracker_only_target=1u;
        else{
            v.target_visible=1u;
            v.current_weapon=2u;
            v.marine_inventory_valid=1u;
            v.marine_owned=1u<<2;
            v.marine_ammo[1]=100u;
            if(mode==2u)v.other_player_alien_trapped=1u;
            else v.other_player_swarmed=1u;
        }
    }
    v.acid_count=count;
    for(unsigned k=0;k<count;k++){
        const double along=2.5+(double)k;
        const double side=(double)offset/100.0;
        v.acid[k]=(AvpMpBotAcid){
            (uint32_t)((vertical?side:along)*65536.0),
            (uint32_t)((vertical?along:side)*65536.0)};
        avp_mp_bot_acid_risk_mark(risk,v.acid[k].x,v.acid[k].y);
    }
    unsigned fired=0u;
    for(unsigned tick=1;tick<=200;tick++){
        v.tick=tick;
        avp_mp_bot_step(&bot,&v,&cmd);
        fired|=!!(cmd.actions&(1u<<AVP_MP_FIRE));
        const double a=(double)(v.heading>>16)*2.0*pi/65536.0;
        const double fx=cos(a),fy=-sin(a);
        double dx=0.0,dy=0.0;
        if(cmd.actions&(1u<<AVP_MP_FORWARD)){dx+=fx;dy+=fy;}
        if(cmd.actions&(1u<<AVP_MP_BACKWARD)){dx-=fx;dy-=fy;}
        if(cmd.actions&(1u<<AVP_MP_STRAFE_LEFT)){dx+=fy;dy-=fx;}
        if(cmd.actions&(1u<<AVP_MP_STRAFE_RIGHT)){dx-=fy;dy+=fx;}
        if(cmd.actions&(1u<<AVP_MP_TURN_LEFT))v.heading+=(0x180u<<16);
        if(cmd.actions&(1u<<AVP_MP_TURN_RIGHT))v.heading-=(0x180u<<16);
        const double x=(double)v.x/65536.0+0.07*dx;
        const double y=(double)v.y/65536.0+0.07*dy;
        if(vertical?(x>=0.3125 && x<=0.6875 && y>=0.3125 && y<7.6875):
                    (y>=0.3125 && y<=0.6875 && x>=0.3125 && x<7.6875)){
            v.x=(uint32_t)(x*65536.0);
            v.y=(uint32_t)(y*65536.0);
        }
        /* Fire/rescue stops to engage at source-safe gun distance; reaching
         * 4.7 means it already crossed both acid patches to defend its ally.
         * Ordinary supply and tracker pursuit must reach the far room. */
        const double along=vertical?(double)v.y/65536.0:
                                    (double)v.x/65536.0;
        if(along >= (mode>=2u?4.7:6.8))
            return mode<2u || fired;
    }
    return 0;
}
int main(void){
    unsigned ok=0u,fail=0u;
    for(unsigned mode=0u;mode<4u;mode++)
    for(unsigned vertical=0u;vertical<2u;vertical++)
    for(unsigned count=1u;count<=2u;count++)
    for(unsigned position=32u;position<=68u;position+=3u)
    for(unsigned offset=22u;offset<=78u;offset+=3u){
        if(run(position,offset,count,vertical,mode))ok++;
        else{
            if(fail<12u)fprintf(stderr,
                "FAIL mode=%u vertical=%u puddles=%u side=%u acid=%u\n",
                mode,vertical,count,position,offset);
            fail++;
        }
    }
    printf("R288: sustained acid crossing/co-op rescue %u PASS, %u FAIL\n",
           ok,fail);
    return fail?1:0;
}
