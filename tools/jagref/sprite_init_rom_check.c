/* RE25 S49: tool-only, naturally called retail $DB92 prefix,
 * $DC1E/$DCA8/$DD24 and $DBFE (original 68000 vs native). Real ROM
 * remains private. CPU hook is enabled in avp_musashi_hook, not the game. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/sprite_init.h"
#include "jaguar/avp_gpu/files_gzip_service_native.h"
#include "jaguar/avp_gpu/files_jpeg_native_expand.h"
#include "m68k.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define ROM_BYTES 4194304u
#define N 3
static uint8_t rom[ROM_BYTES];
static JagMachine A,entry[N],end[N],B;
static uint32_t start_pc[N],stop_pc[N],target_ret[N],entry_sp[N];
static int begun[N],done[N],pending;
static const char *stage_name[N]={"$DB92 prepare","character FILES + CLUT","$DBFE shared FILES"};
void avp_m68k_instr_hook(unsigned int pc);
void avp_m68k_instr_hook(unsigned int pc)
{
    if(A.frames<1600u)return;
    for(int i=0;i<N;i++){
        if(!begun[i]&&pc==start_pc[i]){
            memcpy(&entry[i],&A,sizeof A);
            if(i==0){uint32_t ch=jagm_read16(&A,AVP_JAG_SPRITE_CHARACTER);
                start_pc[1]=ch==4u?0xdc1eu:ch==8u?0xdca8u:ch==0u?0xdd24u:0;}
            entry_sp[i]=m68k_get_reg(NULL,M68K_REG_SP);
            target_ret[i]=i==0?stop_pc[i]:jagm_read32(&A,entry_sp[i]);
            begun[i]=1;
        }else if(begun[i]&&!done[i]&&pc==target_ret[i]&&
                (i==0||m68k_get_reg(NULL,M68K_REG_SP)>entry_sp[i])){
            memcpy(&end[i],&A,sizeof A);
            done[i]=1;
            pending--;
        }
    }
}
static uint32_t rd(void *ctx,uint32_t addr,int width)
{JagMachine *m=ctx;return width==1?jagm_read8(m,addr):width==2?jagm_read16(m,addr):jagm_read32(m,addr);}
static void wr(void *ctx,uint32_t addr,uint32_t value,int width)
{JagMachine *m=ctx;if(width==1)jagm_write8(m,addr,(uint8_t)value);else if(width==2)jagm_write16(m,addr,(uint16_t)value);else jagm_write32(m,addr,value);}
static void fix(JagMachine *m)
{m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;m->on_frame=NULL;m->on_gpu_control=NULL;}
static void on_frame(JagMachine *m,void *ctx)
{
    const char *s=ctx;uint32_t mask=0;
    static const unsigned press[]={950,1150,1400,1700,2000};
    for(unsigned i=0;i<5;i++)if(m->frames>=press[i]&&m->frames<press[i]+6)mask=0x20000u;
    while(s&&*s){unsigned f,n;unsigned long mk;int used=0;
        if(sscanf(s,"%u:%u:%lx%n",&f,&n,&mk,&used)<3||used<1)break;
        if(m->frames>=f&&m->frames<f+n)mask|=(uint32_t)mk;
        s+=used;if(*s==',')s++;
    }
    jagm_set_pad(m,0,mask);
}
static unsigned diff_ram(const JagMachine *endm,const JagMachine *native,
                         const char *label,uint32_t lo,uint32_t hi)
{
    unsigned bad=0;uint32_t first=0;
    if(hi>JAGM_RAM_SIZE||lo>hi){printf(" %s invalid bounds\n",label);return 1;}
    for(uint32_t a=lo;a<hi;a++){
        if(endm->ram[a]!=native->ram[a]){if(!bad)first=a;bad++;}
    }
    printf("  %-25s $%06X..$%06X diff=%u",label,lo,hi,bad);
    if(bad)printf(" first=$%06X expected=%02X actual=%02X",first,endm->ram[first],native->ram[first]);
    putchar('\n');return bad;
}
int main(int argc,char **argv)
{
    FILE *f;unsigned bad=0;uint32_t character,alloc_lo,alloc_hi;
    AvpJagFilesConfig cfg={AVP_JAG_FILES_ARCHIVE_POINTER,0x800000u,0xc00000u,
        AVP_JAG_FILES_DISPATCH_TABLE,AVP_JAG_FILES_PREVIOUS_TAG,32u};
    AvpView3DBus bus={&B,rd,wr};
    AvpJagFilesJpegNativeExpand nx={&bus,0x800000u,0xc00000u,0u,0x200000u,0u,0x200000u,0u,{0,0,0,0},{0,0,0,0}};
    AvpJagFilesGzipServiceNative gs={&bus,0x800000u,0xc00000u,0u,0x200000u,0u,0u};
    uint32_t gzip_size=0;
    AvpJagFilesGzipDispatch gzip={&bus,avp_jagfiles_gzip_service_native,&gs,&gzip_size};
    AvpJagFilesJpegDispatch d={&bus,&cfg,&gzip,avp_jagfiles_jpeg_native_expand,&nx};
    if(argc<2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,ROM_BYTES,f)!=ROM_BYTES||fgetc(f)!=EOF){fclose(f);return 2;}fclose(f);
    if(jagm_init(&A,rom,ROM_BYTES))return 2;
    A.on_frame=on_frame;A.on_frame_ctx=(void*)(argc>2&&strcmp(argv[2],"-")?argv[2]:NULL);
    start_pc[0]=0xdb92u;stop_pc[0]=0xdbacu;
    start_pc[1]=0u;start_pc[2]=0xdbfeu;
    pending=3;
    /* Character dispatch is based on $2F1DA: 0 Marine, 4 Alien, 8 Predator.
     * The $DB92 CPU hook captures the character before its same-field call. */
    while(pending&&A.frames<2900u)jagm_run_frames(&A,1);
    if(!begun[0]){puts("no natural $DB92 call");return 1;}
    character=jagm_read16(&entry[0],AVP_JAG_SPRITE_CHARACTER);
    if(!start_pc[1]){printf("unsupported original character %u\n",character);return 1;}
    printf("original character %u; $DB92 frame %llu; stages complete %d/%d/%d at frame %llu\n",
        character,(unsigned long long)entry[0].frames,done[0],done[1],done[2],(unsigned long long)A.frames);
    if(pending)return 1;
    for(int i=0;i<N;i++){
        int ok;printf("%s original frame %llu..%llu\n",stage_name[i],
                     (unsigned long long)entry[i].frames,(unsigned long long)end[i].frames);
        memcpy(&B,&entry[i],sizeof B);fix(&B);gs.expands=0;
        ok=i==0?avp_jagsprite_driver_prepare(&d):i==1?avp_jagsprite_character_init(&d):avp_jagsprite_common_init(&d);
        if(!ok){printf("  native REFUSED detail=%d gzip=%u\n",avp_jagpanel_special_detail,gs.expands);bad++;continue;}
        alloc_lo=jagm_read32(&entry[i],AVP_JAG_SPRITE_ALLOC_PTR);
        alloc_hi=jagm_read32(&end[i],AVP_JAG_SPRITE_ALLOC_PTR);
        if(i!=0&&alloc_hi>alloc_lo&&alloc_hi<=0x200000u)
            bad+=diff_ram(&end[i],&B,"allocated sprite data",alloc_lo,alloc_hi);
        bad+=diff_ram(&end[i],&B,"allocation ptr",0x02ef4cu,0x02ef50u);
        bad+=diff_ram(&end[i],&B,"FILES control",0x02ef50u,0x02ef60u);
        bad+=diff_ram(&end[i],&B,"loader state",0x02ef60u,0x02ef70u);
        if(i==1||i==2){
            bad+=diff_ram(&end[i],&B,"character pointers",0x02f550u,0x02f590u);
            bad+=diff_ram(&end[i],&B,"marine shared ptr",0x03108au,0x031090u);
            bad+=diff_ram(&end[i],&B,"alien special ptr",0x0310c8u,0x0310ccu);
            bad+=diff_ram(&end[i],&B,"sprite root ptr",0x0391e0u,0x0391e4u);
            bad+=diff_ram(&end[i],&B,"FILES mailbox",0x007ff4u,0x008000u);
            if(i==1&&memcmp(end[i].clut,B.clut,sizeof B.clut)){
                printf("  CLUT mismatch\n");bad++;
            }else if(i==1)puts("  CLUT 256 entries identical");
        }
        printf("  native %s; GZIP service expands=%u\n",ok?"completed":"REFUSED",gs.expands);
    }
    printf("S49 natural sprite initialization: %s (%u mismatched bytes/conditions)\n",bad?"DIFFERENT":"IDENTICAL",bad);
    return bad?1:0;
}
