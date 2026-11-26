/* RE42 R245: TOM VI generates once per frame (every OTHER field) when
 * VP+1 is odd / interlaced, according to Atari Software Reference V10,
 * VI ($F0004E). In noninterlaced mode, VI generates every field.
 *
 * The fixture runs the actual Jaguar reference-machine half-line scheduler.
 * It counts latched/acknowledged TOM video IRQs, including VI=0 exactly at
 * the VC wrap; no CPU polling or bus arbitration timing is claimed.
 *
 * The starting field parity is a model convention, not yet calibrated to
 * the physical console. This tests single-source alternating-field cadence.
 */
#include "jaguar/jag_machine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static JagMachine m;
static uint8_t cart[0x3000];
static void init(unsigned vp,unsigned vi)
{
    memset(cart,0,sizeof cart);
    cart[0x404u]=0u;cart[0x405u]=0x80u;cart[0x406u]=0x20u;
    cart[0x2000u]=0x60u;cart[0x2001u]=0xfeu; /* BRA.S -2 */
    assert(jagm_init(&m,cart,sizeof cart)==0);
    jagm_write16(&m,0xF0002Eu,31u); /* 32 clocks per half-line */
    jagm_write16(&m,0xF0003Eu,(uint16_t)vp);
    jagm_write16(&m,0xF0004Eu,(uint16_t)vi);
    jagm_write16(&m,0xF000E0u,1u); /* video enabled */
}
static void fields(unsigned vp,unsigned vi,const unsigned *expect,unsigned n)
{
    unsigned hits=0u;
    init(vp,vi);
    for(unsigned f=0u;f<n;f++){
        int got;
        jagm_run_frames(&m,1u);
        assert(m.frames==(uint64_t)(f+1u));
        assert(m.vc==0u);
        got=(m.int1_pending&1u)!=0u;
        if(got!=(int)expect[f]){
            fprintf(stderr,"R245 VI cadence violation VP=%u VI=%u field=%u"
                   " observed=%u expected=%u\n",vp,vi,f+1u,
                   (unsigned)got,expect[f]);
            assert(got==(int)expect[f]);
        }
        hits+=(unsigned)got;
        jagm_write16(&m,0xF000E0u,0x0101u); /* clear latched video */
        assert((m.int1_pending&1u)==0u);
    }
    printf("VP=%u VI=%u: %u IRQs over %u fields\n",vp,vi,hits,n);
}
int main(void)
{
    /* Interlaced VP+1=5, fields 1 and 3 belong to parity-1 when VI=0;
     * other VI comparators match during parity-1's full field. */
    static const unsigned vi_zero[]={1u,0u,1u,0u};
    static const unsigned vi_middle[]={0u,1u,0u,1u};
    static const unsigned every[]={1u,1u,1u,1u};
    static const unsigned none[]={0u,0u,0u,0u};
    fields(4u,0u,vi_zero,4u);  /* VC=0 on field wrap */
    fields(4u,1u,vi_middle,4u);
    fields(4u,4u,vi_middle,4u);
    fields(4u,5u,none,4u);    /* outside the physical field */
    fields(5u,1u,every,4u);   /* noninterlaced: unchanged */
    fields(5u,5u,every,4u);
    fields(0u,0u,vi_zero,4u); /* smallest synthetic interlaced VP */
    puts("R245 TOM interlaced VI alternating-field cadence PASS");
    return 0;
}
