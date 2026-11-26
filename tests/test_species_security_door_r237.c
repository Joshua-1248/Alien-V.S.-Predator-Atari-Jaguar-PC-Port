/* R237: every normal security hatch uses real DoorKeys interaction and
 * allows Alien/Predator entry regardless of stale numeric access credentials.
 * Marine clearance and special authored door codes must remain intact. */
#include "doors.h"
#include "amp.h"
#include "avp_runtime.h"
#include "hud_message.h"
#include "joypad.h"
#include "player.h"
#include "avp_sfx_ids.h"
#include <stdio.h>
#include <string.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
static unsigned tests,denied,code,out_sound;
static const AvpHudMessageStep *notice;
static void sfx(void *u,unsigned id){(void)u;out_sound=id;}
static const AvpRuntimeOps ops={.play_sfx=sfx};
const AvpRuntimeOps *avp_runtime_ops(void){return &ops;}
u32 new_pos,levfx_ID,x_pos,y_pos,centre_angle;
s16 player_type,cur_level,new_level;
s32 new_x,new_y;
volatile u32 joy_cur=0xffffffffu,joy_edge=0xffffffffu;
u8 panel_list[AVP_MAX_PANELS];
static AvpAmp amps[AVP_NUM_AMPS];
AvpAmp *amp_data=amps;
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
const AvpHudMessageStep avp_msg_pressure[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_notsecure[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_airlocked[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_jammed[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_jlift[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_escape[]={{0,{0,0,0}}};
const AvpHudMessageStep avp_msg_access_denied[]={{0,{0,0,0}}};
void avp_hudmsg_queue(const AvpHudMessageStep *p){notice=p;if(p==avp_msg_access_denied)++denied;}
void avp_hudmsg_set_access_code(unsigned n){code=n;}
u32 DoEffect(unsigned id){return id;}
#define CHECK(v) do{if(!(v)){fprintf(stderr,"R237 FAIL line %u: %s\n",(unsigned)__LINE__,#v);return 1;}++tests;}while(0)
int main(void)
{
    static u8 maze[64u*64u*8u];
    const unsigned cell=(10u*64u+10u)*8u,door=cell+8u+6u;
    AvpDoorAccessEntry entry[]={{(u16)door,7}};
    const AvpDoorLevelMeta meta={.access=entry,.access_count=1u};
    const s16 species[]={PT_ALIEN,PT_PREDATOR,PT_HUMAN};
    unsigned si,req,level,mode;
    avp_doors_bind_maze(maze,64u,64u);
    avp_doors_bind_panel_ids(NULL);
    cur_level=1;avp_doors_bind_level_meta(&meta,1u);
    for(si=0;si<3u;++si){
      for(req=1u;req<=10u;req+=3u){
        for(level=0u;level<=10u;level+=5u){
          for(mode=0;mode<2u;++mode){
            memset(maze,0,sizeof maze);
            maze[cell+2u]=0x20u;
            entry[0].code=(s16)req;
            player_type=species[si];InitDoors();ResetAccess();InitAccess();
            acs_level=(u8)level;
            x_pos=(10u<<16)|0x8000u;y_pos=(10u<<16)|0x8000u;
            centre_angle=0;new_pos=cell;
            joy_cur=joy_edge=0xffffffffu & ~(1u<<FIRE_B);
            denied=code=out_sound=0;notice=NULL;
            avp_doors_pc_alien_use(mode&&si==0u,1);
            DoorKeys();avp_doors_pc_alien_use(0,0);
            if(si==2u && level<req){
               CHECK(denied==1u && code==req && notice==avp_msg_access_denied);
               MoveDoors();CHECK(maze[door]==0u);
            }else{
               CHECK(denied==0u && code==0u);
               if(si==2u)CHECK(out_sound==AVP_FX_RISE2);
               MoveDoors();CHECK(maze[door]==4u);
            }
          }
        }
      }
    }
    for(si=0u;si<2u;++si){
      player_type=species[si];
      for(req=0u;req<2u;++req){
        memset(maze,0,sizeof maze);maze[cell+2u]=0x20u;
        entry[0].code=req?AVP_ACC_UNUSED:AVP_ACC_JAMMED;
        InitDoors();ResetAccess();acs_level=0;
        x_pos=(10u<<16)|0x8000u;y_pos=(10u<<16)|0x8000u;
        centre_angle=0;new_pos=cell;
        denied=code=0;notice=NULL;
        joy_cur=joy_edge=0xffffffffu & ~(1u<<FIRE_B);
        avp_doors_pc_alien_use(si==0u,1);DoorKeys();avp_doors_pc_alien_use(0,0);
        CHECK(denied==0u && code==0u);
        CHECK(notice==(req?avp_msg_airlocked:avp_msg_jammed));
        MoveDoors();CHECK(maze[door]==0u);
      }
    }
    printf("R237 species-aware security doors: %u checks PASS\n",tests);
    return 0;
}
