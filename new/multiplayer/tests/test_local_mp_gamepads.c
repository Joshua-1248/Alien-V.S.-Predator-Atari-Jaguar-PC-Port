/* Linux joystick reader: original JS event format, held-state persistence,
 * disconnect clearing. A nonblocking pipe is a test-only event source. */
#include "local_mp_gamepads.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
static int send_event(int fd,unsigned char type,unsigned char number,short value)
{
    struct js_event e;
    memset(&e,0,sizeof(e));
    e.type=type;e.number=number;e.value=value;
    return write(fd,&e,sizeof(e))==(ssize_t)sizeof(e);
}
#endif
int main(void)
{
    AvpMpPadReader r;
    memset(&r,0,sizeof(r));r.fd=-1;
    CHECK(!avp_mp_pad_reader_open(&r,""));
#ifndef _WIN32
    {
        AvpMpControllerSample out;
        int fds[2],flags;
        CHECK(!avp_mp_pad_reader_open(&r,"/dev/null")); /* Not a joystick. */
        CHECK(pipe(fds)==0);
        flags=fcntl(fds[0],F_GETFL,0);
        CHECK(flags>=0 && fcntl(fds[0],F_SETFL,flags|O_NONBLOCK)==0);
        r.fd=fds[0];
        strcpy(r.path,"fixture-js");
        strcpy(r.state.path,"fixture-js");
        r.state.axis_count=4;
        CHECK(send_event(fds[1],JS_EVENT_AXIS|JS_EVENT_INIT,0,-25000));
        CHECK(send_event(fds[1],JS_EVENT_BUTTON|JS_EVENT_INIT,5,1));
        CHECK(avp_mp_pad_reader_poll(&r,&out)==0);
        CHECK(!strcmp(out.path,"fixture-js") && out.axis_count==4);
        CHECK(out.axes[0]==-25000 && out.buttons==(1u<<5));
        CHECK(avp_mp_pad_reader_poll(&r,&out)==0); /* Held across polls. */
        CHECK(out.axes[0]==-25000 && out.buttons==(1u<<5));
        CHECK(send_event(fds[1],JS_EVENT_BUTTON,5,0));
        CHECK(send_event(fds[1],JS_EVENT_AXIS,0,0));
        CHECK(avp_mp_pad_reader_poll(&r,&out)==0);
        CHECK(out.axes[0]==0 && out.buttons==0);
        CHECK(send_event(fds[1],JS_EVENT_BUTTON,3,1));
        CHECK(avp_mp_pad_reader_poll(&r,&out)==0 && (out.buttons&(1u<<3)));
        CHECK(close(fds[1])==0);
        CHECK(avp_mp_pad_reader_poll(&r,&out)==-1);
        CHECK(!r.path[0] && r.fd==-1 && out.buttons==0 && !out.path[0]);
    }
#else
    CHECK(!avp_mp_pad_reader_open(&r,"xinput:99"));
#endif
    avp_mp_pad_reader_close(&r);
    puts("PASS: isolated gamepad reader, events, held state, releases, and disconnect clearing");
    return 0;
}
