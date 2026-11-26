/* Count-only GPU/Blitter instruction provenance and model byte requests.
 * These are NOT physical Jaguar bus cycles or actual hardware latencies. */
#include "jaguar/jag_machine.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t commands,masters[5],reads,writes;
static uint64_t gpu_first_steps,gpu_last_steps,gpu_first_ticks,gpu_last_ticks;
static uint64_t missing_pc,wrong_master,model_zero_scheduler;
static uint64_t max_read_per_command,max_write_per_command;
static int first_gpu;
static void observe(JagMachine *m,const JagBlitMmioEvent *e,void *ctx)
{
    uint64_t dr=e->reads_after-e->reads_before;
    uint64_t dw=e->writes_after-e->writes_before;
    (void)m;(void)ctx;
    if(e->master<5u)masters[e->master]++;
    if(e->addr!=0xF02238u)return;
    commands++;reads+=dr;writes+=dw;
    if(dr>max_read_per_command)max_read_per_command=dr;
    if(dw>max_write_per_command)max_write_per_command=dw;
    if(e->cycles_before==e->cycles_after)model_zero_scheduler++;
    if(e->master==1u){
        if(!first_gpu){gpu_first_steps=e->gpu_steps_completed;
            gpu_first_ticks=e->gpu_model_ticks_completed;first_gpu=1;}
        gpu_last_steps=e->gpu_steps_completed;
        gpu_last_ticks=e->gpu_model_ticks_completed;
        if(e->issuer_pc<0xF00000u||e->issuer_pc>=0xF10000u)missing_pc++;
    }else if(e->master>2u)wrong_master++;
}
int main(int argc,char **argv)
{
    FILE *f;long n;uint8_t *rom;JagMachine *m;unsigned fields=600u;
    if(argc<2||argc>3){fprintf(stderr,"usage: %s /private/AVP.jag [fields=600]\n",argv[0]);return 2;}
    if(argc==3){char *end=NULL;unsigned long v=strtoul(argv[2],&end,10);
        if(!end||*end||!v||v>6000u){fputs("fields must be 1..6000\n",stderr);return 2;}
        fields=(unsigned)v;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)||(n=ftell(f))!=4194304L||fseek(f,0,SEEK_SET)){
        fputs("original 4-MiB ROM required\n",stderr);fclose(f);return 2;}
    rom=(uint8_t*)malloc((size_t)n);m=(JagMachine*)malloc(sizeof *m);
    if(!rom||!m){fputs("out of memory\n",stderr);fclose(f);free(rom);free(m);return 2;}
    if(fread(rom,1,(size_t)n,f)!=(size_t)n){fputs("ROM read failed\n",stderr);fclose(f);free(rom);free(m);return 2;}
    fclose(f);
    if(jagm_init(m,rom,(size_t)n)){fputs("reference boot failed\n",stderr);free(rom);free(m);return 2;}
    m->on_blit_mmio=observe;
    jagm_run_frames(m,fields);
    printf("fields=%" PRIu64 " requested=%u halted=%d model_scheduler_cycles=%" PRIu64 "\n",m->frames,fields,m->halted,m->sys_cycles);
    printf("blitter_commands=%" PRIu64 " gpu=%" PRIu64 " cpu68000=%" PRIu64 " dsp=%" PRIu64 " other=%" PRIu64 "\n",commands,masters[1],masters[0],masters[2],masters[3]+masters[4]);
    printf("blitter_model_byte_read_requests=%" PRIu64 " byte_write_requests=%" PRIu64 "\n",reads,writes);
    printf("max_model_reads_one_command=%" PRIu64 " max_model_writes_one_command=%" PRIu64 "\n",max_read_per_command,max_write_per_command);
    printf("gpu_command_retired_steps_first=%" PRIu64 " last=%" PRIu64 " ticks_first=%" PRIu64 " last=%" PRIu64 "\n",gpu_first_steps,gpu_last_steps,gpu_first_ticks,gpu_last_ticks);
    printf("gpu_unexpected_issuer_pc=%" PRIu64 " unassigned_owner=%" PRIu64 " zero_scheduler_delta=%" PRIu64 "\n",missing_pc,wrong_master,model_zero_scheduler);
    puts("Model read/write calls are byte-sized requests, not 64-bit bus transfers, arbitration measurements, or hardware cycle counts.");
    free(rom);free(m);return 0;
}
