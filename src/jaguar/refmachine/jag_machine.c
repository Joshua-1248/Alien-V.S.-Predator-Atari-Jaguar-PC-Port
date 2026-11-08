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
    if(m->jint_pending&m->jint_enable)m->int1_pending|=16u;
    m68k_set_irq((m->int1_pending&m->int1_enable&0x1Fu)?2:0);
}
unsigned long jagm_irq_acks;
int jagm_int_ack(int level){(void)level;jagm_irq_acks++;return 64;} /* bug 7: vector $40 */

/* ------------------------------------------------------------- memory */
static uint32_t ram_off(uint32_t a){return a&(JAGM_RAM_SIZE-1u);} /* 2 MiB repeats in DRAM0 */

static int in_gpu(uint32_t a){return a>=0xF02000u&&a<0xF04000u;}
static int in_dsp(uint32_t a){return a>=0xF1A000u&&a<0xF1E000u;}

static uint16_t tom_read16(JagMachine *m,uint32_t a);
static void tom_write16(JagMachine *m,uint32_t a,uint16_t v);
static uint16_t jerry_read16(JagMachine *m,uint32_t a);
static void jerry_write16(JagMachine *m,uint32_t a,uint16_t v);

/* 32-bit internal locations (GPU/DSP/Blitter space) viewed through 16 bits:
 * "transfers should always be performed in pairs, in the order low address
 * then high address" (p.31). */
static uint32_t local32_read(JagMachine *m,uint32_t a)
{
    uint32_t v=0;
    a&=~3u;
    if(in_gpu(a)){
        if(a>=0xF02200u&&a<0xF022A0u){(void)jag_blitter_read(&m->blit,a-0xF02200u,&v);return v;}
        if(!jrisc_local_read(&m->gpu,a,&v))note_unhandled(m,a,0);
        return v;
    }
    if(in_dsp(a)){
        if(a>=0xF1A140u&&a<0xF1A160u){ /* DAC/I2S block: SSTAT etc. */
            return 0;}
        if(!jrisc_local_read(&m->dsp,a,&v))note_unhandled(m,a,0);
        return v;
    }
    return 0;
}
static void blit_bus_sync(JagMachine *m);
static void local32_write(JagMachine *m,uint32_t a,uint32_t v)
{
    a&=~3u;
    if(in_gpu(a)){
        if(a>=0xF02200u&&a<0xF022A0u){
            blit_bus_sync(m);
            { int sv=master; if(a==0xF02238u)master=3; (void)jag_blitter_write(&m->blit,a-0xF02200u,v); master=sv; }
            if(a==0xF02238u)jrisc_raise(&m->gpu,4); /* blit completes synchronously: GPU int 4 (p.38) */
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
        case 0xF1A150u:m->sclk=(uint16_t)v;return;
        case 0xF1A154u:m->smode=(uint16_t)v;return;
        default:break;
        }
        if(!jrisc_local_write(&m->dsp,a,v))note_unhandled(m,a,1);
        update_cpu_irq(m);
        return;
    }
}

static int is_local32(uint32_t a)
{ return in_gpu(a)||in_dsp(a)||(a>=0xF0A000u&&a<0xF0C000u); }

uint16_t jagm_read16(JagMachine *m,uint32_t a)
{
    a&=0xFFFFFEu;
    if(a<0x800000u)return be16(m->ram+ram_off(a));
    if(a<0xE00000u){uint32_t o=a-0x800000u;return o+1u<m->rom_size?be16(m->rom+o):0xFFFFu;}
    if(a>=0xF00000u&&a<0xF10000u){
        if(in_gpu(a)){uint32_t l=local32_read(m,a);return (uint16_t)((a&2u)?l:(l>>16));}
        return tom_read16(m,a);
    }
    if(a>=0xF10000u&&a<0xF20000u){
        if(in_dsp(a)){uint32_t l=local32_read(m,a);return (uint16_t)((a&2u)?l:(l>>16));}
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
        /* low address (high half) first, commit on the high address */
        if(!(a&2u)){m->hold_addr=a;m->hold_val=(uint32_t)v<<16;m->hold_valid=1;
            if(a>=0xF03000u&&a<0xF04000u){uint32_t old=local32_read(m,a);local32_write(m,a,((uint32_t)v<<16)|(old&0xFFFFu));m->hold_valid=0;}
            else if(a>=0xF1B000u&&a<0xF1D000u){uint32_t old=local32_read(m,a);local32_write(m,a,((uint32_t)v<<16)|(old&0xFFFFu));m->hold_valid=0;}
            return;}
        if(m->hold_valid&&m->hold_addr==(a&~3u)){local32_write(m,a&~3u,m->hold_val|v);m->hold_valid=0;return;}
        { uint32_t old=local32_read(m,a&~3u);local32_write(m,a&~3u,(old&0xFFFF0000u)|v); }
        return;
    }
    if(a>=0xF00000u&&a<0xF10000u){tom_write16(m,a,v);return;}
    if(a>=0xF10000u&&a<0xF20000u){jerry_write16(m,a,v);return;}
    note_unhandled(m,a,1);
}
void jagm_write8(JagMachine *m,uint32_t a,uint8_t v)
{
    a&=0xFFFFFFu;
    if(a<0x800000u){m->ram[ram_off(a)]=v;if(watch_max>0)watch_hit(m,a,v,1);return;}
    if(a<0xE00000u)return;
    if(a>=0xF0A000u&&a<0xF0C000u)a-=0x8000u; /* 32-bit write alias of GPU space */
    { uint16_t w=jagm_read16(m,a&~1u);
      w=(a&1u)?(uint16_t)((w&0xFF00u)|v):(uint16_t)((w&0x00FFu)|((uint16_t)v<<8));
      jagm_write16(m,a&~1u,w); }
}
void jagm_write32(JagMachine *m,uint32_t a,uint32_t v)
{
    a&=0xFFFFFEu;
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
static uint8_t blit_rd8(void *c,uint32_t a)
{
    JagMachine *m=(JagMachine*)c;
    uint8_t v=jagm_read8(m,a);
    view_access(m,a,1u,0,3u);
    return v;
}
static void blit_wr8(void *c,uint32_t a,uint8_t v)
{
    JagMachine *m=(JagMachine*)c;
    a&=0xFFFFFFu;
    if(a<0x800000u){m->ram[ram_off(a)]=v;if(watch_max>0)watch_hit(m,a,v,1);view_access(m,a,1u,1,3u);return;}
    /* byte writes into 32-bit internal space go through read-modify-write */
    jagm_write8(m,a,v);
}
static void blit_bus_sync(JagMachine *m){(void)m;}
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
    case 0xF00004u:return (uint16_t)m->hc_half;
    case 0xF00006u:return (uint16_t)m->vc;
    case 0xF00010u:return (uint16_t)(m->ob>>48);
    case 0xF00012u:return (uint16_t)(m->ob>>32);
    case 0xF00014u:return (uint16_t)(m->ob>>16);
    case 0xF00016u:return (uint16_t)m->ob;
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
    case 0xF00026u:m->obf=v;return;
    case 0xF00028u:m->vmode=v;return;
    case 0xF0002Au:m->bord1=v;return; case 0xF0002Cu:m->bord2=v;return;
    case 0xF0002Eu:m->hp=v&0x3FFu;return; case 0xF00030u:m->hbb=v;return;
    case 0xF00032u:m->hbe=v;return; case 0xF00034u:m->hs=v;return;
    case 0xF00036u:m->hvs=v;return; case 0xF00038u:m->hdb1=v;return;
    case 0xF0003Au:m->hdb2=v;return; case 0xF0003Cu:m->hde=v;return;
    case 0xF0003Eu:m->vp=v&0x7FFu;return; case 0xF00040u:m->vbb=v;return;
    case 0xF00042u:m->vbe=v;return; case 0xF00044u:m->vs=v;return;
    case 0xF00046u:m->vdb=v&0x7FFu;return; case 0xF00048u:m->vde=v&0x7FFu;return;
    case 0xF0004Au:m->veb=v;return; case 0xF0004Cu:m->vee=v;return;
    case 0xF0004Eu:m->vi=v&0x7FFu;return;
    case 0xF00050u:m->pit0=v;m->pit_acc=0;return; case 0xF00052u:m->pit1=v;m->pit_acc=0;return;
    case 0xF00054u:m->heq=v;return; case 0xF00058u:m->bg=v;return;
    case 0xF000E0u: /* INT1: enables bits 0-4, clear bits 8-12 */
        m->int1_enable=v&0x1Fu;m->int1_pending&=(uint16_t)~((v>>8)&0x1Fu);
        if(v&(1u<<(8+4)))m->jint_pending&=0; /* Jerry line is re-evaluated */
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
static uint16_t jerry_read16(JagMachine *m,uint32_t a)
{
    switch(a){
    case 0xF10020u:return m->jint_pending&0x3Fu;
    case 0xF10036u:case 0xF10038u:case 0xF1003Au:case 0xF1003Cu:return 0; /* timer readback */
    case 0xF14000u:{ /* JOY1 (p.95): active-low inputs selected by the output latch */
        uint16_t sel=(uint16_t)(m->joy_out&0x0Fu),r=0xFFFFu;int row=-1;
        if(!(sel&1u))row=0;else if(!(sel&2u))row=1;else if(!(sel&4u))row=2;else if(!(sel&8u))row=3;
        if(row>=0){uint32_t p=m->pad_state[0];unsigned nib=(p>>(row*4))&15u;r=(uint16_t)(r&~((nib&15u)<<8));}
        return r;}
    case 0xF14002u:{ /* JOY2 buttons B0..B3 (active low) */
        uint16_t sel=(uint16_t)(m->joy_out&0x0Fu),r=0xFFFFu;int row=-1;
        if(!(sel&1u))row=0;else if(!(sel&2u))row=1;else if(!(sel&4u))row=2;else if(!(sel&8u))row=3;
        if(row>=0){uint32_t p=m->pad_state[0];unsigned b=(p>>(16+row*2))&3u;r=(uint16_t)(r&~b);}
        return r;}
    case 0xF10032u:return (uint16_t)(0x0100u|(m->asictrl&0x3Fu)); /* ASISTAT: TBE */
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
    case 0xF10030u:return;
    case 0xF10032u:m->asictrl=v;return;
    case 0xF10034u:m->asiclk=v;return;
    case 0xF14000u:m->joy_out=v;return;
    default:break;
    }
    note_unhandled(m,a,1);
}

/* ------------------------------------------------------------ Musashi */
unsigned int m68k_read_memory_8(unsigned int a){return jagm_read8(M,a);}
unsigned int m68k_read_memory_16(unsigned int a){return jagm_read16(M,a);}
unsigned int m68k_read_memory_32(unsigned int a){return jagm_read32(M,a);}
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
{jagm_write32(M,a,v);cpu_dram_write(a&~1u,4u);}

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
    int dir=reflect?-1:1;
    unsigned ypos=(unsigned)((p0>>3)&0x7FFu);
    if(m->vc<ypos||height==0)return;      /* p.18: active while VC>=YPOS and HEIGHT>0 */
    if(depth==4u||depth==5u){
        m->op_deep_bitmap_visits++;
        if((p1>>48)&1u)m->op_deep_release_visits++;
    }
    if(depth>5u){m->op_unsupported_mask|=1u;return;}
    if(firstpix){m->op_unsupported_mask|=2u;}
    if(bpp==24u&&(trans||rmw))m->op_unsupported_mask|=4u;
    if(!scaled){
        int x=xpos;
        for(s=0;s<n;s++,x+=dir){
            uint32_t raw=op_src_pixel(m,data,pitch,bpp,s);
            if(bpp==24u){ if(x>=0&&x<360){m->lbuf[m->lbuf_write][2*x]=(uint16_t)(raw>>16);m->lbuf[m->lbuf_write][2*x+1]=(uint16_t)raw;} continue; }
            if(trans&&raw==0u)continue;   /* p.7/p.19: colour zero, 1..16 bpp */
            if(bpp<=8u)raw=m->clut[(((index>>(bpp-1u))<<bpp)|raw)&255u];
            op_put(m,x,(uint16_t)raw,rmw);
        }
        /* write back DATA += DWIDTH, HEIGHT -= 1 (p.18) */
        { uint64_t np=p0;uint32_t nd=(data>>3)+dwidth;unsigned nh=height?height-1u:0u;
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
            raw=op_src_pixel(m,data,pitch,bpp,s);
            if(bpp==24u){m->op_unsupported_mask|=8u;continue;}
            if(trans&&raw==0u)continue;
            if(bpp<=8u)raw=m->clut[(((index>>(bpp-1u))<<bpp)|raw)&255u];
            op_put(m,x,(uint16_t)raw,rmw);
        }
        /* p.19 REMAINDER: minus one line (1.0 = 0x20); while negative add
           VSCALE, decrementing HEIGHT and advancing DATA by DWIDTH. */
        remv=(int)rem-32;
        while(remv<0&&nh){remv+=(int)vscale;nh--;nd+=dwidth;if(!vscale)break;}
        if(remv<0)remv=0;
        { uint64_t np=p0,np2=p2;
          np&=~((uint64_t)0x1FFFFFu<<43);np|=(uint64_t)(nd&0x1FFFFFu)<<43;
          np&=~((uint64_t)0x3FFu<<14);np|=(uint64_t)nh<<14;wr64(m,addr,np);
          np2&=~((uint64_t)0xFFu<<16);np2|=(uint64_t)((unsigned)remv&0xFFu)<<16;wr64(m,addr+16u,np2); }
    }
}

static void run_gpu_until_obf(JagMachine *m);

static void op_line(JagMachine *m)
{
    uint32_t a=m->olp&0xFFFFF8u;
    unsigned guard=0;
    int preceding_active_rmw=0;
    m->op_lines++;
    while(guard++<4096u){
        uint64_t p0=rd64(m,a);unsigned type=(unsigned)(p0&7u);
        uint32_t link=(uint32_t)((p0>>24)&0x7FFFFu)<<3;
        uint32_t next=(a&~0x3FFFF8u)|link; /* p.18: LINK replaces OLP bits 3..21 */
        m->op_objects++;
        if(type!=0u&&type!=1u)preceding_active_rmw=0;
        switch(type){
        case 0: case 1:{
            uint64_t p1=rd64(m,a+8u);
            unsigned ypos=(unsigned)((p0>>3)&0x7FFu);
            unsigned height=(unsigned)((p0>>14)&0x3FFu);
            int active=m->vc>=ypos&&height!=0u;
            /* JDEV0002 OP item 1: immediately following active bitmap
               can corrupt RMW edge. Do not guess the corrupted color. */
            if(preceding_active_rmw&&active)m->op_rmw_neighbor_visits++;
            preceding_active_rmw=active&&((p1>>46)&1u);
            op_bitmap(m,a,p0,p1,type==1u?rd64(m,a+16u):0u,type==1u);
            a=next;break;
        }
        case 2:{unsigned y=(unsigned)((p0>>3)&0x7FFu);
            if(y==0x7FFu||y==m->vc){m->ob=p0;m->op_gpu_objects++;jrisc_raise(&m->gpu,3);run_gpu_until_obf(m);}
            a+=8u;break;}
        case 3:{unsigned y=(unsigned)((p0>>3)&0x7FFu),cc=(unsigned)((p0>>14)&3u);int t=0;
            switch(cc){case 0:t=(y==m->vc||y==0x7FFu);break;case 1:t=(y>m->vc);break;
                case 2:t=(y<m->vc);break;default:t=(m->obf&1u)!=0;break;}
            a=t?next:a+8u;break;}
        case 4:m->int1_pending|=4u;update_cpu_irq(m);return; /* STOP (p.20) */
        default:m->op_unsupported_mask|=16u;return;
        }
    }
}

/* GPU object: the OP waits for a write to OBF (p.12).  Run only the GPU. */
static void run_gpu_until_obf(JagMachine *m)
{
    uint16_t before=m->obf;unsigned i;
    (void)before;
    m->obf=(uint16_t)(m->obf|0x8000u); /* sentinel: real OBF bit0 is preserved */
    for(i=0;i<200000u&&m->gpu.go;i++){
        jrisc_step(&m->gpu);
        if(!(m->obf&0x8000u))return;
    }
    m->obf&=0x7FFFu;m->op_unsupported_mask|=32u;
}

/* ------------------------------------------------------------- video out */
static void output_line(JagMachine *m,unsigned row)
{
    unsigned mode=(m->vmode>>1)&3u,pw=((m->vmode>>9)&7u)+1u,w,x;
    unsigned hp2=(unsigned)m->hp+1u;
    int hdb=(int)(m->hdb1&0x3FFu)+((m->hdb1&0x400u)?(int)hp2:0);
    int hde=(int)(m->hde&0x3FFu)+((m->hde&0x400u)?(int)hp2:0);
    uint16_t *lb=m->lbuf[m->lbuf_write];
    if(row>=JAGM_MAX_H)return;
    w=hde>hdb?(unsigned)(hde-hdb)/pw:0u;
    if(mode==1u)w=w>360u?360u:w; else w=w>JAGM_MAX_W?JAGM_MAX_W:w;
    if(w>m->frame_w)m->frame_w=w;
    for(x=0;x<w;x++){
        uint32_t rgb;
        if(mode==1u)rgb=avp_jaguar_vmode_rgb24(lb[2*x],lb[2*x+1]); /* p.17 byte order */
        else{uint16_t v=lb[x];
            if((m->vmode&0x100u)&&(v&1u))rgb=avp_jaguar_vmode_rgb16((uint16_t)(v&~1u));
            else if(mode==3u)rgb=avp_jaguar_vmode_rgb16(v);
            else{uint8_t r,g,b;avp_jaguar_cry16_to_rgb(v,&r,&g,&b);rgb=((uint32_t)r<<16)|((uint32_t)g<<8)|b;}}
        m->work[row*JAGM_MAX_W+x]=rgb;
    }
    /* BGEN (p.13): clear after display in CRY/RGB16 */
    if((m->vmode&0x80u)&&mode!=1u)for(x=0;x<720u;x++)lb[x]=m->bg;
}

/* ------------------------------------------------------------- scheduling */
static void run_slice(JagMachine *m,unsigned sys_ticks)
{
    unsigned gt,dt;
    /* 68000 at half the system clock */
    m->cpu_cycle_debt+=sys_ticks/2.0;
    master=0;
    if(m->cpu_cycle_debt>=1.0){int n=(int)m->cpu_cycle_debt;int used=m68k_execute(n);m->cpu_cycle_debt-=used;}
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
        {unsigned c=jrisc_step(&m->gpu);if(!c)break;gt+=c;}}
    master=2;
    for(dt=0;dt<sys_ticks&&m->dsp.go;){unsigned c=jrisc_step(&m->dsp);if(!c)break;dt+=c;}
    update_cpu_irq(m);
    m->sys_cycles+=sys_ticks;
    /* TOM PIT (p.16): sysclk/(PIT0+1)/(PIT1+1) -> CPU int 3 and GPU int 2 */
    if(m->pit0){uint64_t per=(uint64_t)(m->pit0+1u)*(uint64_t)(m->pit1+1u);m->pit_acc+=sys_ticks;
        while(m->pit_acc>=per){m->pit_acc-=per;m->int1_pending|=8u;jrisc_raise(&m->gpu,2);update_cpu_irq(m);}}
    /* JERRY timers (p.85): -> DSP int 2/3 and CPU (F10020 bits 2/3) */
    for(int t=0;t<2;t++){uint16_t pre=m->jpit[t*2],div=m->jpit[t*2+1];
        if(pre){uint64_t per=(uint64_t)(pre+1u)*(uint64_t)(div+1u);m->jpit_acc[t]+=sys_ticks;
            while(m->jpit_acc[t]>=per){m->jpit_acc[t]-=per;jrisc_raise(&m->dsp,2u+(unsigned)t);m->jint_pending|=(uint16_t)(4u<<t);update_cpu_irq(m);}}}
    /* JERRY I2S (p.90-91): internal clock sysclk/(2(N+1)); 32 bit clocks per
       stereo word pair; RISING enables the word-strobe interrupt (DSP int 1). */
    if((m->smode&1u)&&(m->smode&(8u|16u|32u))){uint64_t per=(uint64_t)2u*(m->sclk+1u)*32u;m->i2s_acc+=sys_ticks;
        while(m->i2s_acc>=per){m->i2s_acc-=per;jrisc_raise(&m->dsp,1);
            if(m->capture_audio){if(m->audio_n+2u>m->audio_cap){size_t nc=m->audio_cap?m->audio_cap*2u:65536u;int16_t *na=(int16_t*)realloc(m->audio,nc*sizeof(int16_t));if(!na)break;m->audio=na;m->audio_cap=nc;}
                m->audio[m->audio_n++]=(int16_t)m->ltxd;m->audio[m->audio_n++]=(int16_t)m->rtxd;}}}
}

static void half_line(JagMachine *m)
{
    unsigned hp2=(unsigned)m->hp+1u;
    unsigned period=hp2?hp2:845u;
    unsigned slices=8u,i;
    int line_start=!(m->vc&1u);
    /* OP at the start of each display line between VDB and VDE (p.15) */
    if(line_start&&m->vc>=m->vdb&&m->vc<m->vde&&(m->vmode&1u)){
        op_line(m);
        output_line(m,(m->vc-m->vdb)/2u);
        m->lbuf_write^=1u;
    }
    for(i=0;i<slices;i++){run_slice(m,period/slices+(i<period%slices?1u:0u));if(m->halted&&m->gpu_break_pc&&!m->gpu_break_armed)return;}
    m->vc++;
    if(m->vc==m->vi){m->int1_pending|=1u;update_cpu_irq(m);} /* VI (p.16) */
    if(m->vc>(unsigned)(m->vp?m->vp:523u)){
        unsigned h=m->vde>m->vdb?(unsigned)(m->vde-m->vdb)/2u:0u;
        m->vc=0;m->frames++;
        m->frame_h=h>JAGM_MAX_H?JAGM_MAX_H:h;
        memcpy(m->frame,m->work,sizeof(m->frame));
        m->frame_w=m->frame_w;
        if(m->on_frame)m->on_frame(m,m->on_frame_ctx);
    }
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
