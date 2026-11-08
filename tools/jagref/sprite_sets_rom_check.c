/* RE25 S48 (Claude): the ORIGINAL sprite-set loader $DB3A (called from the
 * level init $D8E4) during the NATURAL retail level load vs the native
 * avp_jagsprite_load_sets().  Same capture method as panels_walk_rom_check:
 * entry/return captures of an untouched natural call (tool-only hooked
 * Musashi).  Compared: every byte allocated by the call ([$2EF4C] at entry
 * to [$2EF4C] at return), the sprite pointer table $2F278-$2F377, the
 * allocation pointer, JPEG/FILES variables, $7F80 block and GZIP mailbox.
 *
 * (method notes from the panel-walker check follow)
 * RE25 S48 (Claude): the ORIGINAL panel walker $F1F6 during the NATURAL
 * retail level load vs the native avp_jagpanel_walk().
 *
 * Built against a tool-only copy of Musashi with its instruction hook
 * enabled (avp_musashi_hook): the machine is captured exactly when the 68000
 * is about to execute $F1F6 (an untouched natural call from the level
 * loader), and again when that call returns to its caller (PC = return
 * address, SP back above the entry frame).  The native walker runs from the
 * entry capture.  Compared with the return capture: every 32 KiB panel slot
 * the walk filled, the panel cache $309BE, the slot index $2F5B6, the
 * profile pointer $2F5BA and the profile tables written, the 68000 texture
 * variables $0309BE-$030A3F, JPEG/FILES variables, the $7F80 converter block
 * and the GZIP mailbox.  Interrupt-driven DRAM (music, OP lists, timers) and
 * decoder scratch/stack are outside the compared set.
 * No ROM bytes, pixels or machine states are written to disk. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/files_dispatch.h"
#include "jaguar/avp_gpu/files_jpeg_native_expand.h"
#include "jaguar/avp_gpu/files_gzip_service_native.h"
#include "jaguar/avp_gpu/panels_walk.h"
#include "jaguar/avp_gpu/sprite_sets.h"
#include "m68k.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define ROM_BYTES 4194304u
#define WALKER 0x00DB3Au   /* sprite-set loader */
static uint8_t rom[ROM_BYTES];
static JagMachine A,ENTRY,EXIT,B;
static int state;               /* 0 waiting for entry, 1 waiting for return, 2 done */
static uint32_t entry_a5,entry_sp,ret_pc;
static unsigned calls_to_skip;
void avp_m68k_instr_hook(unsigned int pc);
void avp_m68k_instr_hook(unsigned int pc)
{
    if(state==0&&pc==WALKER&&A.frames>=1760u){
        if(calls_to_skip){calls_to_skip--;return;}
        memcpy(&ENTRY,&A,sizeof A);
        entry_a5=m68k_get_reg(NULL,M68K_REG_A5);
        entry_sp=m68k_get_reg(NULL,M68K_REG_SP);
        ret_pc=jagm_read32(&A,entry_sp);
        state=1;
    }else if(state==1&&pc==ret_pc&&m68k_get_reg(NULL,M68K_REG_SP)>entry_sp){
        memcpy(&EXIT,&A,sizeof A);
        state=2;
    }
}
static void fix(JagMachine *m){m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;m->on_frame=NULL;m->on_gpu_control=NULL;}
static uint32_t rd(void *c,uint32_t a,int w)
{JagMachine *m=c;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *c,uint32_t a,uint32_t v,int w)
{JagMachine *m=c;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void on_frame(JagMachine *m,void *ctx)
{
    static const unsigned press[]={950u,1150u,1400u,1700u,2000u};
    uint32_t mask=0;const char *e=(const char*)ctx;
    for(unsigned i=0;i<5u;i++)if(m->frames>=press[i]&&m->frames<press[i]+6u)mask=0x20000u;
    while(e&&*e){unsigned s0,ln;unsigned long mk;int u=0;
        if(sscanf(e,"%u:%u:%lx%n",&s0,&ln,&mk,&u)<3)break;
        if(m->frames>=s0&&m->frames<s0+ln){mask|=(uint32_t)mk;}
        e+=u;
        if(*e==','){e++;}
    }
    jagm_set_pad(m,0u,mask);
}
static unsigned diff(const char *what,uint32_t lo,uint32_t hi)
{
    unsigned n=0,c=0;uint32_t first=0;
    for(uint32_t a=lo;a<hi;a++){
        if(EXIT.ram[a]!=ENTRY.ram[a]){c++;}
        if(EXIT.ram[a]!=B.ram[a]){if(!n){first=a;}n++;}
    }
    printf("    %-28s $%06X-$%06X: original changed %6u, differing %u",what,lo,hi,c,n);
    if(n){printf(" (first $%06X)",first);}
    putchar('\n');
    return n;
}
int main(int argc,char **argv)
{
    FILE *f;
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,0xc00000u,
        AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    AvpView3DBus bus={&B,rd,wr};
    AvpJagFilesJpegNativeExpand nx={&bus,0x800000u,0xc00000u,0u,0x200000u,0u,0x200000u,0u,{0,0,0,0},{0,0,0,0}};
    AvpJagFilesGzipServiceNative gs={&bus,0x800000u,0xc00000u,0u,0x200000u,0u,0u};
    uint32_t gz_size=0,a5_native,slots_lo,slots_hi,prof_lo,prof_hi;
    AvpJagFilesGzipDispatch gz={&bus,avp_jagfiles_gzip_service_native,&gs,&gz_size};
    AvpJagFilesJpegDispatch d={&bus,&cfg,&gz,avp_jagfiles_jpeg_native_expand,&nx};
    unsigned bad=0;
    if(argc<2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,ROM_BYTES,f)!=ROM_BYTES||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    calls_to_skip=argc>3?(unsigned)atoi(argv[3]):0u;
    if(jagm_init(&A,rom,ROM_BYTES))return 2;
    A.on_frame=on_frame;A.on_frame_ctx=(void*)(argc>2&&strcmp(argv[2],"-")?argv[2]:NULL);
    while(state!=2&&A.frames<3000u)jagm_run_frames(&A,1u);
    if(state!=2){printf("natural $F1F6 call %s\n",state?"did not return":"not reached");return 1;}
    printf("natural $DB3A call: entry field %llu character word $%04X list $%06X, returned to $%06X at field %llu\n",
           (unsigned long long)ENTRY.frames,jagm_read16(&ENTRY,AVP_JAG_SPRITE_CHARACTER),
           jagm_read32(&ENTRY,AVP_JAG_SPRITE_LIST_TABLE+(uint32_t)(int32_t)(int16_t)jagm_read16(&ENTRY,AVP_JAG_SPRITE_CHARACTER)),
           ret_pc,(unsigned long long)EXIT.frames);
    memcpy(&B,&ENTRY,sizeof B);fix(&B);
    a5_native=(uint32_t)avp_jagsprite_load_sets(&d);
    printf("  native sprite-set loader: %s; GZIP service expands %u\n",a5_native?"completed":"REFUSED",gs.expands);
    slots_lo=jagm_read32(&ENTRY,AVP_JAG_SPRITE_ALLOC_PTR);slots_hi=jagm_read32(&EXIT,AVP_JAG_SPRITE_ALLOC_PTR);
    printf("  allocation $%06X-$%06X (%u bytes); native allocation pointer $%06X\n",slots_lo,slots_hi,slots_hi-slots_lo,jagm_read32(&B,AVP_JAG_SPRITE_ALLOC_PTR));
    if(slots_hi>slots_lo&&slots_hi<=0x200000u)bad+=diff("allocated sprite data",slots_lo,slots_hi);
    else bad++;
    bad+=diff("sprite pointer table",AVP_JAG_SPRITE_PTR_TABLE,AVP_JAG_SPRITE_PTR_TABLE+0x100u);
    bad+=diff("allocation pointer",AVP_JAG_SPRITE_ALLOC_PTR,AVP_JAG_SPRITE_ALLOC_PTR+4u);
    bad+=diff("JPEG/FILES variables",0x02EF40u,0x02EF70u);
    bad+=diff("converter block",0x007F80u,0x007F90u);
    bad+=diff("GZIP mailbox",0x007FF4u,0x008000u);
    (void)prof_lo;(void)prof_hi;
    printf("S48 natural sprite-set load: %s\n",(!bad&&a5_native)?"IDENTICAL":"DIFFERENT");
    return (!bad&&a5_native)?0:1;
}
