/* S40 retail ROM $F1F6 cached-panel comparison. Private ROM is read only.
 * Deliberately tests only cached (no decode / notice / Blitter) entries.
 * Neither media nor the ROM is copied to disk. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/panels_cached.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ROM_SIZE 4194304u
#define LIST 0x190000u
#define HEAD 0x191000u
#define PTR0 0x191100u
#define PTR1 0x191104u
#define TRAMP 0x1000u
#define SP 0x6C00u
static uint8_t rom[ROM_SIZE];
static JagMachine A,B,boot;
static uint32_t rd(void *p,uint32_t a,int w)
{JagMachine*m=p;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *p,uint32_t a,uint32_t v,int w)
{JagMachine*m=p;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void fix(JagMachine *m)
{m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static int run_case(unsigned scenario)
{
    uint32_t off=LIST+2u, next=0u;unsigned differences=0u,first=0u;
    uint16_t initial_index=(uint16_t)((scenario==3u||scenario==10u)?0xFFFFu:scenario==2u?3u:scenario==4u?2u:scenario==1u?1u:scenario==9u?12u:scenario==8u?11u:scenario==7u?10u:scenario==6u?8u:scenario==5u?7u:0u);
    uint16_t effective=initial_index==0xFFFFu?6u:initial_index;
    const uint8_t files[11][2]={{145u,0u},{146u,147u},{148u,195u},{196u,0u},{0u,0u},{74u,0u},{75u,76u},{74u,75u},{75u,76u},{76u,77u},{78u,0u}};
    unsigned counts[11]={1u,2u,2u,1u,0u,1u,2u,2u,2u,2u,1u};
    const unsigned entry_count=(scenario==1u||scenario==9u)?2u:1u;
    AvpView3DBus bus={&B,rd,wr};
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,0xC00000u,
                           AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    memcpy(&A,&boot,sizeof A);fix(&A);
    jagm_write16(&A,AVP_JAG_PANEL_INDEX_ADDR,6u);
    jagm_write16(&A,LIST,initial_index);
    for(unsigned i=0;i<entry_count;i++){
        uint32_t ptr=i?PTR1:PTR0;
        jagm_write32(&A,off,ptr);off+=4u;
        jagm_write32(&A,off,0u);off+=4u;
        /* one ID per entry for case 1; otherwise all IDs on one entry */
        unsigned start=(scenario==1u||scenario==9u)?i:0u;
        unsigned stop=(scenario==1u||scenario==9u)?i+1u:counts[scenario];
        for(unsigned j=start;j<stop;j++)jagm_write8(&A,off++,files[scenario][j]);
        jagm_write8(&A,off++,0xFFu);
        off=(off+1u)&~1u;
    }
    jagm_write32(&A,off,0xFFFFFFFFu);off+=4u;
    jagm_write32(&A,HEAD,LIST);
    if(scenario<5u)
        for(unsigned i=0;i<counts[scenario];i++)
            jagm_write8(&A,AVP_JAG_PANEL_CACHE_BASE+effective+i,files[scenario][i]);
    else for(unsigned i=0;i<counts[scenario];i++){
            uint8_t initial=0xFEu;
            if((scenario==7u && i==1u)||(scenario==8u && i==0u)||
               (scenario==9u && i==1u))initial=files[scenario][i];
            jagm_write8(&A,AVP_JAG_PANEL_CACHE_BASE+effective+i,initial);
         }
    jagm_write16(&A,TRAMP,0x60FEu);
    jagm_write32(&A,SP,TRAMP);
    memcpy(&B,&A,sizeof A);fix(&B);
    m68k_set_reg(M68K_REG_SP,SP);
    m68k_set_reg(M68K_REG_A5,HEAD);
    m68k_set_reg(M68K_REG_PC,0xF1F6u);
    (void)m68k_execute(250000);
    if(m68k_get_reg(NULL,M68K_REG_PC)!=TRAMP){
        fprintf(stderr,"case %u original $F1F6 did not return: pc=%06x\n",scenario,(unsigned)m68k_get_reg(NULL,M68K_REG_PC));return 1;
    }
    if(!(scenario<5u?avp_jagpanel_walk_cached(&bus,LIST,off,&next):avp_jagpanel_walk_cached_or_none(&bus,&cfg,LIST,off,&next))||next!=off){
        fprintf(stderr,"case %u native cached-panel walker failed\n",scenario);return 1;
    }
    /* Compare whole DRAM, excluding only the original 68000 call stack.
     * This catches surprising original writes in cache, pointer and slots. */
    if(scenario>=5u){
        unsigned loaded=0u;
        for(unsigned i=0;i<counts[scenario];i++){
            uint32_t slot=AVP_JAG_PANEL_SLOT_BASE+(uint32_t)(effective+i)*AVP_JAG_PANEL_SLOT_BYTES;
            uint32_t adr=jagm_read32(&A,AVP_JAG_FILES_ARCHIVE_POINTER);
            uint32_t len=0u,tag=0u;
            for(unsigned j=0;j<=files[scenario][i];j++){
                len=jagm_read32(&A,adr+4u);tag=jagm_read32(&A,adr);
                if(j==files[scenario][i])break;
                adr+=8u+((len+3u)&~3u);
            }
            if(tag!=0x4E4F4E45u || len!=256u){
                fputs("test scenario used wrong original NONE file metadata\n",stderr);return 1;
            }
            if(B.ram[AVP_JAG_PANEL_CACHE_BASE+effective+i]!=files[scenario][i])return 1;
            if(!((scenario==7u&&i==1u)||(scenario==8u&&i==0u)||(scenario==9u&&i==1u))){
                unsigned nonzero=0u;
                for(uint32_t k=0;k<len;k++){
                    if(jagm_read8(&B,slot+k)!=jagm_read8(&A,adr+8u+k)){
                        fprintf(stderr,"bad NONE payload case %u file %u offset %u\n",scenario,files[scenario][i],k);return 1;
                    }
                    if(jagm_read8(&B,slot+k))nonzero++;
                }
                if(!nonzero)return 1;
                loaded++;
            }
        }
        if(!loaded)return 1;
    }
    for(uint32_t i=0;i<JAGM_RAM_SIZE;i++){
        if(i>=SP-0x100u && i<SP+4u)continue;
        if(A.ram[i]!=B.ram[i]){if(!differences)first=i;differences++;}
    }
    printf("original $F1F6 cached panel case %u entries=%u ids=%u slot_index=%u DRAM_diff=%u first=%06x\n",
           scenario,entry_count,counts[scenario],effective,differences,first);
    return differences?1:0;
}
int main(int argc,char **argv)
{
    FILE*f;
    if(argc!=2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&A,rom,sizeof rom))return 2;
    jagm_run_frames(&A,40);
    memcpy(&boot,&A,sizeof A);
    for(unsigned i=0;i<11u;i++)if(run_case(i))return 1;
    puts("S40: eleven original retail panel cached/NONE-walker cases match native on all non-stack DRAM bytes");
    return 0;
}
