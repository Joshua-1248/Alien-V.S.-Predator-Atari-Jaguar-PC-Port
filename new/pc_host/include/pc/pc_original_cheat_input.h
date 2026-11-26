#ifndef AVP_PC_ORIGINAL_CHEAT_INPUT_H
#define AVP_PC_ORIGINAL_CHEAT_INPUT_H
#include "avp_types.h"
/* A PC host interpreter for the original Jaguar keypad/PAUSE sequences.
 * It consumes native active-low pad samples, not keyboard symbols: remapped
 * Jaguar buttons work identically. The three retail activation patterns are
 * separate from the post-activation Option+key commands. No gameplay ticks. */
typedef struct AvpPcOriginalCheatInput {
    u32 previous;
    unsigned fuji,banana,gps,banana_index,option_chord;
} AvpPcOriginalCheatInput;
enum {
    AVP_PC_ORIG_FUJI=1u<<0, AVP_PC_ORIG_BANANA=1u<<1,
    AVP_PC_ORIG_GPS=1u<<2, AVP_PC_ORIG_GOD=1u<<3,
    AVP_PC_ORIG_AMMO=1u<<4, AVP_PC_ORIG_TRACKER=1u<<5,
    AVP_PC_ORIG_SECURITY_UP=1u<<6, AVP_PC_ORIG_SECURITY_DOWN=1u<<7,
    AVP_PC_ORIG_WEAPON1=1u<<8, AVP_PC_ORIG_WEAPON2=1u<<9,
    AVP_PC_ORIG_WEAPON3=1u<<10, AVP_PC_ORIG_WEAPON4=1u<<11,
    AVP_PC_ORIG_LEVEL_NEXT=1u<<12, AVP_PC_ORIG_LEVEL_PREV=1u<<13
};
void avp_pc_original_input_reset(AvpPcOriginalCheatInput *s);
unsigned avp_pc_original_input_step(AvpPcOriginalCheatInput *s,u32 pad,
                                   int fuji_enabled,int banana_enabled);
#endif
