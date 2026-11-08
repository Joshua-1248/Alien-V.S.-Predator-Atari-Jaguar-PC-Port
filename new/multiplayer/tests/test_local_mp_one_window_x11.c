/* Host display smoke: one actual X11 window with up to four independently
 * supplied complete frames. This is NOT a gameplay renderer or fake co-op.
 * No original Jaguar code, ROM, or additional windows/processes are used. */
#include "local_mp_compositor.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC_W 320u
#define SRC_H 228u
#define OUT_W 640u
#define OUT_H 456u
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"check failed at line %d: %s\n",__LINE__,#x); goto failed; } } while (0)

static uint32_t images[AVP_LOCAL_MP_MAX_PLAYERS][SRC_H][SRC_W];
static const uint32_t world_color[4] = {0x00200020u,0x00004030u,0x00402000u,0x00202060u};
static const uint32_t hud_color[4] = {0x00bf1c1cu,0x001cbf1cu,0x001c1cbfu,0x00bfbf1cu};

static int show_case(Display *display, Window window, GC gc, unsigned players, unsigned layout)
{
    AvpLocalMpSetup setup;
    AvpLocalMpSourceFrame source[4] = {{0}};
    AvpLocalMpOutputFrame output;
    AvpLocalMpRect rects[4];
    uint32_t *pixels = NULL;
    XImage *image = NULL, *readback = NULL;
    unsigned p,y,x,n;
    int good = 0;
    avp_local_mp_defaults(&setup);
    setup.players = players;
    setup.layout = layout;
    if(players==3u && layout==2u){setup.layout=AVP_MP_SIDE_BY_SIDE;setup.three_layout=AVP_MP_THREE_WIDE_TOP;}
    setup.enabled = 1;
    for (p=0;p<4;p++) {
        for(y=0;y<SRC_H;y++)
            for(x=0;x<SRC_W;x++)
                images[p][y][x] = y >= SRC_H-18u ? hud_color[p] : world_color[p];
        source[p] = (AvpLocalMpSourceFrame){&images[p][0][0],SRC_W,SRC_H,SRC_W};
    }
    pixels = calloc((size_t)OUT_W*OUT_H,sizeof *pixels);
    CHECK(pixels != NULL);
    output = (AvpLocalMpOutputFrame){pixels,OUT_W,OUT_H,OUT_W};
    CHECK(avp_local_mp_compose(&setup,source,&output,0)==players);
    image = XCreateImage(display,DefaultVisual(display,DefaultScreen(display)),
                         (unsigned)DefaultDepth(display,DefaultScreen(display)),
                         ZPixmap,0,(char *)pixels,OUT_W,OUT_H,32,0);
    CHECK(image != NULL);
    pixels=NULL; /* XDestroyImage owns the image data. */
    (void)XPutImage(display,window,gc,image,0,0,0,0,OUT_W,OUT_H);
    XSync(display,False);
    readback=XGetImage(display,window,0,0,OUT_W,OUT_H,AllPlanes,ZPixmap);
    CHECK(readback != NULL);
    n=avp_local_mp_viewports(&setup,OUT_W,OUT_H,rects);
    CHECK(n==players);
    for(p=0;p<n;p++) {
        const AvpLocalMpRect *r=&rects[p];
        CHECK(XGetPixel(readback,(int)(r->x+r->width/2u), (int)(r->y+r->height/2u))==world_color[p]);
        CHECK(XGetPixel(readback,(int)r->x,(int)(r->y+r->height-1u))==hud_color[p]);
    }
    if(players==3u && layout!=2u)
        CHECK(XGetPixel(readback, (int)(3u*OUT_W/4u), (int)(3u*OUT_H/4u))==0u);
    /* No child/secondary game windows are created by presenting any layout. */
    { Window root,parent,*children=NULL;unsigned child_count=0;
      CHECK(XQueryTree(display,window,&root,&parent,&children,&child_count)!=0);
      if(children)XFree(children);
      CHECK(child_count==0u);
    }
    good=1;
failed:
    if(readback)XDestroyImage(readback);
    if(image)XDestroyImage(image);
    else free(pixels);
    return good;
}
int main(void)
{
    Display *d=XOpenDisplay(NULL);
    Window window;
    GC gc;
    int ok;
    if(!d){fputs("Cannot open X display\n",stderr);return 2;}
    window=XCreateSimpleWindow(d,DefaultRootWindow(d),0,0,OUT_W,OUT_H,0,0,0);
    if(!window){XCloseDisplay(d);return 2;}
    XStoreName(d,window,"AVP One Window / Four Viewports Test");
    gc=XCreateGC(d,window,0,NULL);
    if(!gc){XDestroyWindow(d,window);XCloseDisplay(d);return 2;}
    XMapRaised(d,window);
    XSync(d,False);
    ok=show_case(d,window,gc,1,AVP_MP_SIDE_BY_SIDE) &&
       show_case(d,window,gc,2,AVP_MP_SIDE_BY_SIDE) &&
       show_case(d,window,gc,2,AVP_MP_TOP_BOTTOM) &&
       show_case(d,window,gc,3,AVP_MP_SIDE_BY_SIDE) &&
       show_case(d,window,gc,3,2u) &&
       show_case(d,window,gc,4,AVP_MP_SIDE_BY_SIDE);
    XFreeGC(d,gc);
    XDestroyWindow(d,window);
    XCloseDisplay(d);
    if(ok)puts("ONE host window / 1-4 full frames / HUD scaling / pixel readback: PASS");
    return ok?0:1;
}
