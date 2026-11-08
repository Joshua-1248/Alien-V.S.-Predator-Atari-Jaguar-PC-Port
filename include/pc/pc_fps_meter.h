#ifndef AVP_PC_FPS_METER_H
#define AVP_PC_FPS_METER_H

#include <stdint.h>

/* Counts actual host presentation submissions, never Jaguar simulation ticks.
 * No allocations, frame-limiter changes, platform timers or gameplay globals.
 * One-second sampling intentionally avoids per-frame floating-point work. */
typedef struct AvpPcFpsMeter {
    uint64_t start_ns;
    uint64_t frames;
    unsigned display_fps;
    int sample_valid;
} AvpPcFpsMeter;

static inline void avp_pc_fps_meter_reset(AvpPcFpsMeter *m)
{
    if(!m)return;
    m->start_ns=0u;
    m->frames=0u;
    m->display_fps=0u;
    m->sample_valid=0;
}

static inline void avp_pc_fps_meter_present(AvpPcFpsMeter *m,uint64_t now_ns)
{
    uint64_t elapsed;
    if(!m)return;
    if(!m->start_ns || now_ns<m->start_ns){
        m->start_ns=now_ns?now_ns:1u;
        m->frames=0u;
        m->sample_valid=0;
        return; /* first present establishes the sampling epoch */
    }
    ++m->frames;
    elapsed=now_ns>=m->start_ns?now_ns-m->start_ns:0u;
    if(elapsed>=UINT64_C(1000000000)){
        /* Frames <= 1000/second for requested host rates; protect arithmetic
         * against unexpected presentation bursts or long process suspends. */
        uint64_t fps=(m->frames<=UINT64_MAX/UINT64_C(1000000000))
            ? (m->frames*UINT64_C(1000000000)+elapsed/2u)/elapsed
            : 0u;
        m->display_fps=(unsigned)(fps>999999u?999999u:fps);
        m->sample_valid=1;
        m->start_ns=now_ns;
        m->frames=0u;
    }
}

#endif
