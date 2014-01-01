/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkapplicationprivate.h"

#include <gdk/gdkx.h>

typedef GtkApplicationImplDBusClass GtkApplicationImplX11Class;

typedef struct
{
  GtkApplicationImplDBus dbus;

} GtkApplicationImplX11;

G_DEFINE_TYPE (GtkApplicationImplX11, gtk_application_impl_x11, GTK_TYPE_APPLICATION_IMPL_DBUS)

static void
gtk_application_impl_x11_handle_window_realize (GtkApplicationImpl *impl,
                                                GtkWindow          *window)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;
  GdkWindow *gdk_window;
  gchar *window_path;

  gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

  if (!GDK_IS_X11_WINDOW (gdk_window))
    return;

  window_path = gtk_application_impl_dbus_get_window_path (dbus, window);

  gdk_x11_window_set_utf8_property (gdk_window, "_GTK_APPLICATION_ID", dbus->application_id);
  gdk_x11_window_set_utf8_property (gdk_window, "_GTK_UNIQUE_BUS_NAME", dbus->unique_name);
  gdk_x11_window_set_utf8_property (gdk_window, "_GTK_APPLICATION_OBJECT_PATH", dbus->object_path);
  gdk_x11_window_set_utf8_property (gdk_window, "_GTK_WINDOW_OBJECT_PATH", window_path);
  gdk_x11_window_set_utf8_property (gdk_window, "_GTK_APP_MENU_OBJECT_PATH", dbus->app_menu_path);
  gdk_x11_window_set_utf8_property (gdk_window, "_GTK_MENUBAR_OBJECT_PATH", dbus->menubar_path);

  g_free (window_path);
}

static GVariant *
gtk_application_impl_x11_get_window_system_id (GtkApplicationImplDBus *dbus,
                                               GtkWindow              *window)
{
  GdkWindow *gdk_window;

  gdk_window = gtk_widget_get_window (GTK_WIDGET (window));

  if (GDK_IS_X11_WINDOW (gdk_window))
    return g_variant_new_uint32 (GDK_WINDOW_XID (gdk_window));

  return GTK_APPLICATION_IMPL_DBUS_CLASS (gtk_application_impl_x11_parent_class)->get_window_system_id (dbus, window);
}

static void
gtk_application_impl_x11_init (GtkApplicationImplX11 *x11)
{
}

static void
gtk_application_impl_x11_before_emit (GtkApplicationImpl *impl,
                                      GVariant           *platform_data)
{
  const char *startup_notification_id;

  if (g_variant_lookup (platform_data, "desktop-startup-id",
                        "&s", &startup_notification_id))
    {
      gdk_x11_display_set_startup_notification_id (gdk_display_get_default (),
                                                   startup_notification_id);
    }
}

static void
gtk_application_impl_x11_class_init (GtkApplicationImplX11Class *class)
{
  GtkApplicationImplDBusClass *dbus_class = GTK_APPLICATION_IMPL_DBUS_CLASS (class);
  GtkApplicationImplClass *impl_class = GTK_APPLICATION_IMPL_CLASS (class);

  impl_class->handle_window_realize = gtk_application_impl_x11_handle_window_realize;
  dbus_class->get_window_system_id = gtk_application_impl_x11_get_window_system_id;
  impl_class->before_emit = gtk_application_impl_x11_before_emit;
}
