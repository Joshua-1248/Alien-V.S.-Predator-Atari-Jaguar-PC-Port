#ifndef AVP_HUD_SPARSE_RESET_H
#define AVP_HUD_SPARSE_RESET_H
#include "jaguar/avp_gpu/hud_rmw_host_composite.h"
#include <stdlib.h>
#include <string.h>
#define AVP_HUD_RESET_LIMIT 8192u
/* Count is the validity marker. Stale inks are never used. Dense passes
 * fall back to a full clear, and the list saturates rather than overflowing. */
typedef struct AvpHudSparseReplay {
    AvpHudRmwOps ops[AVP_HUD_NATIVE_PIXELS];
    unsigned touched_count;
    int sparse_enabled;
    uint32_t touched[AVP_HUD_RESET_LIMIT];
} AvpHudSparseReplay;
static inline AvpHudSparseReplay *avp_hud_sparse_create(void){return calloc(1,sizeof(AvpHudSparseReplay));}
static inline void avp_hud_sparse_reset(AvpHudSparseReplay *p){
    if(p->touched_count>AVP_HUD_RESET_LIMIT)memset(p->ops,0,sizeof p->ops);
    else for(unsigned i=0;i<p->touched_count;i++)p->ops[p->touched[i]].count=0;
    p->touched_count=0;
}
/* Latch the setting at the beginning of every native replay pass. Disabled
 * passes use the original whole-buffer zero and do not maintain the list.
 * A disabled-to-enabled transition must clear the whole buffer once, since
 * the previous pass did not track its touched indices. */
static inline AvpHudRmwOps *avp_hud_sparse_begin(AvpHudSparseReplay *p,int enabled){
    if(!p)return NULL;
    p->sparse_enabled=!!enabled;
    if(p->sparse_enabled)avp_hud_sparse_reset(p);
    else{
        memset(p->ops,0,sizeof p->ops);
        p->touched_count=AVP_HUD_RESET_LIMIT+1u;
    }
    return p->ops;
}
static inline void avp_hud_sparse_record(AvpHudSparseReplay *p,size_t offset,uint16_t ink){
    if(!p||offset>=AVP_HUD_NATIVE_PIXELS||!ink)return;
    if(!p->sparse_enabled){avp_gpu_hud_replay_record(p->ops,offset,ink);return;}
    if(!p->ops[offset].count && p->touched_count<=AVP_HUD_RESET_LIMIT){
        if(p->touched_count<AVP_HUD_RESET_LIMIT)p->touched[p->touched_count]=offset;
        ++p->touched_count;
    }
    avp_gpu_hud_replay_record(p->ops,offset,ink);
}
#endif
