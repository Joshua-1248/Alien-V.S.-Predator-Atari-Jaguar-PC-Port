#ifndef AVP_WIN32_XLIB_COMPAT_H
#define AVP_WIN32_XLIB_COMPAT_H
/* Win32/GDI counterpart of the small Xlib surface used by pc_video_x11.c.
   Only the calls used by the existing PC frontend are exposed; game code,
   pad bits, renderer state and Jaguar timing remain platform-independent. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
/* winnt.h defines POWER_INFORMATION_LEVEL enumerator ScreenOff, which
   collides with the original game routine void ScreenOff(void). */
#define ScreenOff avp_winnt_ScreenOff
#include <windows.h>
#undef ScreenOff
#include <stdint.h>
#include <stdlib.h>

typedef struct AvpWinDisplay Display;
typedef HWND Window;
typedef struct AvpWinPixmap *Pixmap;
/* Xlib Drawables accept both offscreen Pixmaps and on-screen Windows. */
typedef void *Drawable;
typedef struct AvpWinGC *GC;
typedef unsigned long KeySym;
typedef unsigned char KeyCode;
typedef unsigned long Atom;
typedef struct AvpWinImage {char *data; unsigned width,height;} XImage;
typedef struct AvpWinHints {long flags;int min_width,min_height;} XSizeHints;
typedef struct AvpWinError {unsigned error_code,request_code,minor_code;unsigned long resourceid;} XErrorEvent;
typedef struct AvpWinEvent {
    int type;
    struct {Atom message_type;struct {long l[5];} data;} xclient;
    struct {int width,height;} xconfigure;
    struct {KeyCode keycode;KeySym sym;unsigned state;} xkey;
    struct {int x,y;unsigned state;} xmotion;
    struct {int x,y;unsigned button,state;} xbutton;
} XEvent;
#define False 0
#define True 1
#define NoSymbol 0
#define PMinSize 1
#define ExposureMask 1
#define KeyPressMask 2
#define KeyReleaseMask 4
#define StructureNotifyMask 8
#define FocusChangeMask 16
#define PointerMotionMask 32
#define ButtonPressMask 64
#define ButtonReleaseMask 128
#define FocusIn 9
#define MotionNotify 6
#define ButtonPress 4
#define ButtonRelease 5
#define None 0
#define CurrentTime 0
#define GrabModeAsync 1
#define GrabSuccess 0
#define ZPixmap 2
#define ClientMessage 33
#define DestroyNotify 17
#define ConfigureNotify 22
#define FocusOut 10
#define KeyPress 2
#define KeyRelease 3
#define XK_BackSpace 0xff08u
#define XK_Delete 0xffffu
#define XK_Tab 0xff09u
#define XK_Return 0xff0du
#define XK_KP_Enter 0xff8du
#define ShiftMask 1u
#define ControlMask 4u
#define Mod1Mask 8u
#define Button1Mask 256u
#define XK_Escape 0xff1bu
#define XK_Left 0xff51u
#define XK_Up 0xff52u
#define XK_Right 0xff53u
#define XK_Down 0xff54u
#define XK_Home 0xff50u
#define XK_End 0xff57u
#define XK_Page_Up 0xff55u
#define XK_Page_Down 0xff56u
#define XK_Shift_L 0xffe1u
#define XK_Shift_R 0xffe2u
#define XK_Control_L 0xffe3u
#define XK_Control_R 0xffe4u
#define XK_F2 0xffbfu
#define XK_F3 0xffc0u
#define XK_F5 0xffc2u
#define XK_F6 0xffc3u
#define XK_F7 0xffc4u
#define XK_F8 0xffc5u
#define XK_F9 0xffc6u
#define XK_F10 0xffc7u
#define XK_F12 0xffc9u
#define XK_space 0x20u
#define XK_numbersign 0x23u
#define XK_asterisk 0x2au
#define XK_comma 0x2cu
#define XK_period 0x2eu
#define XK_plus 0x2bu
#define XK_minus 0x2du
#define XK_less 0x3cu
#define XK_greater 0x3eu
#define XK_equal 0x3du
#define XK_KP_Add 0xffabu
#define XK_KP_Subtract 0xffadu
#define XK_Y 0x59u
#define XK_Z 0x5au
#define XK_0 0x30u
#define XK_1 0x31u
#define XK_2 0x32u
#define XK_3 0x33u
#define XK_4 0x34u
#define XK_5 0x35u
#define XK_6 0x36u
#define XK_7 0x37u
#define XK_8 0x38u
#define XK_9 0x39u
#define XK_a 0x61u
#define XK_b 0x62u
#define XK_c 0x63u
#define XK_d 0x64u
#define XK_e 0x65u
#define XK_g 0x67u
#define XK_h 0x68u
#define XK_i 0x69u
#define XK_j 0x6au
#define XK_n 0x6eu
#define XK_o 0x6fu
#define XK_p 0x70u
#define XK_q 0x71u
#define XK_r 0x72u
#define XK_s 0x73u
#define XK_v 0x76u
#define XK_w 0x77u
#define XK_x 0x78u
#define XK_y 0x79u
#define XK_z 0x7au
#define XK_bracketleft 0x5bu
#define XK_bracketright 0x5du
Display *XOpenDisplay(const char *s);
/* Native display operations, separate from Xlib compatibility rendering. */
unsigned avp_win32_display_rates(unsigned width,unsigned height,unsigned *out,unsigned cap);
void avp_win32_display_apply(Window window,unsigned mode,unsigned width,unsigned height,unsigned rate);
void avp_win32_display_restore(void);
int XCloseDisplay(Display *d);
int XSetErrorHandler(int (*fn)(Display*,XErrorEvent*));
int XSetIOErrorHandler(int (*fn)(Display*));
int XGetErrorText(Display *d,int code,char *buffer,int n);
int DefaultScreen(Display *d);
void *DefaultVisual(Display *d,int screen);
int DefaultDepth(Display *d,int screen);
Window RootWindow(Display *d,int screen);
unsigned long BlackPixel(Display *d,int screen);
unsigned long WhitePixel(Display *d,int screen);
Window XCreateSimpleWindow(Display *d,Window parent,int x,int y,unsigned w,unsigned h,unsigned border,unsigned long fore,unsigned long back);
int XDestroyWindow(Display *d,Window win);
int XStoreName(Display *d,Window win,const char *name);
int XSelectInput(Display *d,Window win,long events);
int XSetWMNormalHints(Display *d,Window win,XSizeHints *hints);
Atom XInternAtom(Display *d,const char *name,int only);
int XSetWMProtocols(Display *d,Window win,Atom *atoms,int n);
GC XCreateGC(Display *d,Window win,unsigned long mask,void *values);
int XFreeGC(Display *d,GC gc);
Pixmap XCreatePixmap(Display *d,Window win,unsigned w,unsigned h,unsigned depth);
int XFreePixmap(Display *d,Pixmap p);
XImage *XCreateImage(Display *d,void *visual,unsigned depth,int format,int offset,char *data,unsigned w,unsigned h,int pad,int stride);
int XDestroyImage(XImage *image);
int XSetForeground(Display *d,GC gc,unsigned long pixel);
int XFillRectangle(Display *d,Drawable target,GC gc,int x,int y,unsigned w,unsigned h);
int XDrawLine(Display *d,Drawable target,GC gc,int x1,int y1,int x2,int y2);
int XDrawRectangle(Display *d,Drawable target,GC gc,int x,int y,unsigned w,unsigned h);
int XPutImage(Display *d,Pixmap p,GC gc,XImage *image,int sx,int sy,int dx,int dy,unsigned w,unsigned h);
int XDrawString(Display *d,Drawable target,GC gc,int x,int y,const char *str,int count);
int XCopyArea(Display *d,Pixmap from,Window to,GC gc,int x,int y,unsigned w,unsigned h,int dx,int dy);
int XMapWindow(Display *d,Window win);
int XFlush(Display *d);
int XSync(Display *d,int discard);
int XGrabPointer(Display *d,Window w,int owner,unsigned events,int pointer_mode,int keyboard_mode,Window confine,unsigned long cursor,unsigned long time);
int XUngrabPointer(Display *d,unsigned long time);
int XWarpPointer(Display *d,Window src,Window dest,int sx,int sy,unsigned sw,unsigned sh,int dx,int dy);
int XPending(Display *d);
int XNextEvent(Display *d,XEvent *event);
KeySym XLookupKeysym(const void *key_event,int index);
const char *XKeysymToString(KeySym sym);
#endif
