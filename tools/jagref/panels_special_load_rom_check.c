/* RE25 S48 (Claude): continuous original special-panel transaction vs native.
 *
 * For each special panel $80-$8C the ORIGINAL 68000 routine $F2D6 runs on the
 * reference machine (stub: move.w #id,d0 / movea.l #slot,a1 / jsr $F2D6 /
 * bra *), with its real FILES dispatch, JP80 + JP90 GPU decoding and CRY
 * conversion, $1FA000 overlay loads and $F34E-$F3D8 Blitter composites
 * (both overlays for $80).  The native avp_jagpanel_special_load() runs from
 * an identical copy of the machine.  Compared: the 32 KiB panel slot, the
 * $1FA000 overlay buffer, the RGB scratch, JPEG/FILES 68000 variables, the
 * $7F80-$7F8C converter block and the GPU mailbox/work RAM.
 *
 * Machine state: natural boot to the first level JPEG GPU entry (field
 * 1652, mode 2, CRY table loaded, level decoder resident).  The natural GPU
 * job is cancelled and the 68000 redirected to the stub; the early-boot
 * descriptor tables ($23FE2..) are restored from field 40 because the level
 * loader reuses that DRAM (see the S47 audit).  This is a controlled
 * transaction on an authentic machine state, not a naturally scheduled one.
 * For $80 the secondary index word $2F5AA is set to 1, which selects JP90
 * FILES #69 (the S47 secondary).  The live value here (14) selects GZIP
 * FILES $B4, which needs the cartridge GZIP service - not ported natively
 * yet (S47 gate 3), so that case is reported, not compared.
 * No ROM bytes, pixels or machine states are written to disk. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_dispatch.h"
#include "jaguar/avp_gpu/files_jpeg_native_expand.h"
#include "jaguar/avp_gpu/files_gzip_service_native.h"
#include "jaguar/avp_gpu/panels_special_load.h"
#include "jaguar/avp_gpu/panels_special_lookup.h"
#include "m68k.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#define ROM_BYTES 4194304u
#define STUB 0x04C100u
#define SLOT 0x0DD000u
#define TABLES 0x023FE2u
#define TABLE_BYTES 0x94u   /* $23FE2 primary (13x4) + $24016 secondary (32x3) */
static uint8_t rom[ROM_BYTES],tables[TABLE_BYTES];
static JagMachine A,B,BOOT;
static void fix(JagMachine *m){m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static uint32_t rd(void *c,uint32_t a,int w)
{JagMachine *m=c;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *c,uint32_t a,uint32_t v,int w)
{JagMachine *m=c;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void on_frame(JagMachine *m,void *ctx)
{
    static const unsigned press[]={950u,1150u,1400u,1700u,2000u};
    uint32_t mask=0;(void)ctx;
    for(unsigned i=0;i<5u;i++)if(m->frames>=press[i]&&m->frames<press[i]+6u)mask=0x20000u;
    jagm_set_pad(m,0u,mask);
}
static unsigned diff(const char *what,uint32_t lo,uint32_t hi,int gpu,unsigned *changed)
{
    unsigned n=0,c=0;uint32_t first=0;
    for(uint32_t a=lo;a<hi;a++){
        uint8_t x=gpu?A.gpu.ram[a-0xF03000u]:A.ram[a],y=gpu?B.gpu.ram[a-0xF03000u]:B.ram[a];
        uint8_t z=gpu?BOOT.gpu.ram[a-0xF03000u]:BOOT.ram[a];
        if(x!=z)c++;
        if(x!=y){if(!n)first=a;n++;}
    }
    if(n)printf("    %s $%06X-$%06X: %u bytes differ (first $%06X)\n",what,lo,hi,n,first);
    if(changed)*changed=c;
    return n;
}
static int run_one(uint32_t id,int live)
{
    static const uint8_t stub_tail[]={0x4E,0xB9,0x00,0x00,0xF2,0xD6,0x60,0xFE};
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,0xc00000u,
        AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    AvpView3DBus bus={&B,rd,wr};
    AvpJagFilesJpegNativeExpand nx={&bus,0x800000u,0xc00000u,0u,0x200000u,0u,0x200000u,0u,{0,0,0,0},{0,0,0,0}};
    AvpJagFilesGzipServiceNative gs={&bus,0x800000u,0xc00000u,0u,0x200000u,0u,0u};
    uint32_t gz_size=0;
    AvpJagFilesGzipDispatch gz={&bus,avp_jagfiles_gzip_service_native,&gs,&gz_size};
    AvpJagFilesJpegDispatch d={&bus,&cfg,&gz,avp_jagfiles_jpeg_native_expand,&nx};
    unsigned frames=0,bad=0,slot_changed=0,ov_changed=0;
    int native_ok;
    memcpy(&A,&BOOT,sizeof A);fix(&A);
    /* stub */
    wr(&A,STUB,0x303Cu,2);wr(&A,STUB+2u,id,2);
    wr(&A,STUB+4u,0x227Cu,2);wr(&A,STUB+6u,SLOT,4);
    for(unsigned i=0;i<sizeof stub_tail;i++)wr(&A,STUB+10u+i,stub_tail[i],1);
    if(id==0x80u&&!live)wr(&A,0x2F5AAu,1u,2);   /* secondary index 1 = JP90 #69 (see header) */
    memcpy(&B,&A,sizeof B);fix(&B);
    { AvpJagPanelSpecialLookup lk; int okk=avp_jagpanel_special_lookup(&bus,id,64u,&lk);
      printf("  lookup %d main %u prim %02X,%02X,%02X sec@%06X %02X,%02X,%02X count %u\n",okk,lk.file_index,lk.primary_descriptor[0],lk.primary_descriptor[1],lk.primary_descriptor[2],lk.secondary_descriptor_address,lk.secondary_descriptor[0],lk.secondary_descriptor[1],lk.secondary_descriptor[2],jagm_read16(&B,0x2F5AAu)); }
    native_ok=avp_jagpanel_special_load(&d,id,SLOT,64u);
    jagm_run_frames(&A,0u);                       /* bind Musashi to A */
    m68k_set_reg(M68K_REG_PC,STUB);
    while(frames<3000u&&m68k_get_reg(NULL,M68K_REG_PC)!=STUB+16u){jagm_run_frames(&A,1u);frames++;if(getenv("TRACE")&&frames<40)printf("  f%u pc=%06X sp=%06X sr=%04X gpu_go=%u gpc=%06X\n",frames,m68k_get_reg(NULL,M68K_REG_PC),m68k_get_reg(NULL,M68K_REG_SP),m68k_get_reg(NULL,M68K_REG_SR),A.gpu.go,A.gpu.pc);}
    if(m68k_get_reg(NULL,M68K_REG_PC)!=STUB+16u){printf("special $%02X: original $F2D6 did not return (pc=%06X)\n",id,m68k_get_reg(NULL,M68K_REG_PC));return 1;}
    printf("special $%02X: original returned after %u fields, native %s (stage %d, detail %04X)\n",id,frames,native_ok?"completed":"REFUSED",avp_jagpanel_special_stage,avp_jagpanel_special_detail);
    bad+=diff("panel slot",SLOT,SLOT+0x8000u,0,&slot_changed);
    bad+=diff("overlay buffer",0x1FA000u,0x1FA000u+0x5A00u,0,&ov_changed);
    if(live)diff("(info) $1F0000 work area - cartridge inflater, not reproduced",0x1F0000u,0x1FA000u,0,NULL);
    else bad+=diff("RGB scratch",0x1F0000u,0x1FA000u,0,NULL);
    bad+=diff("JPEG/FILES vars",0x02EF40u,0x02EF70u,0,NULL);
    bad+=diff("converter block",0x7F80u,0x7F90u,0,NULL);
    if(live){
        /* GZIP secondary: compare the service results; the cartridge
         * inflater's heap $1E0000-$1EA000, stack and GPU helper are NOT
         * reproduced by the native service and are reported only. */
        bad+=diff("GZIP mailbox $7FF4-$7FFF",0x7FF4u,0x8000u,0,NULL);
        printf("    mailbox original in=%08X out=%08X size=%08X | native in=%08X out=%08X size=%08X\n",jagm_read32(&A,0x7FF4u),jagm_read32(&A,0x7FF8u),jagm_read32(&A,0x7FFCu),jagm_read32(&B,0x7FF4u),jagm_read32(&B,0x7FF8u),jagm_read32(&B,0x7FFCu));
        printf("    native GZIP expands %u, size %u (original reported %u)\n",gs.expands,gs.last_size,jagm_read32(&A,0x7FFCu));
        diff("(info) cartridge heap",0x1E0000u,0x1EA000u,0,NULL);
    }
    /* $F03A00-$F03ACF holds the converter's CODE, which the original 68000
     * copies from $1BD30 with the Blitter before each conversion.  The port
     * runs the converter natively and has no GPU code image to copy, so that
     * range must equal the code bytes in the original and is excluded. */
    if(!live){ unsigned code_ok=0;
      for(uint32_t i=0;i<0xD0u;i++)if(A.gpu.ram[0xA00u+i]==A.ram[0x1BD30u+i])code_ok++;
      printf("    converter code image in original GPU RAM: %u/208 bytes equal to $1BD30\n",code_ok);
      if(code_ok!=0xD0u)bad++;
      bad+=diff("GPU mailbox/work",0xF03AD0u,0xF04000u,1,NULL); }
    printf("    slot bytes changed by original: %u, overlay buffer bytes changed: %u -> %s\n",
           slot_changed,ov_changed,(!bad&&native_ok&&slot_changed)?"IDENTICAL":"DIFFERENT");
    fflush(stdout);
    return bad||!native_ok||!slot_changed;
}
int main(int argc,char **argv)
{
    FILE *f;unsigned ok=0,n=0;
    if(argc!=2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,ROM_BYTES,f)!=ROM_BYTES||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&A,rom,ROM_BYTES))return 2;
    A.on_frame=on_frame;
    jagm_run_frames(&A,40u);
    memcpy(tables,A.ram+TABLES,TABLE_BYTES);
    if(!jagm_run_until_gpu_pc(&A,0xF03000u,1640u,1760u)){fputs("natural JPEG GPU entry not found\n",stderr);return 1;}
    A.gpu.go=0u;                                  /* cancel the natural level job */
    memcpy(A.ram+TABLES,tables,TABLE_BYTES);
    for(uint32_t a=STUB;a<STUB+0x40u;a++)if(A.ram[a]){fputs("stub area not free\n",stderr);return 1;}
    memcpy(&BOOT,&A,sizeof BOOT);fix(&BOOT);
    for(uint32_t id=0x80u;id<=(getenv("TRACE")?0x80u:0x8Cu);id++){n++;if(!run_one(id,0))ok++;}
    printf("S48 continuous special-panel transactions: %u/%u identical (original $F2D6 vs native)\n",ok,n);
    {   int live_ok;
        puts("special $80 with its LIVE secondary index (GZIP FILES via the cartridge service):");
        live_ok=!run_one(0x80u,1);
        printf("S48 $80 live GZIP secondary: %s\n",live_ok?"IDENTICAL (outputs; heap/stack/GPU helper not reproduced)":"DIFFERENT");
        return (ok==n&&live_ok)?0:1; }
}
