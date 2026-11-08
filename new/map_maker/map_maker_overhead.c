#include "map_maker_overhead.h"
#include <stddef.h>
#include <string.h>
#include <math.h>

int avp_mm_overhead_layout(const AvpMapMaker *m,unsigned width,unsigned height,
                           AvpMmOverheadLayout *layout)
{
    unsigned panel,area_w,area_h,cell;
    if(!m||!layout||!m->w||!m->h||!width||!height)return 0;
    /* Reserve a text panel on the left, and leave the map visible at 640x456. */
    panel=width>=1100u?520u:(width>=720u?330u:260u);
    if(width<=panel+24u||height<=100u)return 0;
    area_w=width-panel-24u;area_h=height-100u;
    cell=area_w/m->w;
    if(cell>area_h/m->h)cell=area_h/m->h;
    if(cell>24u)cell=24u;
    /* 20% steps and temporal easing give fine control at native overview
     * scale and >5x the old 48-pixel maximum for detailed wall work. */
    {
        double visual=m->overhead_zoom_visual;
        double scaled;
        if(!isfinite(visual)||visual<1.0)visual=1.0;
        if(visual>28.0)visual=28.0;
        scaled=(double)cell*pow(1.20,visual-1.0);
        if(scaled>256.0)scaled=256.0;
        cell=(unsigned)(scaled+0.5);
    }
    if(cell<2u)return 0;
    layout->cell_px=cell;
    /* The 2D map occupies a viewport beside the editor text, not the full
     * framebuffer. At extreme zoom, origin may be negative by thousands of
     * pixels; rendering and mouse hit-testing MUST share this same scissor. */
    layout->clip_x0=(int)panel;
    layout->clip_y0=48;
    layout->clip_x1=(int)width-24;
    layout->clip_y1=(int)height-52;
    if(m->overhead_anchor_valid){
        layout->x0=m->overhead_anchor_px-(int)floor(m->overhead_anchor_cell_x*cell+0.5);
        layout->y0=m->overhead_anchor_py-(int)floor(m->overhead_anchor_cell_y*cell+0.5);
    }else if(m->overhead_zoom>1u || m->overhead_zoom_visual>1.01){
        /* Keyboard zoom without a mouse anchor remains selection-centered. */
        layout->x0=(int)panel+(int)area_w/2-(int)((m->wall_drag_lock?m->wall_drag_x:m->x)*cell+cell/2u);
        layout->y0=48+(int)area_h/2-(int)((m->wall_drag_lock?m->wall_drag_y:m->y)*cell+cell/2u);
    }else{
        layout->x0=(int)panel+((int)area_w-(int)(cell*m->w))/2;
        layout->y0=48+((int)area_h-(int)(cell*m->h))/2;
    }
    return 1;
}
void avp_mm_overhead_zoom_at(AvpMapMaker *m,unsigned width,unsigned height,
                             int px,int py,int direction)
{
    AvpMmOverheadLayout l;
    if(!m||!direction||m->wall_drag_lock)return;
    if(!avp_mm_overhead_layout(m,width,height,&l)||!l.cell_px)return;
    /* Wheel events over the left HUD or outside the map canvas must not
     * anchor the camera there and throw the zoomed map off-screen. */
    if(px<l.clip_x0||py<l.clip_y0||px>=l.clip_x1||py>=l.clip_y1)return;
    {
        /* Same transform as rendering and hit testing. Keep the actual
         * map coordinate beneath the mouse fixed during every zoom frame. */
        double x=(double)(px-l.x0)/(double)l.cell_px;
        double y=(double)(py-l.y0)/(double)l.cell_px;
        if(x>=0.0 && y>=0.0 && x<=m->w && y<=m->h){
            m->overhead_anchor_cell_x=x;
            m->overhead_anchor_cell_y=y;
            m->overhead_anchor_px=px;
            m->overhead_anchor_py=py;
            m->overhead_anchor_valid=1;
        }
    }
    avp_mm_zoom(m,direction);
}
void avp_mm_overhead_pan(AvpMapMaker *m,unsigned width,unsigned height,
                         int dx,int dy)
{
    AvpMmOverheadLayout l;
    if(!m||m->mode_3d||!avp_mm_overhead_layout(m,width,height,&l))return;
    if(!m->overhead_anchor_valid){
        /* Reproduce the current origin exactly before applying deltas,
         * including the first drag at native overview zoom. */
        m->overhead_anchor_cell_x=0.0;
        m->overhead_anchor_cell_y=0.0;
        m->overhead_anchor_px=l.x0;
        m->overhead_anchor_py=l.y0;
        m->overhead_anchor_valid=1;
    }
    if(dx>4096)dx=4096;
    if(dx< -4096)dx=-4096;
    if(dy>4096)dy=4096;
    if(dy< -4096)dy=-4096;
    /* Bounded accumulation prevents overflow from pathological X11 motion
     * coordinates, while normal drags can freely pan across the whole map. */
    if(m->overhead_anchor_px+dx>1000000)dx=1000000-m->overhead_anchor_px;
    if(m->overhead_anchor_px+dx< -1000000)dx=-1000000-m->overhead_anchor_px;
    if(m->overhead_anchor_py+dy>1000000)dy=1000000-m->overhead_anchor_py;
    if(m->overhead_anchor_py+dy< -1000000)dy=-1000000-m->overhead_anchor_py;
    m->overhead_anchor_px+=dx;
    m->overhead_anchor_py+=dy;
    {
        double min_x=(double)l.clip_x0-(double)m->w*l.cell_px+l.cell_px;
        double max_x=(double)l.clip_x1-l.cell_px;
        double min_y=(double)l.clip_y0-(double)m->h*l.cell_px+l.cell_px;
        double max_y=(double)l.clip_y1-l.cell_px;
        double origin_x=m->overhead_anchor_px-
                          floor(m->overhead_anchor_cell_x*l.cell_px+0.5);
        double origin_y=m->overhead_anchor_py-
                          floor(m->overhead_anchor_cell_y*l.cell_px+0.5);
        if(origin_x<min_x)m->overhead_anchor_px+=(int)ceil(min_x-origin_x);
        if(origin_x>max_x)m->overhead_anchor_px-=(int)ceil(origin_x-max_x);
        if(origin_y<min_y)m->overhead_anchor_py+=(int)ceil(min_y-origin_y);
        if(origin_y>max_y)m->overhead_anchor_py-=(int)ceil(origin_y-max_y);
    }
}
/* Selecting a large area while zoomed in must not force a separate pan
 * operation. A stationary pointer close to a viewport edge scrolls the 2D
 * canvas in the required direction while the host holds Shift+LMB.
 * Keep off-canvas excursions bounded and NEVER select underneath the HUD. */
int avp_mm_overhead_selection_auto_pan(AvpMapMaker *m,unsigned width,unsigned height,
                                       int px,int py,int step,
                                       unsigned *cell_x,unsigned *cell_y)
{
    AvpMmOverheadLayout l,before,after;
    int x,y,dx=0,dy=0;
    const int margin=22,spill=12;
    unsigned cx,cy;
    if(!m||m->mode_3d||!m->selection_2d_valid||!m->selection_2d_range||
       step<=0||!avp_mm_overhead_layout(m,width,height,&l))return 0;
    /* If the pointer wandered deep into the HUD or outside the canvas,
     * retain the selection exactly as it was rather than moving the map. */
    if(px<l.clip_x0-spill||px>=l.clip_x1+spill||
       py<l.clip_y0-spill||py>=l.clip_y1+spill)return 0;
    if(step>48)step=48;
    if((uint64_t)l.cell_px*m->w>(unsigned)(l.clip_x1-l.clip_x0)){
        if(px<=l.clip_x0+margin)dx=step;
        else if(px>=l.clip_x1-1-margin)dx=-step;
    }
    if((uint64_t)l.cell_px*m->h>(unsigned)(l.clip_y1-l.clip_y0)){
        if(py<=l.clip_y0+margin)dy=step;
        else if(py>=l.clip_y1-1-margin)dy=-step;
    }
    if(!dx&&!dy)return 0;
    before=l;
    avp_mm_overhead_pan(m,width,height,dx,dy);
    if(!avp_mm_overhead_layout(m,width,height,&after) ||
       (before.x0==after.x0 && before.y0==after.y0))return 0;
    x=px<l.clip_x0?l.clip_x0:px>=l.clip_x1?l.clip_x1-1:px;
    y=py<l.clip_y0?l.clip_y0:py>=l.clip_y1?l.clip_y1-1:py;
    if(!avp_mm_overhead_hit(m,width,height,x,y,&cx,&cy))return 0;
    if(cell_x)*cell_x=cx;
    if(cell_y)*cell_y=cy;
    return 1;
}
void avp_mm_overhead_zoom_tick(AvpMapMaker *m,double delta_seconds)
{
    double difference,t;
    if(!m||!isfinite(delta_seconds)||delta_seconds<=0.0)return;
    if(delta_seconds>0.1)delta_seconds=0.1;
    difference=(double)m->overhead_zoom-m->overhead_zoom_visual;
    if(fabs(difference)<0.002){m->overhead_zoom_visual=(double)m->overhead_zoom;return;}
    t=1.0-exp(-13.0*delta_seconds);
    m->overhead_zoom_visual+=difference*t;
}
int avp_mm_overhead_hit(const AvpMapMaker *m,unsigned width,unsigned height,
                        int px,int py,unsigned *cell_x,unsigned *cell_y)
{
    AvpMmOverheadLayout l;
    unsigned x,y;
    if(!avp_mm_overhead_layout(m,width,height,&l)||
       px<l.clip_x0||py<l.clip_y0||px>=l.clip_x1||py>=l.clip_y1||
       px<l.x0||py<l.y0)return 0;
    x=(unsigned)(px-l.x0)/l.cell_px;
    y=(unsigned)(py-l.y0)/l.cell_px;
    if(x>=m->w||y>=m->h)return 0;
    if(cell_x)*cell_x=x;
    if(cell_y)*cell_y=y;
    return 1;
}
unsigned avp_mm_overhead_edge(const AvpMapMaker *m,unsigned width,unsigned height,
                              int px,int py)
{
    AvpMmOverheadLayout l;
    unsigned x,y,left,top,right,bottom,closest,side=4u,limit;
    if(!avp_mm_overhead_layout(m,width,height,&l)||
       !avp_mm_overhead_hit(m,width,height,px,py,&x,&y))return 4u;
    left=(unsigned)(px-(l.x0+(int)(x*l.cell_px)));
    top=(unsigned)(py-(l.y0+(int)(y*l.cell_px)));
    right=l.cell_px-1u-left;bottom=l.cell_px-1u-top;
    closest=l.cell_px;
    if(left<closest){closest=left;side=0u;}
    if(top<closest){closest=top;side=1u;}
    if(right<closest){closest=right;side=2u;}
    if(bottom<closest){closest=bottom;side=3u;}
    /* At native overview zoom, a five-pixel tile has one-pixel edges. */
    /* Snap over roughly the outer quarter of a tile, rather than a
     * one/two-pixel hairline. The tile interior keeps the chosen direction. */
    limit=(l.cell_px+3u)/4u;
    if(limit<2u)limit=2u;
    return closest<limit?side:4u;
}
static void put(uint32_t *pixels,unsigned width,unsigned height,
                const AvpMmOverheadLayout *l,int x,int y,uint32_t c)
{
    if(x>=l->clip_x0&&x<l->clip_x1&&y>=l->clip_y0&&y<l->clip_y1&&
       x>=0&&y>=0&&(unsigned)x<width&&(unsigned)y<height)
        pixels[(size_t)(unsigned)y*width+(unsigned)x]=c;
}
void avp_mm_render_overhead_with_sprites(const AvpMapMaker *m,uint32_t *pixels,
                            unsigned width,unsigned height,
                            AvpMmOverheadLookup lookup,void *context,
                            AvpMmOverheadSpriteLookup sprite_lookup)
{
    AvpMmOverheadLayout l;
    AvpMmOverheadTexture atlas[256];
    unsigned char valid[256];
    unsigned x,y,ix,iy,field;
    if(!pixels||!width||!height||width>16384u||height>16384u)return;
    /* No image is ever read from the game framebuffer: this is a draft-only
     * preview whose pixels reflect the editor's private cell array. */
    for(y=0;y<height;y++)for(x=0;x<width;x++)pixels[(size_t)y*width+x]=0x080a0du;
    if(!avp_mm_overhead_layout(m,width,height,&l))return;
    memset(valid,0,sizeof valid);
    field=m->overhead_ceiling?4u:5u;
    for(y=0;y<m->h;y++)for(x=0;x<m->w;x++){
        unsigned size=l.cell_px;
        int ox=l.x0+(int)(x*size),oy=l.y0+(int)(y*size);
        /* A close zoom exposes only a handful of tiles. Do not decode or
         * rasterize the other thousands of entirely off-screen cells. */
        if(ox>=l.clip_x1||oy>=l.clip_y1||
           ox+(int)size<=l.clip_x0||oy+(int)size<=l.clip_y0)continue;
        const u8 *cell=m->cells+((size_t)y*m->w+x)*8u;
        unsigned f=cell[field];
        AvpMmOverheadTexture *t=&atlas[f];
        if(!valid[f]){
            memset(t,0,sizeof *t);
            if(f && lookup) (void)lookup(context,m->level,f,t);
            valid[f]=1u;
        }
        for(iy=0;iy<size;iy++)for(ix=0;ix<size;ix++){
            uint32_t col=cell[5] ? 0x202630u : 0x0b0c11u;
            if(t->rgb&&t->width&&t->height){
                unsigned tx=(unsigned)(((uint64_t)ix*t->width)/size);
                unsigned ty=(unsigned)(((uint64_t)iy*t->height)/size);
                if(t->width==t->height){
                    unsigned tmp;
                    switch(t->rotation&3u){
                    case 1u:tmp=tx;tx=ty;ty=t->height-1u-tmp;break;
                    case 2u:tx=t->width-1u-tx;ty=t->height-1u-ty;break;
                    case 3u:tmp=tx;tx=t->width-1u-ty;ty=tmp;break;
                    default:break;
                    }
                }
                const uint8_t *rgb=t->rgb+((size_t)ty*t->width+tx)*3u;
                col=((uint32_t)rgb[0]<<16)|((uint32_t)rgb[1]<<8)|(uint32_t)rgb[2];
            }
            put(pixels,width,height,&l,ox+(int)ix,oy+(int)iy,col);
        }
        /* Precise geometry overlay; no wall needs a nonzero flat texture. */
        for(ix=0;ix<size;ix++){
            if(cell[1])put(pixels,width,height,&l,ox+(int)ix,oy,0xb4c5d0u);
            if(cell[3])put(pixels,width,height,&l,ox+(int)ix,oy+(int)size-1,0xb4c5d0u);
        }
        for(iy=0;iy<size;iy++){
            if(cell[0])put(pixels,width,height,&l,ox,oy+(int)iy,0xb4c5d0u);
            if(cell[2])put(pixels,width,height,&l,ox+(int)size-1,oy+(int)iy,0xb4c5d0u);
        }
        /* Small subdued grid around each tile, distinct from solid walls. */
        if(size>=4u){
            if(!cell[1])for(ix=1;ix+1<size;ix++)put(pixels,width,height,&l,ox+(int)ix,oy,0x313944u);
            if(!cell[0])for(iy=1;iy+1<size;iy++)put(pixels,width,height,&l,ox,oy+(int)iy,0x313944u);
        }
    }
    /* Stage-only object previews. With a genuine AAF frame present, draw a
     * compact proportional sprite inside its tile, then keep a tiny group
     * outline as the placement indicator. Unsupported IDs remain crosses.
     * Clip EVERY pixel to the map canvas, including at extreme zoom/pan. */
    for(unsigned i=0u;i<m->object_count && i<AVP_MM_MAX_OBJECTS;i++){
        const AvpMmObjectRecord *a=&m->objects[i];
        unsigned tx=a->x8>>8,ty=a->y8>>8;
        int xx,yy,drawn=0;
        uint32_t color=a->group==3u?0xff8c30u:a->group==0u?0x42c6feu:
                       a->group==1u?0xe869e5u:0xb0ff5eu;
        if(tx>=m->w||ty>=m->h)continue;
        xx=l.x0+(int)(((uint64_t)a->x8*l.cell_px)/256u);
        yy=l.y0+(int)(((uint64_t)a->y8*l.cell_px)/256u);
        if(l.cell_px>=14u && sprite_lookup &&
           xx>=l.clip_x0-(int)l.cell_px && xx<l.clip_x1+(int)l.cell_px &&
           yy>=l.clip_y0-(int)l.cell_px && yy<l.clip_y1+(int)l.cell_px){
            AvpMmOverheadSprite sp={0};
            if(sprite_lookup(context,a->def,&sp) && sp.rgba &&
               sp.width>=1u&&sp.width<=256u&&sp.height>=1u&&sp.height<=256u&&
               sp.x0<sp.x1&&sp.y0<sp.y1&&sp.x1<=sp.width&&sp.y1<=sp.height){
                unsigned bw=sp.x1-sp.x0,bh=sp.y1-sp.y0;
                unsigned bound=l.cell_px*3u/4u;
                unsigned dw,dh;
                if(bound>64u)bound=64u;
                /* Preserve the original image's proportions. */
                if(bw>=bh){dw=bound;dh=(unsigned)(((uint64_t)bound*bh+bw/2u)/bw);}
                else {dh=bound;dw=(unsigned)(((uint64_t)bound*bw+bh/2u)/bh);}
                if(!dw)dw=1u;
                if(!dh)dh=1u;
                for(unsigned py=0;py<dh;py++)for(unsigned px=0;px<dw;px++){
                    unsigned sx=sp.x0+(unsigned)(((uint64_t)px*bw)/dw);
                    unsigned sy=sp.y0+(unsigned)(((uint64_t)py*bh)/dh);
                    uint32_t rgba;
                    rgba=sp.rgba[(size_t)sy*sp.width+sx];
                    if(rgba>>24)put(pixels,width,height,&l,
                        xx-(int)dw/2+(int)px,yy-(int)dh/2+(int)py,rgba&0x00ffffffu);
                }
                /* Colored corners retain the object's placement group. */
                for(int q=-1;q<=1;q++){
                    put(pixels,width,height,&l,xx-(int)dw/2+q,yy-(int)dh/2,color);
                    put(pixels,width,height,&l,xx+(int)dw/2+q,yy+(int)dh/2,color);
                }
                drawn=1;
            }
        }
        if(!drawn)for(int q=-2;q<=2;q++){
            put(pixels,width,height,&l,xx+q,yy,color);
            put(pixels,width,height,&l,xx,yy+q,color);
        }
    }
    /* Shift+arrows: visible rectangular multiselection over the selected
     * 2D edit layer. This overlay is never written into map bytes. */
    if(m->selection_2d_valid && m->selection_2d_range){
        unsigned x0=m->x<m->select_anchor_x?m->x:m->select_anchor_x;
        unsigned x1=m->x>m->select_anchor_x?m->x:m->select_anchor_x;
        unsigned y0=m->y<m->select_anchor_y?m->y:m->select_anchor_y;
        unsigned y1=m->y>m->select_anchor_y?m->y:m->select_anchor_y;
        for(unsigned cy=y0;cy<=y1;cy++)for(unsigned cx=x0;cx<=x1;cx++){
            int ox=l.x0+(int)(cx*l.cell_px),oy=l.y0+(int)(cy*l.cell_px);
            if(ox>=(int)width||oy>=(int)height||
               ox+(int)l.cell_px<=0||oy+(int)l.cell_px<=0)continue;
            for(unsigned k=0;k<l.cell_px;k++){
                put(pixels,width,height,&l,ox+(int)k,oy,0x46dcecu);
                put(pixels,width,height,&l,ox,oy+(int)k,0x46dcecu);
                put(pixels,width,height,&l,ox+(int)k,oy+(int)l.cell_px-1,0x46dcecu);
                put(pixels,width,height,&l,ox+(int)l.cell_px-1,oy+(int)k,0x46dcecu);
            }
        }
    }
    /* Selected-cell border is drawn last, so no texture hides it. */
    if(m->selection_2d_valid && m->x<m->w&&m->y<m->h){
        int sx=l.x0+(int)(m->x*l.cell_px),sy=l.y0+(int)(m->y*l.cell_px);
        unsigned k;
        for(k=0;k<l.cell_px;k++){
            put(pixels,width,height,&l,sx+(int)k,sy,0xffdf4au);
            put(pixels,width,height,&l,sx+(int)k,sy+(int)l.cell_px-1,0xffdf4au);
            put(pixels,width,height,&l,sx,sy+(int)k,0xffdf4au);
            put(pixels,width,height,&l,sx+(int)l.cell_px-1,sy+(int)k,0xffdf4au);
        }
        /* BUILD-like side selection: the active panel side is amber, while
         * the selected cell remains yellow. This is preview-only. */
        if(m->field<4u)for(k=0;k<l.cell_px;k++){
            if(m->field==0u)put(pixels,width,height,&l,sx,sy+(int)k,0xff843bu);
            if(m->field==1u)put(pixels,width,height,&l,sx+(int)k,sy,0xff843bu);
            if(m->field==2u)put(pixels,width,height,&l,sx+(int)l.cell_px-1,sy+(int)k,0xff843bu);
            if(m->field==3u)put(pixels,width,height,&l,sx+(int)k,sy+(int)l.cell_px-1,0xff843bu);
        }
    }
    /* An unmistakable, mouse-facing preview BEFORE drawing: highlights
     * the exact edge that the next wall brush click will affect. This never
     * edits texture IDs or creates geometry. */
    if(m->wall_tool && m->wall_hover_valid && m->wall_hover_x<m->w &&
       m->wall_hover_y<m->h && m->wall_hover_side<4u){
        unsigned side=m->wall_hover_side;
        int ox=l.x0+(int)(m->wall_hover_x*l.cell_px);
        int oy=l.y0+(int)(m->wall_hover_y*l.cell_px);
        for(unsigned k=0;k<l.cell_px;k++){
            uint32_t color=avp_mm_wall_placeable(m,m->wall_hover_x,m->wall_hover_y,side)?
                               0x3cff7bu:0xff605bu; /* green usable; red blocked */
            if(side==0u)put(pixels,width,height,&l,ox,oy+(int)k,color);
            if(side==1u)put(pixels,width,height,&l,ox+(int)k,oy,color);
            if(side==2u)put(pixels,width,height,&l,ox+(int)l.cell_px-1,oy+(int)k,color);
            if(side==3u)put(pixels,width,height,&l,ox+(int)k,oy+(int)l.cell_px-1,color);
        }
    }

}

void avp_mm_render_overhead(const AvpMapMaker *m,uint32_t *pixels,
                            unsigned width,unsigned height,
                            AvpMmOverheadLookup lookup,void *context)
{
    avp_mm_render_overhead_with_sprites(m,pixels,width,height,lookup,context,NULL);
}
