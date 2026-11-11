/* RE35 R90: read-only, post-field TOM/CPU/GPU/MJP record trace of the user's
 * unmodified retail Jaguar cartridge. NOT a cycle-accurate timing oracle.
 * This instrument is independent of the playable native PC runtime.
 * No proprietary payload, artwork, pixels, or executable bytes are output.
 * The ROM image is used in memory and remains unchanged. */
#include "jaguar/jag_machine.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MJP_PENDING_REC 0x4fc40u
#define MJP_STAGED_REC  0x50c40u
#define MJP_GPU_IMAGE   0x7fa4u
#define MJP_GPU_STAGE   0x7fb4u
#define MJP_GPU_STATUS  0x7f80u
#define MJP_GPU_ACK     0x7fa8u

static unsigned long long hash_image(const JagMachine *m)
{
    /* 64-bit FNV1a of the *emulated field*; this is only a change marker,
       not a claim about video pixels or actual hardware time. */
    const unsigned w=m->frame_w<=JAGM_MAX_W?m->frame_w:0u;
    const unsigned h=m->frame_h<=JAGM_MAX_H?m->frame_h:0u;
    uint64_t hash=UINT64_C(14695981039346656037);
    for(unsigned y=0u;y<h;y++)for(unsigned x=0u;x<w;x++){
        const uint32_t pixel=m->frame[(size_t)y*JAGM_MAX_W+x];
        for(unsigned b=0u;b<4u;b++){
            hash^=(uint8_t)(pixel>>(b*8u));
            hash*=UINT64_C(1099511628211);
        }
    }
    return (unsigned long long)hash;
}
static uint64_t get_phrase(JagMachine *m,uint32_t addr)
{
    return ((uint64_t)jagm_read32(m,addr)<<32)|jagm_read32(m,addr+4u);
}
static unsigned picture_data_addr(uint64_t phrase)
{
    return (unsigned)(((phrase>>43)&0x1fffffu)<<3);
}
static int picture_xpos(uint64_t phrase)
{
    int x=(int)(phrase&0xfffu);
    return x&0x800?x-0x1000:x;
}
static unsigned picture_height(uint64_t phrase)
{
    return (unsigned)((phrase>>14)&0x3ffu);
}
static void print_field(JagMachine *m,unsigned field)
{
    const uint64_t pending0=get_phrase(m,MJP_PENDING_REC);
    const uint64_t pending1=get_phrase(m,MJP_PENDING_REC+8u);
    const uint64_t staged0=get_phrase(m,MJP_STAGED_REC);
    const uint64_t staged1=get_phrase(m,MJP_STAGED_REC+8u);
    const unsigned pub=jagm_read32(m,MJP_GPU_IMAGE);
    printf("%u,%06x,%06x,%u,%u,%u,%06x,%u,%06x,%06x,%06x,%06x,%06x,%06x,%u,%u,%u,%06x,%u,%u,%06x,%u,%016llx\n",
        field,m68k_get_reg(NULL,M68K_REG_PC),m->gpu.pc,
        (unsigned)m->gpu.go,jagm_read32(m,MJP_GPU_STAGE),
        jagm_read32(m,MJP_GPU_STATUS),pub,jagm_read32(m,MJP_GPU_ACK),
        jagm_read32(m,0x4beecu),
        picture_data_addr(pending0),picture_data_addr(staged0),
        jagm_read32(m,MJP_PENDING_REC),jagm_read32(m,MJP_STAGED_REC),
        jagm_read32(m,0x4fc00u),
        picture_height(pending0),picture_height(staged0),
        picture_xpos(staged1),m->olp,m->frame_w,m->frame_h,
        jagm_read32(m,0x50c00u),picture_xpos(pending1),
        hash_image(m));
}
int main(int argc,char **argv)
{
    FILE *file;
    uint8_t *rom=NULL;
    JagMachine *m=NULL;
    unsigned first=1718u,last=1748u;
    size_t count;
    long length;
    if(argc<2||argc>4){
        fprintf(stderr,"usage: %s user-owned-AVP.jag [first-field=1718] [last-field=1748]\n",argv[0]);
        return 2;
    }
    if(argc>=3)first=(unsigned)strtoul(argv[2],NULL,10);
    if(argc>=4)last=(unsigned)strtoul(argv[3],NULL,10);
    if(first==0u||first>last||last>60000u){fprintf(stderr,"invalid field interval\n");return 2;}
    file=fopen(argv[1],"rb");if(!file){perror(argv[1]);return 2;}
    if(fseek(file,0,SEEK_END)!=0){fclose(file);return 2;}
    length=ftell(file);
    if(length!=4194304L){fprintf(stderr,"requires original 4 MiB Jaguar cartridge\n");fclose(file);return 2;}
    rewind(file);
    rom=(uint8_t*)malloc((size_t)length);
    m=(JagMachine*)malloc(sizeof(*m));
    if(!rom||!m){fclose(file);free(rom);free(m);return 2;}
    count=fread(rom,1,(size_t)length,file);fclose(file);
    if(count!=(size_t)length||jagm_init(m,rom,count)!=0){free(rom);free(m);return 2;}
    puts("field,pc68,gpu_pc,gpu_go,stage_7fb4,status_7f80,image_7fa4,ack_7fa8,bitmap_input_4beec,pending_data_4fc40,staged_data_50c40,pending_word0,staged_word0,source_head_4fc00,pending_height,staged_height,staged_x,olp,frame_width,frame_height,staged_head_50c00,pending_x,frame_hash_fnv64");
    for(unsigned field=1u;field<=last;field++){
        jagm_set_pad(m,0u,0u);
        jagm_run_frames(m,1u);
        if(m->halted){fprintf(stderr,"reference halted field %u: %s\n",field,m->halt_reason);break;}
        if(field>=first)print_field(m,field);
    }
    const int result=m->frames>=last?0:1;
    free(m->audio);free(m);free(rom);
    return result;
}
