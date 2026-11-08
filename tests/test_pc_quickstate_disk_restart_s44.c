/* Two-process archive test: invoke once with 'save', then with 'load'.
 * All resources are synthetic and contain no game assets. */
#include "pc/pc_quickstate.h"
#include "jaguar/avp_gpu/state.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
__asm__(".pushsection .data\n"
        ".globl __avp_game_state_start\n"
        "__avp_game_state_start:\n"
        ".zero 128\n"
        ".globl __avp_game_state_end\n"
        "__avp_game_state_end:\n"
        ".popsection\n");
extern unsigned char __avp_game_state_start[];
int main(int argc,char **argv)
{
    AvpRomImage rom={0};AvpMazeCandidate mc={0};AvpGpuAutomapSnapshot map={0};
    AvpPcQuickState *q;unsigned char *maze,*romdata;u64 ticks=0;uintptr_t ptr=0;
    assert(argc==2);assert(avp_pc_quickstate_core_bytes()==128);
    maze=(unsigned char*)malloc(40);romdata=(unsigned char*)malloc(32);
    assert(maze&&romdata);
    memset(maze,0x11,40);memset(romdata,0x55,32);
    rom.data=romdata;rom.size=32;rom.exact_retail_sha256=1;
    strcpy(rom.sha256_hex,"c18d92e99a4547a8ebef0c5091d57c0245065d7a54b0e85f0b2e68784f93c783");
    mc.file_index=89;mc.width=2;mc.height=2;mc.decoded=maze;mc.decoded_size=40;
    q=avp_pc_quickstate_create();assert(q);
    if(strcmp(argv[1],"save")==0){
        memset(__avp_game_state_start,0x37,128);
        ptr=(uintptr_t)maze+8u;memcpy(__avp_game_state_start+16,&ptr,sizeof(ptr));
        ptr=(uintptr_t)__avp_game_state_start+48u;memcpy(__avp_game_state_start+32,&ptr,sizeof(ptr));
        ptr=(uintptr_t)romdata+10u;memcpy(__avp_game_state_start+64,&ptr,sizeof(ptr));
        memset(maze+8,0xa5,32);map.level=3;map.seen[1]=42;
        assert(avp_pc_quickstate_save(q,maze+8,2,2,555u));
        assert(avp_pc_quickstate_disk_save(q,&rom,3,&map,sizeof(map)));
        puts("S44 durable quicksave WRITE PASS");
    }else if(strcmp(argv[1],"reject")==0){
        memset(__avp_game_state_start,0x22,128);
        assert(!avp_pc_quickstate_disk_load(q,&rom,&mc,1,&map,sizeof(map)));
        assert(!avp_pc_quickstate_valid(q));
        assert(__avp_game_state_start[0]==0x22&&__avp_game_state_start[127]==0x22);
        puts("S44 damaged quicksave REJECT PASS");
    }else{
        memset(__avp_game_state_start,0x22,128);
        assert(avp_pc_quickstate_disk_load(q,&rom,&mc,1,&map,sizeof(map)));
        assert(avp_pc_quickstate_load(q,&ticks));
        assert(ticks==555u&&map.level==3&&map.seen[1]==42);
        assert(__avp_game_state_start[0]==0x37&&__avp_game_state_start[127]==0x37);
        memcpy(&ptr,__avp_game_state_start+16,sizeof(ptr));assert(ptr==(uintptr_t)maze+8);
        memcpy(&ptr,__avp_game_state_start+32,sizeof(ptr));assert(ptr==(uintptr_t)__avp_game_state_start+48);
        memcpy(&ptr,__avp_game_state_start+64,sizeof(ptr));assert(ptr==(uintptr_t)romdata+10);
        assert(maze[8]==0xa5&&maze[39]==0xa5);
        puts("S44 durable quicksave RESTART+RELOCATE PASS");
    }
    avp_pc_quickstate_destroy(q);free(maze);free(romdata);return 0;
}
