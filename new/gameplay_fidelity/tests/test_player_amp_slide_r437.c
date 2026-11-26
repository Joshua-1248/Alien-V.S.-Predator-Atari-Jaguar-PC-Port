/* Actual projected MovePlayer and AMPCollisions, no substitute collision model. */
#include "player.h"
#include "maze.h"
#include "joypad.h"
#include "avp_runtime.h"
#include "mazescrn.h"
#include "doors.h"
#include "collision.h"
#include "amp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile u32 joy_cur=0xffffffffu,joy_edge=0xffffffffu,joy2_cur=0xffffffffu,joy2_edge=0xffffffffu;
u32 x_pos,y_pos,centre_angle;
s32 sin_ang,cos_ang;
s16 hugkill;
u8 lift_key;
u8 fullbits[256];
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
static AvpAmp amps[AVP_NUM_AMPS];
AvpAmp *amp_data=amps;
static AvpRuntimeOps ops;
const AvpRuntimeOps *avp_runtime_ops(void){return &ops;}
static void active(AvpAmp *a){(void)a;}
static int host_wall(void *u,s32 x,s32 y,s16 h,u16 w){(void)u;return SafePos((u8)h,x,y,w);}
static int host_amp(void *u,s32 x,s32 y){(void)u;return !AMPCollisions(x,y);}
static int noclip(void *u){(void)u;return 1;}
static unsigned cases;
static void fixture(s32 ax,s32 ay,s32 dx,s32 dy,int want_x,int want_y)
{
    const s32 q=0x58000;
    x_pos=(u32)(q+ax);y_pos=(u32)(q+ay);x_vel=dx*4/3;y_vel=dy*4/3;
    const s32 x0=(s32)x_pos,y0=(s32)y_pos;
    /* 4/3 is exact here, and with dec_shift=2 the movement is dx/dy. */
    MovePlayer();
    if((s32)x_pos!=x0+want_x*dx || (s32)y_pos!=y0+want_y*dy || x_vel!=want_x*dx || y_vel!=want_y*dy){
        fprintf(stderr,"FAIL offset %d,%d step %d,%d got %d,%d velocity %d,%d expected %d,%d\n",ax,ay,dx,dy,(s32)x_pos-x0,(s32)y_pos-y0,x_vel,y_vel,want_x*dx,want_y*dy);assert(0);
    }++cases;
}
int main(void){
    static u8 maze[8*8*8];const s32 q=0x58000,w=0x8800,step=0x1800;
    avp_collision_bind_maze(maze,8,8);
    amps[0].xpos=amps[0].ypos=q;amps[0].mode=active;amps[0].creature=AC_AQUEEN;
    collmap[5*AVP_AMP_GRID_W+5]=1;
    assert(AMPCollisions(q+w-1,q));assert(!AMPCollisions(q+w,q));
    assert(AMPCollisions(q-w+1,q));assert(!AMPCollisions(q-w,q));
    for(int sp=0;sp<=8;sp+=4){player_type=(s16)sp;InitMove();dec_shift=2;
        for(int host=0;host<2;host++){
            ops=(AvpRuntimeOps){0};if(host){ops.safe_pos=host_wall;ops.amp_collision=host_amp;}
            for(int sx=-1;sx<=1;sx+=2)for(int sy=-1;sy<=1;sy+=2){
                /* At the corner, both axial candidates are valid; Y wins. */
                fixture(sx*(w+0x600),sy*(w+0x600),-sx*step,-sy*step,0,1);
                /* Only X is valid (already within the X footprint). */
                fixture(sx*(w-0x600),sy*(w+0x600),-sx*step,-sy*step,1,0);
                /* Only Y is valid (already within the Y footprint). */
                fixture(sx*(w+0x600),sy*(w-0x600),-sx*step,-sy*step,0,1);
                /* All three hit: preserve stop. Existing overlap stays blocked. */
                fixture(sx*(w-0x600),sy*(w-0x600),-sx*step,-sy*step,0,0);
                /* A safe full endpoint must not fall through to an axis. */
                fixture(sx*(w+0x600),sy*(w+0x600),sx*step,sy*step,1,1);
            }
            ops.player_noclip=noclip;fixture(w-0x600,w-0x600,-step,-step,1,1);
        }
    }
    printf("PASS %u real MovePlayer/AMPCollisions Queen cases, all species, quadrants, native and runtime callbacks\n",cases);return 0;
}
