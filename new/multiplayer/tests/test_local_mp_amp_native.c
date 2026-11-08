/* Execute the ORIGINAL AMP.S allocator, generation, save/reset and collision
 * grid storage inside four independent world-state snapshots. No fake AI. */
#include "local_mp_amp_native.h"
#ifdef AVP_TEST_FAST_AMP_VALIDATOR
#include "amp_free_list.h"
#endif
#include "avp_runtime.h"
#include "collision.h"
#include <stdio.h>
#include <string.h>
/* Required original level identity for RAM creature-placement save. */
s16 cur_level;
u32 x_pos,y_pos;
s16 player_energy;
/* Link-only external dependencies of unrelated AMP modes, which this
 * allocator test never invokes. Native AMP allocator logic is NOT mocked. */
const AvpRuntimeOps *avp_runtime_ops(void){return NULL;}
u8 SafePos(u8 height,s32 x,s32 y,u16 width)
{(void)height;(void)x;(void)y;(void)width;return 0;}
void AreaAmpDamage(s32 x,s32 y,s32 r,s16 d,u16 f)
{(void)x;(void)y;(void)r;(void)d;(void)f;}
void AreaDamage(s32 x,s32 y,s32 r,s16 d,u16 f)
{(void)x;(void)y;(void)r;(void)d;(void)f;}

#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
int main(void)
{
    AvpMpAmpNativeState host,again,worlds[4],bad;
    AvpMpAmpNativeScope scope={0},nested={0};
    initialise();
#ifdef AVP_TEST_FAST_AMP_VALIDATOR
    avp_mp_amp_native_set_free_list_validator(avp_game_micro_amp_free_list_valid);
#endif
#ifdef AVP_TEST_FAST_AMP_VALIDATOR
    /* R14: compare the actual R13 staged/bitmap snapshot against the R14
     * direct destination path over distinct live AMP world mutations. */
    {
        static AvpMpAmpNativeState staged, direct;
        unsigned comparisons=0;
        for(unsigned i=0;i<1000u;i++){
            level1amps[i%AVP_NUM_AMPS].xpos=(s32)((i*671u) ^ 0x12345678u);
            level1amps[i%AVP_NUM_AMPS].ypos=(s32)((i*919u) ^ 0x0f084211u);
            collmap[i%(AVP_AMP_GRID_W*AVP_AMP_GRID_H)]=(u16)i;
            objmap[i%(AVP_AMP_OBJ_ROW_BYTES*AVP_AMP_OBJ_H)]=(u8)(i*13u);
            inventory_table[i%16u]=(u8)(i^17u);
            avp_mp_amp_native_set_direct_capture(0);
            CHECK(avp_mp_amp_native_capture(&staged));
            avp_mp_amp_native_set_direct_capture(1);
            CHECK(avp_mp_amp_native_capture(&direct));
            CHECK(memcmp(&staged,&direct,sizeof(staged))==0);
            comparisons++;
        }
        avp_mp_amp_native_set_direct_capture(0);
        initialise();
        CHECK(comparisons==1000u);
        /* An invalid live binding must refuse capture without altering its
         * caller's buffer. The R13 staged path has the same contract. */
        {
            AvpAmp *const saved=amp_data;
            static AvpMpAmpNativeState expected;
            memset(&expected,0xa5,sizeof(expected));
            amp_data=level1amps+1;
            staged=expected;
            direct=expected;
            CHECK(!avp_mp_amp_native_capture(&staged));
            avp_mp_amp_native_set_direct_capture(1);
            CHECK(!avp_mp_amp_native_capture(&direct));
            CHECK(memcmp(&expected,&staged,sizeof(expected))==0);
            CHECK(memcmp(&expected,&direct,sizeof(expected))==0);
            amp_data=saved;
        }
        avp_mp_amp_native_set_direct_capture(0);
        printf("PASS: %u complete original AMP snapshot byte-for-byte comparisons and invalid-state nonmutation\n",comparisons);
    }
#endif
    CHECK(avp_mp_amp_native_capture(&host));
#ifdef AVP_TEST_FAST_AMP_VALIDATOR
    avp_mp_amp_native_set_direct_capture(1);
#endif
    for (unsigned i=0;i<4;i++) {
        initialise();
        CHECK(avp_mp_amp_native_capture(&worlds[i]));
        /* Author a valid original allocator free-list, then exercise amp_req. */
        worlds[i].free_count=AVP_NUM_AMPS;
        for (unsigned n=0;n<AVP_NUM_AMPS;n++)
            worlds[i].free_list[n]=(u16)(AVP_NUM_AMPS-1u-n);
    }
    CHECK(avp_mp_amp_native_restore(&host));
    for (unsigned i=0;i<4;i++) {
        CHECK(avp_mp_amp_scope_enter(&scope,&worlds[i]));
        CHECK(!avp_mp_amp_scope_enter(&nested,&worlds[(i+1u)%4u]));
        AvpAmp *a=amp_req(); /* Original allocator (underflow semantics preserved). */
        CHECK(a==&level1amps[0]);
        a->creature=(u16)(i+1u);a->xpos=(s32)(0x10000u*i);
        a->ypos=0x20000;a->astype=11;a->host_static=1;
        cur_level=1;ram_save();
        collmap[i]=(u16)(80u+i);objmap[i]=(u8)(40u+i);
        levels_visit=(u16)(1u<<i);
        inventory_table[2]=(u8)(10+i);
        CHECK(avp_amp_slot_generation(0u)!=0);
        CHECK(avp_mp_amp_scope_leave(&scope,&worlds[i]));
        CHECK(!avp_mp_amp_scope_leave(&scope,&worlds[i]));
        CHECK(avp_mp_amp_native_capture(&again));
        CHECK(memcmp(&host,&again,sizeof(host))==0);
    }
    for (unsigned i=0;i<4;i++) {
        CHECK(avp_mp_amp_scope_enter(&scope,&worlds[i]));
        CHECK(level1amps[0].creature==(u16)(i+1u));
        CHECK(avp_amp_saved_level(1)[0].x==(s16)(256*i));
        CHECK(avp_amp_saved_level(1)[0].astype==11);
        CHECK(collmap[i]==(u16)(80u+i));
        CHECK(objmap[i]==(u8)(40u+i));
        CHECK(levels_visit==(u16)(1u<<i));
        CHECK(inventory_table[2]==(u8)(10+i));
        /* The ORIGINAL allocator release must affect only this world. */
        amp_release(1);
        CHECK(avp_mp_amp_scope_leave(&scope,&worlds[i]));
    }
    CHECK(avp_mp_amp_native_capture(&again));
    CHECK(memcmp(&host,&again,sizeof(host))==0);
    bad=worlds[0];bad.free_count=AVP_NUM_AMPS+1u;
    CHECK(!avp_mp_amp_native_restore(&bad));
    bad=worlds[0];bad.free_list[0]=bad.free_list[1];
    CHECK(!avp_mp_amp_native_restore(&bad));
    bad=worlds[0];bad.valid=0;
    CHECK(!avp_mp_amp_scope_enter(&scope,&bad));
    CHECK(!avp_mp_amp_native_capture(NULL));
    CHECK(!avp_mp_amp_native_restore(NULL));
    CHECK(avp_mp_amp_native_capture(&again));
    CHECK(memcmp(&host,&again,sizeof(host))==0);
#ifdef AVP_TEST_FAST_AMP_VALIDATOR
    avp_mp_amp_native_set_direct_capture(0);
    avp_mp_amp_native_set_free_list_validator(NULL);
    CHECK(avp_mp_amp_native_capture(&again));
    CHECK(memcmp(&host,&again,sizeof(host))==0);
#endif
    puts("PASS: four original AMP allocator/grid/world contexts, persistent state and host restoration; one process");
    return 0;
}
