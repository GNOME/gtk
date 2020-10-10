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
#include "gtkatspiprivate.h"
#include "gtkatspiutilsprivate.h"

#include "a11y/atspi/atspi-accessible.h"

#include "gtkdebug.h"
#include "gtkwindow.h"

#include <gio/gio.h>

#include <locale.h>

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

enum {
  ATSPI_STATE_INVALID,
  ATSPI_STATE_ACTIVE,
  ATSPI_STATE_ARMED,
  ATSPI_STATE_BUSY,
  ATSPI_STATE_CHECKED,
  ATSPI_STATE_COLLAPSED,
  ATSPI_STATE_DEFUNCT,
  ATSPI_STATE_EDITABLE,
  ATSPI_STATE_ENABLED,
  ATSPI_STATE_EXPANDABLE,
  ATSPI_STATE_EXPANDED,
  ATSPI_STATE_FOCUSABLE,
  ATSPI_STATE_FOCUSED,
  ATSPI_STATE_HAS_TOOLTIP,
  ATSPI_STATE_HORIZONTAL,
  ATSPI_STATE_ICONIFIED,
  ATSPI_STATE_MODAL,
  ATSPI_STATE_MULTI_LINE,
  ATSPI_STATE_MULTISELECTABLE,
  ATSPI_STATE_OPAQUE,
  ATSPI_STATE_PRESSED,
  ATSPI_STATE_RESIZABLE,
  ATSPI_STATE_SELECTABLE,
  ATSPI_STATE_SELECTED,
  ATSPI_STATE_SENSITIVE,
  ATSPI_STATE_SHOWING,
  ATSPI_STATE_SINGLE_LINE,
  ATSPI_STATE_STALE,
  ATSPI_STATE_TRANSIENT,
  ATSPI_STATE_VERTICAL,
  ATSPI_STATE_VISIBLE,
  ATSPI_STATE_MANAGES_DESCENDANTS,
  ATSPI_STATE_INDETERMINATE,
  ATSPI_STATE_REQUIRED,
  ATSPI_STATE_TRUNCATED,
  ATSPI_STATE_ANIMATED,
  ATSPI_STATE_INVALID_ENTRY,
  ATSPI_STATE_SUPPORTS_AUTOCOMPLETION,
  ATSPI_STATE_SELECTABLE_TEXT,
  ATSPI_STATE_IS_DEFAULT,
  ATSPI_STATE_VISITED,
  ATSPI_STATE_CHECKABLE,
  ATSPI_STATE_HAS_POPUP,
  ATSPI_STATE_READ_ONLY,
  ATSPI_STATE_LAST_DEFINED,
};

static void
collect_states (GtkAtSpiContext    *self,
                GVariantBuilder *builder)
{
  GtkATContext *ctx = GTK_AT_CONTEXT (self);
  GtkAccessibleValue *value;
  guint64 state = 0;

  state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_VISIBLE);

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_BUSY))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_BUSY);
      if (gtk_boolean_accessible_value_get (value))
        state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_BUSY);
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_CHECKED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_CHECKED);
      switch (gtk_tristate_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_TRISTATE_TRUE:
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_CHECKED);
          break;
        case GTK_ACCESSIBLE_TRISTATE_MIXED:
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_INDETERMINATE);
          break;
        case GTK_ACCESSIBLE_TRISTATE_FALSE:
        default:
          break;
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_DISABLED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_DISABLED);
      if (!gtk_boolean_accessible_value_get (value))
        state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_SENSITIVE);
    }
  else
    state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_SENSITIVE);

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_EXPANDED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_EXPANDED);
      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN)
        {
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_EXPANDABLE);
          if (gtk_boolean_accessible_value_get (value))
            state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_EXPANDED);
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_INVALID))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_INVALID);
      switch (gtk_invalid_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_INVALID_TRUE:
        case GTK_ACCESSIBLE_INVALID_GRAMMAR:
        case GTK_ACCESSIBLE_INVALID_SPELLING:
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_INVALID);
          break;
        case GTK_ACCESSIBLE_INVALID_FALSE:
        default:
          break;
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_PRESSED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_PRESSED);
      switch (gtk_tristate_accessible_value_get (value))
        {
        case GTK_ACCESSIBLE_TRISTATE_TRUE:
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_PRESSED);
          break;
        case GTK_ACCESSIBLE_TRISTATE_MIXED:
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_INDETERMINATE);
          break;
        case GTK_ACCESSIBLE_TRISTATE_FALSE:
        default:
          break;
        }
    }

  if (gtk_at_context_has_accessible_state (ctx, GTK_ACCESSIBLE_STATE_SELECTED))
    {
      value = gtk_at_context_get_accessible_state (ctx, GTK_ACCESSIBLE_STATE_SELECTED);
      if (value->value_class->type == GTK_ACCESSIBLE_VALUE_TYPE_BOOLEAN)
        {
          state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_SELECTABLE);
          if (gtk_boolean_accessible_value_get (value))
            state |= (G_GUINT64_CONSTANT (1) << ATSPI_STATE_SELECTED);
        }
    }

  g_variant_builder_add (builder, "u", (guint32) (state & 0xffffffff));
  g_variant_builder_add (builder, "u", (guint32) (state >> 32));
}

static void
handle_accessible_method (GDBusConnection       *connection,
                          const gchar           *sender,
                          const gchar           *object_path,
                          const gchar           *interface_name,
                          const gchar           *method_name,
                          GVariant              *parameters,
                          GDBusMethodInvocation *invocation,
                          gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;

  if (g_strcmp0 (method_name, "GetRole") == 0)
    {
      GtkAccessibleRole role = gtk_at_context_get_accessible_role (GTK_AT_CONTEXT (self));
      guint atspi_role = gtk_accessible_role_to_atspi_role (role);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(u)", atspi_role));
    }
  else if (g_strcmp0 (method_name, "GetRoleName") == 0)
    {
      GtkAccessibleRole role = gtk_at_context_get_accessible_role (GTK_AT_CONTEXT (self));
      const char *name = gtk_accessible_role_to_name (role, NULL);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", name));
    }
  else if (g_strcmp0 (method_name, "GetLocalizedRoleName") == 0)
    {
      GtkAccessibleRole role = gtk_at_context_get_accessible_role (GTK_AT_CONTEXT (self));
      const char *name = gtk_accessible_role_to_name (role, GETTEXT_PACKAGE);
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", name));
    }
  else if (g_strcmp0 (method_name, "GetState") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("(au)"));

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("au"));
      collect_states (self, &builder);
      g_variant_builder_close (&builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_builder_end (&builder));
    }
  else if (g_strcmp0 (method_name, "GetAttributes") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("(a{ss})"));

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));
      g_variant_builder_add (&builder, "{ss}", "toolkit", "GTK");
      g_variant_builder_close (&builder);

      g_dbus_method_invocation_return_value (invocation, g_variant_builder_end (&builder));
    }
  else if (g_strcmp0 (method_name, "GetApplication") == 0)
    {
      const char *name, *path;

      gtk_at_spi_root_get_application (self->root, &name, &path);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("((so))", name, path));
    }
  else if (g_strcmp0 (method_name, "GetChildAtIndex") == 0)
    {
      GtkWidget *child = NULL;
      int idx, real_idx = 0;

      g_variant_get (parameters, "(i)", &idx);

      GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
      GtkWidget *widget = GTK_WIDGET (accessible);

      real_idx = 0;
      for (child = gtk_widget_get_first_child (widget);
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_get_visible (child))
            continue;

          if (real_idx == idx)
            break;

          real_idx += 1;
        }

      if (child == NULL)
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_ARGUMENT,
                                                 "No child with index %d", idx);
          return;
        }

      GtkATContext *context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (child));

      const char *name = g_dbus_connection_get_unique_name (self->connection);
      const char *path = gtk_at_spi_context_get_context_path (GTK_AT_SPI_CONTEXT (context));

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("((so))", name, path));
    }
  else if (g_strcmp0 (method_name, "GetChildren") == 0)
    {
      GVariantBuilder builder = G_VARIANT_BUILDER_INIT (G_VARIANT_TYPE ("a(so)"));

      GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
      GtkWidget *widget = GTK_WIDGET (accessible);
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (widget);
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_get_visible (child))
            continue;

          GtkATContext *context = gtk_accessible_get_at_context (GTK_ACCESSIBLE (child));

          const char *name = g_dbus_connection_get_unique_name (self->connection);
          const char *path = gtk_at_spi_context_get_context_path (GTK_AT_SPI_CONTEXT (context));

          g_variant_builder_add (&builder, "(so)", name, path);
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(so))", &builder));
    }
  else if (g_strcmp0 (method_name, "GetIndexInParent") == 0)
    {
      GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
      GtkWidget *widget = GTK_WIDGET (accessible);
      GtkWidget *parent = gtk_widget_get_parent (widget);
      GtkWidget *child;
      int idx;

      idx = 0;
      for (child = gtk_widget_get_first_child (parent);
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_get_visible (child))
            continue;

          if (child == widget)
            break;

          idx++;
        }

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", idx));
    }
}

static GVariant *
handle_accessible_get_property (GDBusConnection       *connection,
                                const gchar           *sender,
                                const gchar           *object_path,
                                const gchar           *interface_name,
                                const gchar           *property_name,
                                GError               **error,
                                gpointer               user_data)
{
  GtkAtSpiContext *self = user_data;
  GVariant *res = NULL;

  GtkAccessible *accessible = gtk_at_context_get_accessible (GTK_AT_CONTEXT (self));
  GtkWidget *widget = GTK_WIDGET (accessible);

  if (g_strcmp0 (property_name, "Name") == 0)
    res = g_variant_new_string (gtk_widget_get_name (widget));
  else if (g_strcmp0 (property_name, "Description") == 0)
    {
      char *label = gtk_at_context_get_label (GTK_AT_CONTEXT (self));
      res = g_variant_new_string (label);
      g_free (label);
    }
  else if (g_strcmp0 (property_name, "Locale") == 0)
    res = g_variant_new_string (setlocale (LC_MESSAGES, NULL));
  else if (g_strcmp0 (property_name, "AccessibleId") == 0)
    res = g_variant_new_string ("");
  else if (g_strcmp0 (property_name, "Parent") == 0)
    {
      GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (accessible));

      if (parent == NULL)
        {
          const char *name, *path;

          gtk_at_spi_root_get_application (self->root, &name, &path);
          res = g_variant_new ("(so)", name, path);
        }
      else
        {
          GtkATContext *parent_context =
            gtk_accessible_get_at_context (GTK_ACCESSIBLE (parent));

          if (parent_context != NULL)
            res = g_variant_new ("(so)",
                                 g_dbus_connection_get_unique_name (self->connection),
                                 GTK_AT_SPI_CONTEXT (parent_context)->context_path);
        }

      if (res == NULL)
        res = g_variant_new ("(so)", "", "/org/a11y/atspi/null");
    }
  else if (g_strcmp0 (property_name, "ChildCount") == 0)
    {
      int n_children = 0;
      GtkWidget *child;

      for (child = gtk_widget_get_first_child (widget);
           child;
           child = gtk_widget_get_next_sibling (child))
        {
          if (!gtk_widget_get_visible (child))
            continue;

          n_children++;
        }

      res = g_variant_new_int32 (n_children);
    }
  else
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                 "Unknown property '%s'", property_name);

  return res;
}

static const GDBusInterfaceVTable accessible_vtable = {
  handle_accessible_method,
  handle_accessible_get_property,
  NULL,
};

static void
gtk_at_spi_context_register_object (GtkAtSpiContext *self)
{
  g_dbus_connection_register_object (self->connection,
                                     self->context_path,
                                     (GDBusInterfaceInfo *) &atspi_accessible_interface,
                                     &accessible_vtable,
                                     self,
                                     NULL,
                                     NULL);
}

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
      g_object_set_data_full (G_OBJECT (display), "-gtk-atspi-root",
                              self->root,
                              g_object_unref);
    }

  self->connection = gtk_at_spi_root_get_connection (self->root);

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

  /* UUIDs use '-' as the separator, but that's not a valid character
   * for a DBus object path
   */
  size_t path_len = strlen (self->context_path);
  for (size_t i = 0; i < path_len; i++)
    {
      if (self->context_path[i] == '-')
        self->context_path[i] = '_';
    }

  GTK_NOTE (A11Y, g_message ("ATSPI context path: %s", self->context_path));

  g_free (base_path);
  g_free (uuid);

  gtk_at_spi_context_register_object (self);

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
      GTK_NOTE (A11Y, g_message ("Using ATSPI bus address from environment: %s", bus_address));
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

          GTK_NOTE (A11Y, g_message ("Using ATSPI bus address from D-Bus: %s", addr));
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
            {
              addr = get_bus_address_x11 (display);
              GTK_NOTE (A11Y, g_message ("Using ATSPI bus address from X11: %s", addr));
            }
          else
            {
              GTK_NOTE (A11Y, g_message ("Using ATSPI bus address from D-Bus: %s", addr));
            }

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

const char *
gtk_at_spi_context_get_context_path (GtkAtSpiContext *self)
{
  g_return_val_if_fail (GTK_IS_AT_SPI_CONTEXT (self), NULL);

  return self->context_path;
}
