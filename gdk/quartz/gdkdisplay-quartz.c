/* gdkdisplay-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk/gdk.h>
#include <gdk/gdkdisplayprivate.h>

#include "gdkprivate-quartz.h"
#include "gdkquartzscreen.h"
#include "gdkquartzsurface.h"
#include "gdkquartzdisplay.h"
#include "gdkquartzdevicemanager-core.h"
#include "gdkmonitorprivate.h"
#include "gdkdisplay-quartz.h"
#include "gdkcairocontext-quartz.h"


static GdkSurface *
gdk_quartz_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  /* FIXME: Implement */

  return NULL;
}

GdkDeviceManager *
_gdk_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE,
                       "display", display,
                       NULL);
}

GdkDisplay *
_gdk_quartz_display_open (const gchar *display_name)
{
  if (_gdk_display != NULL)
    return NULL;

  _gdk_display = g_object_new (gdk_quartz_display_get_type (), NULL);
  _gdk_device_manager = _gdk_device_manager_new (_gdk_display);

  _gdk_screen = g_object_new (gdk_quartz_screen_get_type (), NULL);

  _gdk_quartz_surface_init_windowing (_gdk_display);

  _gdk_quartz_events_init ();

  /* Initialize application */
  [NSApplication sharedApplication];
#if 0
  /* FIXME: Remove the #if 0 when we have these functions */
  _gdk_quartz_dnd_init ();
#endif

  g_signal_emit_by_name (_gdk_display, "opened");

  return _gdk_display;
}

static const gchar *
gdk_quartz_display_get_name (GdkDisplay *display)
{
  static gchar *display_name = NULL;

  if (!display_name)
    {
      GDK_QUARTZ_ALLOC_POOL;
      display_name = g_strdup ([[[NSHost currentHost] name] UTF8String]);
      GDK_QUARTZ_RELEASE_POOL;
    }

  return display_name;
}

static void
gdk_quartz_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  NSBeep();
}

static void
gdk_quartz_display_sync (GdkDisplay *display)
{
  /* Not supported. */
}

static void
gdk_quartz_display_flush (GdkDisplay *display)
{
  /* Not supported. */
}

static gboolean
gdk_quartz_display_supports_shapes (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_input_shapes (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
}

static gulong
gdk_quartz_display_get_next_serial (GdkDisplay *display)
{
  return 0;
}

static void
gdk_quartz_display_notify_startup_complete (GdkDisplay  *display,
                                            const gchar *startup_id)
{
  /* FIXME: Implement? */
}

static int
gdk_quartz_display_get_n_monitors (GdkDisplay *display)
{
  GdkQuartzDisplay *quartz_display = GDK_QUARTZ_DISPLAY (display);

  return quartz_display->monitors->len;
}


static GdkMonitor *
gdk_quartz_display_get_monitor (GdkDisplay *display,
                                int         monitor_num)
{
  GdkQuartzDisplay *quartz_display = GDK_QUARTZ_DISPLAY (display);

  if (0 <= monitor_num || monitor_num < quartz_display->monitors->len)
    return (GdkMonitor *)quartz_display->monitors->pdata[monitor_num];

  return NULL;
}

static gboolean
gdk_quartz_display_get_setting (GdkDisplay  *display,
                                const gchar *name,
                                GValue      *value)
{
  return _gdk_quartz_get_setting (name, value);
}


G_DEFINE_TYPE (GdkQuartzDisplay, gdk_quartz_display, GDK_TYPE_DISPLAY)

static void
gdk_quartz_display_init (GdkQuartzDisplay *display)
{
  GDK_QUARTZ_ALLOC_POOL;

  display->monitors = g_ptr_array_new_with_free_func (g_object_unref);

  GDK_QUARTZ_RELEASE_POOL;
}

static void
gdk_quartz_display_dispose (GObject *object)
{
  GdkQuartzDisplay *display_quartz = GDK_QUARTZ_DISPLAY (object);

  g_ptr_array_free (display_quartz->monitors, TRUE);

  G_OBJECT_CLASS (gdk_quartz_display_parent_class)->dispose (object);
}

static void
gdk_quartz_display_finalize (GObject *object)
{
  GdkQuartzDisplay *display_quartz = GDK_QUARTZ_DISPLAY (object);

  G_OBJECT_CLASS (gdk_quartz_display_parent_class)->finalize (object);
}

static void
gdk_quartz_display_class_init (GdkQuartzDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->finalize = gdk_quartz_display_finalize;
  object_class->dispose = gdk_quartz_display_dispose;

  display_class->surface_type = GDK_TYPE_QUARTZ_SURFACE;
  display_class->cairo_context_type = GDK_TYPE_QUARTZ_CAIRO_CONTEXT;

  display_class->get_name = gdk_quartz_display_get_name;
  display_class->beep = gdk_quartz_display_beep;
  display_class->sync = gdk_quartz_display_sync;
  display_class->flush = gdk_quartz_display_flush;
  display_class->queue_events = _gdk_quartz_display_queue_events;
  display_class->has_pending = _gdk_quartz_display_has_pending;
  display_class->get_default_group = gdk_quartz_display_get_default_group;
  display_class->supports_shapes = gdk_quartz_display_supports_shapes;
  display_class->supports_input_shapes = gdk_quartz_display_supports_input_shapes;
  display_class->get_default_cursor_size = _gdk_quartz_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_quartz_display_get_maximal_cursor_size;
  display_class->supports_cursor_alpha = _gdk_quartz_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_quartz_display_supports_cursor_color;

  display_class->get_next_serial = gdk_quartz_display_get_next_serial;
  display_class->notify_startup_complete = gdk_quartz_display_notify_startup_complete;
  display_class->event_data_copy = _gdk_quartz_display_event_data_copy;
  display_class->event_data_free = _gdk_quartz_display_event_data_free;
  display_class->create_surface_impl = _gdk_quartz_display_create_surface_impl;
  display_class->get_keymap = _gdk_quartz_display_get_keymap;
  display_class->text_property_to_utf8_list = _gdk_quartz_display_text_property_to_utf8_list;
  display_class->get_n_monitors = gdk_quartz_display_get_n_monitors;
  display_class->get_monitor = gdk_quartz_display_get_monitor;
  display_class->get_setting = gdk_quartz_display_get_setting;

  ProcessSerialNumber psn = { 0, kCurrentProcess };

  /* Make the current process a foreground application, i.e. an app
   * with a user interface, in case we're not running from a .app bundle
   */
  TransformProcessType (&psn, kProcessTransformToForegroundApplication);
}
