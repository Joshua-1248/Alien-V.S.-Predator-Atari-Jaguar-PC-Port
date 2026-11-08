/* S41: retail $F1F6 notice publication, mapped NONE and unmapped zero-fill.
 * Original ROM and decoded resources stay in test memory only. */
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
#define NOTICE0 0x191200u
#define NOTICE1 0x191204u
#define NOTICE_BASE 0x194000u
#define TRAMP 0x1000u
#define SP 0x6C00u
static uint8_t rom[ROM_SIZE];
static JagMachine a,b,boot;
static uint32_t rd(void *p,uint32_t addr,int width)
{JagMachine *m=p;return width==1?jagm_read8(m,addr):width==2?jagm_read16(m,addr):jagm_read32(m,addr);}
static void wr(void *p,uint32_t addr,uint32_t v,int width)
{JagMachine *m=p;if(width==1)jagm_write8(m,addr,(uint8_t)v);else if(width==2)jagm_write16(m,addr,(uint16_t)v);else jagm_write32(m,addr,v);}
static void fix(JagMachine *m)
{m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
/* 0-8: mixed notice/cache/miss/entry scenarios. 9-21: exhaustively
 * cover the 13 original retail notice mapping keys one by one. */
static const uint8_t ids[22][2]={{0,0},{145,0},{51,0},{51,41},
                                {51,145},{74,0},{51,41},{146,51},{10,0},
                                {51,0},{43,0},{44,0},{41,0},{10,0},
                                {11,0},{28,0},{0,0},{29,0},{19,0},
                                {35,0},{17,0},{1,0}};
static const unsigned counts[22]={0,1,1,2,2,1,2,2,1,
                                  1,1,1,1,1,1,1,1,1,1,1,1,1};
static int run_case(unsigned which)
{
    uint32_t off=LIST+2u,next=0u;unsigned diff=0u,first=0u;
    unsigned entries=which==6u||which==7u?2u:1u;
    unsigned initial=which==8u?0xffffu:which+3u;
    unsigned effective=which==8u?6u:initial;
    AvpView3DBus bus={&b,rd,wr};
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,0xC00000u,
                           AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    memcpy(&a,&boot,sizeof a);fix(&a);
    jagm_write16(&a,AVP_JAG_PANEL_INDEX_ADDR,6u);
    jagm_write32(&a,AVP_JAG_PANEL_NOTICE_NEXT_ADDR,NOTICE_BASE);
    for(unsigned k=0;k<0x500u;k++)jagm_write8(&a,NOTICE_BASE+k,0xA5u);
    jagm_write16(&a,LIST,(uint16_t)initial);
    for(unsigned i=0;i<entries;i++){
        unsigned start=entries==2u?i:0u;
        unsigned end=entries==2u?i+1u:counts[which];
        jagm_write32(&a,off,i?PTR1:PTR0);off+=4u;
        jagm_write32(&a,off,(which==7u&&i==0u)?0u:(i?NOTICE1:NOTICE0));off+=4u;
        for(unsigned j=start;j<end;j++){
            jagm_write8(&a,off++,ids[which][j]);
            jagm_write8(&a,AVP_JAG_PANEL_CACHE_BASE+effective+j,
                        which==5u?0xFEu:ids[which][j]);
        }
        jagm_write8(&a,off++,0xFFu);off=(off+1u)&~1u;
    }
    jagm_write32(&a,off,0xffffffffu);off+=4u;
    jagm_write32(&a,HEAD,LIST);
    jagm_write16(&a,TRAMP,0x60feu);
    jagm_write32(&a,SP,TRAMP);
    memcpy(&b,&a,sizeof a);fix(&b);
    m68k_set_reg(M68K_REG_SP,SP);
    m68k_set_reg(M68K_REG_A5,HEAD);
    m68k_set_reg(M68K_REG_PC,0xF1F6u);
    (void)m68k_execute(350000);
    if(m68k_get_reg(NULL,M68K_REG_PC)!=TRAMP){
        fprintf(stderr,"case %u: original $F1F6 did not return PC=%06x\n",which,(unsigned)m68k_get_reg(NULL,M68K_REG_PC));return 1;
    }
    if(!avp_jagpanel_walk_cached_or_none_notice(&bus,&cfg,LIST,off,&next)||next!=off){
        fprintf(stderr,"case %u: native notice walker failed\n",which);return 1;
    }
    for(uint32_t k=0u;k<JAGM_RAM_SIZE;k++){
        if(k>=SP-0x100u&&k<SP+4u)continue;
        if(a.ram[k]!=b.ram[k]){if(!diff)first=k;diff++;}
    }
    printf("original $F1F6 notice case %u ids=%u entries=%u DRAM_diff=%u first=%06x notice_end=%06x\n",
           which,counts[which],entries,diff,first,jagm_read32(&a,AVP_JAG_PANEL_NOTICE_NEXT_ADDR));
    if(diff){
        for(uint32_t k=0u;k<JAGM_RAM_SIZE;k++){
            if(k>=SP-0x100u&&k<SP+4u)continue;
            if(a.ram[k]!=b.ram[k]){
                fprintf(stderr,"first difference at %06x original=%02x native=%02x\n",k,a.ram[k],b.ram[k]);break;
            }
        }
        return 1;
    }
    /* The additional 13 retail map cases must actually copy original
     * nonzero 256-byte NONE records. This prohibits a vacuous equality. */
    if(which>=9u){
        AvpJagFilesPlan plan;
        unsigned index=74u+(which-9u),nonzero=0u;
        if(avp_jagfiles_plan(&bus,&cfg,index,&plan)!=AVP_JAGFILES_OK ||
           plan.tag!=0x4E4F4E45u ||
           jagm_read32(&b,plan.header+4u)!=256u)return 1;
        for(unsigned j=0;j<256u;j++){
            uint8_t v=jagm_read8(&b,plan.payload+j);
            if(v!=jagm_read8(&b,NOTICE_BASE+j))return 1;
            if(v)nonzero++;
        }
        if(!nonzero)return 1;
    }
    /* Ensure original actually published notice pointer, and changed notice
     * buffer bytes for nonempty notice scenarios. */
    if(jagm_read32(&a,NOTICE0)!=NOTICE_BASE && which!=7u)return 1;
    if(counts[which] && jagm_read8(&a,NOTICE_BASE)==0xA5u &&
       which!=1u && which!=5u && which!=7u)return 1;
    return 0;
}
int main(int argc,char **argv)
{
    FILE *f;
    if(argc!=2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&a,rom,sizeof rom))return 2;
    jagm_run_frames(&a,40);
    memcpy(&boot,&a,sizeof a);
    for(unsigned i=0;i<22u;i++)if(run_case(i))return 1;
    puts("S41 twenty-two original retail panel notice-buffer cases match native on all non-stack DRAM bytes");
    return 0;
}
