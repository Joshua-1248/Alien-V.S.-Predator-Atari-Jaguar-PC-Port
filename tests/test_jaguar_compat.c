#include "jaguar/cry.h"
#include "jaguar/object_processor.h"
#include "jaguar/framebuffer.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void test_player_clut(void)
{
    assert(avp_jaguar_game_clut(0x8880u,0x8880u,0)==0u);
    /* Marine, Alien and Predator reference entries come from PLAYER.S::pain
     * and MAZESCRN.S::MazeList. They must remain different from neutral. */
    assert(avp_jaguar_game_clut(0x8780u,0x8880u,0)==0x0f00u);
    assert(avp_jaguar_game_clut(0x7780u,0x8880u,0)==0xff00u);
    assert(avp_jaguar_game_clut(0x8980u,0x8880u,0)==0x0100u);
    assert(avp_jaguar_game_clut(0x8880u,0x9558u,0)==0x1dd8u);
    assert(avp_jaguar_game_clut(0x8880u,0x8880u,0x80)==0x0080u);
    assert(avp_jaguar_game_clut(0x8880u,0x8880u,-128)==0x007fu);
}

static void test_cry(void)
{
    static const struct { u16 cry; u8 r,g,b; } cv[]={
        {0x0000,0,0,0},{0x00ff,0,0,254},{0x1234,6,7,51},
        {0x8f80,8,127,0},{0xffff,254,254,0},{0x4a7f,64,126,123}
    };
    u8 *q=(u8*)malloc(131072u);
    unsigned i;
    assert(q);
    for(i=0;i<131072u;i++)q[i]=(u8)((i*37u+11u)&255u);
    for(i=0;i<sizeof(cv)/sizeof(cv[0]);i++){
        u8 r=0,g=0,b=0;
        avp_jaguar_cry16_to_rgb(cv[i].cry,&r,&g,&b);
        assert(r==cv[i].r&&g==cv[i].g&&b==cv[i].b);
    }
    assert(avp_jaguar_rgb_to_cry_qconv(0,0,0,q,131072u)==0x0000u);
    assert(avp_jaguar_rgb_to_cry_qconv(255,0,0,q,131072u)==0x0bffu);
    assert(avp_jaguar_rgb_to_cry_qconv(0,255,0,q,131072u)==0x0bffu);
    assert(avp_jaguar_rgb_to_cry_qconv(0,0,255,q,131072u)==0xfbffu);
    assert(avp_jaguar_rgb_to_cry_qconv(128,64,32,q,131072u)==0x2680u);
    assert(avp_jaguar_rgb_to_cry_qconv(64,128,64,q,131072u)==0x6680u);
    assert(avp_jaguar_rgb_to_cry_qconv(12,12,12,q,131072u)==0xe60cu);
    assert(avp_jaguar_rgb_to_cry_qconv(255,255,255,q,131072u)==0xe6ffu);
    assert(avp_jaguar_rmw_blend_cry(0x1234u,0x8880u)==0x0000u);
    assert(avp_jaguar_rmw_blend_cry(0x8f80u,0x77ffu)==0xff7fu);
    assert(avp_jaguar_rmw_blend_cry(0xffffu,0x0101u)==0xffffu);
    assert(avp_jaguar_rmw_blend_cry(0x0000u,0xffffu)==0x0000u);
    assert(avp_jaguar_rmw_blend_cry(0x4a7fu,0x9205u)==0x0c84u);
    free(q);
}

static void test_predator_vmode(void)
{
    /* Retail RDB.DTA $44C8-$44DB contains VMODE/X_FADE word pairs:
     * $06C7/$9558, $0EC3/$0CF8, $0EC3/$8880,
     * $06C7/$26F8, $06C7/$8880.  The last 2 differ in VMODE
     * even where the original color modifier is neutral. */
    assert((0x06c7u&6u)==6u); /* RGB16, PWIDTH=4 */
    assert((0x0ec3u&6u)==2u); /* RGB24, PWIDTH=8 */
    /* RGB16: 5 red, 6 green, 5 blue. */
    assert(avp_jaguar_vmode_rgb16(0xf800u)==0xf80000u); /* TRM p.23: MSBs, low bits zero */
    assert(avp_jaguar_vmode_rgb16(0x07c0u)==0x0000f8u);
    assert(avp_jaguar_vmode_rgb16(0x003fu)==0x0000fc00u);
    assert(avp_jaguar_vmode_rgb16(0xffffu)==0xf8fcf8u);
    /* RGB24: first screen word is G,R; second word low byte is B. */
    assert(avp_jaguar_vmode_rgb24(0x1234u,0x56abu)==0x003412abu);
    assert(avp_jaguar_vmode_rgb24(0x8880u,0x8880u)==0x00808880u);
}

static void test_op_4bpp_phrase_stride(void)
{
    /* TOM manual pp. 18-19: 4-bpp is 16 pixels per 64-bit phrase.  An
     * object with source width 20 has a two-phrase (16-byte) row stride,
     * NOT 10 packed bytes per row.  Retain high-nibble-first HILO=1. */
    u8 a[32]={0};
    a[0]=0x12u; a[7]=0x34u; a[8]=0x56u; a[9]=0x78u;
    a[10]=0x9au; /* phrase padding must not become next row */
    a[16]=0xbcu; a[17]=0xdeu;
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,0u,0u)==1u);
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,1u,0u)==2u);
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,14u,0u)==3u);
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,15u,0u)==4u);
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,16u,0u)==5u);
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,19u,0u)==8u);
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,0u,1u)==11u);
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,1u,1u)==12u);
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,2u,1u)==13u);
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,20u,0u)==0u);
    assert(avp_jaguar_op4_contiguous_pixel(a,sizeof(a),20u,0u,2u)==0u);
    assert(avp_jaguar_op4_contiguous_pixel(a,9u,20u,19u,0u)==0u);
    assert(avp_jaguar_op4_contiguous_pixel(NULL,0u,20u,0u,0u)==0u);
    /* All existing AVP pause brushes have complete phrases per row, so
     * this generalized OP sampler must reproduce their old packed layout. */
    for(unsigned w=16;w<=320;w+=16){
        u8 row[160];
        for(unsigned j=0;j<w/2u;j++)row[j]=(u8)(j*37u+5u);
        for(unsigned x=0;x<w;x++){
            unsigned bi=x/2u;
            u8 old=(u8)((x&1u)?(row[bi]&15u):(row[bi]>>4));
            assert(avp_jaguar_op4_contiguous_pixel(row,w/2u,w,x,0u)==old);
        }
    }
}

static void test_op_bitmap_header_and_pitch(void)
{
    /* Synthetic MJP-compatible OP phrase construction from original
     * reconstructed src/game/mjp.c::Make_New. Preserve the 68000 longword
     * order rather than interpreting these as host-endian uint64_t memory. */
    u32 words[6]={0};
    AvpJaguarOpBitmapHeader b;
    const u32 base=0x4fc20u, data=0x12000u;
    const u32 link=(base+0x20u)>>3;
    const u32 src_phrases=data>>3;
    const u32 w=320u/4u;
    words[0]=(link>>8)|(src_phrases<<11);
    words[1]=(64u<<4)|(110u<<14)|(link<<24);
    words[2]=(w>>4)|0x8000u;
    words[3]=2u|0x4000u|0x8000u|(w<<18)|(w<<10);
    assert(avp_jaguar_op_decode_bitmap_words(words,4u,&b));
    assert(b.type==0u && b.ypos==128u && b.height==110u);
    assert(b.link_address==base+0x20u && b.data_address==data);
    assert(b.xpos==2 && b.depth==4u && b.dwidth==80u && b.iwidth==80u);
    assert(b.trans==1u && b.reflect==0u && b.rmw==0u);
    assert(b.firstpix==0u && b.hscale==0u && b.vscale==0u);
    /* The parser reports raw register fields, including any unusual source
     * PITCH; it must not silently normalize a value to our assumptions. */
    assert(b.pitch==3u);
    words[1]|=1u;  /* Make_Sca: SCBITOBJ */
    words[5]=0x00442c2cu; /* HSCALE=44, VSCALE=44, REMAINDER=68 */
    assert(!avp_jaguar_op_decode_bitmap_words(words,4u,&b));
    assert(avp_jaguar_op_decode_bitmap_words(words,6u,&b));
    assert(b.type==1u && b.hscale==44u && b.vscale==44u && b.remainder==68u);
    words[3]=(words[3]&~0xfffu)|0xfffu;
    assert(avp_jaguar_op_decode_bitmap_words(words,6u,&b) && b.xpos==-1);
    words[3]=(words[3]&~0xfffu)|0x800u;
    assert(avp_jaguar_op_decode_bitmap_words(words,6u,&b) && b.xpos==-2048);
    /* Decode documented bitmap-specific flags without silently treating
     * this as executed clipping, reflection, or source-pixel shading. */
    {
        uint64_t raw=((uint64_t)1u<<48)|
                     ((uint64_t)1u<<47)|((uint64_t)1u<<46)|
                     ((uint64_t)1u<<45)|((uint64_t)87u<<38)|
                     ((uint64_t)10u<<28)|((uint64_t)6u<<18)|
                     ((uint64_t)2u<<15)|((uint64_t)2u<<12)|0x7ffu;
        raw|=((uint64_t)45u<<49);
        words[2]=(u32)(raw>>32);words[3]=(u32)raw;
        assert(avp_jaguar_op_decode_bitmap_words(words,6u,&b));
        assert(b.firstpix==45u && b.index==87u);
        assert(b.reflect==1u && b.rmw==1u && b.trans==1u && b.release==1u);
        assert(b.dwidth==6u && b.iwidth==10u && b.depth==2u && b.pitch==2u);
        assert(b.xpos==2047);
        words[2]&=~0x3fu; /* IWIDTH high six bits are here. */
        words[3]&=~0xf0000000u; /* IWIDTH low four bits are here. */
        assert(!avp_jaguar_op_decode_bitmap_words(words,6u,&b));
    }
    words[1]=(words[1]&~7u)|2u; /* GPU object, NOT a bitmap object */
    assert(!avp_jaguar_op_decode_bitmap_words(words,6u,&b));
    assert(!avp_jaguar_op_decode_bitmap_words(NULL,6u,&b));
    assert(!avp_jaguar_op_decode_bitmap_words(words,6u,NULL));
    /* Physically repeated and interleaved source phrases: PITCH 0/2/3. */
    {
        u8 src[64]={0};
        src[0]=0x12;src[8]=0x34;src[16]=0x56;src[24]=0x78;
        src[32]=0x9a;src[40]=0xbc;
        assert(avp_jaguar_op4_phrased_pixel(src,sizeof(src),32u,4u,0u,0u,0u)==1u);
        assert(avp_jaguar_op4_phrased_pixel(src,sizeof(src),32u,4u,0u,16u,0u)==1u);
        assert(avp_jaguar_op4_phrased_pixel(src,sizeof(src),32u,4u,1u,16u,0u)==3u);
        assert(avp_jaguar_op4_phrased_pixel(src,sizeof(src),32u,4u,2u,16u,0u)==5u);
        assert(avp_jaguar_op4_phrased_pixel(src,sizeof(src),32u,4u,3u,16u,0u)==7u);
        assert(avp_jaguar_op4_phrased_pixel(src,sizeof(src),32u,4u,3u,0u,1u)==9u);
        assert(avp_jaguar_op4_phrased_pixel(src,sizeof(src),32u,4u,3u,16u,1u)==0u);
        assert(avp_jaguar_op4_phrased_pixel(src,17u,32u,4u,2u,16u,0u)==5u);
        assert(avp_jaguar_op4_phrased_pixel(src,16u,32u,4u,2u,16u,0u)==0u);
        assert(avp_jaguar_op4_phrased_pixel(src,sizeof(src),32u,1024u,1u,0u,0u)==0u);
        assert(avp_jaguar_op4_phrased_pixel(src,sizeof(src),32u,4u,8u,0u,0u)==0u);
    }
}

/* Original PRE_LIST bounds were established against the retail 68000
 * CPU record builder in R224. These are small derived header-word
 * fixtures, not original game artwork, executable images or music. */
static void test_op_branch_conditions_and_prelist(void)
{
    const struct {u32 words[2];u32 target;unsigned ypos;} v[]={
        {{0x0000000eu,0xab0040e3u},0x7558u,28u}, /* NTSC VDB */
        {{0x0000000eu,0xac004fe3u},0x7560u,508u},/* NTSC VDE */
        {{0x0000000eu,0xab004163u},0x7558u,44u}, /* PAL VDB */
        {{0x0000000eu,0xac0052e3u},0x7560u,604u}/* PAL VDE */
    };
    AvpJaguarOpBranchHeader b;
    unsigned i;
    for(i=0;i<4u;i++){
        assert(avp_jaguar_op_decode_branch_words(v[i].words,2u,&b));
        assert(b.ypos==v[i].ypos&&b.link_address==v[i].target);
        assert(b.condition==1u);
        assert(avp_jaguar_op_branch_taken(&b,b.ypos-2u,0u));
        assert(!avp_jaguar_op_branch_taken(&b,b.ypos,0u));
        assert(!avp_jaguar_op_branch_taken(&b,b.ypos+2u,0u));
    }
    /* PRE_LIST branch #1 -> STOP before VDB; branch #2 -> dynamic
     * display list until VDE; neither branch after VDE. This remains
     * a logical predicate test, not hardware-time-accurate traversal. */
    assert(avp_jaguar_op_decode_branch_words(v[0].words,2u,&b));
    assert(avp_jaguar_op_branch_taken(&b,26u,0u));
    assert(!avp_jaguar_op_branch_taken(&b,28u,0u));
    assert(avp_jaguar_op_decode_branch_words(v[1].words,2u,&b));
    assert(avp_jaguar_op_branch_taken(&b,28u,0u));
    assert(!avp_jaguar_op_branch_taken(&b,508u,0u));
    /* Exhaust the non-interlaced (even half-line) VC field for both
     * validated PRE_LIST video modes. Neither half-line equality must
     * leak into the active dynamic list. */
    for(i=0;i<2u;i++){
        AvpJaguarOpBranchHeader before,after;
        unsigned begin=i?44u:28u,end=i?604u:508u;
        assert(avp_jaguar_op_decode_branch_words(v[i*2u].words,2u,&before));
        assert(avp_jaguar_op_decode_branch_words(v[i*2u+1u].words,2u,&after));
        for(unsigned vc=0;vc<=0x7feu;vc+=2u){
            int left=avp_jaguar_op_branch_taken(&before,vc,0u);
            int right=avp_jaguar_op_branch_taken(&after,vc,0u);
            assert(left==(vc<begin));
            assert(right==(vc<end));
            assert((!left&&right)==(vc>=begin&&vc<end));
        }
    }
    /* All FOUR documented, bit-field-encodable conditions. */
    {
        u32 words[2]={0,0};
        for(unsigned cc=0;cc<4u;cc++){
            uint64_t p=((uint64_t)0x15555u<<24)|((uint64_t)cc<<14)|
                       ((uint64_t)99u<<3)|3u;
            words[0]=(u32)(p>>32);words[1]=(u32)p;
            assert(avp_jaguar_op_decode_branch_words(words,2u,&b));
            assert(b.ypos==99u&&b.condition==cc&&b.link_address==0xaaaa8u);
            if(cc==0u){
                assert(avp_jaguar_op_branch_taken(&b,99u,0u));
                assert(!avp_jaguar_op_branch_taken(&b,98u,0u));
            }else if(cc==1u){
                assert(avp_jaguar_op_branch_taken(&b,98u,0u));
                assert(!avp_jaguar_op_branch_taken(&b,99u,0u));
            }else if(cc==2u){
                assert(avp_jaguar_op_branch_taken(&b,100u,0u));
                assert(!avp_jaguar_op_branch_taken(&b,99u,0u));
            }else{
                assert(avp_jaguar_op_branch_taken(&b,99u,1u));
                assert(!avp_jaguar_op_branch_taken(&b,99u,0u));
            }
        }
        /* Wildcard YPOS=7ff on condition 0. */
        words[1]=(words[1]&~(0x7ffu<<3))|(0x7ffu<<3);
        assert(avp_jaguar_op_decode_branch_words(words,2u,&b));
        assert(b.condition==3u);
        words[1]&=~(3u<<14);
        assert(avp_jaguar_op_decode_branch_words(words,2u,&b));
        assert(avp_jaguar_op_branch_taken(&b,0u,0u));
        assert(avp_jaguar_op_branch_taken(&b,1023u,0u));
        words[1]=(words[1]&~7u)|4u; /* STOP is not BRANCH */
        assert(!avp_jaguar_op_decode_branch_words(words,2u,&b));
        assert(!avp_jaguar_op_decode_branch_words(words,1u,&b));
        assert(!avp_jaguar_op_decode_branch_words(NULL,2u,&b));
        assert(!avp_jaguar_op_decode_branch_words(words,2u,NULL));
        assert(!avp_jaguar_op_branch_taken(NULL,0u,0u));
        b.condition=4u; /* documented CC field is only 2 bits */
        assert(!avp_jaguar_op_branch_taken(&b,0u,1u));
    }
}

/* A physical-window reader for CPU-visible, big-endian Jaguar object
 * header phrases. This deliberately does not make emulator code a source. */
static void op_put32(u8 *memory,size_t size,u32 base,u32 addr,u32 value)
{
    size_t off;
    assert(addr>=base);
    off=(size_t)(addr-base);
    assert(off<=size && 4u<=size-off);
    memory[off]=(u8)(value>>24);
    memory[off+1u]=(u8)(value>>16);
    memory[off+2u]=(u8)(value>>8);
    memory[off+3u]=(u8)value;
}

static void test_op_trace_original_prelist(void)
{
    /* These four phrase pairs are authenticated original 68000-produced
     * data from R224_PRELIST_BRANCH_OBJECT_CONDITIONS.md. The dynamic
     * list head and STOP are bounded test stubs, NOT extracted assets. */
    const u32 originals[2][4]={
        {0x0000000eu,0xab0040e3u,0x0000000eu,0xac004fe3u},
        {0x0000000eu,0xab004163u,0x0000000eu,0xac0052e3u}
    };
    u8 ram[0x20]={0};
    AvpJaguarOpTraceStep steps[8];
    AvpJaguarOpTraceResult r;
    unsigned mode;
    for(mode=0u;mode<2u;mode++){
        unsigned vdb=mode?44u:28u, vde=mode?604u:508u,vc;
        memset(ram,0,sizeof(ram));
        for(unsigned i=0u;i<4u;i++)
            op_put32(ram,sizeof(ram),0x7548u,0x7548u+i*4u,
                     originals[mode][i]);
        op_put32(ram,sizeof(ram),0x7548u,0x7558u+4u,4u);
        op_put32(ram,sizeof(ram),0x7548u,0x7560u+4u,4u);
        for(vc=0u;vc<=2046u;vc+=2u){
            r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),0x7548u,
                                           0x7548u,vc,0u,steps,8u);
            assert(r.status==AVP_JAG_OP_TRACE_STOP);
            assert(steps[0].type==3u && steps[0].address==0x7548u);
            if(vc<vdb){
                assert(r.count==2u && steps[0].branch_taken==1u);
                assert(steps[0].next_address==0x7558u);
                assert(steps[1].type==4u && steps[1].address==0x7558u);
            }else if(vc<vde){
                assert(r.count==3u && steps[0].branch_taken==0u);
                assert(steps[1].address==0x7550u && steps[1].branch_taken==1u);
                assert(steps[1].next_address==0x7560u);
                assert(steps[2].type==4u && steps[2].address==0x7560u);
            }else{
                assert(r.count==3u && steps[0].branch_taken==0u);
                assert(steps[1].address==0x7550u && steps[1].branch_taken==0u);
                assert(steps[2].type==4u && steps[2].address==0x7558u);
            }
        }
    }
    /* A one-entry budget returns LIMIT, never a made-up STOP. */
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),0x7548u,
                                  0x7548u,100u,0u,steps,1u);
    assert(r.status==AVP_JAG_OP_TRACE_LIMIT && r.count==1u);
}

static void test_op_trace_guardrails_and_types(void)
{
    u8 ram[0x80]={0};
    AvpJaguarOpTraceStep steps[8];
    AvpJaguarOpTraceResult r;
    const u32 base=0x1000u;
    uint64_t p;
    /* CPU-encoded bitmap (two phrases) -> STOP; high/low longword order
     * uses the same packing defined in original MJP Make_New. */
    p=((uint64_t)(0x1010u>>3)<<24)|((uint64_t)0x2000u>>3<<43)|
      ((uint64_t)10u<<14)|((uint64_t)4u<<3)|0u;
    op_put32(ram,sizeof(ram),base,base,(u32)(p>>32));
    op_put32(ram,sizeof(ram),base,base+4u,(u32)p);
    p=((uint64_t)1u<<28)|((uint64_t)1u<<18)|((uint64_t)4u<<12);
    op_put32(ram,sizeof(ram),base,base+8u,(u32)(p>>32));
    op_put32(ram,sizeof(ram),base,base+12u,(u32)p);
    op_put32(ram,sizeof(ram),base,0x1014u,4u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,base,8u,0u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_STOP && r.count==2u);
    assert(steps[0].type==0u && steps[0].active==1u);
    assert(steps[0].next_address==0x1010u && steps[1].address==0x1010u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,base,2u,0u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_STOP && steps[0].active==0u);
    /* Scaled bitmap has a THREE-phrase header, else out-of-range. */
    p=((uint64_t)(0x1018u>>3)<<24)|((uint64_t)10u<<14)|
      ((uint64_t)4u<<3)|1u;
    op_put32(ram,sizeof(ram),base,base,(u32)(p>>32));
    op_put32(ram,sizeof(ram),base,base+4u,(u32)p);
    op_put32(ram,sizeof(ram),base,base+16u,0u);
    op_put32(ram,sizeof(ram),base,base+20u,0x00302020u);
    op_put32(ram,sizeof(ram),base,base+24u+4u,4u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,base,10u,0u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_STOP && r.count==2u);
    assert(steps[0].type==1u && steps[0].next_address==0x1018u);
    /* GPU YPOS=0x7ff always interrupts. Other lines skip to next phrase. */
    op_put32(ram,sizeof(ram),base,0x1030u,0u);
    op_put32(ram,sizeof(ram),base,0x1034u,(0x7ffu<<3)|2u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,0x1030u,0u,0u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_GPU && r.count==1u);
    assert(steps[0].active==1u && steps[0].next_address==0x1038u);
    op_put32(ram,sizeof(ram),base,0x1034u,(24u<<3)|2u);
    op_put32(ram,sizeof(ram),base,0x103cu,4u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,0x1030u,22u,0u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_STOP && r.count==2u);
    assert(steps[0].active==0u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,0x1030u,24u,0u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_GPU && steps[0].active==1u);
    /* Self-linking branch causes a diagnosed cycle. */
    p=((uint64_t)(0x1040u>>3)<<24)|((uint64_t)3u<<14)|3u;
    op_put32(ram,sizeof(ram),base,0x1040u,(u32)(p>>32));
    op_put32(ram,sizeof(ram),base,0x1044u,(u32)p);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,0x1040u,0u,1u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_CYCLE && r.count==1u);
    assert(steps[0].next_address==0x1040u);
    op_put32(ram,sizeof(ram),base,0x1044u,7u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,0x1040u,0u,0u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_INVALID_OBJECT && r.count==0u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,0x1041u,0u,0u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_OUT_OF_RANGE && r.count==0u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,0x2000u,0u,0u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_OUT_OF_RANGE && r.count==0u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,base,0u,0u,NULL,8u);
    assert(r.status==AVP_JAG_OP_TRACE_BAD_INPUT && r.count==0u);
    r=avp_jaguar_op_trace_scanline(ram,sizeof(ram),base,base,0u,0u,steps,0u);
    assert(r.status==AVP_JAG_OP_TRACE_BAD_INPUT && r.count==0u);
    r=avp_jaguar_op_trace_scanline(NULL,sizeof(ram),base,base,0u,0u,steps,8u);
    assert(r.status==AVP_JAG_OP_TRACE_BAD_INPUT && r.count==0u);
}

static void test_op_unscaled_cry16_pixels(void)
{
    /* TRM Rev.8 pp.18-23: full 64-bit source phrases, physical CRY16
     * line-buffer entries, indexed CLUT, pitch/firstpix/reflect/TRANS.
     * Synthetic deterministic data; not a retail frame or GPU result. */
    AvpJaguarOpBitmapHeader h={0};
    AvpJaguarOpPixelResult r;
    u8 ram[128]={0};
    u16 line[72],clut[256];
    unsigned i;
    for(i=0u;i<256u;i++)clut[i]=(u16)(0x1000u+i);
    h.type=0u;h.depth=2u;h.pitch=1u;h.iwidth=2u;
    h.data_address=0x2000u;h.xpos=4;
    /* Two complete 4bpp phrases: 0,1,...15 then 0xf,0xe,...0. */
    for(i=0u;i<8u;i++){
        ram[i]=(u8)((i*2u<<4)|(i*2u+1u));
        ram[8u+i]=(u8)(((15u-i*2u)<<4)|(14u-i*2u));
    }
    for(i=0u;i<72u;i++)line[i]=0x2222u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==32u);
    assert(line[3]==0x2222u&&line[4]==0x1000u&&line[5]==0x1001u);
    assert(line[19]==0x100fu&&line[20]==0x100fu);
    assert(line[35]==0x1000u&&line[36]==0x2222u);
    /* Logical transparent color zero does NOT overwrite existing CRY. */
    h.trans=1u;
    for(i=0u;i<72u;i++)line[i]=0x2222u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==30u);
    assert(line[4]==0x2222u&&line[35]==0x2222u);
    assert(line[5]==0x1001u&&line[34]==0x1001u);
    /* FIRSTPIX is an unscaled pixel-PAIR source skip, with the bits
     * appropriate to the source depth. 4bpp 0x18 begins at source 6. */
    h.firstpix=0x18u;h.trans=0u;h.xpos=1;
    for(i=0u;i<72u;i++)line[i]=0x2222u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==26u);
    assert(line[0]==0x2222u&&line[1]==0x1006u&&line[2]==0x1007u);
    assert(line[26]==0x1000u&&line[27]==0x2222u);
    /* Reflection decrements destination coordinates but source order
     * remains unchanged. Off-screen pixels are clipped to the line. */
    h.firstpix=0;h.reflect=1u;h.xpos=3;
    for(i=0u;i<72u;i++)line[i]=0x2222u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==4u);
    assert(line[3]==0x1000u&&line[2]==0x1001u&&line[0]==0x1003u);
    assert(line[4]==0x2222u);
    /* PITCH=2 skips an interleaved phrase; PITCH=0 repeats one phrase. */
    h.reflect=0u;h.xpos=0;h.pitch=2u;h.iwidth=2u;
    ram[16]=0xabu;ram[17]=0xcdu;
    for(i=0u;i<72u;i++)line[i]=0x2222u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==32u);
    assert(line[15]==0x100fu&&line[16]==0x100au&&line[17]==0x100bu);
    h.pitch=0u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&line[16]==0x1000u);
    /* 1bpp/2bpp/8bpp CLUT index selection with high bits from INDEX. */
    h.pitch=1u;h.iwidth=1u;h.xpos=0;h.index=0x53u;h.trans=0u;
    ram[0]=0x80u;
    h.depth=0u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==64u);
    assert(line[0]==0x10a7u&&line[1]==0x10a6u);
    h.depth=1u;ram[0]=0x1bu;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==32u);
    assert(line[0]==0x10a4u&&line[1]==0x10a5u);
    h.depth=3u;ram[0]=0xe5u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==8u&&line[0]==0x10e5u);
    /* Direct CRY16 bypasses CLUT and respects Jaguar big-endian source. */
    h.depth=4u;h.index=0;h.iwidth=1u;h.xpos=2;
    ram[0]=0x12u;ram[1]=0x34u;ram[2]=0x56u;ram[3]=0x78u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          NULL,0u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==4u);
    assert(line[2]==0x1234u&&line[3]==0x5678u);
    /* The existing source-backed CRY RMW function is reused only when
     * the compositor explicitly operates in physical CRY16 mode. */
    h.rmw=1u;h.depth=2u;h.iwidth=1u;h.xpos=0;
    ram[0]=0x10u;clut[1]=0x0101u;line[0]=0x1234u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK);
    assert(line[0]==avp_jaguar_rmw_blend_cry(0x1234u,0x0101u));
    /* Reject unsupported scaled objects or direct-color transparency,
     * rather than silently approximating unknown physical reservations. */
    h.type=1u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_UNSUPPORTED);
    h.type=0u;h.depth=4u;h.trans=1u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          NULL,0u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_UNSUPPORTED);
    /* Preflight failure must leave the destination buffer entirely intact. */
    h.trans=0u;h.rmw=0u;h.depth=2u;h.iwidth=2u;h.pitch=1u;h.xpos=0;
    for(i=0u;i<72u;i++)line[i]=0x2222u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,8u,0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OUT_OF_RANGE&&r.drawn==0u);
    for(i=0u;i<72u;i++)assert(line[i]==0x2222u);
    h.data_address=0x1ff8u;
    r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u);
    assert(r.status==AVP_JAG_OP_PIXEL_OUT_OF_RANGE&&r.drawn==0u);
    assert(avp_jaguar_op_compose_unscaled_cry16(NULL,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,72u).status==AVP_JAG_OP_PIXEL_BAD_INPUT);
}

/* RE24 Full Sweep 04: TOM 3.5 fixed-point SCBITOBJ horizontal & vertical
 * paths. Hardware-spec synthetic vectors plus originally reconstructed MJP
 * startup remainder ordering; not a claim of matched in-game frame pixels. */
static void test_op_scaled_cry16_pixels_and_progression(void)
{
    AvpJaguarOpBitmapHeader h={0};
    AvpJaguarOpPixelResult r;
    AvpJaguarOpScaledVerticalStep v={0};
    u8 ram[128]={0};
    u16 line[96],other[96],clut[256];
    unsigned i,depth;
    h.type=1u;h.pitch=1u;h.iwidth=1u;h.data_address=0x2000u;
    h.depth=4u;h.xpos=2;h.hscale=16u;h.vscale=16u;
    for(i=0u;i<256u;i++)clut[i]=(u16)(0x4000u|i);
    ram[0]=0x12u;ram[1]=0x34u;ram[2]=0x56u;ram[3]=0x78u;
    ram[4]=0x9au;ram[5]=0xbcu;ram[6]=0xdeu;ram[7]=0xf0u;
    for(i=0u;i<96u;i++)line[i]=0xababu;
    r=avp_jaguar_op_compose_scaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          NULL,0u,line,96u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==2u);
    assert(line[1]==0xababu&&line[2]==0x1234u&&line[3]==0x9abcu);
    assert(line[4]==0xababu);
    /* Horizontal fractional enlargement: floor(output*32/HSCALE).
     * Six visible destination pixels sample source 0,0,1,2,2,3. */
    h.hscale=48u;
    r=avp_jaguar_op_compose_scaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          NULL,0u,line,96u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==6u);
    assert(line[2]==0x1234u&&line[3]==0x1234u&&line[4]==0x5678u);
    assert(line[5]==0x9abcu&&line[6]==0x9abcu&&line[7]==0xdef0u);
    /* REFLECT decrements destination coordinates without reversing
     * the source sampling order, clipping off-screen destinations. */
    h.reflect=1u;h.xpos=3;
    for(i=0u;i<96u;i++)line[i]=0xababu;
    r=avp_jaguar_op_compose_scaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          NULL,0u,line,96u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==4u);
    assert(line[3]==0x1234u&&line[2]==0x1234u&&line[1]==0x5678u&&line[0]==0x9abcu);
    assert(line[4]==0xababu);
    /* At exact HSCALE=32, scaled one-pixel read must agree with the
     * existing unscaled path when FIRSTPIX=0 and REFLECT=0. */
    h.reflect=0u;h.xpos=0;h.firstpix=0;h.hscale=32u;
    for(depth=0u;depth<=4u;depth++){
        h.depth=depth;h.trans=depth<4u;h.rmw=0u;h.index=0u;
        for(i=0u;i<96u;i++)line[i]=other[i]=0x2222u;
        r=avp_jaguar_op_compose_scaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                             clut,256u,line,96u);
        assert(r.status==AVP_JAG_OP_PIXEL_OK);
        h.type=0u;
        r=avp_jaguar_op_compose_unscaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                               clut,256u,other,96u);
        assert(r.status==AVP_JAG_OP_PIXEL_OK);
        assert(memcmp(line,other,sizeof(line))==0);
        h.type=1u;
    }
    /* Scaled FIRSTPIX includes single-pixel resolution and PITCH=2
     * selects the second phrase from 16 bytes further in memory. */
    h.depth=2u;h.iwidth=2u;h.firstpix=0x18u;h.hscale=16u;
    h.pitch=2u;h.trans=0u;h.index=0u;
    for(i=0u;i<8u;i++)ram[i]=(u8)((i*2u<<4)|(i*2u+1u));
    for(i=0u;i<8u;i++)ram[16u+i]=(u8)(((15u-i*2u)<<4)|(14u-i*2u));
    for(i=0u;i<96u;i++)line[i]=0xababu;
    r=avp_jaguar_op_compose_scaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,96u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==13u);
    assert(line[0]==0x4006u&&line[1]==0x4008u&&line[4]==0x400eu);
    assert(line[5]==0x400fu&&line[6]==0x400du&&line[12]==0x4001u);
    h.pitch=0u;h.firstpix=0u;h.iwidth=2u;h.hscale=32u;
    r=avp_jaguar_op_compose_scaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,96u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&line[16]==0x4000u);
    /* Indexed transparency and physical CRY RMW with source pixel 1. */
    h.pitch=1u;h.iwidth=1u;h.trans=1u;h.rmw=1u;
    memset(ram,0,sizeof(ram));ram[0]=0x01u;clut[1]=0x0101u;
    for(i=0u;i<96u;i++)line[i]=0x1234u;
    r=avp_jaguar_op_compose_scaled_cry16(&h,ram,sizeof(ram),0x2000u,
                                          clut,256u,line,96u);
    assert(r.status==AVP_JAG_OP_PIXEL_OK&&r.drawn==1u);
    assert(line[0]==0x1234u);
    assert(line[1]==avp_jaguar_rmw_blend_cry(0x1234u,0x0101u));
    /* Truncated second phrase must not partly overwrite the output. */
    h.rmw=0u;h.trans=0u;h.iwidth=2u;h.pitch=1u;h.hscale=32u;
    for(i=0u;i<96u;i++)line[i]=0x1234u;
    r=avp_jaguar_op_compose_scaled_cry16(&h,ram,8u,0x2000u,
                                          clut,256u,line,96u);
    assert(r.status==AVP_JAG_OP_PIXEL_OUT_OF_RANGE&&r.drawn==0u);
    for(i=0u;i<96u;i++)assert(line[i]==0x1234u);
    h.hscale=0u;
    assert(avp_jaguar_op_compose_scaled_cry16(&h,ram,sizeof(ram),0x2000u,
                clut,256u,line,96u).status==AVP_JAG_OP_PIXEL_UNSUPPORTED);
    h.hscale=32u;h.depth=4u;h.trans=1u;
    assert(avp_jaguar_op_compose_scaled_cry16(&h,ram,sizeof(ram),0x2000u,
                NULL,0u,line,96u).status==AVP_JAG_OP_PIXEL_UNSUPPORTED);
    h.type=0u;h.trans=0u;
    assert(avp_jaguar_op_compose_scaled_cry16(&h,ram,sizeof(ram),0x2000u,
                NULL,0u,line,96u).status==AVP_JAG_OP_PIXEL_UNSUPPORTED);
    /* Original retail Make_TLi sets REMAINDER=VSCALE+0x20. Follow
     * the original PC source's while(remainder<32) then -=32 ordering. */
    h.type=1u;h.vscale=16u;h.remainder=48u;
    assert(avp_jaguar_op_scaled_vertical_step(&h,&v));
    assert(v.source_lines_advanced==0u&&v.next_remainder==16u);
    h.remainder=v.next_remainder;
    assert(avp_jaguar_op_scaled_vertical_step(&h,&v));
    assert(v.source_lines_advanced==1u&&v.next_remainder==0u);
    h.remainder=v.next_remainder;
    assert(avp_jaguar_op_scaled_vertical_step(&h,&v));
    assert(v.source_lines_advanced==2u&&v.next_remainder==0u);
    /* Source skips for scale=1, and preserving initial phase. */
    h.vscale=1u;h.remainder=0u;
    assert(avp_jaguar_op_scaled_vertical_step(&h,&v));
    assert(v.source_lines_advanced==32u&&v.next_remainder==0u);
    h.vscale=32u;h.remainder=64u;
    assert(avp_jaguar_op_scaled_vertical_step(&h,&v));
    assert(v.source_lines_advanced==0u&&v.next_remainder==32u);
    h.vscale=0u;
    assert(!avp_jaguar_op_scaled_vertical_step(&h,&v));
    assert(!avp_jaguar_op_scaled_vertical_step(NULL,&v));
    assert(!avp_jaguar_op_scaled_vertical_step(&h,NULL));
}

static void test_pause_op(void)
{
    AvpJaguarPauseOpState s;
    u8 pal[16][3]={{0}};
    u8 paused[208u*36u/2u],pointer[32u*28u/2u],hud[64u*20u/2u];
    u8 bar[160u*24u/2u],slider[32u*32u/2u],game[96u*24u/2u];
    u8 one[32u*24u/2u],two[32u*24u/2u],three[32u*24u/2u],bsave[128u*24u/2u];
    u32 pixels[320u*228u];
    size_t i;
    memset(paused,0,sizeof(paused));memset(pointer,0,sizeof(pointer));
    memset(hud,0,sizeof(hud));memset(bar,0,sizeof(bar));memset(slider,0,sizeof(slider));
    memset(game,0,sizeof(game));memset(one,0,sizeof(one));memset(two,0,sizeof(two));
    memset(three,0,sizeof(three));memset(bsave,0,sizeof(bsave));
    pal[1][0]=0x12;pal[1][1]=0x34;pal[1][2]=0x56;
    paused[0]=0x10;
    for(i=0;i<320u*228u;i++)pixels[i]=0x00abcdefu;

    avp_jaguar_pause_op_init(&s);
    avp_jaguar_pause_op_set_assets(&s,paused,pointer,hud,bar,slider,game,one,two,three,bsave,pal);
    assert(s.assets_ready);
    avp_jaguar_pause_op_event(&s,0x504c4e4bu,1,0,0,pixels,320,228); /* PLNK */
    assert(avp_jaguar_pause_op_active(&s));
    avp_jaguar_pause_op_prepare_frame(&s,pixels,320,228);
    assert(pixels[82u*320u+62u]==0x00123456u);

    pixels[0]=0x00112233u;
    avp_jaguar_pause_op_event(&s,0x5053564fu,0,0,0,pixels,320,228); /* PSVO */
    pixels[0]=0;
    avp_jaguar_pause_op_event(&s,0x5052564fu,0,0,0,pixels,320,228); /* PRVO */
    assert(pixels[0]==0x00112233u);
    assert(!avp_jaguar_pause_op_active(&s));

    avp_jaguar_pause_op_event(&s,0x504d454eu,1,0,0,pixels,320,228); /* PMEN */
    avp_jaguar_pause_op_event(&s,0x50435552u,77,88,1,pixels,320,228); /* PCUR */
    avp_jaguar_pause_op_event(&s,0x50534c44u,99,0,0,pixels,320,228); /* PSLD */
    assert(s.menu_visible&&s.cursor_visible&&s.bsave_visible);
    assert(s.cursor_x==77&&s.cursor_y==88&&s.slider_x==99);
    /* Cursor events preserve original Jaguar coordinates, while the hosted
       visible raster begins 14 lines below the object's authored origin.
       The fake 4-bpp pointer marks only its top-left pixel. */
    pointer[0]=0x10;
    avp_jaguar_pause_op_prepare_frame(&s,pixels,320,228);
    assert(pixels[(88u-14u)*320u+77u]==0x00123456u);
    assert(pixels[88u*320u+77u]==0x00abcdefu);
    pointer[0]=0;
    /* In-game save confirmation only hides the selected slot image, not
       the neighboring slots or the rest of the pause menu. */
    game[0]=0x10;one[0]=0x10;two[0]=0x10;
    avp_jaguar_pause_op_event(&s,0x5053464cu,0,1,0,pixels,320,228); /* PSFL slot 0 hide */
    avp_jaguar_pause_op_prepare_frame(&s,pixels,320,228);
    assert(pixels[76u*320u+102u]==0x00abcdefu);
    assert(pixels[105u*320u+102u]==0x00123456u);
    avp_jaguar_pause_op_event(&s,0x5053464cu,0,0,0,pixels,320,228); /* show */
    avp_jaguar_pause_op_prepare_frame(&s,pixels,320,228);
    assert(pixels[76u*320u+102u]==0x00123456u);
    avp_jaguar_pause_op_event(&s,0x5053464cu,2,1,0,pixels,320,228);
    avp_jaguar_pause_op_prepare_frame(&s,pixels,320,228);
    assert(pixels[76u*320u+102u]==0x00123456u);
    avp_jaguar_pause_op_event(&s,0x5052564fu,0,0,0,pixels,320,228);
    assert(!s.save_flash_hidden);
    /* A new MAZESCRN.S SetMazeList must replace, rather than retain, the
     * previous pause OP list and SaveOvers surface between campaigns. */
    avp_jaguar_pause_op_event(&s,0x5053564fu,0,0,0,pixels,320,228);
    avp_jaguar_pause_op_event(&s,0x504d454eu,1,0,0,pixels,320,228);
    assert(s.active && s.base && s.base_count==320u*228u);
    avp_jaguar_pause_op_event(&s,0x534d4c30u,0,512,512,pixels,320,228);
    assert(!s.active && !s.menu_visible && !s.cursor_visible);
    assert(!s.base && !s.base_count && s.assets_ready);
    avp_jaguar_pause_op_event(&s,0x5053564fu,0,0,0,pixels,320,228);
    assert(s.active && s.base);
    avp_jaguar_pause_op_event(&s,0x524d4c30u,0,512,0,pixels,320,228);
    assert(!s.active && !s.base && !s.base_count && s.assets_ready);
    avp_jaguar_pause_op_destroy(&s);
}

/* Rev.8 pp.18-22, non-interlaced OP DATA / HEIGHT / REMAINDER
 * writeback. These tests use Jaguar 68000 phrase-order words and a
 * separate byte-addressed memory window; no PC-rendering assumptions. */
static void op_pack_bitmap(u32 words[6],unsigned type,unsigned height,
                           unsigned dwidth,unsigned data_phrase,
                           unsigned vscale,unsigned remainder)
{
    const uint64_t p0=(UINT64_C(0x1d52)<<24)|
                      ((uint64_t)(data_phrase&0x1fffffu)<<43)|
                      ((uint64_t)(height&0x3ffu)<<14)|
                      ((uint64_t)7u<<3)|type;
    const uint64_t p1=((uint64_t)5u<<49)|((uint64_t)1u<<48)|
                      ((uint64_t)0x41u<<38)|((uint64_t)4u<<28)|
                      ((uint64_t)(dwidth&0x3ffu)<<18)|
                      ((uint64_t)3u<<15)|((uint64_t)2u<<12)|
                      0x3ffu;
    const uint64_t p2=(UINT64_C(0x133579)<<24)|
                      ((uint64_t)remainder<<16)|
                      ((uint64_t)vscale<<8)|32u;
    words[0]=(u32)(p0>>32);words[1]=(u32)p0;
    words[2]=(u32)(p1>>32);words[3]=(u32)p1;
    words[4]=(u32)(p2>>32);words[5]=(u32)p2;
}

static void test_op_bitmap_writeback(void)
{
    u32 w[6],after[6],copy[6];
    AvpJaguarOpBitmapHeader hdr;
    unsigned rows=0xeeeeu;
    /* Unscaled: one source row per displayed row; reserved flag/coords
     * retained exactly. The physical DATA source increments DWIDTH*8. */
    op_pack_bitmap(w,0u,3u,5u,0x1111u,0u,0u);
    assert(avp_jaguar_op_advance_bitmap_words(w,4u,after,&rows));
    assert(rows==1u&&after[2]==w[2]&&after[3]==w[3]);
    assert(avp_jaguar_op_decode_bitmap_words(after,4u,&hdr));
    assert(hdr.height==2u && hdr.data_address==(0x1116u<<3));
    assert(hdr.xpos==1023&&hdr.firstpix==5u&&hdr.release==1u);
    assert(w[0]!=after[0]&&w[1]!=after[1]);
    /* Preserve input and support in-place packed-header advancement. */
    memcpy(copy,w,sizeof(copy));
    assert(avp_jaguar_op_advance_bitmap_words(copy,4u,copy,NULL));
    assert(!memcmp(copy,after,4u*sizeof(u32)));
    assert(avp_jaguar_op_advance_bitmap_words(after,4u,after,&rows));
    assert(avp_jaguar_op_decode_bitmap_words(after,4u,&hdr));
    assert(hdr.height==1u&&hdr.data_address==(0x111bu<<3));
    assert(avp_jaguar_op_advance_bitmap_words(after,4u,after,&rows));
    assert(avp_jaguar_op_decode_bitmap_words(after,4u,&hdr));
    assert(hdr.height==0u&&hdr.data_address==(0x1120u<<3));
    memcpy(copy,after,sizeof(copy));
    assert(avp_jaguar_op_advance_bitmap_words(after,4u,after,&rows));
    assert(rows==0u&&!memcmp(after,copy,4u*sizeof(u32)));
    /* 24-bit wrap is encoded in the 21-bit phrase field. */
    op_pack_bitmap(w,0u,2u,8u,0x1ffffeu,0u,0u);
    assert(avp_jaguar_op_advance_bitmap_words(w,4u,after,&rows));
    assert(avp_jaguar_op_decode_bitmap_words(after,4u,&hdr));
    assert(hdr.data_address==48u&&hdr.height==1u);
    /* Scaled 1:1 sourced from original AVP Update2: starting remainder
     * 0x40 and 0x20 scale consumes the first source row after a line,
     * followed by source progression on subsequent lines. */
    op_pack_bitmap(w,1u,4u,3u,0x3456u,32u,64u);
    assert(avp_jaguar_op_advance_bitmap_words(w,6u,after,&rows));
    assert(rows==0u);
    assert(avp_jaguar_op_decode_bitmap_words(after,6u,&hdr));
    assert(hdr.height==4u&&hdr.data_address==(0x3456u<<3)&&hdr.remainder==32u);
    assert(after[2]==w[2]&&after[3]==w[3]);
    assert(avp_jaguar_op_advance_bitmap_words(after,6u,after,&rows));
    assert(rows==0u);
    assert(avp_jaguar_op_decode_bitmap_words(after,6u,&hdr));
    assert(hdr.remainder==0u&&hdr.height==4u);
    assert(avp_jaguar_op_advance_bitmap_words(after,6u,after,&rows));
    assert(rows==1u);
    assert(avp_jaguar_op_decode_bitmap_words(after,6u,&hdr));
    assert(hdr.remainder==0u&&hdr.height==3u&&hdr.data_address==(0x3459u<<3));
    /* Reduction 1/32 may consume 32 source rows during one displayed
     * row, saturating 10-bit HEIGHT at zero without unsigned underflow. */
    op_pack_bitmap(w,1u,3u,2u,0x12u,1u,0u);
    assert(avp_jaguar_op_advance_bitmap_words(w,6u,after,&rows));
    assert(rows==32u);
    assert(avp_jaguar_op_decode_bitmap_words(after,6u,&hdr));
    assert(hdr.height==0u&&hdr.data_address==(0x52u<<3)&&hdr.remainder==0u);
    /* Reject truncated header, unsupported zero VSCALE, non-bitmap.
     * Failure must not overwrite destination or row count. */
    memcpy(copy,w,sizeof(copy));
    for(unsigned i=0;i<6u;i++)after[i]=0xc0ffee00u+i;
    assert(!avp_jaguar_op_advance_bitmap_words(w,4u,after,&rows));
    for(unsigned i=0;i<6u;i++)assert(after[i]==0xc0ffee00u+i);
    assert(rows==32u);
    copy[5]&=~0xff00u; /* zero out VSCALE field */
    assert(!avp_jaguar_op_advance_bitmap_words(copy,6u,after,&rows));
    assert(after[0]==0xc0ffee00u);
    copy[1]=(copy[1]&~7u)|2u;
    assert(!avp_jaguar_op_advance_bitmap_words(copy,6u,after,&rows));
    assert(!avp_jaguar_op_advance_bitmap_words(NULL,6u,after,&rows));
    assert(!avp_jaguar_op_advance_bitmap_words(w,6u,NULL,&rows));
    /* Transactional big-endian memory header writeback, including
     * guard bytes and failure on incomplete physical memory window. */
    {
        u8 memory[72];
        unsigned i;
        op_pack_bitmap(w,1u,2u,2u,0x1fffffu,32u,0u);
        memset(memory,0x5au,sizeof(memory));
        for(i=0u;i<6u;i++){
            memory[8u+i*4u]=(u8)(w[i]>>24);
            memory[8u+i*4u+1u]=(u8)(w[i]>>16);
            memory[8u+i*4u+2u]=(u8)(w[i]>>8);
            memory[8u+i*4u+3u]=(u8)w[i];
        }
        assert(avp_jaguar_op_writeback_bitmap_memory(memory,sizeof(memory),
                                                     0x8000u,0x8008u,&rows));
        assert(rows==1u && memory[7u]==0x5au && memory[32u]==0x5au);
        for(i=0u;i<6u;i++)after[i]=((u32)memory[8u+4u*i]<<24)|
            ((u32)memory[9u+4u*i]<<16)|((u32)memory[10u+4u*i]<<8)|
            memory[11u+4u*i];
        assert(avp_jaguar_op_decode_bitmap_words(after,6u,&hdr));
        assert(hdr.height==1u&&hdr.data_address==8u&&hdr.remainder==0u);
        /* Missing last scaled phrase must fail without any mutation. */
        for(i=0u;i<6u;i++)w[i]=after[i];
        assert(!avp_jaguar_op_writeback_bitmap_memory(memory,31u,
                                                      0x8000u,0x8008u,&rows));
        for(i=0u;i<6u;i++)assert(w[i]==((u32)memory[8u+4u*i]<<24|
          (u32)memory[9u+4u*i]<<16|(u32)memory[10u+4u*i]<<8|
          memory[11u+4u*i]));
        assert(!avp_jaguar_op_writeback_bitmap_memory(memory,sizeof(memory),
                                                      0x8000u,0x8009u,NULL));
    }
}

static void test_framebuffer(void)
{
    AvpJaguarFramebuffer fb={0};
    assert(avp_jaguar_framebuffer_init(&fb,320u,228u));
    assert(fb.width==320u&&fb.height==228u);
    assert(avp_jaguar_framebuffer_count(&fb)==320u*228u);
    avp_jaguar_framebuffer_clear(&fb,0x00123456u);
    assert(fb.pixels[0]==0x00123456u);
    assert(fb.pixels[320u*228u-1u]==0x00123456u);
    avp_jaguar_framebuffer_destroy(&fb);
    assert(!fb.pixels&&fb.width==0u&&fb.height==0u);
}

int main(void)
{
    test_cry();test_predator_vmode();
    test_op_4bpp_phrase_stride();test_op_bitmap_header_and_pitch();
    test_op_branch_conditions_and_prelist();
    test_op_trace_original_prelist();test_op_trace_guardrails_and_types();
    test_op_unscaled_cry16_pixels();
    test_op_scaled_cry16_pixels_and_progression();
    test_op_bitmap_writeback();
    test_player_clut();
    test_pause_op();
    test_framebuffer();
    return 0;
}
