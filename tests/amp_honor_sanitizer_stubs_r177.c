#include "amp.h"
#include "collision.h"
u16 invisflag;
s16 use_cocoon,num_cocoons,key_lock,game_over;
u8 acs_level;
u16 avp_random(void){return 0;}
s32 sin_d0(u16 x){(void)x;return 0;}
s32 cos_d0(u16 x){(void)x;return 0;}
int LineOfSight(const AvpXY *a,const AvpXY *b){(void)a;(void)b;return 0;}
void MakeCocoon(u32 x,u32 y){(void)x;(void)y;}
