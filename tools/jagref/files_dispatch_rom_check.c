/* RE24 S30 retail-ROM check: compare the native FILES locator against the
 * original 68000 routine at $BD22 on authentic decompressed game memory.
 * This runs only in jagref and never exports ROM records or asset bytes. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_dispatch.h"
#include "m68k.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FILE_LOOKUP_PC 0xBD22u
#define RETURN_SP 0x6C00u
#define RETURN_LOOP_PC 0x1000u
static JagMachine machine;
static uint32_t rd(void *ctx,uint32_t a,int w)
{JagMachine*m=ctx;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *ctx,uint32_t a,uint32_t v,int w)
{JagMachine*m=ctx;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static int lookup68000(unsigned index,uint32_t *address)
{
    /* The original FILES routine is leaf-like: its RTS returns to a
     * harmless BRA.S $-2 in disposable DRAM, without touching gameplay. */
    jagm_write16(&machine,RETURN_LOOP_PC,0x60FEu);
    jagm_write32(&machine,RETURN_SP,RETURN_LOOP_PC);
    m68k_set_reg(M68K_REG_SP,RETURN_SP);
    m68k_set_reg(M68K_REG_D0,index);
    m68k_set_reg(M68K_REG_PC,FILE_LOOKUP_PC);
    m68k_execute(150000);
    if(m68k_get_reg(NULL,M68K_REG_PC)!=RETURN_LOOP_PC)return 0;
    *address=(uint32_t)m68k_get_reg(NULL,M68K_REG_A0);
    return 1;
}
int main(int argc,char**argv)
{
    FILE*f;long size;uint8_t*rom;unsigned checked=0,tag_seen[9]={0};
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,0xC00000u,
                            AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    AvpView3DBus bus={&machine,rd,wr};AvpJagFilesPlan plan;
    if(argc!=2){fprintf(stderr,"usage: files_dispatch_rom_check AVP.jag\n");return 2;}
    f=fopen(argv[1],"rb");if(!f)return 2;
    if(fseek(f,0,SEEK_END)){fclose(f);return 2;}
    size=ftell(f);rewind(f);
    if(size!=4194304L){fclose(f);fputs("expected exact 4 MiB retail cartridge\n",stderr);return 2;}
    rom=malloc((size_t)size);
    if(!rom||fread(rom,1,(size_t)size,f)!=(size_t)size){fclose(f);free(rom);return 2;}
    fclose(f);
    if(jagm_init(&machine,rom,(size_t)size)){free(rom);return 2;}
    jagm_run_frames(&machine,30);
    if(jagm_read32(&machine,0xBD22u)==0 || jagm_read32(&machine,cfg.dispatch_table)==0){
        fputs("game code/dispatch table not initialized\n",stderr);free(rom);return 2;
    }
    for(unsigned i=0;i<256u;i++){
        uint32_t native_addr,original_addr=0u;
        int rc=avp_jagfiles_plan(&bus,&cfg,i,&plan);
        if(rc==AVP_JAGFILES_NOT_FOUND)break;
        if(rc!=AVP_JAGFILES_OK){fprintf(stderr,"file %u: plan error %d\n",i,rc);free(rom);return 1;}
        native_addr=plan.header;
        if(!lookup68000(i,&original_addr)||original_addr!=native_addr){
            fprintf(stderr,"file %u: original locator $%06x native $%06x\n",i,original_addr,native_addr);
            free(rom);return 1;
        }
        if(plan.table_entry<cfg.dispatch_table || (plan.table_entry-cfg.dispatch_table)%12u!=0u){
            free(rom);return 1;
        }
        unsigned k=(plan.table_entry-cfg.dispatch_table)/12u;
        if(k>=9u){free(rom);return 1;}
        tag_seen[k]++;
        checked++;
    }
    if(checked!=240u || avp_jagfiles_plan(&bus,&cfg,checked,&plan)!=AVP_JAGFILES_NOT_FOUND){
        fprintf(stderr,"retail archive terminator missing after %u records\n",checked);free(rom);return 1;
    }
    {
        uint32_t end_addr=0u;
        if(!lookup68000(checked,&end_addr) || jagm_read32(&machine,end_addr)!=0u){
            fputs("original 68000 archive terminator mismatch\n",stderr);free(rom);return 1;
        }
    }
    /* Compare actual original 68000 $BD44 uncompressed transactions
     * against the independent native C NONE expander, byte for byte. */
    {
        const unsigned ids[]={145u,146u,147u,148u,195u,196u,201u,202u,203u};
        const uint32_t orig_dst=0x1E0000u,native_dst=0x1E1000u;
        for(unsigned n=0;n<sizeof ids/sizeof ids[0];n++){
            uint32_t src,length,written=0;
            if(avp_jagfiles_plan(&bus,&cfg,ids[n],&plan)!=AVP_JAGFILES_OK ||
               plan.tag!=0x4E4F4E45u || plan.init_pc!=0u){
                fprintf(stderr,"retail NONE record %u metadata mismatch\n",ids[n]);free(rom);return 1;
            }
            src=plan.payload;length=jagm_read32(&machine,plan.header+4u);
            if(length==0u||length>256u){free(rom);return 1;}
            for(uint32_t j=0;j<length;j++){
                jagm_write8(&machine,orig_dst+j,0xA5u);
                jagm_write8(&machine,native_dst+j,0x5Au);
            }
            jagm_write16(&machine,RETURN_LOOP_PC,0x60FEu);
            jagm_write32(&machine,RETURN_SP,RETURN_LOOP_PC);
            m68k_set_reg(M68K_REG_SP,RETURN_SP);
            m68k_set_reg(M68K_REG_D0,ids[n]);
            m68k_set_reg(M68K_REG_A1,orig_dst);
            m68k_set_reg(M68K_REG_PC,0xBD44u);
            m68k_execute(300000);
            if(m68k_get_reg(NULL,M68K_REG_PC)!=RETURN_LOOP_PC){
                fprintf(stderr,"retail NONE %u did not return: pc=%06x\n",ids[n],m68k_get_reg(NULL,M68K_REG_PC));free(rom);return 1;
            }
            if(avp_jagfiles_expand_none(&bus,&cfg,&plan,native_dst,native_dst+length,&written)!=AVP_JAGFILES_OK||written!=length){
                fprintf(stderr,"native NONE expander %u failed\n",ids[n]);free(rom);return 1;
            }
            for(uint32_t j=0;j<length;j++){
                uint8_t x=jagm_read8(&machine,orig_dst+j),y=jagm_read8(&machine,native_dst+j);
                if(x!=y||y!=jagm_read8(&machine,src+j)){
                    fprintf(stderr,"retail NONE %u byte mismatch at %u\n",ids[n],j);free(rom);return 1;
                }
            }
        }
        printf("original 68000 $BD44 vs native NONE expansion: %zu real file records, all output bytes IDENTICAL\n",
               sizeof ids/sizeof ids[0]);
    }
    printf("retail FILES: %u file records; original 68000 $BD22 vs native record addresses: IDENTICAL\n",checked);
    printf("original 68000 handler table at $%06X: nine original tag classes\n",cfg.dispatch_table);
    for(unsigned i=0;i<9u;i++)printf("  handler %u: %u records\n",i,tag_seen[i]);
    free(rom);return 0;
}
