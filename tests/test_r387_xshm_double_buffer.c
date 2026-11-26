/* R387: exercise the real double-buffer XShm helpers against Xvfb.
 * The drawing/application symbols are discarded by section GC; this test
 * drives XShm transfer, completion events and buffer reuse directly. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../new/pc_host/src/pc_video_x11.c"

static void check_color(Display *d,Window w,unsigned width,unsigned height,
                        uint32_t color)
{
    XImage *image=XGetImage(d,w,0,0,width,height,AllPlanes,ZPixmap);
    assert(image);
    assert((uint32_t)XGetPixel(image,0,0)==color);
    assert((uint32_t)XGetPixel(image,width/2u,height/2u)==color);
    assert((uint32_t)XGetPixel(image,width-1u,height-1u)==color);
    XDestroyImage(image);
}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));
    uint32_t *pixels0,*pixels1;
    XImage *image0,*image1;
    assert(v);
    v->dpy=XOpenDisplay(NULL);
    if(!v->dpy){puts("SKIP: X server unavailable");free(v);return 0;}
    v->screen=DefaultScreen(v->dpy);
    v->win=XCreateSimpleWindow(v->dpy,DefaultRootWindow(v->dpy),
                                0,0,320,240,0,0,0);
    assert(v->win);
    v->gc=XCreateGC(v->dpy,v->win,0,NULL);assert(v->gc);
    XMapWindow(v->dpy,v->win);
    XSync(v->dpy,False);
    if(!XShmQueryExtension(v->dpy)){
        puts("SKIP: MIT-SHM extension unavailable");
        XFreeGC(v->dpy,v->gc);XDestroyWindow(v->dpy,v->win);
        XCloseDisplay(v->dpy);free(v);return 0;
    }
    assert(present_create_shm(v,320,240,&v->shm,&pixels0,&image0,1));
    v->shm_active=1;
    v->shm_images[0]=image0;v->shm_pixels[0]=pixels0;
    assert(present_create_shm(v,320,240,&v->shm_alt,&pixels1,&image1,0));
    v->shm_images[1]=image1;v->shm_pixels[1]=pixels1;
    v->shm_double_buffered=1;
    v->shm_completion_type=XShmGetEventBase(v->dpy)+ShmCompletion;
    v->present_img=image0;v->present_pixels=pixels0;
    assert(pixels0!=pixels1 && image0!=image1);
    for(unsigned frame=0;frame<64;frame++){
        const uint32_t color=((frame*17u)&0xffu)<<16|
                              (((frame*7u)&0xffu)<<8)|((frame*31u)&0xffu);
        for(size_t i=0;i<320u*240u;i++)v->present_pixels[i]=color;
        unsigned prev=v->shm_write_slot;
        assert(XShmPutImage(v->dpy,v->win,v->gc,v->present_img,
                            0,0,0,0,320,240,True));
        XFlush(v->dpy);
        present_shm_next_writer(v);
        assert(v->shm_write_slot==(prev^1u));
        /* The server may still be reading the submitted image. The other
         * slot must not alias it and cannot be reused before completion. */
        assert(v->present_pixels!=v->shm_pixels[prev]);
        if(frame%13u==0u){XSync(v->dpy,False);check_color(v->dpy,v->win,320,240,color);}
    }
    XSync(v->dpy,False);
    /* Release every queued read before either segment is destroyed. */
    present_destroy_image(v);
    assert(!v->present_pixels && !v->present_img && !v->shm_active);
    XFreeGC(v->dpy,v->gc);
    XDestroyWindow(v->dpy,v->win);
    XCloseDisplay(v->dpy);
    free(v);
    puts("R387 dual XShm: 64 alternating images, completion ownership PASS");
    return 0;
}
