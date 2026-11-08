#include "doors.h"
#include "amp.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "../src/game/doors.c"
u32 x_pos,y_pos;
u16 collmap[AVP_AMP_GRID_W*AVP_AMP_GRID_H];
AvpAmp level1amps[AVP_NUM_AMPS],*amp_data=level1amps;
static uint64_t fnv=UINT64_C(14695981039346656037);
static void feed(unsigned v){fnv^=(uint8_t)v;fnv*=UINT64_C(1099511628211);}
static int ref(uint32_t player,uint32_t door){uint32_t v=player-door;if(v&0x80000000u)v=0u-v;return v<=DC_SIZE;}
static void check(uint32_t player,uint32_t door,int axis){uint8_t p=0;DoorState d={0};d.ptr=&p;d.lim=64;d.grid1=d.grid2=0xffffu;d.index=axis?4u:0u;if(axis){x_pos=0;y_pos=player;d.xpos=0;d.ypos=(s32)door;}else{x_pos=player;y_pos=0;d.xpos=(s32)door;d.ypos=0;}int r=door_collides(&d);assert(r==ref(player,door));feed((unsigned)r);}
int main(void){
  static const uint32_t points[]={0u,1u,0x47ffu,0x4800u,0x4801u,0xffffb7ffu,0xffffb800u,0xffffb801u,0x7fffffffu,0x80000000u,0xffffffffu,0x00010000u,0xffff0000u,0x12345678u,0xfedcba98u};
  for(unsigned a=0;a<sizeof(points)/sizeof(points[0]);a++)for(unsigned b=0;b<sizeof(points)/sizeof(points[0]);b++){for(int axis=0;axis<2;axis++){check(points[a],points[b],axis);}}
  for(uint32_t i=0;i<4096u;i++){uint32_t x=i*0x9e3779b9u,y=i*0x01010101u;check(x,y,0);check(y,x,1);}
  printf("door delta: %llu combinations hash=%016llx\n", (unsigned long long)(450+8192),(unsigned long long)fnv);return 0;
}
