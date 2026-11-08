/* S23: observed CPU writers must be actual Musashi 68000 writes and must
 * never be fabricated from host, GPU, or Blitter memory operations. */
#include "jaguar/gpu_view_access.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static JagMachine machine;
static uint8_t cart[0x4000];
static JagGpuViewMap *map;
static unsigned at;
static void jr_inst(unsigned off,unsigned op,unsigned a,unsigned b)
{
    unsigned v=(op<<10)|(a<<5)|b;
    assert(off+2u<=4096u);
    machine.gpu.ram[off]=(uint8_t)(v>>8);
    machine.gpu.ram[off+1]=(uint8_t)v;
}
static void jr_movei(unsigned off,unsigned dst,uint32_t imm)
{
    jr_inst(off,38,0,dst);
    machine.gpu.ram[off+2]=(uint8_t)(imm>>8);
    machine.gpu.ram[off+3]=(uint8_t)imm;
    machine.gpu.ram[off+4]=(uint8_t)(imm>>24);
    machine.gpu.ram[off+5]=(uint8_t)(imm>>16);
}
static void w16(unsigned v)
{
    assert(at+2<=sizeof cart);
    cart[at++]=(uint8_t)(v>>8);cart[at++]=(uint8_t)v;
}
static void w32(uint32_t v){w16(v>>16);w16(v);}
static void move_l(uint32_t value,uint32_t address)
{
    w16(0x23FCu);w32(value);w32(address); /* 68000 MOVE.L #val,abs.l */
}
static void move_w(uint16_t value,uint32_t address)
{
    w16(0x33FCu);w16(value);w32(address); /* 68000 MOVE.W #val,abs.l */
}
static void move_b(uint8_t value,uint32_t address)
{
    w16(0x13FCu);w16(value);w32(address); /* 68000 MOVE.B #val,abs.l */
}
int main(void)
{
    FILE *f;char line[512];int found12=0,found13=0,found14=0;
    cart[0x404]=0;cart[0x405]=0x80;cart[0x406]=0x20;cart[0x407]=0;
    at=0x2000;
    move_l(0xAABBCCDDu,0x1200u);
    move_w(0x1234u,0x13FEu);
    move_b(0xB7u,0x1400u);
    w16(0x60FEu); /* BRA * */
    assert(jagm_init(&machine,cart,sizeof cart)==0);
    map=(JagGpuViewMap*)calloc(1,sizeof *map);assert(map);
    machine.on_cpu_dram_write=jag_gpu_view_cpu_write_observe;
    machine.on_cpu_dram_write_ctx=map;
    jagm_run_frames(&machine,1);
    assert(map->cpu_requests==3 && map->cpu_bad_requests==0);
    assert(map->writer[0x12].writes==1 && map->writer[0x13].writes==1);
    assert(map->writer[0x14].writes==1);
    assert(map->writer[0x12].seen && map->writer[0x12].first_field==0);
    assert(map->writer[0x12].last_pc68>=0x802000u);
    assert(map->writer[0x12].last_pc68<=0x802020u);
    assert(jagm_read32(&machine,0x1200u)==0xAABBCCDDu);
    assert(jagm_read16(&machine,0x13FEu)==0x1234u);
    assert(jagm_read8(&machine,0x1400u)==0xB7u);
    /* A host-side write must not be classified as a 68000 write. */
    jagm_write32(&machine,0x1500u,0xCAFEBABEu);
    assert(map->cpu_requests==3 && !map->writer[0x15].seen);
    /* Real JRISC reads the CPU-populated page and writes to a different
     * page; only its READ page should appear in the provenance report. */
    machine.on_cpu_dram_write=NULL;machine.on_cpu_dram_write_ctx=NULL;
    machine.on_gpu_view_dram_access=jag_gpu_view_access_observe;
    machine.on_gpu_view_dram_access_ctx=map;
    jr_movei(0,14,0x1200u);jr_inst(6,41,14,1); /* LOAD */
    jr_movei(8,15,0x1600u);jr_inst(14,47,15,1);/* STORE */
    jr_movei(16,20,0xF03020u);jr_inst(22,52,20,0); /* JUMP */
    jr_inst(24,57,0,0);
    machine.gpu.pc=0xF03000u;machine.gpu.go=1u;
    assert(jagm_run_until_gpu_pc(&machine,0xF03020u,0,2));
    assert(map->page[0x12].gpu_reads>0u);
    assert(map->page[0x16].gpu_writes>0u);
    assert(jagm_read32(&machine,0x1600u)==0xAABBCCDDu);
    /* Independent synthetic GPU/Blitter read events exercise an additional
     * CPU-written page and a consumed page with no observed CPU writer. */
    {
        JagGpuViewDramAccess e={0};
        e.master=3;e.address=0x1500u;e.width=1;
        jag_gpu_view_access_observe(NULL,&e,map);
        e.master=1;e.address=0x1400u;
        jag_gpu_view_access_observe(NULL,&e,map);
    }
    f=tmpfile();assert(f && jag_gpu_view_provenance_csv(map,f));
    rewind(f);assert(fgets(line,sizeof line,f));
    assert(strstr(line,"observed_cpu_writer,cpu_writes")!=NULL);
    while(fgets(line,sizeof line,f)){
        if(strncmp(line,"18,001200,",10)==0){found12=1;assert(strstr(line,",1,1,")!=NULL);}
        if(strncmp(line,"20,001400,",10)==0){found14=1;assert(strstr(line,",1,1,")!=NULL);}
        if(strncmp(line,"21,001500,",10)==0){found13=1;assert(strstr(line,",0,0,")!=NULL);}
        assert(strncmp(line,"19,001300,",10)!=0); /* written but not read */
    }
    assert(found12&&found13&&found14);
    fclose(f);
    /* One transaction touching both sides of a physical page boundary. */
    {
        JagCpuDramWriteEvent e={0};
        e.address=0x1FFFFEu;e.width=4;e.pc68=0x801234u;e.field=1234;
        jag_gpu_view_cpu_write_observe(NULL,&e,map);
        assert(map->writer[0x1FFF].writes==1);
        assert(map->writer[0].writes==1);
        assert(map->writer[0].last_field==1234);
        e.address=0x800000u;
        jag_gpu_view_cpu_write_observe(NULL,&e,map);
        assert(map->cpu_bad_requests==1);
    }
    machine.on_cpu_dram_write=NULL;machine.on_cpu_dram_write_ctx=NULL;
    free(map);
    puts("RE24 S23 real 68000 write provenance and read-consumer correlation PASS");
    return 0;
}
