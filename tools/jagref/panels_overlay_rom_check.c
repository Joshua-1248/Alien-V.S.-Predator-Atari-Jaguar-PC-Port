/* RE25-S45: byte-for-byte retail 68000 $F34E-$F3D8 versus native
 * post-FILES overlay Blitter dispatch. Authentic retail descriptor geometry,
 * synthetic source pixels, original machine/Blitter model. It does NOT run
 * JP80 nor GZIP decompression or claim actual hardware scheduling parity. */
#include "jaguar/jag_machine.h"
#include "jaguar/avp_gpu/panels_overlay.h"
#include "jaguar/avp_gpu/panels_special_lookup.h"
#include "m68k.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#define ROM_BYTES 4194304u
#define STACK 0x6c00u
#define RET_PC 0x1000u
#define DEST 0xdd000u
static uint8_t rom[ROM_BYTES];
static JagMachine ref,native,boot;
static uint32_t rd(void *ctx,uint32_t addr,int width)
{JagMachine*m=ctx;return width==1?jagm_read8(m,addr):width==2?jagm_read16(m,addr):jagm_read32(m,addr);}
static void wr(void *ctx,uint32_t addr,uint32_t val,int width)
{JagMachine*m=ctx;if(width==1)jagm_write8(m,addr,(uint8_t)val);else if(width==2)jagm_write16(m,addr,(uint16_t)val);else jagm_write32(m,addr,val);}
static void fix(JagMachine*m)
{m->gpu.bus.ctx=m;m->dsp.bus.ctx=m;m->blit.bus.ctx=m;}
static int run_one(unsigned case_no,uint8_t x,uint8_t y,uint16_t width,uint16_t height,uint16_t flags)
{
    unsigned steps;
    uint32_t diff=0u,first=0u,changed=0u;
    uint64_t blits0;
    JagBlitter ba,bb;
    AvpView3DBus bus={&native,rd,wr};
    memcpy(&ref,&boot,sizeof ref);fix(&ref);
    /* Synthetic content in original scratch/32-KiB panel slot; not ROM assets. */
    for(uint32_t i=0u;i<0x6000u;i++){
        jagm_write8(&ref,AVP_JAG_PANEL_OVERLAY_SCRATCH+i,
                    (uint8_t)(((i*17u)+(i>>4)+case_no*31u)&0xffu));
    }
    for(uint32_t i=0u;i<0x8000u;i++){
        jagm_write8(&ref,DEST+i,
                    (uint8_t)(((i*19u)+(i>>7)+case_no*13u)&0xffu));
    }
    jagm_write16(&ref,AVP_JAG_PANEL_OVERLAY_WIDTH,width);
    jagm_write16(&ref,AVP_JAG_PANEL_OVERLAY_HEIGHT,height);
    jagm_write16(&ref,AVP_JAG_PANEL_OVERLAY_A2_FLAGS,flags);
    jagm_write16(&ref,RET_PC,0x60feu);
    jagm_write32(&ref,STACK,RET_PC);
    memcpy(&native,&ref,sizeof native);fix(&native);
    blits0=ref.blit.blits;
    m68k_set_reg(M68K_REG_SR,0x2700u);
    m68k_set_reg(M68K_REG_SP,STACK);
    m68k_set_reg(M68K_REG_A1,DEST);
    m68k_set_reg(M68K_REG_D1,(uint16_t)((int16_t)(int8_t)x+0x40));
    m68k_set_reg(M68K_REG_D2,(uint16_t)((int16_t)(int8_t)y+0x40));
    m68k_set_reg(M68K_REG_PC,0xf34eu);
    for(steps=0u;steps<300u;steps++){
        if(m68k_get_reg(NULL,M68K_REG_PC)==RET_PC)break;
        (void)m68k_execute(1);
    }
    if(steps==300u||m68k_get_reg(NULL,M68K_REG_PC)!=RET_PC||ref.blit.blits!=blits0+1u){
        fprintf(stderr,"case %u 68000 failed pc=%06x blits=%llu steps=%u\n",case_no,
                (unsigned)m68k_get_reg(NULL,M68K_REG_PC),
                (unsigned long long)(ref.blit.blits-blits0),steps);return 1;
    }
    for(uint32_t i=0u;i<0x8000u;i++)if(ref.ram[DEST+i]!=native.ram[DEST+i])changed++;
    if(!changed||ref.blit.pixels_written<=native.blit.pixels_written){
        fprintf(stderr,"case %u original did not write pixels\n",case_no);return 1;
    }
    if(!avp_jagpanel_overlay_postload(&bus,x,y,DEST,1,1)){
        fprintf(stderr,"case %u native rejected valid original\n",case_no);return 1;
    }
    for(uint32_t i=0u;i<JAGM_RAM_SIZE;i++){
        if(i>=STACK-0x100u&&i<STACK+4u)continue;
        if(ref.ram[i]!=native.ram[i]){if(!diff)first=i;diff++;}
    }
    ba=ref.blit;bb=native.blit;ba.bus.ctx=NULL;bb.bus.ctx=NULL;
    if(diff||memcmp(&ba,&bb,sizeof ba)!=0||ref.blit.unsupported||native.blit.unsupported){
        fprintf(stderr,"case %u x=%d y=%d width=%u height=%u DRAM_diff=%u first=%06x blit_equal=%d unsupported=%x/%x\n",
                case_no,(int)(int8_t)x,(int)(int8_t)y,width,height,diff,first,
                memcmp(&ba,&bb,sizeof ba)==0,ref.blit.unsupported,native.blit.unsupported);return 1;
    }
    printf("S45 retail overlay postload case %u x=%d y=%d %ux%u pixel_change=%u DRAM_diff=0 Blitter_equal PASS\n",
           case_no,(int)(int8_t)x,(int)(int8_t)y,width,height,changed);
    return 0;
}
int main(int argc,char**argv)
{
    FILE*f;
    static const uint16_t dims[][2]={{128u,128u},{127u,129u},{130u,126u},{64u,64u},{256u,128u}};
    if(argc!=2||(f=fopen(argv[1],"rb"))==NULL)return 2;
    if(fread(rom,1,sizeof rom,f)!=sizeof rom||fgetc(f)!=EOF){fclose(f);return 2;}fclose(f);
    if(jagm_init(&ref,rom,sizeof rom))return 2;
    jagm_run_frames(&ref,40);memcpy(&boot,&ref,sizeof boot);
    unsigned checked=0u;
    for(unsigned id=0u;id<13u;id++){
        uint32_t addr=AVP_JAG_PANEL_SPECIAL_TABLE+id*4u;
        /* $F322 takes the THREE bytes following FILES index, exactly. */
        uint8_t loadid=jagm_read8(&boot,addr+1u);
        uint8_t x=jagm_read8(&boot,addr+2u);
        uint8_t y=jagm_read8(&boot,addr+3u);
        if(!loadid)continue;
        for(unsigned j=0;j<5u;j++){
            uint8_t xx=(uint8_t)(x+(uint8_t)j), yy=(uint8_t)(y-(uint8_t)j);
            if(run_one(checked,xx,yy,dims[j][0],dims[j][1],0x3820u))return 1;
            checked++;
        }
    }
    if(checked<10u){fprintf(stderr,"too few retail overlays: %u\n",checked);return 1;}
    printf("S45 %u retail-descriptor overlay postload Blitter checks PASS\n",checked);
    return 0;
}
