/* Retail Jaguar PAUSE cheat sequences, decoded from actual active-low pad bits.
 * No ROM content is distributed, no renderer/player globals required. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include "pc/pc_original_cheat_input.h"
#include "joypad.h"
#define B(n) (1u<<(n))
#define PAD(...) (~(0u __VA_ARGS__))
static void step(AvpPcOriginalCheatInput *s,u32 down,unsigned expected,
                 int fuji,int banana)
{unsigned got=avp_pc_original_input_step(s,~down,fuji,banana);
 if(got!=expected){fprintf(stderr,"fail: down=%08x got=%04x expected=%04x banana=%u index=%u fuji=%u gps=%u\n",(unsigned)down,got,expected,s->banana,s->banana_index,s->fuji,s->gps);assert(got==expected);}}
int main(void)
{
    AvpPcOriginalCheatInput s;
    const u32 pause=B(PAUSE),option=B(OPTION);
    const u32 a=B(KEY_1)|B(KEY_3),b=B(KEY_2)|B(KEY_5)|B(KEY_7)|B(KEY_9);
    const unsigned banana_code[]={FIRE_B,FIRE_A,KEY_9,FIRE_A,KEY_9,FIRE_A,
        KEY_STAR,OPTION,KEY_6,KEY_HASH,KEY_STAR,KEY_STAR,OPTION,KEY_2,OPTION};
    avp_pc_original_input_reset(&s);
    step(&s,0,0,0,0);
    step(&s,pause|option,0,0,0);
    step(&s,pause|option|a,0,0,0);
    step(&s,pause|option,0,0,0);
    step(&s,pause|option|b,AVP_PC_ORIG_FUJI,0,0);
    step(&s,0,0,1,0);
    step(&s,option|B(KEY_1),AVP_PC_ORIG_WEAPON1,1,0);
    step(&s,option,0,1,0);
    step(&s,option|B(KEY_8),AVP_PC_ORIG_TRACKER,1,0);
    step(&s,option,0,1,0);
    step(&s,option|B(KEY_6),AVP_PC_ORIG_SECURITY_UP,1,0);
    step(&s,option,0,1,0);
    step(&s,option|B(KEY_9),AVP_PC_ORIG_SECURITY_DOWN,1,0);
    step(&s,option,0,1,0);
    step(&s,option|B(KEY_1)|B(KEY_2)|B(KEY_3)|B(KEY_4),AVP_PC_ORIG_AMMO,1,0);
    step(&s,option|B(KEY_1)|B(KEY_2)|B(KEY_3)|B(KEY_4),0,1,0);
    step(&s,0,0,1,0);
    /* Original global-positioning code is a distinct pause-time path. */
    avp_pc_original_input_reset(&s);
    step(&s,pause,0,0,0);
    step(&s,pause|option,0,0,0);
    step(&s,pause,0,0,0);
    step(&s,pause|a,0,0,0);
    step(&s,pause,0,0,0);
    step(&s,pause|option,AVP_PC_ORIG_GPS,0,0);
    step(&s,0,0,0,0);
    /* Banana's 15 native symbol entries are required in exact order. */
    avp_pc_original_input_reset(&s);
    step(&s,pause,0,0,0);
    step(&s,0,0,0,0); /* original code releases Pause */
    step(&s,option,0,0,0);
    step(&s,0,0,0,0);
    step(&s,B(KEY_6),0,0,0);
    step(&s,0,0,0,0);
    step(&s,a,0,0,0);
    step(&s,0,0,0,0);
    for(unsigned i=0;i<sizeof(banana_code)/sizeof(banana_code[0]);i++){
        step(&s,B(banana_code[i]),i+1==sizeof(banana_code)/sizeof(banana_code[0])?
             AVP_PC_ORIG_BANANA:0,0,0);
        step(&s,0,0,0,0);
    }
    step(&s,option|B(KEY_5),AVP_PC_ORIG_GOD,0,1);
    step(&s,option,0,0,1);
    step(&s,option|B(KEY_5),AVP_PC_ORIG_GOD,0,1);
    step(&s,option,0,0,1);
    step(&s,option|B(FIRE_A),AVP_PC_ORIG_LEVEL_NEXT,0,1);
    step(&s,option,0,0,1);
    step(&s,option|B(FIRE_B),AVP_PC_ORIG_LEVEL_PREV,0,1);
    /* Incorrect codes never enable Fuji/Banana or surprise-toggle GPS. */
    avp_pc_original_input_reset(&s);
    step(&s,pause|option,0,0,0);
    step(&s,pause|option|a,0,0,0);
    step(&s,pause|option,0,0,0);
    step(&s,option|b,0,0,0); /* PAUSE released early */
    step(&s,0,0,0,0);
    puts("PASS original Jaguar input: Fuji, GPS, Banana, commands, God, edges, early release");
    return 0;
}
