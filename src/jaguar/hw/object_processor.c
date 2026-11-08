#include "jaguar/object_processor.h"
#include "jaguar/cry.h"
#include <stdlib.h>
#include <string.h>

/* Atari Jaguar Technical Reference Manual, Rev. 8, pp. 18-19:
 * BITMAP objects fetch whole eight-byte phrases and advance the source
 * address by DWIDTH phrases for each source line.  For this contiguous
 * HILO=1/4-bpp subset, DWIDTH = ceil(source_width / 16).
 * Bounds checking prevents reading past an incomplete brush. */
u8 avp_jaguar_op4_contiguous_pixel(const u8 *src,size_t source_bytes,
                                   unsigned source_width,unsigned x,unsigned y)
{
    const unsigned dwidth=(unsigned)(((size_t)source_width+15u)/16u);
    /* A 10-bit OP DWIDTH is sufficient for the supported contiguous 4-bpp
     * source width. Nonrepresentable widths cannot describe an OP object. */
    if(dwidth>1023u)return 0;
    return avp_jaguar_op4_phrased_pixel(src,source_bytes,source_width,
                                       dwidth,1u,x,y);
}

int avp_jaguar_op_decode_bitmap_words(const u32 *words, size_t word_count,
                                      AvpJaguarOpBitmapHeader *out)
{
    uint64_t p0,p1;
    unsigned type,x;
    AvpJaguarOpBitmapHeader b;
    if(!words||!out||word_count<4u)return 0;
    p0=((uint64_t)words[0]<<32)|words[1];
    type=(unsigned)(p0&7u);
    if(type>1u||(type==1u&&word_count<6u))return 0;
    p1=((uint64_t)words[2]<<32)|words[3];
    memset(&b,0,sizeof(b));
    b.type=type;
    b.ypos=(unsigned)((p0>>3)&0x7ffu);
    b.height=(unsigned)((p0>>14)&0x3ffu);
    b.link_address=(u32)(((p0>>24)&0x7ffffu)<<3);
    b.data_address=(u32)(((p0>>43)&0x1fffffu)<<3);
    x=(unsigned)(p1&0xfffu);
    b.xpos=(x&0x800u)?(int)x-4096:(int)x;
    b.depth=(unsigned)((p1>>12)&7u);
    b.pitch=(unsigned)((p1>>15)&7u);
    b.dwidth=(unsigned)((p1>>18)&0x3ffu);
    b.iwidth=(unsigned)((p1>>28)&0x3ffu);
    b.index=(unsigned)((p1>>38)&0x7fu);
    b.reflect=(unsigned)((p1>>45)&1u);
    b.rmw=(unsigned)((p1>>46)&1u);
    b.trans=(unsigned)((p1>>47)&1u);
    b.release=(unsigned)((p1>>48)&1u);
    b.firstpix=(unsigned)((p1>>49)&0x3fu);
    if(type==1u){
        const uint64_t p2=((uint64_t)words[4]<<32)|words[5];
        b.hscale=(unsigned)(p2&255u);
        b.vscale=(unsigned)((p2>>8)&255u);
        b.remainder=(unsigned)((p2>>16)&255u);
    }
    /* The manual requires a nonzero image width for bitmap objects. */
    if(!b.iwidth)return 0;
    *out=b;
    return 1;
}

int avp_jaguar_op_decode_branch_words(const u32 *words,size_t word_count,
                                      AvpJaguarOpBranchHeader *out)
{
    uint64_t phrase;
    AvpJaguarOpBranchHeader b;
    if(!words||!out||word_count<2u)return 0;
    phrase=((uint64_t)words[0]<<32)|words[1];
    if((phrase&7u)!=3u)return 0;
    b.ypos=(unsigned)((phrase>>3)&0x7ffu);
    b.condition=(unsigned)((phrase>>14)&3u);
    b.link_address=(u32)(((phrase>>24)&0x7ffffu)<<3);
    *out=b;
    return 1;
}

int avp_jaguar_op_branch_taken(const AvpJaguarOpBranchHeader *branch,
                               unsigned vertical_count,unsigned obf)
{
    unsigned vc;
    if(!branch||branch->condition>3u)return 0;
    vc=vertical_count&0x7ffu;
    switch(branch->condition){
    case 0u:return branch->ypos==vc||branch->ypos==0x7ffu;
    case 1u:return branch->ypos>vc;
    case 2u:return branch->ypos<vc;
    case 3u:return (obf&1u)!=0u;
    default:return 0;
    }
}

/* Jaguar RAM words are BIG-ENDIAN even when the PC host is not.
 * Reject out-of-window reads (including a short scaled-object header).
 * Using subtraction also avoids offset+length overflow. */
static int op_read_words(const u8 *memory,size_t size,u32 base,u32 addr,
                         u32 *words,size_t count)
{
    size_t off,n,i;
    if(!memory||!words||addr<base||(addr&7u)||count>6u)return 0;
    off=(size_t)(addr-base);
    n=count*4u;
    if(off>size||n>size-off)return 0;
    for(i=0;i<count;i++){
        const u8 *v=memory+off+i*4u;
        words[i]=((u32)v[0]<<24)|((u32)v[1]<<16)|((u32)v[2]<<8)|v[3];
    }
    return 1;
}

AvpJaguarOpTraceResult avp_jaguar_op_trace_scanline(
    const u8 *memory,size_t memory_size,u32 memory_base,u32 object_list_address,
    unsigned vertical_count,unsigned object_flag,
    AvpJaguarOpTraceStep *steps,size_t step_capacity)
{
    AvpJaguarOpTraceResult result;
    u32 pc=object_list_address;
    size_t n;
    result.status=AVP_JAG_OP_TRACE_BAD_INPUT;
    result.count=0u;
    result.address=pc;
    if(!memory||(!steps&&step_capacity)||!step_capacity)return result;
    for(n=0;n<step_capacity;n++){
        u32 words[6]={0};
        unsigned type;
        AvpJaguarOpTraceStep step;
        AvpJaguarOpBitmapHeader bitmap;
        AvpJaguarOpBranchHeader branch;
        size_t j;
        /* A linked list that repeats an address cannot finish a scanline.
         * Stop before adding a duplicate, rather than looping forever. */
        for(j=0;j<result.count;j++)if(steps[j].address==pc){
            result.status=AVP_JAG_OP_TRACE_CYCLE;
            result.address=pc;
            return result;
        }
        if(!op_read_words(memory,memory_size,memory_base,pc,words,2u)){
            result.status=AVP_JAG_OP_TRACE_OUT_OF_RANGE;
            result.address=pc;
            return result;
        }
        type=words[1]&7u;
        if(type>4u){
            result.status=AVP_JAG_OP_TRACE_INVALID_OBJECT;
            result.address=pc;
            return result;
        }
        memset(&step,0,sizeof(step));
        step.address=pc;
        step.type=type;
        if(type<=1u){
            /* Bitmap uses two phrases; scaled bitmap uses three. */
            if(!op_read_words(memory,memory_size,memory_base,pc,words,
                              type?6u:4u)){
                result.status=AVP_JAG_OP_TRACE_OUT_OF_RANGE;
                result.address=pc;
                return result;
            }
            if(!avp_jaguar_op_decode_bitmap_words(words,type?6u:4u,&bitmap)){
                result.status=AVP_JAG_OP_TRACE_INVALID_OBJECT;
                result.address=pc;
                return result;
            }
            step.active=(vertical_count&0x7ffu)>=bitmap.ypos && bitmap.height>0u;
            step.next_address=bitmap.link_address;
        }else if(type==2u){
            /* GPU interrupt objects occupy a single phrase. No OP-side
             * memory writes occur until the GPU services this request. */
            const unsigned ypos=(words[1]>>3)&0x7ffu;
            step.active=(ypos==0x7ffu || ypos==(vertical_count&0x7ffu));
            if(pc>UINT32_MAX-8u){
                result.status=AVP_JAG_OP_TRACE_OUT_OF_RANGE;
                result.address=pc;
                return result;
            }
            step.next_address=pc+8u;
        }else if(type==3u){
            if(!avp_jaguar_op_decode_branch_words(words,2u,&branch)){
                result.status=AVP_JAG_OP_TRACE_INVALID_OBJECT;
                result.address=pc;
                return result;
            }
            step.branch_taken=(unsigned)avp_jaguar_op_branch_taken(&branch,
                                vertical_count,object_flag);
            if(step.branch_taken)step.next_address=branch.link_address;
            else{
                if(pc>UINT32_MAX-8u){
                    result.status=AVP_JAG_OP_TRACE_OUT_OF_RANGE;
                    result.address=pc;
                    return result;
                }
                step.next_address=pc+8u;
            }
        }else{
            /* STOP is terminal and contains a phrase of uninterpreted data. */
            step.next_address=pc;
        }
        steps[result.count++]=step;
        result.address=pc;
        if(type==4u){result.status=AVP_JAG_OP_TRACE_STOP;return result;}
        if(type==2u&&step.active){
            result.status=AVP_JAG_OP_TRACE_GPU;return result;
        }
        pc=step.next_address;
        result.address=pc;
    }
    result.status=AVP_JAG_OP_TRACE_LIMIT;
    return result;
}

u8 avp_jaguar_op4_phrased_pixel(const u8 *src,size_t source_bytes,
                                unsigned width,unsigned dwidth_phrases,
                                unsigned pitch_phrases,unsigned x,unsigned y)
{
    size_t row,phrase,at;
    u8 packed;
    if(!src||!width||x>=width||pitch_phrases>7u||dwidth_phrases>1023u)return 0;
    /* Overflow-safe phrase address: DATA + y*DWIDTH + (x/16)*PITCH.
     * These are physical 64-bit phrase steps, *not* packed bytes per row. */
    if(dwidth_phrases && (size_t)y>SIZE_MAX/(size_t)dwidth_phrases)return 0;
    row=(size_t)y*dwidth_phrases;
    if(pitch_phrases && (size_t)(x/16u)>SIZE_MAX/(size_t)pitch_phrases)return 0;
    phrase=(size_t)(x/16u)*pitch_phrases;
    if(row>SIZE_MAX-phrase)return 0;
    phrase+=row;
    if(phrase>SIZE_MAX/8u)return 0;
    at=phrase*8u+(size_t)((x%16u)/2u);
    if(at>=source_bytes)return 0;
    packed=src[at];
    return (u8)((x&1u)?(packed&15u):(packed>>4));
}

/* The physical TOM line buffer is 360 x 32 bits: two CRY16 pixels
 * per longword. The pointer below addresses these physical 16-bit pixels
 * directly; host RGB conversion and VBlank belong to a separate stage.
 * This standalone primitive deliberately does not modify the OP header's
 * DATA/HEIGHT or simulate OP object-to-object scanline execution. */
AvpJaguarOpPixelResult avp_jaguar_op_compose_unscaled_cry16(
    const AvpJaguarOpBitmapHeader *h,
    const u8 *source_memory,size_t source_size,u32 source_base,
    const u16 *clut,size_t clut_entries,
    u16 *line_pixels,size_t line_width)
{
    AvpJaguarOpPixelResult result={AVP_JAG_OP_PIXEL_BAD_INPUT,0u};
    unsigned bpp,pixels_per_phrase,first_skip;
    size_t count,k;
    if(!h||!source_memory||!line_pixels||!line_width)return result;
    if(h->type!=0u||h->depth>4u||h->pitch>7u||h->iwidth>1023u||
       h->dwidth>1023u||h->firstpix>63u||h->index>127u||
       h->reflect>1u||h->rmw>1u||h->trans>1u||!h->iwidth){
        result.status=AVP_JAG_OP_PIXEL_UNSUPPORTED;
        return result;
    }
    bpp=1u<<h->depth;
    pixels_per_phrase=64u/bpp;
    /* The unscaled hardware writes pixel pairs, and FIRSTPIX bit zero
     * is reserved for scaled objects (manual p.19). The upper remaining
     * (log2(pixels/phrase)-1) bits select which PAIR starts the phrase. */
    first_skip=(h->firstpix>>(h->depth+1u))*2u;
    count=(size_t)h->iwidth*pixels_per_phrase;
    if(first_skip>=pixels_per_phrase||first_skip>=count){
        result.status=AVP_JAG_OP_PIXEL_UNSUPPORTED;
        return result;
    }
    count-=first_skip;
    if(bpp<=8u&&(!clut||clut_entries<256u))return result;
    if(bpp==16u&&h->trans){
        result.status=AVP_JAG_OP_PIXEL_UNSUPPORTED;
        return result;
    }
    /* This preflight covers every source phrase that could write into
     * the visible line buffer, so out-of-window source reads never leave
     * a half-updated destination. The physical source memory window is
     * independent from the already-decoded object-list header window. */
    for(k=0u;k<count;k++){
        const int64_t dest=(int64_t)h->xpos+(h->reflect?-(int64_t)k:(int64_t)k);
        size_t pos,phrase,byte_offset;
        uint64_t addr;
        unsigned pixel_in_phrase;
        if(dest<0||((uint64_t)dest)>=(uint64_t)line_width)continue;
        pos=k+first_skip;
        phrase=pos/pixels_per_phrase;
        pixel_in_phrase=(unsigned)(pos%pixels_per_phrase);
        addr=(uint64_t)h->data_address+(uint64_t)phrase*h->pitch*8u;
        if(addr<(uint64_t)source_base||addr-(uint64_t)source_base>(uint64_t)SIZE_MAX){
            result.status=AVP_JAG_OP_PIXEL_OUT_OF_RANGE;return result;
        }
        byte_offset=(size_t)(addr-(uint64_t)source_base)+(size_t)(pixel_in_phrase*bpp)/8u;
        if(byte_offset>=source_size||(bpp==16u&&source_size-byte_offset<2u)){
            result.status=AVP_JAG_OP_PIXEL_OUT_OF_RANGE;return result;
        }
    }
    for(k=0u;k<count;k++){
        const int64_t dest=(int64_t)h->xpos+(h->reflect?-(int64_t)k:(int64_t)k);
        size_t pos,phrase,byte_offset;
        u16 src;
        unsigned pixel_in_phrase;
        if(dest<0||((uint64_t)dest)>=(uint64_t)line_width)continue;
        pos=k+first_skip;
        phrase=pos/pixels_per_phrase;
        pixel_in_phrase=(unsigned)(pos%pixels_per_phrase);
        byte_offset=(size_t)((uint64_t)h->data_address-(uint64_t)source_base+
                    (uint64_t)phrase*h->pitch*8u)+(size_t)(pixel_in_phrase*bpp)/8u;
        if(bpp==16u){
            src=(u16)(((u16)source_memory[byte_offset]<<8)|source_memory[byte_offset+1u]);
        }else{
            const unsigned group=8u/bpp;
            const unsigned shift=(group-1u-(pixel_in_phrase%group))*bpp;
            const unsigned raw=((unsigned)source_memory[byte_offset]>>shift)&((1u<<bpp)-1u);
            unsigned idx;
            if(h->trans&&raw==0u)continue;
            /* INDEX provides the upper 8-bpp palette address bits, with
             * the relevant bits left-aligned in its seven-bit field. */
            idx=(((h->index>>(bpp-1u))<<bpp)|raw)&255u;
            src=clut[idx];
        }
        if(h->rmw)
            line_pixels[(size_t)dest]=avp_jaguar_rmw_blend_cry(line_pixels[(size_t)dest],src);
        else
            line_pixels[(size_t)dest]=src;
        result.drawn++;
    }
    result.status=AVP_JAG_OP_PIXEL_OK;
    return result;
}

/* SCBITOBJ horizontal pixel generation.  Unlike unscaled two-pixel
 * writes, every scaled destination pixel chooses exactly one source pixel.
 * The existing AVP MJP host reconstruction uses this fixed 0x20 step for
 * title and selection object rendering.  Keeping the implementation
 * independent allows red/green hardware tests before live integration. */
AvpJaguarOpPixelResult avp_jaguar_op_compose_scaled_cry16(
    const AvpJaguarOpBitmapHeader *h,
    const u8 *source_memory,size_t source_size,u32 source_base,
    const u16 *clut,size_t clut_entries,
    u16 *line_pixels,size_t line_width)
{
    AvpJaguarOpPixelResult result={AVP_JAG_OP_PIXEL_BAD_INPUT,0u};
    unsigned bpp,pixels_per_phrase,first_skip;
    size_t source_count,output_count,k;
    unsigned pass;
    if(!h||!source_memory||!line_pixels||!line_width)return result;
    if(h->type!=1u||h->depth>4u||h->pitch>7u||h->iwidth>1023u||
       h->dwidth>1023u||h->firstpix>63u||h->index>127u||
       h->reflect>1u||h->rmw>1u||h->trans>1u||!h->iwidth||
       !h->hscale||h->hscale>255u||h->vscale>0xE0u){
        result.status=AVP_JAG_OP_PIXEL_UNSUPPORTED;
        return result;
    }
    bpp=1u<<h->depth;
    pixels_per_phrase=64u/bpp;
    /* FIRSTPIX scales at single-pixel granularity (unscaled is paired).
     * Bits below DEPTH address finer-than-a-pixel subpositions and are
     * ignored, as the source pixel must be a complete indexed value. */
    first_skip=h->firstpix>>h->depth;
    source_count=(size_t)h->iwidth*pixels_per_phrase;
    if(first_skip>=pixels_per_phrase||first_skip>=source_count){
        result.status=AVP_JAG_OP_PIXEL_UNSUPPORTED;
        return result;
    }
    source_count-=first_skip;
    if(bpp<=8u&&(!clut||clut_entries<256u))return result;
    if(bpp==16u&&h->trans){
        result.status=AVP_JAG_OP_PIXEL_UNSUPPORTED;
        return result;
    }
    /* Ceiling is necessary for the final partial source-pixel exposure.
     * All these terms are at most (1023*64*255) and fit in uint64_t. */
    output_count=(size_t)(((uint64_t)source_count*h->hscale+31u)/32u);
    /* Preflight all visible source accesses.  An invalid source window
     * must not leave the line buffer partly overwritten. */
    for(pass=0u;pass<2u;pass++)for(k=0u;k<output_count;k++){
        const int64_t dest=(int64_t)h->xpos+
                           (h->reflect?-(int64_t)k:(int64_t)k);
        size_t source_pixel,phrase,byte_offset;
        unsigned pixel_in_phrase;
        uint64_t addr;
        u16 src;
        if(dest<0||(uint64_t)dest>=(uint64_t)line_width)continue;
        source_pixel=first_skip+(size_t)(((uint64_t)k*32u)/h->hscale);
        if(source_pixel>=source_count+first_skip)continue;
        phrase=source_pixel/pixels_per_phrase;
        pixel_in_phrase=(unsigned)(source_pixel%pixels_per_phrase);
        addr=(uint64_t)h->data_address+(uint64_t)phrase*h->pitch*8u;
        if(addr<(uint64_t)source_base||addr-(uint64_t)source_base>(uint64_t)SIZE_MAX){
            result.status=AVP_JAG_OP_PIXEL_OUT_OF_RANGE;return result;
        }
        byte_offset=(size_t)(addr-(uint64_t)source_base)+
                    (size_t)(pixel_in_phrase*bpp)/8u;
        if(byte_offset>=source_size||(bpp==16u&&source_size-byte_offset<2u)){
            result.status=AVP_JAG_OP_PIXEL_OUT_OF_RANGE;return result;
        }
        if(pass==0u)continue;
        if(bpp==16u)
            src=(u16)(((u16)source_memory[byte_offset]<<8)|source_memory[byte_offset+1u]);
        else{
            const unsigned group=8u/bpp;
            const unsigned shift=(group-1u-(pixel_in_phrase%group))*bpp;
            const unsigned raw=((unsigned)source_memory[byte_offset]>>shift)&((1u<<bpp)-1u);
            unsigned idx;
            if(h->trans&&raw==0u)continue;
            idx=(((h->index>>(bpp-1u))<<bpp)|raw)&255u;
            src=clut[idx];
        }
        if(h->rmw)
            line_pixels[(size_t)dest]=avp_jaguar_rmw_blend_cry(line_pixels[(size_t)dest],src);
        else
            line_pixels[(size_t)dest]=src;
        result.drawn++;
    }
    result.status=AVP_JAG_OP_PIXEL_OK;
    return result;
}

/* TOM direct 24-bit bitmap source mode, TRM Rev.8 pp.17, 21-23.
 * Each 64-bit source phrase contains TWO complete 32-bit slots; 24 bits
 * of each slot are the physical RGB value, and one byte is unused.
 * The 24-bit OP path bypasses the indexed CLUT and produces a 32-bit
 * physical line-buffer pixel.  Do not squeeze it into a CRY16 line.
 *
 * Deliberately accepts the unscaled, FIRSTPIX=0, non-transparent,
 * non-RMW subset only.  Physical transparent codes and 24-bit RMW are
 * not established by the retail documentation.  Not wired to the host
 * renderer or to the CRY16-only scanline transaction. */
AvpJaguarOpPixelResult avp_jaguar_op_compose_unscaled_rgb24(
    const AvpJaguarOpBitmapHeader *h,
    const u8 *source_memory,size_t source_size,u32 source_base,
    u32 *line_pixels,size_t line_width)
{
    AvpJaguarOpPixelResult result={AVP_JAG_OP_PIXEL_BAD_INPUT,0u};
    size_t count,k;
    unsigned pass;
    if(!h||!source_memory||!line_pixels||!line_width)return result;
    if(h->type!=0u||h->depth!=5u||h->pitch>7u||!h->iwidth||
       h->iwidth>1023u||h->dwidth>1023u||h->firstpix!=0u||
       h->reflect>1u||h->rmw!=0u||h->trans!=0u){
        result.status=AVP_JAG_OP_PIXEL_UNSUPPORTED;
        return result;
    }
    count=(size_t)h->iwidth*2u;
    /* Validate every visible address before the first pixel write.
     * Phrase arithmetic uses 64-bit intermediates, and source window
     * subtraction avoids overflow and addresses below source_base. */
    for(pass=0u;pass<2u;pass++)for(k=0u;k<count;k++){
        const int64_t dest=(int64_t)h->xpos+
                           (h->reflect?-(int64_t)k:(int64_t)k);
        const uint64_t addr=(uint64_t)h->data_address+
                             (uint64_t)(k/2u)*h->pitch*8u+
                             (uint64_t)(k%2u)*4u;
        size_t off;
        if(dest<0||(uint64_t)dest>=(uint64_t)line_width)continue;
        if(addr<(uint64_t)source_base||
           addr-(uint64_t)source_base>(uint64_t)SIZE_MAX){
            result.status=AVP_JAG_OP_PIXEL_OUT_OF_RANGE;
            return result;
        }
        off=(size_t)(addr-(uint64_t)source_base);
        if(off>source_size||source_size-off<4u){
            result.status=AVP_JAG_OP_PIXEL_OUT_OF_RANGE;
            return result;
        }
        if(pass!=0u){
            const u16 first=(u16)(((u16)source_memory[off]<<8)|
                                   source_memory[off+1u]);
            const u16 second=(u16)(((u16)source_memory[off+2u]<<8)|
                                    source_memory[off+3u]);
            line_pixels[(size_t)dest]=avp_jaguar_vmode_rgb24(first,second);
            result.drawn++;
        }
    }
    result.status=AVP_JAG_OP_PIXEL_OK;
    return result;
}

int avp_jaguar_op_scaled_vertical_step(
    const AvpJaguarOpBitmapHeader *h,AvpJaguarOpScaledVerticalStep *out)
{
    unsigned rem,advanced=0u;
    AvpJaguarOpScaledVerticalStep step;
    if(!h||!out||h->type!=1u||!h->vscale||h->vscale>0xE0u||
       h->remainder>255u)return 0;
    rem=h->remainder;
    /* Same state ordering as AVP's original Make_TLi/Update2 phrase
     * remainder handling: advance a source line while REMAINDER<0x20,
     * then subtract one 0x20 output line.  Do not reset at clip edges. */
    while(rem<32u){rem+=h->vscale;advanced++;}
    step.next_remainder=rem-32u;
    step.source_lines_advanced=advanced;
    *out=step;
    return 1;
}

/* Preserve all 68000-visible header bits other than the three documented
 * TOM writeback fields (DATA, HEIGHT, scaled REMAINDER).  No host endian or
 * struct-layout assumptions are permitted in this path. */
int avp_jaguar_op_advance_bitmap_words(
    const u32 *words,size_t word_count,u32 *updated_words,
    unsigned *source_rows)
{
    AvpJaguarOpBitmapHeader h;
    AvpJaguarOpScaledVerticalStep vstep;
    u32 output[6];
    uint64_t first,third;
    unsigned rows,new_height,new_data_phrase,new_remainder;
    size_t count;
    if(!words||!updated_words||!avp_jaguar_op_decode_bitmap_words(
            words,word_count,&h))return 0;
    count=h.type?6u:4u;
    if(h.height==0u){
        memcpy(updated_words,words,count*sizeof(*words));
        if(source_rows)*source_rows=0u;
        return 1;
    }
    if(h.type==1u){
        if(!avp_jaguar_op_scaled_vertical_step(&h,&vstep))return 0;
        rows=vstep.source_lines_advanced;
        new_remainder=vstep.next_remainder;
    }else{
        rows=1u;
        new_remainder=0u;
    }
    /* HEIGHT is a saturating 10-bit source-line count; the DATA pointer
     * follows every consumed source line even if HEIGHT reaches zero.
     * The 21-bit DATA phrase field wraps in Jaguar's 24-bit address space. */
    new_height=(rows>=h.height)?0u:h.height-rows;
    new_data_phrase=(unsigned)(((uint64_t)(h.data_address>>3)+
                      (uint64_t)rows*h.dwidth)&0x1fffffu);
    memcpy(output,words,count*sizeof(*words));
    first=((uint64_t)words[0]<<32)|words[1];
    first &= ~(((uint64_t)0x3ffu<<14)|((uint64_t)0x1fffffu<<43));
    first |= (uint64_t)new_height<<14;
    first |= (uint64_t)new_data_phrase<<43;
    output[0]=(u32)(first>>32);
    output[1]=(u32)first;
    if(h.type==1u){
        third=((uint64_t)words[4]<<32)|words[5];
        third=(third&~((uint64_t)0xffu<<16))|
              ((uint64_t)new_remainder<<16);
        output[4]=(u32)(third>>32);
        output[5]=(u32)third;
    }
    memcpy(updated_words,output,count*sizeof(*output));
    if(source_rows)*source_rows=rows;
    return 1;
}

int avp_jaguar_op_writeback_bitmap_memory(
    u8 *memory,size_t memory_size,u32 memory_base,u32 object_address,
    unsigned *source_rows)
{
    u32 input[6]={0},output[6]={0};
    unsigned type,rows;
    size_t count,i,offset;
    if(!memory||object_address<memory_base||(object_address&7u))return 0;
    if(!op_read_words(memory,memory_size,memory_base,
                      object_address,input,2u))return 0;
    type=input[1]&7u;
    if(type>1u)return 0;
    count=type?6u:4u;
    if(!op_read_words(memory,memory_size,memory_base,
                      object_address,input,count)||
       !avp_jaguar_op_advance_bitmap_words(input,count,output,&rows))return 0;
    offset=(size_t)(object_address-memory_base);
    /* Preflight ensured all `count` longwords fit the physical RAM window. */
    for(i=0u;i<count;i++){
        const u32 v=output[i];
        memory[offset+i*4u]=(u8)(v>>24);
        memory[offset+i*4u+1u]=(u8)(v>>16);
        memory[offset+i*4u+2u]=(u8)(v>>8);
        memory[offset+i*4u+3u]=(u8)v;
    }
    if(source_rows)*source_rows=rows;
    return 1;
}

/* Combine the standalone OP primitives without changing the active host
 * renderer. The two-pass implementation is intentionally conservative:
 * unsupported pixel modes and unserviced GPU objects cannot be silently
 * rendered as though they were complete object-list execution. */
AvpJaguarOpScanlineResult avp_jaguar_op_compose_scanline_cry16(
    u8 *object_memory,size_t object_memory_size,u32 object_memory_base,
    u32 list_address,unsigned vertical_count,unsigned object_flag,
    const u8 *source_memory,size_t source_memory_size,u32 source_memory_base,
    const u16 *clut,size_t clut_entries,
    u16 *line_pixels,u16 *scratch_line,size_t line_width,
    AvpJaguarOpTraceStep *steps,size_t step_capacity)
{
    AvpJaguarOpScanlineResult r;
    AvpJaguarOpTraceResult tr;
    size_t i;
    memset(&r,0,sizeof(r));
    r.status=AVP_JAG_OP_SCANLINE_INVALID;
    r.trace_status=AVP_JAG_OP_TRACE_BAD_INPUT;
    r.stop_address=list_address;
    /* The physical TOM line buffer holds 360 32-bit words (720 CRY16
     * pixels). The scratch buffer must be distinct to support rollback. */
    if(!object_memory||!source_memory||!line_pixels||!scratch_line||
       line_pixels==scratch_line||!steps||!step_capacity||
       !line_width||line_width>720u||line_width>SIZE_MAX/sizeof(*line_pixels))
        return r;
    tr=avp_jaguar_op_trace_scanline(object_memory,object_memory_size,
         object_memory_base,list_address,vertical_count,object_flag,
         steps,step_capacity);
    r.trace_status=tr.status;
    r.objects=tr.count;
    r.stop_address=tr.address;
    if(tr.status==AVP_JAG_OP_TRACE_GPU){
        r.status=AVP_JAG_OP_SCANLINE_GPU_PENDING;
        return r;
    }
    if(tr.status!=AVP_JAG_OP_TRACE_STOP){
        r.status=AVP_JAG_OP_SCANLINE_TRACE_ERROR;
        return r;
    }
    memcpy(scratch_line,line_pixels,line_width*sizeof(*scratch_line));
    /* Preflight all active drawing and object-header writebacks. Pixel
     * helpers preflight their complete visible source window before draw.
     * No RAM or caller line-buffer writes occur in this pass. */
    for(i=0u;i<tr.count;i++){
        u32 words[6]={0},next[6]={0};
        AvpJaguarOpBitmapHeader b;
        AvpJaguarOpPixelResult p;
        unsigned t=steps[i].type;
        size_t n;
        if(t>1u||!steps[i].active)continue;
        n=t?6u:4u;
        /* Linked headers must not overlap any header visited this line.
         * Otherwise earlier writebacks could invalidate a later commit,
         * breaking our all-or-nothing non-hardware test contract. */
        {
            size_t j;
            for(j=0u;j<tr.count;j++){
                if(j!=i && (uint64_t)steps[j].address>=steps[i].address &&
                   (uint64_t)steps[j].address<(uint64_t)steps[i].address+n*4u){
                    r.status=AVP_JAG_OP_SCANLINE_INVALID;
                    return r;
                }
            }
        }
        if(!op_read_words(object_memory,object_memory_size,
            object_memory_base,steps[i].address,words,n)||
           !avp_jaguar_op_decode_bitmap_words(words,n,&b)||
           !avp_jaguar_op_advance_bitmap_words(words,n,next,NULL)){
            r.status=AVP_JAG_OP_SCANLINE_INVALID;
            return r;
        }
        if(t==0u)p=avp_jaguar_op_compose_unscaled_cry16(&b,
            source_memory,source_memory_size,source_memory_base,
            clut,clut_entries,scratch_line,line_width);
        else p=avp_jaguar_op_compose_scaled_cry16(&b,
            source_memory,source_memory_size,source_memory_base,
            clut,clut_entries,scratch_line,line_width);
        if(p.status!=AVP_JAG_OP_PIXEL_OK){
            r.status=(p.status==AVP_JAG_OP_PIXEL_OUT_OF_RANGE)?
                AVP_JAG_OP_SCANLINE_RANGE:AVP_JAG_OP_SCANLINE_UNSUPPORTED;
            return r;
        }
        r.active_bitmaps++;
        r.drawn_pixels+=p.drawn;
    }
    /* All input headers, output locations and source windows have now
     * been validated; each unique linked header can be written exactly
     * once. The prior trace rejects repeated linked-header addresses. */
    for(i=0u;i<tr.count;i++){
        if(steps[i].type<=1u&&steps[i].active){
            /* This operation cannot fail for a previously validated,
             * unchanged header in the supplied bounded memory window. */
            if(!avp_jaguar_op_writeback_bitmap_memory(object_memory,
                  object_memory_size,object_memory_base,steps[i].address,NULL)){
                r.status=AVP_JAG_OP_SCANLINE_INVALID;
                return r;
            }
        }
    }
    memcpy(line_pixels,scratch_line,line_width*sizeof(*line_pixels));
    r.status=AVP_JAG_OP_SCANLINE_STOP;
    return r;
}

/* Isolated two-bank physical CRY16 video output.  The manual states
 * that the bank swap happens at the beginning of a new display line and
 * that BGEN overwrites the bank only AFTER video has read it.  This API
 * explicitly models the order while leaving clock arbitration to a
 * future, independently verified hardware scheduler. */
int avp_jaguar_op_cry16_pair_init(AvpJaguarOpCry16LinePair *pair,
    unsigned first_write_bank,u16 bank0_value,u16 bank1_value)
{
    size_t i;
    if(!pair||first_write_bank>1u)return 0;
    for(i=0u;i<AVP_JAG_OP_CRY16_LINE_PIXELS;i++){
        pair->bank[0][i]=bank0_value;
        pair->bank[1][i]=bank1_value;
    }
    pair->write_bank=first_write_bank;
    return 1;
}

u16 *avp_jaguar_op_cry16_pair_write(AvpJaguarOpCry16LinePair *pair)
{
    if(!pair||pair->write_bank>1u)return NULL;
    return pair->bank[pair->write_bank];
}

int avp_jaguar_op_cry16_pair_publish(AvpJaguarOpCry16LinePair *pair,
    u16 *output,size_t output_count,int bgen,u16 background_cry)
{
    size_t i;
    const u16 *read_bank;
    if(!pair||!output||!output_count||
       output_count>AVP_JAG_OP_CRY16_LINE_PIXELS||pair->write_bank>1u||
       (bgen!=0&&bgen!=1))return 0;
    /* The published buffer is a separate host destination.  In-place
     * conversion would be corrupted by the BGEN clear after display. */
    {
        const uintptr_t addr=(uintptr_t)output;
        const uintptr_t lo=(uintptr_t)&pair->bank[0][0];
        const uintptr_t hi=lo+sizeof(pair->bank);
        const size_t bytes=output_count*sizeof(*output);
        /* Reject either output starting within the banks or an output
         * starting just before them and extending into bank storage. */
        if((addr>=lo&&addr<hi)||
           (addr<lo&&(uintptr_t)(lo-addr)<(uintptr_t)bytes))return 0;
    }
    read_bank=pair->bank[pair->write_bank];
    memcpy(output,read_bank,output_count*sizeof(*output));
    if(bgen){
        for(i=0u;i<AVP_JAG_OP_CRY16_LINE_PIXELS;i++)
            pair->bank[pair->write_bank][i]=background_cry;
    }
    pair->write_bank^=1u;
    return 1;
}

/* TOM video pixel path, Atari TRM Rev.8 pp.12-13 and 22-23.
 * The physical line buffer is already assembled as 16-bit words; the
 * VMODE converter does NOT reinterpret source Object Processor headers.
 * This is deliberately an off-screen test primitive, not a renderer switch.
 * The 16-bit direct output mode needs an external colour interpretation,
 * which cannot be inferred from the TOM RGB DAC modes. */
AvpJaguarOpVideoResult avp_jaguar_op_vmode_decode_line(
    const u16 *words,size_t word_count,unsigned mode,unsigned varmod,
    u32 *pixels,size_t pixel_capacity)
{
    AvpJaguarOpVideoResult result={AVP_JAG_OP_VIDEO_BAD_INPUT,0u};
    size_t i,count;
    if(!words||!pixels||!word_count||word_count>720u||varmod>1u||mode>3u)
        return result;
    if(mode==2u||(varmod&&mode!=0u)){
        result.status=AVP_JAG_OP_VIDEO_UNSUPPORTED;
        return result;
    }
    if(mode==1u&&(word_count&1u))return result;
    count=mode==1u?word_count/2u:word_count;
    if(pixel_capacity<count)return result;
    /* All input validation is completed before the first output write. */
    for(i=0u;i<count;i++){
        if(mode==1u){
            pixels[i]=avp_jaguar_vmode_rgb24(words[i*2u],words[i*2u+1u]);
        }else if(mode==3u||(varmod&&(words[i]&1u))){
            /* VARMOD bit 0 is a colour-format discriminator, not the
             * least significant GREEN source bit. Bit 0 is excluded
             * from the RGB16 six-bit component in this mixed mode. */
            pixels[i]=avp_jaguar_vmode_rgb16((u16)(words[i]&
                                 (varmod?0xfffeu:0xffffu)));
        }else{
            u8 r,g,b;
            avp_jaguar_cry16_to_rgb(words[i],&r,&g,&b);
            pixels[i]=((u32)r<<16)|((u32)g<<8)|b;
        }
    }
    result.status=AVP_JAG_OP_VIDEO_OK;
    result.pixels=count;
    return result;
}

void avp_jaguar_pause_op_init(AvpJaguarPauseOpState *s)
{
    if(s)memset(s,0,sizeof(*s));
}

void avp_jaguar_pause_op_destroy(AvpJaguarPauseOpState *s)
{
    if(!s)return;
    free(s->base);
    s->base=NULL;
    s->base_count=0;
}

void avp_jaguar_pause_op_set_assets(AvpJaguarPauseOpState *s,
    const u8 *paused,const u8 *pointer,const u8 *hud,const u8 *bar,const u8 *slider,
    const u8 *game,const u8 *one,const u8 *two,const u8 *three,const u8 *bsave,
    const u8 palette_rgb[16][3])
{
    if(!s)return;
    s->paused=paused;s->pointer=pointer;s->hud=hud;s->bar=bar;s->slider=slider;
    s->game=game;s->one=one;s->two=two;s->three=three;s->bsave=bsave;
    if(palette_rgb)memcpy(s->palette_rgb,palette_rgb,sizeof(s->palette_rgb));
    s->assets_ready=paused&&pointer&&hud&&bar&&slider&&game&&one&&two&&three&&bsave;
}

void avp_jaguar_pause_op_event(AvpJaguarPauseOpState *s,unsigned event,u32 a,u32 b,u32 c,
    u32 *pixels,unsigned width,unsigned height)
{
    if(!s)return;
    switch(event){
    case 0x534d4c30u: /* SML0: SetMazeList installs a fresh OP list. */
    case 0x524d4c30u: /* RML0: RestoreMazeList replaces the current list. */
        /* MAZESCRN.S resets the CPU pause state at SML0.  The hosted OP
         * cannot leave an old PAUSED/menu link or SaveOvers image active
         * after that list has been replaced. Keep the ROM-loaded brushes. */
        s->active=0;
        s->menu_visible=0;
        s->cursor_visible=0;
        s->bsave_visible=0;
        s->save_flash_hidden=0;
        s->save_flash_slot=0;
        free(s->base);s->base=NULL;s->base_count=0;
        break;
    case 0x50535052u: /* PSPR */
        if(a==0u){s->menu_visible=0;s->cursor_visible=0;}
        break;
    case 0x504c4e4bu: /* PLNK */
        s->active=a?1:0;
        if(!s->active){s->menu_visible=0;s->cursor_visible=0;s->save_flash_hidden=0;}
        break;
    case 0x5053564fu: /* PSVO: SaveOvers */
        if(pixels&&width&&height){
            size_t n=(size_t)width*height;
            if(s->base_count!=n){
                u32 *q=(u32*)realloc(s->base,n*sizeof(*q));
                if(q){s->base=q;s->base_count=n;}
            }
            if(s->base&&s->base_count==n)memcpy(s->base,pixels,n*sizeof(*s->base));
        }
        s->active=1;
        break;
    case 0x5052564fu: /* PRVO: RestoreOvers */
        if(pixels&&s->base&&s->base_count==(size_t)width*height)
            memcpy(pixels,s->base,s->base_count*sizeof(*pixels));
        s->active=0;s->menu_visible=0;s->cursor_visible=0;s->save_flash_hidden=0;
        break;
    case 0x504d454eu: /* PMEN */
        s->menu_visible=a?1:0;
        if(s->menu_visible)s->active=1;
        break;
    case 0x50435552u: /* PCUR */
        s->cursor_x=(int)a;s->cursor_y=(int)b;s->cursor_visible=1;s->bsave_visible=c?1:0;
        break;
    case 0x50534c44u: /* PSLD */
        s->slider_x=(int)a;
        break;
    case 0x5053464cu: /* PSFL: in-game SaveGame slot blink, not title SAVE. */
        if(a<3u){s->save_flash_slot=(int)a;s->save_flash_hidden=b?1:0;}
        break;
    case 0x50504658u: /* PPFX: pulse parity; presentation remains atomic. */
    default:
        break;
    }
}

int avp_jaguar_pause_op_active(const AvpJaguarPauseOpState *s)
{
    return s&&s->active;
}

static void draw_j4(const AvpJaguarPauseOpState *s,u32 *pixels,unsigned width,unsigned height,
    const u8 *src,int lx,int ly,int sw,int sh)
{
    int sx,sy;
    if(!s||!pixels||!src||!width||!height||sw<=0||sh<=0)return;
    for(sy=0;sy<sh;sy++)for(sx=0;sx<sw;sx++){
        /* All retail pause brushes use whole-phrase (16-pixel) widths. */
        const size_t source_bytes=(((size_t)sw+15u)/16u)*8u*(size_t)sh;
        unsigned pi=avp_jaguar_op4_phrased_pixel(src,source_bytes,
            (unsigned)sw,(unsigned)(((size_t)sw+15u)/16u),1u,
            (unsigned)sx,(unsigned)sy);
        int x0,x1,y0,y1,yy;
        u32 c;
        if(pi==0u)continue; /* OBJ_TRANS */
        c=((u32)s->palette_rgb[pi][0]<<16)|((u32)s->palette_rgb[pi][1]<<8)|s->palette_rgb[pi][2];
        x0=(int)(((long long)(lx+sx)*width)/320ll);
        x1=(int)(((long long)(lx+sx+1)*width+319ll)/320ll);
        y0=(int)(((long long)(ly+sy)*height)/228ll);
        y1=(int)(((long long)(ly+sy+1)*height+227ll)/228ll);
        if(x0<0)x0=0;
        if(y0<0)y0=0;
        if(x1>(int)width)x1=(int)width;
        if(y1>(int)height)y1=(int)height;
        for(yy=y0;yy<y1;yy++){
            int xx;
            for(xx=x0;xx<x1;xx++)pixels[(size_t)yy*width+(unsigned)xx]=c;
        }
    }
}

static void overlay(const AvpJaguarPauseOpState *s,u32 *pixels,unsigned width,unsigned height)
{
    if(!s||!s->active||!s->assets_ready)return;
    if(!s->menu_visible){
        draw_j4(s,pixels,width,height,s->paused,62,82,208,36);
        return;
    }
    draw_j4(s,pixels,width,height,s->hud,58,42,64,20);
    draw_j4(s,pixels,width,height,s->bar,130,39,160,24);
    draw_j4(s,pixels,width,height,s->slider,s->slider_x,34,32,32);
    if(!s->save_flash_hidden||s->save_flash_slot!=0){
        draw_j4(s,pixels,width,height,s->game,102,76,96,24);
        draw_j4(s,pixels,width,height,s->one,196,76,32,24);
    }
    if(!s->save_flash_hidden||s->save_flash_slot!=1){
        draw_j4(s,pixels,width,height,s->game,102,105,96,24);
        draw_j4(s,pixels,width,height,s->two,195,104,32,24);
    }
    if(!s->save_flash_hidden||s->save_flash_slot!=2){
        draw_j4(s,pixels,width,height,s->game,102,133,96,24);
        draw_j4(s,pixels,width,height,s->three,195,132,32,24);
    }
    if(s->bsave_visible)draw_j4(s,pixels,width,height,s->bsave,100,161,128,24);
    /* Pause menu cursor coordinates include the 14-line HUD/OP Y origin.
     * The host draws in the already-visible 320x228 viewport, so subtract
     * the origin once here instead of changing original PAUSE_EV_CURSOR Y. */
    if(s->cursor_visible)draw_j4(s,pixels,width,height,s->pointer,s->cursor_x,s->cursor_y-14,32,28);
}

void avp_jaguar_pause_op_prepare_frame(const AvpJaguarPauseOpState *s,u32 *pixels,
    unsigned width,unsigned height)
{
    if(!s||!pixels||!s->active)return;
    if(s->base&&s->base_count==(size_t)width*height)
        memcpy(pixels,s->base,s->base_count*sizeof(*pixels));
    overlay(s,pixels,width,height);
}
