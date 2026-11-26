/* R212: independently test the software OP list decoder/tracer against
 * the 3-bit TYPE=3 CC hardware semantics already executed by jag_machine
 * since R207. The original Jaguar Software Reference V10 corrects the
 * 2-bit typo in the older Rev.8 manual. No game-side renderer changes. */
#include "jaguar/object_processor.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void be_phrase(uint8_t *ram,unsigned off,uint64_t p)
{
    for(unsigned i=0;i<8u;i++)ram[off+i]=(uint8_t)(p>>(56u-8u*i));
}
static uint64_t branch_phrase(unsigned cc,unsigned y,unsigned link)
{
    return (uint64_t)3u|((uint64_t)(y&0x7ffu)<<3)|
           ((uint64_t)(cc&7u)<<14)|((uint64_t)((link>>3)&0x7ffffu)<<24);
}
static void check(unsigned cc,unsigned y,unsigned vc,unsigned obf,unsigned hc,
                  int taken)
{
    uint8_t ram[0x40],original[sizeof ram];
    AvpJaguarOpTraceStep steps[5];
    AvpJaguarOpTraceResult r;
    AvpJaguarOpBranchHeader decoded;
    uint32_t words[2];
    const unsigned base=0x1000u;
    const uint64_t phrase=branch_phrase(cc,y,base+16u);
    memset(ram,0,sizeof ram);
    be_phrase(ram,0u,phrase);
    be_phrase(ram,8u,UINT64_C(0x0123456700000004));
    be_phrase(ram,16u,UINT64_C(0x89abcdef00000004));
    memcpy(original,ram,sizeof ram);
    words[0]=(uint32_t)(phrase>>32);words[1]=(uint32_t)phrase;
    assert(avp_jaguar_op_decode_branch_words(words,2u,&decoded));
    assert(decoded.condition==cc&&decoded.ypos==y&&decoded.link_address==base+16u);
    assert(avp_jaguar_op_branch_taken_at_hc(&decoded,vc,obf,hc)==taken);
    if(cc==4u&&hc==0x400u)
        assert(!avp_jaguar_op_branch_taken(&decoded,vc,obf));
    r=avp_jaguar_op_trace_scanline_at_hc(ram,sizeof ram,base,base,
                                       vc,obf,hc,steps,5u);
    if(cc>4u){
        assert(r.status==AVP_JAG_OP_TRACE_INVALID_OBJECT);
        assert(r.address==base&&r.count==0u);
    }else{
        assert(r.status==AVP_JAG_OP_TRACE_STOP);
        assert(r.count==2u&&steps[0].address==base);
        assert(steps[0].branch_taken==(unsigned)taken);
        assert(steps[0].next_address==base+(taken?16u:8u));
        assert(steps[1].address==base+(taken?16u:8u));
        assert(steps[1].type==4u);
        if(!hc){
            AvpJaguarOpTraceStep legacy[5];
            AvpJaguarOpTraceResult old=avp_jaguar_op_trace_scanline(
                ram,sizeof ram,base,base,vc,obf,legacy,5u);
            assert(old.status==r.status&&old.count==r.count);
            assert(legacy[0].branch_taken==steps[0].branch_taken);
            assert(legacy[1].address==steps[1].address);
        }
    }
    assert(memcmp(original,ram,sizeof ram)==0);
}
int main(void)
{
    unsigned total=0u;
    for(unsigned hc=0;hc<=0x400u;hc+=0x400u){
      for(unsigned obf=0;obf<=1u;obf++){
        for(unsigned vc=27u;vc<=29u;vc++){
          for(unsigned y=27u;y<=29u;y++){
            for(unsigned cc=0;cc<=7u;cc++){
              int yes=cc==0u?(y==vc):cc==1u?(y>vc):
                      cc==2u?(y<vc):cc==3u?(obf!=0u):
                      cc==4u?(hc!=0u):0;
              check(cc,y,vc,obf,hc,yes);total++;
            }
          }
          /* YPOS=0x7ff is a wildcard for CC0 only; CC4 tests HC10. */
          check(0u,0x7ffu,vc,obf,hc,1);total++;
          check(4u,0x7ffu,vc,obf,hc,hc!=0u);total++;
        }
      }
    }
    printf("R212 software OP CC0..4/HC10 + unsupported CC5..7: %u cases PASS\n",total);
    return 0;
}
