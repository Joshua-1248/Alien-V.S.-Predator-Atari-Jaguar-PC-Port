/* RE24 Session 2: ROM-gated 1:1 check of the native C 3D-view port.
 *
 *   view3d_port_check AVP.jag FIELD [input] [FUZZ_SEEDS]
 *
 * Boots the user's own cartridge on jagref (exactly as gpu_pipeline_check.c)
 * until the GPU starts a 3D view at $F03000.  From that one machine state it
 * runs the view twice:
 *   A  the ROM's GPU program on the TRM JRISC core (overlays loaded by the
 *      original sequencer), stopped where the sequencer loads stage 3;
 *   B  src/jaguar/avp_gpu/view3d.c (stages 0, 1, 2) on a copy of the machine,
 *      driving the same TRM Blitter model through a plain bus.
 * It fails unless every Blitter command (all registers at command time,
 * excluding the original's code-copy blits into GPU RAM), all 2 MiB of DRAM,
 * the stage-2 list head, the DIV remainder and every GPU register except the
 * call/stack scratch registers r28-r31 are identical.
 *
 * With FUZZ_SEEDS = N, it additionally repeats the comparison N times on
 * randomized scenes derived from the booted state (camera position/angle,
 * maze wall/door bytes, object positions/headings): both sides always see
 * identical memory, so this exercises every wall type and object path.
 *
 * No cartridge bytes, captured memory or derived data are written anywhere.
 * Exit 0 = identical. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/view3d.h"
#include "view3d_snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIEW_ENTRY 0xF03000u
#define VIEW_STAGE3 0xF030DCu  /* sequencer block that loads stage 3 */
#define MAXB 30000u
typedef struct BlitRec { uint32_t v[19]; } BlitRec;
static BlitRec la[MAXB],lb[MAXB];
static unsigned na,nb;
static void rec(BlitRec *r,const JagBlitter *b)
{
    uint32_t v[19]={b->cmd,b->count,b->a1_base,b->a1_flags,b->a1_pixel,b->a1_fpixel,b->a1_inc,b->a1_finc,
                    b->a1_step,b->a1_fstep,b->a1_clip,b->a2_base,b->a2_flags,b->a2_pixel,b->a2_step,b->iinc,
                    (uint32_t)(b->patd>>32),(uint32_t)b->patd,b->zinc};
    memcpy(r->v,v,sizeof v);
}
static void log_orig(void *c,const JagBlitter *b){(void)c;if(b->a1_base==0xF0B220u)return;if(na<MAXB)rec(&la[na],b);na++;}
static void log_port(void *c,const JagBlitter *b){(void)c;if(nb<MAXB)rec(&lb[nb],b);nb++;}
static uint32_t brd(void *c,uint32_t a,int w){JagMachine *m=c;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void bwr(void *c,uint32_t a,uint32_t v,int w){JagMachine *m=c;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void fix(JagMachine *m){m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static const char *field_name[19]={"cmd","count","a1_base","a1_flags","a1_pixel","a1_fpixel","a1_inc","a1_finc",
    "a1_step","a1_fstep","a1_clip","a2_base","a2_flags","a2_pixel","a2_step","iinc","patd_hi","patd_lo","zinc"};
static JagMachine BOOT,A,B;

static uint64_t rng;
static uint32_t rnd(void){rng^=rng<<13;rng^=rng>>7;rng^=rng<<17;return (uint32_t)(rng>>11);}

/* Same-memory randomization for both sides (see header). */
static void fuzz_scene(JagMachine *m,unsigned long seed)
{
    uint32_t w,h,maze,cx,cy,in,dens,v;
    rng=(uint64_t)seed*0x9E3779B97F4A7C15ull+1u;
    w=m->gpu.reg[1][4];h=m->gpu.reg[1][5];maze=m->gpu.reg[1][18];
    if(w<3u||h<3u)return;
    cx=1u+rnd()%(w-2u);cy=1u+rnd()%(h-2u);
    v=rnd()&0xFFFFu;jagm_write32(m,0x7FB0u,(cx<<16)|v);
    v=rnd()&0xFFFFu;jagm_write32(m,0x7FB4u,(cy<<16)|v);
    v=rnd()<<1;v^=rnd();jagm_write32(m,0x7FB8u,v);
    v=rnd();jagm_write32(m,0x7FA8u,(uint32_t)(int32_t)(int16_t)v);
    v=rnd();jagm_write32(m,0x7FACu,(uint32_t)(int32_t)(int16_t)v);
    dens=rnd()%40u+5u;
    for(uint32_t c=0;c<w*h;c++){
        uint32_t a=maze+c*8u;
        for(unsigned f=0;f<4u;f++)if(rnd()%100u<dens)jagm_write8(m,a+f,(uint8_t)rnd());
        if(rnd()%4u==0u){jagm_write8(m,a+6u,(uint8_t)rnd());jagm_write8(m,a+7u,(uint8_t)rnd());}
    }
    for(unsigned f=0;f<4u;f++)jagm_write8(m,maze+(cy*w+cx)*8u+f,0);
    in=jagm_read32(m,0x7F9Cu);
    for(unsigned o=0;o<300u;o++){
        uint32_t r=in+o*48u,x,y;
        if(!jagm_read32(m,r+8u)||!(rnd()&1u))continue;
        x=(cx<<16)|(rnd()&0xFFFFu);x+=((rnd()%6u)-3u)<<16;
        y=(cy<<16)|(rnd()&0xFFFFu);y+=((rnd()%6u)-3u)<<16;
        jagm_write32(m,r,x);jagm_write32(m,r+4u,y);
        jagm_write16(m,r+18u,(uint16_t)rnd());
    }
}

/* One comparison from machine state A (B is copied from it). */
static int compare_view(int verbose,AvpView3DState *out_state)
{
    AvpView3DState st;
    AvpView3DBus bus;
    unsigned long k=0;
    unsigned bl=0,first=~0u,dr=0,rd=0;
    uint32_t firstd=0;
    int bank;
    memcpy(&B,&A,sizeof A);fix(&B);
    memset(&st,0,sizeof st);
    bank=(int)((A.gpu.flags>>14)&1u);
    memcpy(st.r,A.gpu.reg[bank],sizeof st.r);
    memcpy(st.alt,A.gpu.reg[bank^1],sizeof st.alt);
    st.remain=A.gpu.remain;st.divoff=(int)(A.gpu.divctrl&1u);
    st.sort_head=jagm_read32(&A,AVP_VIEW3D_SORT_HEAD_ADDR);
    na=nb=0;
    A.blit.on_blit=log_orig;A.blit.on_blit_ctx=NULL;
    B.blit.on_blit=log_port;B.blit.on_blit_ctx=NULL;
    while(A.gpu.pc!=VIEW_STAGE3&&k<100000000ul){jrisc_step(&A.gpu);k++;}
    bus.ctx=&B;bus.rd=brd;bus.wr=bwr;
    avp_view3d_stage0(&st,&bus);
    avp_view3d_stage1(&st,&bus);
    avp_view3d_stage2(&st,&bus);
    for(unsigned i=0;i<na&&i<nb&&i<MAXB;i++)if(memcmp(la[i].v,lb[i].v,sizeof la[i].v)){if(first==~0u)first=i;bl++;}
    for(uint32_t a=0;a<JAGM_RAM_SIZE;a++)if(A.ram[a]!=B.ram[a]){if(!dr)firstd=a;dr++;}
    bank=(int)((A.gpu.flags>>14)&1u);
    for(int i=0;i<28;i++){
        if(A.gpu.reg[bank][i]!=st.r[i]){rd++;if(verbose)printf("r%d original %08X port %08X\n",i,A.gpu.reg[bank][i],st.r[i]);}
        if(A.gpu.reg[bank^1][i]!=st.alt[i]){rd++;if(verbose)printf("alt r%d original %08X port %08X\n",i,A.gpu.reg[bank^1][i],st.alt[i]);}
    }
    if(A.gpu.remain!=st.remain)rd++;
    if(jagm_read32(&A,AVP_VIEW3D_SORT_HEAD_ADDR)!=st.sort_head)rd++;
    if(verbose||bl||dr||rd||na!=nb||st.fault||A.gpu.pc!=VIEW_STAGE3){
        printf("JRISC: %lu instructions to $%06X\n",k,A.gpu.pc);
        printf("blits: original=%u port=%u differing=%u\n",na,nb,bl);
        printf("DRAM bytes differing: %u",dr);if(dr)printf(" (first $%06X)",firstd);putchar('\n');
        printf("register/state differences: %u\n",rd);
        if(st.fault)printf("port fault %08X\n",st.fault);
        if(first!=~0u){
            printf("first differing blit #%u\n",first);
            for(int j=0;j<19;j++)printf("  %-9s %08X %08X%s\n",field_name[j],la[first].v[j],lb[first].v[j],la[first].v[j]!=lb[first].v[j]?"  <--":"");
        }
    }
    if(out_state)*out_state=st;
    return (bl||dr||rd||na!=nb||na>MAXB||st.fault||A.gpu.pc!=VIEW_STAGE3)?1:0;
}

int main(int argc,char **argv)
{
    FILE *f;long n;uint8_t *rom;
    unsigned field=argc>2?(unsigned)atoi(argv[2]):2199u;
    unsigned long seeds=argc>4?strtoul(argv[4],NULL,10):0ul;
    static const unsigned presses[]={950,1150,1400,1700,2000};
    AvpView3DState st;
    unsigned long fails=0,types[16]={0},objects=0,columns=0;
    const char *snap_out=NULL,*snap_in=NULL;
    if(argc>=4&&strcmp(argv[2],"--capture")==0){
        snap_out=argv[3];field=argc>4?(unsigned)atoi(argv[4]):2199u;
        seeds=argc>6?strtoul(argv[6],NULL,10):0u;
    }else if(argc>=4&&strcmp(argv[2],"--replay")==0){
        snap_in=argv[3];seeds=argc>4?strtoul(argv[4],NULL,10):0u;
    }
    if(argc<2||(snap_in&&!snap_in[0])||(snap_out&&!snap_out[0])){
        fputs("usage: check ROM [FIELD input seeds | --capture FILE FIELD input seeds | --replay FILE seeds]\n",stderr);return 2;
    }
    f=fopen(argv[1],"rb");if(!f)return 1;fseek(f,0,SEEK_END);n=ftell(f);fseek(f,0,SEEK_SET);
    rom=malloc((size_t)n);if(!rom||fread(rom,1,(size_t)n,f)!=(size_t)n)return 1;fclose(f);
    if(jagm_init(&BOOT,rom,(size_t)n))return 1;
    if(snap_in){
        if(!avp_view3d_snapshot_read(snap_in,&BOOT,avp_view3d_rom_fingerprint(rom,(size_t)n))){
            fputs("snapshot corrupt, incompatible, or belongs to a different ROM\n",stderr);free(rom);return 4;
        }
    }else{
    while(BOOT.frames<field){
        uint32_t mask=0;
        for(unsigned i=0;i<5;i++)if(BOOT.frames>=presses[i]&&BOOT.frames<presses[i]+6)mask=0x20000u;
        { const char *e=argc>(snap_out?5:3)?argv[snap_out?5:3]:NULL;
          while(e&&*e&&*e!='-'){unsigned s0,ln;unsigned long mk;int used=0;
              if(sscanf(e,"%u:%u:%lx%n",&s0,&ln,&mk,&used)<3)break;
              if(BOOT.frames>=s0&&BOOT.frames<s0+ln)mask|=(uint32_t)mk;
              e+=used;if(*e==',')e++;} }
        jagm_set_pad(&BOOT,0,mask);jagm_run_frames(&BOOT,1);
    }
    if(!jagm_run_until_gpu_pc(&BOOT,VIEW_ENTRY,field,field+40)){puts("view start not reached");return 3;}
        if(snap_out&&!avp_view3d_snapshot_write(snap_out,&BOOT,
                   avp_view3d_rom_fingerprint(rom,(size_t)n))){
            fputs("snapshot capture failed\n",stderr);free(rom);return 4;
        }
    }
    printf("view starts at field %llu\n",(unsigned long long)BOOT.frames);
    memcpy(&A,&BOOT,sizeof A);fix(&A);
    fails+=(unsigned long)compare_view(1,&st);
    printf("captured view: %s (objects drawn %u, object columns %u)\n",fails?"DIFFERENT":"IDENTICAL",
           st.objects_drawn,st.object_columns);
    for(unsigned long s=1;s<=seeds;s++){
        memcpy(&A,&BOOT,sizeof A);fix(&A);
        fuzz_scene(&A,s);
        if(compare_view(0,&st)){fails++;printf("fuzz seed %lu: DIFFERENT\n",s);}
        for(int t=0;t<16;t++)types[t]+=st.wall_type_hits[t];
        objects+=st.objects_drawn;columns+=st.object_columns;
    }
    if(seeds){
        printf("fuzzed views: %lu, differing: %lu; objects %lu, object columns %lu; wall hits by type:",
               seeds,fails,objects,columns);
        for(int t=0;t<16;t++)printf(" %d:%lu",t,types[t]);
        putchar('\n');
    }
    free(rom);
    return fails?1:0;
}
