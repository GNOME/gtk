/*
 * Copyright © 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <gio/gdesktopappinfo.h>

#include "gdkwayland.h"
#include "gdkprivate-wayland.h"
#include "gdkapplaunchcontextprivate.h"
#include "gdkscreen.h"
#include "gdkinternals.h"
#include "gdkintl.h"

typedef struct {
  gchar *token;
  gboolean failed;
} AppLaunchData;

static void
token_provider_done (gpointer                                  data,
                     struct zxdg_activation_token_provider_v1 *provider,
                     const char                               *token)
{
  AppLaunchData *app_launch_data = data;

  app_launch_data->token = g_strdup (token);
}

static void
token_provider_failed (gpointer                                  data,
                       struct zxdg_activation_token_provider_v1 *provider)
{
  AppLaunchData *app_launch_data = data;

  app_launch_data->failed = TRUE;
}

static const struct zxdg_activation_token_provider_v1_listener token_provider_listener = {
  token_provider_done,
  token_provider_failed,
};

static char *
gdk_wayland_app_launch_context_get_startup_notify_id (GAppLaunchContext *context,
                                                      GAppInfo          *info,
                                                      GList             *files)
{
  GdkWaylandDisplay *display;
  gchar *id = NULL;

  g_object_get (context, "display", &display, NULL);

  if (display->xdg_activation)
    {
      struct zxdg_activation_token_provider_v1 *token_provider;
      GdkSeat *seat;
      GdkWindow *focus_window;
      AppLaunchData app_launch_data = { 0 };

      seat = gdk_display_get_default_seat (GDK_DISPLAY (display));
      focus_window = gdk_wayland_device_get_focus (gdk_seat_get_keyboard (seat));
      token_provider =
        zxdg_activation_v1_get_activation_token (display->xdg_activation,
                                                 gdk_wayland_window_get_wl_surface (focus_window),
                                                 _gdk_wayland_seat_get_last_implicit_grab_serial (seat, NULL),
                                                 gdk_wayland_seat_get_wl_seat (seat));

      zxdg_activation_token_provider_v1_add_listener (token_provider,
                                                      &token_provider_listener,
                                                      &app_launch_data);

      while (app_launch_data.token == NULL && !app_launch_data.failed)
        wl_display_roundtrip (display->wl_display);

      if (!app_launch_data.failed)
        {
          zxdg_activation_v1_associate (display->xdg_activation,
                                        app_launch_data.token,
                                        g_app_info_get_id (info));
        }
      else
        g_warning ("App activation failed");

      zxdg_activation_token_provider_v1_destroy (token_provider);
      id = app_launch_data.token;
    }
  else if (display->gtk_shell_version >= 3)
    {
      id = g_uuid_string_random ();
      gtk_shell1_notify_launch (display->gtk_shell, id);
    }

  g_object_unref (display);

  return id;
}

static void
gdk_wayland_app_launch_context_launch_failed (GAppLaunchContext *context,
                                              const char        *startup_notify_id)
{
}

typedef struct _GdkWaylandAppLaunchContext GdkWaylandAppLaunchContext;
typedef struct _GdkWaylandAppLaunchContextClass GdkWaylandAppLaunchContextClass;

struct _GdkWaylandAppLaunchContext
{
  GdkAppLaunchContext base;
  gchar *name;
  guint serial;
};

struct _GdkWaylandAppLaunchContextClass
{
  GdkAppLaunchContextClass base_class;
};

GType gdk_wayland_app_launch_context_get_type (void);

G_DEFINE_TYPE (GdkWaylandAppLaunchContext, gdk_wayland_app_launch_context, GDK_TYPE_APP_LAUNCH_CONTEXT)

static void
gdk_wayland_app_launch_context_class_init (GdkWaylandAppLaunchContextClass *klass)
{
  GAppLaunchContextClass *ctx_class = G_APP_LAUNCH_CONTEXT_CLASS (klass);

  ctx_class->get_startup_notify_id = gdk_wayland_app_launch_context_get_startup_notify_id;
  ctx_class->launch_failed = gdk_wayland_app_launch_context_launch_failed;
}

static void
gdk_wayland_app_launch_context_init (GdkWaylandAppLaunchContext *ctx)
{
}

GdkAppLaunchContext *
_gdk_wayland_display_get_app_launch_context (GdkDisplay *display)
{
  GdkAppLaunchContext *ctx;

  ctx = g_object_new (gdk_wayland_app_launch_context_get_type (),
                      "display", display,
                      NULL);

  return ctx;
}
