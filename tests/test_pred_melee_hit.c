/* Retail PLAYER.S TestSpark melee trace with Predator wrist range and
 * honour-bit ownership; no injected shot callback. */
#include "player.h"
#include "maze.h"
#include "mazescrn.h"
#include "amp.h"
#include <assert.h>
#include <string.h>

AvpAmp amps[AVP_NUM_AMPS],*amps_at=amps;
u32 x_pos=0x100000u,y_pos=0x100000u,centre_angle=0,fire_angle=0;
s16 swing_x=0;
s16 fire_damage=30;
u32 fire_distance=0x90u<<8;
u16 fire_width=20;
u32 FireDistance(void){return 0;}
s32 cos_d0(u16 a){(void)a;return 0x7fff;}
s32 sin_d0(u16 a){(void)a;return 0;}
static void acting(AvpAmp *a){(void)a;}
void make_spark_at(s32 x,s32 y,int blood){(void)x;(void)y;(void)blood;}
void make_spark_hit_at(s32 x,s32 y,const AvpAmp*a){(void)x;(void)y;(void)a;}
int main(void){
    AvpAmp *a=&amps[0];
    player_type=PT_PREDATOR;wep_fire=0xff;seed0=seed1=0;invisflag=0;
    memset(amps,0,sizeof(amps));a->mode=acting;a->flags=(u16)((1u<<AMP_KILLABLE)|(1u<<AMP_INVHIT));a->energy=300;
    a->xpos=(s32)x_pos+0x8000;a->ypos=(s32)y_pos;
    assert(TestSpark()==a && a->energy==270);
    /* R272: PLAYER.S:1066 ORs invisflag in -- "once set, stays set". */
    assert(a->flags&(1u<<AMP_INVHIT));
    assert(a->flags&(1u<<AMP_PHIT));
    assert(a->flags&(1u<<AMP_CLOSEHIT));
    a->xpos=(s32)x_pos+0x18000;a->energy=300;
    assert(TestSpark()==NULL && a->energy==300);
    a->xpos=(s32)x_pos+0x8000;
    wep_fire=0;
    assert(TestSpark()==NULL && a->energy==300);
    return 0;
}
