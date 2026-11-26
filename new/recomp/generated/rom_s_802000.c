/*
 * Static recompilation of the active retail MAIN/ROM.S module.
 * Address/oracle: historical ROM.SYM/ROM.DTA reconstruction (176 bytes,
 * linked at $802000). This file models the original 68000 register/memory
 * effects and branches; it is not the readable-C gameplay reimplementation.
 */
#include "recomp/avp_recomp.h"

#define TRACE(pc_) do{m->cpu.pc=(pc_);if(m->hooks.trace_pc)m->hooks.trace_pc(m->hooks.user,&m->cpu,(pc_));}while(0)
static int call_abs(AvpSrMachine*m,uint32_t pc,uint32_t target){TRACE(pc);return avp_sr_call(m,target);}

int avp_sr_rom_s_802000(AvpSrMachine*m){
    AvpSrCpu*c=&m->cpu;
    /* BRA.W .start; first 14 bytes are the three historical service pointers. */
    TRACE(0x00802000u);
    TRACE(0x00802010u);
    c->d[0]=0x00814103u;                         /* move.l #DATAx,d0 */
    TRACE(0x00802016u);
    if(c->d[0] < 0x00815000u){                 /* cmp.l #ROM_FREE,d0; bls */
        TRACE(0x0080201eu);
    }else{avp_sr_fault(m,0x00802020u,"ROM.S DATAx exceeds ROM_FREE (ILLEGAL)");return -1;}

    TRACE(0x00802020u); avp_sr_write32(m,0x00f0210cu,0x00070007u); if(c->faulted)return -1;
    TRACE(0x0080202au); avp_sr_write32(m,0x00f02114u,8u); if(c->faulted)return -1;
    TRACE(0x00802034u); avp_sr_write32(m,0x00f02100u,0u); if(c->faulted)return -1;

    /* WAIT_BLIT: BTST #0,B_CMD. PC backend returns idle unless a host blitter is busy. */
    for(unsigned spin=0;;++spin){
        TRACE(0x0080203eu); c->d[7]=avp_sr_read32(m,0x00f02238u);if(c->faulted)return -1;
        TRACE(0x00802044u);
        if(!(c->d[7]&1u))break;
        if(spin>1000000u){avp_sr_fault(m,0x00f02238u,"WAIT_BLIT timeout");return -1;}
    }
    TRACE(0x00802048u); avp_sr_write32(m,0x00f02208u,0u);if(c->faulted)return -1;

    TRACE(0x00802052u); c->a[7]=0x00007548u;    /* lea STACK,a7 */
    TRACE(0x00802056u); c->d[0]=0x00007fe8u;    /* rombsslo/end_bss are equal in retail map */
    TRACE(0x0080205cu);
    if(c->d[0] > 0x00007fe8u){avp_sr_fault(m,0x00802064u,"ROM.S BSS ordering failure (TST/ILLEGAL)");return -1;}

    TRACE(0x00802064u); avp_sr_write32(m,0x00007fe8u,0x001e0000u);if(c->faulted)return -1; /* _alloc_buf */
    if(call_abs(m,0x0080206cu,0x008022ccu))return -1; /* _init_alloc */
    if(call_abs(m,0x00802070u,0x008030e6u))return -1; /* _init_inflate */
    TRACE(0x00802074u); if((int16_t)(c->d[0]&0xffffu)!=0){avp_sr_fault(m,0x00802078u,"init_inflate returned error");return -1;}

    TRACE(0x00802078u); avp_sr_write32(m,0x00007ff4u,0x00803804u);if(c->faulted)return -1; /* _inptr=TEXT */
    TRACE(0x00802080u); avp_sr_write32(m,0x00007ff8u,0x00008000u);if(c->faulted)return -1; /* _outptr=RAM_START */
    if(call_abs(m,0x00802088u,0x00803112u))return -1; /* inflate text */
    TRACE(0x0080208cu); if((int16_t)(c->d[0]&0xffffu)!=0){avp_sr_fault(m,0x008020a0u,"TEXT inflate failed");return -1;}

    TRACE(0x00802090u); avp_sr_write32(m,0x00007ff4u,0x0080e842u);if(c->faulted)return -1; /* _inptr=DATA */
    if(call_abs(m,0x00802098u,0x00803112u))return -1; /* inflate data */
    TRACE(0x0080209cu); if((int16_t)(c->d[0]&0xffffu)!=0){avp_sr_fault(m,0x008020a0u,"DATA inflate failed");return -1;}

    /* JMP RAM_START: hand control to the RAM-resident retail program. */
    TRACE(0x008020a4u); c->pc=0x00008000u;
    return 0;
}
