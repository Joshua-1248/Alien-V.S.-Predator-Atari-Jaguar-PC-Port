#ifndef AVP_JAGUAR_AVP_GPU_MAZE2_OCCLUSION_H
#define AVP_JAGUAR_AVP_GPU_MAZE2_OCCLUSION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AvpMaze2Occlusion {
    unsigned width,height;
    float *depth;
} AvpMaze2Occlusion;

int avp_gpu_maze2_occlusion_ensure(AvpMaze2Occlusion *o,unsigned width,unsigned height);
void avp_gpu_maze2_occlusion_destroy(AvpMaze2Occlusion *o);
void avp_gpu_maze2_occlusion_clear(AvpMaze2Occlusion *o);
void avp_gpu_maze2_occlusion_mark_span(AvpMaze2Occlusion *o,unsigned x,int y0,int y1,float depth);
float avp_gpu_maze2_occlusion_depth(const AvpMaze2Occlusion *o,int x,int y);
int avp_gpu_maze2_occlusion_blocks(const AvpMaze2Occlusion *o,int x,int y,float object_depth);

#ifdef __cplusplus
}
#endif
#endif
