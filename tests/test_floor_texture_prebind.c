/* Real host floor raster pass against the preceding R126 texture-file lookup.
 * Uses synthetic maze and images; no ROM, external assets, or diagnostics. */
#include "../src/jaguar/avp_gpu/software_renderer.c"
#include <assert.h>
/* Projection-derived shade values are common to both paths. Test positive,
 * negative, and zero shades without linking unrelated C++ bridge modules. */
int avp_gpu_retail_flat_intensity_delta(double d)
{
    return (int)(d*3.0)-11;
}

static void previous_floor_rows(const AvpFloorPass *p,const int file_of[256])
{
    AvpJaguarFramebuffer *fb=p->fb;
    for(unsigned y=0;y<fb->height;y++){
        double retail_row=((double)y+0.5)*228.0/(double)fb->height;
        int ceiling=retail_row<114.0;double off=fabs(retail_row-114.0);
        double perp,lefttan,step,wx,wy,wxstep,wystep;
        int floor_shade;
        if(retail_row>=106.0&&retail_row<122.0)continue;
        if(off<0.5)off=0.5;
        perp=64.0/off;if(perp>8.0)perp=8.0;
        floor_shade=avp_gpu_retail_flat_intensity_delta(perp);
        lefttan=(0.5-(double)fb->width*0.5)/p->focal;step=1.0/p->focal;
        wx=p->px+(p->fx+p->rx*lefttan)*perp;
        wy=p->py+(p->fy+p->ry*lefttan)*perp;
        wxstep=p->rx*step*perp;wystep=p->ry*step*perp;
        for(unsigned x=0;x<fb->width;x++,wx+=wxstep,wy+=wystep){
            int cx,cy,fid;u8 flat;
            if(!avp_gpu_floor_valid_cell(wx,wy,p->mw,p->mh,&cx,&cy))continue;
            {const u8*c=p->maze+((size_t)cy*p->mw+(size_t)cx)*8u;flat=c[ceiling?4:5];}
            fid=file_of[flat];
            if(fid>=0&&p->state->panel_tex[fid].rgb){
                double ux=wx-(double)cx,uy=wy-(double)cy;
                const AvpGpuRgbImage*t=&p->state->panel_tex[fid];
                int tx=(int)(ux*t->width),ty=(int)(uy*t->height);
                if(p->flat_rotation[flat] && t->width==t->height){
                    int oldx=tx,oldy=ty;
                    switch(p->flat_rotation[flat]){
                    case 1u:tx=oldy;ty=t->height-1-oldx;break;
                    case 2u:tx=t->width-1-oldx;ty=t->height-1-oldy;break;
                    case 3u:tx=t->width-1-oldy;ty=oldx;break;
                    default:break;
                    }
                }
                fb->pixels[(size_t)y*fb->width+x]=sample_tex_shaded(t,tx,ty,floor_shade);
            }
        }
    }
}

int main(void)
{
    static const unsigned sizes[][2]={{320,228},{853,480},{1280,720},
                                     {1920,1080},{3440,1440},{3840,2160}};
    static const double views[][3]={{3.2,3.8,0.0},{31.5,31.5,0.7},
                                    {63.7,0.2,2.5},{0.3,63.6,-1.4}};
    const unsigned mw=64,mh=64;
    const size_t maze_bytes=(size_t)mw*mh*8u;
    AvpAvpGpuState *state=calloc(1,sizeof(*state));
    u8 *maze=calloc(1,maze_bytes);
    int file_of[256];
    assert(state&&maze);
    for(unsigned i=0;i<65536u;i++)k_cry_rgb[i]=(uint32_t)(i*16777619u);
    for(unsigned f=0;f<74u;f++){
        AvpGpuRgbImage *t=&state->panel_tex[f];
        t->width=(f&1u)?16:32;
        t->height=(f&2u)?16:32;
        if(f%7u==0u)continue; /* Missing image despite an authored file ID. */
        size_t n=(size_t)t->width*(size_t)t->height;
        t->rgb=malloc(n*3u);
        t->cry16=malloc(n*sizeof(u16));
        assert(t->rgb&&t->cry16);
        for(size_t k=0;k<n;k++){
            t->rgb[3u*k]=(u8)(f+k);t->rgb[3u*k+1]=(u8)(f+3u*k);
            t->rgb[3u*k+2]=(u8)(f+7u*k);
            t->cry16[k]=(u16)((f*659u+k*97u)&65535u);
        }
    }
    for(unsigned cy=0;cy<mh;cy++)for(unsigned cx=0;cx<mw;cx++){
        size_t o=((size_t)cy*mw+cx)*8u;
        maze[o+4u]=(u8)((cx*13u+cy*17u)&255u);
        maze[o+5u]=(u8)((cx*19u+cy*7u+3u)&255u);
    }
    for(unsigned f=0;f<256u;f++){
        file_of[f]=(f%11u==0u)?-1:(int)(f%74u);
    }
    for(size_t si=0;si<sizeof(sizes)/sizeof(sizes[0]);si++){
        unsigned w=sizes[si][0],h=sizes[si][1];
        size_t count=(size_t)w*h,bytes=count*sizeof(uint32_t);
        uint32_t *before=malloc(bytes),*after=malloc(bytes);
        assert(before&&after);
        for(size_t vi=0;vi<sizeof(views)/sizeof(views[0]);vi++){
            AvpJaguarFramebuffer fb={w,h,before};
            AvpFloorPass pass={0};
            const double angle=views[vi][2];
            pass.state=state;pass.fb=&fb;pass.maze=maze;pass.mw=mw;pass.mh=mh;
            pass.px=views[vi][0];pass.py=views[vi][1];
            pass.fx=cos(angle);pass.fy=sin(angle);
            pass.rx=-pass.fy;pass.ry=pass.fx;pass.focal=(double)w*0.5;
            for(unsigned f=0;f<256u;f++){
                int id=file_of[f];
                pass.flat_tex[f]=id>=0 && state->panel_tex[id].rgb
                                 ?&state->panel_tex[id]:NULL;
                pass.flat_rotation[f]=f%4u;
            }
            for(size_t k=0;k<count;k++)before[k]=(uint32_t)(k*35u+si*17u+vi);
            memcpy(after,before,bytes);
            previous_floor_rows(&pass,file_of);
            fb.pixels=after;
            avp_floor_rows(&pass,0,h);
            assert(memcmp(before,after,bytes)==0);
        }
        free(before);free(after);
    }
    for(unsigned f=0;f<74u;f++){
        free(state->panel_tex[f].rgb);free(state->panel_tex[f].cry16);
    }
    free(maze);free(state);
    return 0;
}
