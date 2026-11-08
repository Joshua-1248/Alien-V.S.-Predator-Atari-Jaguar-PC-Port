#include "pc/pc_original_cheat_input.h"
#include "joypad.h"
#include <string.h>
#define BIT(b) (1u<<(b))
#define DOWN(p,b) (!((p)&BIT(b)))
/* Original MAZESCRN.S::bnalist, including its intentional doubled '*'. */
static const unsigned banana_keys[]={FIRE_B,FIRE_A,KEY_9,FIRE_A,KEY_9,FIRE_A,
    KEY_STAR,OPTION,KEY_6,KEY_HASH,KEY_STAR,KEY_STAR,OPTION,KEY_2,OPTION};
void avp_pc_original_input_reset(AvpPcOriginalCheatInput *s)
{
    if(!s)return;
    memset(s,0,sizeof(*s));s->previous=0xffffffffu;
}
unsigned avp_pc_original_input_step(AvpPcOriginalCheatInput *s,u32 pad,
                                    int fuji_enabled,int banana_enabled)
{
    u32 edge,pressed;
    unsigned out=0;
    const u32 ones=BIT(KEY_1)|BIT(KEY_3);
    const u32 fours=BIT(KEY_2)|BIT(KEY_5)|BIT(KEY_7)|BIT(KEY_9);
    const u32 weapons=BIT(KEY_1)|BIT(KEY_2)|BIT(KEY_3)|BIT(KEY_4);
    const int pause=DOWN(pad,PAUSE),option=DOWN(pad,OPTION);
    if(!s)return 0;
    pressed=~pad;
    edge=pressed&s->previous;
    s->previous=pad;
    /* Hold That Fuji: hold Pause+Option; 1+3, release; 2+5+7+9. */
    if(!pause||!option)s->fuji=0;
    else switch(s->fuji){
    case 0:s->fuji=1;break;
    case 1:if((pressed&ones)==ones)s->fuji=2;
           break;
    case 2:if((pressed&ones)==0)s->fuji=3;break;
    case 3:if((pressed&fours)==fours){out|=AVP_PC_ORIG_FUJI;s->fuji=0;}break;
    default:s->fuji=0;break;
    }
    /* Global Positioning: hold Pause; Option, 1+3, Option again. */
    if(!pause)s->gps=0;
    else switch(s->gps){
    case 0:if(edge&BIT(OPTION))s->gps=1;
           break;
    case 1:if(!option && (pressed&ones)==ones)s->gps=2;break;
    case 2:if(!(pressed&ones))s->gps=3;break;
    case 3:if(edge&BIT(OPTION)){out|=AVP_PC_ORIG_GPS;s->gps=0;}break;
    default:s->gps=0;break;
    }
    /* Banana Stops: press/release Pause, Option, 6, 1+3; release;
     * then EXACTLY the 15 ordered keypad/fire-button presses in bnalist.
     * Unlike Fuji/GPS, retail DoPause does NOT require keeping Pause held
     * after the first press (the source waits in its ordinary pause menu). */
    switch(s->banana){
    case 0:if(edge&BIT(PAUSE))s->banana=1;break;
    case 1:if(option)s->banana=2;
           break;
    case 2:if((edge&BIT(KEY_6)) && !option)s->banana=3;
           break;
    case 3:if((pressed&ones)==ones)s->banana=4;
           break;
    case 4:if(!(pressed&(ones|BIT(PAUSE)|BIT(OPTION)))){
               s->banana=5;s->banana_index=0;
           }break;
    case 5:
        if(edge){
            const u32 expected=BIT(banana_keys[s->banana_index]);
            if(edge==expected){
                if(++s->banana_index==sizeof(banana_keys)/sizeof(banana_keys[0])){
                    out|=AVP_PC_ORIG_BANANA;s->banana=0;s->banana_index=0;
                }
            }else{s->banana=0;s->banana_index=0;}
        }
        break;
    default:s->banana=0;break;
    }
    /* During pause the digits are a code, never gameplay cheat commands. */
    if(!pause && option && (fuji_enabled||banana_enabled)){
        if((pressed&weapons)==weapons && (edge&weapons)){
            if(!(s->option_chord&1u))out|=AVP_PC_ORIG_AMMO;
            s->option_chord=1u;
        }else{
            s->option_chord=0;
            if(edge&BIT(KEY_1))out|=AVP_PC_ORIG_WEAPON1;
            if(edge&BIT(KEY_2))out|=AVP_PC_ORIG_WEAPON2;
            if(edge&BIT(KEY_3))out|=AVP_PC_ORIG_WEAPON3;
            if(edge&BIT(KEY_4))out|=AVP_PC_ORIG_WEAPON4;
        }
        if(edge&BIT(KEY_8))out|=AVP_PC_ORIG_TRACKER;
        if(edge&BIT(KEY_6))out|=AVP_PC_ORIG_SECURITY_UP;
        if(edge&BIT(KEY_9))out|=AVP_PC_ORIG_SECURITY_DOWN;
        if(banana_enabled){
            if(edge&BIT(KEY_5))out|=AVP_PC_ORIG_GOD;
            if(edge&BIT(FIRE_A))out|=AVP_PC_ORIG_LEVEL_NEXT;
            if(edge&BIT(FIRE_B))out|=AVP_PC_ORIG_LEVEL_PREV;
        }
    }else s->option_chord=0;
    return out;
}
