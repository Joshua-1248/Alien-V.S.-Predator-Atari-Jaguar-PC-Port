/* R255: MAZE.S view-only Alien bite uses modulo-32 CPU register arithmetic.
 * Exercises the real NextFrame, not a reimplementation of its displacement. */
#include "maze.h"
#include "avp_runtime.h"
#include "amp.h"
#include "player.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

s16 player_type,player_energy=100,player_dead,game_over,key_lock,destruct_flag,launch_flag,cheat,use_cocoon;
u8 wep_fire;
AvpAmp *amps_at;
int CheckCocoon(void){return 0;}
static unsigned draw_count, update_count;
static void draw(void *user){(void)user;++draw_count;}
void UpdatePlayer(void){++update_count;}
void UpdateAMPs(void){}
void PreFrame(void){}
void SwapScreens(void){}
void PostFrame(void){}
void DoPause(void){}

/* Only the MAZE.S NextFrame section is linked; all other game services are
 * test-controlled. No change to the original frame implementation is needed. */
static AvpRuntimeOps ops;
const AvpRuntimeOps *avp_runtime_ops(void){return &ops;}

int main(void)
{
    const u32 coords[]={0u,1u,0x7ffffffeu,0x7fffffffu,0x80000000u,
                        0x80000001u,0xffffffffu,0xffff0000u};
    const u16 angles[]={0u,0x1000u,0x2000u,0x4000u,0x8000u,0xc000u};
    unsigned cases=0;
    memset(&ops,0,sizeof(ops));ops.gpu_draw_screen=draw;
    for(unsigned shift=1;shift<=4;shift++)
        for(unsigned ai=0;ai<sizeof angles/sizeof angles[0];ai++)
            for(unsigned xi=0;xi<sizeof coords/sizeof coords[0];xi++)
                for(unsigned yi=0;yi<sizeof coords/sizeof coords[0];yi++){
                    u32 px=coords[xi],py=coords[yi],a=(u32)angles[ai]<<16;
                    s32 c=cos_d0(angles[ai]),s=sin_d0(angles[ai]);
                    u32 expected_x=px+(u32)(c>>shift);
                    u32 expected_y=py-(u32)(s>>shift);
                    x_pos=px;y_pos=py;centre_angle=a;alien_bite=(s16)shift;
                    NextFrame();
                    assert(x_pos==px&&y_pos==py&&centre_angle==a);
                    assert(gpu_xpos==expected_x&&gpu_ypos==expected_y);
                    assert(gpu_angle==a&&gpu_sin_ang==s&&gpu_cos_ang==c);
                    ++cases;
                }
    assert(cases==1536u&&draw_count==cases&&update_count==cases);
    puts("R255 NextFrame Alien bite 32-bit snapshot parity: PASS (1536 cases)");
    return 0;
}
