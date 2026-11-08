#include "local_mp_bot_levels.h"
#include "avp_resources.h"
#include "doors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Preserve production's exact source-authored metadata without exporting a
 * new gameplay-global API. The included definitions remain in this test TU. */
#include "../../../src/game/door_retail_meta.c"
u32 levfx_ID;
u8 panel_list[AVP_MAX_PANELS];
u32 panel_pos;
int main(int argc,char **argv)
{
    AvpRomImage rom={0};
    AvpMpBotLevelLayout levels[AVP_MAX_LEVEL]={0};
    uint8_t *maps[AVP_MAX_LEVEL]={0};
    AvpMpBotLevelPlan plan;
    unsigned tested=0,found=0,vent=0,first_failed=0;
    FILE *f=NULL;
    int rc=1;
    long size;
    if(argc!=2){fprintf(stderr,"usage: audit ORIGINAL_RETAIL_ROM.jag\n");return 2;}
    f=fopen(argv[1],"rb");
    if(!f||fseek(f,0,SEEK_END)||(size=ftell(f))!=4194304L||fseek(f,0,SEEK_SET))goto done;
    rom.data=(u8*)malloc((size_t)size);if(!rom.data)goto done;
    rom.size=(size_t)size;
    if(fread(rom.data,1,rom.size,f)!=rom.size)goto done;
    fclose(f);f=NULL;
    /* Caller verifies the canonical ROM digest. This harness inspects only
     * the user's local authentic original file, never exports the bytes. */
    rom.exact_retail_sha256=1;
    InitDoors();
    memset(panel_list,0xa5,sizeof panel_list);
    panel_pos=37u;
    for(unsigned i=0;i<AVP_MAX_LEVEL;i++){
        size_t n=0;
        unsigned w,h;
        if(!avp_resources_decode_file(&rom,87u+i,NULL,&maps[i],&n) ||
           !maps[i]||n<16u)goto done;
        w=(unsigned)maps[i][0]*256u+maps[i][1];
        h=(unsigned)maps[i][2]*256u+maps[i][3];
        if(w==0||h==0||w>64u||h>64u||n!=8u+w*h*8u)goto done;
        if(!avp_resources_retail_panel_info(&rom,i+1u,
                      &levels[i].panel_ids,levels[i].panel_list,NULL))goto done;
        levels[i].map=(AvpMpBotMap){maps[i]+8u,w,h,fullbits,
                                    &retail_meta[i],10u,NULL,NULL};
        levels[i].meta=&retail_meta[i];
    }
    if(panel_pos!=37u)goto done;
    for(unsigned k=0;k<AVP_MAX_PANELS;k++)if(panel_list[k]!=0xa5u)goto done;
    /* The original bind operation must receive the SAME index positions
     * and count as the new pure query, preserving retail startup exactly. */
    for(unsigned i=0;i<AVP_MAX_LEVEL;i++){
        AvpDoorPanelIds bound;
        uint8_t verified[AVP_MAX_PANELS];
        unsigned verified_count=0u;
        if(!avp_resources_retail_panel_info(&rom,i+1u,&bound,verified,
                                            &verified_count)||
           !avp_resources_bind_retail_panel_info(&rom,i+1u,&bound)||
           panel_pos!=verified_count ||
           memcmp(panel_list,verified,sizeof verified))goto done;
    }
    for(unsigned i=0;i<AVP_MAX_LEVEL;i++){
        const AvpDoorLevelMeta *m=&retail_meta[i];
        if(i<5u)for(size_t j=0;j<m->access_count;j++){
            const AvpDoorAccessEntry *a=&m->access[j];
            unsigned width=levels[i].map.width;
            unsigned cell=a->door_offset/8u,x=cell%width,y=cell/width;
            unsigned ax=x,ay=y,bx=x,by=y;
            if(a->code!=AVP_ACC_JAMMED)continue;
            if(a->door_offset%8u==6u){if(!x)continue;ax=x-1u;}
            else if(a->door_offset%8u==7u){if(!y)continue;ay=y-1u;}
            else continue;
            tested++;
            if(avp_mp_bot_plan_levels(levels,0u,i+1u,
                             ax,ay,i+1u,bx,by,1,&plan)){
                found++;
                if(plan.kind==AVP_MP_BOT_TRANS_DUCT)vent++;
                printf("JAMMED_LEVEL %u cell(%u,%u) to (%u,%u) => detour via level %u %s (%u hops)\n",
                       i+1u,ax,ay,bx,by,plan.to_level,
                       plan.kind==AVP_MP_BOT_TRANS_DUCT?"VENT":"OTHER",plan.hops);
            }else{
                uint16_t groups[AVP_MP_BOT_MAP_CELLS]={0};
                unsigned comp=avp_mp_bot_components(&levels[i].map,groups);
                printf("NO DETOUR jam level%u door(%u,%u) side%u: comp %u / %u (total %u)\n",
                       i+1u,x,y,a->door_offset%8u,
                       groups[ay*64u+ax],groups[by*64u+bx],comp);
            }
        }
    }
    /* Every successful planned hop must match one of the real records, and
     * no synthetic path exists in a wholly nonfloor map. */
    {
        unsigned examined=0;
        for(unsigned i=0;i<AVP_MAX_LEVEL;i++){
            const AvpDoorLevelMeta *m=&retail_meta[i];
            for(size_t j=0;j<m->exit_count;j++){
                unsigned x=(m->exits[j].panel_offset/8u)%levels[i].map.width;
                unsigned y=(m->exits[j].panel_offset/8u)/levels[i].map.width;
                unsigned dst=m->exits[j].level;
                if(x>=levels[i].map.width||y>=levels[i].map.height||dst<1u||dst>15u)continue;
                examined++;
                if(!avp_mp_bot_plan_levels(levels,0u,i+1u,x,y,dst,0u,0u,0,&plan)){
                    printf("AUTHORED_EXIT_UNREACHABLE level%u tile(%u,%u) target%u panel_offset%u SOURCE panel:%u file:%u floor:%u DEST floor:%u\n",
                           i+1u,x,y,dst,m->exits[j].panel_offset,
                           levels[i].map.cells[m->exits[j].panel_offset],
                           levels[i].panel_list[(levels[i].map.cells[m->exits[j].panel_offset]&0x7f)-1u],
                           levels[i].map.cells[((size_t)y*levels[i].map.width+x)*8u+5u],
                           levels[dst-1u].map.cells[((size_t)m->exits[j].y*levels[dst-1u].map.width+m->exits[j].x)*8u+5u]);
                    first_failed++;
                }
            }
        }
        printf("RETAIL GRAPH: %u authored exits examined; %u first-hop searches unavailable; %u jammed side pairs inspected, %u detours (%u vent)\n",
               examined,first_failed,tested,found,vent);
        if(examined!=64u||tested!=8u||found<7u||vent<7u||first_failed)goto done;
    }
    rc=0;
done:
    if(f)fclose(f);
    for(unsigned i=0;i<AVP_MAX_LEVEL;i++)free(maps[i]);
    free(rom.data);
    return rc;
}
