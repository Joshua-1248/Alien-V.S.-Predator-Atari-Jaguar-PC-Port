/* Isolated D-Bus session mock used with test_pc_idle_inhibit.
 * Only needed by the Linux host test, never linked into the game. */
#include <stdio.h>
#include <stdint.h>
#include <time.h>

typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
extern DBusConnection *dbus_bus_get_private(int, void *);
extern int dbus_bus_request_name(DBusConnection *, const char *, unsigned, void *);
extern int dbus_connection_read_write(DBusConnection *, int);
extern DBusMessage *dbus_connection_pop_message(DBusConnection *);
extern int dbus_message_is_method_call(DBusMessage *, const char *, const char *);
extern DBusMessage *dbus_message_new_method_return(DBusMessage *);
extern int dbus_message_append_args(DBusMessage *, int, ...);
extern int dbus_message_get_args(DBusMessage *, void *, int, ...);
extern int dbus_connection_send(DBusConnection *, DBusMessage *, uint32_t *);
extern void dbus_connection_flush(DBusConnection *);
extern void dbus_message_unref(DBusMessage *);
extern void dbus_connection_close(DBusConnection *);
extern void dbus_connection_unref(DBusConnection *);

int main(void)
{
    DBusConnection *bus = dbus_bus_get_private(0, NULL);
    unsigned in = 0, out = 0, remaining = 50;
    if (!bus) return 1;
    if (dbus_bus_request_name(bus, "org.freedesktop.ScreenSaver", 0, NULL) != 1)
        return 2;
    puts("READY");
    fflush(stdout);
    while (remaining-- && (in < 2 || out < 2)) {
        DBusMessage *m;
        if (!dbus_connection_read_write(bus, 100)) break;
        while ((m = dbus_connection_pop_message(bus)) != NULL) {
            int inh = dbus_message_is_method_call(m, "org.freedesktop.ScreenSaver", "Inhibit");
            int uninh = dbus_message_is_method_call(m, "org.freedesktop.ScreenSaver", "UnInhibit");
            if (inh || uninh) {
                uint32_t value = inh ? 4242 : 0;
                DBusMessage *reply;
                if (inh) ++in;
                else if (dbus_message_get_args(m, NULL, 'u', &value, 0) && value == 4242) ++out;
                reply = dbus_message_new_method_return(m);
                if (reply) {
                    if (inh) dbus_message_append_args(reply, 'u', &value, 0);
                    dbus_connection_send(bus, reply, NULL);
                    dbus_connection_flush(bus);
                    dbus_message_unref(reply);
                }
            }
            dbus_message_unref(m);
        }
    }
    dbus_connection_close(bus);
    dbus_connection_unref(bus);
    printf("D-Bus idle inhibition: %u Inhibit / %u UnInhibit\n", in, out);
    return (in == 2 && out == 2) ? 0 : 3;
}
