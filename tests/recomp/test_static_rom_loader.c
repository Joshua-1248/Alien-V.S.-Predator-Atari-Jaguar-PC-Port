#include "recomp/avp_recomp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct T{unsigned calls;unsigned writes;}T;
static uint32_t rd(void*u,uint32_t a){(void)u;(void)a;return 0;}
static void wr(void*u,uint32_t a,uint32_t v){T*t=u;(void)a;(void)v;t->writes++;}
static int ext(void*u,AvpSrCpu*c,uint32_t target){T*t=u;t->calls++;if(target==0x008022ccu||target==0x008030e6u||target==0x00803112u){c->d[0]=0;return 0;}return -1;}
int main(void){
 uint8_t*ram=calloc(1,AVP_SR_RAM_SIZE),*rom=calloc(1,AVP_SR_ROM_SIZE);T t={0};AvpSrMachine m;AvpSrHooks h={&t,rd,wr,ext,0};
 if(!ram||!rom)return 2;
 avp_sr_init(&m,ram,AVP_SR_RAM_SIZE,rom,AVP_SR_ROM_SIZE,&h);
 if(avp_sr_call(&m,0x00802000u)!=0){fprintf(stderr,"fault %08x %s\n",m.fault_addr,m.fault_reason);return 3;}
 if(m.cpu.pc!=0x00008000u||m.cpu.a[7]!=0x00007548u)return 4;
 if(avp_sr_read32(&m,0x7fe8)!=0x001e0000u||avp_sr_read32(&m,0x7ff4)!=0x0080e842u||avp_sr_read32(&m,0x7ff8)!=0x00008000u)return 5;
 if(t.calls!=4u)return 6; /* init_alloc + init_inflate + two inflate calls */
 puts("static ROM.S recomp test passed");free(ram);free(rom);return 0;
}
