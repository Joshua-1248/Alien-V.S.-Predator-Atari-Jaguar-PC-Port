/* Linux PC window/screensaver integration (not part of original Jaguar code).
 *
 * An actively focused game inhibits session idle locking/screensavers through
 * org.freedesktop.ScreenSaver (GNOME SessionManager as fallback), plus the X11
 * ScreenSaver extension. The Xlib reset is a fallback for older X11 desktops.
 * Never forge player input, reset timers while unfocused, or change system idle
 * preferences globally. Each acquisition has a matching release.
 *
 * Runtime-loaded libdbus-1 and libXss avoid mandatory -dev dependencies and
 * allow the original game to run when either API is unavailable.
 */
#include "pc_idle_inhibit.h"
#ifndef _WIN32
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/* libdbus types and ABI constants from dbus/dbus-shared.h / dbus-protocol.h.
 * Only opaque types and the public C ABI are used; no private structs.
 */
typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
#define AVP_DBUS_SESSION 0
#define AVP_DBUS_STRING 's'
#define AVP_DBUS_UINT32 'u'
#define AVP_DBUS_INVALID 0
#define AVP_DBUS_TIMEOUT_MS 700
#define AVP_IDLE_RESET_NS UINT64_C(20000000000)
#define AVP_IDLE_RETRY_NS UINT64_C(30000000000)

typedef struct AvpDbusApi {
    DBusConnection *(*bus_get_private)(int, void *);
    int (*bus_name_has_owner)(DBusConnection *, const char *, void *);
    DBusMessage *(*message_new_method_call)(const char *, const char *, const char *, const char *);
    int (*message_append_args)(DBusMessage *, int, ...);
    DBusMessage *(*connection_send_with_reply_and_block)(DBusConnection *, DBusMessage *, int, void *);
    int (*message_get_args)(DBusMessage *, void *, int, ...);
    void (*message_unref)(DBusMessage *);
    void (*connection_set_exit_on_disconnect)(DBusConnection *, int);
    int (*connection_get_is_connected)(DBusConnection *);
    void (*connection_close)(DBusConnection *);
    void (*connection_unref)(DBusConnection *);
} AvpDbusApi;

struct AvpPcIdleInhibit {
    Display *display;
    int active;
    void *xss_library;
    void (*xss_suspend)(Display *, Bool);
    int xss_held;
    void *dbus_library;
    AvpDbusApi dbus;
    DBusConnection *bus;
    uint32_t fdo_cookie, gnome_cookie;
    int fdo_held, gnome_held;
    uint64_t last_reset_ns, last_retry_ns;
};

static uint64_t avp_idle_now(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static void avp_idle_dbus_close(AvpPcIdleInhibit *s)
{
    if (s->bus) {
        s->dbus.connection_close(s->bus);
        s->dbus.connection_unref(s->bus);
        s->bus = NULL;
    }
    s->fdo_held = 0;
    s->gnome_held = 0;
}

static void avp_idle_dbus_load(AvpPcIdleInhibit *s)
{
    void *lib = dlopen("libdbus-1.so.3", RTLD_NOW | RTLD_LOCAL);
    AvpDbusApi a = {0};
    if (!lib) return;
#define AVP_LOAD_DBUS(member, symbol) do { \
    *(void **)(&a.member) = dlsym(lib, symbol); \
    if (!a.member) { dlclose(lib); return; } \
} while (0)
    AVP_LOAD_DBUS(bus_get_private, "dbus_bus_get_private");
    AVP_LOAD_DBUS(bus_name_has_owner, "dbus_bus_name_has_owner");
    AVP_LOAD_DBUS(message_new_method_call, "dbus_message_new_method_call");
    AVP_LOAD_DBUS(message_append_args, "dbus_message_append_args");
    AVP_LOAD_DBUS(connection_send_with_reply_and_block, "dbus_connection_send_with_reply_and_block");
    AVP_LOAD_DBUS(message_get_args, "dbus_message_get_args");
    AVP_LOAD_DBUS(message_unref, "dbus_message_unref");
    AVP_LOAD_DBUS(connection_set_exit_on_disconnect, "dbus_connection_set_exit_on_disconnect");
    AVP_LOAD_DBUS(connection_get_is_connected, "dbus_connection_get_is_connected");
    AVP_LOAD_DBUS(connection_close, "dbus_connection_close");
    AVP_LOAD_DBUS(connection_unref, "dbus_connection_unref");
#undef AVP_LOAD_DBUS
    s->dbus = a;
    s->dbus_library = lib;
}

static void avp_idle_dbus_connect(AvpPcIdleInhibit *s)
{
    if (!s->dbus_library || s->bus) return;
    s->bus = s->dbus.bus_get_private(AVP_DBUS_SESSION, NULL);
    if (s->bus) s->dbus.connection_set_exit_on_disconnect(s->bus, 0);
}

static DBusMessage *avp_idle_dbus_send(AvpPcIdleInhibit *s,
    DBusMessage *message)
{
    DBusMessage *reply;
    if (!message) return NULL;
    reply = s->dbus.connection_send_with_reply_and_block(
        s->bus, message, AVP_DBUS_TIMEOUT_MS, NULL);
    s->dbus.message_unref(message);
    return reply;
}

static int avp_idle_dbus_inhibit(AvpPcIdleInhibit *s, int gnome, uint32_t *cookie)
{
    const char *dest = gnome ? "org.gnome.SessionManager" : "org.freedesktop.ScreenSaver";
    const char *path = gnome ? "/org/gnome/SessionManager" : "/org/freedesktop/ScreenSaver";
    const char *app = "Alien vs Predator (Atari Jaguar PC)";
    const char *reason = "Game is being played";
    uint32_t zero = 0, idle_flag = 8;
    DBusMessage *request, *reply;
    if (!s->bus || !s->dbus.bus_name_has_owner(s->bus, dest, NULL)) return 0;
    request = s->dbus.message_new_method_call(dest, path, dest, "Inhibit");
    if (!request) return 0;
    if (!(gnome ?
          s->dbus.message_append_args(request, AVP_DBUS_STRING, &app,
              AVP_DBUS_UINT32, &zero, AVP_DBUS_STRING, &reason,
              AVP_DBUS_UINT32, &idle_flag, AVP_DBUS_INVALID) :
          s->dbus.message_append_args(request, AVP_DBUS_STRING, &app,
              AVP_DBUS_STRING, &reason, AVP_DBUS_INVALID))) {
        s->dbus.message_unref(request);
        return 0;
    }
    reply = avp_idle_dbus_send(s, request);
    if (!reply) return 0;
    {
        int ok = s->dbus.message_get_args(reply, NULL, AVP_DBUS_UINT32,
                                          cookie, AVP_DBUS_INVALID);
        s->dbus.message_unref(reply);
        return ok != 0;
    }
}

static void avp_idle_dbus_uninhibit(AvpPcIdleInhibit *s, int gnome,
                                    uint32_t cookie)
{
    const char *dest = gnome ? "org.gnome.SessionManager" : "org.freedesktop.ScreenSaver";
    const char *path = gnome ? "/org/gnome/SessionManager" : "/org/freedesktop/ScreenSaver";
    const char *method = gnome ? "Uninhibit" : "UnInhibit";
    DBusMessage *request, *reply;
    if (!s->bus || !s->dbus.connection_get_is_connected(s->bus)) return;
    request = s->dbus.message_new_method_call(dest, path, dest, method);
    if (!request) return;
    if (!s->dbus.message_append_args(request, AVP_DBUS_UINT32, &cookie,
                                      AVP_DBUS_INVALID)) {
        s->dbus.message_unref(request);
        return;
    }
    reply = avp_idle_dbus_send(s, request);
    if (reply) s->dbus.message_unref(reply);
}

static void avp_idle_dbus_release(AvpPcIdleInhibit *s)
{
    if (s->fdo_held) avp_idle_dbus_uninhibit(s, 0, s->fdo_cookie);
    if (s->gnome_held) avp_idle_dbus_uninhibit(s, 1, s->gnome_cookie);
    s->fdo_held = 0;
    s->gnome_held = 0;
    avp_idle_dbus_close(s); /* disconnect always releases service-owned cookies */
}

static void avp_idle_dbus_acquire(AvpPcIdleInhibit *s)
{
    if (!s->dbus_library) return;
    if (s->bus && !s->dbus.connection_get_is_connected(s->bus))
        avp_idle_dbus_close(s);
    avp_idle_dbus_connect(s);
    if (!s->bus) return;
    if (!s->fdo_held && !s->gnome_held) {
        s->fdo_held = avp_idle_dbus_inhibit(s, 0, &s->fdo_cookie);
        if (!s->fdo_held)
            s->gnome_held = avp_idle_dbus_inhibit(s, 1, &s->gnome_cookie);
    }
}

AvpPcIdleInhibit *avp_pc_idle_inhibit_create(Display *display)
{
    AvpPcIdleInhibit *s;
    int event_base = 0, error_base = 0;
    int (*query_extension)(Display *, int *, int *) = NULL;
    if (!display) return NULL;
    s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->display = display;
    s->xss_library = dlopen("libXss.so.1", RTLD_NOW | RTLD_LOCAL);
    if (s->xss_library) {
        *(void **)(&query_extension) = dlsym(s->xss_library, "XScreenSaverQueryExtension");
        *(void **)(&s->xss_suspend) = dlsym(s->xss_library, "XScreenSaverSuspend");
        if (!query_extension || !s->xss_suspend ||
            !query_extension(display, &event_base, &error_base))
            s->xss_suspend = NULL;
    }
    avp_idle_dbus_load(s);
    return s;
}

void avp_pc_idle_inhibit_set_active(AvpPcIdleInhibit *s, int active)
{
    if (!s || s->active == !!active) return;
    s->active = !!active;
    if (s->active) {
        if (s->xss_suspend && !s->xss_held) {
            s->xss_suspend(s->display, True);
            s->xss_held = 1;
        }
        XResetScreenSaver(s->display);
        XFlush(s->display);
        s->last_reset_ns = avp_idle_now();
        s->last_retry_ns = s->last_reset_ns;
        avp_idle_dbus_acquire(s);
    } else {
        avp_idle_dbus_release(s);
        if (s->xss_held) {
            s->xss_suspend(s->display, False);
            s->xss_held = 0;
            XFlush(s->display);
        }
    }
}

void avp_pc_idle_inhibit_tick(AvpPcIdleInhibit *s)
{
    uint64_t now;
    if (!s || !s->active) return;
    now = avp_idle_now();
    if (!now) return;
    if (now - s->last_reset_ns >= AVP_IDLE_RESET_NS) {
        XResetScreenSaver(s->display);
        XFlush(s->display);
        s->last_reset_ns = now;
    }
    if (now - s->last_retry_ns >= AVP_IDLE_RETRY_NS) {
        s->last_retry_ns = now;
        avp_idle_dbus_acquire(s);
    }
}

void avp_pc_idle_inhibit_destroy(AvpPcIdleInhibit *s)
{
    if (!s) return;
    avp_pc_idle_inhibit_set_active(s, 0);
    /* Release a possible idle bus connection acquired without a cookie. */
    avp_idle_dbus_close(s);
    if (s->dbus_library) dlclose(s->dbus_library);
    /* libXss installs an Xlib extension close-display hook.  The Display
     * remains open until the caller returns from this destructor, so
     * unloading libXss here would leave a dangling hook and crash inside
     * XCloseDisplay. Keep this tiny shared library loaded until exit. */
    free(s);
}
#endif
