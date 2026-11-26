/* White-box regression for the conservative precision-only guard. */
#include "../local_mp_bot.c"
#include <stdio.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"guard check failed at line %d\n",__LINE__);return 1;} }while(0)
int main(void){
 uint8_t cells[16*8]={0},flags[256]={0};
 AvpMpBotObservation v={0};
 v.map=(AvpMpBotMap){cells,16,1,flags,NULL,0,NULL,NULL,0};
 v.x=(7u<<16)|32768;v.y=32768;v.nearby_peer_valid=1;
 v.nearby_peer_x=v.x-(2u<<16);v.nearby_peer_y=v.y;
 v.prop_count=1;v.props[0]=(AvpMpBotProp){.x=v.nearby_peer_x,.y=v.y,.halfwidth=AVP_MP_PEER_CLEARANCE};
 CHECK(bot_peer_precision_clear(&v,1,0,0.68));
 CHECK(!bot_peer_precision_clear(&v,-1,0,0.68));
 v.prop_count=0;CHECK(!bot_peer_precision_clear(&v,1,0,0.68));
 v.prop_count=AVP_MP_BOT_MAX_NEAR_PROPS;CHECK(!bot_peer_precision_clear(&v,1,0,0.68));
 v.prop_count=2;v.props[1]=(AvpMpBotProp){.x=v.x+40000,.y=v.y,.halfwidth=AVP_MP_PEER_CLEARANCE};
 CHECK(!bot_peer_precision_clear(&v,1,0,0.68));
 v.prop_count=1;v.nearby_peer_x=v.x-60000;v.props[0].x=v.nearby_peer_x;
 CHECK(!bot_peer_precision_clear(&v,1,0,0.68));
 v.player_collision_disabled=1;CHECK(bot_peer_precision_clear(&v,1,0,0.68));
 puts("7 clearance checks PASS");return 0;
}
