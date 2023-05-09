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
#include <gdk/gdkframeclockprivate.h>

#include "gdkprivate-quartz.h"
#include "gdkquartzscreen.h"
#include "gdkquartzwindow.h"
#include "gdkquartzdisplay.h"
#include "gdkquartzdevicemanager-core.h"
#include "gdkscreen.h"
#include "gdkmonitorprivate.h"
#include "gdkdisplaylinksource.h"
#include "gdkdisplay-quartz.h"
#include "gdkmonitor-quartz.h"
#include "gdkglcontext-quartz.h"
#include "gdkinternal-quartz.h"
#include "gdkwindow.h"

/* Note about coordinates: There are three coordinate systems at play:
 *
 * 1. Core Graphics starts at the origin at the upper right of the
 * main window (the one with the menu bar when you look at arrangement
 * in System Preferences>Displays) and increases down and to the
 * right; up and to the left are negative values of y and x
 * respectively.
 *
 * 2. AppKit (functions beginning with "NS" for NextStep) coordinates
 * also have their origin at the main window, but it's the *lower*
 * left corner and coordinates increase up and to the
 * right. Coordinates below or left of the origin are negative.
 *
 * 3. Gdk coordinates origin is at the upper left corner of the
 * imaginary rectangle enclosing all monitors and like Core Graphics
 * increase down and to the right. There are no negative coordinates.
 *
 * We need to deal with all three because AppKit's NSScreen array is
 * recomputed with new pointers whenever the monitor arrangement
 * changes so we can't cache the references it provides. CoreGraphics
 * screen IDs are constant between reboots so those are what we use to
 * map GdkMonitors and screens, but the sizes and origins must be
 * converted to Gdk coordinates to make sense to Gdk and we must
 * frequently convert between Gdk and AppKit coordinates when
 * determining the drawable area of a monitor and placing windows and
 * views (the latter containing our cairo surfaces for drawing on).
 */

static gint MONITORS_CHANGED = 0;

static void display_reconfiguration_callback (CGDirectDisplayID            display,
                                              CGDisplayChangeSummaryFlags  flags,
                                              void                        *data);

static GdkWindow *
gdk_quartz_display_get_default_group (GdkDisplay *display)
{
/* X11-only. */
  return NULL;
}

GdkDeviceManager *
_gdk_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE,
                       "display", display,
                       NULL);
}

void
_gdk_quartz_display_add_frame_callback (GdkDisplay             *display,
                                        GdkWindow              *window)
{
  GdkQuartzDisplay *display_quartz;
  GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (window->impl);

  display_quartz = GDK_QUARTZ_DISPLAY (display);

  impl->frame_link.data = window;
  impl->frame_link.next = display_quartz->windows_awaiting_frame;

  display_quartz->windows_awaiting_frame = &impl->frame_link;

  if (impl->frame_link.next == NULL)
    gdk_display_link_source_unpause ((GdkDisplayLinkSource *)display_quartz->frame_source);
}

void
_gdk_quartz_display_remove_frame_callback (GdkDisplay             *display,
                                           GdkWindow              *window)
{
  GdkQuartzDisplay *display_quartz = GDK_QUARTZ_DISPLAY (display);
  GSList *link;

  link = g_slist_find (display_quartz->windows_awaiting_frame, window);

  if (link != NULL)
    {
      display_quartz->windows_awaiting_frame =
        g_slist_remove_link (display_quartz->windows_awaiting_frame, link);
    }

  if (display_quartz->windows_awaiting_frame == NULL)
    gdk_display_link_source_pause ((GdkDisplayLinkSource *)display_quartz->frame_source);
}

static gboolean
gdk_quartz_display_frame_cb (gpointer data)
{
  GdkDisplayLinkSource *source;
  GdkQuartzDisplay *display_quartz = data;
  GSList *iter, **last_next = NULL;
  gint64 presentation_time;

  source = (GdkDisplayLinkSource *)display_quartz->frame_source;

  iter = display_quartz->windows_awaiting_frame;
  display_quartz->windows_awaiting_frame = NULL;

  if (iter == NULL)
    {
      gdk_display_link_source_pause (source);
      return G_SOURCE_CONTINUE;
    }

  presentation_time = source->presentation_time;

  for (; iter != NULL; iter = iter->next)
    {
      GdkWindow *window = iter->data;
      GdkWindowImplQuartz *impl = GDK_WINDOW_IMPL_QUARTZ (window->impl);
      GdkFrameClock *frame_clock = gdk_window_get_frame_clock (window);
      GdkFrameTimings *timings;

      /* Clear the frame_link */
      iter->data = NULL;
      if (last_next && *last_next)
        *last_next = NULL;
      last_next = &iter->next;

      if (frame_clock == NULL)
        continue;

      _gdk_frame_clock_thaw (frame_clock);

      if (impl->pending_frame_counter)
        {
          timings = gdk_frame_clock_get_timings (frame_clock, impl->pending_frame_counter);
          if (timings != NULL)
            timings->presentation_time = presentation_time - source->refresh_interval;
          impl->pending_frame_counter = 0;
        }

      timings = gdk_frame_clock_get_current_timings (frame_clock);

      if (timings != NULL)
        {
          timings->refresh_interval = source->refresh_interval;
          timings->predicted_presentation_time = source->presentation_time;
        }
    }

  return G_SOURCE_CONTINUE;
}

static void
gdk_quartz_display_init_display_link (GdkDisplay *display)
{
  GdkQuartzDisplay *display_quartz = GDK_QUARTZ_DISPLAY (display);

  display_quartz->frame_source = gdk_display_link_source_new ();
  g_source_set_callback (display_quartz->frame_source,
                         gdk_quartz_display_frame_cb,
                         display,
                         NULL);
  g_source_attach (display_quartz->frame_source, NULL);
}

GdkDisplay *
_gdk_quartz_display_open (const gchar *display_name)
{
  if (_gdk_display != NULL)
    return NULL;

  _gdk_display = g_object_new (gdk_quartz_display_get_type (), NULL);
  _gdk_display->device_manager = _gdk_device_manager_new (_gdk_display);

  _gdk_screen = g_object_new (gdk_quartz_screen_get_type (), NULL);
  _gdk_quartz_screen_init_visuals (_gdk_screen);

  _gdk_quartz_window_init_windowing (_gdk_display, _gdk_screen);

  _gdk_quartz_events_init ();

  /* Initialize application */
  [NSApplication sharedApplication];
  gdk_quartz_display_init_display_link (_gdk_display);

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
  /* Not needed. */
}

static void
gdk_quartz_display_flush (GdkDisplay *display)
{
  /* Not needed. */
}

static gboolean
gdk_quartz_display_supports_selection_notification (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  /* X11-only. */
  return FALSE;
}

static gboolean
gdk_quartz_display_request_selection_notification (GdkDisplay *display,
                                                   GdkAtom     selection)
{
  /* X11-only. */
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_clipboard_persistence (GdkDisplay *display)
{
  /* X11-only */
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_shapes (GdkDisplay *display)
{
  /* Not needed, nothing ever calls this.*/
  return FALSE;
}

static gboolean
gdk_quartz_display_supports_input_shapes (GdkDisplay *display)
{
  /* Not needed, nothign ever calls this. */
  return FALSE;
}

static void
gdk_quartz_display_store_clipboard (GdkDisplay    *display,
                                    GdkWindow     *clipboard_window,
                                    guint32        time_,
                                    const GdkAtom *targets,
                                    gint           n_targets)
{
  /* MacOS persists pasteboard items automatically, no application
   * action is required.
   */
}


static gboolean
gdk_quartz_display_supports_composite (GdkDisplay *display)
{
  /* X11-only. */
  return FALSE;
}

static gulong
gdk_quartz_display_get_next_serial (GdkDisplay *display)
{
  /* X11-only. */
  return 0;
}

static void
gdk_quartz_display_notify_startup_complete (GdkDisplay  *display,
                                            const gchar *startup_id)
{
  /* This should call finishLaunching, but doing so causes Quartz to throw
   * "_createMenuRef called with existing principal MenuRef already"
   * " associated with menu".
  [NSApp finishLaunching];
  */
}

static void
gdk_quartz_display_push_error_trap (GdkDisplay *display)
{
  /* X11-only. */
}

static gint
gdk_quartz_display_pop_error_trap (GdkDisplay *display, gboolean ignore)
{
  /* X11 only. */
  return 0;
}

/* The display monitor list comprises all of the CGDisplays connected
   to the system, some of which may not be drawable either because
   they're asleep or are mirroring another monitor. The NSScreens
   array contains only the monitors that are currently drawable and we
   use the index of the screens array placing GdkNSViews, so we'll use
   the same for determining the number of monitors and indexing them.
 */

int
get_active_displays (CGDirectDisplayID **displays)
{
  unsigned int n_displays = 0;

  CGGetActiveDisplayList (0, NULL, &n_displays);
  if (displays)
    {
      *displays = g_new0 (CGDirectDisplayID, n_displays);
      CGGetActiveDisplayList (n_displays, *displays, &n_displays);
    }

  return n_displays;
}

static inline GdkRectangle
cgrect_to_gdkrect (CGRect cgrect)
{
  GdkRectangle gdkrect = {(int)trunc (cgrect.origin.x),
                          (int)trunc (cgrect.origin.y),
                          (int)trunc (cgrect.size.width),
                          (int)trunc (cgrect.size.height)};
  return gdkrect;
}

static void
configure_monitor (GdkMonitor       *monitor,
                   GdkQuartzDisplay *display)
{
  GdkQuartzMonitor *quartz_monitor = GDK_QUARTZ_MONITOR (monitor);
  CGSize disp_size = CGDisplayScreenSize (quartz_monitor->id);
  gint width = (int)trunc (disp_size.width);
  gint height = (int)trunc (disp_size.height);
  CGRect disp_bounds = CGDisplayBounds (quartz_monitor->id);
  CGRect main_bounds = CGDisplayBounds (CGMainDisplayID());
  /* Change origin to Gdk coordinates. */
  disp_bounds.origin.x = disp_bounds.origin.x + display->geometry.origin.x;
  disp_bounds.origin.y =
    display->geometry.origin.y - main_bounds.size.height + disp_bounds.origin.y;
  GdkRectangle disp_geometry = cgrect_to_gdkrect (disp_bounds);
  CGDisplayModeRef mode = CGDisplayCopyDisplayMode (quartz_monitor->id);
  gint refresh_rate = (int)trunc (CGDisplayModeGetRefreshRate (mode));

  monitor->width_mm = width;
  monitor->height_mm = height;
  monitor->geometry = disp_geometry;
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 1080
  if (mode && gdk_quartz_osx_version () >= GDK_OSX_MOUNTAIN_LION)
  {
    monitor->scale_factor = CGDisplayModeGetPixelWidth (mode) / CGDisplayModeGetWidth (mode);
    CGDisplayModeRelease (mode);
  }
  else
#endif
    monitor->scale_factor = 1;
  monitor->refresh_rate = refresh_rate;
  monitor->subpixel_layout = GDK_SUBPIXEL_LAYOUT_UNKNOWN;
}

static void
display_rect (GdkQuartzDisplay *display)
{
  uint32_t disp, n_displays = 0;
  float min_x = 0.0, max_x = 0.0, min_y = 0.0, max_y = 0.0;
  float min_x_mm = 0.0, max_x_mm = 0.0, min_y_mm = 0.0, max_y_mm = 0.0;
  float main_height;
  CGDirectDisplayID *displays;

  n_displays = get_active_displays (&displays);
  for (disp = 0; disp < n_displays; ++disp)
    {
      CGRect bounds = CGDisplayBounds (displays[disp]);
      CGSize disp_size = CGDisplayScreenSize (displays[disp]);
      float x_scale = disp_size.width / bounds.size.width;
      float y_scale = disp_size.height / bounds.size.height;
      if (disp == 0)
        main_height = bounds.size.height;
      min_x = MIN (min_x, bounds.origin.x);
      min_y = MIN (min_y, bounds.origin.y);

      max_x = MAX (max_x, bounds.origin.x + bounds.size.width);
      max_y = MAX (max_y, bounds.origin.y + bounds.size.height);
      min_x_mm = MIN (min_x_mm, bounds.origin.x / x_scale);
      min_y_mm = MIN (min_y_mm, main_height - (bounds.size.height + bounds.origin.y) / y_scale);
      max_x_mm = MAX (max_x_mm, (bounds.origin.x + bounds.size.width) / x_scale);
      max_y_mm = MAX (max_y_mm, (bounds.origin.y + bounds.size.height) / y_scale);

    }
  g_free (displays);
  /* Adjusts the origin to AppKit coordinates. */
  display->geometry = NSMakeRect (-min_x, main_height - min_y,
                                  max_x - min_x, max_y - min_y);
  display->size = NSMakeSize (max_x_mm - min_x_mm, max_y_mm - min_y_mm);
}

static gboolean
same_monitor (gconstpointer a, gconstpointer b)
{
  GdkQuartzMonitor *mon_a = GDK_QUARTZ_MONITOR (a);
  CGDirectDisplayID disp_id = (CGDirectDisplayID)GPOINTER_TO_INT (b);
  if (!mon_a)
    return FALSE;
  return mon_a->id == disp_id;
}

static void
display_reconfiguration_callback (CGDirectDisplayID            cg_display,
                                  CGDisplayChangeSummaryFlags  flags,
                                  void                        *data)
{
  GdkQuartzDisplay *display = data;

  /* Ignore the begin configuration signal. */
  if (flags & kCGDisplayBeginConfigurationFlag)
      return;

  if (flags & (kCGDisplayMovedFlag | kCGDisplayAddFlag | kCGDisplayEnabledFlag |
               kCGDisplaySetMainFlag | kCGDisplayMirrorFlag |
               kCGDisplayUnMirrorFlag))
    {
      GdkQuartzMonitor *monitor = NULL;
      guint index;

      if (!g_ptr_array_find_with_equal_func (display->monitors,
                                             GINT_TO_POINTER (cg_display),
                                             same_monitor,
                                             &index))
        {
          monitor = g_object_new (GDK_TYPE_QUARTZ_MONITOR,
                                  "display", display, NULL);
          monitor->id = cg_display;
          g_ptr_array_add (display->monitors, monitor);
          display_rect (display);
          configure_monitor (GDK_MONITOR (monitor), display);
          gdk_display_monitor_added (GDK_DISPLAY (display),
                                     GDK_MONITOR (monitor));
        }
      else
        {
          monitor = g_ptr_array_index (display->monitors, index);
          display_rect (display);
          configure_monitor (GDK_MONITOR (monitor), display);
        }
    }
  else if (flags & (kCGDisplayRemoveFlag |  kCGDisplayDisabledFlag))
    {
      guint index;

      if (g_ptr_array_find_with_equal_func (display->monitors,
                                            GINT_TO_POINTER (cg_display),
                                            same_monitor,
                                            &index))
        {
          GdkQuartzMonitor *monitor = g_ptr_array_index (display->monitors,
                                                         index);
          gdk_display_monitor_removed (GDK_DISPLAY (display),
                                       GDK_MONITOR (monitor));
          g_ptr_array_remove_fast (display->monitors, monitor);
        }
    }

  g_signal_emit (display, MONITORS_CHANGED, 0);
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
  int n_displays = gdk_quartz_display_get_n_monitors (display);

  if (monitor_num >= 0 && monitor_num < n_displays)
    return g_ptr_array_index (quartz_display->monitors, monitor_num);

  return NULL;
}

static GdkMonitor *
gdk_quartz_display_get_primary_monitor (GdkDisplay *display)
{
  GdkQuartzDisplay *quartz_display = GDK_QUARTZ_DISPLAY (display);
  CGDirectDisplayID primary_id = CGMainDisplayID ();
  GdkMonitor *monitor = NULL;
  guint index;

  if (g_ptr_array_find_with_equal_func (quartz_display->monitors,
                                        GINT_TO_POINTER (primary_id),
                                        same_monitor, &index))
    monitor = g_ptr_array_index (quartz_display->monitors, index);

  return monitor;
}

static GdkMonitor *
gdk_quartz_display_get_monitor_at_window (GdkDisplay *display,
                                          GdkWindow *window)
{
  GdkWindowImplQuartz *impl = NULL;
  NSWindow *nswindow = NULL;
  NSScreen *screen = NULL;
  GdkMonitor *monitor = NULL;
  GdkWindow *onscreen_window = window;

  /*
   * This stops crashes when there is no NSWindow available on
   * an offscreen window which occurs for children of children
   * of an onscreen window (children of an onscreen window do
   * have NSWindow set)
   * https://gitlab.gnome.org/GNOME/gimp/-/issues/7608
   */
  while (onscreen_window && onscreen_window->window_type == GDK_WINDOW_OFFSCREEN)
    onscreen_window = onscreen_window->parent;

  if (!onscreen_window)
    return NULL;

  impl = GDK_WINDOW_IMPL_QUARTZ (onscreen_window->impl);
  nswindow = impl->toplevel;
  screen = [nswindow screen];

  if (screen)
  {
    GdkQuartzDisplay *quartz_display = GDK_QUARTZ_DISPLAY (display);
    guint index;
    CGDirectDisplayID disp_id =
      [[[screen deviceDescription]
        objectForKey: @"NSScreenNumber"] unsignedIntValue];
    if (g_ptr_array_find_with_equal_func (quartz_display->monitors,
                                          GINT_TO_POINTER (disp_id),
                                          same_monitor, &index))
      monitor = g_ptr_array_index (quartz_display->monitors, index);
  }
  if (!monitor)
    {
      GdkRectangle rect = cgrect_to_gdkrect (NSRectToCGRect ([nswindow frame]));
      monitor = gdk_display_get_monitor_at_point (display,
                                                 rect.x + rect.width/2,
                                                 rect.y + rect.height /2);
    }
  return monitor;
}

G_DEFINE_TYPE (GdkQuartzDisplay, gdk_quartz_display, GDK_TYPE_DISPLAY)

static void
gdk_quartz_display_init (GdkQuartzDisplay *display)
{
  uint32_t n_displays = 0, disp;
  CGDirectDisplayID *displays;

  display_rect(display); /* Initialize the overall display coordinates. */
  n_displays = get_active_displays (&displays);
  display->monitors = g_ptr_array_new_full (n_displays, g_object_unref);
  for (disp = 0; disp < n_displays; ++disp)
    {
      GdkQuartzMonitor *monitor = g_object_new (GDK_TYPE_QUARTZ_MONITOR,
                                                       "display", display, NULL);
      monitor->id = displays[disp];
      g_ptr_array_add (display->monitors, monitor);
      configure_monitor (GDK_MONITOR (monitor), display);
    }
  g_free (displays);
  CGDisplayRegisterReconfigurationCallback (display_reconfiguration_callback,
                                            display);
  /* So that monitors changed will keep display->geometry syncronized. */
  g_signal_emit (display, MONITORS_CHANGED, 0);
}

static void
gdk_quartz_display_dispose (GObject *object)
{
  GdkQuartzDisplay *quartz_display = GDK_QUARTZ_DISPLAY (object);

  g_ptr_array_free (quartz_display->monitors, TRUE);
  CGDisplayRemoveReconfigurationCallback (display_reconfiguration_callback,
                                          quartz_display);

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

  /* display_class->get_app_launch_context = NULL; Has default. */
  display_class->before_process_all_updates = _gdk_quartz_display_before_process_all_updates;
  display_class->after_process_all_updates = _gdk_quartz_display_after_process_all_updates;
  display_class->get_next_serial = gdk_quartz_display_get_next_serial;
  display_class->notify_startup_complete = gdk_quartz_display_notify_startup_complete;
  display_class->event_data_copy = _gdk_quartz_display_event_data_copy;
  display_class->event_data_free = _gdk_quartz_display_event_data_free;
  display_class->create_window_impl = _gdk_quartz_display_create_window_impl;
  display_class->get_keymap = _gdk_quartz_display_get_keymap;
  display_class->push_error_trap = gdk_quartz_display_push_error_trap;
  display_class->pop_error_trap = gdk_quartz_display_pop_error_trap;

  display_class->get_selection_owner = _gdk_quartz_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_quartz_display_set_selection_owner;
  display_class->send_selection_notify = NULL; /* Ignore. X11 stuff removed in master.  */
  display_class->get_selection_property = _gdk_quartz_display_get_selection_property;
  display_class->convert_selection = _gdk_quartz_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_quartz_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_quartz_display_utf8_to_string_target;

/* display_class->get_default_seat; The parent class default works fine. */

  display_class->get_n_monitors = gdk_quartz_display_get_n_monitors;
  display_class->get_monitor = gdk_quartz_display_get_monitor;
  display_class->get_primary_monitor = gdk_quartz_display_get_primary_monitor;
  display_class->get_monitor_at_window = gdk_quartz_display_get_monitor_at_window;
  display_class->make_gl_context_current = gdk_quartz_display_make_gl_context_current;

  /**
   * GdkQuartzDisplay::monitors-changed:
   * @display: The object on which the signal is emitted
   *
   * The ::monitors-changed signal is emitted whenever the arrangement
   * of the monitors changes, either because of the addition or
   * removal of a monitor or because of some other configuration
   * change in System Preferences>Displays including a resolution
   * change or a position change. Note that enabling or disabling
   * mirroring will result in the addition or removal of the mirror
   * monitor(s).
   */
  MONITORS_CHANGED =
    g_signal_new (g_intern_static_string ("monitors-changed"),
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0, NULL);

  ProcessSerialNumber psn = { 0, kCurrentProcess };

  /* Make the current process a foreground application, i.e. an app
   * with a user interface, in case we're not running from a .app bundle
   */
  TransformProcessType (&psn, kProcessTransformToForegroundApplication);
}
