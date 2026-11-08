/* RE25-S43 original $F3E6-$F548 mismatch and three Blitter commands.
 * Reads authenticated retail ROM privately; never publishes game assets.
 * Compares all Jaguar DRAM (except 68000 stack), all Blitter state,
 * exact blit count and resulting pixels. Host bus is the SAME reference
 * hardware model; bus-level parity is not physical-cycle parity. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/panels_cached.h"
#include "jaguar/avp_gpu/panels_triple_rebuild.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ROM_BYTES 4194304u
#define STACK 0x6c00u
#define RET_PC 0x1000u
static uint8_t rom[ROM_BYTES];
static JagMachine ref,native,boot;
static uint32_t rd(void *p,uint32_t a,int w)
{JagMachine *m=p;return w==1?jagm_read8(m,a):w==2?jagm_read16(m,a):jagm_read32(m,a);}
static void wr(void *p,uint32_t a,uint32_t v,int w)
{JagMachine *m=p;if(w==1)jagm_write8(m,a,(uint8_t)v);else if(w==2)jagm_write16(m,a,(uint16_t)v);else jagm_write32(m,a,v);}
static void fix(JagMachine *m)
{m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static int check_one(unsigned index,unsigned mismatch_position,unsigned pattern)
{
    uint32_t cache=AVP_JAG_PANEL_CACHE_BASE+index-1u;
    uint32_t src=AVP_JAG_PANEL_SLOT_BASE+(index-1u)*AVP_JAG_PANEL_SLOT_BYTES;
    uint32_t differences=0u,first=0u,changed_pixels=0u;
    JagBlitter aa,bb;
    unsigned steps;
    AvpView3DBus bus={&native,rd,wr};
    memcpy(&ref,&boot,sizeof ref);fix(&ref);
    jagm_write16(&ref,AVP_JAG_PANEL_INDEX_ADDR,(uint16_t)index);
    for(unsigned j=0u;j<4u;j++)jagm_write8(&ref,cache+j,(uint8_t)(j==mismatch_position?0x75u:0x26u));
    /* Instrument a genuine data move, not merely matching register values.
     * These are controlled test patterns, not extracted game resources. */
    for(uint32_t j=0u;j<4u*AVP_JAG_PANEL_SLOT_BYTES;j++)
        jagm_write8(&ref,src+j,(uint8_t)((j*13u+pattern*31u+(j>>5)+(j>>15)*43u)&0xffu));
    jagm_write16(&ref,RET_PC,0x60feu);
    jagm_write32(&ref,STACK,RET_PC);
    memcpy(&native,&ref,sizeof native);fix(&native);
    m68k_set_reg(M68K_REG_SR,0x2700u);
    m68k_set_reg(M68K_REG_SP,STACK);
    m68k_set_reg(M68K_REG_PC,0xf3e6u);
    for(steps=0u;steps<500u;steps++){
        if(m68k_get_reg(NULL,M68K_REG_PC)==RET_PC)break;
        (void)m68k_execute(1);
    }
    if(steps==500u||m68k_get_reg(NULL,M68K_REG_PC)!=RET_PC||
       jagm_read16(&ref,AVP_JAG_PANEL_INDEX_ADDR)!=index+3u||
       ref.blit.blits!=native.blit.blits+3u){
        fprintf(stderr,"original mismatch index=%u pos=%u stopped pc=%06x blits=%llu\n",
                index,mismatch_position,(unsigned)m68k_get_reg(NULL,M68K_REG_PC),
                (unsigned long long)(ref.blit.blits-native.blit.blits));return 1;
    }
    for(uint32_t k=0u;k<4u*AVP_JAG_PANEL_SLOT_BYTES;k++)
        if(ref.ram[src+k]!=native.ram[src+k])changed_pixels++;
    if(!changed_pixels || ref.blit.pixels_written<=native.blit.pixels_written){
        fprintf(stderr,"index=%u pos=%u: original three Blits did not actually change seeded pixels\n",
                index,mismatch_position);return 1;
    }
    if(!avp_jagpanel_triple_rebuild(&bus,1)){
        fprintf(stderr,"native mismatch index=%u pos=%u refused original valid path\n",
                index,mismatch_position);return 1;
    }
    for(uint32_t i=0u;i<JAGM_RAM_SIZE;i++){
        if(i>=STACK-0x100u && i<STACK+4u)continue;
        if(ref.ram[i]!=native.ram[i]){
            if(!differences)first=i;
            differences++;
        }
    }
    aa=ref.blit;bb=native.blit;
    aa.bus.ctx=NULL;bb.bus.ctx=NULL;
    if(differences||memcmp(&aa,&bb,sizeof aa)!=0||
       ref.blit.blits!=native.blit.blits||
       ref.blit.unsupported||native.blit.unsupported){
        fprintf(stderr,"index=%u pos=%u DRAM_diff=%u first=%06x blit_state_equal=%d writes=%llu/%llu unsupported=%x/%x\n",
                index,mismatch_position,differences,first,memcmp(&aa,&bb,sizeof aa)==0,
                (unsigned long long)ref.blit.pixels_written,
                (unsigned long long)native.blit.pixels_written,
                ref.blit.unsupported,native.blit.unsupported);
        return 1;
    }
    printf("S43 original triple-cache miss index=%u pos=%u: 3 Blits, changed=%u bytes, DRAM_diff=0, Blitter_state_equal PASS\n",
           index,mismatch_position,changed_pixels);
    return 0;
}
int main(int argc,char **argv)
{
    static const unsigned indices[]={1u,2u,3u,4u,7u,16u,25u,32u};
    FILE *f;
    if(argc!=2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}
    fclose(f);
    if(jagm_init(&ref,rom,sizeof rom))return 2;
    jagm_run_frames(&ref,40);
    memcpy(&boot,&ref,sizeof boot);
    for(unsigned i=0u;i<sizeof indices/sizeof indices[0];i++){
        for(unsigned pos=1u;pos<=3u;pos++)
            if(check_one(indices[i],pos,i*3u+pos))return 1;
    }
    puts("S43: 24 original-ROM three-pass cache-miss Blitter transactions match native DRAM and modeled hardware state");
    return 0;
}
