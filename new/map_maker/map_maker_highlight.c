/* PC Map Maker surface highlighting. The original MAZE2 raster remains
 * unmodified. Geometry is projected using the same image-plane focal length,
 * vertical 128/228 wall scale, and editor horizon offset as software_renderer.
 * Its result is a four-segment wireframe, never a filled overlay or new wall.
 */
#include "map_maker_highlight.h"
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#define MM_HIGHLIGHT_RGB 0x00ffd44au
#define MM_REGION_RGB 0x0046dcecu
#define MM_NEAR_PLANE 0.04

typedef struct MmCameraPoint {double front,side,z;} MmCameraPoint;
typedef struct MmPoint2 {double x,y;} MmPoint2;

static MmCameraPoint to_camera(const AvpMapMaker *m,double wx,double wy,double z)
{
    double dx=wx-m->camera_x,dy=wy-m->camera_y;
    double fx=cos(m->camera_heading),fy=sin(m->camera_heading);
    MmCameraPoint p={dx*fx+dy*fy,-dx*fy+dy*fx,z};
    return p;
}

static MmPoint2 project(const AvpMapMaker *m,MmCameraPoint p,
                        unsigned width,unsigned height)
{
    double focal_x=(double)width/(2.0*m->view_ray_scale);
    double focal_y=128.0*(double)height/228.0;
    MmPoint2 q={(double)width*0.5+p.side*focal_x/p.front,
                 (double)height*0.5+m->camera_pitch*(double)height*0.52-
                 p.z*focal_y/p.front};
    return q;
}

/* Liang-Barsky viewport clipping prevents a nearly edge-on face from
 * creating huge offscreen Bresenham loops and keeps all writes in bounds. */
static int cliptest(double p,double q,double *lo,double *hi)
{
    if(p==0.0)return q>=0.0;
    double t=q/p;
    if(p<0.0){if(t>*hi)return 0;if(t>*lo)*lo=t;}
    else {if(t<*lo)return 0;if(t<*hi)*hi=t;}
    return 1;
}
static int clip_view(MmPoint2 *a,MmPoint2 *b,unsigned width,unsigned height)
{
    double dx=b->x-a->x,dy=b->y-a->y,lo=0.0,hi=1.0;
    if(!cliptest(-dx,a->x,&lo,&hi)||
       !cliptest(dx,(double)(width-1u)-a->x,&lo,&hi)||
       !cliptest(-dy,a->y,&lo,&hi)||
       !cliptest(dy,(double)(height-1u)-a->y,&lo,&hi))return 0;
    double x=a->x,y=a->y;
    b->x=x+hi*dx;b->y=y+hi*dy;
    a->x=x+lo*dx;a->y=y+lo*dy;
    return 1;
}
static int stroke_rgb(uint32_t *pixels,unsigned width,unsigned height,
                      MmPoint2 a,MmPoint2 b,uint32_t rgb)
{
    int x0,y0,x1,y1,dx,dy,sx,sy,err;
    if(!clip_view(&a,&b,width,height))return 0;
    x0=(int)floor(a.x+0.5);y0=(int)floor(a.y+0.5);
    x1=(int)floor(b.x+0.5);y1=(int)floor(b.y+0.5);
    dx=abs(x1-x0);dy=abs(y1-y0);
    sx=x0<x1?1:-1;sy=y0<y1?1:-1;err=dx-dy;
    for(;;){
        if(x0>=0&&y0>=0&&(unsigned)x0<width&&(unsigned)y0<height)
            pixels[(size_t)y0*width+(unsigned)x0]=rgb;
        if(x0==x1&&y0==y1)break;
        int twice=2*err;
        if(twice> -dy){err-=dy;x0+=sx;}
        if(twice<dx){err+=dx;y0+=sy;}
    }
    return 1;
}
/* Clip the complete quad to the camera's near plane and then the screen.
 * Clipping *each edge* independently would make the outline vanish when an
 * up-close surface filled the whole viewport: all four real edges would be
 * offscreen. A clipped polygon correctly outlines the visible rectangle. */
static unsigned clip_near(const MmCameraPoint *src,unsigned count,MmCameraPoint *dst)
{
    unsigned out=0;
    for(unsigned i=0;i<count;i++){
        MmCameraPoint a=src[i],b=src[(i+1u)%count];
        int ai=a.front>=MM_NEAR_PLANE,bi=b.front>=MM_NEAR_PLANE;
        if(ai!=bi){
            double t=(MM_NEAR_PLANE-a.front)/(b.front-a.front);
            MmCameraPoint cross={MM_NEAR_PLANE,a.side+t*(b.side-a.side),a.z+t*(b.z-a.z)};
            dst[out++]=cross;
        }
        if(bi)dst[out++]=b;
    }
    return out;
}
static double axis_value(MmPoint2 p,unsigned side)
{
    return side<2u?p.x:p.y;
}
static int boundary_inside(MmPoint2 p,unsigned side,double border)
{
    double v=axis_value(p,side);
    return (side&1u)?v<=border:v>=border;
}
static unsigned clip_polygon(const MmPoint2 *src,unsigned count,MmPoint2 *dst,
                             unsigned side,double border)
{
    unsigned out=0;
    if(!count)return 0;
    for(unsigned i=0;i<count;i++){
        MmPoint2 a=src[i],b=src[(i+1u)%count];
        int ai=boundary_inside(a,side,border);
        int bi=boundary_inside(b,side,border);
        if(ai!=bi){
            double av=axis_value(a,side),bv=axis_value(b,side);
            double t=(border-av)/(bv-av);
            MmPoint2 cross={a.x+t*(b.x-a.x),a.y+t*(b.y-a.y)};
            dst[out++]=cross;
        }
        if(bi)dst[out++]=b;
    }
    return out;
}

int avp_mm_draw_3d_highlight(const AvpMapMaker *m,uint32_t *pixels,
                             unsigned width,unsigned height)
{
    MmCameraPoint corners[4];
    double x,y,z;
    int drawn=0;
    if(!m||!pixels||!m->mode_3d||!m->highlight_3d||!m->target_valid||
       !width||!height||width>8192u||height>8192u||
       m->target_x>=m->w||m->target_y>=m->h||m->target_field>5u||
       !isfinite(m->camera_x)||!isfinite(m->camera_y)||
       !isfinite(m->camera_heading)||!isfinite(m->camera_pitch)||
       !isfinite(m->view_ray_scale)||m->view_ray_scale<=0.0)return 0;
    x=(double)m->target_x;y=(double)m->target_y;
    switch(m->target_field){
        case 0u: /* west face */
        case 2u:{ /* east face */
            double wall_x=x+(m->target_field==2u?1.0:0.0);
            corners[0]=to_camera(m,wall_x,y, 0.5);
            corners[1]=to_camera(m,wall_x,y+1.0, 0.5);
            corners[2]=to_camera(m,wall_x,y+1.0,-0.5);
            corners[3]=to_camera(m,wall_x,y,-0.5);break;
        }
        case 1u: /* north face */
        case 3u:{ /* south face */
            double wall_y=y+(m->target_field==3u?1.0:0.0);
            corners[0]=to_camera(m,x,wall_y, 0.5);
            corners[1]=to_camera(m,x+1.0,wall_y, 0.5);
            corners[2]=to_camera(m,x+1.0,wall_y,-0.5);
            corners[3]=to_camera(m,x,wall_y,-0.5);break;
        }
        default:{ /* ceiling (+) or floor (-) of the selected 1x1 cell */
            z=m->target_field==4u?0.5:-0.5;
            corners[0]=to_camera(m,x,y,z);
            corners[1]=to_camera(m,x+1.0,y,z);
            corners[2]=to_camera(m,x+1.0,y+1.0,z);
            corners[3]=to_camera(m,x,y+1.0,z);break;
        }
    }
    {
        MmCameraPoint nearpoly[8];
        MmPoint2 poly[16],scratch[16];
        unsigned n=clip_near(corners,4u,nearpoly);
        const double borders[4]={1.0,(double)width-2.0,
                                 1.0,(double)height-2.0};
        for(unsigned i=0;i<n;i++)poly[i]=project(m,nearpoly[i],width,height);
        for(unsigned side=0;side<4u && n;side++){
            n=clip_polygon(poly,n,scratch,side,borders[side]);
            for(unsigned i=0;i<n;i++)poly[i]=scratch[i];
        }
        if(n>=3u)for(unsigned i=0;i<n;i++)
            drawn|=stroke_rgb(pixels,width,height,poly[i],poly[(i+1u)%n],MM_HIGHLIGHT_RGB);
    }
    return drawn;
}

/* A selected 2D cell rectangle remains selected on entering 3D. Draw its
 * floor-level footprint with the same clipped projection as face outlines.
 * This is an editor-only hint, not a depth-tested Jaguar game effect or
 * conversion into a limited 128-face selection array. */
int avp_mm_draw_3d_cell_selection(const AvpMapMaker *m,uint32_t *pixels,
                                  unsigned width,unsigned height)
{
    MmCameraPoint corners[4],nearpoly[8];
    MmPoint2 poly[16],scratch[16];
    unsigned xmin,xmax,ymin,ymax,n;
    const double borders[4]={1.0,(double)width-2.0,1.0,(double)height-2.0};
    int drawn=0;
    double z;
    if(!m||!pixels||!m->mode_3d||!m->selection_2d_valid||
       !m->w||!m->h||m->x>=m->w||m->y>=m->h||
       !width||!height||width>8192u||height>8192u||
       !isfinite(m->camera_x)||!isfinite(m->camera_y)||
       !isfinite(m->camera_heading)||!isfinite(m->camera_pitch)||
       !isfinite(m->view_ray_scale)||m->view_ray_scale<=0.0)return 0;
    xmin=m->selection_2d_range && m->select_anchor_x<m->x?m->select_anchor_x:m->x;
    xmax=m->selection_2d_range && m->select_anchor_x>m->x?m->select_anchor_x:m->x;
    ymin=m->selection_2d_range && m->select_anchor_y<m->y?m->select_anchor_y:m->y;
    ymax=m->selection_2d_range && m->select_anchor_y>m->y?m->select_anchor_y:m->y;
    if(xmax>=m->w||ymax>=m->h)return 0;
    z=m->overhead_ceiling?0.5:-0.5;
    corners[0]=to_camera(m,(double)xmin,(double)ymin,z);
    corners[1]=to_camera(m,(double)xmax+1.0,(double)ymin,z);
    corners[2]=to_camera(m,(double)xmax+1.0,(double)ymax+1.0,z);
    corners[3]=to_camera(m,(double)xmin,(double)ymax+1.0,z);
    n=clip_near(corners,4u,nearpoly);
    for(unsigned i=0;i<n;i++)poly[i]=project(m,nearpoly[i],width,height);
    for(unsigned side=0;side<4u&&n;side++){
        n=clip_polygon(poly,n,scratch,side,borders[side]);
        for(unsigned i=0;i<n;i++)poly[i]=scratch[i];
    }
    if(n>=3u)for(unsigned i=0;i<n;i++)
        drawn|=stroke_rgb(pixels,width,height,poly[i],poly[(i+1u)%n],MM_REGION_RGB);
    return drawn;
}

/* Object markers are editor UI only; the Jaguar renderer does not draw staged
 * AMP objects before playtesting. Project the stored 8.8 coordinates and
 * reject objects hidden behind a blocking panel on the camera ray. */
static int object_clear_line(const AvpMapMaker *m,double wx,double wy)
{
    double dx=wx-m->camera_x,dy=wy-m->camera_y,t=0.0;
    int cx=(int)m->camera_x,cy=(int)m->camera_y;
    int sx=dx>=0.0?1:-1,sy=dy>=0.0?1:-1;
    double ux=fabs(dx)>1e-10?fabs(1.0/dx):INFINITY;
    double uy=fabs(dy)>1e-10?fabs(1.0/dy):INFINITY;
    double tx=isfinite(ux)?(dx>0.0?(cx+1.0-m->camera_x):(m->camera_x-cx))*ux:INFINITY;
    double ty=isfinite(uy)?(dy>0.0?(cy+1.0-m->camera_y):(m->camera_y-cy))*uy:INFINITY;
    for(unsigned n=0;n<128u;n++){
        int nx=cx,ny=cy;
        unsigned near,far;
        if(tx<=ty){t=tx;tx+=ux;nx+=sx;near=sx>0?2u:0u;}
        else {t=ty;ty+=uy;ny+=sy;near=sy>0?3u:1u;}
        if(t>=1.0)return 1;
        if(nx<0||ny<0||nx>=m->w||ny>=m->h)return 0;
        far=(near+2u)&3u;
        const u8 *a=m->cells+((size_t)cy*m->w+(unsigned)cx)*8u;
        const u8 *b=m->cells+((size_t)ny*m->w+(unsigned)nx)*8u;
        if((a[near]&&avp_mm_panel_type(a[near])<12u)||
           (b[far]&&avp_mm_panel_type(b[far])<12u))return 0;
        cx=nx;cy=ny;
    }
    return 0;
}
static int object_screen(const AvpMapMaker *m,unsigned i,unsigned width,
                         unsigned height,MmPoint2 *p,double *depth)
{
    MmCameraPoint c;
    const AvpMmObjectRecord *o;
    if(!m||!p||!depth||!m->mode_3d||!width||!height||
       width>8192u||height>8192u||i>=m->object_count||
       i>=AVP_MM_MAX_OBJECTS||!isfinite(m->view_ray_scale)||
       m->view_ray_scale<=0.0||!isfinite(m->camera_x)||
       !isfinite(m->camera_y)||!isfinite(m->camera_heading)||
       !isfinite(m->camera_pitch))return 0;
    o=&m->objects[i];
    c=to_camera(m,(double)o->x8/256.0,(double)o->y8/256.0,0.0);
    if(c.front<=0.06||c.front>41.0)return 0;
    *p=project(m,c,width,height);
    if(!isfinite(p->x)||!isfinite(p->y)||p->x<8.0||p->y<8.0||
       p->x>(double)width-9.0||p->y>(double)height-9.0)return 0;
    if(!object_clear_line(m,(double)o->x8/256.0,(double)o->y8/256.0))return 0;
    *depth=c.front;
    return 1;
}
int avp_mm_3d_object_at(const AvpMapMaker *m,unsigned width,unsigned height,
                        int px,int py)
{
    int best=-1;double best_dist=145.0,best_depth=INFINITY;
    if(!m||px<0||py<0||(unsigned)px>=width||(unsigned)py>=height)return -1;
    for(unsigned i=0;i<m->object_count&&i<AVP_MM_MAX_OBJECTS;i++){
        MmPoint2 p;double depth,ex,ey,dist;
        if(!object_screen(m,i,width,height,&p,&depth))continue;
        ex=p.x-(double)px;ey=p.y-(double)py;dist=ex*ex+ey*ey;
        if(dist<=best_dist&&(dist<best_dist||depth<best_depth)){
            best=(int)i;best_dist=dist;best_depth=depth;
        }
    }
    return best;
}
int avp_mm_draw_3d_objects(const AvpMapMaker *m,uint32_t *pixels,
                           unsigned width,unsigned height)
{
    int count=0;
    if(!m||!pixels||!m->mode_3d)return 0;
    for(unsigned i=0;i<m->object_count&&i<AVP_MM_MAX_OBJECTS;i++){
        MmPoint2 p;double depth;
        if(object_screen(m,i,width,height,&p,&depth)){
            uint32_t color=m->selected_objects[i]?0x0046dcecu:0x00ffd44au;
            MmPoint2 a={p.x-6.0,p.y},b={p.x+6.0,p.y};
            (void)stroke_rgb(pixels,width,height,a,b,color);
            a=(MmPoint2){p.x,p.y-6.0};b=(MmPoint2){p.x,p.y+6.0};
            (void)stroke_rgb(pixels,width,height,a,b,color);
            if(m->selected_objects[i]){
                a=(MmPoint2){p.x-9.0,p.y-9.0};b=(MmPoint2){p.x+9.0,p.y-9.0};
                (void)stroke_rgb(pixels,width,height,a,b,color);
                a=(MmPoint2){p.x+9.0,p.y+9.0};
                (void)stroke_rgb(pixels,width,height,b,a,color);
                a=(MmPoint2){p.x-9.0,p.y+9.0};
                (void)stroke_rgb(pixels,width,height,b,a,color);
                b=(MmPoint2){p.x-9.0,p.y-9.0};
                (void)stroke_rgb(pixels,width,height,a,b,color);
            }
            ++count;
        }
    }
    return count;
}
