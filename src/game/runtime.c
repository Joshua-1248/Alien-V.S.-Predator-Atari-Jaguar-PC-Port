#include "avp_runtime.h"
#include <string.h>
static AvpRuntimeOps g_ops;
void avp_runtime_bind(const AvpRuntimeOps *ops){ if(ops) g_ops=*ops; else memset(&g_ops,0,sizeof(g_ops)); }
const AvpRuntimeOps *avp_runtime_ops(void){ return &g_ops; }
void avp_runtime_reset_ops(void){ memset(&g_ops,0,sizeof(g_ops)); }
/* MUSIC.o FXFadeDo/FXFadeUp ($9A5C/$9A80) chain their step routine into the
 * VBlank handler ($2EE80): the master-volume fade advances on EVERY VBlank,
 * concurrently with whatever the 68000 is doing, not only inside WaitFXFa. */
static void (*g_fx_vblank)(void);
void avp_runtime_set_fx_vblank(void (*fn)(void)){ g_fx_vblank=fn; }
void avp_runtime_fx_vblank(void){ if(g_fx_vblank) g_fx_vblank(); }
