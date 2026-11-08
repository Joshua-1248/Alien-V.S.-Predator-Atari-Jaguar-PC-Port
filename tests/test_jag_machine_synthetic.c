/* RE24-S10: reference-machine plumbing with a synthetic (non-commercial)
 * cartridge: 68000 programs TOM video registers, a word-swapped OLP and a
 * TYPE0 RGB16 bitmap object; the TRM Object Processor and RGB16 output must
 * place it exactly (TRM pp.12-18, p.23).  Also checks header write-back. */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t cart[0x4000];
static unsigned pc;
static JagMachine M;
static void w16(uint16_t v){cart[pc]=(uint8_t)(v>>8);cart[pc+1]=(uint8_t)v;pc+=2;}
static void w32(uint32_t v){w16((uint16_t)(v>>16));w16((uint16_t)v);}
static void movel(uint32_t imm,uint32_t addr){w16(0x23FC);w32(imm);w32(addr);}
static void movew(uint16_t imm,uint32_t addr){w16(0x33FC);w16(imm);w32(addr);}

int main(void)
{
    uint64_t p0,p1,stop=4;
    uint32_t list=0x1000,data=0x2000;
    /* cartridge header: run address at $800404 */
    cart[0x404]=0x00;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0x00;
    pc=0x2000;
    for(unsigned i=0;i<8;i++)movel(0xF800F800u,data+4u*i);             /* 16 red RGB16 pixels */
    /* TYPE0: YPOS=28 HEIGHT=4 LINK=stop DATA=$2000; XPOS=10 DEPTH=4 PITCH=1 DWIDTH=0 IWIDTH=4 */
    p0=0u|((uint64_t)28<<3)|((uint64_t)4<<14)|((uint64_t)((list+16)>>3)<<24)|((uint64_t)(data>>3)<<43);
    p1=10u|((uint64_t)4<<12)|((uint64_t)1<<15)|((uint64_t)0<<18)|((uint64_t)4<<28);
    movel((uint32_t)(p0>>32),list);movel((uint32_t)p0,list+4);
    movel((uint32_t)(p1>>32),list+8);movel((uint32_t)p1,list+12);
    movel(0,list+16);movel((uint32_t)stop,list+20);
    movew(28,0xF00046);movew(508,0xF00048);                             /* VDB, VDE */
    movew(0x00CB,0xF00038);movew(0x0680,0xF0003C);                      /* HDB1, HDE */
    movel(list<<16,0xF00020);                                          /* OLP, word-swapped */
    movew(0x06C7,0xF00028);                                            /* VMODE RGB16, PWIDTH 3, BGEN */
    w16(0x60FE);                                                       /* bra.s * */
    assert(jagm_init(&M,cart,sizeof cart)==0);
    jagm_run_frames(&M,1); /* the list is not rebuilt, so one field only */
    assert(M.frame_w==320u);
    for(unsigned x=0;x<40;x++){
        uint32_t px=M.frame[0*JAGM_MAX_W+x];
        assert(px==((x>=10&&x<26)?0xF80000u:0u)); /* TRM p.23: 5 MSBs of red, rest zero */
    }
    assert(M.frame[4*JAGM_MAX_W+12]==0u);    /* HEIGHT 4: lines 0..3 only (half-lines 28..34) */
    assert(M.frame[3*JAGM_MAX_W+12]==0xF80000u);
    /* write-back: HEIGHT decremented to zero; DATA advanced by DWIDTH (=0) */
    { uint32_t hi=jagm_read32(&M,list),lo=jagm_read32(&M,list+4);uint64_t q=((uint64_t)hi<<32)|lo;
      assert(((q>>14)&0x3FFu)==0u); assert(((q>>43)<<3)==data); }
    assert(M.int1_pending&4u);              /* STOP object raised the object interrupt (p.20) */
    puts("Jaguar reference machine synthetic-cartridge tests passed");
    return 0;
}
