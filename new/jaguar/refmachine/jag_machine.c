/* RE24-S10 Jaguar reference machine.  See include/jaguar/jag_machine.h.
 * Register semantics: Atari Jaguar Technical Reference Manual Rev.8
 * (TOM pp.9-24, GPU pp.29-63, Blitter pp.64-82, JERRY pp.83-96, DSP pp.97-111).
 *
 * Clocking (NTSC): system clock 26.590906 MHz; the 68000 runs at half of it
 * (p.84 "This clock is divided by two to provide a clock for an external
 * processor").  The video clock is taken equal to the system clock, which is
 * how the NTSC console derives both; HP+1 video clocks per half line (p.14).
 * Scheduling is interleaved in short slices; bus contention is not modelled.
 *
 * Boot state: no boot ROM is executed.  The cartridge is entered at the
 * address in its header ($800404) in supervisor mode.  The cartridge code
 * sets its own stack, GPU/Blitter registers and inflates the program. */
#include "jaguar/jag_machine.h"
#include "jaguar/cry.h"
#include "m68k.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYS_HZ 26590906.0

static JagMachine *M; /* Musashi is single-instance */
/* optional write watch (diagnostic, env JAGM_WATCH=lo,hi[,max]) */
static int master; /* 0=68000 1=GPU 2=DSP 3=Blitter 4=OP */
/* Only the 68000 callback path has an instruction-progress counter from
 * Musashi. For its HC reads, account for elapsed 68000 cycles within the
 * current short scheduler slice (two pixel/system clocks per CPU cycle).
 * GPU/DSP and arbiter beam phases are NOT cycle-exact in this model. */
static int cpu_beam_active;

static uint32_t watch_lo=1,watch_hi=0;static long watch_max=0;
static void watch_init(void){const char *e=getenv("JAGM_WATCH");if(e){unsigned long a,b;long n=200;if(sscanf(e,"%lx,%lx,%ld",&a,&b,&n)>=2){watch_lo=(uint32_t)a;watch_hi=(uint32_t)b;watch_max=n;}}}
static void watch_hit(JagMachine *m,uint32_t a,uint32_t v,int w){
    static const char *nm[]={"68K","GPU","DSP","BLT","OP"};
    if(a<watch_lo||a>watch_hi||watch_max<=0)return;
    watch_max--;
    fprintf(stderr,"W %s f=%llu vc=%u a=%06x v=%0*x pc68=%06x gpc=%06x bcmd=%08x a1=%06x a2=%06x a2p=%08x\n",nm[master],(unsigned long long)m->frames,m->vc,a,w*2,v,m68k_get_reg(NULL,M68K_REG_PC),m->gpu.pc,m->blit.cmd,m->blit.a1_base,m->blit.a2_base,m->blit.a2_pixel);}

static uint16_t be16(const uint8_t *p){return (uint16_t)((p[0]<<8)|p[1]);}
static uint32_t be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}

static void note_unhandled(JagMachine *m,uint32_t a,int write)
{
    unsigned i;
    for(i=0;i<m->unhandled_n;i++)if(m->unhandled[i].addr==a){if(write)m->unhandled[i].writes++;else m->unhandled[i].reads++;return;}
    if(m->unhandled_n<256){m->unhandled[m->unhandled_n].addr=a;m->unhandled[m->unhandled_n].reads=!write;m->unhandled[m->unhandled_n].writes=!!write;m->unhandled_n++;}
}

/* ------------------------------------------------------------------ IRQ */
static void update_cpu_irq(JagMachine *m)
{
    /* TOM INT1 (p.16): pending & enabled -> 68000 interrupt.  TOM drives a
       single level; the Jaguar uses IPL level 2 for TOM interrupts. */
    if(m->gpu.cpu_irq){m->int1_pending|=2u;m->gpu.cpu_irq=0;}
    if(m->dsp.cpu_irq){m->dsp.cpu_irq=0;m->jint_pending|=2u;}
    /* Rev.8 TOM INT1 bit 4: the Jerry input is active-high and
     * EDGE-triggered.  Its source latches live in JERRY JINTCTRL and must
     * remain pending until separately acknowledged there.  Merely clearing
     * TOM INT1 bit 12 must not destroy Jerry's interrupt state, or re-latch
     * TOM while the input stays high.  Detect a genuine low->high change. */
    {
        uint8_t high=(uint8_t)((m->jint_pending&m->jint_enable&0x3Fu)!=0u);
        if(high&&!m->jerry_cpu_irq_high&&(m->int1_enable&16u))
            m->int1_pending|=16u;
        m->jerry_cpu_irq_high=high;
    }
    m68k_set_irq((m->int1_pending&m->int1_enable&0x1Fu)?2:0);
}
unsigned long jagm_irq_acks;
int jagm_int_ack(int level){(void)level;jagm_irq_acks++;return 64;} /* bug 7: vector $40 */

/* ------------------------------------------------------------- memory */
static uint32_t ram_off(uint32_t a){return a&(JAGM_RAM_SIZE-1u);} /* 2 MiB repeats in DRAM0 */

static int in_gpu(uint32_t a){return a>=0xF02000u&&a<0xF04000u;}
static int in_dsp(uint32_t a){return a>=0xF1A000u&&a<0xF1E000u;}

static void op_resume_after_obf(JagMachine *m);
static uint16_t tom_read16(JagMachine *m,uint32_t a);
static void tom_write16(JagMachine *m,uint32_t a,uint16_t v);
static uint16_t jerry_read16(JagMachine *m,uint32_t a);
static void blit_bus_sync(JagMachine *m);
static void jerry_write16(JagMachine *m,uint32_t a,uint16_t v);

/* 32-bit internal locations (GPU/DSP/Blitter space) viewed through 16 bits:
 * "transfers should always be performed in pairs, in the order low address
 * then high address" (p.31). */
static uint32_t local32_read(JagMachine *m,uint32_t a)
{
    uint32_t v=0;
    a&=~3u;
    if(in_gpu(a)){
        if(a>=0xF02200u&&a<0xF022A0u){
            blit_bus_sync(m);
            (void)jag_blitter_read(&m->blit,a-0xF02200u,&v);
            if(a==0xF02238u&&m->on_blit_status_read){
                JagBlitStatusReadEvent e;
                memset(&e,0,sizeof e);
                e.field=m->frames;
                e.sys_cycles=m->sys_cycles;
                e.gpu_steps_completed=m->gpu_steps_completed;
                e.gpu_model_ticks_completed=m->gpu_model_ticks_completed;
                e.issuer_pc=master==1&&m->gpu.executing?m->gpu.pc-2u:
                            (master==0?m68k_get_reg(NULL,M68K_REG_PC):0u);
                e.status=v;e.cmd=m->blit.cmd;
                e.master=(uint8_t)master;
                e.stopped=(uint8_t)m->blit.stopped;
                e.gpu_irq_latched=(uint8_t)((m->gpu.latches>>4)&1u);
                m->on_blit_status_read(m,&e,m->on_blit_status_read_ctx);
            }
            return v;
        }
        if(!jrisc_local_read(&m->gpu,a,&v))note_unhandled(m,a,0);
        return v;
    }
    if(in_dsp(a)){
        if(a>=0xF1A140u&&a<0xF1A160u){
            /* V10 JERRY SSTAT $F1A150 is read-only and shares SCLK's
             * write address. Bit 0 reflects the generated word-strobe pin.
             * No external serial input is wired to this reference machine;
             * other SSTAT status bits remain unmodelled. */
            return a==0xF1A150u?(uint32_t)m->i2s_ws:0u;
        }
        if(!jrisc_local_read(&m->dsp,a,&v))note_unhandled(m,a,0);
        return v;
    }
    return 0;
}
static void local32_write(JagMachine *m,uint32_t a,uint32_t v)
{
    a&=~3u;
    if(in_gpu(a)){
        if(a>=0xF02200u&&a<0xF022A0u){
            /* Completion is not synonymous with writing B_CMD.  NOGO never
             * starts a transfer, while B_STOP may suspend a transfer before
             * its final pixel.  GPU interrupt 4 is raised on completion of
             * the default synchronous model, including a final B_STOP RESUME.
             * Experimental R286 mode defers the interrupt until its final
             * scheduled destination group has been processed.
             * ABORT terminates rather than completing the transfer.
             * This does not attempt to model interrupt/bus-cycle latency. */
            const int was_stopped=m->blit.stopped;
            const int start=(a==0xF02238u)&&!(v&0x80u)&&!was_stopped;
            const int resume=(a==0xF02278u)&&was_stopped&&(v&1u)&&!(v&2u);
            const int observe=m->on_blit_mmio!=NULL&&
                              (a==0xF02238u||a==0xF02278u);
            JagBlitMmioEvent event;
            if(observe){
                memset(&event,0,sizeof event);
                event.field=m->frames;
                event.cycles_before=m->sys_cycles;
                event.addr=a; event.value=v;
                event.cmd_before=m->blit.cmd;
                event.master=(uint8_t)master;
                event.stopped_before=(uint8_t)was_stopped;
                event.gpu_irq_before=(uint8_t)((m->gpu.latches>>4)&1u);
                event.blits_before=m->blit.blits;
                event.pixels_written_before=m->blit.pixels_written;
                event.estimated_ticks_before=m->blit.est_ticks;
                event.gpu_steps_completed=m->gpu_steps_completed;
                event.gpu_model_ticks_completed=m->gpu_model_ticks_completed;
                event.reads_before=m->blit_read_requests;
                event.writes_before=m->blit_write_requests;
                for(unsigned region=0;region<JAGM_BLIT_REGION_COUNT;region++){
                    event.region_reads_before[region]=m->blit_region_reads[region];
                    event.region_writes_before[region]=m->blit_region_writes[region];
                }
                event.issuer_pc=master==1&&m->gpu.executing?
                                  (m->gpu.pc-2u):(master==0?
                                  m68k_get_reg(NULL,M68K_REG_PC):0u);
            }
            blit_bus_sync(m);
            if(getenv("JAGM_R286_TRACE")&&a==0xF02238u)
                fprintf(stderr,"R286 CMD f=%llu cpu=%06x value=%08x cnt=%08x before running=%d stopped=%d blits=%llu\n",
                    (unsigned long long)m->frames,m68k_get_reg(NULL,M68K_REG_PC),v,m->blit.count,
                    m->blit.running,m->blit.stopped,(unsigned long long)m->blit.blits);
            { int sv=master; if(a==0xF02238u)master=3; (void)jag_blitter_write(&m->blit,a-0xF02200u,v); master=sv; }
            /* R287: a new progressive transfer can clear the very DRAM
             * stack from which the 68000 is about to return. Retire its
             * issuing instruction at the bus grant, rather than continuing
             * the rest of this CPU timeslice against in-flight Blitter RAM
             * writes. Guest-initiated MMIO is distinguished from host test
             * setup by cpu_beam_active. No effect in synchronous mode. */
            if(cpu_beam_active && a==0xF02238u &&
               m->blit.async_mode && m->blit.running)
                m68k_end_timeslice();
            if((start||resume)&&!m->blit.stopped&&!m->blit.running)
                jrisc_raise(&m->gpu,4); /* completed transfer -> GPU IRQ4 */
            if(observe){
                event.cycles_after=m->sys_cycles;
                event.cmd_after=m->blit.cmd;
                event.stopped_after=(uint8_t)m->blit.stopped;
                event.gpu_irq_after=(uint8_t)((m->gpu.latches>>4)&1u);
                event.blits_after=m->blit.blits;
                event.pixels_written_after=m->blit.pixels_written;
                event.estimated_ticks_after=m->blit.est_ticks;
                event.reads_after=m->blit_read_requests;
                event.writes_after=m->blit_write_requests;
                for(unsigned region=0;region<JAGM_BLIT_REGION_COUNT;region++){
                    event.region_reads_after[region]=m->blit_region_reads[region];
                    event.region_writes_after[region]=m->blit_region_writes[region];
                }
                m->on_blit_mmio(m,&event,m->on_blit_mmio_ctx);
            }
            return;
        }
        if(getenv("JAGM_TRACE_GPU")&&(a==0xF02110u||a==0xF02114u))fprintf(stderr,"GPU %s %08x (master=%d 68k pc=%06x gpu pc=%06x f=%llu)\n",a==0xF02110u?"PC":"CTRL",v,master,m68k_get_reg(NULL,M68K_REG_PC),m->gpu.pc,(unsigned long long)m->frames);
        if(watch_max>0)watch_hit(m,a,v,4);
        if(m->on_gpu_control&&(a==0xF02110u||a==0xF02114u)){
            JagGpuControlEvent e;
            memset(&e,0,sizeof e);
            e.field=m->frames;e.cycles=m->sys_cycles;e.vc=m->vc;
            e.addr=a;e.value=v;e.pc_before=m->gpu.pc;
            e.go_before=(uint8_t)m->gpu.go;e.master=(uint8_t)master;
            e.pc68=master==0?m68k_get_reg(NULL,M68K_REG_PC):0;
            if(!jrisc_local_write(&m->gpu,a,v))note_unhandled(m,a,1);
            e.pc_after=m->gpu.pc;e.go_after=(uint8_t)m->gpu.go;
            m->on_gpu_control(m,&e,m->on_gpu_control_ctx);
        }else if(!jrisc_local_write(&m->gpu,a,v))note_unhandled(m,a,1);
        return;
    }
    if(in_dsp(a)){
        switch(a){
        case 0xF1A140u:m->dac_l=(int16_t)v;return; /* DAC1/DAC2 (p.89) */
        case 0xF1A144u:m->dac_r=(int16_t)v;return;
        case 0xF1A148u:m->ltxd=(uint16_t)v;return; /* LTXD/RTXD (p.91) */
        case 0xF1A14Cu:m->rtxd=(uint16_t)v;return;
        case 0xF1A150u:
            /* V10 JERRY SCLK is an 8-bit divider.  High bits written by
             * 68000 or DSP must not slow the physical bit clock. */
            m->sclk=(uint16_t)(v&0x00FFu);return;
        case 0xF1A154u:
            m->smode=(uint16_t)v;
            /* INTERNAL=0 tristates the generated WS output. Without
             * externally supplied serial pins, the observed pin is low. */
            if(!(m->smode&1u))m->i2s_ws=0u;
            return;
        default:break;
        }
        if(!jrisc_local_write(&m->dsp,a,v))note_unhandled(m,a,1);
        update_cpu_irq(m);
        return;
    }
}

static int is_local32(uint32_t a)
{ return in_gpu(a)||in_dsp(a)||(a>=0xF0A000u&&a<0xF0C000u); }

/* TRM Rev.8 pp.59-60, 109-110: DORG bit 0 maps the CPU I/O 16-bit
 * word lanes of each 32-bit GPU/DSP register. BIG_IO=0 puts the low
 * word at the lower 68000 address, BIG_IO=1 puts the high word there.
 * This does not reorder the 32-bit RISC operand itself or its RAM bytes.
 * The GPU's I/O port also covers Blitter registers and local RAM. */
static int io_big_word_first(const JagMachine *m,uint32_t a)
{
    a&=0xFFFFFFu;
    if(in_gpu(a)|| (a>=0xF0A000u&&a<0xF0C000u))
        return (m->gpu.bigend&1u)!=0u;
    if(in_dsp(a))return (m->dsp.bigend&1u)!=0u;
    return 1;
}

/* TOM Rev.8 pp.17-18: the line-buffer windows have additional 32-bit
 * WRITE-ONLY aliases at +0x8000.  A/B are independent physical banks;
 * the third alias tracks whichever bank the OP is currently writing.
 * Each physical buffer holds 360 32-bit words = 0x5a0 bytes.
 * Do not make reads from these addresses alias the normal 16-bit view. */
static int lbuf_32write_alias(JagMachine *m,uint32_t a,
                              unsigned *bank,unsigned *byte_offset)
{
    static const uint32_t starts[3]={0xF08800u,0xF09000u,0xF09800u};
    for(unsigned i=0u;i<3u;i++){
        if(a>=starts[i]&&a-starts[i]<0x5A0u){
            *bank=(i==2u)?m->lbuf_write:i;
            *byte_offset=(unsigned)(a-starts[i]);
            return 1;
        }
    }
    return 0;
}
/* The Blitter's current synchronous adapter breaks each pixel request into
 * byte callbacks.  Recombine those bytes into the physical 16-bit slots of
 * the 32-bit alias; this is DATA fidelity, not a model of real 32-bit bus
 * grants, atomicity or arbitration. */
static int lbuf_blit_alias_byte(JagMachine *m,uint32_t a,uint8_t v)
{
    unsigned bank,offset;
    if(!lbuf_32write_alias(m,a,&bank,&offset))return 0;
    uint16_t *word=&m->lbuf[bank][offset>>1];
    if(offset&1u)*word=(uint16_t)((*word&0xFF00u)|v);
    else *word=(uint16_t)((*word&0x00FFu)|((uint16_t)v<<8));
    return 1;
}

uint16_t jagm_read16(JagMachine *m,uint32_t a)
{
    a&=0xFFFFFEu;
    if(a<0x800000u)return be16(m->ram+ram_off(a));
    if(a<0xE00000u){uint32_t o=a-0x800000u;return o+1u<m->rom_size?be16(m->rom+o):0xFFFFu;}
    if(a>=0xF00000u&&a<0xF10000u){
        if(in_gpu(a)){
            uint32_t l=local32_read(m,a);
            const int high_word=io_big_word_first(m,a)?!(a&2u):!!(a&2u);
            return (uint16_t)(high_word?(l>>16):l);
        }
        return tom_read16(m,a);
    }
    if(a>=0xF10000u&&a<0xF20000u){
        if(in_dsp(a)){
            uint32_t l=local32_read(m,a);
            const int high_word=io_big_word_first(m,a)?!(a&2u):!!(a&2u);
            return (uint16_t)(high_word?(l>>16):l);
        }
        return jerry_read16(m,a);
    }
    note_unhandled(m,a,0);
    return 0xFFFFu;
}
uint8_t jagm_read8(JagMachine *m,uint32_t a)
{
    uint16_t w;
    a&=0xFFFFFFu;
    if(a<0x800000u)return m->ram[ram_off(a)];
    if(a<0xE00000u){uint32_t o=a-0x800000u;return o<m->rom_size?m->rom[o]:0xFFu;}
    w=jagm_read16(m,a&~1u);
    return (uint8_t)((a&1u)?w:(w>>8));
}
uint32_t jagm_read32(JagMachine *m,uint32_t a)
{
    a&=0xFFFFFEu;
    if(is_local32(a)&&!(a&2u))return local32_read(m,a);
    return ((uint32_t)jagm_read16(m,a)<<16)|jagm_read16(m,a+2u);
}
void jagm_write16(JagMachine *m,uint32_t a,uint16_t v)
{
    a&=0xFFFFFEu;
    if(a<0x800000u){uint8_t *p=m->ram+ram_off(a);p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;if(watch_max>0)watch_hit(m,a,v,2);return;}
    if(a<0xE00000u)return; /* ROM */
    if(a>=0xF0A000u&&a<0xF0C000u)a-=0x8000u; /* 32-bit write alias, here written as halves */
    if(is_local32(a)){
        /* CPU I/O is a pair of 16-bit bus cycles in address order.
         * The BIG_IO latch selects which physical 32-bit half belongs
         * to each CPU word. Commit control registers on the second
         * half; local RAM remains directly writable per 16-bit word. */
        const int high_first=io_big_word_first(m,a);
        const int write_high=high_first?!(a&2u):!!(a&2u);
        const uint32_t insert=write_high?((uint32_t)v<<16):(uint32_t)v;
        const uint32_t keep=write_high?0x0000FFFFu:0xFFFF0000u;
        if(!(a&2u)){
            m->hold_addr=a;m->hold_val=insert;m->hold_valid=1;
            if((a>=0xF03000u&&a<0xF04000u)||
               (a>=0xF1B000u&&a<0xF1D000u)){
                uint32_t old=local32_read(m,a);
                local32_write(m,a,(old&keep)|insert);
                m->hold_valid=0;
            }
            return;
        }
        if(m->hold_valid&&m->hold_addr==(a&~3u)){
            local32_write(m,a&~3u,m->hold_val|insert);
            m->hold_valid=0;
            return;
        }
        { uint32_t old=local32_read(m,a&~3u);
          local32_write(m,a&~3u,(old&keep)|insert); }
        return;
    }
    if(a>=0xF00000u&&a<0xF10000u){tom_write16(m,a,v);return;}
    if(a>=0xF10000u&&a<0xF20000u){jerry_write16(m,a,v);return;}
    note_unhandled(m,a,1);
}
void jagm_write8(JagMachine *m,uint32_t a,uint8_t v)
{
    a&=0xFFFFFFu;
    /* R151: generic CPU/GPU byte stores are not Blitter bus requests.
     * Count a Blitter byte write only in blit_wr8, the actual callback
     * installed on the Blitter bus. The old increment here also counted
     * unrelated byte stores whenever MMIO tracing happened to be enabled. */
    if(a<0x800000u){m->ram[ram_off(a)]=v;if(watch_max>0)watch_hit(m,a,v,1);return;}
    if(a<0xE00000u)return;
    if(a>=0xF0A000u&&a<0xF0C000u)a-=0x8000u; /* 32-bit write alias of GPU space */
    { uint16_t w=jagm_read16(m,a&~1u);
      w=(a&1u)?(uint16_t)((w&0xFF00u)|v):(uint16_t)((w&0x00FFu)|((uint16_t)v<<8));
      jagm_write16(m,a&~1u,w); }
}
void jagm_write32(JagMachine *m,uint32_t a,uint32_t v)
{
    unsigned bank,off;
    a&=0xFFFFFEu;
    /* A 32-bit WRITE through TOM's +8000 line-buffer port. The normal
     * 16-bit A/B/current windows and all alias reads are unchanged. */
    if(!(a&3u)&&lbuf_32write_alias(m,a,&bank,&off)&&off<=0x59Cu){
        m->lbuf[bank][off>>1]=(uint16_t)(v>>16);
        m->lbuf[bank][(off>>1)+1u]=(uint16_t)v;
        return;
    }
    if(a>=0xF0A000u&&a<0xF0C000u)a-=0x8000u;
    if(is_local32(a)&&!(a&2u)){local32_write(m,a,v);return;}
    jagm_write16(m,a,(uint16_t)(v>>16));jagm_write16(m,a+2u,(uint16_t)v);
}

/* The caller is an original JRISC or Blitter bus transaction. This
 * instrument stays outside the normal DRAM accessors, so CPU reads, Object
 * Processor fetches, and read-modify-write helpers are not misclassified.
 * It never reads additional machine memory and cannot change bus results. */
static void view_access(JagMachine *m,uint32_t a,unsigned width,int write,unsigned actor)
{
    JagGpuViewDramAccess e;
    a&=0xFFFFFFu;
    if(!m->on_gpu_view_dram_access||a>=0x800000u||(actor!=1u&&actor!=3u))return;
    e.field=m->frames;e.address=a;
    /* JRISC increments PC before executing LOAD/STORE; recover the actual
     * instruction address when the GPU is executing this request. */
    e.gpu_pc=m->gpu.executing?m->gpu.pc-2u:m->gpu.pc;
    e.master=(uint8_t)actor;e.width=(uint8_t)width;e.is_write=(uint8_t)(write!=0);
    m->on_gpu_view_dram_access(m,&e,m->on_gpu_view_dram_access_ctx);
}
/* bus for the GPU/DSP (JRisc) and the blitter */
static uint32_t risc_read(void *c,uint32_t a,int w)
{
    JagMachine *m=(JagMachine*)c;
    uint32_t v;
    if(w==1)v=jagm_read8(m,a);
    else if(w==2)v=jagm_read16(m,a);
    else v=jagm_read32(m,a);
    view_access(m,w==1?a:(a&~1u),(unsigned)w,0,m->gpu.executing?1u:2u);
    return v;
}
static void risc_write(void *c,uint32_t a,uint32_t v,int w)
{
    JagMachine *m=(JagMachine*)c;
    if(w==1)jagm_write8(m,a,(uint8_t)v);
    else if(w==2)jagm_write16(m,a,(uint16_t)v);
    else jagm_write32(m,a,v);
    view_access(m,w==1?a:(a&~1u),(unsigned)w,1,m->gpu.executing?1u:2u);
}
/* Classify one *requested* Jaguar bus byte address. Address mapping follows
 * jagm_read8/write8: 0..7FFFFF DRAM mirror, 800000..DFFFFF cart,
 * E00000..FFFFFF remaining MMIO/unmapped. The upper address bits are
 * ignored by the 24-bit external address decoder. */
static unsigned blit_memory_region(uint32_t a)
{
    a&=0xFFFFFFu;
    return a<0x800000u?JAGM_BLIT_DRAM:
           (a<0xE00000u?JAGM_BLIT_CART:JAGM_BLIT_OTHER);
}
/* R263: GPU and DSP local instruction RAM are byte-addressable by the
 * Blitter. A generic byte read/modify/write through the CPU-facing BIG_IO
 * 16-bit lane mapping can swap neighboring DSP RAM bytes (R153 regression).
 * Use the physical local RAM bytes only for the documented RAM windows.
 * Control registers, ROM, aliases' read semantics and the watch-enabled
 * fallback remain unchanged; keep Blitter bus/provenance observers intact. */
static uint8_t *blit_local_ram_byte(JagMachine *m,uint32_t a,int for_write)
{
    a&=0xFFFFFFu;
    if(a>=0xF03000u&&a<0xF04000u)return m->gpu.ram+(a-0xF03000u);
    if(for_write&&a>=0xF0B000u&&a<0xF0C000u)
        return m->gpu.ram+(a-0xF0B000u);
    if(a>=0xF1B000u&&a<0xF1D000u)
        return m->dsp.ram+(a-0xF1B000u);
    return NULL;
}
static uint8_t blit_rd8(void *c,uint32_t a)
{
    JagMachine *m=(JagMachine*)c;
    uint8_t *local=blit_local_ram_byte(m,a,0);
    uint8_t v=local?*local:jagm_read8(m,a);
    if(m->on_blit_mmio){
        m->blit_read_requests++;
        m->blit_region_reads[blit_memory_region(a)]++;
    }
    view_access(m,a,1u,0,3u);
    return v;
}
static void blit_wr8(void *c,uint32_t a,uint8_t v)
{
    JagMachine *m=(JagMachine*)c;
    a&=0xFFFFFFu;
    if(m->on_blit_mmio){
        m->blit_write_requests++;
        m->blit_region_writes[blit_memory_region(a)]++;
    }
    if(a<0x800000u){m->ram[ram_off(a)]=v;if(watch_max>0)watch_hit(m,a,v,1);view_access(m,a,1u,1,3u);return;}
    if(lbuf_blit_alias_byte(m,a,v))return;
    /* Preserve the legacy register-watch instrumentation if active, as its
     * reported word accesses are part of the existing diagnostic contract. */
    if(watch_max<=0){
        uint8_t *local=blit_local_ram_byte(m,a,1);
        if(local){*local=v;return;}
    }
    /* MMIO/control registers and watch-enabled local RAM retain the
     * existing generic byte RMW implementation, including its side effects. */
    jagm_write8(m,a,v);
}
/* R286: the reference machine can retire real destination transfer groups
 * over successive scheduler slices, rather than executing an entire blit
 * atomically on B_CMD. A group costs two estimated ticks, as in the legacy
 * Blitter counter. This is a scheduling experiment, NOT cycle-exact TOM
 * arbitration; the 68000 and GPU still run sequentially within each slice.
 * Reads/writes to Blitter MMIO flush any elapsed group credit first. */
static void blit_bus_sync(JagMachine *m)
{
    unsigned groups;
    if(!m->blit.running){m->blit_tick_credit=0u;return;}
    groups=m->blit_tick_credit/2u;
    if(!groups)return;
    m->blit_tick_credit%=2u;
    if(jag_blitter_advance(&m->blit,groups)){
        jrisc_raise(&m->gpu,4);
        m->blit_tick_credit=0u;
    }
    if(!m->blit.running)m->blit_tick_credit=0u;
}
/* diagnostic: JAGM_BLIT_LOG=file writes every blit's registers */
static FILE *blit_log;
static void blit_logger(void *ctx,const JagBlitter *b)
{
    JagMachine *m=(JagMachine*)ctx;
    fprintf(blit_log,"%llu %u cmd=%08x cnt=%08x a1b=%06x a1f=%08x a1p=%08x a1fp=%08x a1i=%08x a1fi=%08x a1s=%08x a1fs=%08x clip=%08x a2b=%06x a2f=%08x a2p=%08x a2s=%08x iinc=%08x\n",
        (unsigned long long)m->frames,m->vc,b->cmd,b->count,b->a1_base,b->a1_flags,b->a1_pixel,b->a1_fpixel,b->a1_inc,b->a1_finc,b->a1_step,b->a1_fstep,b->a1_clip,b->a2_base,b->a2_flags,b->a2_pixel,b->a2_step,b->iinc);
}

/* ------------------------------------------------------------- TOM regs */
static uint16_t tom_read16(JagMachine *m,uint32_t a)
{
    switch(a){
    case 0xF00000u:return m->memcon1;
    case 0xF00002u:return m->memcon2;
    case 0xF00004u:{
        unsigned hc=m->hc_half;
        if(master==0&&cpu_beam_active){
            unsigned period=(unsigned)m->hp+1u;
            /* Musashi counts 68000 cycles; TOM's pixel clock is twice the
             * 68000 clock. Clamp an instruction crossing the half-line
             * boundary until the scheduler enters the next half-line. */
            unsigned elapsed=(unsigned)m68k_cycles_run()*2u;
            unsigned pos=(hc&0x3ffu)+elapsed;
            if(pos>=period)pos=period-1u;
            hc=(hc&0x400u)|(pos&0x3ffu);
        }
        return (uint16_t)hc;
    }
    case 0xF00006u:
        /* Rev.8 p.12: VC bit 11 is the current field selector. The low
         * eleven bits continue to count half lines as before. */
        return (uint16_t)((m->vc&0x7ffu)|((m->frames&1u)<<11));
    /* TOM OB0..OB3 are bus-word ordered, NOT phrase-MSB-first. Original
     * Flare TOM netlist OB.NET maps DR[0:15] to OB0, DR[16:31] to OB1,
     * DR[32:47] to OB2, and DR[48:63] to OB3. The addresses are still
     * big-endian within each 16-bit word. STOP and GPU object payloads
     * use this same latch, regardless of payload bit patterns. */
    case 0xF00010u:return (uint16_t)m->ob;
    case 0xF00012u:return (uint16_t)(m->ob>>16);
    case 0xF00014u:return (uint16_t)(m->ob>>32);
    case 0xF00016u:return (uint16_t)(m->ob>>48);
    case 0xF000E0u:return m->int1_pending&0x1Fu; /* pending sources */
    default:break;
    }
    if(a>=0xF00400u&&a<0xF00800u)return m->clut[((a-0xF00400u)>>1)&255u];
    if(a>=0xF00800u&&a<0xF01D9Eu+2u){
        unsigned bank,idx;
        if(a<0xF01000u){bank=0;idx=(a-0xF00800u)>>1;}
        else if(a<0xF01800u){bank=1;idx=(a-0xF01000u)>>1;}
        else{bank=m->lbuf_write;idx=(a-0xF01800u)>>1;}
        return idx<720u?m->lbuf[bank][idx]:0u;
    }
    note_unhandled(m,a,0);
    return 0;
}
static void tom_write16(JagMachine *m,uint32_t a,uint16_t v)
{
    switch(a){
    case 0xF00000u:m->memcon1=v;return;
    case 0xF00002u:m->memcon2=v;return;
    case 0xF00004u:return; case 0xF00006u:return;  /* test only */
    case 0xF00020u:if(getenv("JAGM_TRACE_OLP"))fprintf(stderr,"OLP hi %04x pc=%06x\n",v,m68k_get_reg(NULL,M68K_REG_PC));m->olp=(m->olp&0xFFFF0000u)|v;return; /* OLP word order: F00020 carries address bits 15..0 (the cartridge writes $75480000 for list $7548; R222 audit) */
    case 0xF00022u:if(getenv("JAGM_TRACE_OLP"))fprintf(stderr,"OLP lo %04x pc=%06x\n",v,m68k_get_reg(NULL,M68K_REG_PC));m->olp=(m->olp&0x0000FFFFu)|((uint32_t)v<<16);return;
    case 0xF00026u:
        m->obf=v;
        m->op_waiting_for_obf=0u; /* any value releases GPUOBJ */
        /* When the GPU writes OBF during the synchronous object pass, that
         * pass resumes itself on return.  A later 68000/GPU write must
         * instead resume at the saved next-phrase address, not re-read OLP.
         * This models list control flow, NOT exact beam/bus timing. */
        if(m->op_resume_valid&&!m->op_processing)op_resume_after_obf(m);
        return;
    case 0xF00028u:m->vmode=v;return;
    case 0xF0002Au:m->bord1=v;return; case 0xF0002Cu:m->bord2=v;return;
    case 0xF0002Eu:m->hp=v&0x3FFu;return; case 0xF00030u:m->hbb=v;return;
    case 0xF00032u:m->hbe=v;return; case 0xF00034u:m->hs=v;return;
    case 0xF00036u:m->hvs=v;return; case 0xF00038u:m->hdb1=v;return;
    case 0xF0003Au:m->hdb2=v;return; case 0xF0003Cu:m->hde=v;return;
    case 0xF0003Eu:m->vp=v&0x7FFu;return; case 0xF00040u:m->vbb=v;return;
    case 0xF00042u:m->vbe=v;return; case 0xF00044u:m->vs=v;return;
    case 0xF00046u:m->vdb=v&0x7FFu;return;
    case 0xF00048u:
        /* Software Reference V10, TOM video control: VDE=$FFFF is the
         * console-specific all-lines OP workaround. Preserve this one
         * sentinel instead of masking it to the ordinary 11-bit 0x7FF
         * comparator; all other writes still use the 11-bit field. */
        m->vde=(v==0xFFFFu)?0xFFFFu:(v&0x7FFu);return;
    case 0xF0004Au:m->veb=v;return; case 0xF0004Cu:m->vee=v;return;
    case 0xF0004Eu:m->vi=v&0x7FFu;return;
    case 0xF00050u:m->pit0=v;m->pit_acc=0;return; case 0xF00052u:m->pit1=v;m->pit_acc=0;return;
    case 0xF00054u:m->heq=v;return; case 0xF00058u:m->bg=v;return;
    case 0xF000E0u: /* INT1: enables bits 0-4, clear bits 8-12 */
        m->int1_enable=v&0x1Fu;m->int1_pending&=(uint16_t)~((v>>8)&0x1Fu);
        update_cpu_irq(m);return;
    case 0xF000E2u:return; /* INT2: bus priority restore (no contention model) */
    default:break;
    }
    if(a>=0xF00400u&&a<0xF00800u){m->clut[((a-0xF00400u)>>1)&255u]=v;return;} /* writes both tables */
    if(a>=0xF00800u&&a<0xF01D9Eu+2u){
        unsigned bank,idx;
        if(a<0xF01000u){bank=0;idx=(a-0xF00800u)>>1;}
        else if(a<0xF01800u){bank=1;idx=(a-0xF01000u)>>1;}
        else{bank=m->lbuf_write;idx=(a-0xF01800u)>>1;}
        if(idx<720u)m->lbuf[bank][idx]=v;
        return;
    }
    note_unhandled(m,a,1);
}

/* ------------------------------------------------------------ JERRY regs */
/* Atari Jaguar Technical Reference Rev.8 pp.85-86: the live JPIT
 * prescaler/divider down-counters are readable at different addresses
 * from their programming latches.  Both stages reload on zero, with
 * periods (N+1) and (M+1), respectively.  The reference scheduler
 * records the elapsed system clocks within each complete timer period;
 * derive both counter phases without adding reads that advance the timer.
 * This is DATA-level readback, not a model of mid-cycle latch/bus timing.
 * Register-write phase resets and prescaler=0 disable semantics remain
 * those of the existing timer scheduler. */
static uint16_t jerry_pit_counter(const JagMachine *m,unsigned timer,
                                  int divider)
{
    uint32_t pre=m->jpit[timer*2u],div=m->jpit[timer*2u+1u];
    uint64_t elapsed=m->jpit_acc[timer];
    uint64_t p=(uint64_t)pre+1u;
    return divider?(uint16_t)(div-(uint32_t)((elapsed/p)%((uint64_t)div+1u))):
                   (uint16_t)(pre-(uint32_t)(elapsed%p));
}
/* R259 JERRY UART. The documented double-buffered receiver is fed by an
 * explicit external-event fixture rather than invented bytes from an absent
 * ComLynx cable. Asynchronous TX is timed at SYS_HZ/[16*(ASICLK+1)] baud;
 * the Atari format always has a parity slot (ODD bit when parity disabled),
 * so one character occupies start+8 data+parity+stop = eleven bit times.
 * Output pin polarity and the known UART start-bit silicon bug are not
 * yet simulated; UARTO is not connected to the native PC multiplayer. */
static uint16_t jerry_asi_status(const JagMachine *m)
{
    uint16_t status=(uint16_t)(m->asictrl&0x3Fu);
    if(m->asi_rx_count)status|=0x0080u;  /* RBF */
    if(!m->asi_tx_pending)status|=0x0100u; /* TBE: holding buffer empty */
    status|=m->asi_errors&0x0E00u;       /* PE, FE, OE */
    status|=(uint16_t)((m->asi_rx_line^((m->asictrl>>3)&1u))<<13);
    status|=m->asictrl&0x4000u;         /* TXBRK */
    if(m->asi_errors&0x0E00u)status|=0x8000u; /* ERROR */
    return status;
}
static uint64_t jerry_uart_character_ticks(const JagMachine *m)
{
    return 11u*16u*((uint64_t)m->asiclk+1u);
}
static void jerry_uart_tx_advance(JagMachine *m,unsigned ticks)
{
    uint64_t remaining=ticks;
    while(remaining&&m->asi_tx_busy){
        if(remaining<m->asi_tx_ticks){m->asi_tx_ticks-=remaining;return;}
        remaining-=m->asi_tx_ticks;
        if(m->asi_tx_pending){
            m->asi_tx_shift=m->asi_tx_hold;
            m->asi_tx_pending=0u;
            m->asi_tx_ticks=jerry_uart_character_ticks(m);
            /* TBE is a separate TX holding-register event. */
            if(m->asictrl&0x0010u){
                m->jint_pending|=0x0010u;
                update_cpu_irq(m);
            }
        }else{
            m->asi_tx_busy=0u;
            m->asi_tx_ticks=0u;
        }
    }
}
void jagm_uart_receive_byte(JagMachine *m,uint8_t data,uint16_t error_bits)
{
    if(m->asi_rx_count<2u){
        m->asi_rx_fifo[m->asi_rx_count++]=data;
        m->asi_rx_last=data;
    }else m->asi_errors|=0x0800u; /* OE: preserve unread characters */
    m->asi_errors|=(uint16_t)(error_bits&0x0600u);
    if(m->asictrl&0x0020u){
        m->jint_pending|=0x0010u;
        update_cpu_irq(m);
    }
}
static uint16_t jerry_read16(JagMachine *m,uint32_t a)
{
    switch(a){
    case 0xF10020u:return m->jint_pending&0x3Fu;
    case 0xF10030u:{ /* ASIDATA read consumes one RX holding byte */
        uint8_t v=m->asi_rx_count?m->asi_rx_fifo[0]:m->asi_rx_last;
        if(m->asi_rx_count){
            m->asi_rx_count--;
            if(m->asi_rx_count)m->asi_rx_fifo[0]=m->asi_rx_fifo[1];
        }
        return v;
    }
    case 0xF10032u:return jerry_asi_status(m); /* ASISTAT */
    case 0xF10034u:return m->asiclk; /* ASICLK is RW, unlike ASICTRL */
    case 0xF10036u:return jerry_pit_counter(m,0u,0); /* JPIT1 */
    case 0xF10038u:return jerry_pit_counter(m,0u,1); /* JPIT2 */
    case 0xF1003Au:return jerry_pit_counter(m,1u,0); /* JPIT3 */
    case 0xF1003Cu:return jerry_pit_counter(m,1u,1); /* JPIT4 */
    case 0xF14000u:{ /* JOYSTICK: port 1 J8..J11, port 2 J12..J15.
                           * Atari Jaguar Technical Reference V10 pp.14-16:
                           * selector bits 0..3 run in normal order for J5,
                           * selector bits 4..7 are mirrored for J6. */
        uint16_t r=0xFFFFu;
        unsigned port;
        if(!(m->joy_out&0x8000u))return r; /* J0..J7 outputs disabled */
        for(port=0u;port<2u;port++){
            unsigned row;
            for(row=0u;row<4u;row++){
                unsigned select_bit=port?(7u-row):row;
                if(!(m->joy_out&(1u<<select_bit))){
                    unsigned nib=(m->pad_state[port]>>(row*4u))&15u;
                    r=(uint16_t)(r&~(uint16_t)(nib<<(8u+port*4u)));
                    break;
                }
            }
        }
        return r;
    }
    case 0xF14002u:{ /* JOYBUTS: B0/B1 for J5, B2/B3 for J6 */
        uint16_t r=0xFFFFu;
        unsigned port;
        if(!(m->joy_out&0x8000u))return r;
        for(port=0u;port<2u;port++){
            unsigned row;
            for(row=0u;row<4u;row++){
                unsigned select_bit=port?(7u-row):row;
                if(!(m->joy_out&(1u<<select_bit))){
                    unsigned b=(m->pad_state[port]>>(16u+row*2u))&3u;
                    r=(uint16_t)(r&~(uint16_t)(b<<(port*2u)));
                    break;
                }
            }
        }
        return r;
    }
    default:break;
    }
    note_unhandled(m,a,0);
    return 0xFFFFu;
}
static void jerry_write16(JagMachine *m,uint32_t a,uint16_t v)
{
    switch(a){
    case 0xF10000u:m->jpit[0]=v;m->jpit_acc[0]=0;return;
    case 0xF10002u:m->jpit[1]=v;m->jpit_acc[0]=0;return;
    case 0xF10004u:m->jpit[2]=v;m->jpit_acc[1]=0;return;
    case 0xF10006u:m->jpit[3]=v;m->jpit_acc[1]=0;return;
    case 0xF10010u:case 0xF10012u:case 0xF10014u:return; /* clock dividers */
    case 0xF10020u:m->jint_enable=v&0x3Fu;m->jint_pending&=(uint16_t)~((v>>8)&0x3Fu);update_cpu_irq(m);return;
    case 0xF10030u: /* ASIDATA: start TX or fill the double buffer */
        if(!m->asi_tx_busy){
            m->asi_tx_shift=(uint8_t)v;
            m->asi_tx_busy=1u;
            m->asi_tx_ticks=jerry_uart_character_ticks(m);
        }else{
            /* The physically undefined third overlapping write replaces
             * the pending byte; it never rewrites an in-flight character. */
            m->asi_tx_hold=(uint8_t)v;
            m->asi_tx_pending=1u;
        }
        return;
    case 0xF10032u:
        m->asictrl=v&0x403Fu;
        if(v&0x0040u)m->asi_errors=0u; /* CLRERR is a command, not a latch */
        return;
    case 0xF10034u:m->asiclk=v;return;
    case 0xF14000u:m->joy_out=v;return;
    default:break;
    }
    note_unhandled(m,a,1);
}

/* ------------------------------------------------------------ Musashi */
unsigned int m68k_read_memory_8(unsigned int a){return jagm_read8(M,a);}
unsigned int m68k_read_memory_16(unsigned int a){return jagm_read16(M,a);}
unsigned int m68k_read_memory_32(unsigned int a)
{
    /* A 68000 longword in GPU/DSP I/O is two 16-bit cycles, not a
     * native 32-bit RISC transaction. Preserve the programmed BIG_IO
     * word order while leaving direct GPU/DSP longword reads intact. */
    if(in_gpu(a)||in_dsp(a))
        return ((uint32_t)jagm_read16(M,a)<<16)|jagm_read16(M,a+2u);
    return jagm_read32(M,a);
}
unsigned int m68k_read_disassembler_8(unsigned int a){return jagm_read8(M,a);}
unsigned int m68k_read_disassembler_16(unsigned int a){return jagm_read16(M,a);}
unsigned int m68k_read_disassembler_32(unsigned int a){return jagm_read32(M,a);}
/* These entrypoints are called by Musashi for actual 68000 writes.
 * Observe only after committing the transaction. Do not instrument generic
 * jagm_write* accessors, since GPU/DSP and host tests use them as well. */
static void cpu_dram_write(uint32_t a,unsigned width)
{
    JagCpuDramWriteEvent e;
    a&=0xFFFFFFu;
    if(!M||!M->on_cpu_dram_write||a>=0x800000u)return;
    e.field=M->frames;e.address=a;e.width=(uint8_t)width;
    e.pc68=m68k_get_reg(NULL,M68K_REG_PC)&0xFFFFFFu;
    M->on_cpu_dram_write(M,&e,M->on_cpu_dram_write_ctx);
}
void m68k_write_memory_8(unsigned int a,unsigned int v)
{jagm_write8(M,a,(uint8_t)v);cpu_dram_write(a,1u);}
void m68k_write_memory_16(unsigned int a,unsigned int v)
{jagm_write16(M,a,(uint16_t)v);cpu_dram_write(a&~1u,2u);}
void m68k_write_memory_32(unsigned int a,unsigned int v)
{
    if(in_gpu(a)||in_dsp(a)){
        jagm_write16(M,a,(uint16_t)(v>>16));
        jagm_write16(M,a+2u,(uint16_t)v);
    }else jagm_write32(M,a,v);
    cpu_dram_write(a&~1u,4u);
}

/* ------------------------------------------------------- Object Processor */
static uint64_t rd64(JagMachine *m,uint32_t a)
{ return ((uint64_t)jagm_read32(m,a)<<32)|jagm_read32(m,a+4u); }
static void wr64(JagMachine *m,uint32_t a,uint64_t v)
{ jagm_write32(m,a,(uint32_t)(v>>32));jagm_write32(m,a+4u,(uint32_t)v); }

static void op_put(JagMachine *m,int x,uint16_t v,int rmw)
{
    if(x<0||x>=720)return; /* clipped at the end of the line buffer (bug 5) */
    m->lbuf[m->lbuf_write][x]=rmw?avp_jaguar_rmw_blend_cry(m->lbuf[m->lbuf_write][x],v):v;
}

/* TOM RGB24 occupies two consecutive 16-bit line-buffer words per pixel.
 * Use the same physical source->buffer mapping for BITOBJ and for the
 * documented SCBITOBJ HSCALE=0x20 subset.  The 24-bit output mode consumes
 * these two words together; never send an RGB24 source through CRY16 op_put. */
static void op_put_rgb24(JagMachine *m,int x,uint32_t raw)
{
    if(x<0||x>=360)return; /* 720 line-buffer words = 360 RGB24 pixels */
    m->lbuf[m->lbuf_write][2u*(unsigned)x]=(uint16_t)(raw>>16);
    m->lbuf[m->lbuf_write][2u*(unsigned)x+1u]=(uint16_t)raw;
}

/* returns pixel value (bpp<=16) for source pixel index s */
static uint32_t op_src_pixel(JagMachine *m,uint32_t data,unsigned pitch,unsigned bpp,uint32_t s)
{
    unsigned ppp=bpp==24u?2u:64u/bpp;
    uint32_t phrase=s/ppp,k=s%ppp;
    uint32_t a=data+phrase*8u*pitch;
    if(bpp==24u)return jagm_read32(m,a+k*4u);
    if(bpp==16u)return jagm_read16(m,a+k*2u);
    if(bpp==8u)return jagm_read8(m,a+k);
    { uint8_t byte=jagm_read8(m,a+(k*bpp)/8u);unsigned group=8u/bpp,sh=(group-1u-(k%group))*bpp;
      return (byte>>sh)&((1u<<bpp)-1u); }
}

static void op_bitmap(JagMachine *m,uint32_t addr,uint64_t p0,uint64_t p1,uint64_t p2,int scaled)
{
    unsigned height=(unsigned)((p0>>14)&0x3FFu);
    uint32_t data=(uint32_t)((p0>>43)&0x1FFFFFu)<<3;
    int xpos=(int)(p1&0xFFFu);if(xpos&0x800)xpos-=0x1000;
    unsigned depth=(unsigned)((p1>>12)&7u),pitch=(unsigned)((p1>>15)&7u);
    unsigned dwidth=(unsigned)((p1>>18)&0x3FFu),iwidth=(unsigned)((p1>>28)&0x3FFu);
    unsigned index=(unsigned)((p1>>38)&0x7Fu),reflect=(unsigned)((p1>>45)&1u);
    unsigned rmw=(unsigned)((p1>>46)&1u),trans=(unsigned)((p1>>47)&1u),firstpix=(unsigned)((p1>>49)&0x3Fu);
    unsigned bpp=depth==5u?24u:(1u<<depth);
    unsigned ppp=bpp==24u?2u:64u/bpp;
    uint32_t n=(uint32_t)iwidth*ppp,s;
    uint32_t first_skip=0u;
    int dir=reflect?-1:1;
    unsigned ypos=(unsigned)((p0>>3)&0x7FFu);
    /* SCBITOBJ HEIGHT is source rows minus one (TRM Rev.8 p.19).
     * The last row is still drawable with HEIGHT=0 while REMAINDER>0;
     * ordinary BITOBJ remains active only with HEIGHT>0.  Preserve the
     * vertical-step/write-back logic and the unresolved field timing. */
    if(m->vc<ypos||(height==0u&&(!scaled||((p2>>16)&0xFFu)==0u)))return;
    if(depth==4u||depth==5u){
        m->op_deep_bitmap_visits++;
        if((p1>>48)&1u)m->op_deep_release_visits++;
    }
    if(depth>5u){m->op_unsupported_mask|=1u;return;}
    /* TOM OP, Rev.8 pp.18-22: FIRSTPIX selects a source pixel pair
     * for unscaled BITOBJ or one source pixel for scaled SCBITOBJ.
     * For RGB24 each source phrase contains just two 32-bit slots:
     * the scaled DEPTH=5 case uses FIRSTPIX bit 5 to skip slot zero;
     * bits 0..4 do not select partial pixels.  As with 1..16 bpp,
     * subtract the skipped source pixel from IWIDTH's exposed width,
     * leave XPOS intact and preserve DATA/DWIDTH line write-back.
     * Unscaled RGB24 with nonzero FIRSTPIX remains unverified. */
    if(bpp<=16u||(bpp==24u&&scaled)){
        first_skip=scaled ? (firstpix>>depth) :
                   ((firstpix>>(depth+1u))<<1);
        if(first_skip>=ppp||first_skip>=n){
            m->op_unsupported_mask|=2u;
            return;
        }
        n-=first_skip;
    }else if(firstpix){m->op_unsupported_mask|=2u;}
    if(bpp==24u&&(trans||rmw))m->op_unsupported_mask|=4u;
    if(!scaled){
        int x=xpos;
        for(s=0;s<n;s++,x+=dir){
            uint32_t raw=op_src_pixel(m,data,pitch,bpp,s+first_skip);
            if(bpp==24u){op_put_rgb24(m,x,raw);continue;}
            if(trans&&raw==0u)continue;   /* p.7/p.19: colour zero, 1..16 bpp */
            if(bpp<=8u)raw=m->clut[(((index>>(bpp-1u))<<bpp)|raw)&255u];
            op_put(m,x,(uint16_t)raw,rmw);
        }
        /* TOM Rev.8 pp.15,18: VP+1 odd selects interlaced video.
         * An unscaled object consumes TWO source rows per displayed line
         * in interlace (one in non-interlace). HEIGHT saturates at zero;
         * DATA advances by the same number of DWIDTH strides. This is
         * the documented header write-back, not a claim of full field/
         * half-line phase accuracy for the reference video scheduler. */
        { unsigned rows=(m->vp&1u)?1u:2u;
          uint64_t np=p0;
          uint32_t nd=(data>>3)+dwidth*rows;
          unsigned nh=height>rows?height-rows:0u;
          np&=~((uint64_t)0x1FFFFFu<<43);np|=(uint64_t)(nd&0x1FFFFFu)<<43;
          np&=~((uint64_t)0x3FFu<<14);np|=(uint64_t)nh<<14;wr64(m,addr,np); }
    }else{
        unsigned hscale=(unsigned)(p2&0xFFu),vscale=(unsigned)((p2>>8)&0xFFu),rem=(unsigned)((p2>>16)&0xFFu);
        /* Atari JDEV0002 OP 2/3: 7.0 is maximum valid VSCALE; for
           24bpp scaled objects only HSCALE=1.0 is reliable. We cannot
           infer the actual distorted pixels, so do not render a guess. */
        if(vscale>0xE0u){m->op_unsupported_mask|=64u;return;}
        if(bpp==24u&&hscale!=0x20u){m->op_unsupported_mask|=128u;return;}
        uint32_t out=hscale?(uint32_t)(((uint64_t)n*hscale+31u)/32u):0u,k;
        int remv;unsigned nh=height;uint32_t nd=data>>3;
        for(k=0;k<out;k++){
            int x=xpos+dir*(int)k;uint32_t raw;
            s=(uint32_t)(((uint64_t)k*32u)/hscale);if(s>=n)break;
            raw=op_src_pixel(m,data,pitch,bpp,s+first_skip);
            /* Jaguar errata permits RGB24 scaled bitmaps only at HSCALE
             * 1.0.  That case uses the same two-word physical OP pixel as
             * BITOBJ; VSCALE/REMAINDER still update the source line below. */
            if(bpp==24u){op_put_rgb24(m,x,raw);continue;}
            if(trans&&raw==0u)continue;
            if(bpp<=8u)raw=m->clut[(((index>>(bpp-1u))<<bpp)|raw)&255u];
            op_put(m,x,(uint16_t)raw,rmw);
        }
        /* p.19 REMAINDER: minus one line (1.0 = 0x20); while negative add
           VSCALE, decrementing HEIGHT and advancing DATA by DWIDTH. */
        /* Rev.8 p.19: each VSCALE addition advances DATA by one DWIDTH,
         * even after HEIGHT has saturated at zero.  Previously the
         * `&&nh` loop condition truncated DATA and REMAINDER for small
         * VSCALE and short objects.  Match the standalone OP writeback
         * primitive; do not infer scaled-interlaced field phases here. */
        remv=(int)rem-32;
        while(remv<0){
            remv+=(int)vscale;
            if(nh)nh--;
            nd+=dwidth;
            if(!vscale)break; /* unverified VSCALE=0 legacy behavior */
        }
        if(remv<0)remv=0;
        { uint64_t np=p0,np2=p2;
          np&=~((uint64_t)0x1FFFFFu<<43);np|=(uint64_t)(nd&0x1FFFFFu)<<43;
          np&=~((uint64_t)0x3FFu<<14);np|=(uint64_t)nh<<14;wr64(m,addr,np);
          np2&=~((uint64_t)0xFFu<<16);np2|=(uint64_t)((unsigned)remv&0xFFu)<<16;wr64(m,addr+16u,np2); }
    }
}

static int run_gpu_until_obf(JagMachine *m);

/* Execute one OP list segment.  The cursor is explicit so an OBF write
 * outside the original GPUOBJ callback can continue after the interrupt
 * object, without restarting at OLP.  The line-buffer timing of a delayed
 * continuation remains an unsupported approximation. */
static void op_list_walk(JagMachine *m,uint32_t a,unsigned op_start_hc)
{
    unsigned guard=0;
    int preceding_active_rmw=0;
    while(guard++<4096u){
        uint64_t p0=rd64(m,a);unsigned type=(unsigned)(p0&7u);
        uint32_t link=(uint32_t)((p0>>24)&0x7FFFFu)<<3;
        uint32_t next=(a&~0x3FFFF8u)|link; /* p.18: LINK replaces OLP bits 3..21 */
        m->op_objects++;
        if(type!=0u&&type!=1u)preceding_active_rmw=0;
        switch(type){
        case 0: case 1:{
            /* TRM Rev.8 pp.18-19: BITOBJ must start on a 16-byte
             * boundary; SCBITOBJ on a 32-byte boundary.  Do not invent
             * valid output for an undefined header alignment.  Existing
             * scaled misalignment reporting (bit 9) is preserved. */
            if(type==0u&&(a&15u)!=0u){
                m->op_unsupported_mask|=1024u;
                return;
            }
            if(type==1u&&(a&31u)!=0u){
                m->op_unsupported_mask|=512u;
                return;
            }
            uint64_t p1=rd64(m,a+8u);
            uint64_t p2=type==1u?rd64(m,a+16u):0u;
            unsigned ypos=(unsigned)((p0>>3)&0x7FFu);
            unsigned height=(unsigned)((p0>>14)&0x3FFu);
            int active=m->vc>=ypos&&
                (height!=0u||(type==1u&&((p2>>16)&0xFFu)!=0u));
            /* JDEV0002 OP item 1: immediately following active bitmap
               can corrupt RMW edge. Do not guess the corrupted color. */
            if(preceding_active_rmw&&active)m->op_rmw_neighbor_visits++;
            preceding_active_rmw=active&&((p1>>46)&1u);
            op_bitmap(m,a,p0,p1,p2,type==1u);
            a=next;break;
        }
        case 2:
            /* A GPU object is one uninterpreted 64-bit phrase: only TYPE
             * (bits 0..2) is an OP header. Bits 3..63 are ISR payload,
             * NOT a YPOS filter. The Atari programming appendix requires
             * BRANCH objects to gate the interrupt by VC. The Rev.8 table
             * that describes a YPOS here conflicts with the hardware
             * programming guidance and the single-phrase format.
             * Do not suppress an interrupt based on ISR payload bits. */
            m->ob=p0;
            m->op_gpu_objects++;
            /* Preserve the address *after* this single-phrase GPU object.
             * The GPU might be stopped or take several scheduler slices to
             * write OBF.  Only OBF explicitly releases this cursor. */
            m->op_resume_addr=a+8u;
            m->op_resume_hc=op_start_hc;
            m->op_resume_valid=1u;
            jrisc_raise(&m->gpu,3);
            if(!run_gpu_until_obf(m))return; /* no OBF: STOP here, not later */
            m->op_resume_valid=0u;
            a+=8u;
            break;
        case 3:{unsigned y=(unsigned)((p0>>3)&0x7FFu),cc=(unsigned)((p0>>14)&7u);int t=0;
            /* TOM OP branch CC is THREE bits (14..16); Rev.8's printed
             * 14..15 is a table error because the same table specifies
             * CC=4.  Software Reference V10 explicitly gives 14..16.
             * HC10 is latched at this OP start.  R209 executes a second
             * HDB2 start in the documented next-half configuration;
             * precise beam timing and same-half starts remain outside. */
            switch(cc){case 0:t=(y==m->vc||y==0x7FFu);break;
                case 1:t=(y>m->vc);break;case 2:t=(y<m->vc);break;
                case 3:t=(m->obf&1u)!=0;break;
                case 4:t=(op_start_hc&0x400u)!=0;break;
                default:m->op_unsupported_mask|=256u;break;}
            a=t?next:a+8u;break;}
        case 4:
            /* STOP always exposes its complete phrase in OB0-3, but
             * generates a CPU Object interrupt ONLY with the INT FLAG
             * (phrase bit 3) set. Rev.8 omitted this bit; the later
             * Software Reference V10 and OP quick-reference specify it. */
            m->ob=p0;
            if(p0&8u){m->int1_pending|=4u;update_cpu_irq(m);}
            return;
        default:m->op_unsupported_mask|=16u;return;
        }
    }
}

static void op_line(JagMachine *m,unsigned op_start_hc)
{
    if(m->op_waiting_for_obf||m->op_resume_valid)return;
    m->op_lines++;
    m->op_processing=1u;
    op_list_walk(m,m->olp&0xFFFFF8u,op_start_hc);
    m->op_processing=0u;
}

static void op_resume_after_obf(JagMachine *m)
{
    uint32_t addr=m->op_resume_addr;
    unsigned hc=m->op_resume_hc;
    m->op_resume_valid=0u;
    m->op_processing=1u;
    op_list_walk(m,addr,hc);
    m->op_processing=0u;
}

/* R149: retire one GPU instruction.  The MMIO event occurring INSIDE that
 * instruction records the completed count, making ordering within a single
 * reference-machine scheduler slice observable without adding wait states. */
static unsigned gpu_step_observed(JagMachine *m)
{
    unsigned c=jrisc_step(&m->gpu);
    if(c){m->gpu_steps_completed++;m->gpu_model_ticks_completed+=c;}
    return c;
}

/* GPU object: the OP waits for a write to OBF (p.12).  Run only the GPU. */
static int run_gpu_until_obf(JagMachine *m)
{
    unsigned i;int saved_master=master;
    /* OBF is a guest-visible register.  Bit 15 is not reserved as a busy
     * sentinel: any GPU/CPU write, including 0x8000 or 0x8001, resumes
     * processing.  Track the pending wait separately from the register. */
    m->op_waiting_for_obf=1u;
    master=1; /* GPU owns requests even when invoked by the Object Processor. */
    for(i=0;i<200000u&&m->gpu.go;i++){
        gpu_step_observed(m);
        if(!m->op_waiting_for_obf){master=saved_master;return 1;}
    }
    master=saved_master;
    /* Remain stopped until an explicit OBF write.  The saved OP phrase
     * cursor now permits deferred list continuation; this unsupported bit
     * still reports that exact beam/line-buffer/bus-cycle phasing is absent. */
    m->op_unsupported_mask|=32u;
    return 0;
}

/* ------------------------------------------------------------- video out */
/* R209: each HDB start displays one physical line-buffer segment.  An
 * HDB2 start in the next horizontal half-period fetches from the newly
 * selected bank and places that segment at its physical screen offset.
 * A 24-bit source uses two 16-bit buffer words per displayed pixel. */
static void output_line_segment(JagMachine *m,unsigned row,
                                unsigned dest_x,unsigned count)
{
    unsigned mode=(m->vmode>>1)&3u,x,w=count;
    uint16_t *lb=m->lbuf[m->lbuf_write];
    if(row>=JAGM_MAX_H||dest_x>=JAGM_MAX_W)return;
    if(mode==1u&&w>360u)w=360u; /* two words per 24-bit pixel */
    if(w>JAGM_MAX_W-dest_x)w=JAGM_MAX_W-dest_x;
    if(dest_x+w>m->work_w)m->work_w=dest_x+w;
    for(x=0;x<w;x++){
        uint32_t rgb;
        if(mode==1u)rgb=avp_jaguar_vmode_rgb24(lb[2*x],lb[2*x+1]);
        else{
            uint16_t v=lb[x];
            if((m->vmode&0x100u)&&(v&1u))
                rgb=avp_jaguar_vmode_rgb16((uint16_t)(v&~1u));
            else if(mode==3u)rgb=avp_jaguar_vmode_rgb16(v);
            else{
                uint8_t r,g,b;
                avp_jaguar_cry16_to_rgb(v,&r,&g,&b);
                rgb=((uint32_t)r<<16)|((uint32_t)g<<8)|b;
            }
        }
        m->work[row*JAGM_MAX_W+dest_x+x]=rgb;
    }
    /* TRM Rev.8 VMODE bit 7 (BGEN): hardware background refill operates
     * only in 16-bit CRY (MODE=0) and 16-bit RGB (MODE=3).  RGB24 and
     * 16-bit direct output retain their line-buffer contents even if BGEN
     * is set.  Do not clear the direct-output bank as in R209. */
    if((m->vmode&0x80u)&&(mode==0u||mode==3u))
        for(x=0;x<720u;x++)lb[x]=m->bg;
}

/* ------------------------------------------------------------- scheduling */
static void jerry_i2s_interrupt(JagMachine *m)
{
    jrisc_raise(&m->dsp,1);
    m->jint_pending|=0x0020u; /* JINTCTRL J_SYN */
    update_cpu_irq(m);
}

static void run_slice(JagMachine *m,unsigned sys_ticks)
{
    unsigned gt,dt;
    /* 68000 at half the system clock */
    /* R287 experimental shared-DRAM arbitration. While a progressive
     * Blitter owns the bus, do not issue 68000 instructions which might
     * touch DRAM before its transfers retire. Do not accrue catch-up CPU
     * cycles: the hardware would have waited for bus access. GPU and DSP
     * work remains scheduled separately. This conservative bus grant is
     * NOT a cycle-accurate Jaguar contention model. */
    if(!(m->blit.async_mode && m->blit.running))
        m->cpu_cycle_debt+=sys_ticks/2.0;
    else m->cpu_cycle_debt=0.0;
    master=0;
    if(!(m->blit.async_mode && m->blit.running) &&
       m->cpu_cycle_debt>=1.0){
        int n=(int)m->cpu_cycle_debt;
        int used;
        cpu_beam_active=1;
        used=m68k_execute(n);
        cpu_beam_active=0;
        m->cpu_cycle_debt-=used;
    }
    master=1;
    for(gt=0;gt<sys_ticks&&m->gpu.go;){
        static long tr=-2;
        if(tr==-2){const char *e=getenv("JAGM_GPU_TRACE");tr=e?atol(e):-1;}
        if(tr>0&&m->frames>=(uint64_t)atol(getenv("JAGM_GPU_TRACE_FRAME")?getenv("JAGM_GPU_TRACE_FRAME"):"0")){uint32_t w=0;jrisc_local_read(&m->gpu,m->gpu.pc&~3u,&w);
            {unsigned ins=(m->gpu.pc&2u)?(w&0xFFFFu):(w>>16);if(!jrisc_in_local(&m->gpu,m->gpu.pc))ins=jagm_read16(m,m->gpu.pc);
            uint32_t *R=jrisc_regs(&m->gpu);
            fprintf(stderr,"G %06x %04x %-12s r%u,r%u  s=%08x d=%08x fl=%x\n",m->gpu.pc,ins,jrisc_mnemonic(0,ins>>10,(ins>>5)&31),(ins>>5)&31,ins&31,R[(ins>>5)&31],R[ins&31],m->gpu.flags&7u);}
            tr--;}
        { static long snap_pc=-2;static uint64_t snap_frame;static int snapped;static unsigned long snap_word;
          if(snap_pc==-2){const char *e=getenv("JAGM_GPU_SNAP");snap_pc=-1;if(e){unsigned long pcv;unsigned long long fr=0;if(sscanf(e,"%lx,%llu,%lx",&pcv,&fr,&snap_word)>=1){snap_pc=(long)pcv;snap_frame=fr;}}}
          if(snap_pc>=0&&!snapped&&m->frames>=snap_frame&&m->gpu.pc==(uint32_t)snap_pc&&(!snap_word||jagm_read16(m,m->gpu.pc)==snap_word)){FILE *o=fopen("gpu_snap.bin","wb");if(o){fwrite(m->gpu.ram,1,m->gpu.ram_size,o);fclose(o);}snapped=1;} }
        if(m->gpu_break_armed&&m->frames>=m->break_min&&m->gpu.pc==m->gpu_break_pc){m->halted=1;m->gpu_break_armed=0;break;}
        {unsigned c=gpu_step_observed(m);if(!c)break;gt+=c;}}
    master=2;
    for(dt=0;dt<sys_ticks&&m->dsp.go;){unsigned c=jrisc_step(&m->dsp);if(!c)break;dt+=c;}
    /* The interval just executed contributes new Blitter model time.
     * No idle-time carry: a fresh command cannot inherit previous cycles. */
    if(m->blit.running){
        m->blit_tick_credit+=sys_ticks;
        blit_bus_sync(m);
    }else m->blit_tick_credit=0u;
    update_cpu_irq(m);
    m->sys_cycles+=sys_ticks;
    /* TOM PIT (p.16): sysclk/(PIT0+1)/(PIT1+1) -> CPU int 3 and GPU int 2 */
    if(m->pit0){uint64_t per=(uint64_t)(m->pit0+1u)*(uint64_t)(m->pit1+1u);m->pit_acc+=sys_ticks;
        while(m->pit_acc>=per){m->pit_acc-=per;m->int1_pending|=8u;jrisc_raise(&m->gpu,2);update_cpu_irq(m);}}
    /* JERRY timers (p.85): -> DSP int 2/3 and CPU (F10020 bits 2/3) */
    for(int t=0;t<2;t++){uint16_t pre=m->jpit[t*2],div=m->jpit[t*2+1];
        if(pre){uint64_t per=(uint64_t)(pre+1u)*(uint64_t)(div+1u);m->jpit_acc[t]+=sys_ticks;
            while(m->jpit_acc[t]>=per){m->jpit_acc[t]-=per;jrisc_raise(&m->dsp,2u+(unsigned)t);m->jint_pending|=(uint16_t)(4u<<t);update_cpu_irq(m);}}}
    /* JERRY asynchronous serial transmitter advances independently of
     * DSP execution and the I2S audio serial clock. */
    jerry_uart_tx_advance(m,sys_ticks);
    /* The synchronous serial source is independently maskable for the
     * external 68000 through JERRY JINTCTRL bit 5.  A selected serial
     * event drives DSP IRQ1 *and* latches J_SYN, just as JERRY timers
     * separately drive their DSP and CPU interrupt paths (TRM Rev.8
     * pp. 86-87, 91-92). TOM sees the JERRY level's rising edge through
     * update_cpu_irq(); TOM ACK does not clear the J_SYN source. */
    /* JERRY I2S (V10 pp. 75-78): the internal 8-bit SCLK divider
     * clocks serial words even when interrupt enables are all zero.
     * Generated WS changes once per 16 bit clocks; a stereo pair takes
     * 32 bit clocks. RISING/FALLING respond to distinct WS transitions,
     * while EVERYWORD responds every 16 clocks independent of WSEN.
     * The initial low WS phase is a model convention, not a measured
     * physical-console reset phase or serial-data pin waveform. */
    if(m->smode&1u){
        const uint64_t half=(uint64_t)2u*(m->sclk+1u)*16u;
        const uint64_t pair=half*2u;
        uint64_t pos=m->i2s_acc%pair;
        uint64_t remaining=sys_ticks;
        while(remaining){
            const uint64_t edge=(pos<half)?half:pair;
            const uint64_t advance=(edge-pos<remaining)?edge-pos:remaining;
            pos+=advance;remaining-=advance;
            if(pos==half){
                /* When WSEN=0, no further high pulses are generated.
                 * A high pulse already in progress is allowed to end. */
                if((m->smode&4u)&&!m->i2s_ws){
                    m->i2s_ws=1u;
                    if(m->smode&8u)jerry_i2s_interrupt(m);
                }
                if(m->smode&32u)jerry_i2s_interrupt(m);
            }else if(pos==pair){
                if(m->i2s_ws){
                    m->i2s_ws=0u;
                    if(m->smode&16u)jerry_i2s_interrupt(m);
                }
                if(m->smode&32u)jerry_i2s_interrupt(m);
                if(m->capture_audio){
                    if(m->audio_n+2u>m->audio_cap){
                        size_t nc=m->audio_cap?m->audio_cap*2u:65536u;
                        int16_t *na=(int16_t*)realloc(m->audio,nc*sizeof(int16_t));
                        if(!na)break;
                        m->audio=na;m->audio_cap=nc;
                    }
                    /* JOYSTICK bit 8 gates externally audible output, not
                     * the DSP serial clock, data latches or WS interrupt. */
                    const int output_enabled=(m->joy_out&0x0100u)!=0u;
                    m->audio[m->audio_n++]=output_enabled?(int16_t)m->ltxd:0;
                    m->audio[m->audio_n++]=output_enabled?(int16_t)m->rtxd:0;
                }
                pos=0u;
            }
        }
        m->i2s_acc=pos;
    }
}

static void half_line(JagMachine *m)
{
    unsigned hp2=(unsigned)m->hp+1u;
    unsigned period=hp2?hp2:845u;
    unsigned slices=8u,i;
    int line_start=!(m->vc&1u);
    /* HC counts from 0 through HP on EACH half-line; HC10 identifies
     * which physical half is being generated. Expose the current slice
     * boundary to TOM reads instead of leaving HC permanently at zero. */
    m->hc_half=(m->vc&1u)<<10;
    unsigned hdb1=(m->hdb1&0x3ffu)+((m->hdb1&0x400u)?period:0u);
    unsigned hdb2=(m->hdb2&0x3ffu)+((m->hdb2&0x400u)?period:0u);
    unsigned hde=(m->hde&0x3ffu)+((m->hde&0x400u)?period:0u);
    unsigned pw=((m->vmode>>9)&7u)+1u;
    /* R219: HDB1/HDB2 are independent horizontal comparators.  A match
     * starts an Object Processor pass; register numbering does NOT impose
     * temporal order.  Build the chronological list once for the physical
     * line, then run the events in the appropriate HC10 half.  Equal HDB
     * values yield one event, and an out-of-line comparator is ignored.
     *
     * Each pass uses the current line-buffer bank; the next comparator
     * defines the first segment's output extent, regardless of whether
     * HDB1 or HDB2 matched first.  This handles reverse order and lone
     * HDB2 while retaining R218's ordered/same-half coverage.
     *
     * Beam-time MMIO changes, active display-fetch overlap and shared-bus
     * arbitration remain unimplemented.  For irregular HDE outside the
     * physical line, retain R218's compatibility fallback. */
    {
        const int active=(m->vc>=m->vdb||m->vde>m->vp)&&
                         m->vc<m->vde&&(m->vmode&1u);
        unsigned event_tick[2],event_hc[2],event_dest[2],event_width[2];
        unsigned event_n=0u,event_i=0u,elapsed=0u;
        unsigned comparator[2]={hdb1,hdb2},raw_hc[2]={m->hdb1,m->hdb2};
        unsigned starts[2],start_hc[2],start_n=0u;
        if(active&&hde>0u&&hde<=2u*period){
            for(unsigned k=0u;k<2u;k++){
                const unsigned p=comparator[k];
                if(p>=2u*period||p>=hde||((raw_hc[k]&0x3ffu)>=period))continue;
                if(start_n&&starts[0]==p)continue;
                starts[start_n]=p;start_hc[start_n]=raw_hc[k];start_n++;
            }
            if(start_n==2u&&starts[1]<starts[0]){
                unsigned t=starts[0];starts[0]=starts[1];starts[1]=t;
                t=start_hc[0];start_hc[0]=start_hc[1];start_hc[1]=t;
            }
            for(unsigned k=0u;k<start_n;k++){
                unsigned p=starts[k];
                unsigned next=k+1u<start_n?starts[k+1u]:hde;
                if((p<period)!=line_start)continue;
                event_tick[event_n]=p%period;
                event_hc[event_n]=start_hc[k];
                event_dest[event_n]=(p-starts[0])/pw;
                event_width[event_n]=(next-p)/pw;
                event_n++;
            }
        }else if(active){
            /* R218 compatibility for HDE beyond the line period; it is
             * not evidence for a second OP pass in that configuration. */
            const int late=(hdb1&0x400u)!=0u&&
                 hdb1>=period&&hdb1<2u*period&&hdb1<hde&&
                 (hdb2==hdb1||hdb2>=2u*period);
            if((line_start&&!late)||(!line_start&&late)){
                event_tick[0]=(line_start&&hdb1>=period)?0u:hdb1%period;
                event_hc[0]=m->hdb1;
                event_dest[0]=0u;
                event_width[0]=hde>hdb1?(hde-hdb1)/pw:0u;
                event_n=1u;
            }
        }
        for(i=0u;i<slices;i++){
            unsigned chunk=period/slices+(i<period%slices?1u:0u);
            unsigned cursor=elapsed,end_tick=elapsed+chunk;
            while(event_i<event_n&&event_tick[event_i]<=end_tick){
                unsigned tick=event_tick[event_i];
                m->hc_half=((m->vc&1u)<<10)|(cursor&0x3ffu);
                if(tick>cursor)run_slice(m,tick-cursor);
                if(m->halted&&m->gpu_break_pc&&!m->gpu_break_armed)return;
                m->hc_half=((m->vc&1u)<<10)|(tick&0x3ffu);
                op_line(m,event_hc[event_i]);
                if(m->vc>=m->vdb)
                    output_line_segment(m,(m->vc-m->vdb)/2u,
                                        event_dest[event_i],event_width[event_i]);
                m->lbuf_write^=1u;
                cursor=tick;
                event_i++;
            }
            m->hc_half=((m->vc&1u)<<10)|(cursor&0x3ffu);
            if(end_tick>cursor)run_slice(m,end_tick-cursor);
            if(m->halted&&m->gpu_break_pc&&!m->gpu_break_armed)return;
            elapsed=end_tick;
        }
    }
    /* TOM VI compares against the actual eleven-bit half-line counter.
     * Wrap VC at VP before the comparator: the start of the next field is
     * VC=0, not the phantom VP+1. The former ordering could never raise
     * VI=0 and incorrectly raised an IRQ for VI=VP+1.  This handles the
     * comparator boundary, not bus-cycle-accurate IRQ latency. */
    { int completed_field=0;
      m->vc++;
      /* VP holds the LAST half-line index.  VP=0 explicitly selects a
       * one-half-line field for diagnostics, not an NTSC reset request.
       * jagm_init installs the NTSC default of 523 until guest software
       * programs VP; no replacement at runtime is permitted (TRM p.15). */
      if(m->vc>(unsigned)m->vp){
          unsigned h=m->vde>m->vdb?(unsigned)(m->vde-m->vdb)/2u:0u;
          m->vc=0;m->frames++;
          m->frame_h=h>JAGM_MAX_H?JAGM_MAX_H:h;
          memcpy(m->frame,m->work,sizeof(m->frame));
          /* Commit only the current field's horizontal extent. */
          m->frame_w=m->work_w;
          m->work_w=0u;
          completed_field=1;
      }
      /* Atari Software Reference V10, TOM VI: in interlaced video
       * (VP+1 odd, so VP bit 0 clear), VI interrupts once per complete
       * frame, i.e. on alternate FIELDS, not every field. Noninterlaced
       * VI retains its once-per-field comparator. The reference machine
       * uses the existing exposed VC field parity (frames bit 0) and
       * selects parity 1 in interlace; actual first-field phasing versus
       * Jaguar silicon has not been measured. The check runs AFTER VC
       * wraps so VI=0 compares against the NEW field's parity. */
      if(m->vc==m->vi&&((m->vp&1u)||(m->frames&1u))){
          m->int1_pending|=1u;update_cpu_irq(m);
      }
      if(completed_field&&m->on_frame)m->on_frame(m,m->on_frame_ctx);
    }
    /* A stopped machine exposes the next half-line's counter origin. */
    m->hc_half=(m->vc&1u)<<10;
}

void jagm_run_frames(JagMachine *m,unsigned frames)
{
    uint64_t target=m->frames+frames;
    M=m;
    while(m->frames<target&&!m->halted)half_line(m);
}

int jagm_run_until_gpu_pc(JagMachine *m,uint32_t pc,uint64_t min_fields,uint64_t max_fields)
{
    M=m;m->gpu_break_pc=pc;m->gpu_break_armed=1;m->break_min=min_fields;m->halted=0;
    while(!m->halted&&m->frames<max_fields)half_line(m);
    if(m->halted){m->halted=0;return 1;}
    m->gpu_break_armed=0;return 0;
}

void jagm_set_blitter_async(JagMachine *m,int enable)
{
    if(!m->blit.running&&!m->blit.stopped){
        jag_blitter_set_async(&m->blit,enable);
        m->blit_tick_credit=0u;
    }
}

void jagm_set_pad(JagMachine *m,unsigned port,uint32_t mask){if(port<2)m->pad_state[port]=mask;}
void jagm_set_wave_rom(JagMachine *m,const uint8_t *r){m->dsp.wave_rom=r;}

int jagm_init(JagMachine *m,const uint8_t *rom,size_t rom_size)
{
    JRiscBus rb;JagBlitBus bb;
    memset(m,0,sizeof(*m));
    m->rom=rom;m->rom_size=rom_size;M=m;watch_init();
    rb.ctx=m;rb.read=risc_read;rb.write=risc_write;
    jrisc_init(&m->gpu,JRISC_GPU,rb);jrisc_init(&m->dsp,JRISC_DSP,rb);
    m->gpu.check_hazards=m->dsp.check_hazards=1; /* report score-board-uncovered hazards */
    bb.ctx=m;bb.read8=blit_rd8;bb.write8=blit_wr8;jag_blitter_init(&m->blit,bb);
    if(getenv("JAGM_BLIT_LOG")&&(blit_log=fopen(getenv("JAGM_BLIT_LOG"),"w"))!=NULL){m->blit.on_blit=blit_logger;m->blit.on_blit_ctx=m;}
    /* NTSC timing defaults until the program writes TOM (values are only used
       before the cartridge programs the video registers). */
    m->hp=844;m->vp=523;m->vdb=0;m->vde=0;m->vi=0x7FF;
    m->asi_rx_line=1u; /* no cable: disconnected input pulled high */
    m68k_init();m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();
    if(rom_size<0x2000u)return -1;
    m68k_set_reg(M68K_REG_SR,0x2700);
    m68k_set_reg(M68K_REG_SP,0x00004000u);
    m68k_set_reg(M68K_REG_PC,be32(rom+0x404u));
    return 0;
}

int jagm_write_ppm(const JagMachine *m,const char *path)
{
    FILE *f=fopen(path,"wb");unsigned x,y,w=m->frame_w?m->frame_w:1u,h=m->frame_h?m->frame_h:1u;
    if(!f)return -1;
    fprintf(f,"P6\n%u %u\n255\n",w,h);
    for(y=0;y<h;y++)for(x=0;x<w;x++){uint32_t p=m->frame[y*JAGM_MAX_W+x];fputc((int)((p>>16)&255u),f);fputc((int)((p>>8)&255u),f);fputc((int)(p&255u),f);}
    fclose(f);return 0;
}
