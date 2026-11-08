/* RE31 R12: X11 readback equivalence of direct status overlay versus the
 * original back-pixmap path, on Linux builds without optional MIT-SHM.
 * No retail ROM and no substituted simulated gameplay is involved. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test) do { if(!(test)){fprintf(stderr,"R12 X11 failed at %d: %s\n",__LINE__,#test);goto done;} } while(0)
static void overlay(Display *d,Drawable target,GC gc,int screen,unsigned w,
                    const char *status,const char *fps)
{
    XSetForeground(d,gc,BlackPixel(d,screen));
    XFillRectangle(d,target,gc,0,0,w,23u);
    XSetForeground(d,gc,WhitePixel(d,screen));
    XDrawString(d,target,gc,8,16,status,(int)strlen(status));
    if(fps){
        const unsigned x=w>108u?w-108u:0u;
        XSetForeground(d,gc,BlackPixel(d,screen));
        XFillRectangle(d,target,gc,(int)x,0,108u,21u);
        XSetForeground(d,gc,WhitePixel(d,screen));
        XDrawString(d,target,gc,(int)x+7,15,fps,(int)strlen(fps));
    }
}
static int run_case(Display *d,Window win,GC gc,int screen,unsigned w,unsigned h,
                    const char *status,const char *fps)
{
    int ok=0;
    XImage *src=NULL,*direct=NULL,*reference=NULL;
    Pixmap back=0;
    uint32_t *buffer=NULL;
    buffer=(uint32_t *)malloc((size_t)w*h*sizeof(*buffer));
    CHECK(buffer!=NULL);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)
        buffer[(size_t)y*w+x]=((x*13u+y*41u+7u)&255u)<<16 |
            ((x*71u+y*3u+29u)&255u)<<8 | ((x*5u+y*17u+121u)&255u);
    src=XCreateImage(d,DefaultVisual(d,screen),(unsigned)DefaultDepth(d,screen),
                     ZPixmap,0,(char *)buffer,w,h,32,0);
    CHECK(src!=NULL);
    buffer=NULL; /* XDestroyImage releases it. */
    back=XCreatePixmap(d,win,w,h,(unsigned)DefaultDepth(d,screen));
    CHECK(back!=0);
    /* R12 optimized: pixels directly to window, banner/FPS afterward. */
    XPutImage(d,win,gc,src,0,0,0,0,w,h);
    overlay(d,win,gc,screen,w,status,fps);
    XSync(d,False);
    direct=XGetImage(d,win,0,0,w,h,AllPlanes,ZPixmap);
    CHECK(direct!=NULL);
    /* R11 reference: pixels into back pixmap, banner/FPS, full copy. */
    XPutImage(d,back,gc,src,0,0,0,0,w,h);
    overlay(d,back,gc,screen,w,status,fps);
    XCopyArea(d,back,win,gc,0,0,w,h,0,0);
    XSync(d,False);
    reference=XGetImage(d,win,0,0,w,h,AllPlanes,ZPixmap);
    CHECK(reference!=NULL);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        const unsigned long a=XGetPixel(direct,(int)x,(int)y);
        const unsigned long b=XGetPixel(reference,(int)x,(int)y);
        if(a!=b){fprintf(stderr,"R12 X11 mismatch at %u,%u: %08lx/%08lx\n",x,y,a,b);goto done;}
    }
    ok=1;
done:
    if(reference)XDestroyImage(reference);
    if(direct)XDestroyImage(direct);
    if(back)XFreePixmap(d,back);
    if(src)XDestroyImage(src);
    else free(buffer);
    return ok;
}
int main(void)
{
    Display *d=XOpenDisplay(NULL);
    GC gc=0;
    Window win=0;
    int ok=0;
    int screen;
    if(!d){fprintf(stderr,"X display unavailable\n");return 77;}
    screen=DefaultScreen(d);
    win=XCreateSimpleWindow(d,RootWindow(d,screen),0,0,1920,1080,0,0,0);
    if(!win)goto done;
    gc=XCreateGC(d,win,0,NULL);
    if(!gc)goto done;
    XMapRaised(d,win);
    XSync(d,False);
    ok=run_case(d,win,gc,screen,640,456,"DEATHMATCH - Unlimited Time | P1 2/3",NULL) &&
       run_case(d,win,gc,screen,1920,1080,"DEATHMATCH - Unlimited Time | P1 2/3", "FPS: 240") &&
       run_case(d,win,gc,screen,1920,1080,"DEATHMATCH OVER - PLAYER 1 WINS | P1 12/7", "FPS: 120");
    if(ok)puts("R12 X11 deathmatch direct/backbuffer pixel parity: PASS (3 cases)");
done:
    if(gc)XFreeGC(d,gc);
    if(win)XDestroyWindow(d,win);
    XCloseDisplay(d);
    return ok?0:1;
}
