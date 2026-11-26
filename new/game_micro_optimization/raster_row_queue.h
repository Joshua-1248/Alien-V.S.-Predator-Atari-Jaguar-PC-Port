#ifndef AVP_RASTER_ROW_QUEUE_H
#define AVP_RASTER_ROW_QUEUE_H
/* POSIX only: the existing synchronous worker pool owns the lifetime of
 * this stack context. Each claimed band has exactly one writer. No spin
 * waits: workers return as soon as the last band is claimed. */
#include <stdatomic.h>
typedef struct AvpRasterRowQueue {
    void (*rows)(void *,unsigned,unsigned);
    void *context;
    unsigned height,band;
    atomic_uint next;
} AvpRasterRowQueue;
static inline void avp_raster_row_queue_run(void *arg,unsigned first,unsigned last)
{
    AvpRasterRowQueue *q=(AvpRasterRowQueue *)arg;
    if(first==last)return;
    for(;;){
        unsigned y=atomic_fetch_add_explicit(&q->next,q->band,memory_order_relaxed);
        if(y>=q->height)return;
        unsigned remaining=q->height-y;
        q->rows(q->context,y,y+(remaining<q->band?remaining:q->band));
    }
}
#endif
