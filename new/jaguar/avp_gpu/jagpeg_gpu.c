/* RE24 Session 2 (Claude): native C port of the retail game's GPU JagPEG
 * decoder (the GPU program the 68000 starts at $F03000 from exp_jpeg).
 *
 * Hand-written from the original GPU code; no ROM data.  Register-faithful
 * like view3d.c: B0[] / B1[] are the GPU's two register banks (the decoder
 * runs Huffman decoding in bank 0 and the IDCT / colour conversion in bank
 * 1, exchanging values with MOVEFA/MOVETA).  All memory accesses, including
 * the decoder's work areas in GPU local RAM ($F03A00-$F03FFF) and the
 * Blitter registers, go through the bus so the output is bit-identical.
 *
 * Work areas (GPU local RAM):
 *   $F03A00  64 decoded pixels (32-bit) for the Blitter
 *   $F03B00  coefficient blocks: Y $F03B00, Cb $F03C00, Cr $F03D00
 *   $F03E00  64 quantisation entries: zigzag byte offset<<16 | quant value
 *   $F03F40  mailbox (record, destination, width, flags; see jagpeg_load.h)
 *   $F03F58/$F03F5C  current block x/y;  $F03F60.. DC predictors;
 *   $F03F6C  layout (record word +2, minus 1): 0 = 4:4:4, 1 = 2 Y blocks,
 *            2 = 4 Y blocks per macroblock.
 */
#include "jaguar/avp_gpu/jagpeg_load.h"

#define RD32(a) bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,4)
#define RD8(a)  (bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,1)&0xFFu)
#define RD16(a) (bus->rd(bus->ctx,(uint32_t)(a)&0xFFFFFFu,2)&0xFFFFu)
#define WR32(a,v) bus->wr(bus->ctx,(uint32_t)(a)&0xFFFFFFu,(uint32_t)(v),4)

static uint32_t imul16(uint32_t a,uint32_t b){return (uint32_t)((int32_t)(int16_t)a*(int32_t)(int16_t)b);}
static uint32_t mul16(uint32_t a,uint32_t b){return (a&0xFFFFu)*(b&0xFFFFu);}
static uint32_t sar(uint32_t v,unsigned n){return (uint32_t)((int32_t)v>>n);}
static int neg32(uint32_t v){return (v>>31)!=0;}
static uint32_t sat8(uint32_t v){int32_t x=(int32_t)v;return x<0?0u:(x>255?255u:(uint32_t)x);}
/* SH / SHA with a register count: positive = right, negative = left */
static uint32_t sh(uint32_t d,uint32_t c)
{int32_t n=(int32_t)c;if(n>=0)return n>=32?0u:d>>n;n=-n;return n>=32?0u:d<<n;}
static uint32_t sha(uint32_t d,uint32_t c)
{int32_t n=(int32_t)c;if(n>=0)return n>=32?(uint32_t)((int32_t)d>>31):(uint32_t)((int32_t)d>>n);n=-n;return n>=32?0u:d<<n;}

typedef struct Dec { uint32_t B0[32],B1[32]; } Dec;

/* Read the next code bit (bank 0: r3 = bit buffer, r4 = bits left, r7 =
 * stream pointer).  Returns the bit shifted out of r3 (the carry). */
static unsigned shift_bit(uint32_t *R){unsigned c=R[3]>>31;R[3]<<=1;return c;}
static void refill31(uint32_t *R,const AvpView3DBus *bus){R[3]=RD32(R[7]);R[4]=31u;R[7]+=4u;}

/* $F0334E: magnitude bits that straddle the bit buffer, read one at a time.
 * R2 = size, first bit already in r3.  Result in R8. */
static void slow_magnitude(uint32_t *R,const AvpView3DBus *bus)
{
    R[4]-=1u;
    if(shift_bit(R)){                        /* $F03372: positive */
        unsigned c;
        R[2]-=1u;R[8]=2u;R[4]-=1u;
        if(neg32(R[4]))refill31(R,bus);
        c=shift_bit(R);R[8]=R[8]+R[16]+c;R[2]-=1u;
        if(R[2]==0u)return;
        do{
            R[8]<<=1;R[4]-=1u;
            if(neg32(R[4]))refill31(R,bus);
            c=shift_bit(R);R[8]=R[8]+R[16]+c;R[2]-=1u;
        }while(R[2]!=0u);
        return;
    }
    R[8]=R[20];R[2]-=1u;                     /* negative: start from -2 */
    do{
        unsigned c;
        R[8]<<=1;R[4]-=1u;
        if(neg32(R[4]))refill31(R,bus);
        c=shift_bit(R);R[8]=R[8]+R[16]+c;R[2]-=1u;
    }while(R[2]!=0u);
    R[8]+=1u;
}
/* $F031EE / $F0326E: R2 = magnitude size; extend the next R2 bits to a
 * signed value in R8 (JPEG "EXTEND").  Returns 1 if the slow path ran. */
static void magnitude(uint32_t *R,const AvpView3DBus *bus)
{
    int slow=neg32(R[4]-R[2]);
    R[8]=R[3];
    if(slow){slow_magnitude(R,bus);return;}
    R[4]-=R[2];R[2]=0u-R[2];
    { int positive=(R[8]>>31)!=0;
      R[3]=sh(R[3],R[2]);
      if(positive){R[2]+=32u;R[8]=sh(R[8],R[2]);return;} }
    R[8]>>=1;R[2]+=31u;R[8]|=0x80000000u;R[8]=sha(R[8],R[2]);R[8]+=1u;
}

/* Bank-1 IDCT and output ($F033A8-$F03990). */
static void idct_and_output(Dec *d,const AvpView3DBus *bus)
{
    uint32_t *R=d->B1,*A=d->B0;
    /* rows */
    R[0]=8u;R[24]=0xF033CAu;R[15]=A[6];R[14]=A[9];
    do{
        unsigned c=R[14]&1u;R[14]>>=1;
        if(c){
            R[2]=RD32(R[15]+8u);R[3]=RD32(R[15]+24u);
            R[1]=R[2];R[1]+=R[3];R[18]=R[3];R[18]=imul16(R[7],R[18]);R[1]=imul16(R[10],R[1]);R[18]+=R[1];
            R[19]=R[2];R[19]=imul16(R[11],R[19]);R[19]+=R[1];
            R[16]=RD32(R[15]);R[25]=RD32(R[15]+16u);
            R[17]=R[16];R[17]-=R[25];R[16]+=R[25];R[17]<<=13;R[16]<<=13;
            R[20]=R[16]+R[19];R[23]=R[16]-R[19];R[21]=R[17]+R[18];R[22]=R[17]-R[18];
            R[19]=RD32(R[15]+4u);R[18]=RD32(R[15]+12u);R[17]=RD32(R[15]+20u);R[16]=RD32(R[15]+28u);
            goto odd_row;
        row_back:
            R[1]=1u<<10;
            R[25]=R[20]+R[19]+R[1];R[5]=R[21]+R[18]+R[1];R[25]=sar(R[25],11);R[5]=sar(R[5],11);
            WR32(R[15],R[25]);R[15]+=4u;WR32(R[15],R[5]);R[15]+=4u;
            R[25]=R[22]+R[17]+R[1];R[5]=R[23]+R[16]+R[1];R[25]=sar(R[25],11);R[5]=sar(R[5],11);
            WR32(R[15],R[25]);R[15]+=4u;WR32(R[15],R[5]);R[15]+=4u;
            R[25]=R[23]-R[16]+R[1];R[5]=R[22]-R[17]+R[1];R[25]=sar(R[25],11);R[5]=sar(R[5],11);
            WR32(R[15],R[25]);R[15]+=4u;WR32(R[15],R[5]);R[15]+=4u;
            R[25]=R[21]-R[18]+R[1];R[5]=R[20]-R[19]+R[1];R[25]=sar(R[25],11);R[5]=sar(R[5],11);
            WR32(R[15],R[25]);R[15]+=4u;WR32(R[15],R[5]);R[15]+=4u;
        }else R[15]+=32u;
        R[0]-=1u;
        continue;
    odd_row:
        R[1]=R[19]+R[16];R[2]=R[17]+R[18];R[1]=imul16(R[12],R[1]);R[2]=imul16(R[28],R[2]);
        R[3]=R[16]+R[18];R[4]=R[17]+R[19];R[5]=R[3]+R[4];R[5]=imul16(R[13],R[5]);
        R[16]=imul16(R[8],R[16]);R[17]=imul16(R[27],R[17]);R[18]=imul16(R[29],R[18]);R[19]=imul16(R[6],R[19]);
        R[3]=imul16(R[26],R[3]);R[4]=imul16(R[9],R[4]);
        R[3]+=R[5];R[4]+=R[5];R[16]+=R[3];R[17]+=R[4];R[16]+=R[1];R[17]+=R[2];
        R[19]+=R[1];R[18]+=R[2];R[19]+=R[4];R[18]+=R[3];
        goto row_back;
    }while(R[0]!=0u);
    /* columns */
    R[15]=A[6];R[24]=0xF034D0u;R[0]=8u;
    do{
        R[2]=RD32(R[15]+64u);R[14]=24u<<3;R[3]=RD32(R[14]+R[15]);
        R[1]=R[2]+R[3];R[18]=imul16(R[7],R[3]);R[1]=imul16(R[10],R[1]);R[18]+=R[1];
        R[19]=imul16(R[11],R[2]);R[19]+=R[1];
        R[16]=RD32(R[15]);R[14]=A[19];R[17]=R[16];R[14]>>=1;R[25]=RD32(R[15]+R[14]);
        R[16]+=R[25];R[17]-=R[25];R[16]<<=13;R[17]<<=13;
        R[20]=R[16]+R[19];R[23]=R[16]-R[19];R[21]=R[17]+R[18];R[22]=R[17]-R[18];
        R[19]=RD32(R[15]+32u);R[18]=RD32(R[15]+96u);R[14]=5u<<5;R[15]+=R[14];
        R[17]=RD32(R[15]);R[16]=RD32(R[15]+64u);
        R[1]=R[16]+R[19];R[2]=R[17]+R[18];R[1]=imul16(R[12],R[1]);R[2]=imul16(R[28],R[2]);
        R[3]=R[16]+R[18];R[4]=R[17]+R[19];R[5]=R[3]+R[4];R[5]=imul16(R[13],R[5]);
        R[16]=imul16(R[8],R[16]);R[17]=imul16(R[27],R[17]);R[18]=imul16(R[29],R[18]);R[19]=imul16(R[6],R[19]);
        R[3]=imul16(R[26],R[3]);R[4]=imul16(R[9],R[4]);
        R[3]+=R[5];R[4]+=R[5];R[16]+=R[3];R[17]+=R[4];R[16]+=R[1];R[17]+=R[2];
        R[19]+=R[1];R[18]+=R[2];R[19]+=R[4];R[18]+=R[3];
        R[1]=1u<<17;R[15]=A[6];R[3]=0x80u;
        R[25]=R[20]+R[19]+R[1];R[5]=R[21]+R[18]+R[1];R[25]=sar(R[25],18);R[5]=sar(R[5],18);
        WR32(R[15],R[25]);R[15]+=32u;WR32(R[15],R[5]);R[15]+=32u;
        R[25]=R[22]+R[17]+R[1];R[5]=R[23]+R[16]+R[1];R[25]=sar(R[25],18);R[5]=sar(R[5],18);
        WR32(R[15],R[25]);R[15]+=32u;WR32(R[15],R[5]);R[15]+=32u;
        R[25]=R[23]-R[16]+R[1];R[5]=R[22]-R[17]+R[1];R[25]=sar(R[25],18);R[5]=sar(R[5],18);
        WR32(R[15],R[25]);R[15]+=32u;WR32(R[15],R[5]);R[15]+=32u;
        R[25]=R[21]-R[18]+R[1];R[5]=R[20]-R[19]+R[1];R[25]=sar(R[25],18);R[5]=sar(R[5],18);
        WR32(R[15],R[25]);R[15]+=32u;WR32(R[15],R[5]);R[15]+=32u;
        R[15]=A[6];R[15]+=4u;A[6]=R[15];      /* next column (bank 0 r6) */
        R[0]-=1u;
    }while(R[0]!=0u);
    /* colour conversion once a macroblock's chroma is available */
    R[2]=0xF03954u;R[1]=A[15];
    if(neg32(R[1]-5u))return;
    R[20]=A[19];R[16]=0xF03B00u;R[19]=R[16]+R[20];R[18]=R[19]+R[20];
    R[14]=R[20]>>1;R[20]=0xF03A00u;R[4]=0u;
    R[17]=0x59BAu;R[23]=0xFFFFE9FAu;R[25]=0xFFFFD24Cu;R[5]=0x7168u;
    R[15]=A[12];R[22]=RD32(R[15]+12u);
#define PIXEL() do{ \
        R[2]=RD32(R[18]);R[1]=RD32(R[16]);R[3]=RD32(R[19]);R[1]+=R[14]; \
        R[3]=imul16(R[17],R[3]);R[2]=imul16(R[5],R[2]);R[3]=sar(R[3],14);R[2]=sar(R[2],14); \
        R[3]+=R[1];R[2]+=R[1];R[3]=sat8(R[3]);R[2]=sat8(R[2]); \
        R[21]=R[2];R[3]<<=16;R[21]|=R[3]; \
        R[2]=RD32(R[18]);R[3]=RD32(R[19]);R[2]=imul16(R[23],R[2]);R[3]=imul16(R[25],R[3]); \
        R[3]+=R[2];R[3]=sar(R[3],14);R[1]+=R[3];R[1]=sat8(R[1]);R[1]<<=24;R[21]|=R[1]; }while(0)
    if(R[22]==0u){                           /* $F037A6: one chroma sample per pixel */
        R[24]=0xF037B0u;R[22]=32u;
        do{
            PIXEL();
            WR32(R[18],R[4]);WR32(R[16],R[4]);WR32(R[19],R[4]);
            R[16]+=4u;R[19]+=4u;R[18]+=4u;
            WR32(R[20],R[21]);R[20]+=4u;
            PIXEL();
            WR32(R[20],R[21]);R[20]+=4u;
            WR32(R[18],R[4]);WR32(R[16],R[4]);WR32(R[19],R[4]);
            R[16]+=4u;R[19]+=4u;R[18]+=4u;
            R[22]-=1u;
        }while(R[22]!=0u);
    }else if(R[22]==1u){                     /* $F036F8: chroma halved horizontally */
        R[22]=A[11];
        if(R[22]&1u){R[19]+=16u;R[18]+=16u;}
        R[22]=8u;R[24]=0xF0370Eu;R[15]=4u;
        for(;;){
            PIXEL();
            WR32(R[16],R[4]);R[16]+=4u;WR32(R[20],R[21]);R[20]+=4u;
            PIXEL();
            WR32(R[20],R[21]);R[20]+=4u;
            WR32(R[18],R[4]);WR32(R[16],R[4]);WR32(R[19],R[4]);
            R[16]+=4u;R[19]+=4u;R[18]+=4u;
            R[15]-=1u;if(R[15]!=0u)continue;
            R[15]=4u;R[19]+=16u;R[18]+=16u;R[22]-=1u;
            if(R[22]==0u)break;
        }
    }else{                                   /* $F0362A: chroma halved both ways */
        R[22]=A[15];R[22]>>=2;
        if(R[22]&1u){R[19]+=16u;R[18]+=16u;}
        if(!neg32(R[22]-4u)){R[22]=1u<<7;R[18]+=R[22];R[19]+=R[22];}
        R[22]=8u;R[24]=0xF03650u;R[15]=4u;
        for(;;){
            PIXEL();
            WR32(R[16],R[4]);R[16]+=4u;WR32(R[20],R[21]);R[20]+=4u;
            PIXEL();
            WR32(R[20],R[21]);R[20]+=4u;
            WR32(R[18],R[4]);WR32(R[16],R[4]);WR32(R[19],R[4]);
            R[16]+=4u;R[19]+=4u;R[18]+=4u;
            R[15]-=1u;if(R[15]!=0u)continue;
            R[15]=4u;
            if(!(R[22]&1u)){R[19]-=16u;R[18]-=16u;R[22]-=1u;continue;}
            R[19]+=16u;R[18]+=16u;R[22]-=1u;
            if(R[22]==0u)break;
        }
    }
#undef PIXEL
    /* $F0383A: blit the 8x8 pixel block to the destination */
    R[16]=0xF03F44u;R[1]=RD32(R[16]);R[2]=0xF03A00u;
    R[16]+=20u;R[3]=RD32(R[16]);R[16]+=4u;R[4]=RD32(R[16]);R[16]-=12u;R[5]=RD32(R[16]);
    R[16]=0xF02200u;WR32(R[16],R[1]);
    R[16]+=4u;R[17]=0x28u;R[17]|=R[5];WR32(R[16],R[17]);
    R[16]+=8u;R[17]=R[4];R[17]<<=16;R[17]|=R[3];WR32(R[16],R[17]);
    R[16]+=4u;R[17]=0x1FFF8u;WR32(R[16],R[17]);
    R[16]+=20u;WR32(R[16],R[2]);
    R[16]+=4u;R[17]=0x1828u;WR32(R[16],R[17]);
    R[16]+=8u;R[17]=0u;WR32(R[16],R[17]);
    R[16]+=4u;R[17]=0x1FFF8u;WR32(R[16],R[17]);
    R[16]+=8u;R[17]=0x80008u;WR32(R[16],R[17]);
    R[16]-=4u;R[17]=0x1800601u;WR32(R[16],R[17]);
    /* advance the block position */
    R[15]=A[12];R[16]=RD32(R[15]+12u);R[17]=0xF038F8u;
    if(R[16]!=2u){
        R[16]=0xF03F40u;R[17]=RD32(R[16]);R[17]+=4u;R[16]=RD16(R[17]);R[16]<<=3;
        R[17]=0xF038E6u;R[3]+=8u;
        if(R[3]==R[16]){R[3]=0u;R[4]+=8u;R[16]=0xF03F5Cu;WR32(R[16],R[4]);}
        R[16]=0xF03F58u;WR32(R[16],R[3]);
        return;
    }
    R[16]=A[15];R[16]>>=2;R[17]=0xF03944u;
    if(R[16]==2u)R[3]+=8u;
    else if(R[16]==3u){R[3]-=8u;R[4]+=8u;}
    else if(R[16]==4u)R[3]+=8u;
    else{
        R[4]-=8u;R[3]+=8u;
        R[16]=0xF03F40u;R[17]=RD32(R[16]);R[17]+=4u;R[16]=RD16(R[17]);R[16]<<=3;
        if(R[3]==R[16]){R[3]=0u;R[4]+=16u;}
    }
    R[16]=0xF03F5Cu;WR32(R[16],R[4]);
    R[16]=0xF03F58u;WR32(R[16],R[3]);
}

void avp_jagpeg_gpu_decode(const AvpView3DBus *bus,const AvpJagpegJob *job)
{
    static Dec dec;
    Dec *d=&dec;
    uint32_t *R=d->B0,*S=d->B1;
    (void)job;                               /* the GPU reads the job from the mailbox */
    WR32(AVP_JPG_MBOX_DONE,1u);
    R[16]=0u;R[17]=32u;R[22]=0xF0u;R[21]=15u;R[20]=0u-2u;R[19]=0x100u;R[18]=0x3Cu;
    R[1]=0xF03E00u;R[4]=0u;R[31]=0xF03F40u;R[7]=RD32(R[31]);
    R[12]=0xF03F60u;R[14]=R[12];
    WR32(R[14],R[16]);WR32(R[14]+4u,R[16]);WR32(R[14]+8u,R[16]);
    R[7]+=2u;R[31]=RD16(R[7]);R[7]+=2u;R[31]-=1u;WR32(R[14]+12u,R[31]);
    R[15]=0u;
    R[23]=0xF0326Eu;R[24]=0xF03242u;R[25]=0xF032B0u;R[26]=0xF0328Eu;R[28]=0xF0334Eu;R[29]=0xF03372u;R[0]=0xF0333Au;
    R[31]=0xF03F48u;R[9]=RD32(R[31]);R[9]+=4u;R[10]=R[9];R[31]=0x60u;R[10]+=R[31];   /* Huffman trees */
    R[11]=RD16(R[7]);R[7]+=2u;R[31]=RD16(R[7]);R[7]+=2u;R[11]=mul16(R[31],R[11]);   /* blocks */
    R[31]=0u;WR32(0xF03F58u,R[31]);WR32(0xF03F5Cu,R[31]);
    /* IDCT constants (bank 1) */
    S[8]=0x98Eu;S[9]=0xFFFFF384u;S[10]=0x1151u;S[11]=0x187Eu;S[12]=0xFFFFE333u;S[13]=0x25A1u;
    S[6]=0x300Bu;S[7]=0xFFFFC4DFu;S[26]=0xFFFFC13Bu;S[27]=0x41B3u;S[28]=0xFFFFADFDu;S[29]=0x6254u;
    S[15]=0xF03B00u;
    WR32(0xF00050u,0xFFFFFFFFu);
    R[6]=0xF03B00u;
    for(int i=0;i<24*8;i++){WR32(R[6],R[16]);R[6]+=4u;}
    R[31]=0u;R[30]=0xF03156u;
    for(;;){                                 /* $F03182: next block */
        R[6]=0xF03B00u;R[16]=0u;
        R[13]=R[15];if(!neg32(R[15]-11u))R[13]=0u;
        R[13]<<=6;R[6]+=R[13];
        R[5]=R[1];R[1]+=R[19];R[9]=1u;
        /* DC: walk the Huffman tree */
        R[27]=0xF0320Eu;R[2]=R[10];R[31]=3u<<5;R[2]-=R[31];
        for(;;){
            R[4]-=1u;
            if(neg32(R[4]))refill31(R,bus);
            if(shift_bit(R))R[2]-=4u;
            R[2]=RD32(R[2]);
            if(!neg32(R[19]-R[2]))break;
        }
        if(R[2]==0u){R[8]=0u;R[30]=0xF0320Eu;}
        else{
            if(R[4]==0u){R[3]=RD32(R[7]);R[4]=R[17];R[7]+=4u;}
            magnitude(R,bus);
        }
        /* $F0320E: DC prediction and dequantisation */
        R[31]=RD32(R[15]+R[12]);
        if(!neg32(R[15]-11u))R[31]=RD32(R[12]);
        R[31]+=R[8];R[13]=RD32(R[5]);R[13]=imul16(R[31],R[13]);
        if(!neg32(R[15]-11u))WR32(R[12],R[31]);else WR32(R[15]+R[12],R[31]);
        R[5]+=4u;WR32(R[6],R[13]);
        /* AC coefficients */
        R[27]=0xF0328Eu;
        for(;;){
            R[2]=R[10];
            for(;;){
                R[4]-=1u;
                if(neg32(R[4]))refill31(R,bus);
                if(shift_bit(R))R[2]-=4u;
                R[2]=RD32(R[2]);
                if(!neg32(R[19]-R[2]))break;
            }
            R[14]=R[2];R[14]&=R[22];R[14]>>=2;R[5]+=R[14];   /* skip the run */
            R[2]&=R[21];
            if(R[2]==0u){                    /* EOB or ZRL */
                if(R[14]!=R[18])break;
                R[5]+=4u;
                if(neg32(R[5]-R[1]))continue;
                break;
            }
            if(R[4]==0u){R[3]=RD32(R[7]);R[4]=R[17];R[7]+=4u;}
            magnitude(R,bus);
            /* $F0328E: store at the zigzag position */
            R[14]=RD32(R[5]);R[13]=R[14];R[13]<<=16;R[14]>>=16;R[13]>>=16;
            R[8]=imul16(R[13],R[8]);
            WR32(R[14]+R[6],R[8]);
            R[14]>>=5;R[31]=1u;R[14]=0u-R[14];R[31]=sh(R[31],R[14]);R[9]|=R[31];
            R[5]+=4u;
            if(!neg32(R[5]-R[1]))break;
        }
        idct_and_output(d,bus);
        /* $F032C8 */
        R[30]=0xF032C8u;
        R[1]-=R[19];R[30]=0xF03182u;R[15]+=4u;
        if(neg32(R[15]-12u))continue;
        R[14]=R[12];R[31]=RD32(R[14]+12u);
        if(R[31]==0u)R[15]=0u;
        else{R[13]=8u;R[31]+=1u;R[13]=mul16(R[31],R[13]);if(R[15]==R[13])R[15]=0u;}
        R[11]-=1u;
        if(R[11]==0u)break;
    }
    WR32(AVP_JPG_MBOX_DONE,AVP_JPG_DONE_MAGIC);
    R[31]=0xF00050u;R[0]=RD32(R[31]);
}

/* ---- RGB -> CRY converter ($F03A00, copied from DRAM $1BD30) ------------
 * Converts cv->pixels (rounded down to a multiple of 4) packed 32-bit
 * pixels at cv->source into 16-bit CRY words at cv->dest: intensity = the
 * largest channel; the two other channels are scaled by $FF00/max and
 * looked up (byte) in the colour table at cv->arg (two 64 KiB halves; the
 * second is used when the middle channel is the largest, and the table
 * result is XORed with $F0 when the low channel is).  Ends with $7F80 = 0. */
void avp_jagpeg_gpu_convert(const AvpView3DBus *bus,const AvpJagpegConvert *cv)
{
    uint32_t R[32]={0};
    (void)cv;                                /* the program reads $7F80-$7F8C */
    R[14]=0xFFu;R[15]=0xF0u;R[19]=0xFF00u;R[0]=0x7F80u;
    R[5]=RD32(R[0]);
    if(R[5]!=0u){
        R[5]&=~3u;
        R[0]+=4u;R[3]=RD32(R[0]);R[0]+=4u;R[4]=RD32(R[0]);R[0]+=4u;R[11]=RD32(R[0]);
        R[1]=0x10000u;R[12]=R[11]+R[1];
        do{
            R[6]=RD32(R[3]);R[3]+=4u;
            R[8]=R[6];R[7]=R[6];R[6]>>=16;R[7]>>=24;R[6]&=R[14];R[8]&=R[14];
            R[10]=R[6];if(R[6]<R[7])R[10]=R[7];
            if(R[10]<R[8])R[10]=R[8];
            R[20]=R[19];
            R[9]=R[10];
            if(R[10]!=0u){
                uint32_t rem;
                R[20]=avp_view3d_div(R[20],R[10],0,&rem);
                R[8]=mul16(R[20],R[8]);
                if(R[6]==R[10]){
                    R[7]=mul16(R[20],R[7]);R[8]>>=8;R[7]&=R[19];R[7]+=R[8];R[7]+=R[11];
                    R[9]=RD8(R[7]);
                }else{
                    R[6]=mul16(R[20],R[6]);
                    if(R[7]==R[10]){
                        R[8]&=R[19];R[6]>>=8;R[8]+=R[6];R[8]+=R[12];
                        R[9]=RD8(R[8]);
                    }else{
                        R[7]=mul16(R[20],R[7]);R[6]>>=8;R[7]&=R[19];R[7]+=R[6];R[7]+=R[11];
                        R[9]=RD8(R[7]);R[9]^=R[15];
                    }
                }
            }
            R[9]<<=8;R[9]|=R[10];
            R[5]-=1u;
            bus->wr(bus->ctx,R[4]&0xFFFFFFu,R[9]&0xFFFFu,2);
            R[4]+=2u;
        }while(R[5]!=0u);
    }
    WR32(0x7F80u,0u);
}
