#include "jaguar/avp_gpu/world_scene_gpu_wire.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t read32(const uint8_t *b)
{
    return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
}
static double f64(const uint8_t *b)
{
    uint64_t n=(uint64_t)read32(b)|((uint64_t)read32(b+4)<<32);
    double v;
    memcpy(&v,&n,8u);
    return v;
}
int avp_gpu_wall_texture_file(unsigned level,u8 wall,unsigned *file)
{
    if(level!=1u || wall>=74u)return 0;
    if(file)*file=wall;
    return 1;
}
int avp_gpu_wall_notice_index(unsigned level,u8 wall,unsigned *index)
{
    if(level!=1u || wall!=16u)return 0;
    if(index)*index=8u;
    return 1;
}
int avp_gpu_flat_texture_file(unsigned level,u8 flat,unsigned *file)
{
    if(level!=1u || flat>=74u)return 0;
    if(file)*file=flat;
    return 1;
}
unsigned avp_gpu_flat_texture_rotation(unsigned level,u8 flat)
{
    return level==1u ? flat&3u : 0u;
}
int avp_gpu_wall_window_pair_file(unsigned level,u8 wall,unsigned *file)
{
    if(level!=1u || wall!=16u)return 0;
    if(file)*file=74u;
    return 1;
}

int main(void)
{
    AvpWorldScenePacket p={0};
    AvpWorldGpuWire wire={0};
    uint8_t maze[8]={0};
    const uint32_t w=1920u,h=1080u;
    assert(avp_world_scene_init(&p,w,h));
    p.maze=malloc(sizeof(maze));assert(p.maze);
    memcpy(p.maze,maze,sizeof(maze));
    p.maze_capacity=sizeof(maze);p.map_width=p.map_height=1u;
    assert(!avp_world_gpu_wire_pack(&wire,&p));
    for(uint32_t x=0;x<w;x++) {
        for(uint32_t i=0;i<3u;i++) {
            double z=(double)x/3.0+(double)i/1024.0;
            double uv=(double)(x%71u)/71.0+(double)i/8192.0;
            assert(avp_world_scene_hit(&p,x,z,uv,10u+i,2u+i,i&1u,
                i==2u,-100+(int)i,500+(int)i));
        }
        avp_world_scene_end_column(&p,x,(double)x/17.0);
    }
    assert(avp_world_scene_complete(&p));
    assert(avp_world_gpu_wire_pack(&wire,&p));
    size_t n=AVP_WORLD_GPU_WIRE_HEADER_BYTES+(size_t)w*AVP_WORLD_GPU_WIRE_COLUMN_BYTES+
              (size_t)w*3u*AVP_WORLD_GPU_WIRE_HIT_BYTES;
    assert(wire.bytes==n);
    assert(read32(wire.data)==AVP_WORLD_GPU_WIRE_MAGIC);
    assert(read32(wire.data+4)==1u && read32(wire.data+8)==w &&
           read32(wire.data+12)==h && read32(wire.data+16)==w*3u);
    assert(read32(wire.data+20)==AVP_WORLD_GPU_WIRE_HEADER_BYTES);
    assert(read32(wire.data+24)==AVP_WORLD_GPU_WIRE_HEADER_BYTES+
           w*AVP_WORLD_GPU_WIRE_COLUMN_BYTES);
    for(uint32_t x=0u;x<w;x++) {
        const uint8_t *c=wire.data+AVP_WORLD_GPU_WIRE_HEADER_BYTES+
                         x*AVP_WORLD_GPU_WIRE_COLUMN_BYTES;
        assert(read32(c)==x*3u && read32(c+4)==3u);
        assert(f64(c+8)==(double)x/17.0);
        assert(read32(c+16)==1u && read32(c+20)==0u);
        for(uint32_t i=0;i<3u;i++) {
            const uint8_t *d=wire.data+read32(wire.data+24)+
                             ((size_t)x*3u+i)*AVP_WORLD_GPU_WIRE_HIT_BYTES;
            assert(f64(d)==(double)x/3.0+(double)i/1024.0);
            assert(f64(d+8)==(double)(x%71u)/71.0+(double)i/8192.0);
            assert(read32(d+16)==10u+i && read32(d+20)==2u+i);
            assert(read32(d+24)==(i&1u) && read32(d+28)==(i==2u));
            assert((int32_t)read32(d+32)==-100+(int)i);
            assert((int32_t)read32(d+36)==500+(int)i);
        }
    }
    uint8_t *copy=malloc(n);assert(copy);memcpy(copy,wire.data,n);
    assert(avp_world_gpu_wire_pack(&wire,&p));
    assert(!memcmp(copy,wire.data,n));free(copy);
    /* No stale-frame exposure after incomplete or corrupt input. */
    p.columns[w/2].ready=0u;
    assert(!avp_world_gpu_wire_pack(&wire,&p) && wire.bytes==0u);
    p.columns[w/2].ready=1u;
    p.hits[0].distance=NAN;
    assert(!avp_world_gpu_wire_pack(&wire,&p) && wire.bytes==0u);
    p.hits[0].distance=0.0;
    assert(avp_world_gpu_wire_pack(&wire,&p));
    /* A failed new-frame attempt may not leave last frame readable. */
    assert(!avp_world_scene_begin(&p,NULL,p.maze,1u,1u,1u,
                                  0.0,0.0,1.0,0.0,0.0,1.0,1.0));
    assert(!avp_world_scene_complete(&p));
    assert(!avp_world_gpu_wire_pack(&wire,&p) && wire.bytes==0u);
    /* Re-establish readiness for overflow test. */
    p.map_width=p.map_height=1u;
    for(uint32_t x=0u;x<w;x++) p.columns[x].ready=1u;
    p.columns[0].count=p.hit_capacity_per_column+1u;
    assert(!avp_world_gpu_wire_pack(&wire,&p) && wire.bytes==0u);
    avp_world_gpu_wire_destroy(&wire);
    avp_world_scene_destroy(&p);
    puts("R331 GPU ray stream: 1920 columns, 5760 hits, byte-exact LE packing, stale-frame rejection PASS");
    return 0;
}
