/* RE24-S10: Jaguar GPU/DSP RISC core against explicit statements of the Atari
 * Jaguar Technical Reference Manual Rev.8 (page numbers in each check). */
#include "jaguar/jrisc.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t ext[0x10000]; /* external memory at 0x000000..0x00FFFF */
static uint32_t bus_read(void *c,uint32_t a,int w){(void)c;a&=0xFFFFu;
    if(w==1)return ext[a];
    if(w==2)return (uint32_t)(ext[a]<<8)|ext[a+1];
    return ((uint32_t)ext[a]<<24)|((uint32_t)ext[a+1]<<16)|((uint32_t)ext[a+2]<<8)|ext[a+3];}
static void bus_write(void *c,uint32_t a,uint32_t v,int w){(void)c;a&=0xFFFFu;
    if(w==1){ext[a]=(uint8_t)v;return;}
    if(w==2){ext[a]=(uint8_t)(v>>8);ext[a+1]=(uint8_t)v;return;}
    ext[a]=(uint8_t)(v>>24);ext[a+1]=(uint8_t)(v>>16);ext[a+2]=(uint8_t)(v>>8);ext[a+3]=(uint8_t)v;}

static JRisc J; static uint32_t at;
static uint16_t I(unsigned op,unsigned r1,unsigned r2){return (uint16_t)((op<<10)|((r1&31)<<5)|(r2&31));}
static void emit(uint16_t w){J.ram[at-J.ram_base]=(uint8_t)(w>>8);J.ram[at-J.ram_base+1]=(uint8_t)w;at+=2;}
static void movei(uint32_t v,unsigned r){emit(I(38,0,r));emit((uint16_t)v);emit((uint16_t)(v>>16));}
static void start(int kind){JRiscBus b={0,bus_read,bus_write};jrisc_init(&J,kind,b);at=J.ram_base;}
static void run_n(unsigned n){J.go=1;for(unsigned i=0;i<n;i++)jrisc_step(&J);}
#define R(n) (jrisc_regs(&J)[n])
#define FZ ((J.flags&1u)!=0)
#define FC ((J.flags&2u)!=0)
#define FN ((J.flags&4u)!=0)

int main(void)
{
    /* p.51 MOVEI: first word low, second word high. */
    start(JRISC_GPU);movei(0x12345678u,3);run_n(1);assert(R(3)==0x12345678u&&J.pc==J.ram_base+6);
    /* p.45/47 ADDQ/SUBQ range 1-32 (field 0 encodes 32); ADDQT keeps flags. */
    start(JRISC_GPU);emit(I(35,0,1));emit(I(2,0,1));emit(I(6,0,1));emit(I(3,5,1));run_n(2);assert(R(1)==32u&&!FZ);
    run_n(1);assert(R(1)==0u&&FZ);run_n(1);assert(R(1)==5u&&FZ);
    /* p.45 ADD carry, p.57 SUB borrow, SUBC/ADDC carry-in */
    start(JRISC_GPU);movei(0xFFFFFFFFu,1);emit(I(35,1,2));emit(I(0,2,1));run_n(3);assert(R(1)==0&&FZ&&FC);
    start(JRISC_GPU);emit(I(35,1,1));emit(I(35,2,2));emit(I(4,2,1));run_n(3);assert(R(1)==0xFFFFFFFFu&&FC&&FN);
    emit(I(35,5,3));emit(I(35,0,4));emit(I(5,4,3));run_n(3);assert(R(3)==4u); /* 5-0-borrow */
    /* p.47 CMPQ immediate -16..15: field 0x1F = -1 */
    start(JRISC_GPU);movei(0xFFFFFFFFu,1);emit(I(31,31,1));run_n(2);assert(FZ&&!FC);
    /* p.44 ABS: 0x80000000 unchanged with N set; negative -> C set, N cleared */
    start(JRISC_GPU);movei(0x80000000u,1);emit(I(22,0,1));movei(0xFFFFFFF6u,2);emit(I(22,0,2));
    run_n(2);assert(R(1)==0x80000000u&&FN);run_n(2);assert(R(2)==10u&&FC&&!FN);
    /* p.54 SHLQ encodes 32-n; SHRQ/SHARQ n 1-32; SH sign selects direction */
    start(JRISC_GPU);movei(0x80000001u,1);emit(I(24,32-4,1));run_n(2);assert(R(1)==0x10u&&FC);
    movei(0x80000000u,2);emit(I(27,4,2));run_n(2);assert(R(2)==0xF8000000u);
    movei(0x0000F000u,3);movei((uint32_t)-4,4);emit(I(23,4,3));run_n(3);assert(R(3)==0x000F0000u);
    movei(40u,5);emit(I(23,5,3));run_n(2);assert(R(3)==0u&&FZ); /* >=32 gives zero */
    /* p.53 ROR by bottom 5 bits; C = bit31 of unshifted data */
    start(JRISC_GPU);movei(0x80000001u,1);emit(I(35,1,2));emit(I(28,2,1));run_n(3);assert(R(1)==0xC0000000u&&FC);
    /* p.40 conditions and p.35 delay slot: the instruction after a jump always runs */
    start(JRISC_GPU);emit(I(35,0,1));          /* moveq 0,r1 */
    emit(I(30,1,1));                            /* cmp r1,r1 -> Z */
    emit(I(53,4,2));                            /* jr eq,+4 (from the slot) */
    emit(I(35,7,2));                            /* delay slot: moveq 7,r2 */
    emit(I(35,9,3)); emit(I(35,9,3)); emit(I(35,9,3));
    emit(I(35,4,4));                            /* target */
    run_n(5);assert(R(2)==7u&&R(3)==0u&&R(4)==4u);
    /* p.40 table: NZ not taken when Z set; slot still executes */
    start(JRISC_GPU);emit(I(30,1,1));emit(I(53,3,1));emit(I(35,7,2));emit(I(35,8,3));run_n(4);assert(R(2)==7u&&R(3)==8u);
    /* JUMP (Rn) absolute; condition 0 = always */
    start(JRISC_GPU);movei(J.ram_base+0x40u,1);emit(I(52,1,0));emit(I(57,0,0));
    at=J.ram_base+0x40u;emit(I(35,3,5));run_n(4);assert(R(5)==3u);
    /* p.51 MOVE PC gives the address of the current instruction */
    start(JRISC_GPU);emit(I(57,0,0));emit(I(51,0,6));run_n(2);assert(R(6)==J.ram_base+2u);
    /* p.42/43 banks: MOVETA/MOVEFA; REGPAGE switches; IMASK forces bank 0 */
    start(JRISC_GPU);emit(I(35,9,1));emit(I(36,1,2));run_n(2);assert(J.reg[1][2]==9u);
    J.flags|=JRISC_FLAG_REGPAGE;assert(jrisc_regs(&J)==J.reg[1]);J.flags|=JRISC_FLAG_IMASK;assert(jrisc_regs(&J)==J.reg[0]);
    /* p.42 DIV unsigned; remainder register positive or remainder-minus-divisor */
    { uint32_t rem=0,q=jrisc_div_quotient(100u,7u,0,&rem);assert(q==14u&&(rem==2u||rem==(uint32_t)(2-7)));
      for(uint32_t a=1;a<5000;a+=37)for(uint32_t b=1;b<300;b+=13){q=jrisc_div_quotient(a,b,0,&rem);
        assert(q==a/b);assert(rem==a%b||rem==(uint32_t)((int32_t)(a%b)-(int32_t)b));}
      q=jrisc_div_quotient(3u<<16,2u<<16,1,&rem);assert(q==0x18000u); /* 16.16: 3/2 = 1.5 */ }
    start(JRISC_GPU);movei(1000u,1);movei(10u,2);emit(I(21,2,1));run_n(3);assert(R(1)==100u);
    /* p.41 MAC group */
    start(JRISC_GPU);emit(I(35,3,8));emit(I(35,4,9));emit(I(35,5,10));emit(I(35,6,11));
    emit(I(18,9,8));emit(I(20,11,10));emit(I(19,0,2));run_n(7);assert(R(2)==3u*4u+5u*6u&&R(8)==3u);
    /* p.53/58 PACK/UNPACK bit maps */
    start(JRISC_GPU);movei(0xABCDu,1);emit(I(63,1,1));emit(I(34,1,2));emit(I(63,0,2));run_n(4);
    assert(R(1)==((0xAu<<22)|(0xBu<<13)|0xCDu));assert(R(2)==0xABCDu);
    /* p.54 SAT8/16/24 unsigned saturation, N cleared */
    start(JRISC_GPU);movei((uint32_t)-5,1);emit(I(32,0,1));movei(300u,2);emit(I(32,0,2));movei(0x2000000u,3);emit(I(62,0,3));
    run_n(6);assert(R(1)==0u&&R(2)==255u&&R(3)==0xFFFFFFu&&!FN);
    /* p.49 indexed loads: (R14+n) offset in longs, field 0 = 32 */
    start(JRISC_GPU);movei(J.ram_base+0x100u,14);movei(0xCAFEu,1);emit(I(49,2,1));emit(I(43,2,2));emit(I(35,0,3));emit(I(58,3,4));
    run_n(6);assert(R(2)==0xCAFEu);
    { uint32_t v;jrisc_local_read(&J,J.ram_base+0x108u,&v);assert(v==0xCAFEu); }
    /* p.50 LOADP/STOREP external phrase: high long at the lower address */
    start(JRISC_GPU);memset(ext,0,sizeof ext);movei(0x100u,1);movei(0x11112222u,2);movei(0x33334444u,3);
    { uint32_t a=J.base+0x118u;jrisc_local_write(&J,a,0x11112222u);} emit(I(48,1,3));emit(I(42,1,4));run_n(5);
    assert(bus_read(0,0x100,4)==0x11112222u&&bus_read(0,0x104,4)==0x33334444u&&R(4)==0x33334444u&&J.hidata==0x11112222u);
    /* p.49 LOADB/LOADW external zero-extend */
    ext[0x200]=0xFE;ext[0x201]=0xDC;start(JRISC_GPU);movei(0x200u,1);emit(I(39,1,2));emit(I(40,1,3));run_n(3);assert(R(2)==0xFEu&&R(3)==0xFEDCu);
    /* p.38 interrupts: vector RAM+16n, IMASK set, stacked value + 2 resumes */
    start(JRISC_GPU);J.reg[0][31]=J.ram_base+0x400u;
    at=J.ram_base+0x80u;emit(I(57,0,0));emit(I(35,1,7));emit(I(35,2,7));
    J.pc=J.ram_base+0x80u;J.flags=(1u<<(4+2)); /* INT_ENA2 */
    run_n(1);jrisc_raise(&J,2);run_n(1); /* interrupt entry */
    assert(J.pc==J.ram_base+0x20u&&(J.flags&JRISC_FLAG_IMASK)&&J.reg[0][31]==J.ram_base+0x3FCu);
    { uint32_t v;jrisc_local_read(&J,J.ram_base+0x3FCu,&v);assert(v==J.ram_base+0x80u); }
    /* not taken between a jump and its delay slot (p.39) */
    start(JRISC_GPU);J.reg[0][31]=J.ram_base+0x400u;J.flags=(1u<<4);
    emit(I(53,1,0)); /* jr t,+1 -> instruction after the slot */
    emit(I(35,5,1)); emit(I(35,6,2));
    J.go=1;jrisc_step(&J);jrisc_raise(&J,0);jrisc_step(&J);assert(R(1)==5u); /* slot ran, no entry yet */
    jrisc_step(&J);assert(J.pc==J.ram_base);/* now vectored to int 0 */
    { uint32_t v;jrisc_local_read(&J,J.ram_base+0x3FCu,&v);assert(v+2u==J.ram_base+4u); }
    /* p.58 flags register: IMASK cleared only by writing 0; INT_CLR clears latches */
    start(JRISC_GPU);J.flags|=JRISC_FLAG_IMASK;J.latches=0x1Fu;jrisc_local_write(&J,J.base+0x100u,JRISC_FLAG_IMASK|(1u<<11));
    assert((J.flags&JRISC_FLAG_IMASK)&&J.latches==0x1Bu);jrisc_local_write(&J,J.base+0x100u,0);assert(!(J.flags&JRISC_FLAG_IMASK));
    /* p.60 control: CPUINT raises host interrupt, version 2 reads back */
    { uint32_t v;jrisc_local_write(&J,J.base+0x114u,JRISC_CTRL_CPUINT);assert(J.cpu_irq);jrisc_local_read(&J,J.base+0x114u,&v);assert(((v>>12)&15u)==2u); }
    /* DSP p.99-111: ADDQMOD/SUBQMOD circular, SAT16S, SAT32S 40-bit, MIRROR */
    start(JRISC_DSP);J.modulo=0xFFFFFFF0u;movei(0x100Eu,1);emit(I(63,4,1));emit(I(32,4,1));run_n(2);assert(R(1)==0x1002u);run_n(1);assert(R(1)==0x100Eu);
    start(JRISC_DSP);movei(70000u,1);emit(I(33,0,1));movei((uint32_t)-70000,2);emit(I(33,0,2));run_n(4);assert(R(1)==0x7FFFu&&R(2)==0xFFFF8000u);
    start(JRISC_DSP);movei(0x7FFFu,1);movei(0x7FFFu,2);
    emit(I(18,2,1));emit(I(20,2,1));emit(I(19,0,3));emit(I(42,0,3));
    run_n(2+1+1+1);assert(R(3)==0x7FFE0002u);run_n(1);assert(R(3)==0x7FFE0002u); /* in range: unchanged */
    start(JRISC_DSP);movei(0x00000001u,1);emit(I(48,0,1));run_n(2);assert(R(1)==0x80000000u&&FN);
    /* DSP 40-bit accumulator overflow then SAT32S saturates positive */
    start(JRISC_DSP);movei(0x7FFFu,1);movei(0x7FFFu,2);emit(I(18,2,1));for(int i=0;i<4;i++)emit(I(20,2,1));emit(I(19,0,3));emit(I(42,0,3));
    run_n(2+1+4+1+1);assert(R(3)==0x7FFFFFFFu);
    { uint32_t hi;jrisc_local_read(&J,J.base+0x120u,&hi);assert(hi==1u); /* 5*0x3FFF0001 = 0x1_3FFB_0005 */ }
    /* DSP wave ROM absent -> reads zero, never invented */
    { uint32_t v=1;assert(jrisc_local_read(&J,0xF1D200u,&v)&&v==0u); }
    puts("JRISC TRM instruction-set tests passed");
    return 0;
}
