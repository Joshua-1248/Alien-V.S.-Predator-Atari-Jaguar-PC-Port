/* R150: Observe B_CMD status polling in the original ROM reference run.
 * These are model MMIO reads with scheduler markers, NOT real bus stalls. */
#include "jaguar/jag_machine.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t reads,reads_gpu,reads_68000,reads_other;
static uint64_t idle_reads,stopped_reads,unexpected_status;
static uint64_t commands,commands_gpu,commands_68000;
static uint64_t read_after_cmd_same_slice,invalid_gpu_pc;
static uint64_t last_command_sys_cycle;
static int have_last_command;
static void on_cmd(JagMachine *m,const JagBlitMmioEvent *e,void *ctx)
{
    (void)m;(void)ctx;
    if(e->addr==0xF02238u){
        commands++;
        if(e->master==1u)commands_gpu++;
        else if(e->master==0u)commands_68000++;
        last_command_sys_cycle=e->cycles_after;
        have_last_command=1;
    }
}
static void on_status(JagMachine *m,const JagBlitStatusReadEvent *e,void *ctx)
{
    (void)m;(void)ctx;
    reads++;
    if(e->master==1u){reads_gpu++;
        if(e->issuer_pc<0xF00000u||e->issuer_pc>=0xF10000u)invalid_gpu_pc++;
    }else if(e->master==0u)reads_68000++;
    else reads_other++;
    if(e->status&1u)idle_reads++;
    if(e->status&2u)stopped_reads++;
    if((e->status&0x805u)!=0x805u && !(e->status&2u))unexpected_status++;
    if(have_last_command && last_command_sys_cycle==e->sys_cycles)read_after_cmd_same_slice++;
}
int main(int argc,char **argv)
{
    FILE *f;long n;uint8_t *rom;JagMachine *m;unsigned fields=600u;
    if(argc<2||argc>3){fprintf(stderr,"usage: %s /private/AVP.jag [fields=600]\n",argv[0]);return 2;}
    if(argc==3){char *end=NULL;unsigned long v=strtoul(argv[2],&end,10);
        if(!end||*end||v==0u||v>6000u){fputs("fields must be 1..6000\n",stderr);return 2;}
        fields=(unsigned)v;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)||(n=ftell(f))!=4194304L||fseek(f,0,SEEK_SET)){
        fputs("original 4-MiB ROM required\n",stderr);fclose(f);return 2;}
    rom=(uint8_t*)malloc((size_t)n);m=(JagMachine*)malloc(sizeof *m);
    if(!rom||!m){fputs("out of memory\n",stderr);fclose(f);free(rom);free(m);return 2;}
    if(fread(rom,1,(size_t)n,f)!=(size_t)n){fputs("ROM read failed\n",stderr);fclose(f);free(rom);free(m);return 2;}
    fclose(f);
    if(jagm_init(m,rom,(size_t)n)){fputs("reference boot failed\n",stderr);free(rom);free(m);return 2;}
    m->on_blit_mmio=on_cmd;
    m->on_blit_status_read=on_status;
    jagm_run_frames(m,fields);
    printf("fields=%" PRIu64 " requested=%u halted=%d model_scheduler_cycles=%" PRIu64 "\n",m->frames,fields,m->halted,m->sys_cycles);
    printf("command_writes=%" PRIu64 " gpu=%" PRIu64 " cpu68000=%" PRIu64 "\n",commands,commands_gpu,commands_68000);
    printf("b_cmd_status_reads=%" PRIu64 " gpu=%" PRIu64 " cpu68000=%" PRIu64 " other=%" PRIu64 "\n",reads,reads_gpu,reads_68000,reads_other);
    printf("idle_read_values=%" PRIu64 " stopped_read_values=%" PRIu64 " unexpected_status=%" PRIu64 "\n",idle_reads,stopped_reads,unexpected_status);
    printf("status_reads_same_scheduler_stamp_as_last_command=%" PRIu64 " invalid_gpu_issuer_pc=%" PRIu64 "\n",read_after_cmd_same_slice,invalid_gpu_pc);
    puts("Read counts are software-model register accesses; scheduler markers are not Jaguar bus cycles or busy durations.");
    free(rom);free(m);return 0;
}
