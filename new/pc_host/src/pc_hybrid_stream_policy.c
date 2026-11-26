#include "pc/pc_hybrid_stream_policy.h"
#include <stdint.h>
#include <string.h>

int avp_hybrid_stream_capacity(size_t required,size_t current,
    size_t maximum,size_t *planned,int *resize)
{
    size_t n;
    if(!planned||!resize||!required||!maximum||required>maximum||
       current>maximum)return 0;
    if(required<=current){*planned=current;*resize=0;return 1;}
    n=current?current:4096u;
    while(n<required){
        if(n>maximum/2u){n=maximum;break;}
        n*=2u;
    }
    if(n<required||n>maximum)return 0;
    *planned=n;*resize=1;
    return 1;
}

int avp_hybrid_stream_foreground_size(uint32_t width,uint32_t height,
    size_t *bytes)
{
    uint64_t n;
    if(!bytes||!width||!height||width>8192u||height>8192u)return 0;
    n=(uint64_t)width*(uint64_t)height*4u;
    if(n>AVP_HYBRID_FOREGROUND_MAX_BYTES||n>SIZE_MAX)return 0;
    *bytes=(size_t)n;
    return 1;
}

/* Conservative: absent/uninitialized image always needs a transfer,
 * even when the new pixels happen to match the unsubmitted staging data. */
int avp_hybrid_stream_foreground_upload_needed(const uint32_t *staging,
    const uint32_t *pixels,uint32_t width,uint32_t height,
    size_t staging_bytes,int image_uploaded)
{
    size_t bytes=0u;
    if(!pixels||!avp_hybrid_stream_foreground_size(width,height,&bytes))
        return -1;
    if(!image_uploaded)return 1;
    if(!staging||staging_bytes!=bytes)return -1;
    return memcmp(staging,pixels,bytes)!=0;
}
