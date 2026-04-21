/*
 * Copyright © 2025 the GTK team
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

#include "gdkapplaunchcontextprivate.h"

#include <glib.h>

typedef struct _GdkWin32AppLaunchContext GdkWin32AppLaunchContext;
typedef struct _GdkWin32AppLaunchContextClass GdkWin32AppLaunchContextClass;

struct _GdkWin32AppLaunchContext
{
  GdkAppLaunchContext base;
};

struct _GdkWin32AppLaunchContextClass
{
  GdkAppLaunchContextClass base_class;
};

GType gdk_win32_app_launch_context_get_type (void);

G_DEFINE_TYPE (GdkWin32AppLaunchContext, gdk_win32_app_launch_context, GDK_TYPE_APP_LAUNCH_CONTEXT)

static void
gdk_win32_app_launch_context_init (GdkWin32AppLaunchContext *context_win32)
{
}

static char *
gdk_win32_app_launch_context_get_startup_notify_id (GAppLaunchContext *context,
                                                    GAppInfo          *info,
                                                    GList             *files)
{
  const char *tokens[3] = {
    "win32-startup-notify",
    "foreground-window",
    NULL
  };

  return g_strjoinv (",", (char**)tokens);
}

static void
gdk_win32_app_launch_context_class_init (GdkWin32AppLaunchContextClass *klass)
{
  GAppLaunchContextClass *ctx_class = G_APP_LAUNCH_CONTEXT_CLASS (klass);

  ctx_class->get_startup_notify_id = gdk_win32_app_launch_context_get_startup_notify_id;
}

GdkAppLaunchContext *
gdk_win32_display_get_app_launch_context (GdkDisplay *display)
{
  GdkAppLaunchContext *ctx;

  ctx = g_object_new (gdk_win32_app_launch_context_get_type (),
                      "display", display,
                      NULL);

  return ctx;
}
