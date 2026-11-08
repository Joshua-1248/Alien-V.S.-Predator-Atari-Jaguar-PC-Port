/* R284: Windows PE game-state section (cmake/avp_game_state_pe.ld) gathers the
 * core game globals; F5/F9 session quickstate round-trips them.  Built and run
 * by tools/check_windows_build_r284.sh (MinGW-w64 + Wine). */
#include "pc/pc_quickstate.h"
#include "levels.h"
#include <stdio.h>
#include <string.h>
extern unsigned char __avp_game_state_start[],__avp_game_state_end[];
int main(void){
  u8 maze[32];u64 t=0;AvpPcQuickState*q;
  unsigned char*p=(unsigned char*)&cur_level;
  printf("core=%zu cur_level in range=%d\n",avp_pc_quickstate_core_bytes(),p>=__avp_game_state_start&&p<__avp_game_state_end);
  q=avp_pc_quickstate_create(); if(!q){puts("FAIL create");return 1;}
  memset(maze,7,sizeof maze); cur_level=5; new_level=9;
  if(!avp_pc_quickstate_save(q,maze,2,2,77)){puts("FAIL save");return 1;}
  cur_level=12;new_level=3;memset(maze,1,sizeof maze);
  if(!avp_pc_quickstate_load(q,&t)){puts("FAIL load");return 1;}
  if(cur_level!=5||new_level!=9||t!=77||maze[0]!=7){puts("FAIL restore");return 1;}
  puts("PE quickstate round-trip OK");return 0;}
