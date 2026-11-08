/* RE32 R43: Exercise the actual PC floor/ceiling raster (not only menu mode
 * values) with a synthetic 64x64 maze and flat texture. ROM-free. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;} ++checks;}while(0)

/* Our synthetic flat is unshaded RGB, so no ROM shader data is needed. */
int avp_gpu_retail_flat_intensity_delta(double d) {(void)d;return 0;}

static int test_view(unsigned w,unsigned h,int *checks)
{
    AvpAvpGpuState *state=(AvpAvpGpuState*)calloc(1,sizeof(*state));
    AvpFloorPass p={0};
    AvpJaguarFramebuffer fb={0};
    unsigned char rgb[3]={0x12,0x93,0xd8};
    AvpGpuRgbImage img={1,1,rgb,NULL};
    u8 *maze=(u8*)calloc(64u*64u,8u);
    uint32_t *pixels=(uint32_t*)calloc((size_t)w*h,sizeof(uint32_t));
    unsigned previous=0;
    int checks_this=0;
    if(!state||!maze||!pixels){free(state);free(maze);free(pixels);return 0;}
    for(unsigned i=0;i<64u*64u;i++){
        maze[i*8u+4u]=1u;maze[i*8u+5u]=1u;
    }
    fb.width=w;fb.height=h;fb.pixels=pixels;
    p.state=state;p.fb=&fb;p.maze=maze;p.mw=64;p.mh=64;
    p.px=10;p.py=10;p.fx=0.7071067811865476;
    p.fy=0.7071067811865476;p.rx=-p.fy;p.ry=p.fx;
    p.focal=(double)w*0.5;p.flat_tex[1]=&img;
    for(int mode=0;mode<AVP_HOST_DRAW_DISTANCE_CHOICES;mode++){
        unsigned drawn=0;
        state->host_extend_draw_distance=mode;
        memset(pixels,0,(size_t)w*h*sizeof(uint32_t));
        avp_floor_rows(&p,0,h);
        for(unsigned y=0;y<h;y++){
            unsigned mid=w/2;
            double r=((double)y+0.5)*228.0/(double)h;
            double d;
            int expected=avp_host_flat_row_depth(r,mode,&d);
            uint32_t color=pixels[(size_t)y*w+mid];
            /* The checked camera is safely inside the map for all sampled
             * depths below 90 cells; larger depths never hit a valid cell. */
            if(expected && d<60.0){
                if(color!=0x1293d8u){
                    fprintf(stderr,"wrong %ux%u mode%d row%u depth%.4f color%08x\n",
                            w,h,mode,y,d,color);
                    free(state);free(maze);free(pixels);return 0;
                }
                checks_this++;
            }
            if(color){
                if(color!=0x1293d8u){free(state);free(maze);free(pixels);return 0;}
                drawn++;
            }
        }
        if(mode && drawn<previous){
            fprintf(stderr,"regression %ux%u mode%d %u < %u\n",w,h,mode,drawn,previous);
            free(state);free(maze);free(pixels);return 0;
        }
        previous=drawn;
        checks_this++;
    }
    /* Under the 228-line Jaguar raster, every additional setting has at
     * least one newly visible ceiling and one newly visible floor row. */
    if(h==228){
        static const unsigned row[]={105,108,110,111,112,115,116,117,119,122};
        static const unsigned first_mode[]={0,1,2,3,4,4,3,2,1,0};
        for(unsigned i=0;i<sizeof(row)/sizeof(row[0]);i++){
            double d;
            for(int mode=0;mode<AVP_HOST_DRAW_DISTANCE_CHOICES;mode++){
                int show=avp_host_flat_row_depth((double)row[i]+0.5,mode,&d);
                if(show!=(mode>=(int)first_mode[i])){
                    fprintf(stderr,"unexpected row %u first=%u mode=%d\n",row[i],first_mode[i],mode);
                    free(state);free(maze);free(pixels);return 0;
                }
                checks_this++;
            }
        }
    }
    *checks+=checks_this;
    free(state);free(maze);free(pixels);
    return 1;
}

int main(void)
{
    static const unsigned views[][2]={{320,228},{320,114},{160,114},
                                      {640,228},{640,360},{853,480},{1280,720}};
    int checks=0;
    /* Exact original row decision / depths must stay identical when off. */
    for(unsigned h=114;h<=720;h+=57){
        for(unsigned y=0;y<h;y++){
            double row=((double)y+0.5)*228.0/(double)h;
            double off=fabs(row-114.0),real,original;
            int skip=(row>=106.0&&row<122.0);
            int r=avp_host_flat_row_depth(row,0,&real);
            CHECK(r==!skip);
            if(!skip){
                if(off<0.5)off=0.5;
                original=64.0/off;
                if(original>8.0)original=8.0;
                CHECK(real==original);
            }
        }
    }
    for(unsigned i=0;i<sizeof(views)/sizeof(views[0]);i++)
        CHECK(test_view(views[i][0],views[i][1],&checks));
    printf("PASS actual floor/ceiling draw distance rows, original compatibility, and viewport scales: %d checks\n",checks);
    return 0;
}
