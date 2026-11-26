#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../x11_fps_tile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void){
    Display *d=XOpenDisplay(NULL);assert(d);
    int screen=DefaultScreen(d);Window root=RootWindow(d,screen);
    unsigned depth=(unsigned)DefaultDepth(d,screen),w=1280,h=720;
    unsigned long black=BlackPixel(d,screen),white=WhitePixel(d,screen);
    GC gc=XCreateGC(d,root,0,NULL);assert(gc);
    Pixmap p=XCreatePixmap(d,root,w,h,depth);assert(p);
    AvpMicroFpsTile t={0}; uint32_t *out=malloc((size_t)w*h*4);assert(out);
    const char *labels[]={"FPS: --","FPS: 60","FPS: 144","FPS: 240","FPS: 240","FPS: 1000","FPS: 0"};
    unsigned long bg=0x123456; unsigned long long total=0;
    for(unsigned i=0;i<sizeof(labels)/sizeof(labels[0]);i++){
        XSetForeground(d,gc,bg);XFillRectangle(d,p,gc,0,0,w,h);
        XSetForeground(d,gc,black);XFillRectangle(d,p,gc,w-108,0,108,21);
        XSetForeground(d,gc,white);XDrawString(d,p,gc,w-101,15,labels[i],(int)strlen(labels[i]));
        XImage *ref=XGetImage(d,p,0,0,w,h,AllPlanes,ZPixmap);assert(ref);
        for(unsigned j=0;j<w*h;j++)out[j]=(uint32_t)bg;
        assert(avp_micro_fps_tile_prepare(&t,d,root,gc,depth,black,white,labels[i]));
        assert(avp_micro_fps_tile_blit(&t,out,w,h));
        for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)assert(out[y*w+x]==XGetPixel(ref,x,y));
        total+=(unsigned long long)w*h;XDestroyImage(ref);
    }
    assert(!avp_micro_fps_tile_blit(&t,out,107,720));
    printf("PASS: %llu exact X11 pixels, seven label states\n",total);
    free(out);XFreePixmap(d,p);XFreeGC(d,gc);XCloseDisplay(d);
}
