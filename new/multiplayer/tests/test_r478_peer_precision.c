#include "local_mp_bot.h"
#include "local_mp_input.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
int main(void){
 uint8_t cells[16*8]={0},flags[256]={0};
 for(unsigned i=0;i<16;i++)cells[i*8+5]=1;
 unsigned totals[3][2]={{0}};
 for(unsigned dm=0;dm<2;dm++)for(unsigned sp=0;sp<3;sp++)for(unsigned behavior=0;behavior<3;behavior++)for(unsigned h=0;h<64;h++)for(unsigned variant=0;variant<3;variant++){
  AvpMpBotState s={0};AvpMpBotObservation v={0};AvpMpPlayerCommand c={0};
  v.map=(AvpMpBotMap){cells,16,1,flags,NULL,0,NULL,NULL,0};v.x=(7u<<16)|32768;v.y=32768;v.target_x=(11u<<16)|32768;v.target_y=v.y;v.tick=1;v.observed_world_level=4;v.player_index=2;v.player_species=sp;v.behavior=behavior;v.deathmatch=dm;v.player_energy=v.max_energy=1000;v.has_target=1;v.goal=AVP_MP_BOT_PATROL;v.heading=h*0x04000000u;
  if(variant==1){v.nearby_peer_valid=1;v.nearby_peer_index=3;v.nearby_peer_x=v.x-(2u<<16);v.nearby_peer_y=v.y;v.prop_count=1;v.props[0].x=v.nearby_peer_x;v.props[0].y=v.nearby_peer_y;v.props[0].halfwidth=0xa000u;}
  if(variant==2){v.acid_count=1;v.acid[0].x=(1u<<16)|32768;v.acid[0].y=v.y;}
  avp_mp_bot_step_precise(&s,&v,0,&c);
  totals[variant][0]++;
  if((c.analog_axis_mask&3u)==3u && fabs(c.analog_forward)+fabs(c.analog_strafe)>0.00001)totals[variant][1]++;
 }
 for(unsigned i=0;i<3;i++)printf("%s: proportional movement %u/%u\n",(const char*[]){"unobstructed","peer two tiles behind","acid six tiles behind"}[i],totals[i][1],totals[i][0]);
 for(unsigned i=0;i<3;i++)if(totals[i][0]!=1152u || totals[i][1]!=1152u)return 1;
 return 0;
}
