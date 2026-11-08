/* R134: compare actual host HUD sampler against the shipped R133
 * per-native-pixel sampling loop. Uses synthetic host pixels, no ROM. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void r133_hud_sample_native(u32 *before,const AvpJaguarFramebuffer *fb)
{
    for(unsigned y=0;y<228u;y++){
        unsigned fy0=(unsigned)(((unsigned long long)(2u*y+1u)*fb->height)/(2u*228u));
        if(fy0>=fb->height)fy0=fb->height-1u;
        for(unsigned x=0;x<320u;x++){
            unsigned fx0=(unsigned)(((unsigned long long)(2u*x+1u)*fb->width)/(2u*320u));
            size_t idx=(size_t)y*320u+x;
            if(fx0>=fb->width)fx0=fb->width-1u;
            before[idx]=fb->pixels[(size_t)fy0*fb->width+fx0];
        }
    }
}

int main(void)
{
    static const unsigned dims[][2]={
        {1,1},{1,228},{320,1},{2,3},{17,11},{159,113},{319,227},
        {320,228},{321,229},{426,300},{640,456},{800,600},{853,480},
        {1024,768},{1280,720},{1600,900},{1920,1080},{2048,1536},
        {2560,1440},{3440,1440},{3840,2160},{4096,2160},{400,2000},
        {2000,400},{15,4096},{4096,15}
    };
    enum { NATIVE_PIXELS=320*228 };
    u32 *actual=malloc((NATIVE_PIXELS+2u)*sizeof(u32));
    u32 *expected=malloc((NATIVE_PIXELS+2u)*sizeof(u32));
    unsigned count=0u;
    assert(actual&&expected);
    for(size_t s=0;s<sizeof(dims)/sizeof(dims[0]);s++){
        unsigned w=dims[s][0],h=dims[s][1];
        size_t n=(size_t)w*h;
        u32 *pixels=malloc(n*sizeof(u32));
        AvpJaguarFramebuffer fb={w,h,pixels};
        assert(pixels);
        for(unsigned variant=0;variant<4u;variant++){
            for(size_t i=0;i<n;i++){
                u32 value=(u32)(i*0x9e3779b9u+(i>>4)*0x85ebca6bu+variant*0x27d4eb2du);
                pixels[i]=(variant==0u)?value:
                          (variant==1u)?0u:
                          (variant==2u)?(value&0xff00ffffu):
                          ((i%127u==0u)?0x00ffffffu:value);
            }
            actual[0]=expected[0]=0xfacefeedu;
            actual[NATIVE_PIXELS+1u]=expected[NATIVE_PIXELS+1u]=0xcafef00du;
            avp_gpu_hud_sample_native(actual+1u,&fb);
            r133_hud_sample_native(expected+1u,&fb);
            if(memcmp(actual,expected,(NATIVE_PIXELS+2u)*sizeof(u32))!=0){
                fprintf(stderr,"R134 HUD sampling mismatch %ux%u variant %u\n",w,h,variant);
                abort();
            }
            ++count;
        }
        free(pixels);
    }
    free(actual);free(expected);
    printf("R134 HUD native sample vs R133 parity PASS (%u complete tiles)\n",count);
    return 0;
}
