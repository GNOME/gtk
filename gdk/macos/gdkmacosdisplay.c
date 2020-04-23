/*
 * Copyright © 2020 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdk.h>

#include "gdkmacosdisplay-private.h"
#include "gdkmacosmonitor-private.h"

/**
 * SECTION:macos_interaction
 * @Short_description: macOS backend-specific functions
 * @Title: macOS Interaction
 * @Include: gdk/macos/gdkmacos.h
 *
 * The functions in this section are specific to the GDK macOS backend.
 * To use them, you need to include the `<gdk/macos/gdkmacos.h>` header and
 * use the macOS-specific pkg-config `gtk4-macos` file to build your
 * application.
 *
 * To make your code compile with other GDK backends, guard backend-specific
 * calls by an ifdef as follows. Since GDK may be built with multiple
 * backends, you should also check for the backend that is in use (e.g. by
 * using the GDK_IS_MACOS_DISPLAY() macro).
 * |[<!-- language="C" -->
 * #ifdef GDK_WINDOWING_MACOS
 *   if (GDK_IS_MACOS_DISPLAY (display))
 *     {
 *       // make macOS-specific calls here
 *     }
 *   else
 * #endif
 * #ifdef GDK_WINDOWING_X11
 *   if (GDK_IS_X11_DISPLAY (display))
 *     {
 *       // make X11-specific calls here
 *     }
 *   else
 * #endif
 *   g_error ("Unsupported GDK backend");
 * ]|
 */

G_DEFINE_TYPE (GdkMacosDisplay, gdk_macos_display, GDK_TYPE_DISPLAY)

static void
gdk_macos_display_add_monitor (GdkMacosDisplay *self,
                               GdkMacosMonitor *monitor)
{
  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (GDK_IS_MACOS_MONITOR (monitor));

  g_ptr_array_add (self->monitors, g_object_ref (monitor));
  gdk_display_monitor_added (GDK_DISPLAY (self), GDK_MONITOR (monitor));
}

static void
gdk_macos_display_remove_monitor (GdkMacosDisplay *self,
                                  GdkMacosMonitor *monitor)
{
  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (GDK_IS_MACOS_MONITOR (monitor));

  g_object_ref (monitor);

  if (g_ptr_array_remove (self->monitors, monitor))
    gdk_display_monitor_removed (GDK_DISPLAY (self), GDK_MONITOR (monitor));

  g_object_unref (monitor);
}

static void
gdk_macos_display_load_monitors (GdkMacosDisplay *self)
{
#if 0
  GDK_BEGIN_MACOS_ALLOC_POOL;

  NSArray *screens;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  screens = [NSScreen screens];

  for (id obj in screens)
    {
      CGDirectDisplayID screen_id;
      GdkMonitor *monitor;

      screen_id = [[[obj deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
      monitor = _gdk_macos_monitor_new (self, screen_id);

      gdk_macos_display_add_monitor (self, monitor);

      g_object_unref (monitor);
    }

  GDK_END_MACOS_ALLOC_POOL;
#endif
}

static void
gdk_macos_display_constructed (GObject *object)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)object;

  G_OBJECT_CLASS (gdk_macos_display_parent_class)->constructed (object);

  gdk_macos_display_load_monitors (self);
}

static void
gdk_macos_display_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_macos_display_parent_class)->finalize (object);
}

static void
gdk_macos_display_class_init (GdkMacosDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gdk_macos_display_constructed;
  object_class->finalize = gdk_macos_display_finalize;
}

static void
gdk_macos_display_init (GdkMacosDisplay *self)
{
  self->monitors = g_ptr_array_new_with_free_func (g_object_unref);
}

GdkDisplay *
_gdk_macos_display_open (const gchar *display_name)
{
  GdkDisplay *display;

  GDK_NOTE (MISC, g_message ("opening display %s", display_name ? display_name : ""));

  /* Make the current process a foreground application, i.e. an app
   * with a user interface, in case we're not running from a .app bundle
   */
  //TransformProcessType (&psn, kProcessTransformToForegroundApplication);

  display = g_object_new (GDK_TYPE_MACOS_DISPLAY, NULL);

  //[NSApplication sharedApplication];

  gdk_display_emit_opened (display);

  return g_steal_pointer (&display);
}
