/* R152: per-memory-region model byte callback totals. Not bus transactions. */
#include "jaguar/jag_machine.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t cmds, gpu_cmds, cpu_cmds, cmd_reads, cmd_writes;
static uint64_t command_read_regions[JAGM_BLIT_REGION_COUNT];
static uint64_t command_write_regions[JAGM_BLIT_REGION_COUNT];
static void observe(JagMachine *m,const JagBlitMmioEvent *e,void *ctx)
{
    (void)m;(void)ctx;
    if(e->addr!=0xF02238u)return;
    cmds++;
    if(e->master==1u)gpu_cmds++;
    if(e->master==0u)cpu_cmds++;
    cmd_reads+=e->reads_after-e->reads_before;
    cmd_writes+=e->writes_after-e->writes_before;
    for(unsigned i=0;i<JAGM_BLIT_REGION_COUNT;i++){
        command_read_regions[i]+=e->region_reads_after[i]-e->region_reads_before[i];
        command_write_regions[i]+=e->region_writes_after[i]-e->region_writes_before[i];
    }
}
int main(int argc,char **argv)
{
    static const char *names[]={"dram", "cart", "other"};
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
    printf("fields=%" PRIu64 " requested=%u halted=%d commands=%" PRIu64 " gpu=%" PRIu64 " cpu=%" PRIu64 "\n",
           m->frames,fields,m->halted,cmds,gpu_cmds,cpu_cmds);
    printf("per_command_reads=%" PRIu64 " per_command_writes=%" PRIu64 "\n",cmd_reads,cmd_writes);
    printf("global_reads=%" PRIu64 " global_writes=%" PRIu64 "\n",m->blit_read_requests,m->blit_write_requests);
    for(unsigned i=0;i<JAGM_BLIT_REGION_COUNT;i++)
        printf("%s command_read_bytes=%" PRIu64 " command_write_bytes=%" PRIu64
               " global_read_bytes=%" PRIu64 " global_write_bytes=%" PRIu64 "\n",
               names[i],command_read_regions[i],command_write_regions[i],
               m->blit_region_reads[i],m->blit_region_writes[i]);
    /* There are no control writes outside a synchronous command during the
     * observed game trace; region totals must add to global totals regardless. */
    {uint64_t rr=0,ww=0;
     for(unsigned i=0;i<JAGM_BLIT_REGION_COUNT;i++){
         rr+=m->blit_region_reads[i];ww+=m->blit_region_writes[i];}
     if(rr!=m->blit_read_requests||ww!=m->blit_write_requests){
         fputs("region counter invariant failed\n",stderr);goto done;}}
    puts("Software byte callbacks only. Not bus transactions, latency, or arbitration.");
    result=(m->halted||m->frames!=fields)?1:0;
    if(result&&m->halted)fprintf(stderr,"reference halted: %s\n",m->halt_reason);
done:
    if(f)fclose(f);
    free(m);free(rom);
    return result;
}
