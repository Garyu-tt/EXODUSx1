/* Optional GDBus adapter for the upstream SteamOS Manager session contract.
 * Private reference session bus only; not an actual display/session controller.
 * Interface source: evlav/steamos-manager@a0d399defebd60db30ba38325d9cf47dbda49bee
 * steamos-manager-proxy/src/{session_management1,manager2}.rs (MIT).
 */
#include <gio/gio.h>
#include <glib-unix.h>

#define BUS_NAME "com.steampowered.SteamOSManager1"
#define OBJ_PATH "/com/steampowered/SteamOSManager1"
#define SESSION_IFACE BUS_NAME ".SessionManagement1"
#define MANAGER_IFACE BUS_NAME ".Manager2"

static const char xml[] =
    "<node>"
    "<interface name='" SESSION_IFACE "'>"
    "<method name='CleanTemporarySessions'/>"
    "<method name='SwitchToDesktopMode'/>"
    "<method name='SwitchToDesktopSession'><arg type='s' direction='in'/></method>"
    "<method name='SwitchToGameMode'/>"
    "<method name='SwitchToLoginMode'><arg type='s' direction='in'/></method>"
    "<method name='ValidDesktopSessions'><arg type='as' direction='out'/></method>"
    "<property name='DefaultDesktopSession' type='s' access='readwrite'/>"
    "<property name='DefaultLoginMode' type='s' access='readwrite'/>"
    "</interface>"
    "<interface name='" MANAGER_IFACE "'>"
    "<method name='ReloadConfig'/>"
    "<property name='DeviceModel' type='(ss)' access='read'/>"
    "</interface>"
    "</node>";

struct backend {
    const char *path;
    struct config active;
    GDBusConnection *bus;
    GMainLoop *loop;
};
static void changed(struct backend *b) {
    GVariantBuilder values, invalid;
    g_variant_builder_init(&values, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&values, "{sv}", "DefaultLoginMode",
                          g_variant_new_string(b->active.mode));
    g_variant_builder_init(&invalid, G_VARIANT_TYPE("as"));
    g_dbus_connection_emit_signal(b->bus, NULL, OBJ_PATH,
        "org.freedesktop.DBus.Properties", "PropertiesChanged",
        g_variant_new("(sa{sv}as)", SESSION_IFACE, &values, &invalid), NULL);
}
static gboolean reload_config(struct backend *b) {
    struct config candidate;
    if (!read_config(b->path, &candidate) ||
        candidate.generation <= b->active.generation) {
        report("rejected", &b->active);
        return FALSE;
    }
    b->active = candidate;
    report("reloaded", &b->active);
    changed(b);
    return TRUE;
}
static gboolean reload_signal(gpointer data) {
    reload_config(data);
    return G_SOURCE_CONTINUE;
}
static gboolean stop_signal(gpointer data) {
    struct backend *b = data;
    g_main_loop_quit(b->loop);
    return G_SOURCE_CONTINUE;
}
/* Owner-controlled directory, one writer at a time. Atomic visibility via
 * rename; not a crash-proof transaction or interprocess writer lock. */
static gboolean set_mode(struct backend *b, const char *mode, GError **error) {
    struct config disk;
    if (strcmp(mode, "game") && strcmp(mode, "desktop")) {
        g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS, "Expected game or desktop");
        return FALSE;
    }
    if (!read_config(b->path, &disk) || disk.generation != b->active.generation ||
        strcmp(disk.mode, b->active.mode)) {
        g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Configuration changed; reload first");
        return FALSE;
    }
    if (!strcmp(mode, b->active.mode)) return TRUE;
    if (disk.generation == UINT64_MAX) {
        g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Generation exhausted");
        return FALSE;
    }
    char *temp = g_strdup_printf("%s.pending.XXXXXX", b->path);
    int fd = mkstemp(temp);
    if (fd < 0) goto fail;
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); goto fail; }
    int ok = fprintf(f, "generation=%" PRIu64 "\nmode=%s\n", disk.generation+1, mode) > 0;
    if (fflush(f) || fsync(fd)) ok = 0;
    if (fclose(f)) ok = 0;
    if (!ok || rename(temp, b->path)) goto fail;
    g_free(temp);
    if (reload_config(b)) return TRUE;
    g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Published config not activated");
    return FALSE;
fail:
    if (fd >= 0) unlink(temp);
    g_free(temp);
    g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED, "Configuration publication failed");
    return FALSE;
}
static GVariant *get_property(GDBusConnection *conn, const gchar *sender,
    const gchar *object, const gchar *iface, const gchar *name, GError **error, gpointer data) {
    (void)conn; (void)sender; (void)object;
    struct backend *b = data;
    if (!strcmp(iface, SESSION_IFACE) && !strcmp(name, "DefaultLoginMode"))
        return g_variant_new_string(b->active.mode);
    g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "No verified provider");
    return NULL;
}
static gboolean set_property(GDBusConnection *conn, const gchar *sender,
    const gchar *object, const gchar *iface, const gchar *name, GVariant *value,
    GError **error, gpointer data) {
    (void)conn; (void)sender; (void)object;
    if (!strcmp(iface, SESSION_IFACE) && !strcmp(name, "DefaultLoginMode"))
        return set_mode(data, g_variant_get_string(value, NULL), error);
    g_set_error_literal(error, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "No verified provider");
    return FALSE;
}
static void method_call(GDBusConnection *conn, const gchar *sender,
    const gchar *object, const gchar *iface, const gchar *method,
    GVariant *params, GDBusMethodInvocation *invocation, gpointer data) {
    (void)conn; (void)sender; (void)object; (void)params;
    struct backend *b = data;
    if (!strcmp(iface, MANAGER_IFACE) && !strcmp(method, "ReloadConfig")) {
        if (reload_config(b)) g_dbus_method_invocation_return_value(invocation, NULL);
        else g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR,
             G_DBUS_ERROR_FAILED, "Invalid or non-newer configuration");
        return;
    }
    if (!strcmp(iface, SESSION_IFACE) && !strcmp(method, "ValidDesktopSessions")) {
        g_dbus_method_invocation_return_value(invocation,
            g_variant_new("(^as)", (const char *const[]){NULL}));
        return;
    }
    g_dbus_method_invocation_return_error_literal(invocation, G_DBUS_ERROR,
        G_DBUS_ERROR_NOT_SUPPORTED, "Actual graphical session provider not connected");
}
static const GDBusInterfaceVTable vtable = {
    .method_call = method_call, .get_property = get_property, .set_property = set_property
};
int main(int argc, char **argv) {
    if (argc != 2 || geteuid() == 0) {
        fputs("usage: unprivileged exodus-sessiond-dbus CONFIG on a private session bus\n", stderr);
        return 2;
    }
    struct backend b = {.path=argv[1]};
    if (!read_config(b.path, &b.active)) return 1;
    GError *error = NULL;
    b.bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!b.bus) goto fail;
    GDBusNodeInfo *info = g_dbus_node_info_new_for_xml(xml, &error);
    if (!info) goto fail;
    guint ids[2] = {0,0};
    for (unsigned i=0;i<2;i++) {
        ids[i] = g_dbus_connection_register_object(b.bus, OBJ_PATH,
            info->interfaces[i], &vtable, &b, NULL, &error);
        if (!ids[i]) goto fail;
    }
    GVariant *reply = g_dbus_connection_call_sync(b.bus, "org.freedesktop.DBus",
        "/org/freedesktop/DBus", "org.freedesktop.DBus", "RequestName",
        g_variant_new("(su)", BUS_NAME, 4u), G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE, 5000, NULL, &error);
    if (!reply) goto fail;
    guint result;
    g_variant_get(reply, "(u)", &result);
    g_variant_unref(reply);
    if (result != 1) { fputs("Bus name already owned\n", stderr); return 1; }
    b.loop = g_main_loop_new(NULL,FALSE);
    guint hup = g_unix_signal_add(SIGHUP, reload_signal, &b);
    guint term = g_unix_signal_add(SIGTERM, stop_signal, &b);
    report("ready", &b.active);
    g_main_loop_run(b.loop);
    g_source_remove(hup); g_source_remove(term);
    report("stopped", &b.active);
    g_main_loop_unref(b.loop);
    for (unsigned i=0;i<2;i++) g_dbus_connection_unregister_object(b.bus,ids[i]);
    g_dbus_node_info_unref(info);
    g_object_unref(b.bus);
    return 0;
fail:
    fprintf(stderr,"D-Bus startup failed: %s\n",error?error->message:"unknown");
    g_clear_error(&error);
    return 1;
}
