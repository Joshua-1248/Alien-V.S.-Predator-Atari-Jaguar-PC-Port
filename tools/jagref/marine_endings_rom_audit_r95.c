/* RE35 R95: read-only verification of original 68000 Marine endings.
 * The cartridge's compressed 68000 program is decoded by its own bootstrap;
 * auditing the raw ROM at RAM addresses $01DAxx would read compressed data.
 * No proprietary ROM/program/audio bytes are embedded in this tool. */
#include "jaguar/jag_machine.h"
#include "m68k.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t mem32(const JagMachine *m,uint32_t address){
    const uint8_t *p=m->ram+address;
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static int expect_imm(const JagMachine *m,uint32_t a,uint32_t imm,uint32_t dest,const char *label){
    /* MOVE.L #immediate, $absolute.l: 23 FC [imm32] [abs32]. */
    const uint8_t *p=m->ram+a;
    const int ok=p[0]==0x23u&&p[1]==0xfcu&&mem32(m,a+2u)==imm&&mem32(m,a+6u)==dest;
    if(!ok)fprintf(stderr,"ROM mismatch: %s at $%05x\n",label,a);
    return ok;
}
static int has_countdown(const JagMachine *m,uint32_t addr,const char *word){
    const size_t n=strlen(word);
    for(unsigned k=0u;k+n<=38u;k++)
        if(memcmp(m->ram+addr+k,word,n)==0)return 1;
    return 0;
}
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"usage: %s user-owned-AVP.jag\n",argv[0]);return 2;}
    FILE *f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
    if(fseek(f,0,SEEK_END)!=0||ftell(f)!=4194304L||fseek(f,0,SEEK_SET)!=0){fclose(f);return 2;}
    uint8_t *rom=(uint8_t *)malloc(4194304u);
    JagMachine *m=(JagMachine *)malloc(sizeof(*m));
    if(!rom||!m){fclose(f);free(rom);free(m);return 2;}
    int ok=fread(rom,1,4194304u,f)==4194304u;fclose(f);
    if(!ok||jagm_init(m,rom,4194304u)!=0){free(rom);free(m);return 2;}
    for(unsigned field=0u;field<350u;field++){
        jagm_set_pad(m,0u,0u);jagm_run_frames(m,1u);
        if(m->halted){fprintf(stderr,"ROM bootstrap stopped: %s\n",m->halt_reason);ok=0;break;}
    }
    if(ok){
        ok&=expect_imm(m,0x1DA20u,0x18u,0x4BEB0u,"Do_Lose initial X");
        ok&=expect_imm(m,0x1DA2Au,0xA6u,0x4BEB4u,"Do_Lose initial Y");
        ok&=expect_imm(m,0x1DAAEu,0x18u,0x4BEB0u,"Do_Lose print X");
        ok&=expect_imm(m,0x1DAB8u,0xA6u,0x4BEB4u,"Do_Lose print Y");
        ok&=expect_imm(m,0x1DAA4u,0x2B036u,0x4BE70u,"base-explosion record");
        ok&=expect_imm(m,0x1DB92u,0x18u,0x4BEB0u,"Do_Escap text X");
        ok&=expect_imm(m,0x1DB9Cu,0xA6u,0x4BEB4u,"Do_Escap text Y");
        ok&=expect_imm(m,0x1DC42u,1u,0x4BEC8u,"spoken-cue typing mute");
        ok&=expect_imm(m,0x1E226u,0u,0x4BEC8u,"escape-report typing enabled");
        ok&=mem32(m,0x1DC32u+2u)==0x22848u; /* first spoken sound before Show_Tex */
        ok&=mem32(m,0x1DC4Cu)==0x4eb90001u&&((unsigned)m->ram[0x1DC50u]<<8|m->ram[0x1DC51u])==0xD1E0u;
        ok&=mem32(m,0x1DC52u)==0x20390004u&&mem32(m,0x1DC56u)==0xbea8d1b9u;
        ok&=mem32(m,0x1DCA0u)==0x20390004u&&mem32(m,0x1DCA4u)==0xbea8d1b9u;
        /* R96 deeper disassembly: Show_Tex $1D3FE branches over the
         * $1D40C VBlank wait when the $4BEC8 fast flag is set.
         * Retail 0x46=70 VBlank pacing is documented, not silently
         * represented as an exact second at 60 Hz. */
        ok&=mem32(m,0x1D3FEu)==0x0CB90000u;
        ok&=mem32(m,0x1D408u)==0x6700FEEAu;
        ok&=mem32(m,0x1D40Cu)==0x4EB90001u;
        ok&=mem32(m,0x1E15Au)==0x42B87FA8u;
        ok&=m->ram[0x1DC68u]==0x70u && m->ram[0x1DC69u]==0x46u;
        ok&=m->ram[0x1DCB6u]==0x70u && m->ram[0x1DCB7u]==0x46u;
        ok&=m->ram[0x1DD8Eu]==0x70u && m->ram[0x1DD8Fu]==0x46u;
        ok&=has_countdown(m,0x2ADDAu,"Five");
        ok&=has_countdown(m,0x2AE01u,"Four");
        ok&=has_countdown(m,0x2AE28u,"Three");
        ok&=memcmp(m->ram+0x2B036u,"Sector Time",11u)==0;
    }
    if(ok)puts("PASS: retail Jaguar Do_Lose/Do_Escap X,Y, countdown sound order, line advances and TYP1B gating");
    else fprintf(stderr,"Original-ROM Marine ending crosswalk FAILED\n");
    free(m->audio);free(m);free(rom);
    return ok?0:1;
}
