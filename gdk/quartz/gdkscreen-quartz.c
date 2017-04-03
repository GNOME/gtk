/* gdkscreen-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
 * Copyright (C) 2009,2010  Kristian Rietveld  <kris@gtk.org>
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

#include "gdkprivate-quartz.h"
#include "gdkdisplay-quartz.h"
#include "gdkmonitor-quartz.h"
 

/* A couple of notes about this file are in order.  In GDK, a
 * GdkScreen can contain multiple monitors.  A GdkScreen has an
 * associated root window, in which the monitors are placed.  The
 * root window "spans" all monitors.  The origin is at the top-left
 * corner of the root window.
 *
 * Cocoa works differently.  The system has a "screen" (NSScreen) for
 * each monitor that is connected (note the conflicting definitions
 * of screen).  The screen containing the menu bar is screen 0 and the
 * bottom-left corner of this screen is the origin of the "monitor
 * coordinate space".  All other screens are positioned according to this
 * origin.  If the menu bar is on a secondary screen (for example on
 * a monitor hooked up to a laptop), then this screen is screen 0 and
 * other monitors will be positioned according to the "secondary screen".
 * The main screen is the monitor that shows the window that is currently
 * active (has focus), the position of the menu bar does not have influence
 * on this!
 *
 * Upon start up and changes in the layout of screens, we calculate the
 * size of the GdkScreen root window that is needed to be able to place
 * all monitors in the root window.  Once that size is known, we iterate
 * over the monitors and translate their Cocoa position to a position
 * in the root window of the GdkScreen.  This happens below in the
 * function gdk_quartz_screen_calculate_layout().
 *
 * A Cocoa coordinate is always relative to the origin of the monitor
 * coordinate space.  Such coordinates are mapped to their respective
 * position in the GdkScreen root window (_gdk_quartz_window_xy_to_gdk_xy)
 * and vice versa (_gdk_quartz_window_gdk_xy_to_xy).  Both functions can
 * be found in gdkwindow-quartz.c.  Note that Cocoa coordinates can have
 * negative values (in case a monitor is located left or below of screen 0),
 * but GDK coordinates can *not*!
 */

static void  gdk_quartz_screen_dispose          (GObject         *object);
static void  gdk_quartz_screen_finalize         (GObject         *object);
static void  gdk_quartz_screen_calculate_layout (GdkQuartzScreen *screen);

static void display_reconfiguration_callback (CGDirectDisplayID            display,
                                              CGDisplayChangeSummaryFlags  flags,
                                              void                        *userInfo);

static gint get_mm_from_pixels (NSScreen *screen, int pixels);

G_DEFINE_TYPE (GdkQuartzScreen, gdk_quartz_screen, GDK_TYPE_SCREEN);

static void
gdk_quartz_screen_init (GdkQuartzScreen *quartz_screen)
{
  GdkScreen *screen = GDK_SCREEN (quartz_screen);
  NSDictionary *dd = [[[NSScreen screens] objectAtIndex:0] deviceDescription];
  NSSize size = [[dd valueForKey:NSDeviceResolution] sizeValue];

  _gdk_screen_set_resolution (screen, size.width);

  gdk_quartz_screen_calculate_layout (quartz_screen);

  CGDisplayRegisterReconfigurationCallback (display_reconfiguration_callback,
                                            screen);

  quartz_screen->emit_monitors_changed = FALSE;
}

static void
gdk_quartz_screen_dispose (GObject *object)
{
  GdkQuartzScreen *screen = GDK_QUARTZ_SCREEN (object);

  if (screen->screen_changed_id)
    {
      g_source_remove (screen->screen_changed_id);
      screen->screen_changed_id = 0;
    }

  CGDisplayRemoveReconfigurationCallback (display_reconfiguration_callback,
                                          screen);

  G_OBJECT_CLASS (gdk_quartz_screen_parent_class)->dispose (object);
}

static void
gdk_quartz_screen_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_quartz_screen_parent_class)->finalize (object);
}

/* Protocol to build cleanly for OSX < 10.7 */
@protocol ScaleFactor
- (CGFloat) backingScaleFactor;
@end

static void
gdk_quartz_screen_calculate_layout (GdkQuartzScreen *screen)
{
  NSArray *array;
  int i;
  int max_x, max_y;
  GdkDisplay *display = gdk_screen_get_display (GDK_SCREEN (screen));
  GdkQuartzDisplay *display_quartz = GDK_QUARTZ_DISPLAY (display);

  g_ptr_array_free (display_quartz->monitors, TRUE);
  display_quartz->monitors = g_ptr_array_new_with_free_func (g_object_unref);

  GDK_QUARTZ_ALLOC_POOL;

  array = [NSScreen screens];

  screen->width = 0;
  screen->height = 0;
  screen->min_x = 0;
  screen->min_y = 0;
  max_x = max_y = 0;

  /* We determine the minimum and maximum x and y coordinates
   * covered by the monitors.  From this we can deduce the width
   * and height of the root screen.
   */
  for (i = 0; i < [array count]; i++)
    {
      GdkQuartzMonitor *monitor = g_object_new (GDK_TYPE_QUARTZ_MONITOR,
                                                "display", display,
                                                NULL);
      g_ptr_array_add (display_quartz->monitors, monitor);
      monitor->nsscreen = [array objectAtIndex:i];

      NSRect rect = [[array objectAtIndex:i] frame];

      screen->min_x = MIN (screen->min_x, rect.origin.x);
      max_x = MAX (max_x, rect.origin.x + rect.size.width);

      screen->min_y = MIN (screen->min_y, rect.origin.y);
      max_y = MAX (max_y, rect.origin.y + rect.size.height);
    }

  screen->width = max_x - screen->min_x;
  screen->height = max_y - screen->min_y;

  for (i = 0; i < [array count] ; i++)
    {
      NSScreen *nsscreen;
      NSRect rect;
      GdkMonitor *monitor;

      monitor = GDK_MONITOR(display_quartz->monitors->pdata[i]);
      nsscreen = [array objectAtIndex:i];
      rect = [nsscreen frame];

      monitor->geometry.x = rect.origin.x - screen->min_x;
      monitor->geometry.y
          = screen->height - (rect.origin.y + rect.size.height) + screen->min_y;
      monitor->geometry.width = rect.size.width;
      monitor->geometry.height = rect.size.height;
      if (gdk_quartz_osx_version() >= GDK_OSX_LION)
        monitor->scale_factor = [(id <ScaleFactor>) nsscreen backingScaleFactor];
      else
        monitor->scale_factor = 1;
      monitor->width_mm = get_mm_from_pixels(nsscreen, monitor->geometry.width);
      monitor->height_mm = get_mm_from_pixels(nsscreen, monitor->geometry.height);
      monitor->refresh_rate = 0; // unknown
      monitor->manufacturer = NULL; // unknown
      monitor->model = NULL; // unknown
      monitor->subpixel_layout = GDK_SUBPIXEL_LAYOUT_UNKNOWN; // unknown
    }

  GDK_QUARTZ_RELEASE_POOL;
}

void
_gdk_quartz_screen_update_window_sizes (GdkScreen *screen)
{
  GList *windows, *list;

  /* The size of the root window is so that it can contain all
   * monitors attached to this machine.  The monitors are laid out
   * within this root window.  We calculate the size of the root window
   * and the positions of the different monitors in gdkscreen-quartz.c.
   *
   * This data is updated when the monitor configuration is changed.
   */

  /* FIXME: At some point, fetch the root window from GdkScreen.  But
   * on OS X will we only have a single root window anyway.
   */
  _gdk_root->x = 0;
  _gdk_root->y = 0;
  _gdk_root->abs_x = 0;
  _gdk_root->abs_y = 0;

  windows = gdk_screen_get_toplevel_windows (screen);

  for (list = windows; list; list = list->next)
    _gdk_quartz_window_update_position (list->data);

  g_list_free (windows);
}

static void
process_display_reconfiguration (GdkQuartzScreen *screen)
{
  gdk_quartz_screen_calculate_layout (GDK_QUARTZ_SCREEN (screen));

  _gdk_quartz_screen_update_window_sizes (GDK_SCREEN (screen));

  if (screen->emit_monitors_changed)
    {
      g_signal_emit_by_name (screen, "monitors-changed");
      screen->emit_monitors_changed = FALSE;
    }
}

static gboolean
screen_changed_idle (gpointer data)
{
  GdkQuartzScreen *screen = data;

  process_display_reconfiguration (data);

  screen->screen_changed_id = 0;

  return FALSE;
}

static void
display_reconfiguration_callback (CGDirectDisplayID            display,
                                  CGDisplayChangeSummaryFlags  flags,
                                  void                        *userInfo)
{
  GdkQuartzScreen *screen = userInfo;

  if (flags & kCGDisplayBeginConfigurationFlag)
    {
      /* Ignore the begin configuration signal. */
      return;
    }
  else
    {
      /* We save information about the changes, so we can emit
       * ::monitors-changed when appropriate.  This signal must be
       * emitted when the number, size of position of one of the
       * monitors changes.
       */
      if (flags & kCGDisplayMovedFlag
          || flags & kCGDisplayAddFlag
          || flags & kCGDisplayRemoveFlag
          || flags & kCGDisplayEnabledFlag
          || flags & kCGDisplayDisabledFlag)
        screen->emit_monitors_changed = TRUE;

      /* At this point Cocoa does not know about the new screen data
       * yet, so we delay our refresh into an idle handler.
       */
      if (!screen->screen_changed_id)
        {
          screen->screen_changed_id = gdk_threads_add_idle (screen_changed_idle,
                                                            screen);
          g_source_set_name_by_id (screen->screen_changed_id, "[gtk+] screen_changed_idle");
        }
    }
}

static GdkDisplay *
gdk_quartz_screen_get_display (GdkScreen *screen)
{
  return _gdk_display;
}

static GdkWindow *
gdk_quartz_screen_get_root_window (GdkScreen *screen)
{
  return _gdk_root;
}

static gint
get_mm_from_pixels (NSScreen *screen, int pixels)
{
  const float mm_per_inch = 25.4;
  NSDictionary *dd = [[[NSScreen screens] objectAtIndex:0] deviceDescription];
  NSSize size = [[dd valueForKey:NSDeviceResolution] sizeValue];
  float dpi = size.width;
  return (pixels / dpi) * mm_per_inch;
}

static void
gdk_quartz_screen_class_init (GdkQuartzScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->dispose = gdk_quartz_screen_dispose;
  object_class->finalize = gdk_quartz_screen_finalize;

  screen_class->get_display = gdk_quartz_screen_get_display;
  screen_class->get_root_window = gdk_quartz_screen_get_root_window;
  screen_class->get_setting = _gdk_quartz_screen_get_setting;
}
