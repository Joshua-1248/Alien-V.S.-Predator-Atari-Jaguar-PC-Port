#include "recomp/avp_recomp.h"
int avp_sr_call(AvpSrMachine*m,uint32_t target){
    if(m->cpu.faulted)return -1;
    if(target==0x00802000u)return avp_sr_rom_s_802000(m);
    if(m->hooks.call_external)return m->hooks.call_external(m->hooks.user,&m->cpu,target);
    avp_sr_fault(m,target,"no statically recompiled block or external hook");return -1;
}
