#include "../raster_row_queue.h"
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
typedef struct Surface { unsigned w,h; uint32_t *color; float *depth; atomic_uint *visits; } Surface;
static void rows(void *arg,unsigned first,unsigned last){
    Surface *s=arg; assert(first<last && last<=s->h);
    for(unsigned y=first;y<last;y++){
        assert(atomic_fetch_add_explicit(&s->visits[y],1,memory_order_relaxed)==0);
        uint32_t v=y*2654435761u+1;
        for(unsigned x=0;x<s->w;x++){
            for(unsigned k=0;k<1+(y%7);k++) v=v*1664525u+1013904223u;
            s->color[(size_t)y*s->w+x]=v;
            s->depth[(size_t)y*s->w+x]=FLT_MAX;
        }
    }
}
typedef struct Job {AvpRasterRowQueue *q;unsigned first,last;} Job;
static void *worker(void *arg){Job *j=arg;avp_raster_row_queue_run(j->q,j->first,j->last);return NULL;}
int main(void){
    const unsigned heights[]={1,3,31,32,33,127,719,720,721,1080,1439,1440,2160};
    const unsigned widths[]={1,19,321,1280,2560};
    unsigned long long pixels=0; unsigned cases=0;
    for(unsigned i=0;i<sizeof heights/sizeof *heights;i++)for(unsigned j=0;j<sizeof widths/sizeof *widths;j++){
        unsigned h=heights[i],w=widths[j];size_t n=(size_t)h*w;
        Surface a={w,h,malloc(n*4),malloc(n*sizeof(float)),malloc(h*sizeof(atomic_uint))};
        Surface b={w,h,malloc(n*4),malloc(n*sizeof(float)),malloc(h*sizeof(atomic_uint))};
        assert(a.color&&a.depth&&a.visits&&b.color&&b.depth&&b.visits);
        for(unsigned y=0;y<h;y++){atomic_init(&a.visits[y],0);atomic_init(&b.visits[y],0);}
        rows(&a,0,h);
        for(unsigned nworkers=1;nworkers<=8;nworkers++){
            memset(b.color,0x55,n*4); memset(b.depth,0,n*sizeof(float));
            for(unsigned y=0;y<h;y++)atomic_store(&b.visits[y],0);
            AvpRasterRowQueue q={rows,&b,h,32,ATOMIC_VAR_INIT(0)};
            Job jobs[8];pthread_t threads[7];
            for(unsigned k=0;k<nworkers;k++){
                jobs[k]=(Job){&q,h*k/nworkers,h*(k+1)/nworkers};
                if(k+1<nworkers)assert(!pthread_create(&threads[k],NULL,worker,&jobs[k]));
            }
            worker(&jobs[nworkers-1]);
            for(unsigned k=0;k+1<nworkers;k++)assert(!pthread_join(threads[k],NULL));
            assert(!memcmp(a.color,b.color,n*4));assert(!memcmp(a.depth,b.depth,n*sizeof(float)));
            for(unsigned y=0;y<h;y++)assert(atomic_load(&b.visits[y])==1);
            pixels+=n;cases++;
        }
        free(a.color);free(a.depth);free(a.visits);free(b.color);free(b.depth);free(b.visits);
    }
    printf("PASS %u cases, %llu color and depth pixels each, one visit per row\n",cases,pixels);
}
