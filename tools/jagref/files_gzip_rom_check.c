/* S39 retail-ROM check: original $BF52 GZIP initializer versus the existing
 * native cartridge-vector boundary with independently executed ROM services.
 * This check is reference-only and never writes cartridge data to disk. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_gzip_vectors.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ROM_SIZE 4194304u
#define DRAM_SIZE (2u*1024u*1024u)
#define TRAMPOLINE_PC 0x1000u
#define TRAMPOLINE_SP 0x6C00u
#define STACK_BEGIN 0x6B00u
#define STACK_END 0x6C04u

static JagMachine machine,initial;
static uint8_t rom[ROM_SIZE],original_ram[DRAM_SIZE];
static uint8_t original_gpu[4096u];
static uint32_t boot_sr;
static uint32_t readbus(void *p,uint32_t a,int w)
{JagMachine*m=p;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void writebus(void *p,uint32_t a,uint32_t v,int w)
{JagMachine*m=p;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static int run_cpu(uint32_t pc)
{
    jagm_write16(&machine,TRAMPOLINE_PC,0x60FEu);
    jagm_write32(&machine,TRAMPOLINE_SP,TRAMPOLINE_PC);
    m68k_set_reg(M68K_REG_SR,boot_sr);
    m68k_set_reg(M68K_REG_SP,TRAMPOLINE_SP);
    m68k_set_reg(M68K_REG_PC,pc);
    (void)m68k_execute(2000000);
    return m68k_get_reg(NULL,M68K_REG_PC)==TRAMPOLINE_PC;
}
static int invoke_rom_service(void *p,uint32_t vector)
{
    uint32_t target;
    (void)p;
    if(vector!=AVP_JAGFILES_GZIP_INIT_VECTOR1 &&
       vector!=AVP_JAGFILES_GZIP_INIT_VECTOR2)return 0;
    target=jagm_read32(&machine,vector);
    return target>=0x800000u && target<0xC00000u && run_cpu(target);
}
static void restore_initial(void)
{
    memcpy(&machine,&initial,sizeof machine);
    machine.gpu.bus.ctx=&machine;
    machine.dsp.bus.ctx=&machine;
    machine.blit.bus.ctx=&machine;
}
int main(int argc,char **argv)
{
    FILE*f;AvpView3DBus bus={&machine,readbus,writebus};
    uint32_t changed=0,failed=0,first=0;
    if(argc!=2 || !(f=fopen(argv[1],"rb")))return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&machine,rom,sizeof rom))return 2;
    jagm_run_frames(&machine,30);
    boot_sr=m68k_get_reg(NULL,M68K_REG_SR);
    memcpy(&initial,&machine,sizeof initial);
    if(!run_cpu(AVP_JAGFILES_GZIP_INIT_PC)){
        fputs("original $BF52 did not return\n",stderr);return 1;
    }
    memcpy(original_ram,machine.ram,sizeof original_ram);
    for(unsigned i=0;i<sizeof original_gpu;i++)original_gpu[i]=jagm_read8(&machine,0xF03000u+i);
    restore_initial();
    if(!avp_jagfiles_gzip_init_vectors(&bus,invoke_rom_service,NULL)){
        fputs("native GZIP vector boundary failed to invoke original ROM\n",stderr);return 1;
    }
    for(unsigned i=0;i<DRAM_SIZE;i++){
        /* The two entry routes use different 68000 call frames; this region
         * contains only deliberately injected test return-stack traffic. */
        if(i>=STACK_BEGIN && i<STACK_END)continue;
        if(initial.ram[i]!=original_ram[i])changed++;
        if(machine.ram[i]!=original_ram[i]){
            if(!failed)first=i;
            failed++;
        }
    }
    for(unsigned i=0;i<sizeof original_gpu;i++)
        if(jagm_read8(&machine,0xF03000u+i)!=original_gpu[i])failed++;
    printf("original retail $BF52 vs native vector wrapper: changed_DRAM=%u differing_DRAM_or_GPU=%u first=%06x\n",changed,failed,first);
    if(failed || changed==0u){fputs("retail GZIP init parity failed or vacuous\n",stderr);return 1;}
    puts("S39 GZIP initialization: original 68000 + both cartridge ROM vectors equal native boundary outside caller-specific stack");
    return 0;
}
