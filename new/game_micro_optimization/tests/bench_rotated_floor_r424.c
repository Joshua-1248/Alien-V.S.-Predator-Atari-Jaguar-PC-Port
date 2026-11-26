/* Compile with -Iinclude -I<directory containing point_floor_row_r423.h>.
 * Paired baseline/current measurement, with identical inputs and checksums. */
#define avp_micro_point_floor_row_impl baseline_row
#define avp_micro_point_floor_row baseline_wrapper
#define avp_micro_floor_defer_color_clear baseline_defer
#include "point_floor_row_r423.h"
#undef AVP_MICRO_POINT_FLOOR_ROW_H
#undef avp_micro_point_floor_row_impl
#undef avp_micro_point_floor_row
#undef avp_micro_floor_defer_color_clear
#define AVP_MICRO_TEST_ROTATED_FLOOR 1
#include "../point_floor_row.h"
#include <stdio.h>
#include <time.h>
#include <assert.h>
static u32 rgb[65536],row[2560];static u16 tex[128*128];static u8 maze[64*64*8],shades[256];
static const AvpGpuRgbImage *textures[256];static unsigned rotations[256];static AvpGpuRgbImage img;
static volatile u32 sink;
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
#define CALL(F) F(row,2560,maze,64,64,i&1u,28.123,27.456,dx,dy,textures,rotations,shades,rgb,1)
static double run(int fast,unsigned kind,u32 *hash){
    u32 h=0;const double start=seconds();
    for(unsigned i=0;i<10000;i++){
        double dx=(i&2u?-1:1)*0.0047293,dy=(i&4u?-1:1)*0.0036271;
        if(kind==1)dx=0.0;
        if(kind==2)dy=0.0;
        if(fast){CALL(avp_micro_point_floor_row_impl);}else{CALL(baseline_row);}
        h=(h*16777619u)^row[(i*113u)%2560u];
    }
    sink=h;*hash=h;return seconds()-start;
}
int main(void){
    img.width=img.height=128;img.cry16=tex;
    for(unsigned i=0;i<65536;i++)rgb[i]=i*2654435761u;
    for(unsigned i=0;i<128*128;i++)tex[i]=(u16)(i*3817u);
    for(unsigned i=0;i<sizeof maze;i++)maze[i]=(u8)(i*17u+(i>>7));
    for(unsigned i=0;i<256;i++)textures[i]=&img;
    avp_gpu_host_flat_row_shades(shades,-29);
    for(unsigned rot=0;rot<=3;rot++){
        for(unsigned i=0;i<256;i++)rotations[i]=rot;
        for(unsigned kind=0;kind<3;kind++){
            double a=0,b=0;u32 ha,hb;
            for(unsigned k=0;k<2;k++){
                if(k==0){a+=run(0,kind,&ha);b+=run(1,kind,&hb);}else{b+=run(1,kind,&hb);a+=run(0,kind,&ha);}
                assert(ha==hb);
            }
            printf("rot=%u axis=%u baseline=%.6f candidate=%.6f ratio=%.3fx hash=%08x\n",rot,kind,a,b,a/b,ha);
        }
    }
}
