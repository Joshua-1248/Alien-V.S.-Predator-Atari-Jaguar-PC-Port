#define _POSIX_C_SOURCE 200809L
#include "platform_pc_scale.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char**argv){
 unsigned sw=argc>1?atoi(argv[1]):320,sh=argc>2?atoi(argv[2]):228,dw=argc>3?atoi(argv[3]):2560,dh=argc>4?atoi(argv[4]):1440;
 if(!sw||!sh||!dw||!dh)return 2;
 uint32_t *src=malloc((size_t)sw*sh*4),*dst=malloc((size_t)dw*dh*4);
 if(!src||!dst)return 3;
 for(size_t i=0;i<(size_t)sw*sh;i++)src[i]=(uint32_t)((i*2654435761ULL)>>16);
 for(int round=0;round<5;round++){
  double start=seconds();uint64_t sum=0;
  for(int i=0;i<100;i++){
    avp_pc_scale_nearest32(src,sw,sh,dst,dw,dh);
    sum+=dst[(i*9923u)%((size_t)dw*dh)];
  }
  printf("round %d: %.4f ms/frame sum=%llu\n",round,(seconds()-start)*10,(unsigned long long)sum);
 }
 free(src);free(dst);return 0;
}
