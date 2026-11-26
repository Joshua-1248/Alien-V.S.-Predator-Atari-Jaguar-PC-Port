/* Regression for O(1) identity index replacing the 92-slot linear lookup. */
#include "jaguar/avp_gpu/texture_filter.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
u16 avp_jaguar_rgb_to_cry_qconv(u8 r,u8 g,u8 b,const u8 *q,size_t n)
{(void)q;(void)n;return (u16)(((unsigned)r<<8u)|((unsigned)g^b));}
int main(void)
{
    AvpGpuRgbImage original[74]={0},notice[18]={0};
    AvpGpuRgbImage hires[74]={0},hi_notice[18]={0};
    uint8_t rgb[4u*92u*2u];
    AvpFilterCache *cache=avp_filter_cache_create();
    assert(cache);
    for(unsigned i=0;i<92u;i++){
        uint8_t *p=rgb+i*8u;
        memset(p,(int)(i+1u),8u);
        AvpGpuRgbImage *o=i<74u?&original[i]:&notice[i-74u];
        o->rgb=p;o->width=1;o->height=1;
        AvpGpuRgbImage *h=i<74u?&hires[i]:&hi_notice[i-74u];
        *h=*o;
    }
    avp_filter_cache_prepare(cache,hires,hi_notice,original,notice,0,NULL,0);
    for(unsigned i=0;i<92u;i++){
        const AvpGpuRgbImage *src=i<74u?&original[i]:&notice[i-74u];
        assert(!avp_filter_cache_lookup_ready(cache,src));
        const AvpFilterImage *chain=avp_filter_cache_lookup(cache,src);
        assert(chain&&chain->source_rgb==src->rgb&&chain->levels==1u);
        assert(avp_filter_cache_lookup_ready(cache,src)==chain);
        AvpGpuRgbImage copy=*src;
        assert(avp_filter_cache_lookup_ready(cache,&copy)==chain);
        copy.width=2;
        assert(!avp_filter_cache_lookup_ready(cache,&copy));
    }
    /* Equal RGB identities in multiple slots must resolve to first match. */
    notice[2]=original[5];
    avp_filter_cache_prepare(cache,NULL,NULL,original,notice,0,NULL,0);
    const AvpFilterImage *first=avp_filter_cache_lookup(cache,&original[5]);
    assert(first&&first==avp_filter_cache_lookup(cache,&notice[2]));
    /* Replacement selected, old source no longer resolves at that slot. */
    uint8_t replacement[4]={7,8,9,10};
    hires[5].rgb=replacement;
    avp_filter_cache_prepare(cache,hires,hi_notice,original,notice,1,NULL,0);
    assert(!avp_filter_cache_lookup_ready(cache,&original[5]));
    assert(avp_filter_cache_lookup(cache,&hires[5]));
    assert(avp_filter_cache_lookup(cache,&original[5])==NULL);
    /* When a duplicate is selected in the replacement set, it is visible. */
    hi_notice[2]=notice[2];
    avp_filter_cache_prepare(cache,hires,hi_notice,original,notice,1,NULL,0);
    assert(avp_filter_cache_lookup(cache,&notice[2]));
    avp_filter_cache_clear(cache);
    assert(!avp_filter_cache_lookup_ready(cache,&notice[2]));
    assert(!avp_filter_cache_lookup(cache,&hires[5]));
    avp_filter_cache_destroy(cache);
    puts("R386 texture index: 92 identities, duplicates, replacement, invalidation PASS");
    return 0;
}
