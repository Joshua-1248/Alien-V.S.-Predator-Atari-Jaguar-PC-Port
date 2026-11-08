#include "pc/pc_win32_keymap.h"
#include <assert.h>
#include <stdio.h>

static void check(unsigned vk,unsigned scan,int shift,int extended,unsigned long sym)
{
    assert(avp_pc_win32_keysym(vk,scan,shift,extended)==sym);
}
int main(void)
{
    check('4',5,0,0,'4');
    check('8',9,0,0,'8');
    check('8',9,1,0,'*');
    check('3',4,1,0,'#');
    check('A',30,0,0,'a');
    check(0x64u,0,0,0,'4'); /* keypad */
    check(0x0du,0x1cu,0,0,0xff0du); /* main Enter paints */
    check(0x0du,0x1cu,0,1,0xff8du); /* keypad Enter changes view */
    check(0x70u,0,0,0,0xffbeu); /* F1 */
    check(0x71u,0,0,0,0xffbfu); /* F2 multiplayer menu */
    check(0x73u,0,0,0,0xffc1u); /* F4 */
    check(0x7bu,0,0,0,0xffc9u); /* F12 */
    check(0xbau,0,0,0,';');
    check(0xbdu,0,0,0,'-');
    check(0xbfu,0,0,0,'/');
    check(0xc0u,0,0,0,'`');
    check(0xdbu,0,0,0,'[');
    check(0xdcu,0,0,0,'\\');
    check(0xdeu,0,0,0,'\'');
    check(0x11u,0x1du,0,0,0xffe3u); /* left Control */
    check(0x11u,0x1du,0,1,0xffe4u); /* right Control */
    check(0x12u,0x38u,0,1,0xffeau); /* right Alt */
    check(0x10u,0x36u,1,0,0xffe2u); /* right Shift */
    check(0x2eu,0,0,0,0xffffu); /* Delete */
    assert(avp_pc_win32_keycode(0x1du,0)!=avp_pc_win32_keycode(0x1du,1));
    assert(avp_pc_win32_keysym(0xffu,0,0,0)==0u);
    puts("Win32 keysym routing tests passed");
    return 0;
}
