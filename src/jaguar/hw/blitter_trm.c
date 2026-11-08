/* TOM Blitter from the Atari Jaguar TRM Rev.8 pp.64-82; see blitter_trm.h. */
#include "jaguar/blitter_trm.h"
#include <string.h>
#include "jaguar/cry.h"

/* command bits, p.73-75 */
#define C_SRCEN    (1u<<0)
#define C_SRCENZ   (1u<<1)
#define C_SRCENX   (1u<<2)
#define C_DSTEN    (1u<<3)
#define C_DSTENZ   (1u<<4)
#define C_DSTWRZ   (1u<<5)
#define C_CLIP_A1  (1u<<6)
#define C_UPDA1F   (1u<<8)
#define C_UPDA1    (1u<<9)
#define C_UPDA2    (1u<<10)
#define C_DSTA2    (1u<<11)
#define C_GOURD    (1u<<12)
#define C_GOURZ    (1u<<13)
#define C_TOPBEN   (1u<<14)
#define C_TOPNEN   (1u<<15)
#define C_PATDSEL  (1u<<16)
#define C_ADDDSEL  (1u<<17)
#define C_CMPDST   (1u<<25)
#define C_BCOMPEN  (1u<<26)
#define C_DCOMPEN  (1u<<27)
#define C_BKGWREN  (1u<<28)
#define C_SRCSHADE (1u<<30)
#define C_BUSHI    (1u<<29)

uint32_t jag_blitter_errata_risks(const JagBlitter *b)
{
    uint32_t risk=0u, ps1,ps2,xclip;
    if(!b)return 0u;
    ps1=(b->a1_flags>>3)&7u;
    ps2=(b->a2_flags>>3)&7u;
    xclip=b->a1_clip&0x7FFFu;
    /* JDEV0002/Blitter/3: A1 clip X, even without C_CLIP_A1, affects
       the destination (including DSTA2). Zero means clipping disabled. */
    if(xclip&&ps1<=5u&&(xclip%(64u/(1u<<ps1)))!=0u)
        risk|=JB_ERRATA_PHANTOM_CLIP;
    /* JDEV0002/Blitter/4: 2bpp unaligned blits are unreliable. */
    if((ps1==1u&&((b->a1_pixel&0xFFFFu)%32u)!=0u)||
       (ps2==1u&&((b->a2_pixel&0xFFFFu)%32u)!=0u))
        risk|=JB_ERRATA_2BPP_ALIGN;
    /* JDEV0002/Blitter/5: Z operations with either color modifier. */
    if((b->cmd&(C_ADDDSEL|C_SRCSHADE))&&
       (b->cmd&(C_SRCENZ|C_DSTENZ|C_DSTWRZ|(7u<<18))))
        risk|=JB_ERRATA_Z_COLOUR;
    if(b->cmd&C_BUSHI)risk|=JB_ERRATA_PRIORITY;
    return risk;
}

void jag_blitter_init(JagBlitter *b,JagBlitBus bus){memset(b,0,sizeof(*b));b->bus=bus;}

uint32_t jag_blitter_width(uint32_t flags)
{   /* p.70: 4-bit exponent (bits 14-11), 2 stored mantissa bits (10-9) with
       implicit leading one and the binary point after it: (1.mm) x 2^e. */
    uint32_t e=(flags>>11)&15u,m=(flags>>9)&3u;
    return ((4u|m)<<e)>>2;
}

static void set64(uint64_t *r,uint32_t off,uint32_t v)
{ if(off&4u)*r=(*r&0xFFFFFFFF00000000ull)|v; else *r=(*r&0xFFFFFFFFull)|((uint64_t)v<<32); }

int jag_blitter_read(JagBlitter *b,uint32_t off,uint32_t *v)
{
    switch(off){
    case 0x38:*v=b->stopped?2u:1u;return 1;     /* status: IDLE (+STOPPED) */
    /* bug 10 (p.135): pointer registers read back at F02204 / F0222C */
    case 0x04:*v=b->a1_pixel;return 1;
    case 0x2C:*v=b->a2_pixel;return 1;
    case 0x0C:*v=b->a1_pixel;return 1;  /* documented address also answered */
    case 0x30:*v=b->a2_pixel;return 1;
    case 0x18:*v=b->a1_fpixel;return 1;
    default:*v=0;return off<0xA0u;
    }
}

typedef struct Gen { /* one address generator */
    uint32_t base,flags,psize_log,pbits,width,pitch_mul,zoff;
    int32_t x,y;      /* integer parts (16-bit signed semantics) */
    uint32_t fx,fy;   /* A1 fractions */
} Gen;

static void gen_setup(Gen *g,uint32_t base,uint32_t flags,uint32_t pixel,uint32_t frac)
{
    static const uint32_t pm[4]={1,2,4,3}; /* p.70 pitch 0,1,2,3 -> 1,2,4,3 phrases */
    g->base=base&~7u;g->flags=flags;g->psize_log=(flags>>3)&7u;if(g->psize_log>5)g->psize_log=5;
    g->pbits=1u<<g->psize_log;g->width=jag_blitter_width(flags);g->pitch_mul=pm[flags&3u];
    g->zoff=(flags>>6)&7u;
    g->x=(int16_t)(pixel&0xFFFFu);g->y=(int16_t)(pixel>>16);g->fx=frac&0xFFFFu;g->fy=frac>>16;
}
static uint32_t gen_pixel_bits(const Gen *g,int32_t x,int32_t y,uint32_t *addr)
{   /* p.66: Y as 12-bit unsigned, X as unsigned 0..32767 */
    uint64_t lin=(uint64_t)((uint32_t)y&0xFFFu)*g->width+((uint32_t)x&0x7FFFu);
    uint64_t bit=lin*g->pbits;
    uint64_t phrase=bit>>6;
    *addr=g->base+(uint32_t)(phrase*8u*g->pitch_mul)+(uint32_t)((bit&63u)>>3);
    return (uint32_t)(bit&7u);
}
static uint32_t rd_pix(JagBlitter *b,const Gen *g,int32_t x,int32_t y,int z)
{
    uint32_t a,sh=gen_pixel_bits(g,x,y,&a),v=0,n;
    if(z)a=(a&~7u)+g->zoff*8u+(a&7u);
    if(g->pbits<8){uint8_t byte=b->bus.read8(b->bus.ctx,a);return (byte>>(8u-sh-g->pbits))&((1u<<g->pbits)-1u);}
    for(n=0;n<g->pbits/8u;n++)v=(v<<8)|b->bus.read8(b->bus.ctx,a+n);
    return v;
}
static void wr_pix(JagBlitter *b,const Gen *g,int32_t x,int32_t y,uint32_t v,int z)
{
    uint32_t a,sh=gen_pixel_bits(g,x,y,&a),n,nb;
    if(z)a=(a&~7u)+g->zoff*8u+(a&7u);
    if(g->pbits<8){uint8_t byte=b->bus.read8(b->bus.ctx,a);uint32_t s=8u-sh-g->pbits,m=((1u<<g->pbits)-1u)<<s;
        byte=(uint8_t)((byte&~m)|((v<<s)&m));b->bus.write8(b->bus.ctx,a,byte);return;}
    nb=g->pbits/8u;
    for(n=0;n<nb;n++)b->bus.write8(b->bus.ctx,a+n,(uint8_t)(v>>(8u*(nb-1u-n))));
}
static uint32_t lane(uint64_t r,uint32_t pbits,uint32_t k)
{ uint32_t sh=64u-pbits*(k+1u); return (uint32_t)((r>>sh)&(pbits==32u?0xFFFFFFFFull:((1ull<<pbits)-1u))); }

static uint32_t lfu(uint32_t f,uint32_t s,uint32_t d)
{
    uint32_t r=0;
    if(f&1u)r|=~s&~d;
    if(f&2u)r|=~s&d;
    if(f&4u)r|=s&~d;
    if(f&8u)r|=s&d;
    return r;
}
/* p.74 ADDDSEL, 16-bit: TOPBEN/TOPNEN clear -> independent signed CRY field
 * offsets with saturating intensity; both set -> 16-bit saturating add. */
static uint32_t add16(uint32_t cmd,uint32_t s,uint32_t d,uint32_t *unsup)
{
    if((cmd&C_TOPBEN)&&(cmd&C_TOPNEN)){int32_t r=(int32_t)d+(int16_t)s;return (uint32_t)(r<0?0:(r>0xFFFF?0xFFFF:r));}
    if(!(cmd&(C_TOPBEN|C_TOPNEN))){
        int32_t y=(int32_t)(d&0xFFu)+(int8_t)(s&0xFFu);uint32_t r=(d+s)&0x0F00u,c=(d+s)&0xF000u;
        r=((d&0x0F00u)+(s&0x0F00u))&0x0F00u;c=((d&0xF000u)+(s&0xF000u))&0xF000u;
        return c|r|(uint32_t)(y<0?0:(y>255?255:y));
    }
    *unsup|=JB_UNSUP_ADDDSEL_SUB16;return (d+s)&0xFFFFu;
}

static void record(JagBlitter *b)
{
    for(unsigned i=0;i<b->usage_n;i++)if(b->usage[i].cmd==b->cmd&&b->usage[i].a1f==b->a1_flags&&b->usage[i].a2f==b->a2_flags){b->usage[i].n++;return;}
    if(b->usage_n<256u){b->usage[b->usage_n].cmd=b->cmd;b->usage[b->usage_n].a1f=b->a1_flags;b->usage[b->usage_n].a2f=b->a2_flags;b->usage[b->usage_n].n=1;b->usage_n++;}
}

/* Gouraud intensity / Z: four lanes, integer parts in PATD / SRCZ1, fractions
 * in SRCD / SRCZ2 (p.74, p.81).  Each pass adds the increment to every lane. */
static void gour_step(JagBlitter *b)
{
    for(unsigned k=0;k<4;k++){
        unsigned sh=48u-16u*k;
        if(b->cmd&C_GOURD){
            uint32_t fr=(uint32_t)(b->srcd>>sh)&0xFFFFu,in=(uint32_t)(b->patd>>sh)&0xFFFFu;
            uint32_t nf=fr+(b->iinc&0xFFFFu),carry=nf>>16;nf&=0xFFFFu;
            int32_t iv=(int32_t)(in&0xFFu)+(int32_t)(int8_t)((b->iinc>>16)&0xFFu)+(int32_t)carry;
            uint32_t colour=in&0xFF00u;
            if(b->cmd&C_TOPBEN){uint32_t full=(in+(((b->iinc>>16)&0xFFu)|(((b->iinc>>16)&0x80u)?0xFF00u:0u))+carry)&0xFFFFu;in=full;}
            else{ if(iv<0)iv=0; if(iv>255)iv=255; in=colour|(uint32_t)iv; }  /* saturate (p.81) */
            b->srcd=(b->srcd&~(0xFFFFull<<sh))|((uint64_t)nf<<sh);
            b->patd=(b->patd&~(0xFFFFull<<sh))|((uint64_t)in<<sh);
        }
        if(b->cmd&C_GOURZ){
            uint32_t fr=(uint32_t)(b->srcz2>>sh)&0xFFFFu,in=(uint32_t)(b->srcz1>>sh)&0xFFFFu;
            uint32_t nf=fr+(b->zinc&0xFFFFu),carry=nf>>16;nf&=0xFFFFu;
            int32_t zv=(int32_t)in+(int16_t)(b->zinc>>16)+(int32_t)carry;
            if(zv<0)zv=0;
            if(zv>0xFFFF)zv=0xFFFF;
            b->srcz2=(b->srcz2&~(0xFFFFull<<sh))|((uint64_t)nf<<sh);
            b->srcz1=(b->srcz1&~(0xFFFFull<<sh))|((uint64_t)(uint32_t)zv<<sh);
        }
    }
}

static void add_xy(Gen *g,int32_t dx,int32_t dy){g->x=(int16_t)(g->x+dx);g->y=(int16_t)(g->y+dy);}

static void run_blit(JagBlitter *b)
{
    Gen a1,a2,*dst,*src;
    uint32_t inner=b->count&0xFFFFu,outer=b->count>>16;
    uint32_t cmd=b->cmd,lfuf=(cmd>>21)&15u,zmode=(cmd>>18)&7u;
    int dsta2=(cmd&C_DSTA2)!=0;
    if(!inner)inner=65536u;
    if(!outer)outer=65536u;
    gen_setup(&a1,b->a1_base,b->a1_flags,b->a1_pixel,b->a1_fpixel);
    gen_setup(&a2,b->a2_base,b->a2_flags,b->a2_pixel,0);
    dst=dsta2?&a2:&a1; src=dsta2?&a1:&a2;
    /* SRCSHADE (p.75) needs GOURZ (bug 9, p.135); without it the data path is
       documented as incorrect and is reported, not invented. */
    if((cmd&C_SRCSHADE)&&!(cmd&C_GOURZ))b->unsupported|=JB_UNSUP_SRCSHADE;
    if(b->on_blit)b->on_blit(b->on_blit_ctx,b);
    record(b);b->blits++;
    {uint32_t risk=jag_blitter_errata_risks(b);
     b->errata_risks|=risk;
     for(unsigned bit=0u;bit<4u;bit++)if(risk&(1u<<bit))b->errata_counts[bit]++;}
    for(uint32_t o=0;o<outer;o++){
        uint32_t left=inner,bitc=0;
        uint32_t dxc=(dst->flags>>16)&3u,sxc=(src->flags>>16)&3u;
        int phrase=dxc==0u;
        uint32_t ppp=64u/dst->pbits,sppp=64u/src->pbits;
        /* logical source cursor (data alignment); the register follows the
           hardware pointer update rule below */
        int32_t scx=src->x,scy=src->y;
        if((cmd&C_SRCENX)&&(cmd&C_SRCEN)&&sxc==0u){src->x=(int16_t)((src->x&~(int32_t)(sppp-1u))+(int32_t)sppp);}
        while(left){
            uint32_t n=1,k0;
            if(phrase){k0=(uint32_t)dst->x&(ppp-1u);n=ppp-k0;if(n>left)n=left;}
            for(uint32_t j=0;j<n;j++){
                int32_t dx=phrase?dst->x+(int32_t)j:dst->x,dy=dst->y;
                int32_t sx=sxc==0u?scx+(int32_t)j:(sxc==1u?src->x+(((src->flags>>19)&1u)?-(int32_t)j:(int32_t)j):src->x),sy=sxc==0u?scy:src->y;
                uint32_t k=((uint32_t)dx)&(ppp-1u);
                uint32_t s,d,w,inhibit=0;
                s=(cmd&C_SRCEN)?rd_pix(b,src,sx,sy,0):lane(b->srcd,dst->pbits,k);
                if((cmd&C_SRCSHADE)&&(cmd&C_GOURZ)&&(cmd&C_SRCEN)&&src->pbits==16u)
                    /* source intensity + IINC integer part, saturating (p.75, p.81) */
                    s=avp_jaguar_srcshade_cry((u16)s,(int)(int8_t)((b->iinc>>16)&0xFFu));
                d=(cmd&C_DSTEN)?rd_pix(b,dst,dx,dy,0):lane(b->dstd,dst->pbits,k);
                if(cmd&C_GOURD)w=lane(b->patd,dst->pbits,k);
                else if(cmd&C_PATDSEL)w=lane(b->patd,dst->pbits,k);
                else if((cmd&C_ADDDSEL)&&dst->pbits==16u)w=add16(cmd,s,d,&b->unsupported);
                else w=lfu(lfuf,s,d)&(dst->pbits==32u?0xFFFFFFFFu:((1u<<dst->pbits)-1u));
                if(cmd&C_CLIP_A1){ /* p.71/p.74: A1 outside its window inhibits writes */
                    int32_t cw=(int32_t)(b->a1_clip&0x7FFFu),ch=(int32_t)((b->a1_clip>>16)&0x7FFFu);
                    int32_t ax=dsta2?a1.x:dx,ay=dsta2?a1.y:dy;
                    if(ax<0||ay<0||ax>=cw||ay>=ch)inhibit=2;
                }
                if(!inhibit&&(cmd&C_BCOMPEN)){ /* bit comparator: counter cleared per inner loop */
                    uint64_t sd=(cmd&C_SRCEN)?(uint64_t)s:b->srcd;
                    uint32_t bit=(cmd&C_SRCEN)?(s&1u):(uint32_t)((sd>>(63u-(bitc&63u)))&1u);
                    if(!bit)inhibit=1;
                }
                if(!inhibit&&(cmd&C_DCOMPEN)&&(dst->pbits==8u||dst->pbits==16u)){
                    uint32_t cv=(cmd&C_CMPDST)?d:s; if(cv==lane(b->patd,dst->pbits,k))inhibit=1;
                }
                if(!inhibit&&zmode&&dst->pbits==16u){
                    uint32_t sz=(cmd&C_GOURZ)?lane(b->srcz1,16,k):((cmd&C_SRCENZ)?rd_pix(b,src,sx,sy,1):lane(b->srcz1,16,k));
                    uint32_t dz=(cmd&C_DSTENZ)?rd_pix(b,dst,dx,dy,1):lane(b->dstz,16,k);
                    if(((zmode&1u)&&sz<dz)||((zmode&2u)&&sz==dz)||((zmode&4u)&&sz>dz))inhibit=1;
                    else if(cmd&C_DSTWRZ)wr_pix(b,dst,dx,dy,sz,1);
                }else if(!inhibit&&(cmd&C_DSTWRZ)&&dst->pbits==16u){
                    uint32_t sz=(cmd&C_GOURZ)?lane(b->srcz1,16,k):lane(b->srcz1,16,k);wr_pix(b,dst,dx,dy,sz,1);
                }
                if(inhibit==2){ /* clipped: no write at all */ }
                else if(inhibit){ if(phrase||(cmd&C_BKGWREN)){wr_pix(b,dst,dx,dy,d,0);b->pixels_written++;} }
                else{wr_pix(b,dst,dx,dy,w,0);b->pixels_written++;}
                bitc++;
                if(!phrase){ /* pixel-mode pointer updates per pass */
                    if(cmd&(C_GOURD|C_GOURZ))gour_step(b);
                }
            }
            left-=n;b->est_ticks+=2u;
            /* destination pointer update (p.71) */
            {
                Gen *gs[2]={dst,src};
                for(int gi=0;gi<2;gi++){
                    Gen *g=gs[gi];uint32_t xc=(g->flags>>16)&3u;int is_a1=(g==&a1);
                    /* Atari JDEV0002 (1995), Blitter item 1: only A1 YADD
                       enables stepping for both generators. For negative Y
                       direction, the corresponding generator's YADD must
                       ALSO be set along with its YSIGN (including A2). */
                    uint32_t yadd=(b->a1_flags>>18)&1u;
                    int ysign=((g->flags>>20)&1u)&&((g->flags>>18)&1u);
                    int xsign=(g->flags>>19)&1u;
                    uint32_t gppp=64u/g->pbits;
                    switch(xc){
                    case 0: if(gi==1){scx+=(int32_t)n;scy=g->y;}
                        g->x=(int16_t)((g->x&~(int32_t)(gppp-1u))+(int32_t)gppp);break;
                    case 1: g->x=(int16_t)(g->x+(xsign?-1:1)*(int32_t)(phrase?n:1u));if(gi==1){scx=g->x;scy=g->y;}break;
                    case 2: break;
                    case 3: if(is_a1){uint32_t fx=g->fx+(b->a1_finc&0xFFFFu),fy=g->fy+(b->a1_finc>>16);
                                g->x=(int16_t)(g->x+(int16_t)(b->a1_inc&0xFFFFu)+(int32_t)(fx>>16));
                                g->y=(int16_t)(g->y+(int16_t)(b->a1_inc>>16)+(int32_t)(fy>>16));
                                g->fx=fx&0xFFFFu;g->fy=fy&0xFFFFu;}
                            break;
                    }
                    if(xc!=3u&&yadd)g->y=(int16_t)(g->y+(ysign?-1:1));
                }
            }
            if(phrase&&(cmd&(C_GOURD|C_GOURZ)))gour_step(b);
        }
        /* outer loop updates (p.73-74) */
        if(cmd&C_UPDA1F){uint32_t fx=a1.fx+(b->a1_fstep&0xFFFFu),fy=a1.fy+(b->a1_fstep>>16);
            a1.fx=fx&0xFFFFu;a1.fy=fy&0xFFFFu;add_xy(&a1,(int32_t)(fx>>16),(int32_t)(fy>>16));}
        if(cmd&C_UPDA1)add_xy(&a1,(int16_t)(b->a1_step&0xFFFFu),(int16_t)(b->a1_step>>16));
        if(cmd&C_UPDA2)add_xy(&a2,(int16_t)(b->a2_step&0xFFFFu),(int16_t)(b->a2_step>>16));
    }
    b->a1_pixel=((uint32_t)(uint16_t)a1.y<<16)|(uint16_t)a1.x;
    b->a1_fpixel=(a1.fy<<16)|a1.fx;
    b->a2_pixel=((uint32_t)(uint16_t)a2.y<<16)|(uint16_t)a2.x;
}

int jag_blitter_write(JagBlitter *b,uint32_t off,uint32_t v)
{
    switch(off&~3u){
    case 0x00:b->a1_base=v;return 1; case 0x04:b->a1_flags=v;return 1;
    case 0x08:b->a1_clip=v;return 1; case 0x0C:b->a1_pixel=v;return 1;
    case 0x10:b->a1_step=v;return 1; case 0x14:b->a1_fstep=v;return 1;
    case 0x18:b->a1_fpixel=v;return 1; case 0x1C:b->a1_inc=v;return 1;
    case 0x20:b->a1_finc=v;return 1; case 0x24:b->a2_base=v;return 1;
    case 0x28:b->a2_flags=v;return 1; case 0x2C:b->a2_mask=v;return 1;
    case 0x30:b->a2_pixel=v;return 1; case 0x34:b->a2_step=v;return 1;
    case 0x38:b->cmd=v;if(!(v&0x80u))run_blit(b);return 1;          /* NOGO bit 7 */
    case 0x3C:b->count=v;return 1;
    case 0x40:case 0x44:set64(&b->srcd,off,v);return 1;
    case 0x48:case 0x4C:set64(&b->dstd,off,v);return 1;
    case 0x50:case 0x54:set64(&b->dstz,off,v);return 1;
    case 0x58:case 0x5C:set64(&b->srcz1,off,v);return 1;
    case 0x60:case 0x64:set64(&b->srcz2,off,v);return 1;
    case 0x68:case 0x6C:set64(&b->patd,off,v);return 1;
    case 0x70:b->iinc=v;return 1; case 0x74:b->zinc=v;return 1;
    case 0x78:b->stop=v;if(v&2u)b->stopped=0;return 1;
    case 0x7C:case 0x80:case 0x84:case 0x88:{ /* I0..I3: 8.16 into lane k (p.77) */
        unsigned k=((off&~3u)-0x7Cu)/4u,sh=48u-16u*k;
        b->patd=(b->patd&~(0xFFull<<sh))|((uint64_t)((v>>16)&0xFFu)<<sh);
        b->srcd=(b->srcd&~(0xFFFFull<<sh))|((uint64_t)(v&0xFFFFu)<<sh);return 1;}
    case 0x8C:case 0x90:case 0x94:case 0x98:{ /* Z0..Z3: 16.16 */
        unsigned k=((off&~3u)-0x8Cu)/4u,sh=48u-16u*k;
        b->srcz1=(b->srcz1&~(0xFFFFull<<sh))|((uint64_t)(v>>16)<<sh);
        b->srcz2=(b->srcz2&~(0xFFFFull<<sh))|((uint64_t)(v&0xFFFFu)<<sh);return 1;}
    }
    return 0;
}
