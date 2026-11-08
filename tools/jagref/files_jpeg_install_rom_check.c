/* Original $BD92 first-installation parity; actual cartridge is required.
 * No ROM bytes, decoded program bytes or GPU captures are written to disk. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_jpeg_init.h"
#include "m68k.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static JagMachine A,B;
static uint32_t rd(void *ctx,uint32_t a,int w)
{JagMachine *m=ctx;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *ctx,uint32_t a,uint32_t v,int w)
{JagMachine *m=ctx;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void fix(JagMachine *m)
{m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static int call_orig(uint32_t tag)
{
    jagm_write16(&A,0x1000u,0x60FEu);
    jagm_write32(&A,0x6C00u,0x1000u);
    m68k_set_reg(M68K_REG_SP,0x6C00u);
    m68k_set_reg(M68K_REG_D0,tag);
    m68k_set_reg(M68K_REG_PC,0xBD92u);
    (void)m68k_execute(300000);
    if(m68k_get_reg(NULL,M68K_REG_PC)!=0x1000u){
        fprintf(stderr,"original $BD92 did not return pc=%06x\n",(unsigned)m68k_get_reg(NULL,M68K_REG_PC));return 0;
    }
    return 1;
}
int main(int argc,char**argv)
{
    static uint8_t rom[4194304u];
    static const uint16_t quality[]={0x3630u,0x3930u,0x3131u};
    FILE *f;
    if(argc!=2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&A,rom,sizeof rom))return 2;
    jagm_run_frames(&A,40);
    for(unsigned mode=0;mode<3;mode++)for(unsigned q=0;q<3;q++){
        AvpJagFilesJpegInstall plan;
        uint32_t tag=0x4A500000u|quality[q];
        AvpView3DBus bus={&B,rd,wr};
        jagm_write32(&A,AVP_JPEG_PREVIOUS_TAG,0x475A4950u);
        jagm_write8(&A,AVP_JPEG_MODE_VARIABLE,(uint8_t)mode);
        jagm_write8(&A,AVP_JPEG_CRY_LOADED,1u);
        jagm_write16(&A,AVP_JPEG_QUALITY_VARIABLE,0xF0F0u);
        memcpy(&B,&A,sizeof A);fix(&B);
        if(!avp_jagfiles_jpeg_install_plan(&bus,mode,&plan)){
            fprintf(stderr,"missing program header mode %u\n",mode);return 1;
        }
        if(!call_orig(tag))return 1;
        if(!avp_jagfiles_jpeg_init_first(&bus,tag)){
            fprintf(stderr,"native initializer refused mode %u\n",mode);return 1;
        }
        unsigned differ=0,first=0;
        for(unsigned i=0;i<0x1000u;i++){
            uint32_t a=0xF03000u+i;
            if(jagm_read8(&A,a)!=jagm_read8(&B,a)){
                if(!differ)first=i;differ++;
            }
        }
        uint32_t qa=jagm_read16(&A,AVP_JPEG_QUALITY_VARIABLE);
        uint32_t qb=jagm_read16(&B,AVP_JPEG_QUALITY_VARIABLE);
        uint32_t ctra=jagm_read32(&A,AVP_JPEG_GPU_CTRL);
        uint32_t ctrb=jagm_read32(&B,AVP_JPEG_GPU_CTRL);
        int blitbad=A.blit.cmd!=B.blit.cmd||A.blit.count!=B.blit.count||
          A.blit.a1_base!=B.blit.a1_base||A.blit.a2_base!=B.blit.a2_base||
          A.blit.a1_pixel!=B.blit.a1_pixel||A.blit.a2_pixel!=B.blit.a2_pixel||
          A.blit.a1_flags!=B.blit.a1_flags||A.blit.a2_flags!=B.blit.a2_flags||A.blit.blits!=B.blit.blits;
        printf("mode=%u quality=%04x len=%u GPU_RAM_bad=%u first=%03x blitter_bad=%d quality=%04x/%04x ctrl=%08x/%08x\n",
               mode,quality[q],plan.rounded_length,differ,first,blitbad,qa,qb,ctra,ctrb);
        if(blitbad){
          printf("A cmd=%08x count=%08x a1_base=%08x a2_base=%08x a1_pixel=%08x a2_pixel=%08x a1_flags=%08x a2_flags=%08x\n",A.blit.cmd,A.blit.count,A.blit.a1_base,A.blit.a2_base,A.blit.a1_pixel,A.blit.a2_pixel,A.blit.a1_flags,A.blit.a2_flags);
          printf("B cmd=%08x count=%08x a1_base=%08x a2_base=%08x a1_pixel=%08x a2_pixel=%08x a1_flags=%08x a2_flags=%08x\n",B.blit.cmd,B.blit.count,B.blit.a1_base,B.blit.a2_base,B.blit.a1_pixel,B.blit.a2_pixel,B.blit.a1_flags,B.blit.a2_flags);
        }
        if(differ||blitbad||qa!=qb||ctra!=ctrb)return 1;
    }
    puts("original $BD92 first-time JPEG program install: 9 cases IDENTICAL");
    return 0;
}
