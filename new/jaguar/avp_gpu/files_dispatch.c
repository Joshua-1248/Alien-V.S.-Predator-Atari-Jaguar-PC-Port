#include "jaguar/avp_gpu/files_dispatch.h"
#include <stdint.h>
#include <string.h>

static uint32_t rd(const AvpView3DBus *b,uint32_t address)
{ return b->rd(b->ctx,address,4); }
static int in_range(uint64_t a,uint64_t n,uint32_t lo,uint32_t hi)
{ return a>=lo && a<hi && n<=((uint64_t)hi-a); }

int avp_jagfiles_plan(const AvpView3DBus *bus,const AvpJagFilesConfig *cfg,
                      uint32_t index,AvpJagFilesPlan *result)
{
    AvpJagFilesPlan p;
    uint32_t a;
    unsigned j;
    if(!bus||!bus->rd||!cfg||!result||index>65535u||
       cfg->archive_begin>=cfg->archive_end||cfg->archive_end>0x1000000u||
       cfg->max_entries==0u||cfg->max_entries>65536u||
       (uint64_t)cfg->dispatch_table+12u*(uint64_t)cfg->max_entries>0x200000u||
       cfg->previous_tag>0x1FFFFCu)return AVP_JAGFILES_INVALID;
    memset(&p,0,sizeof p);
    a=rd(bus,cfg->first_pointer);
    for(uint32_t i=0;i<=index;i++){
        uint32_t size,tag;
        uint64_t next;
        if(!in_range(a,8u,cfg->archive_begin,cfg->archive_end))return AVP_JAGFILES_OUT_OF_RANGE;
        tag=rd(bus,a);
        if(tag==0u)return AVP_JAGFILES_NOT_FOUND;
        size=rd(bus,a+4u);
        if((uint64_t)a+8u+size>cfg->archive_end)return AVP_JAGFILES_OUT_OF_RANGE;
        if(i==index){p.header=a;p.payload=a+8u;p.tag=tag;p.index=index;break;}
        /* $BD2A-$BD36: MOVE.L size; ADDQ.L #3; AND.B #$FC;
         * ADDQ.L #8; ADDA.L to next record.  Enforce checked bounds
         * rather than reproducing a wraparound read into unrelated bus. */
        next=(uint64_t)a+8u+(((uint64_t)size+3u)&~3ull);
        if(next>UINT32_MAX || next<=a || !in_range(next,8u,cfg->archive_begin,cfg->archive_end))
            return AVP_JAGFILES_OUT_OF_RANGE;
        a=(uint32_t)next;
    }
    for(j=0;j<cfg->max_entries;j++){
        uint32_t table=cfg->dispatch_table+j*12u;
        uint32_t key=rd(bus,table);
        if(key==0u)return AVP_JAGFILES_NO_HANDLER;
        if(key!=p.tag)continue;
        p.table_entry=table;
        p.init_pc=rd(bus,table+4u);
        p.expand_pc=rd(bus,table+8u);
        if(p.expand_pc==0u)return AVP_JAGFILES_NO_HANDLER;
        p.call_init=(p.init_pc!=0u && rd(bus,cfg->previous_tag)!=p.tag);
        *result=p;
        return AVP_JAGFILES_OK;
    }
    return AVP_JAGFILES_NO_HANDLER;
}

int avp_jagfiles_execute(const AvpView3DBus *bus,const AvpJagFilesConfig *cfg,
                         const AvpJagFilesPlan *plan,uint32_t dest,
                         AvpJagFilesInvoke invoke,void *ctx)
{
    if(!bus||!bus->rd||!bus->wr||!cfg||!plan||!invoke||!plan->expand_pc||
       cfg->max_entries==0u||cfg->max_entries>65536u||
       (uint64_t)cfg->dispatch_table+12u*(uint64_t)cfg->max_entries>0x200000u||
       plan->table_entry<cfg->dispatch_table||
       (uint64_t)plan->table_entry>=((uint64_t)cfg->dispatch_table+12u*cfg->max_entries)||
       (plan->table_entry-cfg->dispatch_table)%12u!=0u||
       !in_range(plan->header,8u,cfg->archive_begin,cfg->archive_end)||
       plan->payload!=plan->header+8u||rd(bus,plan->header)!=plan->tag||
       rd(bus,plan->table_entry)!=plan->tag||
       rd(bus,plan->table_entry+8u)!=plan->expand_pc||
       rd(bus,plan->table_entry+4u)!=plan->init_pc)return AVP_JAGFILES_INVALID;
    /* Compute the decision again if the caller's last initialized codec
     * changed since plan construction.  This avoids a stale plan. */
    if(plan->init_pc!=0u && rd(bus,cfg->previous_tag)!=plan->tag){
        if(!invoke(ctx,plan->init_pc,plan->header,dest))return AVP_JAGFILES_INVALID;
        bus->wr(bus->ctx,cfg->previous_tag,plan->tag,4);
    }
    return invoke(ctx,plan->expand_pc,plan->payload,dest)?AVP_JAGFILES_OK:AVP_JAGFILES_INVALID;
}

int avp_jagfiles_expand_none(const AvpView3DBus *bus,const AvpJagFilesConfig *cfg,
                             const AvpJagFilesPlan *plan,uint32_t dest,
                             uint32_t dest_end,uint32_t *written)
{
    uint32_t size;
    uint64_t src_end,dst_end;
    if(!bus||!bus->rd||!bus->wr||!cfg||!plan||
       plan->tag!=0x4E4F4E45u /* NONE */ || plan->init_pc!=0u ||
       plan->expand_pc==0u||cfg->max_entries==0u||
       cfg->max_entries>65536u||
       (uint64_t)cfg->dispatch_table+12u*(uint64_t)cfg->max_entries>0x200000u||
       plan->table_entry<cfg->dispatch_table||
       (uint64_t)plan->table_entry>=((uint64_t)cfg->dispatch_table+12u*cfg->max_entries)||
       (plan->table_entry-cfg->dispatch_table)%12u!=0u||
       rd(bus,plan->table_entry)!=plan->tag ||
       rd(bus,plan->table_entry+4u)!=0u ||
       rd(bus,plan->table_entry+8u)!=plan->expand_pc ||
       plan->payload!=plan->header+8u ||
       !in_range(plan->header,8u,cfg->archive_begin,cfg->archive_end) ||
       dest>dest_end || dest_end>0x1000000u)return AVP_JAGFILES_INVALID;
    size=rd(bus,plan->header+4u);
    src_end=(uint64_t)plan->payload+size;
    dst_end=(uint64_t)dest+size;
    if(src_end>cfg->archive_end||dst_end>dest_end ||
       ((uint64_t)dest<src_end && dst_end>plan->payload))
        return AVP_JAGFILES_OUT_OF_RANGE;
    for(uint32_t i=0;i<size;i++){
        uint32_t value=bus->rd(bus->ctx,plan->payload+i,1);
        bus->wr(bus->ctx,dest+i,value,1);
    }
    if(written)*written=size;
    return AVP_JAGFILES_OK;
}
