/* RE25 S48 (Claude): see include/jaguar/avp_gpu/files_gzip_service_native.h */
#include "jaguar/avp_gpu/files_gzip_service_native.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

int avp_jagfiles_gzip_service_native(void *opaque, uint32_t vector)
{
    AvpJagFilesGzipServiceNative *s=(AvpJagFilesGzipServiceNative*)opaque;
    const AvpView3DBus *bus;
    uint32_t in,out,len,cap;
    uint8_t *src,*dst;
    z_stream z;
    int rc;
    if(!s||!s->bus||!s->bus->rd||!s->bus->wr)return 0;
    bus=s->bus;
    if(vector==AVP_JAGFILES_GZIP_INIT_VECTOR1||vector==AVP_JAGFILES_GZIP_INIT_VECTOR2)return 1;
    if(vector!=AVP_JAGFILES_GZIP_EXP_VECTOR)return 0;
    in=bus->rd(bus->ctx,AVP_JAGFILES_GZIP_INPUT,4);
    out=bus->rd(bus->ctx,AVP_JAGFILES_GZIP_OUTPUT,4);
    if(in<s->archive_begin+8u||in>=s->archive_end||
       out<s->output_begin||out>=s->output_end)return 0;
    len=bus->rd(bus->ctx,in-4u,4);                  /* FILES record stored size */
    if(!len||(uint64_t)in+len>s->archive_end)return 0;
    cap=s->output_end-out;
    src=(uint8_t*)malloc(len);dst=(uint8_t*)malloc(cap);
    if(!src||!dst){free(src);free(dst);return 0;}
    for(uint32_t i=0;i<len;i++)src[i]=(uint8_t)bus->rd(bus->ctx,in+i,1);
    memset(&z,0,sizeof z);
    z.next_in=src;z.avail_in=len;z.next_out=dst;z.avail_out=cap;
    rc=inflateInit2(&z,15+16);
    if(rc==Z_OK){rc=inflate(&z,Z_FINISH);inflateEnd(&z);}
    if(rc!=Z_STREAM_END){free(src);free(dst);return 0;}
    for(uint32_t i=0;i<(uint32_t)z.total_out;i++)bus->wr(bus->ctx,out+i,dst[i],1);
    bus->wr(bus->ctx,AVP_JAGFILES_GZIP_RESULT,(uint32_t)z.total_out,4);
    /* the cartridge service leaves its stream pointers advanced */
    /* input stops at the end of the DEFLATE data: the 8-byte gzip trailer
     * (CRC-32, ISIZE) is not consumed by the cartridge inflater */
    if(z.total_in<8u){free(src);free(dst);return 0;}
    bus->wr(bus->ctx,AVP_JAGFILES_GZIP_INPUT,in+(uint32_t)z.total_in-8u,4);
    bus->wr(bus->ctx,AVP_JAGFILES_GZIP_OUTPUT,out+(uint32_t)z.total_out,4);
    s->last_size=(uint32_t)z.total_out;s->expands++;
    free(src);free(dst);
    return 1;
}
