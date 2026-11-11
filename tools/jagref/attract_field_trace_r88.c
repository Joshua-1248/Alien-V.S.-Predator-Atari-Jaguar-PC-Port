/* RE35 R88 original-Jaguar MJP/CROSS field trace.
 * Read-only retail ROM execution through the existing JagMachine model.
 * Records the actual emulated GPU handshake/progress and CPU PC on each
 * Jaguar field; NEVER modifies the retail ROM or shipping PC game.
 * WARNING: JagMachine lacks accurate shared-bus arbitration and is not
 * a hardware-timed oracle. Field counts here must NOT be used as runtime
 * constants without corroboration against an accurate emulator/hardware. */
#include "jaguar/jag_machine.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv)
{
    FILE *file;
    uint8_t *rom;
    JagMachine *m;
    size_t count;
    unsigned last=0u,prev_progress=~0u,prev_status=~0u;
    uint32_t prev_image=~0u;
    if(argc<2||argc>5){
        fprintf(stderr,"usage: %s retail-AVP.jag [first-field=1550] [last-field=1800] [stride=1]\n",argv[0]);
        return 2;
    }
    const unsigned begin=argc>2?(unsigned)strtoul(argv[2],NULL,10):1550u;
    const unsigned end=argc>3?(unsigned)strtoul(argv[3],NULL,10):1800u;
    const unsigned stride=argc>4?(unsigned)strtoul(argv[4],NULL,10):1u;
    if(!stride||end<begin||end>60000u)return 2;
    file=fopen(argv[1],"rb");if(!file){perror(argv[1]);return 2;}
    if(fseek(file,0,SEEK_END)!=0){fclose(file);return 2;}
    const long length=ftell(file);
    if(length!=4194304L){fclose(file);return 2;}
    rewind(file);
    rom=(uint8_t*)malloc((size_t)length);
    m=(JagMachine*)malloc(sizeof(*m));
    if(!rom||!m){fclose(file);free(rom);free(m);return 2;}
    count=fread(rom,1,(size_t)length,file);fclose(file);
    if(count!=(size_t)length||jagm_init(m,rom,count)!=0){free(rom);free(m);return 2;}
    puts("field,pc68,gpu_pc,gpu_go,stage_7fb4,status_7f80,image_7fa4,ack_7fa8,olp");
    for(unsigned field=1u;field<=end;field++){
        jagm_set_pad(m,0u,0u);jagm_run_frames(m,1u);
        if(field<begin)continue;
        const unsigned prog=jagm_read32(m,0x7fb4u);
        const unsigned stat=jagm_read32(m,0x7f80u);
        const uint32_t pic=jagm_read32(m,0x7fa4u);
        if(field==begin||((field-begin)%stride==0u)||prog!=prev_progress||stat!=prev_status||pic!=prev_image){
            printf("%u,%06x,%06x,%u,%u,%u,%06x,%u,%06x\n",field,
                m68k_get_reg(NULL,M68K_REG_PC),m->gpu.pc,(unsigned)m->gpu.go,
                prog,stat,pic,jagm_read32(m,0x7fa8u),m->olp);
        }
        prev_progress=prog;prev_status=stat;prev_image=pic;
        last=field;
    }
    free(m->audio);free(m);free(rom);
    return last==end?0:1;
}
