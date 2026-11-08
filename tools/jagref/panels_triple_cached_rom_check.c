/* S42: compare $F3E6 triple-cache fast-return to independent native C.
 * Authenticated original retail ROM; no external assets written to disk. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/panels_cached.h"
#include "jaguar/avp_gpu/panels_triple_cached.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ROM_BYTES 4194304u
#define STACK 0x6c00u
#define RET_PC 0x1000u
static uint8_t rom[ROM_BYTES];
static JagMachine ref, native, boot;
static uint32_t rd(void *p,uint32_t a,int w)
{JagMachine *m=p;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *p,uint32_t a,uint32_t v,int w)
{JagMachine *m=p;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void fix(JagMachine *m)
{m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static int check_one(unsigned index,unsigned fill)
{
    AvpView3DBus bus;
    uint32_t start=AVP_JAG_PANEL_CACHE_BASE+index-1u,differences=0u;
    memcpy(&ref,&boot,sizeof ref);fix(&ref);
    jagm_write16(&ref,AVP_JAG_PANEL_INDEX_ADDR,(uint16_t)index);
    for(uint32_t j=0;j<4u;j++)jagm_write8(&ref,start+j,(uint8_t)fill);
    jagm_write16(&ref,RET_PC,0x60feu);
    jagm_write32(&ref,STACK,RET_PC);
    memcpy(&native,&ref,sizeof native);fix(&native);
    bus.ctx=&native;bus.rd=rd;bus.wr=wr;
    m68k_set_reg(M68K_REG_SR,0x2700u);
    m68k_set_reg(M68K_REG_SP,STACK);
    m68k_set_reg(M68K_REG_PC,0xf3e6u);
    (void)m68k_execute(12000);
    if(m68k_get_reg(NULL,M68K_REG_PC)!=RET_PC ||
       jagm_read16(&ref,AVP_JAG_PANEL_INDEX_ADDR)!=(index+3u) ||
       !avp_jagpanel_triple_cached_reuse(&bus)){
        fprintf(stderr,"S42 triple-cache ref did not take early return index=%u pc=%06x\n",
                index,(unsigned)m68k_get_reg(NULL,M68K_REG_PC));return 1;
    }
    for(uint32_t a=0;a<JAGM_RAM_SIZE;a++){
        if(a>=STACK-0x100u && a<STACK+4u)continue;
        if(ref.ram[a]!=native.ram[a])differences++;
    }
    if(differences){fprintf(stderr,"triple-cache index %u mismatched %u DRAM bytes\n",index,differences);return 1;}
    printf("original $F3E6 all-equal triple index=%u cached_id=%u DRAM_diff=0 PASS\n",index,fill);
    return 0;
}
int main(int argc,char **argv)
{
    FILE *f;
    static const unsigned indices[]={1u,2u,3u,4u,7u,16u,32u,64u,128u,254u,511u,1024u};
    if(argc!=2||!(f=fopen(argv[1],"rb")))return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&ref,rom,sizeof rom))return 2;
    jagm_run_frames(&ref,40);memcpy(&boot,&ref,sizeof boot);
    for(unsigned i=0;i<sizeof indices/sizeof indices[0];i++)
        if(check_one(indices[i],(unsigned)(i*17u+1u)&255u))return 1;
    puts("S42 twelve original triple-cache early-return cases fully match native outside call stack");
    return 0;
}
