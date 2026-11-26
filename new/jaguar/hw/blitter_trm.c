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
    case 0x38:
        /* Rev.8 B_CMD readback: global IDLE (bit 0), inner IDLE (bit 2)
         * and outer IDLE (bit 11) assert only when work completes. The
         * opt-in scheduler clears all three during a pending transfer and
         * exposes STOPPED (bit 1) on collision. Detailed inner/outer busy
         * microstate readback is not yet modelled. */
        *v=b->stopped?2u:(b->running?0u:((1u<<11)|(1u<<2)|1u));
        return 1;
    /* bug 10 (p.135): pointer registers read back at F02204 / F0222C */
    case 0x04:*v=b->a1_pixel;return 1;
    case 0x2C:*v=b->a2_pixel;return 1;
    case 0x0C:*v=b->a1_pixel;return 1;  /* documented address also answered */
    case 0x30:*v=b->a2_pixel;return 1;
    case 0x18:*v=b->a1_fpixel;return 1;
    default:*v=0;return off<0xA0u;
    }
}

typedef JagBlitGen Gen;

static void gen_setup(Gen *g,uint32_t base,uint32_t flags,uint32_t pixel,uint32_t frac)
{
    static const uint32_t pm[4]={1,2,4,3}; /* p.70 pitch 0,1,2,3 -> 1,2,4,3 phrases */
    g->base=base&~7u;g->flags=flags;g->psize_log=(flags>>3)&7u;if(g->psize_log>5)g->psize_log=5;
    g->pbits=1u<<g->psize_log;g->width=jag_blitter_width(flags);g->pitch_mul=pm[flags&3u];
    g->zoff=(flags>>6)&7u;
    g->x=(int16_t)(pixel&0xFFFFu);g->y=(int16_t)(pixel>>16);g->fx=frac&0xFFFFu;g->fy=frac>>16;
    g->addr_mask=0xFFFFFFFFu;g->mask_enabled=0;
}
static uint32_t gen_pixel_bits(const Gen *g,int32_t x,int32_t y,uint32_t *addr)
{   /* p.66: Y as 12-bit unsigned, X as unsigned 0..32767 */
    /* Atari TRM rev.8 A2_MASK: A2_FLAGS bit 15 masks the X/Y coordinates
     * used by the address generator. Keep the unmasked pointer registers
     * and their steps intact; a wrapping pattern must still advance its
     * logical pointer across the whole destination rectangle. A1 has no
     * corresponding mask bit. */
    if(g->mask_enabled){
        x=(int32_t)((uint16_t)x&(uint16_t)g->addr_mask);
        y=(int32_t)((uint16_t)y&(uint16_t)(g->addr_mask>>16));
    }
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
        /* In CRY mode the 4-bit C/R and 8-bit Y fields receive their
         * own signed offsets and saturate independently. A wrapped nibble
         * would turn a slightly brighter C/R value into the opposite
         * chroma at the 0/F boundary. Reuse the Jaguar-space RMW datapath
         * shared with the original game's HUD rather than host RGB math. */
        return avp_jaguar_rmw_blend_cry((u16)d,(u16)s);
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

/* A phrase read loads all of its lanes together. A later destination
 * phrase may still consume the trailing lanes of that previous read; do not
 * fetch them again after an overlapping destination write has changed RAM.
 * Two entries suffice when source/destination pixel sizes match (one 64-bit
 * destination phrase spans at most two source phrases). These are logical
 * read-before-write snapshots, not hardware bus transactions or cycles. */
typedef JagBlitSourcePhrase JagSourcePhrase;

static const JagSourcePhrase *source_phrase_read(JagBlitter *b,const Gen *src,
    JagSourcePhrase cache[2],unsigned *next_slot,int32_t x,int32_t y,
    uint32_t pixels_per_phrase,int with_z)
{
    int32_t base_x=(int32_t)((uint32_t)x&~(pixels_per_phrase-1u));
    for(unsigned i=0;i<2u;i++)
        if(cache[i].valid&&cache[i].x==base_x&&cache[i].y==y)
            return &cache[i];
    JagSourcePhrase *entry=&cache[(*next_slot)++&1u];
    entry->valid=1;entry->x=base_x;entry->y=y;
    /* TOM performs its enabled source-color and source-Z accesses as
     * separate phrase reads.  Keep each color phrase together before
     * fetching its Z phrase; interleaving per-pixel reads gives a wrong
     * observable bus order even in this synchronous (non-cycle-exact) model.
     * Snapshot both before the first destination write. */
    for(uint32_t i=0;i<pixels_per_phrase;i++)
        entry->color[i]=rd_pix(b,src,base_x+(int32_t)i,y,0);
    if(with_z)
        for(uint32_t i=0;i<pixels_per_phrase;i++)
            entry->z[i]=rd_pix(b,src,base_x+(int32_t)i,y,1);
    return entry;
}

static void run_blit(JagBlitter *b,int resume,uint32_t groups)
{
    Gen a1,a2,*dst,*src;
    uint32_t inner=b->count&0xFFFFu,outer=b->count>>16;
    uint32_t cmd=b->cmd,lfuf=(cmd>>21)&15u,zmode=(cmd>>18)&7u;
    int dsta2=(cmd&C_DSTA2)!=0;
    if(!inner)inner=65536u;
    if(!outer)outer=65536u;
    if(resume){
        a1=b->pause.a1;
        a2=b->pause.a2;
    }else{
        gen_setup(&a1,b->a1_base,b->a1_flags,b->a1_pixel,b->a1_fpixel);
        gen_setup(&a2,b->a2_base,b->a2_flags,b->a2_pixel,0);
        a2.addr_mask=b->a2_mask;
        a2.mask_enabled=(b->a2_flags&(1u<<15))!=0u;
        /* SRCSHADE without GOURZ is a documented silicon error. */
        if((cmd&C_SRCSHADE)&&!(cmd&C_GOURZ))b->unsupported|=JB_UNSUP_SRCSHADE;
        if(b->on_blit)b->on_blit(b->on_blit_ctx,b);
        record(b);b->blits++;
        {uint32_t risk=jag_blitter_errata_risks(b);
         b->errata_risks|=risk;
         for(unsigned bit=0u;bit<4u;bit++)if(risk&(1u<<bit))b->errata_counts[bit]++;}
    }
    dst=dsta2?&a2:&a1;src=dsta2?&a1:&a2;
    b->stopped=0;
    b->pause.active=0;
    for(uint32_t o=resume?b->pause.outer_index:0u;o<outer;o++){
        uint32_t left=(resume&&o==b->pause.outer_index)?b->pause.left:inner;
        uint32_t bitc=(resume&&o==b->pause.outer_index)?b->pause.bitc:0u;
        uint32_t dxc=(dst->flags>>16)&3u,sxc=(src->flags>>16)&3u;
        int phrase=dxc==0u;
        uint32_t ppp=64u/dst->pbits,sppp=64u/src->pbits;
        /* logical source cursor (data alignment); the register follows the
           hardware pointer update rule below */
        int32_t scx=(resume&&o==b->pause.outer_index)?b->pause.scx:src->x;
        int32_t scy=(resume&&o==b->pause.outer_index)?b->pause.scy:src->y;
        /* Preserved across the destination phrases of this inner loop;
         * reset at the next row, which may start at a new source position. */
        JagSourcePhrase src_cache[2]={{0}};
        unsigned src_cache_next=0u;
        if(resume&&o==b->pause.outer_index){
            memcpy(src_cache,b->pause.source_cache,sizeof(src_cache));
            src_cache_next=b->pause.source_cache_next;
        }
        /* SRCENX is a separate source-data fetch at the start of the inner
         * loop, not merely an address-pointer increment. For the supported
         * >=8bpp, equal-size, unmasked phrase path, prefetch the current and
         * following 64-bit source phrases before any destination write.
         * They stay in the same two-entry cache used for alignment/carry.
         * SRCENZ adds a corresponding Z read even if no Z comparison/write
         * consumes that data. We model read ordering, not exact bus ticks. */
        if(!(resume&&o==b->pause.outer_index)&&
           (cmd&(C_SRCEN|C_SRCENX))==(C_SRCEN|C_SRCENX)&&
           dxc==0u&&sxc==0u&&dst->pbits==src->pbits&&
           src->pbits>=8u&&!src->mask_enabled){
            const int with_z=(cmd&C_SRCENZ)!=0u;
            int32_t first=(int32_t)((uint32_t)scx&~(sppp-1u));
            int32_t second=(int32_t)((uint32_t)first+sppp);
            (void)source_phrase_read(b,src,src_cache,&src_cache_next,
                                     first,scy,sppp,with_z);
            (void)source_phrase_read(b,src,src_cache,&src_cache_next,
                                     second,scy,sppp,with_z);
        }
        /* Both generators are in phrase mode. The first destination phrase
         * may straddle two source phrases when its starting source lane is
         * deeper than the destination lane. Real TOM requires SRCENX for
         * the additional source fetch (TRM rev.8, phrase alignment).
         * This synchronous reference reads arbitrary pixels directly and
         * cannot reconstruct the silicon's missing/stale phrase. Record the
         * invalid setup, without manufacturing pixel corruption. */
        if(!(resume&&o==b->pause.outer_index)&&
           (cmd&C_SRCEN)&&!(cmd&C_SRCENX)&&dxc==0u&&sxc==0u&&
           dst->pbits==src->pbits){
            uint32_t dstart=(uint32_t)dst->x&(ppp-1u);
            uint32_t sstart=(uint32_t)scx&(sppp-1u);
            uint32_t first=ppp-dstart;
            if(first>inner)first=inner;
            if(sstart+first>sppp)b->unsupported|=JB_UNSUP_SRCENX_ALIGN;
        }
        if(!(resume&&o==b->pause.outer_index)&&
           (cmd&C_SRCENX)&&(cmd&C_SRCEN)&&sxc==0u){src->x=(int16_t)((src->x&~(int32_t)(sppp-1u))+(int32_t)sppp);}
        while(left){
            int stop_after=0;
            uint32_t n=1,k0;
            if(phrase){k0=(uint32_t)dst->x&(ppp-1u);n=ppp-k0;if(n>left)n=left;}
            /* TOM phrase reads precede the corresponding destination phrase
             * write.  Reading each source pixel immediately before writing
             * it incorrectly feeds back earlier writes if the source and
             * destination windows overlap inside one phrase.  Snapshot only
             * the source cycles enabled by the command, and leave pixel mode
             * untouched.  This is data-result fidelity, not a simulation of
             * the physical 64-bit source latch or bus-cycle arbitration. */
            uint32_t source_latch[64],source_z_latch[64];
            const int latch_source=phrase&&sxc==0u&&(cmd&C_SRCEN);
            /* SRCENZ is a *memory-cycle enable*, not merely an option to
             * obtain Z if a comparison or destination Z write needs it.
             * For the documented 16-bpp Z path, read source Z on every
             * enabled source transfer even with ZMODE=0, DSTWRZ=0, or
             * GOURZ selecting computed Z for the result. Other Z pixel
             * formats retain the previous narrow functional behavior. */
            const int source_z_cycle=(cmd&(C_SRCEN|C_SRCENZ))==
                         (C_SRCEN|C_SRCENZ)&&src->pbits==16u&&dst->pbits==16u;
            const int latch_z=latch_source&&source_z_cycle;
            /* Phrase cache needs equal lane sizes, >=8bpp, and an unmasked
             * source to be able to key one physical source phrase by X/Y.
             * Other combinations retain the R138 per-destination snapshot. */
            const int carry_source=latch_source&&src->pbits==dst->pbits&&
                                   src->pbits>=8u&&!src->mask_enabled;
            if(latch_source){
                for(uint32_t j=0;j<n;j++){
                    if(carry_source){
                        int32_t x=scx+(int32_t)j;
                        const JagSourcePhrase *ph=source_phrase_read(b,src,
                            src_cache,&src_cache_next,x,scy,sppp,latch_z);
                        uint32_t index=(uint32_t)x&(sppp-1u);
                        source_latch[j]=ph->color[index];
                        if(latch_z)source_z_latch[j]=ph->z[index];
                    }else{
                        source_latch[j]=rd_pix(b,src,scx+(int32_t)j,scy,0);
                    }
                }
                /* For layouts without a reusable phrase cache (including
                 * masked A2), read the complete color span before its Z
                 * span; no destination writes have started yet. */
                if(latch_z&&!carry_source)
                    for(uint32_t j=0;j<n;j++)
                        source_z_latch[j]=rd_pix(b,src,scx+(int32_t)j,scy,1);
            }
            for(uint32_t j=0;j<n;j++){
                int32_t dx=phrase?dst->x+(int32_t)j:dst->x,dy=dst->y;
                int32_t sx=sxc==0u?scx+(int32_t)j:(sxc==1u?src->x+(((src->flags>>19)&1u)?-(int32_t)j:(int32_t)j):src->x),sy=sxc==0u?scy:src->y;
                uint32_t k=((uint32_t)dx)&(ppp-1u);
                uint32_t s,d,w,inhibit=0,read_source_z=0u,read_destination_z=0u;
                s=(cmd&C_SRCEN)?(latch_source?source_latch[j]:rd_pix(b,src,sx,sy,0)):
                    lane(b->srcd,dst->pbits,k);
                /* Source Z bus reads follow source color reads and precede
                 * destination data accesses. In phrase mode they were
                 * snapshotted before the first destination write; in pixel
                 * mode perform the source Z read here, exactly once. */
                if(source_z_cycle)
                    read_source_z=latch_z?source_z_latch[j]:rd_pix(b,src,sx,sy,1);
                if((cmd&C_SRCSHADE)&&(cmd&C_GOURZ)&&(cmd&C_SRCEN)&&src->pbits==16u)
                    /* source intensity + IINC integer part, saturating (p.75, p.81) */
                    s=avp_jaguar_srcshade_cry((u16)s,(int)(int8_t)((b->iinc>>16)&0xFFu));
                d=(cmd&C_DSTEN)?rd_pix(b,dst,dx,dy,0):lane(b->dstd,dst->pbits,k);
                /* DSTENZ is a destination-Z bus read enable, independent of
                 * ZMODE, DSTWRZ, GOURZ, and earlier color/bit inhibits.  In
                 * 16-bpp mode fetch this Z pixel exactly once alongside the
                 * other enabled destination data cycles, then reuse it if a
                 * Z comparison is actually requested.  The byte-addressed
                 * bus models access order, not Jaguar bus-cycle timing. */
                if((cmd&C_DSTENZ)&&dst->pbits==16u)
                    read_destination_z=rd_pix(b,dst,dx,dy,1);
                if(cmd&C_GOURD)w=lane(b->patd,dst->pbits,k);
                else if(cmd&C_PATDSEL)w=lane(b->patd,dst->pbits,k);
                else if((cmd&C_ADDDSEL)&&dst->pbits==16u)w=add16(cmd,s,d,&b->unsupported);
                else w=lfu(lfuf,s,d)&(dst->pbits==32u?0xFFFFFFFFu:((1u<<dst->pbits)-1u));
                if(cmd&C_CLIP_A1){ /* p.71/p.74: A1 outside its window inhibits writes */
                    int32_t cw=(int32_t)(b->a1_clip&0x7FFFu),ch=(int32_t)((b->a1_clip>>16)&0x7FFFu);
                    /* With DSTA2, A1 is the SOURCE: clip each computed
                     * source pixel, including interior lanes of an A2
                     * destination phrase. Clipping the unchanged A1 base
                     * pointer would incorrectly admit the entire phrase. */
                    int32_t ax=dsta2?sx:dx,ay=dsta2?sy:dy;
                    if(ax<0||ay<0||ax>=cw||ay>=ch)inhibit=2;
                }
                if(!inhibit&&(cmd&C_BCOMPEN)){ /* bit comparator: counter cleared per inner loop */
                    uint32_t bit;
                    if((cmd&C_SRCEN)&&sxc==2u&&src->pbits==8u){
                        /* TOM character painting with a stationary 8-bit
                         * source: the bit comparator selects one mask bit
                         * per output pixel using its inner-loop counter.
                         * Using (s & 1) repeated the final bit eight times.
                         * Keep the existing paths for other source-address
                         * modes until their register-load timing is traced. */
                        bit=(s>>(7u-(bitc&7u)))&1u;
                    }else if(cmd&C_SRCEN){
                        bit=s&1u;
                    }else{
                        bit=(uint32_t)((b->srcd>>(63u-(bitc&63u)))&1u);
                    }
                    if(!bit)inhibit=1;
                }
                if(!inhibit&&(cmd&C_DCOMPEN)&&(dst->pbits==8u||dst->pbits==16u)){
                    uint32_t cv=(cmd&C_CMPDST)?d:s; if(cv==lane(b->patd,dst->pbits,k))inhibit=1;
                }
                if(!inhibit&&zmode&&dst->pbits==16u){
                    uint32_t sz=(cmd&C_GOURZ)?lane(b->srcz1,16,k):
                        (((cmd&(C_SRCEN|C_SRCENZ))==(C_SRCEN|C_SRCENZ))?
                         (source_z_cycle?read_source_z:rd_pix(b,src,sx,sy,1)):
                         lane(b->srcz1,16,k));
                    uint32_t dz=(cmd&C_DSTENZ)?read_destination_z:lane(b->dstz,16,k);
                    if(((zmode&1u)&&sz<dz)||((zmode&2u)&&sz==dz)||((zmode&4u)&&sz>dz))inhibit=1;
                    else if(cmd&C_DSTWRZ)wr_pix(b,dst,dx,dy,sz,1);
                }else if(!inhibit&&(cmd&C_DSTWRZ)&&dst->pbits==16u){
                    /* Z writes are independent of enabling Z comparison.
                     * SRCENZ only fetches source Z when SRCEN is also set;
                     * GOURZ selects the computed SRCZ1 lane instead. */
                    uint32_t sz=(cmd&C_GOURZ)?lane(b->srcz1,16,k):
                        (((cmd&(C_SRCEN|C_SRCENZ))==(C_SRCEN|C_SRCENZ))?
                         (source_z_cycle?read_source_z:rd_pix(b,src,sx,sy,1)):
                         lane(b->srcz1,16,k));
                    wr_pix(b,dst,dx,dy,sz,1);
                }
                /* Atari B_STOP: only bit/data/Z comparator write inhibits,
                 * in pixel mode with destination XADD=1 and BKGWREN=0.
                 * A1_CLIP by itself is not a collision trigger. The rejected
                 * pixel is consumed; RESUME proceeds to the next pixel. */
                if(inhibit==1&&(b->stop&4u)&&dxc==1u&&!(cmd&C_BKGWREN))
                    stop_after=1;
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
            if(stop_after){
                /* All per-pixel increments are applied exactly once. Keep
                 * outer-loop updates pending when the last pixel inhibits. */
                b->stopped=1;
                b->pause.active=1;
                b->pause.a1=a1;b->pause.a2=a2;
                b->pause.outer_index=o;b->pause.left=left;
                b->pause.bitc=bitc;b->pause.scx=scx;b->pause.scy=scy;
                b->pause.source_cache_next=src_cache_next;
                memcpy(b->pause.source_cache,src_cache,sizeof(src_cache));
                b->a1_pixel=((uint32_t)(uint16_t)a1.y<<16)|(uint16_t)a1.x;
                b->a1_fpixel=(a1.fy<<16)|a1.fx;
                b->a2_pixel=((uint32_t)(uint16_t)a2.y<<16)|(uint16_t)a2.x;
                return;
            }
            /* Progressive mode yields only AFTER a complete destination
             * group and its pointer/Gouraud increments. Keep phrase carry,
             * bit-comparator count, and the pending row-end update intact. */
            if(groups!=UINT32_MAX && --groups==0u &&
               (left!=0u || o+1u<outer)){
                b->pause.active=1;
                b->pause.a1=a1;b->pause.a2=a2;
                b->pause.outer_index=o;b->pause.left=left;
                b->pause.bitc=bitc;b->pause.scx=scx;b->pause.scy=scy;
                b->pause.source_cache_next=src_cache_next;
                memcpy(b->pause.source_cache,src_cache,sizeof(src_cache));
                b->a1_pixel=((uint32_t)(uint16_t)a1.y<<16)|(uint16_t)a1.x;
                b->a1_fpixel=(a1.fy<<16)|a1.fx;
                b->a2_pixel=((uint32_t)(uint16_t)a2.y<<16)|(uint16_t)a2.x;
                return;
            }
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
    b->pause.active=0;
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
    case 0x38:
        /* A collision-stopped Blitter has a pending transfer. Do not replace
         * it with an unrelated command; control it through B_STOP. */
        if(b->stopped||b->running)return 1;
        b->cmd=v;
        if(!(v&0x80u)){
            if(b->async_mode){b->running=1;b->pending_start=1;b->pause.active=0;}
            else run_blit(b,0,UINT32_MAX);
        }
        return 1; /* NOGO bit 7 */
    case 0x3C:b->count=v;return 1;
    case 0x40:case 0x44:set64(&b->srcd,off,v);return 1;
    case 0x48:case 0x4C:set64(&b->dstd,off,v);return 1;
    case 0x50:case 0x54:set64(&b->dstz,off,v);return 1;
    case 0x58:case 0x5C:set64(&b->srcz1,off,v);return 1;
    case 0x60:case 0x64:set64(&b->srcz2,off,v);return 1;
    case 0x68:case 0x6C:set64(&b->patd,off,v);return 1;
    case 0x70:b->iinc=v;return 1; case 0x74:b->zinc=v;return 1;
    case 0x78:
        b->stop=v&4u; /* resume/abort are write-one command bits, not latches */
        if((b->stopped&&b->pause.active)||b->running){
            if(v&2u){ /* ABORT takes precedence if both command bits set. */
                b->stopped=0;b->pause.active=0;
                b->running=0;b->pending_start=0;
            }else if(b->stopped&&(v&1u)){
                b->stopped=0;
                if(b->async_mode){b->running=1;b->pending_start=0;}
                else run_blit(b,1,UINT32_MAX);
            }
        }
        return 1;
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

/* R286: an opt-in experiment for scheduler-visible BUSY, staged real RAM
 * writes, stopped-vs-completed IRQ semantics, and preserved phrase latches.
 * Exactly one destination group costs two *model* system ticks; physical
 * bus cycles, OP priority and memory wait states remain unmodelled. */
void jag_blitter_set_async(JagBlitter *b,int enable)
{
    if(!b->running&&!b->stopped)b->async_mode=enable!=0;
}
int jag_blitter_advance(JagBlitter *b,unsigned groups)
{
    int resume;
    if(!b->async_mode||!b->running||!groups)return 0;
    resume=!b->pending_start;
    b->pending_start=0;
    run_blit(b,resume,groups);
    if(b->stopped){b->running=0;return 0;}
    if(b->pause.active)return 0;
    b->running=0;
    return 1;
}
