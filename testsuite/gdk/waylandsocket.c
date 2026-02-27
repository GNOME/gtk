#include "config.h"

#include <gtk/gtk.h>
#include <locale.h>
#include <wayland-client.h>

#include <gdk/wayland/gdkwaylanddisplay.h>

static gboolean saw_mutter_x11_interop = FALSE;
static gboolean saw_gst_init = FALSE;

void gst_init (int *argc, char **argv);

void
gst_init (int *argc, char **argv)
{
  struct wl_display *wl_display;
  wl_display = wl_display_connect (NULL);
  wl_display_disconnect (wl_display);

  saw_gst_init = TRUE;
}

static void
handle_registry_global (void               *user_data,
                        struct wl_registry *registry,
                        uint32_t            id,
                        const char         *interface,
                        uint32_t            version)
{
  if (strcmp (interface, "mutter_x11_interop") == 0)
    saw_mutter_x11_interop = TRUE;
}

static void
handle_registry_global_remove (void               *user_data,
                               struct wl_registry *registry,
                               uint32_t            name)
{
}

static const struct wl_registry_listener registry_listener = {
  handle_registry_global,
  handle_registry_global_remove
};

static void
test_wayland_connect_to_socket (void)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  g_autoptr(GError) error = NULL;
  int fd_idx;
  int fd;
  g_autofree char *fd_string = NULL;
  GdkDisplay *display;
  struct wl_display *wl_display;
  struct wl_registry *wl_registry;

  g_assert_false (gtk_is_initialized ());

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (connection);

  variant =
    g_dbus_connection_call_with_unix_fd_list_sync (connection,
                                                   "org.gnome.Mutter.ServiceChannel",
                                                   "/org/gnome/Mutter/ServiceChannel",
                                                   "org.gnome.Mutter.ServiceChannel",
                                                   "OpenWaylandServiceConnection",
                                                   g_variant_new ("(u)", 1),
                                                   G_VARIANT_TYPE ("(h)"),
                                                   G_DBUS_CALL_FLAGS_NONE,
                                                   -1,
                                                   NULL,
                                                   &fd_list,
                                                   NULL,
                                                   &error);
  g_assert_no_error (error);
  g_assert_nonnull (variant);

  g_variant_get (variant, "(h)", &fd_idx);
  fd = g_unix_fd_list_get (fd_list, fd_idx, NULL);
  g_assert_cmpint (fd, >=, 0);

  fd_string = g_strdup_printf ("%d", fd);
  setenv ("WAYLAND_SOCKET", fd_string, 1);

  g_assert_true (gtk_init_check ());
  display = gdk_display_get_default ();
  g_assert_nonnull (display);
  g_assert_true (saw_gst_init);

  g_assert_true (GDK_IS_WAYLAND_DISPLAY (display));

  wl_display = gdk_wayland_display_get_wl_display (display);
  g_assert_nonnull (wl_display);
  wl_registry = wl_display_get_registry (wl_display);
  wl_registry_add_listener (wl_registry, &registry_listener, NULL);
  wl_display_roundtrip (wl_display);

  g_assert_true (saw_mutter_x11_interop);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_setenv ("GTK_MODULES", "", TRUE);
  gtk_disable_setlocale();
  setlocale (LC_ALL, "C");

  g_test_add_func ("/wayland/connect-to-socket/basic", test_wayland_connect_to_socket);

  return g_test_run ();
}
