#include "pc/pc_x11_held_keys.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL keymap %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    unsigned char held[256]={0};
    unsigned long sym[256]={0};
    char bitmap[32]={0};
    const unsigned codes[4]={25u,38u,39u,40u}; /* W/A/S/D keycodes on typical X11 */
    const unsigned long names[4]={'w','a','s','d'};
    for(unsigned j=0;j<4u;j++){
        const unsigned code=codes[j];
        bitmap[code>>3]|=(char)(1u<<(code&7u));
        CHECK(avp_pc_x11_physical_down(bitmap,code));
        avp_pc_x11_reconcile_key(&held[code],&sym[code],1,names[j]);
        CHECK(held[code] && sym[code]==names[j]);
        /* A synthetic X11 KeyRelease cleared our event-derived state. */
        held[code]=0;sym[code]=0;
        avp_pc_x11_reconcile_key(&held[code],&sym[code],1,names[j]);
        CHECK(held[code] && sym[code]==names[j]);
        /* The actual physical release must take effect immediately. */
        bitmap[code>>3]&=(char)~(1u<<(code&7u));
        avp_pc_x11_reconcile_key(&held[code],&sym[code],0,0);
        CHECK(!held[code] && !sym[code]);
    }
    CHECK(!avp_pc_x11_physical_down(bitmap,256));
    puts("S93 X11 held keys survive repeat releases, real releases stop input PASS");
    return 0;
}
