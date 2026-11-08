/* RE24-S10 reference-machine driver: runs the user's own AVP.jag headless,
 * Environment: INPUT (scripted pad), WAV (capture DSP I2S output), WAVEROM
 * (user-owned DSP wave ROM), DIS/OPLIST/DUMP (inspection), and in the machine
 * JAGM_WATCH, JAGM_GPU_TRACE(_FRAME), JAGM_TRACE_OLP, JAGM_TRACE_GPU.
 * writes frames as PPM and prints hardware-usage diagnostics.  It never
 * writes ROM bytes or extracted assets anywhere except the frames it renders. */
#include "jaguar/jag_machine.h"
#include "m68k.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static JagMachine mach;
static void gpu_control_csv(JagMachine *m,const JagGpuControlEvent *e,void *opaque)
{
    FILE *f=(FILE*)opaque;(void)m;
    fprintf(f,"%llu,%llu,%u,%u,%06x,%08x,%06x,%06x,%u,%u,%06x\n",
            (unsigned long long)e->field,(unsigned long long)e->cycles,e->vc,
            (unsigned)e->master,e->addr,e->value,e->pc_before,e->pc_after,
            (unsigned)e->go_before,(unsigned)e->go_after,e->pc68);
}

int main(int argc,char **argv)
{
    FILE *f;long n;uint8_t *rom;unsigned frames=60,every=10;const char *out=".";
    if(argc<2){fprintf(stderr,"usage: %s AVP.jag [frames] [every] [outdir]\n",argv[0]);return 2;}
    if(argc>2)frames=(unsigned)atoi(argv[2]);
    if(argc>3)every=(unsigned)atoi(argv[3]);
    if(argc>4)out=argv[4];
    f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 1;}
    fseek(f,0,SEEK_END);n=ftell(f);fseek(f,0,SEEK_SET);rom=(uint8_t*)malloc((size_t)n);
    if(!rom||fread(rom,1,(size_t)n,f)!=(size_t)n){fclose(f);return 1;}fclose(f);
    if(jagm_init(&mach,rom,(size_t)n)){fprintf(stderr,"init failed\n");return 1;}
    { const char *path=getenv("JAGM_GPU_CONTROL_CSV");
      if(path){FILE *log=fopen(path,"w");
        if(!log){perror(path);return 1;}
        fputs("field,cycles,vc,master,addr,value,pc_before,pc_after,go_before,go_after,pc68\n",log);
        mach.on_gpu_control=gpu_control_csv;mach.on_gpu_control_ctx=log;
      } }
    mach.capture_audio=getenv("WAV")!=NULL;
    { static uint8_t wave[2048];const char *wp=getenv("WAVEROM"); /* user-owned 2 KiB DSP wave ROM */
      if(wp){FILE *wf=fopen(wp,"rb");if(wf&&fread(wave,1,sizeof wave,wf)==sizeof wave)jagm_set_wave_rom(&mach,wave);else fprintf(stderr,"WAVEROM unreadable\n");if(wf)fclose(wf);} }
    /* INPUT="start:len:mask,..." holds Jaguar pad matrix bits (see jag_machine.c)
       A=0x20000 B=0x80000 C=0x200000 PAUSE=0x10000 OPTION=0x800000
       UP=1 DOWN=2 LEFT=4 RIGHT=8 */
    for(unsigned fr=0;fr<frames;fr+=every){
        char p[512];
        for(unsigned k=0;k<every;k++){
            uint32_t mask=0;const char *in=getenv("INPUT");unsigned cur=fr+k;
            while(in&&*in){unsigned st,ln;unsigned long mk;int used=0;if(sscanf(in,"%u:%u:%lx%n",&st,&ln,&mk,&used)<3)break;if(cur>=st&&cur<st+ln)mask|=(uint32_t)mk;in+=used;if(*in==',')in++;}
            jagm_set_pad(&mach,0,mask);jagm_run_frames(&mach,1);
        }
        snprintf(p,sizeof p,"%s/frame_%05u.ppm",out,fr+every);
        jagm_write_ppm(&mach,p);
        printf("frame %u: pc=%06x olp=%06x vmode=%04x vdb=%u vde=%u hdb=%x hde=%x int1en=%x gpu.go=%u pc=%06x dsp.go=%u pc=%06x blits=%llu op_lines=%llu w=%u h=%u\n",
            fr+every,m68k_get_reg(NULL,M68K_REG_PC),mach.olp,mach.vmode,mach.vdb,mach.vde,mach.hdb1,mach.hde,mach.int1_enable,
            mach.gpu.go,mach.gpu.pc,mach.dsp.go,mach.dsp.pc,(unsigned long long)mach.blit.blits,(unsigned long long)mach.op_lines,mach.frame_w,mach.frame_h);
    }
    { char buf[128];unsigned pc=m68k_get_reg(NULL,M68K_REG_PC);
      extern unsigned long jagm_irq_acks; printf("acks=%lu vp=%u vec100=%08x 2ee88=%04x ",jagm_irq_acks,mach.vp,jagm_read32(&mach,0x100),jagm_read16(&mach,0x2ee88)); printf("vi=%u vc=%u int1pend=%x sr=%04x sp=%06x\n",mach.vi,mach.vc,mach.int1_pending,m68k_get_reg(NULL,M68K_REG_SR),m68k_get_reg(NULL,M68K_REG_SP));
      if(getenv("DIS")){unsigned q=(unsigned)strtoul(getenv("DIS"),0,16);for(int k=0;k<40;k++){unsigned l=m68k_disassemble(buf,q,M68K_CPU_TYPE_68000);printf("  %06x %s\n",q,buf);q+=l;}}
      for(int k=0;k<12;k++){unsigned l=m68k_disassemble(buf,pc,M68K_CPU_TYPE_68000);printf("  %06x %s\n",pc,buf);pc+=l;} }
    if(getenv("DUMP")){unsigned long a,l;char fn[256];if(sscanf(getenv("DUMP"),"%lx,%lx,%255s",&a,&l,fn)==3){FILE *o=fopen(fn,"wb");for(unsigned long i=0;i<l;i++)fputc(jagm_read8(&mach,(uint32_t)(a+i)),o);fclose(o);}}
    if(getenv("OPLIST")){unsigned vc=(unsigned)atoi(getenv("OPLIST"));uint32_t a=mach.olp&0xFFFFF8u;
      for(int k=0;k<40;k++){uint32_t hi=jagm_read32(&mach,a),lo=jagm_read32(&mach,a+4);unsigned t=lo&7u;
        printf("  obj@%06x %08x%08x type=%u",a,hi,lo,t);
        if(t<=1){uint32_t h1=jagm_read32(&mach,a+8),l1=jagm_read32(&mach,a+12);unsigned long long p0=((unsigned long long)hi<<32)|lo,p1=((unsigned long long)h1<<32)|l1;
          printf(" y=%u h=%u data=%06x x=%d depth=%u pitch=%u dw=%u iw=%u idx=%u refl=%u rmw=%u trans=%u fp=%u",(unsigned)((p0>>3)&0x7FF),(unsigned)((p0>>14)&0x3FF),(unsigned)((p0>>43)<<3),(int)((p1&0xFFF)^0x800)-0x800,(unsigned)((p1>>12)&7),(unsigned)((p1>>15)&7),(unsigned)((p1>>18)&0x3FF),(unsigned)((p1>>28)&0x3FF),(unsigned)((p1>>38)&0x7F),(unsigned)((p1>>45)&1),(unsigned)((p1>>46)&1),(unsigned)((p1>>47)&1),(unsigned)((p1>>49)&0x3F));
          if(t==1){uint32_t l2=jagm_read32(&mach,a+20);printf(" hs=%u vs=%u rem=%u",l2&255,(l2>>8)&255,(l2>>16)&255);}
          printf("\n");a=(a&~0x3FFFF8u)|((uint32_t)((((unsigned long long)hi<<32|lo)>>24)&0x7FFFF)<<3);continue;}
        if(t==3){unsigned y=(lo>>3)&0x7FF,cc=(lo>>14)&3;uint32_t link=((uint32_t)((((unsigned long long)hi<<32|lo)>>24)&0x7FFFF))<<3;int tk=cc==0?(y==vc||y==0x7FF):cc==1?(y>vc):cc==2?(y<vc):(mach.obf&1);
          printf(" branch y=%u cc=%u link=%06x taken@%u=%d\n",y,cc,link,vc,tk);a=tk?((a&~0x3FFFF8u)|link):a+8;continue;}
        printf("\n");if(t==4)break;a+=8;}}
    if(getenv("WAV")&&mach.audio_n){ /* 16-bit stereo at the measured I2S word rate */
        FILE *w=fopen(getenv("WAV"),"wb");uint32_t rate=(uint32_t)(26590906.0/(2.0*(mach.sclk+1u)*32.0)+0.5),bytes=(uint32_t)(mach.audio_n*2u);
        uint8_t h[44]={'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,4,0,16,0,'d','a','t','a',0,0,0,0};
        uint32_t v=36u+bytes;memcpy(h+4,&v,4);memcpy(h+24,&rate,4);v=rate*4u;memcpy(h+28,&v,4);memcpy(h+40,&bytes,4);
        fwrite(h,1,44,w);fwrite(mach.audio,2,mach.audio_n,w);fclose(w);printf("audio: %zu stereo samples at %u Hz (SCLK=%u SMODE=%x)\n",mach.audio_n/2u,rate,mach.sclk,mach.smode);}
    printf("unhandled accesses: %u\n",mach.unhandled_n);
    for(unsigned i=0;i<mach.unhandled_n;i++)printf("  %06x r=%u w=%u\n",mach.unhandled[i].addr,mach.unhandled[i].reads,mach.unhandled[i].writes);
    printf("blitter command usage (%u distinct):\n",mach.blit.usage_n);
    for(unsigned i=0;i<mach.blit.usage_n;i++)printf("  cmd=%08x a1f=%08x a2f=%08x n=%u\n",mach.blit.usage[i].cmd,mach.blit.usage[i].a1f,mach.blit.usage[i].a2f,mach.blit.usage[i].n);
    for(int k=0;k<2;k++){const JRisc *r=k?&mach.dsp:&mach.gpu;printf("%s opcodes executed:",k?"DSP":"GPU");
        for(unsigned o=0;o<64;o++)if(r->op_count[o])printf(" %s=%llu",jrisc_mnemonic(r->kind,o,o==63u?1u:0u),(unsigned long long)r->op_count[o]);
        printf("\n");}
    printf("dsp wave-ROM reads=%llu blitter unsupported=%x\n",(unsigned long long)mach.dsp.wave_rom_reads,mach.blit.unsupported);
    printf("JDEV0002 blitter risks=%x counts phantomclip=%llu 2bpp=%llu zcolour=%llu priority=%llu\n",
        mach.blit.errata_risks,(unsigned long long)mach.blit.errata_counts[0],
        (unsigned long long)mach.blit.errata_counts[1],(unsigned long long)mach.blit.errata_counts[2],
        (unsigned long long)mach.blit.errata_counts[3]);
    printf("JDEV0002 gpu/dsp risks=%x/%x (diagnostic only; no modeled corruption)\n",
        mach.gpu.errata_risks,mach.dsp.errata_risks);
    printf("JDEV0001 deep-bitmap visits=%llu RELEASE visits=%llu (bus-contention exposure, not timing)\n",
        (unsigned long long)mach.op_deep_bitmap_visits,(unsigned long long)mach.op_deep_release_visits);
    printf("JDEV0002 active RMW-followed-by-bitmap visits=%llu (edge corruption not simulated)\n",
        (unsigned long long)mach.op_rmw_neighbor_visits);
    printf("op unsupported mask=%x gpu fault=%u@%06x dsp fault=%u@%06x gpu hazards=%x dsp hazards=%x\n",mach.op_unsupported_mask,mach.gpu.fault,mach.gpu.fault_pc,mach.dsp.fault,mach.dsp.fault_pc,mach.gpu.hazards,mach.dsp.hazards);
    if(mach.on_gpu_control_ctx){
        if(fclose((FILE*)mach.on_gpu_control_ctx)!=0){perror("GPU control CSV");return 1;}
        mach.on_gpu_control_ctx=NULL;mach.on_gpu_control=NULL;
    }
    return 0;
}
