/* RE38 R153: original-ROM reference-machine state parity signature.
 * Prints hashes of emulator model memory; no original ROM data is exported.
 * Must produce the same output when compiled against R152 and R153. */
#include "jaguar/jag_machine.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t hash_bytes(const void *data,size_t n)
{
    const uint8_t *p=(const uint8_t *)data;
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}
    return h;
}
int main(int argc,char **argv)
{
    JagMachine *m=NULL;uint8_t *rom=NULL;FILE *f=NULL;
    long n;unsigned fields=600u;int result=2;
    if(argc<2||argc>3){fprintf(stderr,"usage: %s private/AVP.jag [fields=600]\n",argv[0]);return 2;}
    if(argc==3){char *end=NULL;unsigned long v=strtoul(argv[2],&end,10);
        if(!end||*end||v==0u||v>6000u)return 2;
        fields=(unsigned)v;}
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)||(n=ftell(f))!=4194304L||fseek(f,0,SEEK_SET)){
        fputs("requires original 4 MiB ROM\n",stderr);goto done;}
    rom=(uint8_t*)malloc((size_t)n);m=(JagMachine*)malloc(sizeof *m);
    if(!rom||!m||fread(rom,1,(size_t)n,f)!=(size_t)n){fputs("input read failed\n",stderr);goto done;}
    fclose(f);f=NULL;
    if(jagm_init(m,rom,(size_t)n)){fputs("jagref init failed\n",stderr);goto done;}
    m->on_blit_mmio=NULL; /* measure uninstrumented reference execution */
    jagm_run_frames(m,fields);
    printf("fields=%" PRIu64 " halted=%d blits=%" PRIu64 " pixels=%" PRIu64
           " gpu_pc=%06" PRIx32 " dsp_pc=%06" PRIx32 "\n",
           m->frames,m->halted,m->blit.blits,m->blit.pixels_written,m->gpu.pc,m->dsp.pc);
    printf("ram=%016" PRIx64 " gpu_ram=%016" PRIx64 " dsp_ram=%016" PRIx64 "\n",
           hash_bytes(m->ram,sizeof m->ram),hash_bytes(m->gpu.ram,4096u),
           hash_bytes(m->dsp.ram,8192u));
    printf("frame=%016" PRIx64 " work=%016" PRIx64 " clut=%016" PRIx64 "\n",
           hash_bytes(m->frame,sizeof m->frame),hash_bytes(m->work,sizeof m->work),
           hash_bytes(m->clut,sizeof m->clut));
    result=(m->halted||m->frames!=fields)?1:0;
done:
    if(f)fclose(f);
    free(m);free(rom);
    return result;
}
