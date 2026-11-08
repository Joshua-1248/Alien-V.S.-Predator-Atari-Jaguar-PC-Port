#include "recomp/avp_recomp.h"
#include <stdio.h>
#include <string.h>

static int ram_ok(const AvpSrMachine *m,uint32_t a,size_t n){return a<=m->ram_size&&n<=m->ram_size-a;}
static int rom_ok(const AvpSrMachine *m,uint32_t a,size_t n){
    if(a<AVP_SR_ROM_BASE)return 0;
    a-=AVP_SR_ROM_BASE;
    return a<=m->rom_size&&n<=m->rom_size-a;
}
void avp_sr_init(AvpSrMachine *m,uint8_t *ram,size_t rn,const uint8_t *rom,size_t romn,const AvpSrHooks *h){
    memset(m,0,sizeof(*m));m->ram=ram;m->ram_size=rn;m->rom=rom;m->rom_size=romn;if(h)m->hooks=*h;
}
void avp_sr_fault(AvpSrMachine*m,uint32_t a,const char*r){m->cpu.faulted=1;m->fault_addr=a;snprintf(m->fault_reason,sizeof(m->fault_reason),"%s",r?r:"fault");}
uint8_t avp_sr_read8(AvpSrMachine*m,uint32_t a){
    if(ram_ok(m,a,1))return m->ram[a];
    if(rom_ok(m,a,1))return m->rom[a-AVP_SR_ROM_BASE];
    if((a&0x00f00000u)==0x00f00000u&&m->hooks.mmio_read32){uint32_t v=m->hooks.mmio_read32(m->hooks.user,a&~3u);return (uint8_t)(v>>(24-8*(a&3u)));}
    avp_sr_fault(m,a,"unmapped read8");return 0;
}
uint16_t avp_sr_read16(AvpSrMachine*m,uint32_t a){return (uint16_t)(((uint16_t)avp_sr_read8(m,a)<<8)|avp_sr_read8(m,a+1));}
uint32_t avp_sr_read32(AvpSrMachine*m,uint32_t a){
    if((a&0x00f00000u)==0x00f00000u&&m->hooks.mmio_read32&&!(a&3u))return m->hooks.mmio_read32(m->hooks.user,a);
    return ((uint32_t)avp_sr_read16(m,a)<<16)|avp_sr_read16(m,a+2);
}
void avp_sr_write8(AvpSrMachine*m,uint32_t a,uint8_t v){if(ram_ok(m,a,1)){m->ram[a]=v;return;}avp_sr_fault(m,a,"unmapped write8");}
void avp_sr_write16(AvpSrMachine*m,uint32_t a,uint16_t v){avp_sr_write8(m,a,(uint8_t)(v>>8));if(!m->cpu.faulted)avp_sr_write8(m,a+1,(uint8_t)v);}
void avp_sr_write32(AvpSrMachine*m,uint32_t a,uint32_t v){
    if((a&0x00f00000u)==0x00f00000u&&m->hooks.mmio_write32&&!(a&3u)){m->hooks.mmio_write32(m->hooks.user,a,v);return;}
    avp_sr_write16(m,a,(uint16_t)(v>>16));if(!m->cpu.faulted)avp_sr_write16(m,a+2,(uint16_t)v);
}
