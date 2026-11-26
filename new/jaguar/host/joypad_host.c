#include "joypad.h"
#include "pc/pc_platform.h"

volatile u32 joy_edge=0xffffffffu,joy2_edge=0xffffffffu,joy_cur=0xffffffffu,joy2_cur=0xffffffffu;
volatile s8 joy_special_mode,reset_enabled,pad_flag2,pad_flag3;
volatile u16 reset_wait;
volatile u32 reset_time,frame_counter;
static u32 pc_pad=0xffffffffu;
static void (*pc_pad_poll)(void);
static void default_read_callback(void){}
void (*x_read)(void)=default_read_callback;
static u32 edge_from(u32 oldv,u32 cur){return (~(oldv^cur))|cur;}
void avp_pc_pad_set(u32 v){pc_pad=v;}
u32 avp_pc_pad_get(void){return pc_pad;}
void avp_pc_pad_set_poll_callback(void (*callback)(void)){pc_pad_poll=callback;}
void readpad(void){if(pc_pad_poll)pc_pad_poll();u32 p=pc_pad;joy_edge=edge_from(joy_cur,p);joy_cur=p;joy2_edge=edge_from(joy2_cur,0xffffffffu);joy2_cur=0xffffffffu;++frame_counter;x_read();}
void initpad(void){reset_wait=0;reset_enabled=-1;joy_special_mode=-1;pad_flag2=-1;pad_flag3=0;joy_cur=joy2_cur=0xffffffffu;joy_edge=joy2_edge=0xffffffffu;x_read=default_read_callback;}
