#include "jaguar/avp_gpu/panels_cached.h"
#include <stdint.h>
#include <stddef.h>
/* Deliberately limited: no guessed GZIP/JPG decoder, special-panel or
 * Blitter overlay. S41 adds ROM-verified notice NONE/zero-fill only. */
static int walk(const AvpView3DBus *bus,const AvpJagFilesConfig *cfg,
                uint32_t list_begin,uint32_t list_end,uint32_t *next_list,
                int allow_notice)
{
    uint32_t p=list_begin;
    uint16_t index;
    unsigned entries=0u;
    if(!bus||!bus->rd||!bus->wr||!next_list||
       list_begin>list_end||list_end>0x200000u||list_end-list_begin<6u)return 0;
    {
        uint16_t start=(uint16_t)bus->rd(bus->ctx,p,2);
        p+=2u;
        index=(uint16_t)bus->rd(bus->ctx,AVP_JAG_PANEL_INDEX_ADDR,2);
        if(!(start&0x8000u)){
            index=start;
            bus->wr(bus->ctx,AVP_JAG_PANEL_INDEX_ADDR,index,2);
        }
    }
    for(;;){
        uint32_t ptr,notice;
        if(p>list_end-4u)return 0;
        ptr=bus->rd(bus->ctx,p,4);p+=4u;
        if(ptr&0x80000000u){*next_list=p;return 1;}
        if(p>list_end-4u||ptr>0x1FFFFCu||++entries>32768u)return 0;
        notice=bus->rd(bus->ctx,p,4);p+=4u;
        /* Original $F218 publishes the notice-buffer cursor to its
         * caller-owned descriptor before processing any IDs. */
        if(notice){
            if(!allow_notice || notice>0x1FFFFCu)return 0;
            bus->wr(bus->ctx,notice,
                    bus->rd(bus->ctx,AVP_JAG_PANEL_NOTICE_NEXT_ADDR,4),4);
        }
        {
            uint32_t slot=AVP_JAG_PANEL_SLOT_BASE + (uint32_t)index*AVP_JAG_PANEL_SLOT_BYTES;
            if(slot>0x200000u-AVP_JAG_PANEL_SLOT_BYTES)return 0;
            bus->wr(bus->ctx,ptr,slot,4);
        }
        for(;;){
            uint32_t file;
            if(p>=list_end)return 0;
            file=bus->rd(bus->ctx,p++,1);
            if(file==0xFFu){
                /* $F2BE: original byte-pointer word-alignment before RTS. */
                p=(p+1u)&~1u;
                break;
            }
            {
                uint16_t used=index;
                int missed;
                uint32_t working_a1;
                index=(uint16_t)(index+1u);
                bus->wr(bus->ctx,AVP_JAG_PANEL_INDEX_ADDR,index,2);
                /* $80 always invokes the special loader, even on cache hit.
                 * The reference implementation is only the cache-hit path. */
                if((uint32_t)used+AVP_JAG_PANEL_CACHE_BASE>=0x200000u)
                    return 0;
                missed=file==0x80u ||
                       bus->rd(bus->ctx,AVP_JAG_PANEL_CACHE_BASE+used,1)!=file;
                /* Original A1 points at the cache entry on a hit, but at
                 * the texture slot after $F274 on a miss. This matters
                 * for the retail unmapped-notice zero-fill fallback. */
                working_a1=missed?
                    AVP_JAG_PANEL_SLOT_BASE+(uint32_t)used*AVP_JAG_PANEL_SLOT_BYTES:
                    AVP_JAG_PANEL_CACHE_BASE+used;
                if(missed){
                    AvpJagFilesPlan plan;
                    uint32_t written;
                    /* F26C: original cache entry is written before dispatch. */
                    bus->wr(bus->ctx,AVP_JAG_PANEL_CACHE_BASE+used,file,1);
                    if(!cfg || (file&0x80u) ||
                       avp_jagfiles_plan(bus,cfg,file,&plan)!=AVP_JAGFILES_OK ||
                       plan.tag!=0x4E4F4E45u || plan.init_pc!=0u ||
                       avp_jagfiles_expand_none(bus,cfg,&plan,
                                                AVP_JAG_PANEL_SLOT_BASE+
                                                (uint32_t)used*AVP_JAG_PANEL_SLOT_BYTES,
                                                AVP_JAG_PANEL_SLOT_BASE+
                                                ((uint32_t)used+1u)*AVP_JAG_PANEL_SLOT_BYTES,
                                                &written)!=AVP_JAGFILES_OK)return 0;
                }
                if(notice){
                    uint32_t table=AVP_JAG_PANEL_NOTICE_MAP_BASE;
                    uint32_t mapped=0u;
                    uint32_t current=bus->rd(bus->ctx,AVP_JAG_PANEL_NOTICE_NEXT_ADDR,4);
                    int found=0,terminated=0;
                    /* Retail $F296: ordered original DRAM lookup (word pairs)
                     * with a signed-negative key terminator. No guessed map. */
                    for(unsigned n=0;n<64u;n++,table+=4u){
                        uint16_t key=(uint16_t)bus->rd(bus->ctx,table,2);
                        if(key&0x8000u){terminated=1;break;}
                        if(key==file){
                            mapped=bus->rd(bus->ctx,table+2u,2);
                            found=1;
                            break;
                        }
                    }
                    if(!terminated && !found)return 0;
                    if(current>0x200000u-256u)return 0;
                    if(found){
                        AvpJagFilesPlan plan;
                        uint32_t written=0u;
                        if(!cfg ||
                           avp_jagfiles_plan(bus,cfg,mapped,&plan)!=AVP_JAGFILES_OK ||
                           plan.tag!=0x4E4F4E45u || plan.init_pc!=0u ||
                           avp_jagfiles_expand_none(bus,cfg,&plan,current,current+256u,
                                                    &written)!=AVP_JAGFILES_OK ||
                           written!=256u)return 0;
                    }else{
                        /* Retail $F2C8 clears at the live A1, NOT $2F5BA:
                         * on a cache hit A1 is the cache-entry address;
                         * after a NONE miss it is the current texture slot.
                         * This counterintuitive original behavior was
                         * exposed by a full-DRAM retail-ROM comparison. */
                        if(working_a1>0x200000u-256u)return 0;
                        for(unsigned j=0;j<128u;j++)
                            bus->wr(bus->ctx,working_a1+2u*j,0u,2);
                    }
                    bus->wr(bus->ctx,AVP_JAG_PANEL_NOTICE_NEXT_ADDR,current+256u,4);
                }
            }
        }
    }
}

int avp_jagpanel_walk_cached(const AvpView3DBus *bus,
                             uint32_t list_begin,uint32_t list_end,
                             uint32_t *next_list)
{return walk(bus,NULL,list_begin,list_end,next_list,0);}
int avp_jagpanel_walk_cached_or_none(const AvpView3DBus *bus,
                                     const AvpJagFilesConfig *cfg,
                                     uint32_t list_begin,uint32_t list_end,
                                     uint32_t *next_list)
{return walk(bus,cfg,list_begin,list_end,next_list,0);}

int avp_jagpanel_walk_cached_or_none_notice(const AvpView3DBus *bus,
                                            const AvpJagFilesConfig *cfg,
                                            uint32_t list_begin,uint32_t list_end,
                                            uint32_t *next_list)
{return walk(bus,cfg,list_begin,list_end,next_list,1);}
