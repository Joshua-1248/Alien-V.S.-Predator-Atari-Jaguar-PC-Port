/* S22: synthetic-only Jaguar DRAM view access accounting. */
#include "jaguar/gpu_view_access.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static JagGpuViewMap *map;
static void add(uint32_t addr,unsigned width,unsigned master,unsigned wr,uint32_t pc)
{
    JagGpuViewDramAccess e;
    memset(&e,0,sizeof e);
    e.address=addr;e.width=(uint8_t)width;e.master=(uint8_t)master;
    e.is_write=(uint8_t)wr;e.gpu_pc=pc;
    jag_gpu_view_access_observe(NULL,&e,map);
}
int main(void)
{
    FILE *f;char buf[1024];size_t n;
    map=calloc(1,sizeof *map);assert(map);
    add(0x00100u,4,1,0,0xF03220u);
    add(0x001FEu,4,1,1,0xF03226u); /* crosses 256-byte boundary */
    add(0x200080u,1,3,0,0xF03228u); /* mirrored to 0x0080 */
    add(0x1FFFFEu,4,3,1,0xF0322Au); /* wraps to 0x000000 */
    add(0x7FFFFFu,1,3,0,0xF0322Cu); /* last physical byte */
    add(0x800000u,1,3,0,0xF03230u); /* invalid, not DRAM */
    add(0x00100u,0,1,0,0xF03232u); /* invalid width */
    assert(map->gpu_requests==2 && map->blitter_requests==3);
    assert(map->bad_requests==2 && map->wrap_requests==1);
    assert(map->page[1].gpu_reads==1 && map->page[1].gpu_writes==1);
    assert(map->page[2].gpu_writes==1 && map->page[2].first_pc==0xF03226u);
    assert(map->page[0].blitter_reads==1 && map->page[0].blitter_writes==1);
    assert(map->page[JAG_GPU_VIEW_PAGE_COUNT-1u].blitter_reads==1);
    assert(map->page[JAG_GPU_VIEW_PAGE_COUNT-1u].blitter_writes==1);
    assert(map->page[1].first_pc==0xF03220u && map->page[1].last_pc==0xF03226u);
    f=tmpfile();assert(f && jag_gpu_view_access_csv(map,f));
    rewind(f);n=fread(buf,1,sizeof buf-1,f);buf[n]=0;
    assert(strstr(buf,"page,physical_start,gpu_reads")!=NULL);
    assert(strstr(buf,"1,000100,1,1,0,0,f03220,f03226")!=NULL);
    fclose(f);
    free(map);
    puts("RE24 S22 GPU/Blitter DRAM access page accounting PASS");
    return 0;
}
