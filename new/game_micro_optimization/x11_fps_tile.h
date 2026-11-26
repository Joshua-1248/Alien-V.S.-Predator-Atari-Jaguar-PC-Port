#ifndef AVP_MICRO_X11_FPS_TILE_H
#define AVP_MICRO_X11_FPS_TILE_H
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <string.h>
#define AVP_FPS_TILE_W 108u
#define AVP_FPS_TILE_H 21u
/* Cache the exact server-font raster, not a replacement font. The host GC
 * uses its immutable default font and GXcopy throughout this renderer. */
typedef struct AvpMicroFpsTile {
    char label[32];
    uint32_t pixels[AVP_FPS_TILE_W*AVP_FPS_TILE_H];
    int valid;
} AvpMicroFpsTile;
static inline int avp_micro_fps_tile_prepare(AvpMicroFpsTile *tile,
    Display *d,Drawable target,GC gc,unsigned depth,
    unsigned long black,unsigned long white,const char *label)
{
    if(!tile||!d||!target||!gc||!label||strlen(label)>=sizeof(tile->label))return 0;
    if(tile->valid&&!strcmp(tile->label,label))return 1;
    Pixmap p=XCreatePixmap(d,target,AVP_FPS_TILE_W,AVP_FPS_TILE_H,depth);
    if(!p)return 0;
    XSetForeground(d,gc,black);
    XFillRectangle(d,p,gc,0,0,AVP_FPS_TILE_W,AVP_FPS_TILE_H);
    XSetForeground(d,gc,white);
    XDrawString(d,p,gc,7,15,label,(int)strlen(label));
    XImage *im=XGetImage(d,p,0,0,AVP_FPS_TILE_W,AVP_FPS_TILE_H,AllPlanes,ZPixmap);
    XFreePixmap(d,p);
    if(!im)return 0;
    for(unsigned y=0;y<AVP_FPS_TILE_H;y++)for(unsigned x=0;x<AVP_FPS_TILE_W;x++)
        tile->pixels[y*AVP_FPS_TILE_W+x]=(uint32_t)XGetPixel(im,(int)x,(int)y);
    XDestroyImage(im);
    strcpy(tile->label,label);tile->valid=1;
    return 1;
}
static inline int avp_micro_fps_tile_blit(const AvpMicroFpsTile *tile,
    uint32_t *dst,unsigned w,unsigned h)
{
    if(!tile||!tile->valid||!dst||w<AVP_FPS_TILE_W||h<AVP_FPS_TILE_H)return 0;
    for(unsigned y=0;y<AVP_FPS_TILE_H;y++)
        memcpy(dst+(size_t)y*w+w-AVP_FPS_TILE_W,tile->pixels+y*AVP_FPS_TILE_W,
               AVP_FPS_TILE_W*sizeof(uint32_t));
    return 1;
}
#endif
