/* atspi-child-index.c
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#include "gtk/gtkaccessibleprivate.h"
#include "gtk/a11y/gtkatspicontextprivate.h"
#include "gtk/a11y/gtkatspirootprivate.h"

static const char registry_xml[] =
  "<node>"
  " <interface name='org.a11y.atspi.Socket'>"
  "  <method name='Embed'>"
  "   <arg type='(so)' direction='in'/>"
  "   <arg type='(so)' direction='out'/>"
  "  </method>"
  " </interface>"
  " <interface name='org.a11y.atspi.Registry'>"
  "  <method name='GetRegisteredEvents'>"
  "   <arg type='a(ss)' direction='out'/>"
  "  </method>"
  " </interface>"
  "</node>";

typedef struct
{
  GDBusConnection *connection;
  gboolean         embedded;
  gboolean         listeners_queried;
  GArray          *events;
} TestBus;

typedef struct
{
  char change;
  int  index;
} ChildEvent;

static void
registry_method_call (GDBusConnection       *connection,
                      const char            *sender,
                      const char            *path,
                      const char            *interface,
                      const char            *method,
                      GVariant              *parameters,
                      GDBusMethodInvocation *invocation,
                      gpointer               user_data)
{
  TestBus *bus = user_data;

  if (g_str_equal (method, "Embed"))
    {
      bus->embedded = TRUE;
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("((so))",
                                                            "org.a11y.atspi.Registry",
                                                            "/org/a11y/atspi/accessible/root"));
    }
  else if (g_str_equal (method, "GetRegisteredEvents"))
    {
      GVariantBuilder builder;

      bus->listeners_queried = TRUE;
      g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
      g_variant_builder_add (&builder,
                             "(ss)",
                             g_dbus_connection_get_unique_name (bus->connection),
                             "Object:ChildrenChanged");
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(ss))", &builder));
    }
  else
    {
      g_dbus_method_invocation_return_dbus_error (invocation,
                                                  "org.a11y.atspi.Error.UnknownMethod",
                                                  method);
    }
}

static const GDBusInterfaceVTable registry_vtable = {
  registry_method_call,
  NULL,
  NULL,
  { NULL }
};

static void
on_children_changed (GDBusConnection *connection,
                     const char      *sender,
                     const char      *path,
                     const char      *interface,
                     const char      *signal,
                     GVariant        *parameters,
                     gpointer         user_data)
{
  TestBus *bus = user_data;
  ChildEvent event;
  const char *change;

  g_variant_get_child (parameters, 0, "&s", &change);
  g_variant_get_child (parameters, 1, "i", &event.index);
  event.change = g_str_equal (change, "add") ? 'a' : 'r';
  g_array_append_val (bus->events, event);
}

static gboolean
wait_timeout (gpointer data)
{
  gboolean *expired = data;

  *expired = TRUE;

  return G_SOURCE_REMOVE;
}

#define WAIT_UNTIL(condition) G_STMT_START { \
  gboolean wait_expired = FALSE; \
  GSource *wait_source = g_timeout_source_new (5000); \
  \
  g_source_set_callback (wait_source, wait_timeout, &wait_expired, NULL); \
  g_source_attach (wait_source, NULL); \
  \
  while (!(condition) && !wait_expired) \
    g_main_context_iteration (NULL, TRUE); \
  \
  g_source_destroy (wait_source); \
  g_source_unref (wait_source); \
  \
  g_assert_true (condition); \
} G_STMT_END

typedef struct
{
  gboolean  done;
  GVariant *reply;
  GError   *error;
} AsyncCall;

static void
on_call_complete (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  AsyncCall *call = user_data;

  call->reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
                                               result,
                                               &call->error);
  call->done = TRUE;
}

static GVariant *
call_application (TestBus    *bus,
                  const char *destination,
                  const char *path,
                  const char *interface,
                  const char *method)
{
  AsyncCall call = { 0 };

  g_dbus_connection_call (bus->connection,
                          destination,
                          path,
                          interface,
                          method,
                          g_variant_new ("()"),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          5000,
                          NULL,
                          on_call_complete,
                          &call);
  WAIT_UNTIL (call.done);
  g_assert_no_error (call.error);
  return call.reply;
}

static void
check_remote_index (TestBus   *bus,
                    GtkWidget *widget,
                    int        expected)
{
  GtkATContext *context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (widget));
  GVariant *reference;
  GVariant *reply;
  const char *destination;
  const char *path;
  int index;

  g_assert_true (GTK_IS_AT_SPI_CONTEXT (context));
  gtk_at_context_realize (context);
  reference = gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (context));
  g_variant_get (reference, "(&s&o)", &destination, &path);
  reply = call_application (bus,
                            destination,
                            path,
                            "org.a11y.atspi.Accessible",
                            "GetIndexInParent");
  g_variant_get (reply, "(i)", &index);
  g_assert_cmpint (index, ==, expected);
  g_variant_unref (reply);
  g_variant_unref (reference);
  g_object_unref (context);
}

static void
check_cache_and_order (TestBus   *bus,
                       GtkWidget *parent,
                       GtkWidget *first,
                       GtkWidget *second)
{
  GtkATContext *parent_context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (parent));
  GtkATContext *first_context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (first));
  GtkATContext *second_context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (second));
  GVariant *parent_ref = gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (parent_context));
  GVariant *first_ref = gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (first_context));
  GVariant *second_ref = gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (second_context));
  GVariant *reply;
  GVariant *items;
  const char *destination;
  const char *ignored_destination;
  const char *parent_path;
  const char *first_path;
  const char *second_path;
  gboolean found_first = FALSE;
  gboolean found_second = FALSE;

  g_variant_get (parent_ref, "(&s&o)", &destination, &parent_path);
  g_variant_get (first_ref, "(&s&o)", &ignored_destination, &first_path);
  g_variant_get (second_ref, "(&s&o)", &ignored_destination, &second_path);

  reply = call_application (bus,
                            destination,
                            parent_path,
                            "org.a11y.atspi.Accessible",
                            "GetChildren");
  items = g_variant_get_child_value (reply, 0);
  g_assert_cmpuint (g_variant_n_children (items), ==, 2);
  for (int i = 0; i < 2; i++)
    {
      GVariant *reference = g_variant_get_child_value (items, i);
      const char *path;

      g_variant_get (reference, "(&s&o)", &ignored_destination, &path);
      g_assert_cmpstr (path, ==, i == 0 ? first_path : second_path);
      g_variant_unref (reference);
    }
  g_variant_unref (items);
  g_variant_unref (reply);

  reply = call_application (bus,
                            destination,
                            "/org/a11y/atspi/cache",
                            "org.a11y.atspi.Cache",
                            "GetItems");
  items = g_variant_get_child_value (reply, 0);
  for (gsize i = 0; i < g_variant_n_children (items); i++)
    {
      GVariant *item = g_variant_get_child_value (items, i);
      GVariant *reference = g_variant_get_child_value (item, 0);
      const char *path;
      int index;

      g_variant_get (reference, "(&s&o)", &ignored_destination, &path);
      g_variant_get_child (item, 3, "i", &index);
      if (g_str_equal (path, first_path))
        {
          found_first = TRUE;
          g_assert_cmpint (index, ==, 0);
        }

      if (g_str_equal (path, second_path))
        {
          found_second = TRUE;
          g_assert_cmpint (index, ==, 1);
        }
      g_variant_unref (reference);
      g_variant_unref (item);
    }
  g_assert_true (found_first);
  g_assert_true (found_second);
  g_variant_unref (items);
  g_variant_unref (reply);
  g_variant_unref (parent_ref);
  g_variant_unref (first_ref);
  g_variant_unref (second_ref);
  g_object_unref (parent_context);
  g_object_unref (first_context);
  g_object_unref (second_context);
}

static void
test_connected_index (gconstpointer data)
{
  GdkDisplay *display = gdk_display_get_default ();
  GTestDBus *private_bus = (GTestDBus *) data;
  GDBusNodeInfo *node_info;
  GDBusConnection *connection;
  GVariant *reply;
  GError *error = NULL;
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *children[3];
  GtkATContext *context;
  GtkAtSpiRoot *root;
  guint subscription;
  TestBus bus = { 0 };
  gboolean supported_display = FALSE;

#ifdef GDK_WINDOWING_X11
  supported_display |= GDK_IS_X11_DISPLAY (display);
#endif
#ifdef GDK_WINDOWING_WAYLAND
  supported_display |= GDK_IS_WAYLAND_DISPLAY (display);
#endif

  if (!supported_display)
    {
      g_test_skip ("AT-SPI requires an X11 or Wayland display");
      return;
    }

  connection = g_dbus_connection_new_for_address_sync (g_test_dbus_get_bus_address (private_bus),
                                                       G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                                       NULL,
                                                       NULL,
                                                       &error);
  g_assert_no_error (error);
  bus.connection = connection;
  bus.events = g_array_new (FALSE, FALSE, sizeof (ChildEvent));

  node_info = g_dbus_node_info_new_for_xml (registry_xml, &error);
  g_assert_no_error (error);
  g_dbus_connection_register_object (connection, "/org/a11y/atspi/accessible/root",
                                     node_info->interfaces[0], &registry_vtable,
                                     &bus, NULL, &error);
  g_assert_no_error (error);
  g_dbus_connection_register_object (connection, "/org/a11y/atspi/registry",
                                     node_info->interfaces[1], &registry_vtable,
                                     &bus, NULL, &error);
  g_assert_no_error (error);

  reply = g_dbus_connection_call_sync (connection,
                                       "org.freedesktop.DBus",
                                       "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus",
                                       "RequestName",
                                       g_variant_new ("(su)", "org.a11y.atspi.Registry", 0),
                                       G_VARIANT_TYPE ("(u)"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       5000,
                                       NULL,
                                       &error);
  g_assert_no_error (error);
  g_variant_unref (reply);

  window = gtk_window_new ();
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (window), box);
  gtk_window_present (GTK_WINDOW (window));

  context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (box));
  g_assert_true (GTK_IS_AT_SPI_CONTEXT (context));
  gtk_at_context_realize (context);
  root = gtk_at_spi_context_get_root (GTK_AT_SPI_CONTEXT (context));
  WAIT_UNTIL (bus.embedded);
  WAIT_UNTIL (bus.listeners_queried);
  WAIT_UNTIL (gtk_at_spi_root_has_event_listeners (root));

  subscription = g_dbus_connection_signal_subscribe (connection,
                                                     NULL,
                                                     "org.a11y.atspi.Event.Object",
                                                     "ChildrenChanged",
                                                     gtk_at_spi_context_get_context_path (GTK_AT_SPI_CONTEXT (context)),
                                                     NULL,
                                                     G_DBUS_SIGNAL_FLAGS_NONE,
                                                     on_children_changed,
                                                     &bus,
                                                     NULL);

  for (int i = 0; i < 3; i++)
    {
      children[i] = gtk_label_new ("Child");
      gtk_widget_set_visible (children[i], TRUE);
      gtk_box_append (GTK_BOX (box), children[i]);
      check_remote_index (&bus, children[i], i);
    }

  WAIT_UNTIL (bus.events->len >= 3);

  for (int i = 0; i < 3; i++)
    {
      ChildEvent *event = &g_array_index (bus.events, ChildEvent, i);

      g_assert_cmpint (event->change, ==, 'a');
      g_assert_cmpint (event->index, ==, i);
    }

  gtk_widget_unparent (children[1]);

  WAIT_UNTIL (bus.events->len >= 4);

  g_assert_cmpint (g_array_index (bus.events, ChildEvent, 3).change, ==, 'r');
  g_assert_cmpint (g_array_index (bus.events, ChildEvent, 3).index, ==, 1);
  check_remote_index (&bus, children[2], 1);
  check_cache_and_order (&bus, box, children[0], children[2]);

  g_dbus_connection_signal_unsubscribe (connection, subscription);
  gtk_window_destroy (GTK_WINDOW (window));
  g_object_unref (context);
  g_array_unref (bus.events);
  g_dbus_node_info_unref (node_info);
  g_object_unref (connection);
}

int
main (int argc, char *argv[])
{
  GTestDBus *private_bus;
  char *display;
  char *runtime_dir;
  int result;

  display = g_strdup (g_getenv ("DISPLAY"));
  runtime_dir = g_strdup (g_getenv ("XDG_RUNTIME_DIR"));

  private_bus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (private_bus);

  if (display != NULL)
    g_setenv ("DISPLAY", display, TRUE);
  if (runtime_dir != NULL)
    g_setenv ("XDG_RUNTIME_DIR", runtime_dir, TRUE);

  g_free (display);
  g_free (runtime_dir);

  g_setenv ("AT_SPI_BUS_ADDRESS", g_test_dbus_get_bus_address (private_bus), TRUE);
  g_setenv ("GTK_A11Y", "atspi", TRUE);
  gtk_test_init (&argc, &argv, NULL);
  g_test_add_data_func ("/a11y/atspi-child-index", private_bus, test_connected_index);
  result = g_test_run ();

  g_test_dbus_down (private_bus);
  g_object_unref (private_bus);

  return result;
}
