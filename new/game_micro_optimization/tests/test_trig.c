#define _POSIX_C_SOURCE 200809L
#include "maze.h"
#include "game_micro_optimization.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static volatile s64 sink;
static u16 angles[4096];
static void bench(int on){s64 sum=0;avp_game_micro_opt_set(on);double t=now();for(unsigned i=0;i<6000000;i++){u16 a=angles[i&4095];sum+=sin_d0(a);sum+=cos_d0(a);}sink=sum;printf("Trig %s: %.3f ns/pair checksum %lld\n",on?"ON":"OFF",(now()-t)*1e9/6000000,(long long)sink);}
int main(void){for(unsigned i=0;i<65536;i++){avp_game_micro_opt_set(0);s32 s=sin_d0((u16)i),c=cos_d0((u16)i);avp_game_micro_opt_set(1);if(s!=sin_d0((u16)i)||c!=cos_d0((u16)i)){fprintf(stderr,"Mismatch at %u\n",i);return 1;}}puts("Trig: all 65536 angles, both sine and cosine, PASS");u32 r=12345;for(unsigned i=0;i<4096;i++){r=r*1664525u+1013904223u;angles[i]=(u16)(r>>16);}for(int i=0;i<3;i++){bench(i&1);bench(!(i&1));}return 0;}
