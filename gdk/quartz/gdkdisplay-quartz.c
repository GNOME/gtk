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
#include <gdk/gdkmonitorprivate.h>

#include "gdkprivate-quartz.h"
#include "gdkquartzscreen.h"
#include "gdkquartzwindow.h"
#include "gdkquartzdisplay.h"
#include "gdkquartzdevicemanager-core.h"
#include "gdkscreen.h"
#include "gdkmonitorprivate.h"
#include "gdkdisplay-quartz.h"


static GdkWindow *
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

  /* Initialize application */
  [NSApplication sharedApplication];

  _gdk_display = g_object_new (gdk_quartz_display_get_type (), NULL);
  _gdk_display->device_manager = _gdk_device_manager_new (_gdk_display);

  _gdk_screen = g_object_new (gdk_quartz_screen_get_type (), NULL);
  _gdk_quartz_screen_init_visuals (_gdk_screen);

  _gdk_quartz_window_init_windowing (_gdk_display, _gdk_screen);

  _gdk_quartz_events_init ();

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

static GdkScreen *
gdk_quartz_display_get_default_screen (GdkDisplay *display)
{
  return _gdk_screen;
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
gdk_quartz_display_supports_selection_notification (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);

  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_request_selection_notification (GdkDisplay *display,
                                                   GdkAtom     selection)

{
  /* FIXME: Implement */
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_clipboard_persistence (GdkDisplay *display)
{
  /* FIXME: Implement */
  return FALSE;
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

static void
gdk_quartz_display_store_clipboard (GdkDisplay    *display,
                                    GdkWindow     *clipboard_window,
                                    guint32        time_,
                                    const GdkAtom *targets,
                                    gint           n_targets)
{
  /* FIXME: Implement */
}


static gboolean
gdk_quartz_display_supports_composite (GdkDisplay *display)
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

/* The display monitor list comprises all of the CGDisplays connected
   to the system, some of which may not be drawable either because
   they're asleep or are mirroring another monitor. The NSScreens
   array contains only the monitors that are currently drawable and we
   use the index of the screens array placing GdkNSViews, so we'll use
   the same for determining the number of monitors and indexing them.
 */

static int
gdk_quartz_display_get_n_monitors (GdkDisplay *display)
{
  int n;

  GDK_QUARTZ_ALLOC_POOL;

  n = [[NSScreen screens] count];

  GDK_QUARTZ_RELEASE_POOL;

  return n;
}

static GdkMonitor *
gdk_quartz_display_get_monitor (GdkDisplay *display,
                                int         monitor_num)
{
  GdkQuartzDisplay *quartz_display = GDK_QUARTZ_DISPLAY (display);
  NSArray* screens;
  NSScreen *screen = NULL;
  CGDirectDisplayID id = 0;

  GDK_QUARTZ_ALLOC_POOL;

  screens = [NSScreen screens];
  if (monitor_num >= 0 && monitor_num < [screens count])
    {
      screen = [screens objectAtIndex:monitor_num];
      id = [[[screen deviceDescription] valueForKey: @"NSScreenNumber"] unsignedIntValue];
    }

  GDK_QUARTZ_RELEASE_POOL;

  if (id)
    return g_hash_table_lookup (quartz_display->monitors, GINT_TO_POINTER (id));

  return NULL;
}

static GdkMonitor *
gdk_quartz_display_get_primary_monitor (GdkDisplay *display)
{
  GdkQuartzDisplay *quartz_display = GDK_QUARTZ_DISPLAY (display);
  CGDirectDisplayID primary_id = CGMainDisplayID ();

  return g_hash_table_lookup (quartz_display->monitors,
                              GINT_TO_POINTER (primary_id));
}

G_DEFINE_TYPE (GdkQuartzDisplay, gdk_quartz_display, GDK_TYPE_DISPLAY)

static void
gdk_quartz_display_init (GdkQuartzDisplay *display)
{
  uint32_t max_displays = 0, disp;
  CGDirectDisplayID *displays;
  CGGetActiveDisplayList (0, NULL, &max_displays);
  display->monitors = g_hash_table_new_full (g_direct_hash, NULL,
                                             NULL, g_object_unref);
  displays = g_new0 (CGDirectDisplayID, max_displays);
  CGGetActiveDisplayList (max_displays, displays, &max_displays);
  for (disp = 0; disp < max_displays; ++disp)
    {
      CGSize disp_size = CGDisplayScreenSize (displays[disp]);
      gint width = (int)trunc (disp_size.width);
      gint height = (int)trunc (disp_size.height);
      CGRect disp_bounds = CGDisplayBounds (displays[disp]);
      GdkRectangle disp_geometry = {(int)trunc (disp_bounds.origin.x),
                                    (int)trunc (disp_bounds.origin.y),
                                    (int)trunc (disp_bounds.size.width),
                                    (int)trunc (disp_bounds.size.height)};
      CGDisplayModeRef mode = CGDisplayCopyDisplayMode (displays[disp]);
      gint refresh_rate = (int)trunc (CGDisplayModeGetRefreshRate (mode));
      GdkQuartzMonitor *quartz_monitor = g_object_new (GDK_TYPE_QUARTZ_MONITOR,
                                                       "display", display, NULL);
      GdkMonitor *monitor = GDK_MONITOR (quartz_monitor);

      monitor->width_mm = width;
      monitor->height_mm = height;
      monitor->geometry = disp_geometry;
      monitor->scale_factor = 1;
      monitor->refresh_rate = refresh_rate;
      monitor->subpixel_layout = GDK_SUBPIXEL_LAYOUT_UNKNOWN;

      g_hash_table_insert (display->monitors, GINT_TO_POINTER (displays[disp]),
                           monitor);
      CGDisplayModeRelease (mode);
    }
}

static void
gdk_quartz_display_dispose (GObject *object)
{
  GdkQuartzDisplay *display_quartz = GDK_QUARTZ_DISPLAY (object);

  g_hash_table_destroy (display_quartz->monitors);

  G_OBJECT_CLASS (gdk_quartz_display_parent_class)->dispose (object);
}

static void
gdk_quartz_display_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_quartz_display_parent_class)->finalize (object);
}

static void
gdk_quartz_display_class_init (GdkQuartzDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->finalize = gdk_quartz_display_finalize;
  object_class->dispose = gdk_quartz_display_dispose;

  display_class->window_type = GDK_TYPE_QUARTZ_WINDOW;

  display_class->get_name = gdk_quartz_display_get_name;
  display_class->get_default_screen = gdk_quartz_display_get_default_screen;
  display_class->beep = gdk_quartz_display_beep;
  display_class->sync = gdk_quartz_display_sync;
  display_class->flush = gdk_quartz_display_flush;
  display_class->has_pending = _gdk_quartz_display_has_pending;
  display_class->queue_events = _gdk_quartz_display_queue_events;
  display_class->get_default_group = gdk_quartz_display_get_default_group;
  display_class->supports_selection_notification = gdk_quartz_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_quartz_display_request_selection_notification;

  display_class->supports_shapes = gdk_quartz_display_supports_shapes;
  display_class->supports_input_shapes = gdk_quartz_display_supports_input_shapes;
  display_class->supports_composite = gdk_quartz_display_supports_composite;
  display_class->supports_cursor_alpha = _gdk_quartz_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_quartz_display_supports_cursor_color;

  display_class->supports_clipboard_persistence = gdk_quartz_display_supports_clipboard_persistence;
  display_class->store_clipboard = gdk_quartz_display_store_clipboard;

  display_class->get_default_cursor_size = _gdk_quartz_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_quartz_display_get_maximal_cursor_size;
  display_class->get_cursor_for_type = _gdk_quartz_display_get_cursor_for_type;
  display_class->get_cursor_for_name = _gdk_quartz_display_get_cursor_for_name;
  display_class->get_cursor_for_surface = _gdk_quartz_display_get_cursor_for_surface;

  display_class->get_app_launch_context = NULL; /* FIXME */
  display_class->before_process_all_updates = _gdk_quartz_display_before_process_all_updates;
  display_class->after_process_all_updates = _gdk_quartz_display_after_process_all_updates;
  display_class->get_next_serial = gdk_quartz_display_get_next_serial;
  display_class->notify_startup_complete = gdk_quartz_display_notify_startup_complete;
  display_class->event_data_copy = _gdk_quartz_display_event_data_copy;
  display_class->event_data_free = _gdk_quartz_display_event_data_free;
  display_class->create_window_impl = _gdk_quartz_display_create_window_impl;
  display_class->get_keymap = _gdk_quartz_display_get_keymap;
  display_class->push_error_trap = NULL; /* FIXME */
  display_class->pop_error_trap = NULL; /* FIXME */

  display_class->get_selection_owner = _gdk_quartz_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_quartz_display_set_selection_owner;
  display_class->send_selection_notify = NULL; /* FIXME */
  display_class->get_selection_property = _gdk_quartz_display_get_selection_property;
  display_class->convert_selection = _gdk_quartz_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_quartz_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_quartz_display_utf8_to_string_target;

//  display_class->get_default_seat = NULL; /* FIXME */

  display_class->get_n_monitors = gdk_quartz_display_get_n_monitors;
  display_class->get_monitor = gdk_quartz_display_get_monitor;
  display_class->get_primary_monitor = gdk_quartz_display_get_primary_monitor;
  display_class->get_monitor_at_window = NULL; /* FIXME */

  ProcessSerialNumber psn = { 0, kCurrentProcess };

  /* Make the current process a foreground application, i.e. an app
   * with a user interface, in case we're not running from a .app bundle
   */
  TransformProcessType (&psn, kProcessTransformToForegroundApplication);
}
