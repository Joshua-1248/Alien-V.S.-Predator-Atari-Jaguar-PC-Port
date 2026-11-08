#ifndef AVP_MAP_MAKER_TEXTURE_BROWSER_H
#define AVP_MAP_MAKER_TEXTURE_BROWSER_H
#include "map_maker.h"
#include "map_maker_overhead.h"
#include <stdint.h>
typedef int (*AvpMmBrowserLookup)(void *context,unsigned level,unsigned id,
                                   unsigned field,AvpMmOverheadTexture *texture);
typedef struct AvpMmBrowserLayout {
    int x0,y0;
    unsigned pitch,swatch;
} AvpMmBrowserLayout;
int avp_mm_browser_layout(unsigned width,unsigned height,AvpMmBrowserLayout *layout);
int avp_mm_browser_hit(unsigned width,unsigned height,int x,int y,
                        unsigned page,unsigned *id);
void avp_mm_browser_draw(const AvpMapMaker *m,uint32_t *pixels,
                          unsigned width,unsigned height,
                          AvpMmBrowserLookup lookup,void *context);
#endif
