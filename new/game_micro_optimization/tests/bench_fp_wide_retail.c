#define _POSIX_C_SOURCE 200809L
#include "../fp_wide_fast.h"
#include "jaguar/avp_gpu/fp_overlay.h"
#include "jaguar/cry.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static size_t readfile(const char *dir,int id,const char *tag,uint8_t *out,size_t cap){char path[1024];snprintf(path,sizeof path,"%s/%03d_%s.bin",dir,id,tag);FILE *f=fopen(path,"rb");assert(f);size_t n=fread(out,1,cap,f);assert(feof(f));fclose(f);return n;}
int main(int argc,char **argv){assert(argc==2);unsigned w=2560,h=1440;size_t n=(size_t)w*h;uint32_t *a=malloc(n*4),*b=malloc(n*4);assert(a&&b);uint8_t sprite[320*240+1],p[417],pal[208][3];double sums[2][2]={{0}};unsigned count=0;for(int four=0;four<2;four++)for(int i=0;;i++){int f,pf,sw,sh,x,y;if(four){AvpFpUseOverlayDesc d;if(!avp_gpu_fp_use_overlay_desc(i,&d))break;f=d.file;pf=d.palette_file;sw=d.width;sh=d.height;x=160+d.x_offset;y=114+d.y_offset;}else{AvpFpOverlayDesc d;if(!avp_gpu_fp_overlay_desc(i,&d))break;f=d.file;pf=d.palette_file;sw=d.width;sh=d.height;x=160+d.x_offset;y=114+d.y_offset;}size_t z=readfile(argv[1],f,"GZIP",sprite,sizeof sprite),pc=readfile(argv[1],pf,"NONE",p,sizeof p)/2;assert(pc<=208);for(unsigned j=0;j<pc;j++)avp_jaguar_cry16_to_rgb((p[2*j]<<8)|p[2*j+1],&pal[j][0],&pal[j][1],&pal[j][2]);for(size_t j=0;j<n;j++)a[j]=b[j]=(uint32_t)j*0x9e3779b9u;double total[2]={0};for(int k=0;k<4;k++)for(int q=0;q<2;q++){int opt=(k+q)%2;double t=now();for(int r=0;r<100;r++){if(opt)avp_micro_fp_wide_blit(b,w,h,sprite,z,sw,sh,pal,pc,x,y,four);else avp_gpu_fp_wide_blit(a,w,h,sprite,z,sw,sh,pal,pc,x,y,four);}total[opt]+=now()-t;}assert(!memcmp(a,b,n*4));sums[four][0]+=total[0];sums[four][1]+=total[1];count++;printf("file=%d four=%d baseline %.3f ms candidate %.3f ms speedup %.3fx\n",f,four,total[0]*2.5,total[1]*2.5,total[0]/total[1]);}printf("PASS %u retail overlays; bodies %.3fx flashes %.3fx\n",count,sums[0][0]/sums[0][1],sums[1][0]/sums[1][1]);free(a);free(b);}
