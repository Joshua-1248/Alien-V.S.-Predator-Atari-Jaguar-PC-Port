/* RE38 R151: compare per-command Blitter bus callback requests with the
 * reference-machine global request counters. No proprietary bytes exported.
 * Requests are not Jaguar memory bus cycles or arbitration measurements. */
#include "jaguar/jag_machine.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t cmd_count,cmd_reads,cmd_writes;
static uint64_t gpu_cmds,cpu_cmds,stop_commands;
static void observe(JagMachine *m,const JagBlitMmioEvent *e,void *ctx)
{
    (void)m;(void)ctx;
    if(e->addr!=0xF02238u){stop_commands++;return;}
    cmd_count++;
    cmd_reads+=e->reads_after-e->reads_before;
    cmd_writes+=e->writes_after-e->writes_before;
    if(e->master==1u)gpu_cmds++;
    if(e->master==0u)cpu_cmds++;
}
int main(int argc,char **argv)
{
    FILE *f=NULL;long n;uint8_t *rom=NULL;JagMachine *m=NULL;
    unsigned fields=600u;int result=2;
    if(argc!=2&&argc!=3){fprintf(stderr,"usage: %s /private/AVP.jag [fields=600]\n",argv[0]);return 2;}
    if(argc==3){char *end=NULL;unsigned long v=strtoul(argv[2],&end,10);
        if(!end||*end||!v||v>6000u){fputs("fields must be 1..6000\n",stderr);return 2;}
        fields=(unsigned)v;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)||(n=ftell(f))!=4194304L||fseek(f,0,SEEK_SET)){
        fputs("original 4-MiB Jaguar ROM is required\n",stderr);goto done;}
    rom=(uint8_t*)malloc((size_t)n);m=(JagMachine*)malloc(sizeof *m);
    if(!rom||!m){fputs("out of memory\n",stderr);goto done;}
    if(fread(rom,1,(size_t)n,f)!=(size_t)n){fputs("ROM read failed\n",stderr);goto done;}
    fclose(f);f=NULL;
    if(jagm_init(m,rom,(size_t)n)){fputs("reference boot failed\n",stderr);goto done;}
    m->on_blit_mmio=observe;
    jagm_run_frames(m,fields);
    printf("fields=%" PRIu64 " requested=%u halted=%d\n",m->frames,fields,m->halted);
    printf("commands=%" PRIu64 " gpu=%" PRIu64 " cpu68000=%" PRIu64 " stop_controls=%" PRIu64 "\n",
           cmd_count,gpu_cmds,cpu_cmds,stop_commands);
    printf("per_command_reads=%" PRIu64 " per_command_writes=%" PRIu64 "\n",cmd_reads,cmd_writes);
    printf("global_blitter_reads=%" PRIu64 " global_blitter_writes=%" PRIu64 "\n",
           m->blit_read_requests,m->blit_write_requests);
    printf("write_counter_difference=%" PRIu64 "\n",
           m->blit_write_requests>=cmd_writes?m->blit_write_requests-cmd_writes:0u);
    puts("Only software byte requests. No physical bus cycles or timing inferred.");
    result=(m->halted||m->frames!=fields)?1:0;
    if(result&&m->halted)fprintf(stderr,"reference halted: %s\n",m->halt_reason);
done:
    if(f)fclose(f);
    free(m);free(rom);
    return result;
}
