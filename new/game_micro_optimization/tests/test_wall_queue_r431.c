#include "../raster_row_queue.h"
#include "../opaque_wall_fast.h"
#include <assert.h>
#include <float.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t rgb[65536];
static uint16_t tex[16384];
typedef struct Surface {unsigned w,h;uint32_t *color;float *depth;double *z;atomic_uint *visits;} Surface;
static void columns(void *arg,unsigned first,unsigned last){
    Surface *s=arg;
    assert(first<last && last<=s->w);
    for(unsigned x=first;x<last;x++){
        assert(atomic_fetch_add_explicit(&s->visits[x],1,memory_order_relaxed)==0);
        int h=(int)s->h, a=(int)((x*19u)%s->h),b=h;
        int wh=128+h+(int)(x%113u);
        float d=(float)(x%73u+1u)/16.0f;
        s->z[x]=d;
        if(x%3u==0u){ /* Layered wall; preserve ordered depth tests. */
            avp_micro_opaque_wall128(s->color,s->depth,s->w,x,0,b,0,wh,tex,x%128,0,-35,rgb,d+1.f);
            avp_micro_opaque_wall128(s->color,s->depth,s->w,x,a,b,a,wh,tex,(x+17)%128,0,-19,rgb,d);
        }else{
            avp_micro_opaque_wall128_fresh(s->color,s->depth,s->w,x,a,b,a,wh,tex,x%128,0,-35,rgb,d);
        }
    }
}
static void init(Surface *s){size_t n=(size_t)s->w*s->h;for(size_t i=0;i<n;i++){s->color[i]=0x112233u+(uint32_t)i;s->depth[i]=FLT_MAX;}for(unsigned x=0;x<s->w;x++){atomic_store(&s->visits[x],0);s->z[x]=-1.;}}
static void *worker(void *p){avp_raster_row_queue_run(p,0,1);return NULL;}
int main(void){
    for(unsigned i=0;i<65536;i++)rgb[i]=(i*2654435761u)&0xffffffu;
    for(unsigned i=0;i<16384;i++)tex[i]=(uint16_t)(i*31337u);
    const unsigned ws[]={1,3,63,64,65,127,319,1280,1281,1920,2559,2560,3840},hs[]={1,17,228,720,1440};
    unsigned long long entries=0,cols=0;unsigned cases=0;
    for(unsigned i=0;i<sizeof ws/sizeof *ws;i++)for(unsigned j=0;j<sizeof hs/sizeof *hs;j++){
        unsigned w=ws[i],h=hs[j];size_t n=(size_t)w*h;
        Surface a={w,h,malloc(n*4),malloc(n*sizeof(float)),malloc(w*sizeof(double)),malloc(w*sizeof(atomic_uint))};
        Surface b={w,h,malloc(n*4),malloc(n*sizeof(float)),malloc(w*sizeof(double)),malloc(w*sizeof(atomic_uint))};
        assert(a.color&&a.depth&&a.z&&a.visits&&b.color&&b.depth&&b.z&&b.visits);
        for(unsigned x=0;x<w;x++){atomic_init(&a.visits[x],0);atomic_init(&b.visits[x],0);}init(&a);columns(&a,0,w);
        for(unsigned k=1;k<=8;k++){
            init(&b);AvpRasterRowQueue q={columns,&b,w,64,ATOMIC_VAR_INIT(0)};
            pthread_t threads[7];for(unsigned t=0;t+1<k;t++)assert(!pthread_create(&threads[t],NULL,worker,&q));worker(&q);for(unsigned t=0;t+1<k;t++)assert(!pthread_join(threads[t],NULL));
            assert(!memcmp(a.color,b.color,n*4));assert(!memcmp(a.depth,b.depth,n*sizeof(float)));assert(!memcmp(a.z,b.z,w*sizeof(double)));
            for(unsigned x=0;x<w;x++)assert(atomic_load(&b.visits[x])==1);
            entries+=n;cols+=w;cases++;
        }
        free(a.color);free(a.depth);free(a.z);free(a.visits);free(b.color);free(b.depth);free(b.z);free(b.visits);
    }
    printf("PASS %u cases, %llu color/depth entries each, %llu column depths, exactly one visit per column\n",cases,entries,cols);
}
