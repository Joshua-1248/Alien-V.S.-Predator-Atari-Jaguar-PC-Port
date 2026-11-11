/* RE36 R102: reference-only, per-transaction read-only Jaguar CROSS mailbox
 * observer.  DO NOT treat the model's cycle counts, order inside a scheduler
 * slice, GPU execution, or rendering as hardware timing.  This emits NO
 * proprietary payloads: only control addresses / scalar state / PCs. */
#include "jaguar/jag_machine.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STAGE 0x7fb4u
#define STATUS 0x7f80u
#define IMAGE 0x7fa4u
#define ACK 0x7fa8u

typedef struct TraceState { uint64_t next_event; unsigned first, last, printed; } TraceState;

static int watched(uint32_t address, unsigned width)
{
    static const uint32_t keys[]={STAGE,STATUS,IMAGE,ACK};
    address &= 0xffffffu;
    for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);i++)
        if(address<=keys[i] && address+width>keys[i])return 1;
    /* The original 68000 OP record handoff. Avoid dumping graphic payload. */
    return (address>=0x4fc40u&&address<0x4fc50u)
         ||(address>=0x50c40u&&address<0x50c50u);
}

static void emit(JagMachine *m,TraceState *s,const char *who,const char *action,
                 uint32_t addr,unsigned width,uint32_t val)
{
    if(m->frames+1u<s->first||m->frames>s->last)return;
    /* cycle and halfline are approximate JagMachine scheduler coordinates,
       NOT absolute hardware timestamps or bus-arbitrated cycles. */
    printf("%llu,%llu,%u,%llu,%s,%s,%06x,%u,%08x,%06x,%06x,%u,%u,%06x,%u,%06x\n",
       (unsigned long long)++s->next_event,
       (unsigned long long)m->frames,m->vc,
       (unsigned long long)m->sys_cycles,who,action,addr,width,val,
       m68k_get_reg(NULL,M68K_REG_PC)&0xffffffu,m->gpu.pc,
       jagm_read32(m,STAGE),jagm_read32(m,STATUS),jagm_read32(m,IMAGE),
       jagm_read32(m,ACK),m->olp);
    s->printed++;
}
static void cpu_cb(JagMachine *m,const JagCpuDramWriteEvent *e,void *ctx)
{
    if(watched(e->address,e->width))
        emit(m,(TraceState*)ctx,"68000","write",e->address,e->width,
             e->width==1u?jagm_read8(m,e->address):e->width==2u?jagm_read16(m,e->address):jagm_read32(m,e->address));
}
static void gpu_cb(JagMachine *m,const JagGpuViewDramAccess *e,void *ctx)
{
    if(e->is_write&&watched(e->address,e->width))
        emit(m,(TraceState*)ctx,e->master==1u?"GPU":"Blitter","write",
             e->address,e->width,e->width==1u?jagm_read8(m,e->address):
             e->width==2u?jagm_read16(m,e->address):jagm_read32(m,e->address));
}
static void ctrl_cb(JagMachine *m,const JagGpuControlEvent *e,void *ctx)
{
    if(e->addr==0xf02110u||e->addr==0xf02114u)
        emit(m,(TraceState*)ctx,e->master==0u?"68000":"GPU","gpu_ctrl",
             e->addr,4u,e->value);
}
static void field_cb(JagMachine *m,void *ctx)
{
    TraceState *s=(TraceState*)ctx;
    if(m->frames>=s->first&&m->frames<=s->last)
        emit(m,s,"MODEL","field_end",0u,0u,0u);
}
int main(int argc,char **argv)
{
    if(argc<2||argc>4){fprintf(stderr,"usage: %s retail-AVP.jag [first-field=1718] [last-field=1748]\n",argv[0]);return 2;}
    TraceState st={0,1718u,1748u,0u};
    if(argc>2)st.first=(unsigned)strtoul(argv[2],NULL,10);
    if(argc>3)st.last=(unsigned)strtoul(argv[3],NULL,10);
    if(st.first<1||st.first>st.last||st.last>60000u){fputs("invalid field interval\n",stderr);return 2;}
    FILE *f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return 2;}
    long n=ftell(f);if(n!=4194304L){fclose(f);fputs("expected 4 MiB retail ROM\n",stderr);return 2;}
    rewind(f);
    uint8_t *rom=(uint8_t*)malloc((size_t)n);
    JagMachine *m=(JagMachine*)malloc(sizeof(*m));
    if(!rom||!m){fclose(f);free(rom);free(m);return 2;}
    size_t got=fread(rom,1,(size_t)n,f);fclose(f);
    if(got!=(size_t)n||jagm_init(m,rom,got)!=0){free(rom);free(m);return 2;}
    m->on_cpu_dram_write=cpu_cb;m->on_cpu_dram_write_ctx=&st;
    m->on_gpu_view_dram_access=gpu_cb;m->on_gpu_view_dram_access_ctx=&st;
    m->on_gpu_control=ctrl_cb;m->on_gpu_control_ctx=&st;
    /* Do not use on_frame to claim scanout order: the model calls on_frame
       after each completed field; these are field-boundary samples only. */
    m->on_frame=field_cb;m->on_frame_ctx=&st;
    puts("event,model_field,model_halfline,model_slice_cycles,master,action,address,width,value,pc68,gpu_pc,stage_7fb4,status_7f80,image_7fa4,ack_7fa8,op_list_pointer");
    for(unsigned field=1;field<=st.last;field++){
        jagm_set_pad(m,0u,0u);
        jagm_run_frames(m,1u);
        if(m->halted){fprintf(stderr,"reference halted at field %u: %s\n",field,m->halt_reason);break;}
    }
    const int ok=(m->frames>=st.last);
    fprintf(stderr,"R102 model transactions: %u events in fields [%u,%u], %s\n",st.printed,st.first,st.last,ok?"complete":"halted");
    free(m->audio);free(m);free(rom);
    return ok?0:1;
}
