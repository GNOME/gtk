/* gtkatspicontext.c: AT-SPI GtkATContext implementation
 *
 * Copyright 2020  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkatspicontextprivate.h"

#include "gtkatspicacheprivate.h"
#include "gtkatspirootprivate.h"
#include "gtkdebug.h"
#include "gtkwindow.h"

#include <gio/gio.h>

#if defined(GDK_WINDOWING_WAYLAND)
# include <gdk/wayland/gdkwaylanddisplay.h>
#endif
#if defined(GDK_WINDOWING_X11)
# include <gdk/x11/gdkx11display.h>
# include <gdk/x11/gdkx11property.h>
#endif

struct _GtkAtSpiContext
{
  GtkATContext parent_instance;

  /* The root object, used as a entry point */
  GtkAtSpiRoot *root;

  /* The cache object, used to retrieve ATContexts */
  GtkAtSpiCache *cache;

  /* The address for the ATSPI accessibility bus */
  char *bus_address;

  /* The object path of the ATContext on the bus */
  char *context_path;

  /* Just a pointer; the connection is owned by the GtkAtSpiRoot
   * associated to the GtkATContext
   */
  GDBusConnection *connection;
};

enum
{
  PROP_BUS_ADDRESS = 1,

  N_PROPS
};

static GParamSpec *obj_props[N_PROPS];

G_DEFINE_TYPE (GtkAtSpiContext, gtk_at_spi_context, GTK_TYPE_AT_CONTEXT)

static void
gtk_at_spi_context_state_change (GtkATContext                *self,
                                 GtkAccessibleStateChange     changed_states,
                                 GtkAccessiblePropertyChange  changed_properties,
                                 GtkAccessibleRelationChange  changed_relations,
                                 GtkAccessibleAttributeSet   *states,
                                 GtkAccessibleAttributeSet   *properties,
                                 GtkAccessibleAttributeSet   *relations)
{
}

static void
gtk_at_spi_context_finalize (GObject *gobject)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (gobject);

  g_free (self->bus_address);
  g_free (self->context_path);

  G_OBJECT_CLASS (gtk_at_spi_context_parent_class)->finalize (gobject);
}

static void
gtk_at_spi_context_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (gobject);

  switch (prop_id)
    {
    case PROP_BUS_ADDRESS:
      self->bus_address = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_at_spi_context_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (gobject);

  switch (prop_id)
    {
    case PROP_BUS_ADDRESS:
      g_value_set_string (value, self->bus_address);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_at_spi_context_constructed (GObject *gobject)
{
  GtkAtSpiContext *self = GTK_AT_SPI_CONTEXT (gobject);
  GdkDisplay *display;

  g_assert (self->bus_address);

  /* Every GTK application has a single root AT-SPI object, which
   * handles all the global state, including the cache of accessible
   * objects. We use the GdkDisplay to store it, so it's guaranteed
   * to be unique per-display connection
   */
  display = gtk_at_context_get_display (GTK_AT_CONTEXT (self));
  self->root =
    g_object_get_data (G_OBJECT (display), "-gtk-atspi-root");

  if (self->root == NULL)
    {
      self->root = gtk_at_spi_root_new (self->bus_address);
      self->connection = gtk_at_spi_root_get_connection (self->root);

      g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-root",
                              self->root,
                              g_object_unref);
    }

  /* We use the application's object path to build the path of each
   * accessible object exposed on the accessibility bus; the path is
   * also used to access the object cache
   */
  GApplication *application = g_application_get_default ();
  char *base_path = NULL;

  if (application != NULL)
    {
      const char *app_path = g_application_get_dbus_object_path (application);
      base_path = g_strconcat (app_path, "/a11y", NULL);
    }
  else
    {
      char *uuid = g_uuid_string_random ();
      base_path = g_strconcat ("/org/gtk/application/", uuid, "/a11y", NULL);
      g_free (uuid);
    }

  /* We use a unique id to ensure that we don't have conflicting
   * objects on the bus
   */
  char *uuid = g_uuid_string_random ();

  self->context_path = g_strconcat (base_path, "/", uuid, NULL);

  GTK_NOTE (A11Y, g_message ("ATSPI context path: %s", self->context_path));

  g_free (base_path);
  g_free (uuid);

  G_OBJECT_CLASS (gtk_at_spi_context_parent_class)->constructed (gobject);
}

static void
gtk_at_spi_context_class_init (GtkAtSpiContextClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkATContextClass *context_class = GTK_AT_CONTEXT_CLASS (klass);

  gobject_class->constructed = gtk_at_spi_context_constructed;
  gobject_class->set_property = gtk_at_spi_context_set_property;
  gobject_class->get_property = gtk_at_spi_context_get_property;
  gobject_class->finalize = gtk_at_spi_context_finalize;

  context_class->state_change = gtk_at_spi_context_state_change;

  obj_props[PROP_BUS_ADDRESS] =
    g_param_spec_string ("bus-address", NULL, NULL,
                         NULL,
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, obj_props);
}

static void
gtk_at_spi_context_init (GtkAtSpiContext *self)
{
}

#ifdef GDK_WINDOWING_X11
static char *
get_bus_address_x11 (GdkDisplay *display)
{
  GTK_NOTE (A11Y, g_message ("Acquiring a11y bus via X11..."));

  Display *xdisplay = gdk_x11_display_get_xdisplay (display);
  Atom type_return;
  int format_return;
  gulong nitems_return;
  gulong bytes_after_return;
  guchar *data = NULL;
  char *address = NULL;

  gdk_x11_display_error_trap_push (display);
  XGetWindowProperty (xdisplay, DefaultRootWindow (xdisplay),
                      gdk_x11_get_xatom_by_name_for_display (display, "AT_SPI_BUS"),
                      0L, BUFSIZ, False,
                      (Atom) 31,
                      &type_return, &format_return, &nitems_return,
                      &bytes_after_return, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  address = g_strdup ((char *) data);

  XFree (data);

  return address;
}
#endif

#if defined(GDK_WINDOWING_WAYLAND) || defined(GDK_WINDOWING_X11)
static char *
get_bus_address_dbus (GdkDisplay *display)
{
  GTK_NOTE (A11Y, g_message ("Acquiring a11y bus via DBus..."));

  GError *error = NULL;
  GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

  if (error != NULL)
    {
      g_critical ("Unable to acquire session bus: %s", error->message);
      g_error_free (error);
      return NULL;
    }

  GVariant *res =
    g_dbus_connection_call_sync (connection, "org.a11y.Bus",
                                  "/org/a11y/bus",
                                  "org.a11y.Bus",
                                  "GetAddress",
                                  NULL, NULL,
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  &error);
  if (error != NULL)
    {
      g_critical ("Unable to acquire the address of the accessibility bus: %s",
                  error->message);
      g_error_free (error);
    }

  char *address = NULL;
  if (res != NULL)
    {
      g_variant_get (res, "(s)", &address);
      g_variant_unref (res);
    }

  g_object_unref (connection);

  return address;
}
#endif

static const char *
get_bus_address (GdkDisplay *display)
{
  const char *bus_address;

  bus_address = g_object_get_data (G_OBJECT (display), "-gtk-atspi-bus-address");
  if (bus_address != NULL)
    return bus_address;

  /* The bus address environment variable takes precedence; this is the
   * mechanism used by Flatpak to handle the accessibility bus portal
   * between the sandbox and the outside world
   */
  bus_address = g_getenv ("AT_SPI_BUS_ADDRESS");
  if (bus_address != NULL && *bus_address != '\0')
    {
      GTK_NOTE (A11Y, g_message ("Using ATSPI bus address from environment"));
      g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-bus-address",
                              g_strdup (bus_address),
                              g_free);
      goto out;
    }

#if defined(GDK_WINDOWING_WAYLAND)
  if (bus_address == NULL)
    {
      if (GDK_IS_WAYLAND_DISPLAY (display))
        {
          char *addr = get_bus_address_dbus (display);

          g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-bus-address",
                                  addr,
                                  g_free);

          bus_address = addr;
        }
    }
#endif
#if defined(GDK_WINDOWING_X11)
  if (bus_address == NULL)
    {
      if (GDK_IS_X11_DISPLAY (display))
        {
          char *addr = get_bus_address_dbus (display);

          if (addr == NULL)
            addr = get_bus_address_x11 (display);

          g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-bus-address",
                                  addr,
                                  g_free);

          bus_address = addr;
        }
    }
#endif

out:
  return bus_address;
}

GtkATContext *
gtk_at_spi_create_context (GtkAccessibleRole  accessible_role,
                           GtkAccessible     *accessible,
                           GdkDisplay        *display)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), NULL);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  const char *bus_address = get_bus_address (display);

  if (bus_address == NULL)
    return NULL;

#if defined(GDK_WINDOWING_WAYLAND)
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return g_object_new (GTK_TYPE_AT_SPI_CONTEXT,
                         "accessible-role", accessible_role,
                         "accessible", accessible,
                         "display", display,
                         "bus-address", bus_address,
                         NULL);
#endif
#if defined(GDK_WINDOWING_X11)
  if (GDK_IS_X11_DISPLAY (display))
    return g_object_new (GTK_TYPE_AT_SPI_CONTEXT,
                         "accessible-role", accessible_role,
                         "accessible", accessible,
                         "display", display,
                         "bus-address", bus_address,
                         NULL);
#endif

  return NULL;
}
