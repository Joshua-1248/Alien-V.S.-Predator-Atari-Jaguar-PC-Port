#ifndef AVP_JAGUAR_AVP_GPU_HUD_RMW_HOST_COMPOSITE_H
#define AVP_JAGUAR_AVP_GPU_HUD_RMW_HOST_COMPOSITE_H

#include <stdint.h>
#include <stddef.h>

#define AVP_HUD_NATIVE_PIXELS (320u*228u)
#define AVP_HUD_REPLAY_MAX_INKS 8u

typedef struct AvpHudRmwOps {
    uint16_t ink[AVP_HUD_REPLAY_MAX_INKS];
    uint8_t count; /* 255 means too many overlapping ink operations. */
} AvpHudRmwOps;

typedef uint32_t (*AvpHudRmwApply)(void *context,uint32_t background,uint16_t ink);

static inline void avp_gpu_hud_replay_record(AvpHudRmwOps *ops,size_t native_offset,uint16_t ink)
{
    AvpHudRmwOps *p;
    if(!ops||native_offset>=AVP_HUD_NATIVE_PIXELS||!ink)return;
    p=&ops[native_offset];
    if(p->count<AVP_HUD_REPLAY_MAX_INKS)p->ink[p->count++]=ink;
    else p->count=255u;
}

/* The retail HUD remains a 320x228 native brush, but every translucent
 * read-modify-write ink must operate on each actual host scene pixel.
 * Copying the already-blended 320x228 result would overwrite detailed 3D
 * scenery with enlarged low-resolution samples inside the translucent HUD.
 * Non-RMW opaque HUD messages deliberately keep their native appearance. */
static inline void avp_gpu_hud_replay_scaled_composite(
    uint32_t *host,unsigned width,unsigned height,
    const uint32_t *native_before,const uint32_t *native_after,
    const AvpHudRmwOps *ops,AvpHudRmwApply apply,void *context,
    uint32_t *background_mask,unsigned background_stride)
{
    unsigned xe[321],ye[229];
    unsigned x,y;
    if(!host||!native_before||!native_after||!ops||!apply||!width||!height)return;
    for(x=0;x<=320u;x++)xe[x]=(unsigned)(((uint64_t)x*width+319u)/320u);
    for(y=0;y<=228u;y++)ye[y]=(unsigned)(((uint64_t)y*height+227u)/228u);
    for(y=0;y<228u;y++){
        unsigned y0=ye[y],y1=ye[y+1u];
        if(y0==y1)continue;
        for(x=0;x<320u;x++){
            const size_t i=(size_t)y*320u+x;
            const AvpHudRmwOps *p=&ops[i];
            unsigned x0=xe[x],x1=xe[x+1u],yy,xx;
            int replay_ok=p->count>0u && p->count<=AVP_HUD_REPLAY_MAX_INKS;
            uint32_t simulated=native_before[i];
            if(!p->count && native_before[i]==native_after[i])continue;
            if(replay_ok){
                for(unsigned n=0;n<p->count;n++)simulated=apply(context,simulated,p->ink[n]);
                if(simulated!=native_after[i])replay_ok=0; /* opaque glyph after the ink */
            }
            for(yy=y0;yy<y1;yy++){
                uint32_t *row=host+(size_t)yy*width;
                uint32_t *mask=background_mask?background_mask+(size_t)yy*background_stride:NULL;
                for(xx=x0;xx<x1;xx++){
                    uint32_t out=replay_ok?row[xx]:native_after[i];
                    if(replay_ok)for(unsigned n=0;n<p->count;n++)out=apply(context,out,p->ink[n]);
                    row[xx]=out;
                    if(mask)mask[xx]=1u;
                }
            }
        }
    }
}
#endif
