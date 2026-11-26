#include "../frame_copy_fast.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
int main(void){
    const unsigned dims[][2]={{1,1},{319,227},{1920,1080},{2560,1440},{2561,1441},{3840,2160}};
    size_t total=0;
    for(unsigned k=0;k<sizeof(dims)/sizeof(*dims);k++){
        unsigned w=dims[k][0],h=dims[k][1]; size_t n=(size_t)w*h;
        uint32_t *src=malloc((n+2)*4),*dst=malloc((n+2)*4);assert(src&&dst);
        for(size_t i=0;i<n+2;i++)src[i]=(uint32_t)i*0x9e3779b9u+0x1a2b3c4du;
        for(size_t i=0;i<n+2;i++)dst[i]=0xdeadbeefu;
        AvpMicroFrameCopy pass={src+1,dst+1,w};
        for(unsigned i=7;i>0;i--)avp_micro_frame_copy_rows(&pass,(unsigned)((uint64_t)h*(i-1)/7),(unsigned)((uint64_t)h*i/7));
        assert(!memcmp(src+1,dst+1,n*4));assert(dst[0]==0xdeadbeefu&&dst[n+1]==0xdeadbeefu);
        total+=n;free(src);free(dst);
    }
    printf("PASS: %zu pixels and both guard cells for all six sizes\n",total);
    return 0;
}
