#include "pc/pc_win32_keymap.h"

unsigned char avp_pc_win32_keycode(unsigned scan_code,int extended)
{
    /* The PC game stores pressed keys by X11-style keycode. Preserve distinct
     * slots for enhanced keys (right Control/Alt, arrows and keypad Enter).
     * WM_KEY* lParam supplies the scan code and its extended-key flag. */
    return (unsigned char)((scan_code&0x7fu)|(extended?0x80u:0u));
}

unsigned long avp_pc_win32_keysym(unsigned virtual_key,unsigned scan_code,
                                   int shift_down,int extended)
{
    /* X11 keysyms are intentionally numeric here: compiling this source on
     * Windows must not depend on X11 headers or a separate X11 runtime. */
    if(virtual_key>=0x41u&&virtual_key<=0x5au)
        return virtual_key+0x20u; /* A..Z, unshifted XLookupKeysym(,0) */
    if(virtual_key>=0x30u&&virtual_key<=0x39u){
        if(shift_down&&virtual_key==0x38u)return 0x2au; /* * */
        if(shift_down&&virtual_key==0x33u)return 0x23u; /* # */
        return virtual_key;
    }
    if(virtual_key>=0x60u&&virtual_key<=0x69u)
        return virtual_key-0x30u; /* Numpad digits retain Jaguar keypad use */
    if(virtual_key>=0x70u&&virtual_key<=0x7bu)
        return 0xffbeu+(virtual_key-0x70u); /* F1..F12 */
    switch(virtual_key){
    case 0x08u:return 0xff08u; /* Backspace */
    case 0x09u:return 0xff09u; /* Tab */
    case 0x0du:return extended?0xff8du:0xff0du; /* KP Enter distinct from main Enter */
    case 0x10u:return scan_code==0x36u?0xffe2u:0xffe1u; /* Shift */
    case 0xa0u:return 0xffe1u; /* Left Shift */
    case 0xa1u:return 0xffe2u; /* Right Shift */
    case 0x11u:return extended?0xffe4u:0xffe3u; /* Control */
    case 0xa2u:return 0xffe3u;
    case 0xa3u:return 0xffe4u;
    case 0x12u:return extended?0xffeau:0xffe9u; /* Alt */
    case 0xa4u:return 0xffe9u;
    case 0xa5u:return 0xffeau;
    case 0x1bu:return 0xff1bu; /* Escape */
    case 0x20u:return 0x20u;   /* Space */
    case 0x21u:return 0xff55u; /* Page Up */
    case 0x22u:return 0xff56u; /* Page Down */
    case 0x23u:return 0xff57u; /* End */
    case 0x24u:return 0xff50u; /* Home */
    case 0x25u:return 0xff51u; /* Left */
    case 0x26u:return 0xff52u; /* Up */
    case 0x27u:return 0xff53u; /* Right */
    case 0x28u:return 0xff54u; /* Down */
    case 0x2du:return 0xff63u; /* Insert */
    case 0x2eu:return 0xffffu; /* Delete */
    case 0x6au:return 0x2au;   /* Numpad * */
    case 0x6bu:return 0x2bu;   /* Numpad + */
    case 0x6du:return 0x2du;   /* Numpad - */
    case 0x6eu:return 0x2eu;   /* Numpad decimal */
    case 0x6fu:return 0x2fu;   /* Numpad divide */
    case 0xbau:return 0x3bu;   /* ; */
    case 0xbbu:return 0x3du;   /* = */
    case 0xbcu:return 0x2cu;   /* , */
    case 0xbdu:return 0x2du;   /* - */
    case 0xbeu:return 0x2eu;   /* . */
    case 0xbfu:return 0x2fu;   /* / */
    case 0xc0u:return 0x60u;   /* ` */
    case 0xdbu:return 0x5bu;   /* [ */
    case 0xdcu:return 0x5cu;   /* backslash */
    case 0xddu:return 0x5du;   /* ] */
    case 0xdeu:return 0x27u;   /* apostrophe */
    default:return 0u;
    }
}
