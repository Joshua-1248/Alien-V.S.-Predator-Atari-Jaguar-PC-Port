#include "jaguar/avp_gpu/files_gzip_vectors.h"
#include <stdint.h>

/* Deliberately isolated: the actual cartridge inflater at the three
 * original vector addresses is NOT reconstructed by these wrappers. */
static int valid_bus(const AvpView3DBus *bus,AvpJagFilesGzipVectorInvoke invoke)
{
    return bus && bus->rd && bus->wr && invoke;
}

int avp_jagfiles_gzip_init_vectors(const AvpView3DBus *bus,
                                   AvpJagFilesGzipVectorInvoke invoke,
                                   void *invoke_ctx)
{
    uint32_t work;
    if(!valid_bus(bus,invoke))return 0;
    work=bus->rd(bus->ctx,AVP_JAGFILES_GZIP_WORK_VARIABLE,4);
    bus->wr(bus->ctx,AVP_JAGFILES_GZIP_WORK_MAILBOX,work,4);
    if(!invoke(invoke_ctx,AVP_JAGFILES_GZIP_INIT_VECTOR1))return 0;
    return invoke(invoke_ctx,AVP_JAGFILES_GZIP_INIT_VECTOR2)!=0;
}

int avp_jagfiles_gzip_expand_vector(const AvpView3DBus *bus,
                                    uint32_t payload_address,
                                    uint32_t output_address,
                                    AvpJagFilesGzipVectorInvoke invoke,
                                    void *invoke_ctx,
                                    uint32_t *reported_size)
{
    uint32_t size;
    if(!valid_bus(bus,invoke)||!reported_size||
       payload_address>=0x1000000u||output_address>=0x1000000u)return 0;
    bus->wr(bus->ctx,AVP_JAGFILES_GZIP_INPUT,payload_address,4);
    bus->wr(bus->ctx,AVP_JAGFILES_GZIP_OUTPUT,output_address,4);
    if(!invoke(invoke_ctx,AVP_JAGFILES_GZIP_EXP_VECTOR))return 0;
    size=bus->rd(bus->ctx,AVP_JAGFILES_GZIP_RESULT,4);
    *reported_size=size;
    return 1;
}

int avp_jagfiles_gzip_dispatch_invoke(void *context,uint32_t routine_pc,
                                      uint32_t record_addr,uint32_t dest_addr)
{
    AvpJagFilesGzipDispatch *d=context;
    if(!d)return 0;
    if(routine_pc==AVP_JAGFILES_GZIP_INIT_PC)
        return avp_jagfiles_gzip_init_vectors(d->bus,d->invoke,d->invoke_ctx);
    if(routine_pc==AVP_JAGFILES_GZIP_EXP_PC)
        return avp_jagfiles_gzip_expand_vector(d->bus,record_addr,dest_addr,
                                                d->invoke,d->invoke_ctx,
                                                d->reported_size);
    return 0;
}
