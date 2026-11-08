#include "jaguar/gpu_view_access.h"
#include <limits.h>

static void inc(uint64_t *value,JagGpuViewMap *map)
{
    if(*value==UINT64_MAX)map->counter_overflows++;
    else ++*value;
}
void jag_gpu_view_access_observe(JagMachine *machine,
    const JagGpuViewDramAccess *event,void *user)
{
    JagGpuViewMap *map=(JagGpuViewMap*)user;
    uint32_t a,physical;
    (void)machine;
    if(!map||!event)return;
    if(event->address>=0x800000u||!event->width||event->width>4u||
       (event->master!=1u&&event->master!=3u)||event->is_write>1u){
        inc(&map->bad_requests,map);return;
    }
    if(event->master==1u)inc(&map->gpu_requests,map);
    else inc(&map->blitter_requests,map);
    a=event->address;
    physical=a&(JAGM_RAM_SIZE-1u);
    if((uint32_t)event->width>JAGM_RAM_SIZE-physical)
        inc(&map->wrap_requests,map);
    /* One touch per physical 256-byte page, even for a word crossing a page.
     * The real bus uses a 2 MiB repeating DRAM window. */
    for(unsigned i=0;i<event->width;){
        uint32_t page=(physical>>JAG_GPU_VIEW_PAGE_SHIFT);
        unsigned count=(unsigned)(JAG_GPU_VIEW_PAGE_BYTES-(physical&(JAG_GPU_VIEW_PAGE_BYTES-1u)));
        JagGpuViewPage *p=&map->page[page];
        if(count>event->width-i)count=event->width-i;
        if(!p->visited){p->visited=1u;p->first_pc=event->gpu_pc;}
        p->last_pc=event->gpu_pc;
        if(event->master==1u){
            if(event->is_write)inc(&p->gpu_writes,map);
            else inc(&p->gpu_reads,map);
        }else{
            if(event->is_write)inc(&p->blitter_writes,map);
            else inc(&p->blitter_reads,map);
        }
        physical=(physical+count)&(JAGM_RAM_SIZE-1u);
        i+=count;
    }
}
int jag_gpu_view_access_csv(const JagGpuViewMap *map,FILE *out)
{
    if(!map||!out)return 0;
    if(fputs("page,physical_start,gpu_reads,gpu_writes,blitter_reads,blitter_writes,first_gpu_pc,last_gpu_pc\n",out)==EOF)return 0;
    for(unsigned i=0;i<JAG_GPU_VIEW_PAGE_COUNT;i++){
        const JagGpuViewPage *p=&map->page[i];
        if(!p->visited)continue;
        if(fprintf(out,"%u,%06x,%llu,%llu,%llu,%llu,%06x,%06x\n",
           i,i*JAG_GPU_VIEW_PAGE_BYTES,
           (unsigned long long)p->gpu_reads,(unsigned long long)p->gpu_writes,
           (unsigned long long)p->blitter_reads,(unsigned long long)p->blitter_writes,
           p->first_pc,p->last_pc)<0)return 0;
    }
    return ferror(out)==0;
}

/* S23: real pre-view Musashi 68000 write provenance. No RAM contents. */
void jag_gpu_view_cpu_write_observe(JagMachine *machine,
    const JagCpuDramWriteEvent *event,void *user)
{
    JagGpuViewMap *map=(JagGpuViewMap*)user;
    uint32_t physical;
    (void)machine;
    if(!map||!event)return;
    if(event->address>=0x800000u||
       (event->width!=1u&&event->width!=2u&&event->width!=4u)||
       event->pc68>0xFFFFFFu){
        inc(&map->cpu_bad_requests,map);return;
    }
    inc(&map->cpu_requests,map);
    physical=event->address&(JAGM_RAM_SIZE-1u);
    for(unsigned i=0;i<event->width;){
        unsigned count=JAG_GPU_VIEW_PAGE_BYTES-(physical&(JAG_GPU_VIEW_PAGE_BYTES-1u));
        JagCpuWriterPage *p=&map->writer[physical>>JAG_GPU_VIEW_PAGE_SHIFT];
        if(count>event->width-i)count=event->width-i;
        if(!p->seen){
            p->seen=1u;p->first_pc68=event->pc68;
            p->first_field=event->field;
        }
        p->last_pc68=event->pc68;p->last_field=event->field;
        inc(&p->writes,map);
        physical=(physical+count)&(JAGM_RAM_SIZE-1u);
        i+=count;
    }
}
int jag_gpu_view_provenance_csv(const JagGpuViewMap *map,FILE *out)
{
    if(!map||!out)return 0;
    if(fputs("page,physical_start,gpu_reads,blitter_reads,observed_cpu_writer,cpu_writes,first_pc68,last_pc68,first_field,last_field\n",out)==EOF)return 0;
    /* Report consumed pages only. A zero CPU-writer flag is explicitly
     * unknown/preinitialized, not proof that the CPU never wrote this RAM. */
    for(unsigned i=0;i<JAG_GPU_VIEW_PAGE_COUNT;i++){
        const JagGpuViewPage *r=&map->page[i];
        const JagCpuWriterPage *w=&map->writer[i];
        if(!r->gpu_reads&&!r->blitter_reads)continue;
        if(fprintf(out,"%u,%06x,%llu,%llu,%u,%llu,%06x,%06x,%llu,%llu\n",
            i,i*JAG_GPU_VIEW_PAGE_BYTES,
            (unsigned long long)r->gpu_reads,
            (unsigned long long)r->blitter_reads,
            (unsigned)w->seen,(unsigned long long)w->writes,
            w->first_pc68,w->last_pc68,
            (unsigned long long)w->first_field,
            (unsigned long long)w->last_field)<0)return 0;
    }
    return ferror(out)==0;
}
