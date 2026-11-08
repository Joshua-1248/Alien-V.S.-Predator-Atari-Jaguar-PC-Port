/* S42: original 68000 $BF6E source/output mailbox staging boundary.
 * Stops BEFORE the cartridge vector executes: NOT a decompressor test. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_gzip_vectors.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ROM_BYTES 4194304u
#define STACK 0x6c00u
#define HANDOFF_PC 0xbf82u
static uint8_t rom[ROM_BYTES];
static JagMachine ref, native, boot;
static uint32_t rd(void *p,uint32_t a,int w)
{JagMachine *m=p;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *p,uint32_t a,uint32_t v,int w)
{JagMachine *m=p;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void fix(JagMachine *m)
{m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static int stop_at_vector(void *arg,uint32_t vector)
{
    JagMachine *m=arg;
    uint32_t fn=jagm_read32(m,vector);
    return vector==AVP_JAGFILES_GZIP_EXP_VECTOR && fn>=0x800000u && fn<0xc00000u;
}
static int check_one(unsigned which,unsigned file_id)
{
    uint32_t sp=STACK, payload, dest=0x140000u+which*0x4000u,reported=0u;
    uint32_t first=0u,differences=0u;
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,0xc00000u,
                           AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    AvpJagFilesPlan file;
    AvpView3DBus read_ref, write_native;
    unsigned steps;
    memcpy(&ref,&boot,sizeof ref);fix(&ref);
    memcpy(&native,&boot,sizeof native);fix(&native);
    read_ref.ctx=&ref;read_ref.rd=rd;read_ref.wr=wr;
    write_native.ctx=&native;write_native.rd=rd;write_native.wr=wr;
    if(avp_jagfiles_plan(&read_ref,&cfg,file_id,&file)!=AVP_JAGFILES_OK ||
       file.tag!=0x475a4950u)return 1;
    payload=file.payload;
    jagm_write32(&ref,sp,0x1000u);
    m68k_set_reg(M68K_REG_SR,0x2700u);
    m68k_set_reg(M68K_REG_SP,sp);
    m68k_set_reg(M68K_REG_A0,payload);
    m68k_set_reg(M68K_REG_A1,dest);
    m68k_set_reg(M68K_REG_PC,AVP_JAGFILES_GZIP_EXP_PC);
    for(steps=0;steps<64u;steps++){
        if(m68k_get_reg(NULL,M68K_REG_PC)==HANDOFF_PC)break;
        (void)m68k_execute(1);
    }
    if(m68k_get_reg(NULL,M68K_REG_PC)!=HANDOFF_PC ||
       (uint32_t)m68k_get_reg(NULL,M68K_REG_A0)!=jagm_read32(&ref,AVP_JAGFILES_GZIP_EXP_VECTOR) ||
       (uint32_t)m68k_get_reg(NULL,M68K_REG_A1)!=dest){
        fprintf(stderr,"GZIP case %u: original wrapper did not reach service JSR PC=%06x\n",
                file_id,(unsigned)m68k_get_reg(NULL,M68K_REG_PC));return 1;
    }
    if(!avp_jagfiles_gzip_expand_vector(&write_native,payload,dest,
                                         stop_at_vector,&native,&reported))return 1;
    for(uint32_t i=0u;i<JAGM_RAM_SIZE;i++){
        if(i>=STACK-0x100u && i<STACK+4u)continue;
        if(ref.ram[i]!=native.ram[i]){
            if(!differences)first=i;
            differences++;
        }
    }
    if(jagm_read32(&ref,AVP_JAGFILES_GZIP_INPUT)!=payload ||
       jagm_read32(&ref,AVP_JAGFILES_GZIP_OUTPUT)!=dest ||
       (uint32_t)m68k_get_reg(NULL,M68K_REG_SP)!=sp-4u || differences){
        fprintf(stderr,"GZIP case %u: staging mismatch %u bytes first=%06x\n",
                file_id,differences,first);return 1;
    }
    printf("original $BF6E FILES %u source=%06x destination=%06x staged=%u instructions DRAM_diff=%u PASS\n",
           file_id,payload,dest,steps,differences);
    return 0;
}
int main(int argc,char **argv)
{
    FILE *f;
    static const unsigned files[]={87u,88u,103u,119u,120u,129u,150u,170u,200u,239u};
    if(argc!=2 || !(f=fopen(argv[1],"rb")))return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&ref,rom,sizeof rom))return 2;
    jagm_run_frames(&ref,40);
    memcpy(&boot,&ref,sizeof boot);
    for(unsigned i=0;i<sizeof files/sizeof files[0];i++)if(check_one(i,files[i]))return 1;
    puts("S42 ten retail GZIP $BF6E CPU-to-cartridge handoff stages identical to native wrapper; expansion NOT tested");
    return 0;
}
