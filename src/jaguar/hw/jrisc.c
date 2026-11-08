/* Atari Jaguar GPU/DSP RISC core - see include/jaguar/jrisc.h for the model
 * and the Technical Reference Manual (Rev.8) page references.  Every
 * instruction below cites its TRM entry; behaviour the manual leaves undefined
 * ("C - not defined") keeps the previous flag value rather than inventing one. */
#include "jaguar/jrisc.h"
#include <string.h>

#define HZ_LOAD_WAW      0x01u /* bug 13: write-after-load with no read */
#define HZ_DIV_CHAIN     0x02u /* bug 25: divide uses quotient of previous */
#define HZ_IDX_STORE     0x04u /* bug 2: indexed store of long-latency data */
#define HZ_JUMP_SLOT     0x08u /* jump / MOVEI / MOVE PC in a delay slot */
#define HZ_MMULT_LS      0x10u /* MMULT preceded by LOAD/STORE */
#define HZ_MAC_SEQ       0x20u /* IMULTN/IMACN followed by other than IMACN/RESMAC */

static void errata_note(JRisc *j,unsigned n)
{
    if(!j->check_hazards||n>=5u)return;
    j->errata_risks|=1u<<n;
    j->errata_counts[n]++;
}

static unsigned nints(const JRisc *j){return j->kind==JRISC_DSP?6u:5u;}
static unsigned bank(const JRisc *j)
{ return (j->flags&JRISC_FLAG_IMASK)?0u:((j->flags&JRISC_FLAG_REGPAGE)?1u:0u); }
uint32_t *jrisc_regs(JRisc *j){return j->reg[bank(j)];}

void jrisc_init(JRisc *j,int kind,JRiscBus bus)
{
    memset(j,0,sizeof(*j));
    j->kind=kind;j->bus=bus;
    if(kind==JRISC_DSP){j->base=0xF1A000u;j->ram_base=0xF1B000u;j->ram_size=8192u;}
    else{j->base=0xF02000u;j->ram_base=0xF03000u;j->ram_size=4096u;}
    jrisc_reset(j);
}
void jrisc_reset(JRisc *j)
{
    j->go=0;j->flags=0;j->latches=0;j->pc=j->ram_base;j->delay_pending=0;
    j->mac_active=0;j->acc=0;j->cpu_irq=0;j->fault=0;
    j->errata_risks=0;j->preceding_external_read=0;
    j->gpu_hidata_loaded=0;j->flags_nops_pending=0;j->executing=0;
    memset(j->errata_counts,0,sizeof j->errata_counts);
}

int jrisc_in_local(const JRisc *j,uint32_t a)
{
    a&=0xFFFFFFu;
    if(a>=j->ram_base&&a<j->ram_base+j->ram_size)return 1;
    if(a>=j->base+0x100u&&a<j->base+0x124u)return 1;
    if(j->kind==JRISC_DSP&&a>=0xF1D000u&&a<0xF1E000u)return 1;
    return 0;
}

static uint32_t rd_be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void wr_be32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}

static uint32_t flags_read(const JRisc *j)
{ return j->flags & (j->kind==JRISC_DSP?0x1C1FFu:0xC1FFu); }

int jrisc_local_read(JRisc *j,uint32_t a,uint32_t *out)
{
    a&=0xFFFFFCu;
    if(a>=j->ram_base&&a<j->ram_base+j->ram_size){*out=rd_be32(j->ram+(a-j->ram_base));return 1;}
    if(j->kind==JRISC_DSP&&a>=0xF1D000u&&a<0xF1E000u){
        /* p.98: eight 128-entry signed 16-bit tables, sign-extended to 32. */
        uint32_t o=(a-0xF1D000u)>>2;
        j->wave_rom_reads++;
        if(j->wave_rom){uint16_t w=(uint16_t)((j->wave_rom[o*2]<<8)|j->wave_rom[o*2+1]);
            *out=(uint32_t)(int32_t)(int16_t)w;}
        else *out=0; /* wave ROM is not distributed; absent => zero */
        return 1;
    }
    switch(a-j->base){
    case 0x100:*out=flags_read(j);return 1;
    case 0x110:*out=j->pc;return 1;
    case 0x114:{uint32_t v=j->go?1u:0u;
        v|=j->ctrl_misc&(JRISC_CTRL_SINGLE_STEP|JRISC_CTRL_BUS_HOG);
        v|=(j->latches&0x1Fu)<<6;v|=2u<<12; /* VERSION 2: first production */
        if(j->kind==JRISC_DSP&&(j->latches&0x20u))v|=1u<<16;
        *out=v;return 1;}
    case 0x118:*out=j->kind==JRISC_DSP?0u:j->hidata;return 1;
    case 0x11C:*out=j->remain;return 1;
    case 0x120:if(j->kind==JRISC_DSP){*out=(uint32_t)(int32_t)(int8_t)(uint8_t)((uint64_t)j->acc>>32);return 1;}
        *out=0;return 1;
    default: if(a>=j->base+0x100u&&a<j->base+0x124u){*out=0;return 1;} /* write-only */
    }
    return 0;
}

int jrisc_local_write(JRisc *j,uint32_t a,uint32_t v)
{
    a&=0xFFFFFCu;
    if(a>=j->ram_base&&a<j->ram_base+j->ram_size){wr_be32(j->ram+(a-j->ram_base),v);return 1;}
    if(j->kind==JRISC_DSP&&a>=0xF1D000u&&a<0xF1E000u)return 1; /* ROM */
    switch(a-j->base){
    case 0x100:{uint32_t keep_imask=j->flags&JRISC_FLAG_IMASK;
        uint32_t ena_mask=j->kind==JRISC_DSP?0x101F0u:0x1F0u;
        uint32_t clr=(v>>9)&0x1Fu; if(j->kind==JRISC_DSP&&(v&(1u<<17)))clr|=0x20u;
        j->latches&=~clr;
        /* p.58: IMASK is cleared by writing 0; writing 1 has no effect. */
        if(!(v&JRISC_FLAG_IMASK))keep_imask=0;
        j->flags=(v&(0x7u|ena_mask|JRISC_FLAG_REGPAGE|JRISC_FLAG_DMAEN))|keep_imask;
        if(v&JRISC_FLAG_DMAEN)errata_note(j,1); /* OP conflict depends on scheduling */
        if(j->executing)j->flags_nops_pending=2u;
        return 1;}
    case 0x104:j->mtxc=v;return 1;
    case 0x108:j->mtxa=v;return 1;
    case 0x10C:j->bigend=v;return 1;
    case 0x110:j->pc=v&0xFFFFFEu;return 1;
    case 0x114:
        if(v&JRISC_CTRL_CPUINT)j->cpu_irq=1;
        if(v&JRISC_CTRL_INT0)jrisc_raise(j,0);
        j->ctrl_misc=v&(JRISC_CTRL_SINGLE_STEP|JRISC_CTRL_BUS_HOG);
        if((v&1u)&&!j->go){j->delay_pending=0;}
        j->go=v&1u;
        return 1;
    case 0x118:if(j->kind==JRISC_DSP)j->modulo=v;else j->hidata=v;return 1;
    case 0x11C:j->divctrl=v;return 1;
    default: if(a>=j->base+0x100u&&a<j->base+0x124u)return 1;
    }
    return 0;
}

void jrisc_raise(JRisc *j,unsigned n){if(n<nints(j))j->latches|=1u<<n;}

static uint32_t enables(const JRisc *j)
{ uint32_t e=(j->flags>>4)&0x1Fu; if(j->kind==JRISC_DSP&&(j->flags&(1u<<16)))e|=0x20u; return e; }

/* ---- memory ---- */
static uint32_t mem_read(JRisc *j,uint32_t a,int width,unsigned *extra)
{
    uint32_t v;
    a&=0xFFFFFFu;
    if(jrisc_in_local(j,a)){ /* p.36: only 32-bit transfers locally */
        (void)jrisc_local_read(j,a,&v);return v;
    }
    *extra+=6u; /* external latency estimate, not bus-cycle accuracy */
    j->preceding_external_read=1u;
    if(j->kind==JRISC_GPU&&j->gpu_hidata_loaded)
        errata_note(j,2); /* JDEV0002 GPU/DSP 8: any external LOAD changes HIDATA */
    return j->bus.read?j->bus.read(j->bus.ctx,a,width):0u;
}
static void mem_write(JRisc *j,uint32_t a,uint32_t v,int width,unsigned *extra)
{
    a&=0xFFFFFFu;
    if(jrisc_in_local(j,a)){(void)jrisc_local_write(j,a,v);return;}
    *extra+=2u;
    if(j->kind==JRISC_DSP&&!j->preceding_external_read)
        errata_note(j,3); /* necessary, not sufficient, condition of JDEV0002 item 7 */
    if(j->bus.write)j->bus.write(j->bus.ctx,a,v,width);
}
static uint16_t fetch16(JRisc *j,uint32_t a)
{
    uint32_t l;a&=0xFFFFFEu;
    if(jrisc_in_local(j,a)){(void)jrisc_local_read(j,a,&l);return (uint16_t)((a&2u)?l:(l>>16));}
    return (uint16_t)(j->bus.read?j->bus.read(j->bus.ctx,a,2):0u);
}

/* ---- divide unit (p.42): non-restoring, two bits per tick.  The remainder
 * register holds the final partial remainder without the restoring step,
 * i.e. "either positive (the remainder) or negative (remainder minus the
 * divisor)", exactly as the manual describes. 16.16 mode divides the 48-bit
 * value dividend<<16, producing 32 quotient bits. */
uint32_t jrisc_div_quotient(uint32_t dividend,uint32_t divisor,int offset_mode,
                            uint32_t *remainder_reg)
{
    uint64_t num=offset_mode?((uint64_t)dividend<<16):(uint64_t)dividend;
    int64_t r=(int64_t)(num>>32);   /* initial partial remainder (16.16: top 16 bits) */
    uint32_t low=(uint32_t)num,q=0;
    for(int i=31;i>=0;i--){
        int64_t bit=(low>>i)&1u;
        if(r>=0)r=r*2+bit-(int64_t)divisor;
        else r=r*2+bit+(int64_t)divisor;
        q=(q<<1)|(r>=0?1u:0u);
    }
    if(remainder_reg)*remainder_reg=(uint32_t)r;
    return q;
}

static void setzn(JRisc *j,uint32_t r)
{ j->flags=(j->flags&~(JRISC_FLAG_Z|JRISC_FLAG_N))|(r?0u:JRISC_FLAG_Z)|((r&0x80000000u)?JRISC_FLAG_N:0u); }
static void setc(JRisc *j,int c){ if(c)j->flags|=JRISC_FLAG_C; else j->flags&=~JRISC_FLAG_C; }
static int fz(const JRisc *j){return (j->flags&JRISC_FLAG_Z)!=0;}
static int fc(const JRisc *j){return (j->flags&JRISC_FLAG_C)!=0;}
static int fn(const JRisc *j){return (j->flags&JRISC_FLAG_N)!=0;}

/* p.40 condition field */
static int cond_true(const JRisc *j,unsigned cc)
{
    if((cc&1u)&&fz(j))return 0;
    if((cc&2u)&&!fz(j))return 0;
    { int f=(cc&16u)?fn(j):fc(j);
      if((cc&4u)&&f)return 0;
      if((cc&8u)&&!f)return 0; }
    return 1;
}

static int64_t acc_wrap(const JRisc *j,int64_t v)
{
    if(j->kind==JRISC_DSP){ /* 40-bit signed (p.99) */
        uint64_t u=(uint64_t)v&0xFFFFFFFFFFull; if(u&0x8000000000ull)u|=0xFFFFFF0000000000ull; return (int64_t)u; }
    return (int64_t)(int32_t)(uint32_t)v;
}

static int is_jump(unsigned op){return op==52u||op==53u;}

const char *jrisc_mnemonic(int kind,unsigned op,unsigned r1)
{
    static const char *g[64]={"add","addc","addq","addqt","sub","subc","subq","subqt","neg","and","or","xor","not","btst","bset","bclr",
        "mult","imult","imultn","resmac","imacn","div","abs","sh","shlq","shrq","sha","sharq","ror","rorq","cmp","cmpq",
        "sat8","sat16","move","moveq","moveta","movefa","movei","loadb","loadw","load","loadp","load(r14+n)","load(r15+n)","storeb",
        "storew","store","storep","store(r14+n)","store(r15+n)","move pc","jump","jr","mmult","mtoi","normi","nop","load(r14+rn)","load(r15+rn)",
        "store(r14+rn)","store(r15+rn)","sat24","pack"};
    op&=63u;
    if(kind==JRISC_DSP){
        switch(op){case 32:return "subqmod";case 33:return "sat16s";case 42:return "sat32s";
        case 48:return "mirror";case 63:return "addqmod";case 62:return "(dsp62)";default:break;}
    }else if(op==63)return r1?"unpack":"pack";
    return g[op];
}

static unsigned take_interrupt(JRisc *j)
{
    uint32_t pend=j->latches&enables(j);
    unsigned n;
    uint32_t *r;
    if(!pend||(j->flags&JRISC_FLAG_IMASK)||j->delay_pending||j->mac_active)return 0;
    for(n=nints(j);n-->0;)if(pend&(1u<<n))break;
    j->flags|=JRISC_FLAG_IMASK; /* forces bank 0 */
    r=j->reg[0];
    /* The stacked value is the address of the last instruction word executed:
     * the ISR adds two to resume at the next one (p.38).  After a jump's delay
     * slot the next instruction is the jump target. */
    { unsigned ex=0; r[31]-=4u; mem_write(j,r[31],j->pc-2u,4,&ex); }
    r[30]=j->ram_base+16u*n; /* "corrupted" (p.38); holds the vector here */
    j->pc=j->ram_base+16u*n;
    return 3u;
}

unsigned jrisc_step(JRisc *j)
{
    uint32_t *R,s,d,res,pc0;
    unsigned op,r1,r2,cyc=1,extra=0,ins;
    int in_slot=j->delay_pending;
    if(!j->go||j->fault)return 0;
    if(!in_slot){unsigned c=take_interrupt(j);if(c){j->cycles+=c;return c;}}
    pc0=j->pc;
    ins=fetch16(j,pc0);
    op=ins>>10;r1=(ins>>5)&31u;r2=ins&31u;
    R=j->reg[bank(j)];
    if(j->flags_nops_pending){
        if(op!=57u)errata_note(j,4);
        j->flags_nops_pending--;
    }
    if((op==52u||op==53u)&&!jrisc_in_local(j,pc0))
        errata_note(j,0); /* external branch fetch not reliably executable */
    j->executing=1;
    j->op_count[op]++;
    j->pc=pc0+2u;
    if(in_slot&&(is_jump(op)||op==38u||op==51u)){j->hazards|=HZ_JUMP_SLOT;}
    if(j->mac_active&&!(op==20u||op==19u||(op==54u)))j->hazards|=HZ_MAC_SEQ;
    s=R[r1];d=R[r2];
    switch(op){
    case 0:{uint64_t t=(uint64_t)d+s;res=(uint32_t)t;setzn(j,res);setc(j,(t>>32)&1u);R[r2]=res;break;}          /* ADD p.45 */
    case 1:{uint64_t t=(uint64_t)d+s+(fc(j)?1u:0u);res=(uint32_t)t;setzn(j,res);setc(j,(t>>32)&1u);R[r2]=res;break;} /* ADDC */
    case 2:{uint32_t n=r1?r1:32u;uint64_t t=(uint64_t)d+n;res=(uint32_t)t;setzn(j,res);setc(j,(t>>32)&1u);R[r2]=res;break;} /* ADDQ */
    case 3:{uint32_t n=r1?r1:32u;R[r2]=d+n;break;}                                                                    /* ADDQT */
    case 4:{res=d-s;setzn(j,res);setc(j,d<s);R[r2]=res;break;}                                                       /* SUB p.57 */
    case 5:{uint64_t sub=(uint64_t)s+(fc(j)?1u:0u);res=(uint32_t)((uint64_t)d-sub);setzn(j,res);setc(j,(uint64_t)d<sub);R[r2]=res;break;} /* SUBC */
    case 6:{uint32_t n=r1?r1:32u;res=d-n;setzn(j,res);setc(j,d<n);R[r2]=res;break;}                                   /* SUBQ */
    case 7:{uint32_t n=r1?r1:32u;R[r2]=d-n;break;}                                                                    /* SUBQT */
    case 8:{res=0u-d;setzn(j,res);setc(j,d!=0u);R[r2]=res;break;}                                                    /* NEG p.52 */
    case 9:{res=d&s;setzn(j,res);R[r2]=res;break;}                                                                    /* AND */
    case 10:{res=d|s;setzn(j,res);R[r2]=res;break;}                                                                   /* OR */
    case 11:{res=d^s;setzn(j,res);R[r2]=res;break;}                                                                   /* XOR */
    case 12:{res=~d;setzn(j,res);R[r2]=res;break;}                                                                    /* NOT */
    case 13:{j->flags=(j->flags&~JRISC_FLAG_Z)|(((d>>r1)&1u)?0u:JRISC_FLAG_Z);break;}                                /* BTST: Z only */
    case 14:{res=d|(1u<<r1);setzn(j,res);R[r2]=res;break;}                                                            /* BSET */
    case 15:{res=d&~(1u<<r1);setzn(j,res);R[r2]=res;break;}                                                           /* BCLR */
    case 16:{res=(d&0xFFFFu)*(s&0xFFFFu);setzn(j,res);R[r2]=res;break;}                                              /* MULT p.51 */
    case 17:{res=(uint32_t)((int32_t)(int16_t)d*(int32_t)(int16_t)s);setzn(j,res);R[r2]=res;break;}                 /* IMULT */
    case 18:{int32_t p=(int32_t)(int16_t)d*(int32_t)(int16_t)s;j->acc=acc_wrap(j,p);setzn(j,(uint32_t)p);j->mac_active=1;break;} /* IMULTN */
    case 19:{R[r2]=(uint32_t)j->acc;j->mac_active=0;break;}                                                          /* RESMAC */
    case 20:{int32_t p=(int32_t)(int16_t)d*(int32_t)(int16_t)s;j->acc=acc_wrap(j,j->acc+p);j->mac_active=1;break;}   /* IMACN */
    case 21:{ /* DIV p.47/p.42 */
        if(j->check_hazards&&j->last_was_div&&(r1==j->last_div_dst||r2==j->last_div_dst))j->hazards|=HZ_DIV_CHAIN;
        R[r2]=jrisc_div_quotient(d,s,(j->divctrl&1u)!=0,&j->remain);
        cyc=16;j->last_was_div=1;j->last_div_dst=r2;goto done_nodivreset;}
    case 22:{ /* ABS p.44 */
        if(d==0x80000000u){setzn(j,d);setc(j,1);break;}
        if(d&0x80000000u){res=0u-d;setc(j,1);}else{res=d;setc(j,0);}
        j->flags&=~(JRISC_FLAG_Z|JRISC_FLAG_N);if(!res)j->flags|=JRISC_FLAG_Z;R[r2]=res;break;}
    case 23: case 26:{ /* SH / SHA p.54-55 */
        int32_t c=(int32_t)s;
        if(c>=0){ setc(j,d&1u);
            if(op==23)res=c>=32?0u:(d>>c);
            else res=c>=32?(uint32_t)((int32_t)d>>31):(uint32_t)((int32_t)d>>c);
        }else{ setc(j,d>>31); c=-c; res=c>=32?0u:(d<<c); }
        setzn(j,res);R[r2]=res;break;}
    case 24:{unsigned n=32u-r1;setc(j,d>>31);res=n>=32u?0u:(d<<n);setzn(j,res);R[r2]=res;break;}                     /* SHLQ: field=32-n */
    case 25:{unsigned n=r1?r1:32u;setc(j,d&1u);res=n>=32u?0u:(d>>n);setzn(j,res);R[r2]=res;break;}                   /* SHRQ */
    case 27:{unsigned n=r1?r1:32u;setc(j,d&1u);res=(uint32_t)((int32_t)d>>(n>=32u?31u:n));setzn(j,res);R[r2]=res;break;} /* SHARQ */
    case 28:{unsigned n=s&31u;setc(j,d>>31);res=n?((d>>n)|(d<<(32u-n))):d;setzn(j,res);R[r2]=res;break;}             /* ROR */
    case 29:{unsigned n=r1&31u;setc(j,d>>31);res=n?((d>>n)|(d<<(32u-n))):d;setzn(j,res);R[r2]=res;break;}            /* RORQ (32==0) */
    case 30:{res=d-s;setzn(j,res);setc(j,d<s);break;}                                                                /* CMP */
    case 31:{uint32_t imm=(uint32_t)(((int32_t)(r1<<27))>>27);res=d-imm;setzn(j,res);setc(j,d<imm);break;}          /* CMPQ -16..15 */
    case 32:
        if(j->kind==JRISC_DSP){uint32_t n=r1?r1:32u;uint32_t t=d-n;setc(j,d<n);res=(d&j->modulo)|(t&~j->modulo);setzn(j,res);R[r2]=res;} /* SUBQMOD */
        else{int32_t v=(int32_t)d;res=v<0?0u:(v>255?255u:(uint32_t)v);j->flags&=~(JRISC_FLAG_Z|JRISC_FLAG_N);if(!res)j->flags|=JRISC_FLAG_Z;R[r2]=res;} /* SAT8 */
        break;
    case 33:
        if(j->kind==JRISC_DSP){int32_t v=(int32_t)d;res=(uint32_t)(v<-32768?-32768:(v>32767?32767:v));} /* SAT16S */
        else{int32_t v=(int32_t)d;res=v<0?0u:(v>65535?65535u:(uint32_t)v);}                            /* SAT16 */
        j->flags&=~(JRISC_FLAG_Z|JRISC_FLAG_N);if(!res)j->flags|=JRISC_FLAG_Z;R[r2]=res;break;          /* N cleared */
    case 34:R[r2]=s;break;                                                                               /* MOVE */
    case 35:R[r2]=r1;break;                                                                              /* MOVEQ 0..31 */
    case 36:j->reg[bank(j)^1u][r2]=s;break;                                                              /* MOVETA */
    case 37:R[r2]=j->reg[bank(j)^1u][r1];break;                                                          /* MOVEFA */
    case 38:{uint32_t lo=fetch16(j,pc0+2u),hi=fetch16(j,pc0+4u);R[r2]=lo|(hi<<16);j->pc=pc0+6u;cyc=3;break;} /* MOVEI low,high */
    case 39:{uint32_t v=mem_read(j,s,1,&extra);R[r2]=jrisc_in_local(j,s)?v:(v&0xFFu);break;}           /* LOADB */
    case 40:{uint32_t v=mem_read(j,s,2,&extra);R[r2]=jrisc_in_local(j,s)?v:(v&0xFFFFu);break;}         /* LOADW */
    case 41:R[r2]=mem_read(j,s,4,&extra);break;                                                         /* LOAD */
    case 42:
        if(j->kind==JRISC_DSP){ /* SAT32S p.99: 40-bit = acc overflow byte : register */
            int64_t v=(int64_t)(((uint64_t)(uint8_t)((uint64_t)j->acc>>32)<<32)|d);
            if(v&0x8000000000ll)v|=(int64_t)0xFFFFFF0000000000ull;
            res=v<(int64_t)INT32_MIN?0x80000000u:(v>(int64_t)INT32_MAX?0x7FFFFFFFu:(uint32_t)v);
            setzn(j,res);R[r2]=res;
        }else{ /* LOADP p.50: high long at addr (big-endian phrase), low at addr+4 */
            if(jrisc_in_local(j,s))R[r2]=mem_read(j,s,4,&extra);
            else{j->hidata=mem_read(j,s&~7u,4,&extra);R[r2]=mem_read(j,(s&~7u)+4u,4,&extra);
                 j->gpu_hidata_loaded=1u;}
        }
        break;
    case 43: case 44:{uint32_t n=r1?r1:32u;uint32_t a=R[op==43?14:15]+n*4u;R[r2]=mem_read(j,a,4,&extra);cyc+=2;break;} /* LOAD (R14/15+n) */
    case 45:mem_write(j,s,jrisc_in_local(j,s)?d:(d&0xFFu),jrisc_in_local(j,s)?4:1,&extra);break;                                /* STOREB */
    case 46:mem_write(j,s,jrisc_in_local(j,s)?d:(d&0xFFFFu),jrisc_in_local(j,s)?4:2,&extra);break;                              /* STOREW */
    case 47:mem_write(j,s,d,4,&extra);break;                                                             /* STORE */
    case 48:
        if(j->kind==JRISC_DSP){res=0;for(int b=0;b<32;b++)if(d&(1u<<b))res|=1u<<(31-b);setzn(j,res);R[r2]=res;} /* MIRROR */
        else{ if(jrisc_in_local(j,s))mem_write(j,s,d,4,&extra);
              else{mem_write(j,s&~7u,j->hidata,4,&extra);mem_write(j,(s&~7u)+4u,d,4,&extra);} }      /* STOREP */
        break;
    case 49: case 50:{uint32_t n=r1?r1:32u;uint32_t a=R[op==49?14:15]+n*4u;mem_write(j,a,d,4,&extra);cyc+=2;break;} /* STORE (R14/15+n) */
    case 51:R[r2]=pc0;break;                                                                             /* MOVE PC */
    case 52:case 53:{ /* JUMP / JR p.48 - delayed by one instruction */
        if(in_slot){j->fault=1;j->fault_pc=pc0;break;} /* two sequential jumps: unpredictable */
        if(cond_true(j,r2)){
            uint32_t t=op==52?s:(pc0+2u+(uint32_t)(((int32_t)(r1<<27))>>26));
            j->delay_pending=1;j->delay_target=t&0xFFFFFEu;
        }
        j->last_pc=pc0;j->last_was_div=0;j->instructions++;j->cycles+=cyc;
        j->executing=0;return cyc;}
    case 54:{ /* MMULT p.42 - element order of the packed register halves is
                 not stated by the manual; high half first is assumed here and
                 marked unverified in the audit. */
        unsigned w=j->mtxc&15u,colstep=(j->mtxc&16u)?w*4u:4u;
        uint32_t a=j->ram_base|(j->mtxa&0xFFCu);
        int64_t sum=0;uint32_t *alt=j->reg[1];
        for(unsigned i=0;i<w;i++){
            uint32_t pr=alt[(r1+i/2u)&31u];int16_t x=(int16_t)((i&1u)?(pr&0xFFFFu):(pr>>16));
            uint32_t m=0;(void)jrisc_local_read(j,a,&m);
            sum=acc_wrap(j,sum+(int64_t)x*(int16_t)m);a+=colstep;
        }
        j->acc=sum;res=(uint32_t)sum;setzn(j,res);R[r2]=res;cyc=w+2u;break;}
    case 55:{res=(s&0x007FFFFFu)|((s&0x80000000u)?0xFF800000u:0u);setzn(j,res);R[r2]=res;break;}      /* MTOI (unverified detail) */
    case 56:{int h=-1;for(int b=31;b>=0;b--)if(s&(1u<<b)){h=b;break;}res=h<0?0u:(uint32_t)(h-23);setzn(j,res);R[r2]=res;break;} /* NORMI */
    case 57:break;                                                                                       /* NOP */
    case 58: case 59:{uint32_t a=R[op==58?14:15]+s;R[r2]=mem_read(j,a,4,&extra);cyc+=2;break;}        /* LOAD (R14/15+Rn) */
    case 60: case 61:{uint32_t a=R[op==60?14:15]+s;mem_write(j,a,d,4,&extra);cyc+=2;break;}            /* STORE (R14/15+Rn) */
    case 62:
        if(j->kind==JRISC_DSP){j->fault=2;j->fault_pc=pc0;break;}                                        /* not a DSP opcode */
        {int32_t v=(int32_t)d;res=v<0?0u:(v>0xFFFFFF?0xFFFFFFu:(uint32_t)v);
         j->flags&=~(JRISC_FLAG_Z|JRISC_FLAG_N);if(!res)j->flags|=JRISC_FLAG_Z;R[r2]=res;break;}        /* SAT24 */
    case 63:
        if(j->kind==JRISC_DSP){uint32_t n=r1?r1:32u;uint64_t t=(uint64_t)d+n;setc(j,(t>>32)&1u);
            res=(d&j->modulo)|((uint32_t)t&~j->modulo);setzn(j,res);R[r2]=res;}                      /* ADDQMOD */
        else if(r1==0)R[r2]=((d>>10)&0xF000u)|((d>>5)&0x0F00u)|(d&0xFFu);                             /* PACK */
        else R[r2]=((d&0xF000u)<<10)|((d&0x0F00u)<<5)|(d&0xFFu);                                       /* UNPACK */
        break;
    }
    j->last_was_div=0;
done_nodivreset:
    if(op!=18u&&op!=20u)j->mac_active=0;
    j->last_pc=j->pc-2u;      /* last instruction word fetched (MOVEI: its data) */
    j->instructions++;
    if(in_slot){j->delay_pending=0;j->pc=j->delay_target;cyc+=3u;}
    cyc+=extra;j->cycles+=cyc;
    j->executing=0;
    if(j->ctrl_misc&JRISC_CTRL_SINGLE_STEP){/* single-step: host must re-issue */}
    return cyc;
}

uint64_t jrisc_run(JRisc *j,uint64_t cycles)
{
    uint64_t start=j->cycles;
    while(j->cycles-start<cycles){if(!jrisc_step(j))break;}
    return j->cycles-start;
}
