#ifndef AVP_NEW_LOCAL_MP_MICE_H
#define AVP_NEW_LOCAL_MP_MICE_H
#include "local_mp_input.h"
#include <stddef.h>

#define AVP_MP_DISCOVER_MICE 32u
typedef struct AvpMpPhysicalMouse {
    char path[AVP_MP_DEVICE_PATH];
    char label[128];
    int readable; /* Linux evdev requires permission; 0=no raw input access. */
} AvpMpPhysicalMouse;

/* Device-specific raw-input discovery; independent of X11's merged cursor.
 * Enumerating devices DOES NOT install hooks or claim Player 1's mouse. */
unsigned avp_mp_mice_discover(AvpMpPhysicalMouse out[AVP_MP_DISCOVER_MICE]);

/* Linux evdev: read from a selected physical event device. Windows returns
 * -1 until Raw Input WM_INPUT forwarding has been connected to the host. */
typedef struct AvpMpMouseReader { int fd; char path[AVP_MP_DEVICE_PATH]; } AvpMpMouseReader;
int avp_mp_mouse_reader_open(AvpMpMouseReader *reader,const char *path);
void avp_mp_mouse_reader_close(AvpMpMouseReader *reader);
/* Pump a single owned device, routing only its own events into its profile.
 * Negative return denotes unavailable/disconnected device. */
int avp_mp_mouse_reader_poll(AvpMpMouseReader *reader,const AvpMpInputConfig *cfg,
                            AvpMpMouseState states[AVP_MP_INPUT_PLAYERS]);
#endif
