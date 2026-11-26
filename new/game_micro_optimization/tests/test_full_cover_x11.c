#include "../presentation_fast.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
    Display *d=XOpenDisplay(NULL);assert(d);int s=DefaultScreen(d);Window root=RootWindow(d,s);unsigned depth=(unsigned)DefaultDepth(d,s);
    const unsigned w=2560,h=1440;
    Pixmap a=XCreatePixmap(d,root,w,h,depth),b=XCreatePixmap(d,root,w,h,depth);GC g=XCreateGC(d,a,0,NULL);
    XImage *im=XCreateImage(d,DefaultVisual(d,s),depth,ZPixmap,0,NULL,w,h,32,0);assert(im);im->data=malloc((size_t)im->bytes_per_line*h);assert(im->data);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)XPutPixel(im,x,y,(x*113u+y*179u)&0xffffffu);
    XSetForeground(d,g,0xfefdabu);XFillRectangle(d,a,g,0,0,w,h);XFillRectangle(d,b,g,0,0,w,h);
    XSetForeground(d,g,BlackPixel(d,s));XFillRectangle(d,a,g,0,0,w,h);
    assert(avp_micro_full_cover(1,0,0,w,h,w,h));
    XPutImage(d,a,g,im,0,0,0,0,w,h);XPutImage(d,b,g,im,0,0,0,0,w,h);
    XSetForeground(d,g,WhitePixel(d,s));XDrawString(d,a,g,10,20,"FPS: 144",8);XDrawString(d,b,g,10,20,"FPS: 144",8);
    XImage *ra=XGetImage(d,a,0,0,w,h,AllPlanes,ZPixmap),*rb=XGetImage(d,b,0,0,w,h,AllPlanes,ZPixmap);assert(ra&&rb);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)assert(XGetPixel(ra,x,y)==XGetPixel(rb,x,y));
    XDestroyImage(ra);XDestroyImage(rb);XDestroyImage(im);XFreeGC(d,g);XFreePixmap(d,a);XFreePixmap(d,b);XCloseDisplay(d);
    printf("PASS: %u actual X11 pixels, dirty full-cover destination and overlay\n",w*h);
}
