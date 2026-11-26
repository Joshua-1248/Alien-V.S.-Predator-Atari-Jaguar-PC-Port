#ifndef AVP_PC_XSHM_COMPAT_H
#define AVP_PC_XSHM_COMPAT_H
/* R387: declaration-only compatibility for systems with the Xext runtime but
 * without libxext development headers. Independently declared stable MIT-SHM
 * Xlib ABI; a system-provided XShm.h always takes priority when available.
 * The public extension protocol is described by xextproto's MIT-SHM spec.
 * This file contains no Xorg implementation code. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shm.h>

typedef unsigned long ShmSeg;
typedef struct {
    int type;
    unsigned long serial;
    Bool send_event;
    Display *display;
    Drawable drawable;
    int major_code,minor_code;
    ShmSeg shmseg;
    unsigned long offset;
} XShmCompletionEvent;
typedef struct {
    ShmSeg shmseg;
    int shmid;
    char *shmaddr;
    Bool readOnly;
} XShmSegmentInfo;

extern Bool XShmQueryExtension(Display *);
extern int XShmGetEventBase(Display *);
extern Bool XShmAttach(Display *,XShmSegmentInfo *);
extern Bool XShmDetach(Display *,XShmSegmentInfo *);
extern Bool XShmPutImage(Display *,Drawable,GC,XImage *,int,int,int,int,
                         unsigned int,unsigned int,Bool);
extern XImage *XShmCreateImage(Display *,Visual *,unsigned int,int,char *,
                                XShmSegmentInfo *,unsigned int,unsigned int);
#endif
