/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2013 Canonical Limited
 * Copyright © 2020 Emmanuel Gil Peyrot
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
#include "gtkapplicationwindowprivate.h"
#include "gtknative.h"
#include "gtkprivate.h"

#include <gdk/wayland/gdkwayland.h>
#include <gdk/wayland/gdktoplevel-wayland-private.h>
#include <gdk/wayland/gdkdisplay-wayland.h>


typedef struct
{
  GtkApplicationImplDBusClass parent_class;
} GtkApplicationImplWaylandClass;

typedef struct
{
  guint cookie;
  guint dbus_cookie;
  GtkApplicationInhibitFlags flags;
  GdkSurface *surface;

} GtkApplicationWaylandInhibitor;

static void
gtk_application_wayland_inhibitor_free (GtkApplicationWaylandInhibitor *inhibitor)
{
  g_free (inhibitor);
}

typedef struct
{
  GtkApplicationImplDBus dbus;
  GSList *inhibitors;
  guint next_cookie;

} GtkApplicationImplWayland;

G_DEFINE_TYPE (GtkApplicationImplWayland, gtk_application_impl_wayland, GTK_TYPE_APPLICATION_IMPL_DBUS)

static void
gtk_application_impl_wayland_handle_window_realize (GtkApplicationImpl *impl,
                                                    GtkWindow          *window)
{
  GtkApplicationImplClass *impl_class =
    GTK_APPLICATION_IMPL_CLASS (gtk_application_impl_wayland_parent_class);
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;
  GdkSurface *gdk_surface;
  char *window_path;
  GVariant *state;
  char *id = NULL;

  GTK_DEBUG (SESSION, "Handle window realize");

  gdk_surface = gtk_native_get_surface (GTK_NATIVE (window));

  if (!GDK_IS_WAYLAND_TOPLEVEL (gdk_surface))
    return;

  window_path = gtk_application_impl_dbus_get_window_path (dbus, window);

  gdk_wayland_toplevel_set_dbus_properties (GDK_TOPLEVEL (gdk_surface),
                                            dbus->application_id,
                                            dbus->app_menu_path,
                                            dbus->menubar_path,
                                            window_path,
                                            dbus->object_path,
                                            dbus->unique_name);

  g_free (window_path);

  state = gtk_application_impl_dbus_get_window_state (dbus, window);
  if (state)
    {
      g_variant_lookup (state, "session-id", "s", &id);
      GTK_DEBUG (SESSION, "Found saved session ID %s", id);
    }
  else
    {
      id = g_uuid_string_random ();
      GTK_DEBUG (SESSION, "No saved session ID, using %s", id);
    }

  GTK_DEBUG (SESSION, "Set Wayland toplevel session ID: %s", id);
  gdk_wayland_toplevel_set_session_id (GDK_TOPLEVEL (gdk_surface), id);
  gdk_wayland_toplevel_restore_from_session (GDK_TOPLEVEL (gdk_surface));
  g_free (id);

  impl_class->handle_window_realize (impl, window);
}

static void
gtk_application_impl_wayland_before_emit (GtkApplicationImpl *impl,
                                          GVariant           *platform_data)
{
  const char *startup_notification_id = NULL;

  g_variant_lookup (platform_data, "activation-token", "&s", &startup_notification_id);
  if (!startup_notification_id)
    g_variant_lookup (platform_data, "desktop-startup-id", "&s", &startup_notification_id);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_wayland_display_set_startup_notification_id (gdk_display_get_default (), startup_notification_id);
G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
gtk_application_impl_wayland_window_removed (GtkApplicationImpl *impl,
                                             GtkWindow          *window)
{
  GtkApplicationImplWayland *wayland = (GtkApplicationImplWayland *) impl;
  GSList *iter = wayland->inhibitors;
  GdkSurface *surface;

  while (iter)
    {
      GtkApplicationWaylandInhibitor *inhibitor = iter->data;
      GSList *next = iter->next;

      if (inhibitor->surface && inhibitor->surface == gtk_native_get_surface (GTK_NATIVE (window)))
        {
          inhibitor->surface = NULL;

          if (!inhibitor->dbus_cookie)
            {
              gtk_application_wayland_inhibitor_free (inhibitor);
              wayland->inhibitors = g_slist_delete_link (wayland->inhibitors, iter);
            }
        }

      iter = next;
    }

  surface = gtk_native_get_surface (GTK_NATIVE (window));
  gdk_wayland_toplevel_remove_from_session (GDK_TOPLEVEL (surface));
}

static guint
gtk_application_impl_wayland_inhibit (GtkApplicationImpl         *impl,
                                      GtkWindow                  *window,
                                      GtkApplicationInhibitFlags  flags,
                                      const char                 *reason)
{
  GtkApplicationImplWayland *wayland = (GtkApplicationImplWayland *) impl;
  GdkSurface *surface;
  GtkApplicationWaylandInhibitor *inhibitor;
  gboolean success;

  if (!flags)
    return 0;

  inhibitor = g_new0 (GtkApplicationWaylandInhibitor, 1);
  inhibitor->cookie = ++wayland->next_cookie;
  inhibitor->flags = flags;
  wayland->inhibitors = g_slist_prepend (wayland->inhibitors, inhibitor);

  if (flags & GTK_APPLICATION_INHIBIT_IDLE && window && impl->application == gtk_window_get_application (window))
    {
      surface = gtk_native_get_surface (GTK_NATIVE (window));
      if (GDK_IS_WAYLAND_TOPLEVEL (surface))
        {
          success = gdk_wayland_toplevel_inhibit_idle (GDK_TOPLEVEL (surface));
          if (success)
            {
              flags &= ~GTK_APPLICATION_INHIBIT_IDLE;
              inhibitor->surface = surface;
            }
        }
    }

  if (flags)
    inhibitor->dbus_cookie = GTK_APPLICATION_IMPL_CLASS (gtk_application_impl_wayland_parent_class)->inhibit (impl, window, flags, reason);

  return inhibitor->cookie;
}

static void
gtk_application_impl_wayland_uninhibit (GtkApplicationImpl *impl,
                                        guint               cookie)
{
  GtkApplicationImplWayland *wayland = (GtkApplicationImplWayland *) impl;
  GSList *iter;

  for (iter = wayland->inhibitors; iter; iter = iter->next)
    {
      GtkApplicationWaylandInhibitor *inhibitor = iter->data;

      if (inhibitor->cookie == cookie)
        {
          if (inhibitor->dbus_cookie)
            GTK_APPLICATION_IMPL_CLASS (gtk_application_impl_wayland_parent_class)->uninhibit (impl, inhibitor->dbus_cookie);
          if (inhibitor->surface)
            gdk_wayland_toplevel_uninhibit_idle (GDK_TOPLEVEL (inhibitor->surface));
          gtk_application_wayland_inhibitor_free (inhibitor);
          wayland->inhibitors = g_slist_delete_link (wayland->inhibitors, iter);
          return;
        }
    }

  g_warning ("Invalid inhibitor cookie");
}

static void
gtk_application_impl_wayland_startup (GtkApplicationImpl *impl,
                                      gboolean            support_save)
{
  GtkApplicationImplDBus *dbus = (GtkApplicationImplDBus *) impl;
  GdkDisplay *display = gdk_display_get_default ();
  enum xx_session_manager_v1_reason wl_reason;
  char *id = NULL;
  GVariant *state;

  GTK_APPLICATION_IMPL_CLASS (gtk_application_impl_wayland_parent_class)->startup (impl, support_save);

  if (!support_save)
    return;

  state = gtk_application_impl_retrieve_state (impl);
  if (state)
    {
      GVariant *global = g_variant_get_child_value (state, 0);
      g_variant_lookup (global, "wayland-session", "s", &id);
      g_clear_pointer (&global, g_variant_unref);
      g_clear_pointer (&state, g_variant_unref);
    }

  switch (dbus->reason)
    {
    case GTK_RESTORE_REASON_LAUNCH:
      wl_reason = XX_SESSION_MANAGER_V1_REASON_LAUNCH;
      break;

    case GTK_RESTORE_REASON_RESTORE:
      wl_reason = XX_SESSION_MANAGER_V1_REASON_SESSION_RESTORE;
      break;

    case GTK_RESTORE_REASON_RECOVER:
      wl_reason = XX_SESSION_MANAGER_V1_REASON_RECOVER;
      break;

    case GTK_RESTORE_REASON_PRISTINE:
      wl_reason = XX_SESSION_MANAGER_V1_REASON_LAUNCH;
      g_clear_pointer (&id, g_free);
      break;
    default:
      g_assert_not_reached ();
    }

  GTK_DEBUG (SESSION, "Wayland register session ID %s", id);
  gdk_wayland_display_register_session (display, wl_reason, id);
  g_free (id);
}

static void
gtk_application_impl_wayland_collect_window_state (GtkApplicationImpl   *impl,
                                                   GtkApplicationWindow *window,
                                                   GVariantBuilder      *state)
{
  GdkSurface *surface;
  const char *session_id;

  surface = gtk_native_get_surface (GTK_NATIVE (window));

  session_id = gdk_wayland_toplevel_get_session_id (GDK_TOPLEVEL (surface));
  if (session_id)
    g_variant_builder_add (state, "{sv}", "session-id", g_variant_new_string (session_id));
}

static void
gtk_application_impl_wayland_collect_global_state (GtkApplicationImpl *impl,
                                                   GVariantBuilder    *state)
{
  GdkDisplay *display = gdk_display_get_default ();
  const char *id;

  GTK_APPLICATION_IMPL_CLASS (gtk_application_impl_wayland_parent_class)->collect_global_state (impl, state);

  id = gdk_wayland_display_get_session_id (display);

  if (id)
    g_variant_builder_add (state, "{sv}", "wayland-session", g_variant_new_string (id));
}

static void
gtk_application_impl_wayland_init (GtkApplicationImplWayland *wayland)
{
}

static void
gtk_application_impl_wayland_class_init (GtkApplicationImplWaylandClass *class)
{
  GtkApplicationImplClass *impl_class = GTK_APPLICATION_IMPL_CLASS (class);

  impl_class->handle_window_realize = gtk_application_impl_wayland_handle_window_realize;
  impl_class->before_emit = gtk_application_impl_wayland_before_emit;
  impl_class->window_removed = gtk_application_impl_wayland_window_removed;
  impl_class->inhibit = gtk_application_impl_wayland_inhibit;
  impl_class->uninhibit = gtk_application_impl_wayland_uninhibit;
  impl_class->startup = gtk_application_impl_wayland_startup;
  impl_class->collect_window_state = gtk_application_impl_wayland_collect_window_state;
  impl_class->collect_global_state = gtk_application_impl_wayland_collect_global_state;
}
