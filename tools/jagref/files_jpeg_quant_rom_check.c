/* Retail-only 68000 $BE00 QUANT table parity; no ROM data written to disk. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_jpeg_init.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static JagMachine machine;
static uint32_t rd(void *c,uint32_t a,int w)
{JagMachine *m=c;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *c,uint32_t a,uint32_t v,int w)
{JagMachine *m=c;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static int original_call(uint32_t pc,uint32_t tag)
{
    jagm_write16(&machine,0x1000u,0x60FEu);
    jagm_write32(&machine,0x6C00u,0x1000u);
    m68k_set_reg(M68K_REG_SP,0x6C00u);
    m68k_set_reg(M68K_REG_D0,tag);
    m68k_set_reg(M68K_REG_PC,pc);
    (void)m68k_execute(150000);
    if(m68k_get_reg(NULL,M68K_REG_PC)!=0x1000u){
        fprintf(stderr,"original CPU routine did not return: PC=%06X\n",(unsigned)m68k_get_reg(NULL,M68K_REG_PC));
        return 0;
    }
    return 1;
}
int main(int argc,char **argv)
{
    static uint8_t rom[4194304u];
    const uint16_t qualities[]={0x3630,0x3635,0x3730,0x3735,0x3830,0x3835,0x3930,0x3131};
    AvpView3DBus bus={&machine,rd,wr};
    FILE *f;
    if(argc!=2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&machine,rom,sizeof rom))return 2;
    jagm_run_frames(&machine,30);
    for(unsigned q=0;q<sizeof qualities/sizeof qualities[0];q++){
        AvpJagFilesJpegQuant plan;
        uint8_t expected[AVP_JPEG_QUANT_BYTES],original[AVP_JPEG_QUANT_BYTES];
        const uint16_t quality=qualities[q];
        if(!avp_jagfiles_jpeg_quant_plan(&bus,quality,&plan))return 1;
        if(plan.source!=((q<7u)?jagm_read32(&machine,AVP_JPEG_QUALITY_LIST+q*6u+2u):AVP_JPEG_DEFAULT_TABLE))return 1;
        for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++){
            expected[i]=jagm_read8(&machine,plan.source+i);
            jagm_write8(&machine,plan.target+i,0xA5u);
        }
        jagm_write16(&machine,AVP_JPEG_QUALITY_VARIABLE,quality);
        if(!original_call(0xBE00u,quality))return 1;
        for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++){
            original[i]=jagm_read8(&machine,plan.target+i);
            if(original[i]!=expected[i]){
                fprintf(stderr,"quality=%04X original table mismatched source at %u: got %02X expected %02X\n",
                        quality,i,original[i],expected[i]);
                return 1;
            }
            jagm_write8(&machine,plan.target+i,0x5Au);
        }
        if(!avp_jagfiles_jpeg_quant_apply(&bus,&plan))return 1;
        for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++){
            uint8_t native=jagm_read8(&machine,plan.target+i);
            if(native!=original[i]){
                fprintf(stderr,"quality=%04X original vs native mismatch at %u\n",quality,i);
                return 1;
            }
        }
        printf("JP quality %04X: original $BE00 vs native: %u bytes IDENTICAL (source %06X)\n",
               quality,AVP_JPEG_QUANT_BYTES,plan.source);
    }
    /* Direct execution of original $BD92, no GPU upload needed: the previous
     * codec is JPxx, and mode 2's color table has already been loaded. */
    for(unsigned mode=0;mode<3u;mode++)for(unsigned q=0;q<8u;q++){
        AvpJagFilesJpegQuant plan;
        uint16_t quality=qualities[q];
        uint32_t tag=0x4A500000u|quality;
        jagm_write32(&machine,AVP_JPEG_PREVIOUS_TAG,0x4A503830u);
        jagm_write8(&machine,AVP_JPEG_MODE_VARIABLE,(uint8_t)mode);
        jagm_write8(&machine,AVP_JPEG_CRY_LOADED,1u);
        if(!avp_jagfiles_jpeg_quant_plan(&bus,quality,&plan))return 1;
        for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++)jagm_write8(&machine,plan.target+i,0xA5u);
        if(!original_call(0xBD92u,tag))return 1;
        uint8_t expected[AVP_JPEG_QUANT_BYTES];
        for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++)expected[i]=jagm_read8(&machine,plan.target+i);
        if(jagm_read16(&machine,AVP_JPEG_QUALITY_VARIABLE)!=quality)return 1;
        for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++)jagm_write8(&machine,plan.target+i,0x5Au);
        jagm_write16(&machine,AVP_JPEG_QUALITY_VARIABLE,0u);
        if(!avp_jagfiles_jpeg_init_cached(&bus,tag))return 1;
        if(jagm_read16(&machine,AVP_JPEG_QUALITY_VARIABLE)!=quality)return 1;
        for(unsigned i=0;i<AVP_JPEG_QUANT_BYTES;i++){
            if(jagm_read8(&machine,plan.target+i)!=expected[i]){
                fprintf(stderr,"original $BD92 mode=%u quality=%04X differs at %u\n",mode,quality,i);
                return 1;
            }
        }
    }
    puts("original 68000 $BD92 cached-JP initializer: 24 cases PASS");
    puts("original 68000 JPEG quantization: eight cases PASS");
    return 0;
}
