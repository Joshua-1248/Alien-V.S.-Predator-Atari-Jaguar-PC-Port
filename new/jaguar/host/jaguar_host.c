#include "jaguar.h"
#include "mjp.h"
#include "joypad.h"
#include "jaguar_hw.h"
#ifndef _WIN32
#include <time.h>
#else
#include "pc/pc_platform.h"
#endif

volatile u32 jaguar_initial_ptr;
volatile u16 video_vcenter,video_hcenter,screen_half_width,screen_half_height;
volatile u16 top_half_line,top_line,bottom_half_line,bottom_line,video_phase;
volatile u32 one_second_ticks;
volatile u16 min_frame_ticks,video_mul_a,video_mul_b;

void avp_jaguar_write_lowmem32(uintptr_t address,u32 value){(void)address;(void)value;}
void InitJaguar(void){jaguar_initial_ptr=0x00009020u;initpad();}
void SetScreen(u32 width,u32 height){screen_half_width=(u16)(width>>1);screen_half_height=(u16)(height>>1);top_line=0;top_half_line=0;bottom_line=(u16)height;bottom_half_line=(u16)(height>>1);video_phase=0;}
void InitVideo(void){/* PC faithful baseline follows retail 60 Hz display timing while simulation stays 10 Hz. */video_vcenter=0x010c;video_hcenter=0x0337;video_mul_a=4;video_mul_b=4;one_second_ticks=60;min_frame_ticks=5;SetScreen(320,228);}
void InitMJP(void)
{
    /* The retail OP has two 0x400-byte list buffers and MJP edits its own
     * object records in those lists.  Host InitMJP must bind the matching
     * readable object array before Do_Title/Do_Selec/Do_Intro runs; leaving
     * it unbound silently disabled Make_TLi and all selection objects. */
    static AvpMjpObject frontend_objects[20];
    SetScreen(320,240);
    avp_mjp_set_lines((s32)(s16)top_line,(s32)(s16)top_line);
    avp_mjp_bind_objects(frontend_objects,
        (unsigned)(sizeof(frontend_objects)/sizeof(frontend_objects[0])));
}

/* PC-side reads for CPU code which historically sampled Jaguar MMIO.  The
 * only live PC-core user today is PLAYER.S::init_random, which samples two
 * timer words.  Use a monotonic host clock as the equivalent free-running
 * entropy source; all gameplay RNG evolution after the seed remains the exact
 * translated 68000 algorithm. */
/* All Motorola 68000 reads use big-endian byte lanes. Use one clock
 * snapshot per 32-bit transaction: otherwise a read spanning HC and VC can
 * combine values sampled on opposite sides of a half-line boundary. The
 * existing PC timer proxy is intentionally preserved until cycle-accurate
 * TOM timing is available. */
static u64 pc_mmio_timestamp(void)
{
#ifdef _WIN32
    return avp_pc_now_ns();
#else
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC,&ts);
    return (u64)ts.tv_sec*1000000000ull+(u64)ts.tv_nsec;
#endif
}
static u16 pc_mmio_word(uintptr_t address,u64 timestamp)
{
    if(address==0x00F00004u)return (u16)timestamp;
    if(address==0x00F00006u)return (u16)(timestamp>>16);
    if(address==JAG_CONFIG)return 0x0010u;
    return 0u;
}
u8 avp_host_mmio_read8(uintptr_t address)
{
    u16 word=pc_mmio_word(address&~(uintptr_t)1u,pc_mmio_timestamp());
    return (u8)((address&1u)?word:(word>>8));
}
u16 avp_host_mmio_read16(uintptr_t address)
{
    return pc_mmio_word(address,pc_mmio_timestamp());
}
u32 avp_host_mmio_read32(uintptr_t address)
{
    u64 n=pc_mmio_timestamp();
    return ((u32)pc_mmio_word(address,n)<<16)|pc_mmio_word(address+2u,n);
}
