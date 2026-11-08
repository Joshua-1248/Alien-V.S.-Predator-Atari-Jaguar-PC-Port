#ifndef AVP_LOCAL_MP_GAMEPADS_H
#define AVP_LOCAL_MP_GAMEPADS_H
#include "local_mp_commands.h"

/* Each co-op controller is a separate physical device: Linux /dev/input/jsN,
 * or Windows xinput:N. Player 1 continues using the original PC joystick. */
typedef struct AvpMpPadReader {
    int fd; /* Linux OS descriptor, or -1 on Windows */
    char path[AVP_MP_DEVICE_PATH];
    AvpMpControllerSample state;
} AvpMpPadReader;

int avp_mp_pad_reader_open(AvpMpPadReader *reader,const char *path);
void avp_mp_pad_reader_close(AvpMpPadReader *reader);
/* Nonblocking snapshot; returns 0 when connected, -1 when disconnected.
 * A disconnected device MUST NOT retain stale held button state. */
int avp_mp_pad_reader_poll(AvpMpPadReader *reader,AvpMpControllerSample *out);
#endif
