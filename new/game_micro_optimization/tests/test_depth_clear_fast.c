#include "../depth_clear_fast.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void){
    static const unsigned widths[]={1,17,320,1280,1921,2560,3840};
    static const unsigned heights[]={1,7,228,720,1081,1440,2160};
    size_t count=0;
    for(unsigned k=0;k<sizeof widths/sizeof widths[0];k++){
        unsigned w=widths[k],h=heights[k];size_t n=(size_t)w*h;
        float *a=malloc((n+2)*sizeof *a),*b=malloc((n+2)*sizeof *b);assert(a&&b);
        for(size_t i=0;i<n+2;i++)a[i]=b[i]=(float)(i%121)-60.0f;
        AvpMaze2Occlusion actual={w,h,a+1},reference={w,h,b+1};
        avp_gpu_maze2_occlusion_clear(&reference);
        for(unsigned p=7;p>0;p--)avp_micro_depth_clear_rows(&actual,(unsigned)((size_t)h*(p-1)/7),(unsigned)((size_t)h*p/7));
        assert(!memcmp(a,b,(n+2)*sizeof *a));
        count+=n;free(a);free(b);
    }
    printf("PASS: %zu exact depth cells, guard cells and reverse row partitions\n",count);
    return 0;
}
