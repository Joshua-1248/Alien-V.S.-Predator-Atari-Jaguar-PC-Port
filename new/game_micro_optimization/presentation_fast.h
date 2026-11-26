#ifndef AVP_MICRO_PRESENTATION_FAST_H
#define AVP_MICRO_PRESENTATION_FAST_H
#include <stddef.h>
#include <string.h>
/* Only elide the clear if the following image upload covers every pixel. */
static inline int avp_micro_full_cover(int enabled,int x,int y,unsigned w,unsigned h,unsigned iw,unsigned ih)
{
    return enabled && x==0 && y==0 && w!=0 && h!=0 && w==iw && h==ih;
}
typedef struct AvpMicroTitleCache { char text[280]; int valid; } AvpMicroTitleCache;
static inline int avp_micro_title_same(const AvpMicroTitleCache *c,int enabled,const char *title)
{
    return enabled && c->valid && strcmp(c->text,title)==0;
}
static inline void avp_micro_title_remember(AvpMicroTitleCache *c,int enabled,int success,const char *title)
{
    size_t n=strlen(title);
    c->valid=enabled && success && n<sizeof(c->text);
    if(c->valid)memcpy(c->text,title,n+1u);
}
#endif
