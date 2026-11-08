/* RE24-S12 maze parity probe: renders the live port 3D view (320x228 native)
 * at a pose read from the original program on jagref.  Measurement only. */
#include "avp_rom.h"
#include "avp_resources.h"
#include "collision.h"
#include "jaguar/avp_gpu/state.h"
#include "jaguar/avp_gpu/software_renderer.h"
#include "jaguar/framebuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int avp_gpu_assets_load_retail(AvpAvpGpuState*,const AvpRomImage*);
int main(int argc,char**argv){
    if(argc<6)return 2;
    AvpRomImage rom={0};FILE *f=fopen(argv[1],"rb");fseek(f,0,SEEK_END);rom.size=ftell(f);rewind(f);
    rom.data=malloc(rom.size);if(fread(rom.data,1,rom.size,f)!=rom.size)return 1;fclose(f);rom.exact_retail_sha256=1;
    unsigned file=(unsigned)atoi(argv[2]);u32 x=(u32)strtoul(argv[3],0,16),y=(u32)strtoul(argv[4],0,16),a=(u32)strtoul(argv[5],0,16);
    static AvpMazeCandidate mz[64];unsigned n=avp_resources_find_mazes(&rom,mz,64),k;
    for(k=0;k<n;k++)if(mz[k].file_index==file)break; if(k==n)return 3;
    u8 *m=mz[k].decoded+8;avp_collision_bind_maze(m,mz[k].width,mz[k].height);
    AvpAvpGpuState *st=calloc(1,sizeof *st);avp_gpu_assets_load_retail(st,&rom);
    AvpJaguarFramebuffer fb={0};avp_jaguar_framebuffer_init(&fb,320,228);
    avp_gpu_render_maze(st,&fb,m,mz[k].width,mz[k].height,x,y,a,1,file-86u,0,0,0);
    FILE *o=fopen("maze_port.ppm","wb");fprintf(o,"P6 320 228 255\n");
    for(unsigned i=0;i<320u*228u;i++){u32 c=fb.pixels[i];fputc(c>>16,o);fputc(c>>8,o);fputc(c,o);}fclose(o);return 0;}
