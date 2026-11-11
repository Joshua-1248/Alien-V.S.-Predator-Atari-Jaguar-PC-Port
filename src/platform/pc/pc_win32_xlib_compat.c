#include "pc/win32_xlib_compat.h"
#include "pc/pc_win32_keymap.h"
#include <stdio.h>
#include <string.h>

struct AvpWinDisplay {HWND window;};
struct AvpWinPixmap {HDC dc; HBITMAP bitmap, old_bitmap;};
struct AvpWinGC {unsigned long color;};
static const char *const class_name="AVPJaguarNativeWin32";
static XEvent event_queue[256];
static unsigned next_in,next_out;
static int registered;
static int min_width=320,min_height=228;
static HWND mouse_grabbed_window;
static int display_mode_changed;
static int wheel_remainder;

/* Win32 mouse/button WPARAM uses MK_*; translate to X11 modifier masks.
 * Mouse selections, painting and Ctrl+Z depend on the state being accurate. */
static unsigned win_mouse_state(WPARAM wp)
{
    unsigned state=0;
    if(LOWORD(wp)&MK_SHIFT)state|=ShiftMask;
    if(LOWORD(wp)&MK_CONTROL)state|=ControlMask;
    if(LOWORD(wp)&MK_LBUTTON)state|=Button1Mask;
    if(GetKeyState(VK_MENU)&0x8000)state|=Mod1Mask;
    return state;
}
static unsigned win_key_state(void)
{
    unsigned state=0;
    if(GetKeyState(VK_SHIFT)&0x8000)state|=ShiftMask;
    if(GetKeyState(VK_CONTROL)&0x8000)state|=ControlMask;
    if(GetKeyState(VK_MENU)&0x8000)state|=Mod1Mask;
    return state;
}
/* Windows may request repaints while gameplay is not drawing (for example,
   after Alt-Tab). Keep the existing Xlib-style offscreen surface as the one
   authoritative presentation buffer. */
static HDC last_present_dc;
static unsigned last_present_width,last_present_height;

static void enqueue(const XEvent *e) {unsigned next=(next_in+1u)%256u;if(next!=next_out){event_queue[next_in]=*e;next_in=next;}}
static LRESULT CALLBACK wnd_proc(HWND wnd,UINT message,WPARAM wp,LPARAM lp)
{
    XEvent e;memset(&e,0,sizeof(e));
    switch(message){
    case WM_CLOSE:
        e.type=ClientMessage;e.xclient.message_type=1;e.xclient.data.l[0]=2;enqueue(&e);return 0;
    case WM_DESTROY:
        e.type=DestroyNotify;enqueue(&e);return 0;
    case WM_SIZE:
        e.type=ConfigureNotify;e.xconfigure.width=LOWORD(lp);e.xconfigure.height=HIWORD(lp);enqueue(&e);return 0;
    case WM_KILLFOCUS:
        e.type=FocusOut;enqueue(&e);return 0;
    case WM_SETFOCUS:
        e.type=FocusIn;enqueue(&e);return 0;
    case WM_MOUSEMOVE:
        e.type=MotionNotify;e.xmotion.x=(short)LOWORD(lp);e.xmotion.y=(short)HIWORD(lp);
        e.xmotion.state=win_mouse_state(wp);enqueue(&e);return 0;
    case WM_LBUTTONDOWN:case WM_RBUTTONDOWN:case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:case WM_LBUTTONUP:case WM_RBUTTONUP:case WM_MBUTTONUP:case WM_XBUTTONUP:
        e.type=(message==WM_LBUTTONDOWN||message==WM_RBUTTONDOWN||message==WM_MBUTTONDOWN||message==WM_XBUTTONDOWN)?ButtonPress:ButtonRelease;
        e.xbutton.button=(message==WM_LBUTTONDOWN||message==WM_LBUTTONUP)?1u:((message==WM_MBUTTONDOWN||message==WM_MBUTTONUP)?2u:((message==WM_RBUTTONDOWN||message==WM_RBUTTONUP)?3u:(HIWORD(wp)==XBUTTON1?8u:9u)));
        e.xbutton.x=(short)LOWORD(lp);e.xbutton.y=(short)HIWORD(lp);
        e.xbutton.state=win_mouse_state(wp);enqueue(&e);return 0;
    case WM_MOUSEWHEEL:{
        POINT pt={(short)LOWORD(lp),(short)HIWORD(lp)};
        int n=0;
        if(!ScreenToClient(wnd,&pt))return 0;
        wheel_remainder+=(short)HIWORD(wp);
        while((wheel_remainder>=WHEEL_DELTA||wheel_remainder<=-WHEEL_DELTA)&&n++<32){
            e.type=ButtonPress;
            e.xbutton.button=wheel_remainder>0?4u:5u;
            e.xbutton.x=pt.x;e.xbutton.y=pt.y;
            e.xbutton.state=win_mouse_state(wp);
            enqueue(&e);e.type=ButtonRelease;enqueue(&e);
            wheel_remainder+=(wheel_remainder>0)?-WHEEL_DELTA:WHEEL_DELTA;
        }
        return 0;
    }
    case WM_GETMINMAXINFO:{MINMAXINFO *mm=(MINMAXINFO*)lp;mm->ptMinTrackSize.x=min_width;mm->ptMinTrackSize.y=min_height;return 0;}
    case WM_KEYDOWN:case WM_SYSKEYDOWN:case WM_KEYUP:case WM_SYSKEYUP:
        /* Leave Alt+F4 to DefWindowProc so Windows can close the window.
           All other mapped keys keep the existing Jaguar input routing. */
        if((message==WM_SYSKEYDOWN||message==WM_SYSKEYUP) &&
           wp==VK_F4 && (lp & (1L<<29)))break;
        e.type=(message==WM_KEYDOWN||message==WM_SYSKEYDOWN)?KeyPress:KeyRelease;
        {
            unsigned scan=((unsigned long)lp>>16)&255u;
            int extended=(((unsigned long)lp>>24)&1u)!=0;
            int shift=(GetKeyState(VK_SHIFT)&0x8000)!=0;
            e.xkey.keycode=avp_pc_win32_keycode(scan,extended);
            e.xkey.sym=avp_pc_win32_keysym((unsigned)wp,scan,shift,extended);
            e.xkey.state=win_key_state();
        }
        if(e.xkey.sym)enqueue(&e);
        return 0;
    case WM_PAINT:{
        PAINTSTRUCT ps;
        HDC target=BeginPaint(wnd,&ps);
        if(target&&last_present_dc){
            BitBlt(target,0,0,(int)last_present_width,(int)last_present_height,
                   last_present_dc,0,0,SRCCOPY);
        }
        EndPaint(wnd,&ps);
        return 0;
    }
    case WM_ERASEBKGND:return 1;
    default:break;
    }
    return DefWindowProcA(wnd,message,wp,lp);
}
Display *XOpenDisplay(const char *s)
{
    Display *d;WNDCLASSEXA wc;(void)s;
    d=(Display*)calloc(1,sizeof(*d));if(!d)return NULL;
    if(!registered){memset(&wc,0,sizeof(wc));wc.cbSize=sizeof(wc);wc.hInstance=GetModuleHandleA(NULL);
        wc.lpfnWndProc=wnd_proc;wc.lpszClassName=class_name;wc.hCursor=LoadCursorA(NULL,IDC_ARROW);
        if(!RegisterClassExA(&wc)){free(d);return NULL;}registered=1;}
    return d;
}
int XCloseDisplay(Display *d){free(d);return 0;}
int XSetErrorHandler(int (*fn)(Display*,XErrorEvent*)){(void)fn;return 0;}
int XSetIOErrorHandler(int (*fn)(Display*)){(void)fn;return 0;}
int XGetErrorText(Display *d,int code,char *s,int n){(void)d;if(s&&n>0)snprintf(s,(size_t)n,"Win32 error %d",code);return 0;}
int DefaultScreen(Display *d){(void)d;return 0;}
void *DefaultVisual(Display *d,int sc){(void)d;(void)sc;return NULL;}
int DefaultDepth(Display *d,int sc){(void)d;(void)sc;return 32;}
Window RootWindow(Display *d,int sc){(void)d;(void)sc;return NULL;}
unsigned long BlackPixel(Display *d,int sc){(void)d;(void)sc;return 0u;}
unsigned long WhitePixel(Display *d,int sc){(void)d;(void)sc;return 0xffffffu;}
Window XCreateSimpleWindow(Display *d,Window parent,int x,int y,unsigned w,unsigned h,unsigned border,unsigned long f,unsigned long b)
{
    RECT r={0,0,(LONG)w,(LONG)h};(void)parent;(void)border;(void)f;(void)b;
    AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
    d->window=CreateWindowExA(0,class_name,"Alien vs Predator",WS_OVERLAPPEDWINDOW,
      x?x:CW_USEDEFAULT,y?y:CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,
      NULL,NULL,GetModuleHandleA(NULL),NULL);
    return d->window;
}
int XDestroyWindow(Display *d,Window w){(void)d;if(w)DestroyWindow(w);return 0;}
int XStoreName(Display *d,Window w,const char *s){(void)d;return SetWindowTextA(w,s)?0:-1;}
int XSelectInput(Display *d,Window w,long flags){(void)d;(void)w;(void)flags;return 0;}
int XSetWMNormalHints(Display *d,Window w,XSizeHints *h){(void)d;(void)w;if(h){min_width=h->min_width;min_height=h->min_height;}return 0;}
Atom XInternAtom(Display *d,const char *name,int only){(void)d;(void)only;return strcmp(name,"WM_PROTOCOLS")==0?1u:2u;}
int XSetWMProtocols(Display *d,Window w,Atom *a,int n){(void)d;(void)w;(void)a;(void)n;return 0;}
GC XCreateGC(Display *d,Window w,unsigned long mask,void *values)
{(void)d;(void)w;(void)mask;(void)values;return (GC)calloc(1,sizeof(struct AvpWinGC));}
int XFreeGC(Display *d,GC gc){(void)d;free(gc);return 0;}
Pixmap XCreatePixmap(Display *d,Window w,unsigned width,unsigned height,unsigned depth)
{
    HDC wnd,dc;Pixmap p;(void)d;(void)depth;
    p=(Pixmap)calloc(1,sizeof(*p));if(!p)return NULL;
    wnd=GetDC(w);dc=CreateCompatibleDC(wnd);p->bitmap=CreateCompatibleBitmap(wnd,(int)width,(int)height);
    ReleaseDC(w,wnd);
    if(!dc||!p->bitmap){if(dc)DeleteDC(dc);if(p->bitmap)DeleteObject(p->bitmap);free(p);return NULL;}
    p->dc=dc;p->old_bitmap=(HBITMAP)SelectObject(dc,p->bitmap);return p;
}
int XFreePixmap(Display *d,Pixmap p)
{
    (void)d;
    if(p){
        if(last_present_dc==p->dc){last_present_dc=NULL;last_present_width=last_present_height=0;}
        SelectObject(p->dc,p->old_bitmap);DeleteObject(p->bitmap);DeleteDC(p->dc);free(p);
    }
    return 0;
}
XImage *XCreateImage(Display *d,void *visual,unsigned depth,int format,int offset,char *data,unsigned w,unsigned h,int pad,int stride)
{XImage *p=(XImage*)calloc(1,sizeof(*p));(void)d;(void)visual;(void)depth;(void)format;(void)offset;(void)pad;(void)stride;if(p){p->data=data;p->width=w;p->height=h;}return p;}
int XDestroyImage(XImage *image){free(image);return 0;}
static COLORREF pixel(unsigned long c){return RGB((c>>16)&255u,(c>>8)&255u,c&255u);}
int XSetForeground(Display *d,GC gc,unsigned long color){(void)d;if(gc)gc->color=color;return 0;}
/* On Windows, the direct-window X11 fast path is normally disabled, but
 * drawables still need correct dispatch for the FPS and overlay helpers. */
static HDC target_dc(Display *d,Drawable target,Window *window)
{
    if(!target)return NULL;
    if(d && (Window)target==d->window){
        *window=d->window;
        return GetDC(*window);
    }
    *window=NULL;
    return ((Pixmap)target)->dc;
}
int XFillRectangle(Display *d,Drawable target,GC gc,int x,int y,unsigned w,unsigned h)
{
    RECT r={x,y,x+(LONG)w,y+(LONG)h};HBRUSH b;Window win=NULL;HDC dc;
    if(!target||!gc)return -1;
    dc=target_dc(d,target,&win);if(!dc)return -1;
    b=CreateSolidBrush(pixel(gc->color));
    if(!b){if(win)ReleaseDC(win,dc);return -1;}
    FillRect(dc,&r,b);DeleteObject(b);
    if(win)ReleaseDC(win,dc);
    return 0;
}
/* Xlib's XDrawLine includes the final pixel. Win32's LineTo does not,
 * so write the endpoint explicitly. Use the GC foreground color and draw
 * onto either a pixmap DC or the window DC, like XDrawString does. */
int XDrawLine(Display *d,Drawable target,GC gc,int x1,int y1,int x2,int y2)
{
    Window win=NULL;HDC dc;HPEN pen,old_pen;int ok;
    if(!target||!gc)return -1;
    dc=target_dc(d,target,&win);if(!dc)return -1;
    pen=CreatePen(PS_SOLID,1,pixel(gc->color));
    if(!pen){if(win)ReleaseDC(win,dc);return -1;}
    old_pen=(HPEN)SelectObject(dc,pen);
    ok=MoveToEx(dc,x1,y1,NULL) && LineTo(dc,x2,y2) &&
       SetPixelV(dc,x2,y2,pixel(gc->color));
    SelectObject(dc,old_pen);
    DeleteObject(pen);
    if(win)ReleaseDC(win,dc);
    return ok?0:-1;
}
/* XDrawRectangle is an unfilled outline. X11 width/height specify the
 * distance between extreme pixels; GDI Rectangle uses exclusive right and
 * bottom coordinates. The NULL_BRUSH prevents filling the preview. */
int XDrawRectangle(Display *d,Drawable target,GC gc,int x,int y,unsigned w,unsigned h)
{
    Window win=NULL;HDC dc;HPEN pen,old_pen;HGDIOBJ old_brush;int ok;
    if(!target||!gc)return -1;
    dc=target_dc(d,target,&win);if(!dc)return -1;
    pen=CreatePen(PS_SOLID,1,pixel(gc->color));
    if(!pen){if(win)ReleaseDC(win,dc);return -1;}
    old_pen=(HPEN)SelectObject(dc,pen);
    old_brush=SelectObject(dc,GetStockObject(NULL_BRUSH));
    ok=Rectangle(dc,x,y,x+(int)w+1,y+(int)h+1);
    SelectObject(dc,old_brush);
    SelectObject(dc,old_pen);
    DeleteObject(pen);
    if(win)ReleaseDC(win,dc);
    return ok?0:-1;
}
int XPutImage(Display *d,Pixmap p,GC gc,XImage *img,int sx,int sy,int dx,int dy,unsigned w,unsigned h)
{BITMAPINFO bmi;(void)d;(void)gc;if(!p||!img||!img->data)return -1;memset(&bmi,0,sizeof(bmi));
 bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bmi.bmiHeader.biWidth=(LONG)img->width;
 bmi.bmiHeader.biHeight=-(LONG)img->height;bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;
 bmi.bmiHeader.biCompression=BI_RGB;
 return StretchDIBits(p->dc,dx,dy,(int)w,(int)h,sx,sy,(int)w,(int)h,img->data,&bmi,DIB_RGB_COLORS,SRCCOPY)==(int)GDI_ERROR?-1:0;}
int XDrawString(Display *d,Drawable target,GC gc,int x,int y,const char *str,int n)
{
    Window win=NULL;HDC dc;int ok;
    if(!target||!gc||!str)return -1;
    dc=target_dc(d,target,&win);if(!dc)return -1;
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,pixel(gc->color));
    ok=TextOutA(dc,x,y-13,str,n)?0:-1;
    if(win)ReleaseDC(win,dc);
    return ok;
}
int XCopyArea(Display *d,Pixmap from,Window w,GC gc,int x,int y,unsigned width,unsigned height,int dx,int dy)
{
    HDC dc;
    (void)d;(void)gc;
    if(!from||!w)return -1;
    dc=GetDC(w);
    if(!dc)return -1;
    last_present_dc=from->dc;
    last_present_width=width;last_present_height=height;
    BitBlt(dc,dx,dy,(int)width,(int)height,from->dc,x,y,SRCCOPY);
    ReleaseDC(w,dc);
    return 0;
}
int XMapWindow(Display *d,Window w){(void)d;ShowWindow(w,SW_SHOW);UpdateWindow(w);return 0;}
int XFlush(Display *d){(void)d;GdiFlush();return 0;}
int XSync(Display *d,int discard){(void)discard;return XFlush(d);}
int XGrabPointer(Display *d,Window w,int owner,unsigned events,int pm,int km,Window confine,unsigned long cursor,unsigned long time){
    RECT r;POINT a,b;(void)d;(void)owner;(void)events;(void)pm;(void)km;(void)confine;(void)cursor;(void)time;
    if(!w||!GetClientRect(w,&r))return 1;
    a.x=r.left;a.y=r.top;b.x=r.right;b.y=r.bottom;
    ClientToScreen(w,&a);ClientToScreen(w,&b);r.left=a.x;r.top=a.y;r.right=b.x;r.bottom=b.y;
    if(!ClipCursor(&r))return 1;
    SetCapture(w);mouse_grabbed_window=w;ShowCursor(FALSE);return GrabSuccess;
}
int XUngrabPointer(Display *d,unsigned long time){(void)d;(void)time;if(mouse_grabbed_window){ClipCursor(NULL);ReleaseCapture();ShowCursor(TRUE);mouse_grabbed_window=NULL;}return 0;}
int XWarpPointer(Display *d,Window src,Window dest,int sx,int sy,unsigned sw,unsigned sh,int dx,int dy){POINT p={dx,dy};(void)d;(void)src;(void)sx;(void)sy;(void)sw;(void)sh;if(!dest)return -1;ClientToScreen(dest,&p);return SetCursorPos(p.x,p.y)?0:-1;}

int XPending(Display *d)
{MSG msg;(void)d;while(PeekMessageA(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageA(&msg);}return (int)((next_in+256u-next_out)%256u);}
int XNextEvent(Display *d,XEvent *e)
{(void)d;if(next_in==next_out)return -1;*e=event_queue[next_out];next_out=(next_out+1u)%256u;return 0;}
KeySym XLookupKeysym(const void *key_event,int index)
{
    const struct {KeyCode keycode;KeySym sym;unsigned state;} *k=(const void*)key_event;
    KeySym sym=k->sym;
    if(index!=1 || !(k->state&ShiftMask))return sym;
    /* XLookupKeysym(event,1) is used for Save As filename entry. */
    if(sym>='a'&&sym<='z')return sym-'a'+'A';
    if(sym==XK_comma)return XK_less;
    if(sym==XK_period)return XK_greater;
    if(sym==XK_minus)return '_';
    if(sym==XK_equal)return XK_plus;
    return sym;
}
const char *XKeysymToString(KeySym sym)
{static char buf[3];switch(sym){case XK_BackSpace:return "BackSpace";case XK_Delete:return "Delete";case XK_Tab:return "Tab";case XK_Return:return "Return";case XK_KP_Enter:return "KP_Enter";
 case XK_Escape:return "Escape";case XK_Left:return "Left";case XK_Up:return "Up";case XK_Right:return "Right";
 case XK_Down:return "Down";case XK_Shift_L:return "Shift_L";case XK_Shift_R:return "Shift_R";
 case XK_F2:return "F2";case XK_F5:return "F5";case XK_F6:return "F6";case XK_F7:return "F7";case XK_F8:return "F8";case XK_F9:return "F9";case XK_F10:return "F10";case XK_F12:return "F12";
 case XK_Page_Up:return "Page_Up";case XK_Page_Down:return "Page_Down";
 default:break;}
 if(sym>=32u&&sym<127u){buf[0]=(char)sym;buf[1]=0;return buf;}return "Unknown";}

/* Fullscreen refresh is a real monitor mode on Win32. Never claim that an
 * unsupported rate was applied; use the existing desktop mode as fallback. */
unsigned avp_win32_display_rates(unsigned width,unsigned height,unsigned *out,unsigned cap)
{
    DEVMODEA m;unsigned n=0;
    if(!out || !cap)return 0;
    for(DWORD i=0;;i++){
        memset(&m,0,sizeof(m));m.dmSize=sizeof(m);
        if(!EnumDisplaySettingsA(NULL,i,&m))break;
        if(m.dmPelsWidth!=width || m.dmPelsHeight!=height || m.dmDisplayFrequency<30 || m.dmDisplayFrequency>360)continue;
        unsigned hz=m.dmDisplayFrequency;int duplicate=0;
        for(unsigned j=0;j<n;j++)if(out[j]==hz)duplicate=1;
        if(!duplicate && n<cap)out[n++]=hz;
    }
    return n;
}
void avp_win32_display_restore(void)
{
    if(display_mode_changed){(void)ChangeDisplaySettingsExA(NULL,NULL,NULL,0,NULL);display_mode_changed=0;}
}
void avp_win32_display_apply(Window win,unsigned mode,unsigned width,unsigned height,unsigned rate)
{
    RECT rect;DWORD style;int x,y,w,h;
    if(!win)return;
    avp_win32_display_restore();
    if(mode==2){
        DEVMODEA requested;memset(&requested,0,sizeof(requested));requested.dmSize=sizeof(requested);
        requested.dmPelsWidth=width;requested.dmPelsHeight=height;
        requested.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT;
        if(rate){
            unsigned available[64],count=avp_win32_display_rates(width,height,available,64);
            for(unsigned i=0;i<count;i++)if(available[i]==rate){requested.dmDisplayFrequency=rate;requested.dmFields|=DM_DISPLAYFREQUENCY;break;}
        }
        if(ChangeDisplaySettingsExA(NULL,&requested,NULL,CDS_FULLSCREEN,NULL)==DISP_CHANGE_SUCCESSFUL)display_mode_changed=1;
    }
    style=mode==0?WS_OVERLAPPEDWINDOW:WS_POPUP;
    SetWindowLongPtrA(win,GWL_STYLE,(LONG_PTR)style);
    if(mode==0){
        rect.left=0;rect.top=0;rect.right=(LONG)width;rect.bottom=(LONG)height;
        AdjustWindowRect(&rect,style,FALSE);w=rect.right-rect.left;h=rect.bottom-rect.top;
        x=(GetSystemMetrics(SM_CXSCREEN)-w)/2;y=(GetSystemMetrics(SM_CYSCREEN)-h)/2;
    }else{x=0;y=0;w=GetSystemMetrics(SM_CXSCREEN);h=GetSystemMetrics(SM_CYSCREEN);}
    SetWindowPos(win,mode==0?HWND_NOTOPMOST:HWND_TOP,x,y,w,h,SWP_FRAMECHANGED|SWP_SHOWWINDOW);
}
