/* ROM-run instrumentation for Atari Jaguar TOM Blitter command lifecycle.
 * Uses the original user-provided cartridge, never stores ROM data, and does
 * not claim that model scheduler timestamps correspond to Jaguar bus cycles.
 * Usage: blitter_irq_trace_r147 /private/AVP.jag [fields=600]
 */
#include "jaguar/jag_machine.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t commands, nogos, stopped, resumes, aborts, completions, unchanged;
static uint64_t pseudo_zero_cycle_transfers,blits_before,blits_after;
static uint64_t masters[5];
static void observe(JagMachine *m,const JagBlitMmioEvent *e,void *ctx)
{
    (void)m;(void)ctx;
    if(e->master<5)masters[e->master]++;
    if(e->addr==0xF02238u){
        commands++;
        if(e->value&0x80u)nogos++;
        if(e->stopped_after&&!e->stopped_before)stopped++;
    }else{
        if(e->value&1u)resumes++;
        if(e->value&2u)aborts++;
    }
    if(!e->gpu_irq_before&&e->gpu_irq_after)completions++;
    if(e->blits_after>e->blits_before){
        blits_after+=e->blits_after-e->blits_before;
        blits_before++;
        if(e->cycles_before==e->cycles_after)pseudo_zero_cycle_transfers++;
    }
    if(e->blits_after==e->blits_before && e->pixels_written_after==e->pixels_written_before)unchanged++;
}
int main(int argc,char **argv)
{
    FILE *f;long len;uint8_t *rom;JagMachine *m;unsigned fields=600u;
    if(argc<2||argc>3){fprintf(stderr,"usage: %s /private/AVP.jag [fields=600]\n",argv[0]);return 2;}
    if(argc==3){char *end=NULL;unsigned long n=strtoul(argv[2],&end,10);
        if(!end||*end||!n||n>6000u){fputs("fields must be 1..6000\n",stderr);return 2;}
        fields=(unsigned)n;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)||((len=ftell(f))!=4194304L)||fseek(f,0,SEEK_SET)){
        fputs("Expected original 4-MiB ROM; refusing other files\n",stderr);fclose(f);return 2;}
    rom=(uint8_t*)malloc((size_t)len);m=(JagMachine*)malloc(sizeof *m);
    if(!rom||!m){fputs("out of memory\n",stderr);free(m);free(rom);fclose(f);return 2;}
    if(fread(rom,1,(size_t)len,f)!=(size_t)len){fputs("ROM read failed\n",stderr);free(m);free(rom);fclose(f);return 2;}
    fclose(f);
    if(jagm_init(m,rom,(size_t)len)){fputs("reference boot failed\n",stderr);free(m);free(rom);return 2;}
    m->on_blit_mmio=observe;
    jagm_run_frames(m,fields);
    printf("fields_requested=%u fields_run=%" PRIu64 " model_cycles=%" PRIu64 " halted=%d\n",
           fields,m->frames,m->sys_cycles,m->halted);
    printf("cmd_writes=%" PRIu64 " nogo=%" PRIu64 " stop_events=%" PRIu64
           " resume_writes=%" PRIu64 " abort_writes=%" PRIu64 " irq_rises=%" PRIu64 "\n",
           commands,nogos,stopped,resumes,aborts,completions);
    printf("new_blit_events=%" PRIu64 " blit_count_deltas=%" PRIu64
           " same_scheduler_cycle=%" PRIu64 " nonprogress_events=%" PRIu64 "\n",
           blits_before,blits_after,pseudo_zero_cycle_transfers,unchanged);
    printf("mmio_master_68000=%" PRIu64 " gpu=%" PRIu64 " dsp=%" PRIu64
           " blitter=%" PRIu64 " op=%" PRIu64 "\n",
           masters[0],masters[1],masters[2],masters[3],masters[4]);
    puts("Caution: reference-machine scheduler timestamps are not Jaguar silicon bus cycles.");
    free(m);free(rom);
    return 0;
}
