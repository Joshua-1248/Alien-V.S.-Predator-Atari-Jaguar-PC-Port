/* R383: actual Software/Vulkan-reference raster dispatcher.
 * Ensure stable worker lifetime and identical outputs across full-resolution,
 * tiny, repeated and concurrent viewport passes without retail assets. */
#include "../new/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Fixture {
    unsigned width,height,seed;
    uint32_t *pixels;
} Fixture;
static uint32_t reference(unsigned x,unsigned y,unsigned seed)
{
    uint32_t v=(x*0x9e3779b9u)^(y*0x85ebca6bu)^(seed*0xc2b2ae35u);
    v^=v>>16;v*=0x7feb352du;v^=v>>15;
    return v;
}
static void paint(void *ctx,unsigned first,unsigned last)
{
    Fixture *f=(Fixture*)ctx;
    for(unsigned y=first;y<last;y++)
        for(unsigned x=0;x<f->width;x++)
            f->pixels[(size_t)y*f->width+x]=reference(x,y,f->seed);
}
static void verify(unsigned w,unsigned h,unsigned iterations)
{
    Fixture f={w,h,1u,NULL};
    const size_t count=(size_t)w*h;
    f.pixels=(uint32_t*)malloc((count?count:1u)*sizeof(uint32_t));
    assert(f.pixels);
    for(unsigned rep=0;rep<iterations;rep++){
        f.seed=rep+1u;
        for(size_t k=0;k<count;k++)f.pixels[k]=0xdeadbeefu;
        avp_raster_spans(paint,&f,h,(unsigned)count);
        for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)
            assert(f.pixels[(size_t)y*w+x]==reference(x,y,f.seed));
#ifndef _WIN32
        if(count>=640u*480u && sysconf(_SC_NPROCESSORS_ONLN)>=2){
            AvpRasterPool *pool=(AvpRasterPool*)pthread_getspecific(avp_raster_pool_key);
            assert(pool && pool->worker_count>=1u);
        }
#endif
    }
    free(f.pixels);
}
#ifndef _WIN32
static void *concurrent(void *ctx)
{
    const unsigned id=*(const unsigned*)ctx;
    verify(853u+id*13u,480u,24u);
    verify(1920u,1080u,5u);
    return NULL; /* TLS destructor must stop and join this thread's workers. */
}
#endif
int main(void)
{
    verify(0u,0u,2u);
    verify(320u,228u,16u);
    verify(853u,480u,16u);
    verify(1920u,1080u,36u);
    verify(2560u,1440u,4u);
#ifndef _WIN32
    {pthread_t threads[3];unsigned ids[3]={1u,2u,3u};
     for(unsigned i=0;i<3u;i++)assert(pthread_create(&threads[i],NULL,concurrent,&ids[i])==0);
     for(unsigned i=0;i<3u;i++)assert(pthread_join(threads[i],NULL)==0);}
#endif
#ifndef _WIN32
    {AvpRasterPool *pool=(AvpRasterPool*)pthread_getspecific(avp_raster_pool_key);
     if(pool){assert(pthread_setspecific(avp_raster_pool_key,NULL)==0);
              avp_raster_pool_destroy(pool);}}
#endif
    puts("R383 persistent Software raster workers: PASS");
    return 0;
}
