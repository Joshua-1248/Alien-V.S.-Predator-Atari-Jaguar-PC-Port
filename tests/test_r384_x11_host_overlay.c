/* R384: exercise the ACTUAL host overlay implementation against Xvfb.
 * Source is included solely to access the private X11 presenter helpers;
 * dead global game code is discarded by -ffunction-sections / --gc-sections. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../new/pc_host/src/pc_video_x11.c"

static unsigned count_light(Display *d,Window w,unsigned width,unsigned height,
                            unsigned black)
{
    unsigned lit=0;
    XImage *img=XGetImage(d,w,0,0,width,height,AllPlanes,ZPixmap);
    assert(img);
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)
        if((unsigned)XGetPixel(img,x,y)!=black)lit++;
    XDestroyImage(img);
    return lit;
}
int main(void)
{
    AvpPcVideo *v=calloc(1,sizeof(*v));
    XWindowAttributes a;
    unsigned black;
    assert(v);
    v->dpy=XOpenDisplay(NULL);
    if(!v->dpy){puts("SKIP: no X server");free(v);return 0;}
    v->screen=DefaultScreen(v->dpy);
    black=(unsigned)BlackPixel(v->dpy,v->screen);
    v->win_w=800;v->win_h=600;
    v->win=XCreateSimpleWindow(v->dpy,DefaultRootWindow(v->dpy),0,0,800,600,
        0,black,black);
    assert(v->win);
    v->gc=XCreateGC(v->dpy,v->win,0,NULL);
    assert(v->gc);
    XMapWindow(v->dpy,v->win);
    XSync(v->dpy,False);
    v->cfg.show_fps=1;
    v->fps_meter.sample_valid=1;
    v->fps_meter.display_fps=144;
    assert(avp_pc_host_fps_prepare(v,800));
    assert(pc_x11_direct_window_allowed(v,0));
    v->menu_open=1;
    assert(!pc_x11_direct_window_allowed(v,0));
    v->menu_open=0;
    avp_pc_hybrid_fps_show(v);
    XSync(v->dpy,False);
    assert(v->hybrid_fps_window && v->hybrid_fps_visible);
    assert(XGetWindowAttributes(v->dpy,v->hybrid_fps_window,&a));
    assert(a.map_state==IsViewable);
    assert(count_light(v->dpy,v->hybrid_fps_window,108,21,black)>10u);
    /* A parent game-frame repaint must not erase the FPS child. */
    XSetForeground(v->dpy,v->gc,0x00555555u);
    XFillRectangle(v->dpy,v->win,v->gc,0,0,800,600);
    XSync(v->dpy,False);
    assert(count_light(v->dpy,v->hybrid_fps_window,108,21,black)>10u);
    v->fps_meter.display_fps=65;
    avp_pc_hybrid_fps_show(v);
    XSync(v->dpy,False);
    assert(count_light(v->dpy,v->hybrid_fps_window,108,21,black)>10u);
#ifdef AVP_LOCAL_MULTIPLAYER
    v->local_mp_walktest=1;
    v->local_mp_dm_score_players=3;
    v->local_mp_dm_score_kills[0]=10;
    v->local_mp_dm_score_deaths[0]=2;
    strcpy(v->local_mp_dm_status,"Deathmatch - score 10");
    assert(avp_pc_host_dm_prepare(v,800,600));
    assert(pc_x11_direct_window_allowed(v,0));
    avp_pc_host_dm_show(v,800,600);
    XSync(v->dpy,False);
    assert(v->host_dm_window && v->host_dm_visible && v->host_dm_pixmap);
    assert(XGetWindowAttributes(v->dpy,v->host_dm_window,&a));
    assert(a.map_state==IsViewable);
    assert(count_light(v->dpy,v->host_dm_window,800,v->host_dm_height,black)>15u);
    XSetForeground(v->dpy,v->gc,0x00222222u);
    XFillRectangle(v->dpy,v->win,v->gc,0,0,800,600);
    XSync(v->dpy,False);
    assert(count_light(v->dpy,v->host_dm_window,800,v->host_dm_height,black)>15u);
    avp_pc_host_dm_hide(v);
    assert(XGetWindowAttributes(v->dpy,v->host_dm_window,&a));
    XSync(v->dpy,False);
    assert(XGetWindowAttributes(v->dpy,v->host_dm_window,&a));
    assert(a.map_state==IsUnmapped);
    if(v->host_dm_pixmap)XFreePixmap(v->dpy,v->host_dm_pixmap);
    if(v->host_dm_window)XDestroyWindow(v->dpy,v->host_dm_window);
#endif
    avp_pc_hybrid_fps_hide(v);
    XSync(v->dpy,False);
    assert(XGetWindowAttributes(v->dpy,v->hybrid_fps_window,&a));
    assert(a.map_state==IsUnmapped);
    if(v->host_fps_pixmap)XFreePixmap(v->dpy,v->host_fps_pixmap);
    if(v->hybrid_fps_window)XDestroyWindow(v->dpy,v->hybrid_fps_window);
    XFreeGC(v->dpy,v->gc);
    XDestroyWindow(v->dpy,v->win);
    XCloseDisplay(v->dpy);
    free(v);
    puts("R384 X11 FPS/DM click-through children: PASS");
    return 0;
}
