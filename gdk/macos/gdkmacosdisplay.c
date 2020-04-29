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

#include <AppKit/AppKit.h>
#include <gdk/gdk.h>

#include "gdkmacoscairocontext-private.h"
#include "gdkdisplayprivate.h"
#include "gdkmacoseventsource-private.h"
#include "gdkmacosdisplay-private.h"
#include "gdkmacoskeymap-private.h"
#include "gdkmacosmonitor-private.h"
#include "gdkmacosseat-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacosutils-private.h"

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

struct _GdkMacosDisplay
{
  GdkDisplay           parent_instance;

  char                *name;
  GPtrArray           *monitors;
  GdkMacosKeymap      *keymap;

  int                  width;
  int                  height;
  int                  min_x;
  int                  min_y;
};

struct _GdkMacosDisplayClass
{
  GdkDisplayClass parent_class;
};

G_DEFINE_TYPE (GdkMacosDisplay, gdk_macos_display, GDK_TYPE_DISPLAY)

static GSource *event_source;

static gboolean
gdk_macos_display_get_setting (GdkDisplay  *display,
                               const gchar *setting,
                               GValue      *value)
{
  return FALSE;
}

static int
gdk_macos_display_get_n_monitors (GdkDisplay *display)
{
  return GDK_MACOS_DISPLAY (display)->monitors->len;
}

static GdkMonitor *
gdk_macos_display_get_monitor (GdkDisplay *display,
                               int         index)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)display;

  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (index >= 0);

  if (index < self->monitors->len)
    return g_ptr_array_index (self->monitors, index);

  return NULL;
}

static GdkMonitor *
gdk_macos_display_get_primary_monitor (GdkDisplay *display)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)display;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  return g_ptr_array_index (self->monitors, 0);
}

static GdkMonitor *
gdk_macos_display_get_monitor_at_surface (GdkDisplay *display,
                                          GdkSurface *surface)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)display;
  CGDirectDisplayID screen_id;

  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  screen_id = _gdk_macos_surface_get_screen_id (GDK_MACOS_SURFACE (surface));

  for (guint i = 0; i < self->monitors->len; i++)
    {
      GdkMacosMonitor *monitor = g_ptr_array_index (self->monitors, i);

      if (screen_id == _gdk_macos_monitor_get_screen_id (monitor))
        return GDK_MONITOR (monitor);
    }

  return gdk_macos_display_get_primary_monitor (display);
}

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
  GDK_BEGIN_MACOS_ALLOC_POOL;

  NSArray *screens;
  int max_x = 0;
  int max_y = 0;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  screens = [NSScreen screens];

  for (id obj in screens)
    {
      CGDirectDisplayID screen_id;
      GdkMacosMonitor *monitor;
      NSRect geom;

      geom = [obj frame];
      self->min_x = MIN (self->min_x, geom.origin.x);
      self->min_y = MIN (self->min_y, geom.origin.y);
      max_x = MAX (max_x, geom.origin.x + geom.size.width);
      max_y = MAX (max_y, geom.origin.y + geom.size.height);

      screen_id = [[[obj deviceDescription] objectForKey:@"NSScreenNumber"] unsignedIntValue];
      monitor = _gdk_macos_monitor_new (self, screen_id);

      gdk_macos_display_add_monitor (self, monitor);

      g_object_unref (monitor);
    }

  self->width = max_x - self->min_x;
  self->height = max_y - self->min_y;

  GDK_END_MACOS_ALLOC_POOL;
}

static void
gdk_macos_display_load_seat (GdkMacosDisplay *self)
{
  GdkSeat *seat;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  seat = _gdk_macos_seat_new (self);
  gdk_display_add_seat (GDK_DISPLAY (self), seat);
  g_object_unref (seat);
}

static const gchar *
gdk_macos_display_get_name (GdkDisplay *display)
{
  return GDK_MACOS_DISPLAY (display)->name;
}

static void
gdk_macos_display_beep (GdkDisplay *display)
{
  NSBeep ();
}

static void
gdk_macos_display_flush (GdkDisplay *display)
{
  /* Not Supported */
}

static void
gdk_macos_display_sync (GdkDisplay *display)
{
  /* Not Supported */
}

static gboolean
gdk_macos_display_supports_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gboolean
gdk_macos_display_supports_input_shapes (GdkDisplay *display)
{
  return FALSE;
}

static gulong
gdk_macos_display_get_next_serial (GdkDisplay *display)
{
  return 0;
}

static gboolean
gdk_macos_display_has_pending (GdkDisplay *display)
{
  g_warning ("Has pending");
  return FALSE;
}

static void
gdk_macos_display_notify_startup_complete (GdkDisplay  *display,
                                           const gchar *startup_notification_id)
{
  /* Not Supported */
}

static void
gdk_macos_display_queue_events (GdkDisplay *display)
{
  g_warning ("Queue events");
}

static void
gdk_macos_display_event_data_copy (GdkDisplay *display,
                                   GdkEvent   *event,
                                   GdkEvent   *new_event)
{
  g_assert (GDK_IS_MACOS_DISPLAY (display));
  g_assert (event != NULL);
  g_assert (new_event != NULL);

}

static void
gdk_macos_display_event_data_free (GdkDisplay *display,
                                   GdkEvent   *event)
{
  g_assert (GDK_IS_MACOS_DISPLAY (display));
  g_assert (event != NULL);

}

static GdkSurface *
gdk_macos_display_create_surface (GdkDisplay     *display,
                                  GdkSurfaceType  surface_type,
                                  GdkSurface     *parent,
                                  int             x,
                                  int             y,
                                  int             width,
                                  int             height)
{
  GdkMacosSurface *surface;

  g_assert (GDK_IS_MACOS_DISPLAY (display));
  g_assert (!parent || GDK_IS_MACOS_SURFACE (parent));

  surface = _gdk_macos_surface_new (GDK_MACOS_DISPLAY (display),
                                    surface_type,
                                    parent,
                                    x, y,
                                    width, height);

  return GDK_SURFACE (surface);
}

static GdkKeymap *
gdk_macos_display_get_keymap (GdkDisplay *display)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)display;

  g_assert (GDK_IS_MACOS_DISPLAY (self));

  return GDK_KEYMAP (self->keymap);
}

static void
gdk_macos_display_finalize (GObject *object)
{
  GdkMacosDisplay *self = (GdkMacosDisplay *)object;

  g_clear_pointer (&self->monitors, g_ptr_array_unref);
  g_clear_pointer (&self->name, g_free);

  G_OBJECT_CLASS (gdk_macos_display_parent_class)->finalize (object);
}

static void
gdk_macos_display_class_init (GdkMacosDisplayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (klass);

  object_class->finalize = gdk_macos_display_finalize;

  display_class->cairo_context_type = GDK_TYPE_MACOS_CAIRO_CONTEXT;

  display_class->beep = gdk_macos_display_beep;
  display_class->create_surface = gdk_macos_display_create_surface;
  display_class->event_data_copy = gdk_macos_display_event_data_copy;
  display_class->event_data_free = gdk_macos_display_event_data_free;
  display_class->flush = gdk_macos_display_flush;
  display_class->get_keymap = gdk_macos_display_get_keymap;
  display_class->get_monitor = gdk_macos_display_get_monitor;
  display_class->get_monitor_at_surface = gdk_macos_display_get_monitor_at_surface;
  display_class->get_next_serial = gdk_macos_display_get_next_serial;
  display_class->get_n_monitors = gdk_macos_display_get_n_monitors;
  display_class->get_name = gdk_macos_display_get_name;
  display_class->get_primary_monitor = gdk_macos_display_get_primary_monitor;
  display_class->get_setting = gdk_macos_display_get_setting;
  display_class->has_pending = gdk_macos_display_has_pending;
  display_class->notify_startup_complete = gdk_macos_display_notify_startup_complete;
  display_class->queue_events = gdk_macos_display_queue_events;
  display_class->supports_input_shapes = gdk_macos_display_supports_input_shapes;
  display_class->supports_shapes = gdk_macos_display_supports_shapes;
  display_class->sync = gdk_macos_display_sync;
}

static void
gdk_macos_display_init (GdkMacosDisplay *self)
{
  self->monitors = g_ptr_array_new_with_free_func (g_object_unref);
}

GdkDisplay *
_gdk_macos_display_open (const gchar *display_name)
{
  GdkMacosDisplay *self;
  ProcessSerialNumber psn = { 0, kCurrentProcess };

  GDK_NOTE (MISC, g_message ("opening display %s", display_name ? display_name : ""));

  /* Make the current process a foreground application, i.e. an app
   * with a user interface, in case we're not running from a .app bundle
   */
  TransformProcessType (&psn, kProcessTransformToForegroundApplication);

  [NSApplication sharedApplication];

  self = g_object_new (GDK_TYPE_MACOS_DISPLAY, NULL);
  self->name = g_strdup (display_name);
  self->keymap = _gdk_macos_keymap_new (self);

  gdk_macos_display_load_seat (self);
  gdk_macos_display_load_monitors (self);

  if (event_source == NULL)
    {
      event_source = _gdk_macos_event_source_new ();
      g_source_attach (event_source, NULL);
    }

  gdk_display_emit_opened (GDK_DISPLAY (self));

  return GDK_DISPLAY (self);
}

void
_gdk_macos_display_to_display_coords (GdkMacosDisplay *self,
                                      int              x,
                                      int              y,
                                      int             *out_x,
                                      int             *out_y)
{
  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));

  if (out_y)
    *out_y = self->height - y + self->min_y;

  if (out_x)
    *out_x = x + self->min_x;
}

NSScreen *
_gdk_macos_display_get_screen_at_display_coords (GdkMacosDisplay *self,
                                                 int              x,
                                                 int              y)
{
  GDK_BEGIN_MACOS_ALLOC_POOL;

  NSArray *screens;
  NSScreen *screen = NULL;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), NULL);

  screens = [NSScreen screens];

  for (id obj in screens)
    {
      NSRect geom = [obj frame];

      if (x >= geom.origin.x && x <= geom.origin.x + geom.size.width &&
          y >= geom.origin.y && y <= geom.origin.y + geom.size.height)
        {
          screen = obj;
          break;
        }
    }

  GDK_END_MACOS_ALLOC_POOL;

  return screen;
}
