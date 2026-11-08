/* RE24 Session 2 (Claude): native C port of the original AvP 3D-view
 * renderer.  See include/jaguar/avp_gpu/view3d.h.
 *
 * Ported by hand from the retail game's GPU view program.  Every routine is
 * named after the original GPU address it corresponds to.  Values that the
 * original keeps in GPU registers across calls are kept in the same register
 * slots (R[] = primary bank, ALT[] = secondary bank) so the port reproduces
 * the original exactly, including values it inherits from earlier views.
 *
 * Conventions copied from the original instruction set (TRM Rev.8):
 *  - MULT is an unsigned 16x16 multiply of the low halves; the original
 *    builds 32x16 fixed-point products from two of them (mul32x16()).
 *  - IMULT/IMULTN/IMACN multiply signed 16-bit low halves.
 *  - DIV is the unsigned 32/32 divider, optionally in 16.16 offset mode.
 *  - Comparisons in the original test only the sign (N) or carry (C) of a
 *    32-bit subtraction; neg32()/carry tests below keep that exact meaning,
 *    which differs from a C signed comparison when the difference overflows.
 */
#include "jaguar/avp_gpu/view3d.h"

/* ---- Jaguar bus ------------------------------------------------------ */
#define RD32(a) bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,4)
#define RD16(a) (bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,2)&0xFFFFu)
#define RD8(a)  (bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,1)&0xFFu)
#define WR32(a,v) bus->wr(bus->ctx,(uint32_t)(a)&0xFFFFFFu,(uint32_t)(v),4)
#define WR16(a,v) bus->wr(bus->ctx,(uint32_t)(a)&0xFFFFFFu,(uint32_t)(v)&0xFFFFu,2)

/* Blitter registers (TRM pp.70-77).  The original addresses them through
 * secondary-bank registers set up by the GPU start-up code:
 *   alt r14 = A1_BASE ($F02200), alt r15 = A2_BASE ($F02224),
 *   alt r10 = B_CMD ($F02238), alt r11 = B_COUNT ($F0223C). */
#define A1_BASE   (ALT[14])
#define A1_FLAGS  (ALT[14]+4u)
#define A1_PIXEL  (ALT[14]+12u)
#define A1_FPIXEL (ALT[14]+24u)
#define A1_INC    (ALT[14]+28u)
#define A1_FINC   (ALT[14]+32u)
#define A2_BASE   (ALT[15])
#define A2_FLAGS  (ALT[15]+4u)
#define A2_PIXEL  (ALT[15]+12u)
#define B_CMD     (ALT[10])
#define B_COUNT   (ALT[11])
#define B_PATD    0xF02268u
#define B_IINC    0xF02270u

/* The Blitter wait loop of the original ("movefa r10,r28 / load (r28),r29 /
 * btst #0,r29 / jr EQ").  The Blitter models used by the port complete a
 * command when it is written, so the status read always returns idle. */
static void blit_wait(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t st;
    s->r[28]=s->alt[10];
    do st=bus->rd(bus->ctx,s->r[28]&0xFFFFFFu,4); while(!(st&1u));
    s->r[29]=st;
}

static uint32_t mul16(uint32_t a,uint32_t b){return (a&0xFFFFu)*(b&0xFFFFu);}
/* hi(a)*lo(b) + (lo(a)*lo(b))>>16, the original's 32x16 fixed product */
static uint32_t mul32x16(uint32_t a,uint32_t b){return mul16(a>>16,b)+(mul16(a,b)>>16);}
static uint32_t imul16(uint32_t a,uint32_t b){return (uint32_t)((int32_t)(int16_t)a*(int32_t)(int16_t)b);}
static int neg32(uint32_t v){return (v>>31)!=0;}
static uint32_t sar(uint32_t v,unsigned n){return (uint32_t)((int32_t)v>>n);}
static uint32_t abs32(uint32_t v){return (v&0x80000000u)&&v!=0x80000000u?0u-v:v;}
/* GPU divide unit (TRM Rev.8 p.45): 32/32 unsigned, non-restoring, one
 * quotient bit per step; in offset mode the dividend is treated as 16.16.
 * The remainder register keeps the final partial remainder.  This exact
 * algorithm (including divide-by-zero and overflow results) is checked
 * against the TRM JRISC model by tests/test_view3d_port_s25.c. */
uint32_t avp_view3d_div(uint32_t dividend,uint32_t divisor,int offset_mode,uint32_t *remainder)
{
    uint64_t num=offset_mode?((uint64_t)dividend<<16):(uint64_t)dividend;
    int64_t r=(int64_t)(num>>32);
    uint32_t low=(uint32_t)num,q=0;
    for(int i=31;i>=0;i--){
        int64_t bit=(low>>i)&1u;
        if(r>=0)r=r*2+bit-(int64_t)divisor;
        else r=r*2+bit+(int64_t)divisor;
        q=(q<<1)|(r>=0?1u:0u);
    }
    if(remainder)*remainder=(uint32_t)r;
    return q;
}
static uint32_t gpu_div(AvpView3DState *s,uint32_t dividend,uint32_t divisor)
{return avp_view3d_div(dividend,divisor,s->divoff,&s->remain);}

/* Gouraud intensity increment from a perspective scale value ($F033DE in
 * stage 0, $F0353C in stage 1): B_IINC = ((((v>>7)... - $4000)>>8)*-16<<8
 * + $4000) << 8 >> 8, written as the original computes it. */
static uint32_t shade_iinc(uint32_t v)
{
    uint32_t r1=v-0x4000u;
    r1>>=8;
    r1=imul16(0xFFFFFFF0u,r1);
    r1<<=8;r1+=0x4000u;r1<<=8;r1>>=8;
    return r1;
}

/* Original overlay loader ($F0315C) register effects.  The port has no GPU
 * local RAM to copy code into, so the copy itself is not performed, but the
 * Blitter registers the loader programs are left exactly as the original
 * leaves them, because later blits inherit any register a stage does not
 * set. */
static void overlay_loader(AvpView3DState *s,const AvpView3DBus *bus,uint32_t table,uint32_t longs)
{
    uint32_t *R=s->r,*ALT=s->alt;
    R[1]=RD32(table);R[4]=longs;
    blit_wait(s,bus);
    R[2]=0x28u;R[3]=0xF0B220u;R[5]=0u;
    WR32(ALT[14],R[3]);WR32(ALT[14]+4u,R[2]);WR32(ALT[14]+12u,R[5]);
    WR32(ALT[15],R[1]);WR32(ALT[15]+4u,R[2]);WR32(ALT[15]+12u,R[5]);
    R[4]|=0x10000u;
    WR32(ALT[11],R[4]);
    R[0]=ALT[10];R[1]=0x1800001u;
    /* (B_CMD write of the code copy omitted: see above) */
}

/* ---- Stage 0 ----------------------------------------------------------- */

/* $F03220: read the camera from the 68000 parameter block and publish it to
 * the secondary bank for all later stages. */
void avp_view3d_setup(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt;
    R[0]=0x7FBCu;
    R[2]=RD32(0x7FBCu);           /* destination framebuffer */
    ALT[6]=R[2];
    R[1]=RD32(0x7FB8u);           /* view angle */
    R[5]=RD32(0x7FB4u);           /* camera Y (16.16 maze cells) */
    R[4]=RD32(0x7FB0u);           /* camera X */
    R[0]=0x7FB0u;
    ALT[8]=R[1];
    /* address of the camera's 8-byte maze cell */
    R[0]=mul16(R[5]>>16,ALT[4]);  /* alt r4 = maze width */
    R[1]=R[4]>>16;
    R[0]+=R[1];R[0]<<=3;
    R[1]=ALT[18];                 /* alt r18 = maze base */
    R[0]+=R[1];
    ALT[24]=R[0];
    ALT[22]=R[4];ALT[23]=R[5];
    blit_wait(s,bus);
    R[0]=ALT[15];
    WR32(R[0],R[2]);              /* A2_BASE = framebuffer */
}

/* $F0367C: transform the 68000's object list into view space.
 * Input records are 48 bytes at [$7F9C], 300 of them; output records are
 * written as three longwords at +8/+12/+16 with a 20-byte stride starting
 * at [$7FA0].  [$7FA4] is the base of the image-set table; [$7FA8]/[$7FAC]
 * are sin/cos of the view angle (1.15). */
void avp_view3d_transform_objects(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt;
    int32_t acc;
    R[0]=0x7FACu;R[9]=RD32(0x7FACu);
    R[10]=RD32(0x7FA8u);
    R[15]=RD32(0x7FA4u);
    R[17]=RD32(0x7FA0u);
    R[0]=0x7F9Cu;R[16]=RD32(0x7F9Cu);
    R[23]=0xF0377Eu;R[22]=0xF036BCu;     /* (original branch registers) */
    R[6]=ALT[22];R[7]=ALT[23];           /* camera X, Y */
    R[11]=0x10u;R[12]=0x800u;R[13]=0x80u;R[14]=300u;
    for(;;){
        R[16]+=8u;
        R[8]=0u;
        R[2]=RD32(R[16]);                /* +8: object type / active */
        R[18]=0u;
        R[16]-=8u;
        if(R[2]==0u)goto store;
        R[5]=RD32(R[16]);                /* +0: X */
        R[4]=RD32(R[16]+4u);             /* +4: Y */
        R[3]=RD16(R[16]+14u);            /* +14: image set index (1-based) */
        R[18]=RD16(R[16]+42u);
        R[19]=RD16(R[16]+44u);
        R[5]-=R[6];R[4]-=R[7];
        R[5]=sar(R[5],8);R[4]=sar(R[4],8);
        /* rotate into view space */
        acc=(int32_t)imul16(R[4],R[9]);
        acc=(int32_t)((uint32_t)acc+imul16(R[5],R[10]));
        R[2]=(uint32_t)acc;
        R[10]=0u-R[10];
        acc=(int32_t)imul16(R[5],R[9]);
        acc=(int32_t)((uint32_t)acc+imul16(R[4],R[10]));
        R[5]=(uint32_t)acc;
        R[4]=R[2];
        R[10]=0u-R[10];
        R[4]=sar(R[4],15);
        R[5]=sar(R[5],15);               /* depth */
        if(R[3]==0u)goto store;
        if(neg32(R[5]))goto store;
        if(neg32(R[5]-R[11]))goto store; /* nearer than 16 */
        R[2]=R[4];
        { uint32_t d=R[5]-R[12]; if(!neg32(d)&&d!=0u)goto store; } /* beyond $800 */
        R[4]=abs32(R[4]);
        R[5]+=R[13];
        { int outside=neg32(R[5]-R[4]);  /* |x| > depth+128: outside the view cone */
          R[4]=R[2];
          R[5]-=R[13];
          if(outside)goto store; }
        R[8]=RD16(R[16]+12u);            /* +12: image class */
        R[8]<<=2;R[8]+=R[15];
        R[8]=RD32(R[8]);
        R[3]-=1u;R[3]<<=2;
        R[8]+=R[3];
        R[8]=RD32(R[8]);
        if(R[8]==0u)goto store;
        if(neg32(R[8])){
            /* bit 31: directional image set, select 1 of 4 by the object's
             * heading (+18) relative to the view angle */
            R[8]&=~0x80000000u;
            R[0]=RD16(R[16]+18u);
            R[1]=ALT[8];
            R[0]<<=16;R[1]-=R[0];
            R[1]>>=29;R[0]=6u;R[1]+=5u;R[1]&=R[0];R[1]<<=1;
            R[8]+=R[1];
            R[8]=RD32(R[8]);
        }else R[8]&=~0x80000000u;
        R[0]=R[8];
        R[8]&=~0x40000000u;
        R[0]>>=30;
        R[3]=RD16(R[8]);                 /* frame count */
        R[2]=RD16(R[16]+16u);            /* +16: animation frame */
        if(!(R[2]<R[3])){R[2]=0u;WR16(R[16]+16u,R[2]);}
        R[3]=R[8];
        R[2]<<=2;R[2]+=4u;
        R[8]+=R[2];
        R[8]=RD32(R[8]);
        if(R[8]==0u)goto store;
        R[0]<<=31;R[8]+=R[3];R[8]|=R[0];  /* frame image address, bit 31 = mirror */
    store:
        R[17]+=8u;WR32(R[17],R[8]);
        R[5]<<=16;R[4]<<=16;R[5]>>=16;
        R[17]+=4u;R[4]|=R[5];
        WR32(R[17],R[4]);                /* view X : depth */
        R[19]<<=16;
        R[17]+=4u;R[19]|=R[18];
        WR32(R[17],R[19]);
        R[14]-=1u;
        R[17]+=4u;R[16]+=32u;R[16]+=16u;
        if(R[14]==0u)break;
    }
}

/* $F03266: floor and ceiling.  106 screen rows above and below the horizon
 * (rows 227-r and r), each cast through the maze and drawn as one textured,
 * shaded Blitter span per maze cell crossed; uncovered remainder spans are
 * filled with the pattern colour.  Finally a 16-row band is cleared. */
void avp_view3d_floor(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt;
    blit_wait(s,bus);
    R[0]=RD32(AVP_VIEW3D_VAR_FLOOR_TEX);  /* patched MOVEI at $F03270 */
    ALT[2]=R[0];
    R[1]=0x14220u;R[0]=A2_FLAGS;WR32(R[0],R[1]);
    R[1]=0x33820u;R[0]=A1_FLAGS;WR32(R[0],R[1]);
    R[1]=0x78007800u;R[0]=B_PATD;WR32(R[0],R[1]);R[0]+=4u;WR32(R[0],R[1]);
    R[24]=0xF0362Eu;R[25]=0xF035D4u;R[26]=0xF0348Eu;
    R[14]=0u;                           /* row */
    R[7]=0x40000000u;R[8]=0xE3u;
    R[21]=ALT[4]<<23;R[22]=ALT[5]<<23;  /* maze width, height in 9.23 */
    R[0]=0x7FACu;
    R[10]=RD32(0x7FA8u);R[9]=RD32(0x7FACu);
    R[12]=R[10];R[11]=R[9];
    R[10]=abs32(R[10]);R[9]=abs32(R[9]);
row:
    R[2]=R[14];R[1]=R[8];R[2]<<=1;R[4]=R[7];
    R[1]-=R[2];
    R[4]=gpu_div(s,R[4],R[1]);          /* distance of this row */
    R[5]=R[4];R[17]=R[4];
    R[0]=ALT[3];                        /* alt r3 = 160 (half width) */
    R[23]=R[4];
    R[17]=gpu_div(s,R[17],R[0]);
    R[0]=mul32x16(R[17],R[9]);
    R[1]=mul32x16(R[17],R[10]);
    R[2]=R[17]>>16;                     /* (scratch as left by the original) */
    R[2]=mul16(R[2],R[10]);
    R[17]=R[0];R[18]=R[1];
    R[0]=mul32x16(R[4],R[9]);
    R[1]=mul32x16(R[5],R[10]);
    R[2]=mul32x16(R[5],R[9]);
    R[3]=mul32x16(R[4],R[10]);
    R[6]=mul16(R[4]>>16,R[10]);
    if(!neg32(R[12]))R[1]=0u-R[1];
    else{R[18]=0u-R[18];R[3]=0u-R[3];}
    if(!neg32(R[11]))R[2]=0u-R[2];
    else{R[17]=0u-R[17];R[0]=0u-R[0];}
    R[0]+=R[1];R[2]-=R[3];
    R[4]=R[0];R[5]=R[2];
    R[18]<<=1;R[17]<<=1;R[4]<<=1;R[5]<<=1;
    R[4]+=R[18];R[5]+=R[17];
    R[0]=ALT[22];R[1]=ALT[23];R[0]<<=7;R[1]<<=7;
    R[4]+=R[0];R[5]+=R[1];               /* left end of the row in 9.23 maze units */
    R[13]=0u;                            /* screen x */
    R[15]=R[4];R[16]=R[5];R[19]=R[18];R[20]=R[17];
    R[0]=R[21];
    if(neg32(R[18])){R[19]=abs32(R[19]);R[0]-=R[4];R[0]-=1u;R[15]=R[0];}
    R[0]=R[22];
    if(neg32(R[17])){R[20]=abs32(R[20]);R[0]-=R[5];R[0]-=1u;R[16]=R[0];}
    if(!neg32(R[15]-R[21]))goto fill_rest;     /* (delay slot: the next cmp) */
    if(!neg32(R[16]-R[22])){R[0]=R[17];goto fill_rest;}
    R[0]=R[17];
    R[1]=R[18];R[0]>>=16;R[1]>>=16;R[0]<<=16;R[0]|=R[1];
    R[2]=R[18];R[1]=R[17];R[2]<<=16;R[1]<<=16;R[2]>>=16;R[1]|=R[2];
    blit_wait(s,bus);
    R[2]=A1_INC;WR32(R[2],R[0]);R[2]+=4u;WR32(R[2],R[1]);
    R[1]=R[23]>>7;R[2]=0x4000u;
    R[0]=0xFFFFFFF0u;
    R[1]=shade_iinc(R[1]);
    R[0]=B_IINC;WR32(R[0],R[1]);
    /* skip the part of the row that lies outside the maze */
    R[0]=0xFFFFFFFFu;R[1]=0xFFFFFFFFu;
    if(neg32(R[15])){
        R[0]=R[15];
        if(R[19]==0u)goto fill_rest;
        R[0]=~R[0];R[0]=gpu_div(s,R[0],R[19]);
    }
    if(neg32(R[16])){
        R[1]=R[16];
        if(R[20]==0u)goto fill_rest;
        R[1]=~R[1];R[1]=gpu_div(s,R[1],R[20]);
    }
    R[0]+=1u;R[1]+=1u;
    R[3]=0xF034D6u;
    R[2]=R[0];R[2]|=R[1];
    if(R[2]==0u)goto cell;
    if(!neg32(R[1]-R[0]))R[0]=R[1];
    R[2]=0x140u;
    if(!neg32(R[0]-R[2]))goto fill_rest;
    R[13]+=R[0];
    R[0]|=0x10000u;
    R[1]=B_COUNT;WR32(R[1],R[0]);
    R[2]=R[8];R[1]=A2_BASE;R[2]-=R[14];R[1]+=12u;R[2]<<=16;
    WR32(R[1],R[2]);                     /* floor row */
    R[2]=0x10800u;R[1]=B_CMD;WR32(R[1],R[2]);
    blit_wait(s,bus);
    R[1]=B_COUNT;WR32(R[1],R[0]);
    R[2]=R[14];R[1]=A2_BASE;R[2]<<=16;R[1]+=12u;
    WR32(R[1],R[2]);                     /* ceiling row */
    R[2]=0x10800u;R[1]=B_CMD;WR32(R[1],R[2]);
    R[0]&=~0x10000u;
advance:                                 /* $F0348E: step R0 pixels */
    R[2]=R[18]>>16;R[3]=mul16(R[18],R[0]);R[2]=mul16(R[2],R[0]);R[4]+=R[3];R[2]<<=16;R[4]+=R[2];
    R[2]=R[17]>>16;R[3]=mul16(R[17],R[0]);R[2]=mul16(R[2],R[0]);R[5]+=R[3];R[2]<<=16;R[5]+=R[2];
    R[2]=R[19]>>16;R[3]=mul16(R[19],R[0]);R[2]=mul16(R[2],R[0]);R[15]+=R[3];R[2]<<=16;R[15]+=R[2];
    R[2]=R[20]>>16;R[3]=mul16(R[20],R[0]);R[2]=mul16(R[2],R[0]);R[16]+=R[3];R[2]<<=16;R[16]+=R[2];
    if(!neg32(R[15]-R[21]))goto fill_rest;
    if(!neg32(R[16]-R[22])){R[0]=R[5];goto fill_rest;}
    R[0]=R[5];
cell:                                    /* $F034D6: span inside one maze cell */
    R[0]=R[5];R[1]=R[4];R[0]>>=23;R[1]>>=23;
    R[2]=ALT[4];R[0]=mul16(R[2],R[0]);R[0]+=R[1];R[0]<<=3;
    R[2]=ALT[18];R[0]+=R[2];
    R[0]+=4u;R[27]=RD8(R[0]);            /* cell byte 4: ceiling texture */
    R[0]+=1u;R[3]=RD8(R[0]);             /* cell byte 5: floor texture */
    blit_wait(s,bus);
    R[0]=R[8];R[0]-=R[14];R[0]<<=16;
    R[1]=A2_BASE;R[0]|=R[13];R[1]+=12u;
    ALT[25]=R[13];
    WR32(R[1],R[0]);
    R[3]-=1u;R[1]=ALT[2];R[3]<<=15;R[0]=A1_BASE;R[3]+=R[1];
    WR32(R[0],R[3]);                     /* 32 KiB per texture */
    R[1]=R[5];R[2]=R[4];R[1]<<=9;R[2]<<=9;R[1]>>=25;R[2]>>=25;R[1]<<=16;R[2]|=R[1];
    R[0]+=12u;ALT[7]=R[2];
    WR32(R[0],R[2]);                     /* A1_PIXEL: 7-bit texel position in the cell */
    R[1]=R[4];R[2]=R[5];R[1]<<=16;R[2]<<=16;R[1]>>=16;R[2]|=R[1];
    R[0]+=12u;ALT[26]=R[2];
    WR32(R[0],R[2]);                     /* A1_FPIXEL */
    /* pixels until the ray leaves this cell in X and in Y */
    R[3]=R[16];R[6]=R[15];R[3]<<=9;R[6]<<=9;R[6]=~R[6];R[3]=~R[3];R[6]>>=9;R[3]>>=9;
    R[6]=gpu_div(s,R[6],R[19]);
    R[3]=gpu_div(s,R[3],R[20]);
    R[6]&=~0x80000000u;R[3]&=~0x80000000u;
    R[6]+=1u;R[3]+=1u;
    if(!(R[6]<R[3]))R[6]=R[3];
    R[0]=0x140u;
    R[13]+=R[6];
    if(!neg32(R[13]-R[0])){R[13]-=R[0];R[6]-=R[13];R[13]=R[0];}
    R[6]|=0x10000u;
    R[2]=B_COUNT;ALT[19]=R[6];
    WR32(R[2],R[6]);
    R[2]=B_CMD;R[1]=0x41802801u;WR32(R[2],R[1]);   /* floor span */
    blit_wait(s,bus);
    R[2]=ALT[25];R[3]=R[14];R[3]<<=16;R[1]=A2_BASE;R[3]|=R[2];R[1]+=12u;
    WR32(R[1],R[3]);
    R[27]-=1u;R[1]=ALT[2];R[27]<<=15;R[3]=A1_BASE;R[1]+=R[27];
    WR32(R[3],R[1]);
    R[2]=ALT[7];R[3]+=12u;WR32(R[3],R[2]);
    R[2]=ALT[26];R[3]+=12u;WR32(R[3],R[2]);
    R[6]=ALT[19];R[2]=B_COUNT;WR32(R[2],R[6]);
    R[2]=B_CMD;R[1]=0x41802801u;WR32(R[2],R[1]);   /* ceiling span */
    R[6]&=~0x10000u;
    if(R[13]==R[0])goto next_row;
    R[0]=R[6];
    goto advance;
fill_rest:                               /* $F035D4: rest of the row is outside the maze */
    R[2]=0x140u;R[3]=R[13];
    { uint32_t d=R[13]-R[2];
      R[13]=0u-d;
      if(!neg32(d))goto next_row; }
    blit_wait(s,bus);
    R[13]|=0x10000u;
    R[1]=B_COUNT;WR32(R[1],R[13]);
    R[2]=R[8];R[1]=A2_BASE;R[2]-=R[14];R[1]+=12u;R[2]<<=16;R[2]|=R[3];
    WR32(R[1],R[2]);
    R[2]=0x10800u;R[1]=B_CMD;WR32(R[1],R[2]);
    blit_wait(s,bus);
    R[1]=B_COUNT;WR32(R[1],R[13]);
    R[2]=R[14];R[1]=A2_BASE;R[2]<<=16;R[1]+=12u;R[2]|=R[3];
    WR32(R[1],R[2]);
    R[2]=0x10800u;R[1]=B_CMD;WR32(R[1],R[2]);
next_row:                                /* $F0362E */
    R[0]=0x6Au;R[14]+=1u;
    { int more=neg32(R[14]-R[0]); R[0]=0xF032DEu; if(more)goto row; }
    blit_wait(s,bus);
    R[1]=0x4220u;R[0]=A2_FLAGS;WR32(R[0],R[1]);
    R[1]=0x6A0000u;R[0]+=8u;WR32(R[0],R[1]);
    R[1]=0x100140u;R[0]=B_COUNT;WR32(R[0],R[1]);
    R[1]=0x10800u;R[0]=B_CMD;WR32(R[0],R[1]);   /* 16-row band below the view */
}

void avp_view3d_stage0(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt;
    overlay_loader(s,bus,0x7F64u,0x162u);
    blit_wait(s,bus);
    avp_view3d_setup(s,bus);
    avp_view3d_transform_objects(s,bus);
    R[0]=0u-2u;R[1]=ALT[13];
    WR32(R[1],R[0]);                    /* $7F80 = -2: objects transformed */
    avp_view3d_floor(s,bus);
}

/* ---- Stage 2: objects ---------------------------------------------------- */

/* Object list links may point at the list head, which the original keeps in
 * GPU local RAM. */
static uint32_t link_rd(AvpView3DState *s,const AvpView3DBus *bus,uint32_t a)
{ return a==AVP_VIEW3D_SORT_HEAD_ADDR?s->sort_head:RD32(a); }
static void link_wr(AvpView3DState *s,const AvpView3DBus *bus,uint32_t a,uint32_t v)
{ if(a==AVP_VIEW3D_SORT_HEAD_ADDR)s->sort_head=v; else WR32(a,v); }

/* Signed 1.7 sprite extent byte -> screen pixels at scale `scale`, rounded,
 * as the original computes it: |v|/scale, +1, >>1, sign restored. */
static uint32_t scaled_extent(AvpView3DState *s,uint32_t v,uint32_t scale,uint32_t *sign)
{
    uint32_t m=sar(v,31),q;
    q=gpu_div(s,abs32(v),scale);
    q+=1u;q>>=1;
    *sign=m;
    return (q^m)-m;
}
/* (2*a+1)*b/2 with the sign of a: the source offset of pixel centre a */
static uint32_t centre_offset(uint32_t a,uint32_t b)
{
    uint32_t r0=a<<1,r1=b,r2=b;
    r0+=1u;r1>>=16;r0=abs32(r0);
    r1=mul16(r0,r1);r2=mul16(r0,r2);
    r1<<=16;r1+=r2;r1>>=1;
    if(neg32(a))r1=0u-r1;
    return r1;
}

/* $F0332A: draw one object, column by column.  r4 = view x (8.8 signed),
 * r5 = depth<<8, r8 = image, r11 = 0 or -1 (mirrored); alt r25 = y offset. */
static void draw_object(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt,sg;
    ALT[1]=R[5];
    R[1]=R[5];R[17]=R[5];R[0]=ALT[20];
    R[17]>>=16;R[1]=mul16(R[0],R[1]);R[17]=mul16(R[0],R[17]);
    R[1]>>=7;R[17]<<=9;R[17]+=R[1];      /* source step per screen pixel */
    R[18]=R[5];R[18]<<=7;R[0]=0xA0u;
    R[18]=gpu_div(s,R[18],R[0]);         /* projection scale */
    R[4]>>=9;R[4]<<=17;
    R[0]=R[4];R[4]=abs32(R[4]);
    R[4]=gpu_div(s,R[4],R[18]);
    R[4]+=1u;R[4]>>=1;
    R[0]=sar(R[0],31);R[4]^=R[0];R[4]-=R[0];   /* screen x of the centre */
    R[0]=ALT[26];
    R[4]^=R[11];
    if(R[0]!=0u)R[18]=R[5];
    R[1]=R[18];R[0]=ALT[20];R[18]>>=16;
    R[1]=mul16(R[0],R[1]);R[18]=mul16(R[0],R[18]);
    R[1]>>=7;R[18]<<=9;R[18]+=R[1];
    /* image header: signed left and right extents */
    R[14]=RD8(R[8]);R[8]+=1u;R[13]=RD8(R[8]);R[8]-=1u;
    R[14]<<=24;R[13]<<=24;R[14]=sar(R[14],7);R[13]=sar(R[13],7);
    R[6]=sar(R[14],1);
    R[0]=R[14];R[1]=R[13];
    R[14]=abs32(R[14]);R[13]=abs32(R[13]);
    R[14]=gpu_div(s,R[14],R[18]);
    R[13]=gpu_div(s,R[13],R[18]);
    R[14]+=1u;R[13]+=1u;R[14]>>=1;R[13]>>=1;
    R[2]=R[14];R[2]|=R[13];
    R[0]=sar(R[0],31);
    if(R[2]==0u)return;                  /* smaller than a pixel */
    R[14]^=R[0];R[14]-=R[0];
    R[1]=sar(R[1],31);R[13]^=R[1];R[13]-=R[1];
    R[15]=R[14];
    R[0]=ALT[3];                         /* 160 */
    R[13]+=R[0];R[14]+=R[0];
    R[13]+=R[4];R[14]+=R[4];
    { int left_off=neg32(R[14]);
      R[0]<<=1;                          /* 320 */
      if(left_off){R[15]-=R[14];R[14]=0u;} }
    if(!neg32(R[14]-R[0])){R[13]=0u-R[13];return;}   /* right of the view */
    R[13]=0u-R[13];
    if(!neg32(R[13])){R[13]=0u-R[13];return;}         /* left of the view */
    R[13]=0u-R[13];
    if(!neg32(R[0]-R[13])){}else R[13]=R[0];
    R[1]=centre_offset(R[15],R[18]);
    R[0]=abs32((R[15]<<1)+1u);R[2]=mul16(R[0],R[18]);
    R[15]=R[1];R[15]-=R[6];              /* source x of the first column */
    R[1]=ALT[3];R[1]<<=1;
    R[14]^=R[11];R[13]^=R[11];R[1]&=R[11];
    R[14]+=R[1];R[13]+=R[1];
    R[0]=R[14];R[16]=0x4000u;R[0]<<=3;R[16]+=R[0];  /* wall table of column R14 */
    R[5]>>=4;
    R[11]<<=1;R[11]+=1u;                 /* +1 / -1 column step */
    R[12]=R[11];R[12]<<=3;
    blit_wait(s,bus);
    R[1]=0u;R[0]=B_PATD;WR32(R[0],R[1]);R[0]+=4u;WR32(R[0],R[1]);
    R[1]=ALT[1];R[2]=0x4000u;R[0]=0xFFFFFFF0u;
    R[1]=shade_iinc(R[1]);
    R[0]=B_IINC;WR32(R[0],R[1]);
    R[1]=R[17];R[0]=A1_FINC;R[1]<<=16;R[1]>>=16;WR32(R[0],R[1]);
    R[1]=R[17];R[0]-=4u;R[1]>>=16;WR32(R[0],R[1]);   /* A1_INC */
column:                                  /* $F03470 */
    R[22]=R[16];
    R[2]=0x8E720000u;                    /* default clip: rows -114..114 */
    R[1]=RD32(R[22]);R[0]=R[1];R[1]>>=16;R[0]<<=16;
    if(neg32(R[5]-R[1]))goto draw_column;   /* in front of the wall */
    R[2]=R[0];
    R[22]+=4u;
    if(R[0]==0u)goto next_column;
    R[22]=RD32(R[22]);
    for(;;){                             /* $F0348E: walk the column's span list */
        R[1]=RD32(R[22]);R[0]=R[1];R[1]>>=16;R[0]<<=16;
        if(neg32(R[5]-R[1]))goto draw_column;
        R[2]=R[0];
        if(R[0]==0u)goto next_column;
        R[22]+=4u;
    }
draw_column:                             /* $F034A4 */
    R[0]=R[15];R[0]>>=16;R[0]<<=1;R[0]+=R[8];R[0]+=2u;
    R[0]=RD16(R[0]);                     /* column offset in the image */
    { int empty=R[0]==0u; R[0]+=R[8]; if(empty)goto next_column; }
    R[20]=ALT[25];R[1]=ALT[21];R[1]+=R[20];
    R[20]=RD8(R[0]);R[0]+=1u;R[1]<<=17;
    R[21]=RD8(R[0]);R[0]+=1u;
    ALT[19]=R[0];                        /* column pixel data */
    R[20]<<=24;R[21]<<=24;R[20]=sar(R[20],7);R[21]=sar(R[21],7);
    R[20]+=R[1];R[21]+=R[1];
    R[6]=sar(R[20],1);
    R[20]=scaled_extent(s,R[20],R[17],&sg);R[0]=sg;
    R[21]=scaled_extent(s,R[21],R[17],&sg);R[1]=sg;
    R[27]=R[2];R[26]=R[2];R[27]<<=8;R[26]=sar(R[26],24);R[27]=sar(R[27],24);
    if(neg32(R[20]-R[26]))R[20]=R[26];   /* clip to the wall's top */
    if(neg32(R[27]-R[21]))R[21]=R[27];   /* ... and bottom */
    { uint32_t d=R[20]-R[21]; R[0]=0u-d; if(!neg32(d))goto next_column; }
    blit_wait(s,bus);
    R[1]=B_COUNT;R[0]|=0x10000u;WR32(R[1],R[0]);
    R[1]=ALT[16];R[0]=A2_PIXEL;R[1]+=R[20];R[1]<<=16;R[1]|=R[14];
    WR32(R[0],R[1]);
    R[1]=ALT[19];R[0]=A1_BASE;WR32(R[0],R[1]);
    R[1]=centre_offset(R[20],R[17]);
    R[0]=abs32((R[20]<<1)+1u);R[2]=mul16(R[0],R[17]);
    R[1]-=R[6];
    R[2]=ALT[19];R[0]=A1_FPIXEL;R[2]<<=29;R[2]>>=30;
    WR32(R[0],R[1]);
    R[1]>>=16;R[0]-=12u;R[1]+=R[2];
    WR32(R[0],R[1]);                     /* A1_PIXEL */
    R[0]=B_CMD;R[1]=0x49800801u;WR32(R[0],R[1]);
    s->object_columns++;
next_column:                             /* $F0357C */
    R[14]+=R[11];R[15]+=R[18];
    { int more=R[14]!=R[13]; R[1]=0xF03470u; R[16]+=R[12]; if(more)goto column; }
}

void avp_view3d_stage2(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt;
    overlay_loader(s,bus,0x7F6Cu,0xDEu);
    blit_wait(s,bus);
    R[2]=ALT[6];R[0]=ALT[15];WR32(R[0],R[2]);      /* A2_BASE = framebuffer */
    /* $F03220 */
    R[0]=RD32(AVP_VIEW3D_VAR_SPRITE_PARM);       /* patched MOVEI */
    R[2]=RD16(R[0]);R[0]+=2u;R[3]=RD16(R[0]);
    blit_wait(s,bus);
    R[1]=0x64220u;R[0]=A2_FLAGS;WR32(R[0],R[1]);
    R[1]=0x73820u;R[0]=A1_FLAGS;WR32(R[0],R[1]);
    ALT[26]=R[3];
    R[0]=0x4000u;R[1]=0x2000u;
    R[0]=gpu_div(s,R[0],R[2]);
    R[3]=0x40u;
    R[1]=gpu_div(s,R[1],R[2]);
    ALT[20]=R[0];R[1]-=R[3];ALT[21]=R[1];
    R[9]=0xF0328Au;R[25]=0xF032C2u;
    R[0]=0x7FA0u;R[10]=RD32(R[0]);
    R[16]=300u;R[1]=0u;R[12]=AVP_VIEW3D_SORT_HEAD_ADDR;
    link_wr(s,bus,R[12],R[1]);
    /* insertion sort, farthest first */
    do{
        R[10]+=8u;R[8]=RD32(R[10]);
        R[10]+=6u;R[5]=RD16(R[10]);      /* depth */
        R[10]-=14u;
        if(R[8]!=0u){
            R[14]=link_rd(s,bus,R[12]);R[13]=R[12];
            while(R[14]!=0u){
                R[14]+=14u;R[0]=RD16(R[14]);R[14]-=14u;
                if(!(R[5]<R[0])){        /* insert before the first nearer-or-equal */
                    R[14]+=4u;WR32(R[14],R[10]);R[14]-=4u;
                    break;
                }
                R[13]=R[14];R[14]=RD32(R[14]);
            }
            link_wr(s,bus,R[13],R[10]);
            WR32(R[10],R[14]);
            R[10]+=4u;WR32(R[10],R[13]);R[10]-=4u;
        }
        R[16]-=1u;
        R[10]+=20u;
    }while(R[16]!=0u);
    R[7]=0xF03324u;R[9]=0xF032E8u;R[24]=0xF0357Cu;R[25]=0xF034A4u;R[23]=0xF0358Cu;
    R[10]=link_rd(s,bus,R[12]);
    for(;;){                             /* $F032E8: draw in list order */
        int end=R[10]==0u;
        R[10]+=8u;
        if(end)break;
        R[8]=RD32(R[10]);R[10]+=4u;
        R[4]=RD16(R[10]);R[10]+=2u;      /* view x */
        R[5]=RD16(R[10]);R[10]+=2u;      /* depth */
        R[4]<<=16;R[5]<<=8;R[4]=sar(R[4],8);
        R[0]=RD16(R[10]);R[10]-=16u;     /* y offset */
        R[11]=R[8];R[0]<<=16;R[8]&=~0x80000000u;R[0]=sar(R[0],16);
        R[11]=sar(R[11],31);
        ALT[25]=R[0];
        s->objects_drawn++;
        draw_object(s,bus);
        R[10]=RD32(R[10]);
    }
}

/* ---- Stage 1: walls ------------------------------------------------------ */

static uint32_t ror16(uint32_t v){return (v>>16)|(v<<16);}
static uint32_t sext8hi(uint32_t b){return sar(b<<24,24);}

/* Blitter wait variant that uses r1/r2 ($F03702, $F03A4A, $F03C1A ...). */
static void blit_wait_r1(AvpView3DState *s,const AvpView3DBus *bus)
{
    s->r[1]=s->alt[10];
    do s->r[2]=bus->rd(bus->ctx,s->r[1]&0xFFFFFFu,4); while(!(s->r[2]&1u));
}

/* $F035E2 / $F03774: destination row/column and texture position of a wall
 * span starting at screen row R20 (relative to the horizon) in column R14. */
static void span_position(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt;
    R[1]=ALT[16];R[0]=A2_BASE;R[1]+=R[20];R[0]+=12u;R[1]<<=16;R[1]|=R[14];
    WR32(R[0],R[1]);                       /* A2_PIXEL */
    R[0]=R[20];R[0]<<=1;R[0]+=1u;R[1]=R[0];R[0]=abs32(R[0]);
    R[3]=R[17];R[2]=R[17];R[3]>>=16;
    R[2]=mul16(R[0],R[2]);R[3]=mul16(R[0],R[3]);
    R[2]>>=1;R[3]<<=15;R[2]+=R[3];
    R[1]=sar(R[1],31);R[3]=ALT[17];       /* alt r17 = $400000: texture row 64.0 */
    R[2]^=R[1];R[2]-=R[1];R[3]+=R[2];
    R[0]=A1_BASE;R[3]=ror16(R[3]);R[0]+=24u;
    WR32(R[0],R[3]);                       /* A1_FPIXEL */
    R[3]<<=16;R[0]-=12u;R[3]|=R[15];
    WR32(R[0],R[3]);                       /* A1_PIXEL: texture column R15, row */
}
/* $F035DC: draw R0 rows of the current wall texture from row R20. */
static void wall_span(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt;
    R[1]=B_COUNT;R[0]|=0x10000u;WR32(R[1],R[0]);
    span_position(s,bus);
    R[0]=B_CMD;R[1]=0x41802801u;WR32(R[0],R[1]);
}

/* Clip the column's vertical edges (R20 top ... R21 bottom, with the inner
 * edges between) to the window still open in this column, R18..R19.  The
 * original clips each chain from the outside in and stops at the first edge
 * already inside. */
static void clip_window(uint32_t *R,int n,const int *edge)
{
    for(int i=0;i<n;i++){ if(!neg32(R[edge[i]]-R[18]))break; R[edge[i]]=R[18]; }
    for(int i=n-1;i>=0;i--){ if(!neg32(R[19]-R[edge[i]]))break; R[edge[i]]=R[19]; }
}
static const int edges2[]={20,21},edges3[]={20,26,21},edges4[]={20,26,27,21};

/* Signed wall-profile bytes R26/R27 (in texels from the centre) to screen
 * rows at the current distance; $F0367E-$F036A0 and $F03B78-$F03B9A. */
static void profile_rows(AvpView3DState *s)
{
    uint32_t *R=s->r;
    R[0]=R[26];R[1]=R[27];
    R[26]=abs32(R[26]);R[27]=abs32(R[27]);
    R[26]<<=17;R[27]<<=17;
    R[26]=gpu_div(s,R[26],R[17]);
    R[27]=gpu_div(s,R[27],R[17]);
    R[26]+=1u;R[27]+=1u;R[26]>>=1;R[27]>>=1;
    R[0]=sar(R[0],31);R[26]^=R[0];R[26]-=R[0];
    R[1]=sar(R[1],31);R[27]^=R[1];R[27]-=R[1];
}

/* $F03712 etc.: the state byte (6 or 7) of the door cell this face belongs
 * to.  R21 points at the hit face byte (0..3) of a cell; faces 2/3 belong to
 * the neighbouring cell to the east / south. */
static void door_state(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt;
    R[0]=R[21];R[21]&=~1u;R[0]<<=30;R[21]&=~2u;R[0]>>=30;R[21]+=6u;
    R[1]=R[0];R[0]&=~2u;R[1]&=~1u;
    R[21]+=R[0];
    if(R[1]!=0u){
        R[2]=8u;
        if(R[0]!=0u){R[0]=ALT[4];R[2]=mul16(R[0],R[2]);}
        R[21]+=R[2];
    }
    R[21]=RD8(R[21]);
}

/* Handler addresses of the original wall-type table at $F03CA0 (indexed by
 * wall byte bits 6-3).  Kept so that R0 holds what the original holds. */
static const uint32_t wall_handler[16]={
    0xF03594u,0xF03594u,0xF03594u,0xF03646u,0xF03712u,0xF038AEu,0xF0384Eu,0xF03902u,
    0xF0399Cu,0xF03A60u,0xF03A60u,0xF03AE6u,0xF03C98u,0xF03630u,0xF03646u,0xF03646u};

/* $F03508: draw the wall face hit by this column's ray.
 *   R0  = position along the face (fraction in bits 31-25), R1 = distance,
 *   R3  = wall byte (bit 7 mirror, bits 6-3 type, bits 2-0 texture),
 *   R21 = address of the hit face byte, R18/R19 = open window.
 * On "finish" the window left open (R26..R27, e.g. a hole) becomes R18/R19,
 * or 0/0 when the column is closed. */
static void wall_hit(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt;
    int door_draw=0;
    R[2]=0xF035DCu;ALT[1]=R[2];R[2]=0xF03C8Cu;ALT[2]=R[2];
    R[17]=R[1];ALT[19]=R[1];R[17]<<=1;R[17]&=~1u;   /* distance (texture step) */
    R[26]=R[18];
    { int zero=R[17]==0u; R[27]=R[19]; if(zero)goto finish; }
    { int mirror=(R[3]&0x80u)!=0u; R[3]&=~0x80u; if(mirror)R[0]=~R[0]; }
    R[0]>>=25;R[15]=R[0];                   /* texture column 0..127 */
    blit_wait(s,bus);
    R[1]=R[17];R[2]=0x4000u;R[0]=0xFFFFFFF0u;
    R[1]=shade_iinc(R[1]);
    R[0]=B_IINC;WR32(R[0],R[1]);
    R[0]=A1_BASE;R[1]=R[17];R[0]+=32u;R[1]<<=16;R[2]=R[17];
    WR32(R[0],R[1]);                        /* A1_FINC */
    R[2]>>=16;R[0]-=4u;R[2]<<=16;
    WR32(R[0],R[2]);                        /* A1_INC */
    R[20]=ALT[17];R[20]<<=1;
    R[20]=gpu_div(s,R[20],R[17]);           /* wall height in rows */
    R[0]=R[3];R[2]=R[3];R[1]=7u;R[0]>>=3;R[3]&=R[1];R[0]<<=2;R[1]=0xF03CA0u;R[0]+=R[1];
    { unsigned type=(R[2]>>3)&15u;
      R[0]=wall_handler[type];
      s->wall_type_hits[type]++;
      switch(type){
      case 0:case 1:case 2: goto plain;
      case 3:case 14:case 15: goto shaped;
      case 4: goto door4;
      case 5: goto slide5;
      case 6: goto slide6;
      case 7: goto rise7;
      case 8: goto split8;
      case 9:case 10: goto door9;
      case 11: goto door11;
      case 12: return;
      default: goto grille13;
      } }

plain:                                      /* $F03594 */
    R[2]-=1u;R[2]<<=27;R[2]>>=12;           /* (texture-1) * 32 KiB */
    R[3]=RD32(AVP_VIEW3D_VAR_WALL_TEX);
    R[3]+=R[2];
plain_base:                                 /* $F035A2 */
    R[26]=0u;R[27]=0u;
    R[0]=A1_BASE;WR32(R[0],R[3]);
    R[20]+=1u;R[20]>>=1;R[21]=R[20];R[20]=0u-R[20];
    clip_window(R,2,edges2);
    R[0]=R[20];R[28]=ALT[2];
    { uint32_t d=R[0]-R[21]; R[0]=0u-d; if(!neg32(d))goto finish; }
    wall_span(s,bus);
    goto finish;

grille13:                                   /* $F03630 */
    { int skip=(R[15]&0x40u)!=0u; R[0]=0xF03C98u; if(skip)return; }
    R[0]=RD8(R[21]);R[0]<<=24;R[0]=sar(R[0],31);R[0]>>=25;
    R[15]^=R[0];
shaped:                                     /* $F03646 */
    R[2]=RD32(AVP_VIEW3D_VAR_SHAPED_HT);
    R[1]=RD32(AVP_VIEW3D_VAR_SHAPED_TEX);
    R[0]=R[3];R[3]<<=15;R[0]<<=8;R[3]+=R[1];R[2]+=R[0];
shaped_profile:                             /* $F0365C */
    door_draw=0;
shaped_rows:                                /* $F03662 */
    R[1]=R[15];R[1]<<=1;R[2]+=R[1];
    R[26]=RD8(R[2]);R[2]+=1u;R[27]=RD8(R[2]);   /* this column's profile */
    R[20]+=1u;R[20]>>=1;R[21]=R[20];R[20]=0u-R[20];
    R[26]<<=24;R[27]<<=24;R[26]=sar(R[26],24);R[27]=sar(R[27],24);
    profile_rows(s);
    R[0]=A1_BASE;WR32(R[0],R[3]);
    clip_window(R,4,edges4);
    R[0]=R[20];
    if(door_draw)goto door4_draw;
    R[1]=ALT[1];
    { uint32_t d=R[0]-R[26]; R[0]=0u-d; if(neg32(d))wall_span(s,bus); }   /* upper part */
    R[28]=ALT[2];R[0]=R[27];
    { uint32_t d=R[0]-R[21]; R[0]=0u-d; if(!neg32(d))goto finish; }
    blit_wait_r1(s,bus);
    R[1]=ALT[1];R[20]=R[27];
    wall_span(s,bus);                       /* lower part */
    goto finish;

door4:                                      /* $F03712 */
    door_state(s,bus);
    R[2]=RD32(AVP_VIEW3D_VAR_DOOR4_HT);
    R[1]=RD32(AVP_VIEW3D_VAR_DOOR4_TEX);
    R[0]=R[3];R[3]<<=15;R[0]<<=8;R[3]+=R[1];R[2]+=R[0];
    R[1]=R[15];R[0]=R[21];
    if(R[1]&0x40u)R[0]=0u-R[0];
    ALT[0]=R[0];R[0]+=R[1];R[1]^=R[0];R[0]=0xF0365Cu;
    if(R[1]&0x40u)goto shaped_profile;
    R[0]+=6u;R[1]=ALT[16];
    door_draw=1;
    goto shaped_rows;
door4_draw:                                 /* $F03774 */
    span_position(s,bus);
    R[0]=R[20];R[1]=0xF037D4u;
    { uint32_t d=R[0]-R[26]; R[0]=0u-d;
      if(neg32(d)){R[1]=B_COUNT;R[0]|=0x10000u;WR32(R[1],R[0]);
                   R[0]=B_CMD;R[1]=0x41802801u;WR32(R[0],R[1]);} }
    R[3]=0u;R[0]=R[26];R[1]=0xF0380Cu;
    { uint32_t d=R[0]-R[27]; R[0]=0u-d;
      if(neg32(d)){
          blit_wait(s,bus);
          R[3]=ALT[0];R[2]=A1_BASE;R[2]+=4u;R[1]=RD32(R[2]);   /* A1 pointer readback */
          R[2]+=8u;R[1]+=R[3];WR32(R[2],R[1]);
          R[1]=B_COUNT;R[0]|=0x10000u;WR32(R[1],R[0]);
          R[0]=B_CMD;R[1]=0x41802801u;WR32(R[0],R[1]);} }
    R[0]=R[27];R[1]=0xF03840u;
    { uint32_t d=R[0]-R[21]; R[0]=0u-d;
      if(neg32(d)){
          blit_wait(s,bus);
          R[2]=A1_BASE;R[2]+=4u;R[1]=RD32(R[2]);
          R[2]+=8u;R[1]-=R[3];WR32(R[2],R[1]);
          R[1]=B_COUNT;R[0]|=0x10000u;WR32(R[1],R[0]);
          R[0]=B_CMD;R[1]=0x41802801u;WR32(R[0],R[1]);} }
    R[26]=0u;R[27]=0u;R[0]=0xF03C8Cu;
    goto finish;

slide6:                                     /* $F0384E */
    door_state(s,bus);
    R[2]=RD32(AVP_VIEW3D_VAR_SLIDE6_HT);
    R[1]=RD32(AVP_VIEW3D_VAR_SLIDE6_TEX);
    R[0]=R[3];R[3]<<=15;R[0]<<=8;R[3]+=R[1];R[2]+=R[0];
    R[1]=R[15];R[0]=R[21];
    if(R[15]&0x40u)R[0]=0u-R[0];
    R[15]+=R[0];R[1]^=R[15];R[0]=0xF03C98u;
    if(R[1]&0x40u)return;                  /* slid out of this column */
    R[0]=0xF0365Cu;
    goto shaped_profile;

slide5:                                     /* $F038AE */
    door_state(s,bus);
    R[1]=RD32(AVP_VIEW3D_VAR_SLIDE5_TEX);
    R[3]<<=15;R[3]+=R[1];
    R[1]=R[15];R[0]=R[21];
    if(R[15]&0x40u)R[0]=0u-R[0];
    R[15]+=R[0];R[1]^=R[15];R[0]=0xF03C98u;
    if(R[1]&0x40u)return;
    R[0]=0xF035A2u;
    goto plain_base;

rise7:                                      /* $F03902 */
    door_state(s,bus);
    R[1]=RD32(AVP_VIEW3D_VAR_RISE7_TEX);
    R[3]<<=15;R[3]+=R[1];
    R[0]=R[21];R[21]<<=8;R[3]+=R[21];
    R[20]+=1u;R[20]>>=1;R[21]=R[20];R[20]=0u-R[20];
    R[0]<<=16;R[26]=ALT[17];R[26]-=R[0];
    R[0]=R[26];R[26]<<=1;R[26]=abs32(R[26]);
    R[26]=gpu_div(s,R[26],R[17]);
    R[26]+=1u;R[26]>>=1;R[0]=sar(R[0],31);R[26]^=R[0];R[26]-=R[0];
    R[0]=A1_BASE;WR32(R[0],R[3]);
    clip_window(R,3,edges3);
    R[27]=R[21];R[0]=R[20];R[1]=ALT[1];
    { uint32_t d=R[0]-R[26]; R[28]=ALT[2]; R[0]=0u-d; if(neg32(d))wall_span(s,bus); }
    goto finish;

split8:                                     /* $F0399C */
    door_state(s,bus);
    R[1]=RD32(AVP_VIEW3D_VAR_SPLIT8_TEX);
    R[3]<<=15;R[3]+=R[1];
    R[0]=R[21];R[21]<<=8;R[3]+=R[21];
    R[1]=A1_BASE;R[21]<<=1;
    WR32(R[1],R[3]);
    R[3]-=R[21];ALT[0]=R[3];
    R[20]+=1u;R[20]>>=1;R[21]=R[20];R[20]=0u-R[20];
    R[26]=R[0];R[26]<<=17;
    R[26]=gpu_div(s,R[26],R[17]);
    R[26]+=1u;R[26]>>=1;R[27]=R[26];R[26]=0u-R[26];
    clip_window(R,4,edges4);
    R[0]=R[20];R[1]=ALT[1];
    { uint32_t d=R[0]-R[26]; R[0]=0u-d; if(neg32(d))wall_span(s,bus); }
    R[28]=ALT[2];R[0]=R[27];
    { uint32_t d=R[0]-R[21]; R[0]=0u-d; if(!neg32(d))goto finish; }
    blit_wait_r1(s,bus);
    R[1]=A1_BASE;R[3]=ALT[0];WR32(R[1],R[3]);
    R[1]=ALT[1];R[20]=R[27];
    wall_span(s,bus);
    goto finish;

door9:                                      /* $F03A60 */
    ALT[20]=R[10];ALT[21]=R[11];
    door_state(s,bus);
    R[2]=RD32(AVP_VIEW3D_VAR_DOOR9_HT);
    R[1]=RD32(AVP_VIEW3D_VAR_DOOR9_TEX);
    R[0]=R[3];R[3]<<=15;R[0]<<=8;R[3]+=R[1];R[2]+=R[0];
    R[0]=R[21];R[1]=A1_BASE;ALT[9]=R[3];WR32(R[1],R[3]);
    R[1]=R[15];R[1]<<=1;R[2]+=R[1];
    R[26]=RD8(R[2]);R[2]+=1u;R[27]=RD8(R[2]);
    R[20]+=1u;R[20]>>=1;R[21]=R[20];R[20]=0u-R[20];
    R[26]=sext8hi(R[26]);R[27]=sext8hi(R[27]);
    R[10]=R[27];R[10]-=R[0];R[0]<<=8;ALT[0]=R[0];
    R[1]=R[10];R[10]=abs32(R[10]);R[10]<<=17;
    R[10]=gpu_div(s,R[10],R[17]);
    R[10]+=1u;R[10]>>=1;R[1]=sar(R[1],31);R[10]^=R[1];R[10]-=R[1];
    R[1]=0xF03B78u;R[11]=R[21];
    goto door_common;

door11:                                     /* $F03AE6 */
    ALT[20]=R[10];ALT[21]=R[11];
    door_state(s,bus);
    R[2]=RD32(AVP_VIEW3D_VAR_DOOR11_HT);
    R[1]=RD32(AVP_VIEW3D_VAR_DOOR11_TEX);
    R[0]=R[3];R[3]<<=15;R[0]<<=8;R[3]+=R[1];R[2]+=R[0];
    R[0]=R[21];R[1]=A1_BASE;ALT[9]=R[3];WR32(R[1],R[3]);
    R[1]=R[15];R[1]<<=1;R[2]+=R[1];
    R[26]=RD8(R[2]);R[2]+=1u;R[27]=RD8(R[2]);
    R[20]+=1u;R[20]>>=1;R[21]=R[20];R[20]=0u-R[20];
    R[26]=sext8hi(R[26]);R[27]=sext8hi(R[27]);
    R[10]=12u;R[11]=12u;R[10]-=R[0];R[11]+=R[0];R[0]<<=8;ALT[0]=R[0];
    R[0]=R[10];R[1]=R[11];R[10]=abs32(R[10]);R[11]=abs32(R[11]);
    R[10]<<=17;R[11]<<=17;
    R[10]=gpu_div(s,R[10],R[17]);
    R[11]=gpu_div(s,R[11],R[17]);
    R[10]+=1u;R[11]+=1u;R[10]>>=1;R[11]>>=1;
    R[0]=sar(R[0],31);R[10]^=R[0];R[10]-=R[0];
    R[1]=sar(R[1],31);R[11]^=R[1];R[11]-=R[1];
door_common:                                /* $F03B78 */
    profile_rows(s);
    clip_window(R,4,edges4);
    if(neg32(R[10]-R[26])){R[10]=R[26];if(neg32(R[11]-R[26]))R[11]=R[26];}
    if(neg32(R[27]-R[11])){R[11]=R[27];if(neg32(R[27]-R[10]))R[10]=R[27];}
    R[0]=R[20];R[1]=ALT[1];
    { uint32_t d=R[0]-R[26]; R[0]=0u-d; if(neg32(d))wall_span(s,bus); }
    R[28]=0xF03C34u;R[0]=R[26];
    { uint32_t d=R[0]-R[10]; R[0]=0u-d;
      if(neg32(d)){blit_wait_r1(s,bus);
          R[3]=ALT[9];R[2]=ALT[0];R[1]=A1_BASE;R[3]+=R[2];WR32(R[1],R[3]);
          R[1]=ALT[1];R[20]=R[26];wall_span(s,bus);} }
    R[0]=R[11];R[28]=0xF03C5Cu;
    { uint32_t d=R[0]-R[27]; R[0]=0u-d;
      if(neg32(d)){blit_wait_r1(s,bus);
          R[3]=ALT[9];R[2]=ALT[0];R[1]=A1_BASE;R[3]-=R[2];WR32(R[1],R[3]);
          R[1]=ALT[1];R[20]=R[11];wall_span(s,bus);} }
    R[0]=R[27];R[28]=0xF03C80u;
    { uint32_t d=R[0]-R[21]; R[0]=0u-d;
      if(neg32(d)){blit_wait_r1(s,bus);
          R[1]=A1_BASE;R[3]=ALT[9];WR32(R[1],R[3]);
          R[1]=ALT[1];R[20]=R[27];wall_span(s,bus);} }
    R[26]=R[10];R[27]=R[11];R[0]=ALT[2];R[10]=ALT[20];R[11]=ALT[21];
    goto finish;

finish:                                     /* $F03C8C */
    R[18]=0u;R[19]=0u;
    if(R[27]!=R[26]){R[18]=R[26];R[19]=R[27];}
}

/* $F032D8: cast the ray of screen column R14 at angle R0 through the maze,
 * drawing every wall face it meets until the column is closed or the ray
 * reaches the maximum distance.  R3 = cos correction for this column. */
static void cast_column(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt,t;
    R[4]=ALT[22];R[5]=ALT[23];R[4]<<=16;R[5]<<=16;   /* position within the cell */
    R[8]=ALT[24];R[9]=ALT[24];                       /* camera cell */
    R[13]=0u;ALT[7]=R[3];
    R[1]=R[0];R[0]<<=3;R[0]>>=24;                    /* angle within the octant */
    R[2]=0x100u;R[2]-=R[0];
    R[7]=8u;R[6]=ALT[4];R[7]=0u-R[7];R[6]=imul16(R[7],R[6]);  /* -8 * maze width */
    R[12]=0xF033B8u;R[10]=0xF0331Eu;
    R[1]>>=29;
    /* Octant setup: which two face bytes of a cell the ray can hit (R8, R9),
     * the cell steps R6/R7 along the major and minor axes, and the
     * reflections of the in-cell position R4/R5 and of the face position. */
    switch(R[1]){
    case 0: R[8]+=2u;R[9]+=1u;t=R[6];R[6]=R[7];R[7]=t;R[6]=0u-R[6];R[7]=0u-R[7];break;
    case 1: t=R[4];R[4]=R[5];R[5]=t;R[0]=R[2];R[4]=~R[4];R[5]=~R[5];R[8]+=1u;R[9]+=2u;R[13]=~R[13];break;
    case 2: t=R[4];R[4]=R[5];R[5]=t;R[4]=~R[4];R[8]+=1u;R[7]=0u-R[7];break;
    case 3: R[0]=R[2];R[4]=~R[4];R[9]+=1u;t=R[6];R[6]=R[7];R[7]=t;R[7]=0u-R[7];R[13]=~R[13];break;
    case 4: R[4]=~R[4];R[5]=~R[5];R[9]+=3u;t=R[6];R[6]=R[7];R[7]=t;break;
    case 5: t=R[4];R[4]=R[5];R[5]=t;R[0]=R[2];R[8]+=3u;R[6]=0u-R[6];R[7]=0u-R[7];R[13]=~R[13];break;
    case 6: t=R[4];R[4]=R[5];R[5]=t;R[5]=~R[5];R[8]+=3u;R[9]+=2u;R[6]=0u-R[6];break;
    default:R[0]=R[2];R[5]=~R[5];R[8]+=2u;R[9]+=3u;t=R[6];R[6]=R[7];R[7]=t;R[6]=0u-R[6];R[13]=~R[13];break;
    }
    R[1]=t;
    /* $F033B8: initial window = whole wall height (alt r16 = 114 rows) */
    R[18]=ALT[16];R[19]=ALT[16];R[18]=0u-R[18];
    R[2]=R[19];R[1]=R[18];R[2]<<=24;R[1]<<=24;R[2]>>=8;R[2]|=R[1];R[2]>>=16;
    ALT[31]=R[2];
    R[0]<<=1;R[1]=AVP_VIEW3D_ROM_STEP_A;R[2]=AVP_VIEW3D_ROM_STEP_B;R[1]+=R[0];R[2]+=R[0];
    R[10]=RD16(R[1]);R[11]=RD16(R[2]);
    R[10]|=0x10000u;R[1]=ALT[7];R[10]>>=1;R[11]<<=16;
    R[10]=mul16(R[1],R[10]);R[10]>>=16;              /* distance step per cell */
    R[12]=0u;R[20]=0x40000u;R[21]=0xF0343Eu;
    R[0]=R[4];ALT[26]=R[11];R[0]=~R[0];ALT[25]=R[10];R[0]>>=16;
    R[10]=mul16(R[0],R[10]);R[11]>>=16;R[10]>>=16;R[11]=mul16(R[0],R[11]);  /* first, partial cell */
dda:                                        /* $F03410 */
    if(!neg32(R[12]-R[20]))goto far;
    if(R[5]<R[11]){
        R[3]=RD8(R[9]);
        { int hit=R[3]!=0u; R[8]-=R[7]; if(hit)goto hit_minor; }
        goto resume_minor;
    }
    goto resume_major;
resume_minor:                               /* $F03422 */
    R[9]-=R[7];
resume_major:                               /* $F03424 */
    R[3]=RD8(R[8]);R[5]-=R[11];
    { int hit=R[3]!=0u; R[9]+=R[6]; if(hit)goto hit_major; }
resume_step:                                /* $F0342E */
    R[8]+=R[6];
    if(R[12]!=0u){R[12]+=R[10];goto dda;}
    R[12]+=R[10];R[10]=ALT[25];R[11]=ALT[26];R[4]=0u;
    goto dda;
far:                                        /* $F0343E */
    ALT[19]=R[20];R[0]=0xF034BCu;R[18]=0u;R[19]=0u;
    goto record;
hit_major:                                  /* $F0344C */
    R[1]=R[10];R[0]=R[5];R[1]+=R[12];R[0]^=R[13];
    { int beyond=!neg32(R[1]-R[20]); R[21]=R[8]; if(beyond)goto far; }
    wall_hit(s,bus);
    R[17]=0xF0342Eu;R[0]=0xF034BCu;
    goto record;
hit_minor:                                  /* $F0347A */
    R[1]=R[11];R[0]=R[5];R[1]>>=16;R[1]+=1u;
    R[0]=gpu_div(s,R[0],R[1]);
    R[1]=R[10];R[1]=mul16(R[0],R[1]);R[1]>>=16;R[1]+=R[12];
    { int beyond=!neg32(R[1]-R[20]); R[21]=R[9]; if(beyond)goto far; }
    { int nz=R[12]!=0u; R[2]=R[4];
      if(!nz){R[2]=~R[2];R[2]>>=16;R[0]=mul16(R[2],R[0]);R[0]+=R[4];R[0]>>=16;} }
    R[0]<<=16;R[0]^=R[13];
    wall_hit(s,bus);
    R[17]=0xF03422u;
record:                                     /* $F034BC: column record for stage 2 */
    R[20]=0x40000u;R[21]=0xF0343Eu;
    R[2]=R[19];R[1]=R[18];R[2]<<=24;R[1]<<=24;R[2]>>=8;R[0]=ALT[31];R[2]|=R[1];R[2]>>=16;
    { int same=R[2]==R[0]; ALT[31]=R[2]; if(same)goto resume; }
    { int nz=R[22]!=0u; R[0]=R[22];
      if(!nz){R[22]=ALT[29];R[0]=R[16];WR32(R[16],R[22]);R[0]-=4u;R[22]-=4u;} }
    R[1]=ALT[19];R[1]>>=3;R[1]<<=16;R[1]|=R[2];
    WR32(R[0],R[1]);                       /* depth<<16 | window */
    R[22]+=4u;
    { int open=R[2]!=0u; ALT[29]=R[22]; if(open)goto resume; }
    R[16]+=8u;
    return;
resume:                                     /* jump (r17) */
    if(R[17]==0xF0342Eu)goto resume_step;
    if(R[17]==0xF03422u)goto resume_minor;
    s->fault=R[17]?R[17]:1u;               /* not reachable in the original */
}

void avp_view3d_stage1(AvpView3DState *s,const AvpView3DBus *bus)
{
    uint32_t *R=s->r,*ALT=s->alt;
    overlay_loader(s,bus,0x7F68u,0x2B0u);
    blit_wait(s,bus);
    R[2]=ALT[6];R[0]=ALT[15];WR32(R[0],R[2]);      /* A2_BASE = framebuffer */
    /* $F03220 */
    blit_wait(s,bus);
    R[1]=0x64220u;R[0]=A2_FLAGS;WR32(R[0],R[1]);
    R[1]=0x73820u;R[0]=A1_FLAGS;WR32(R[0],R[1]);
    R[16]=AVP_VIEW3D_COLUMN_TABLE+4u;
    R[0]=AVP_VIEW3D_COLUMN_POOL;ALT[29]=R[0];
    R[23]=0xF03410u;R[24]=0xF0347Au;R[25]=0xF0344Cu;
    R[14]=0u;
    do{                                     /* $F03264: one ray per column */
        R[2]=R[14];R[2]<<=1;R[1]=0x13Fu;R[2]-=R[1];
        R[3]=R[2];R[2]=abs32(R[2]);R[2]<<=9;R[1]=0x140u;
        R[2]=gpu_div(s,R[2],R[1]);
        R[0]=AVP_VIEW3D_ROM_COLUMN_ANGLE;R[2]<<=1;R[0]+=R[2];
        R[0]=RD16(R[0]);                    /* angle of this column off centre */
        R[2]=R[0];R[1]=ALT[8];
        if(!neg32(R[3]))R[0]=0u-R[0];
        R[0]<<=16;R[0]+=R[1];
        R[22]=0u;R[1]=AVP_VIEW3D_ROM_COLUMN_COS;R[3]=0u;R[2]>>=5;R[3]|=0x200u;R[3]-=R[2];R[3]<<=1;R[3]+=R[1];
        R[3]=RD16(R[3]);                    /* fish-eye correction */
        cast_column(s,bus);
        if(s->fault)return;
        R[14]+=1u;R[0]=0x140u;R[1]=0xF03264u;
    }while(neg32(R[14]-R[0]));
}
