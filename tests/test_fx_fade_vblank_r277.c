/* R277: MUSIC.o FXFadeUp ($9A80) chains its step ($9AF6) into the VBlank
 * handler, so the master volume rises on every VBlank (VSync) -- also while
 * MJP Do_Selec runs -- not only inside WaitFXFa.  Retail $9AF6: add step;
 * TST.W; BPL else clamp $7FFF and unhook. */
#include "music.h"
#include "objects.h"
#include "avp_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u32 master;static unsigned vbl;
static void setmv(void*u,u32 v){(void)u;master=v;}
static void wv(void*u){(void)u;++vbl;}
int main(void)
{
    AvpRuntimeOps o;memset(&o,0,sizeof o);
    o.set_master_volume=setmv;o.wait_vblank=wv;avp_runtime_bind(&o);
    InitSynt();
    FXFadeDo(0x7fff);WaitFXFa();assert(master==0u);   /* start from silence */
    vbl=0;FXFadeUp(0xfa);
    VSync();assert(master==0xfau);                   /* advances per VBlank */
    VSync();assert(master==0x1f4u);
    for(unsigned i=0;i<200u;i++)VSync();
    assert(master==0x7fffu);                          /* 131 steps, then clamp */
    { u32 m=master; VSync(); assert(master==m); }     /* unhooked when done */
    puts("R277 VBlank-chained FX fade PASS");
    return 0;
}
