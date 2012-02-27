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

G_DEFINE_TYPE (GdkQuartzScreen, gdk_quartz_screen, GDK_TYPE_SCREEN);

static void
gdk_quartz_screen_init (GdkQuartzScreen *quartz_screen)
{
  GdkScreen *screen = GDK_SCREEN (quartz_screen);
  NSScreen *nsscreen;

  nsscreen = [[NSScreen screens] objectAtIndex:0];
  gdk_screen_set_resolution (screen,
                             72.0 * [nsscreen userSpaceScaleFactor]);

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
gdk_quartz_screen_screen_rects_free (GdkQuartzScreen *screen)
{
  screen->n_screens = 0;

  if (screen->screen_rects)
    {
      g_free (screen->screen_rects);
      screen->screen_rects = NULL;
    }
}

static void
gdk_quartz_screen_finalize (GObject *object)
{
  GdkQuartzScreen *screen = GDK_QUARTZ_SCREEN (object);

  gdk_quartz_screen_screen_rects_free (screen);
}


static void
gdk_quartz_screen_calculate_layout (GdkQuartzScreen *screen)
{
  NSArray *array;
  int i;
  int max_x, max_y;

  GDK_QUARTZ_ALLOC_POOL;

  gdk_quartz_screen_screen_rects_free (screen);

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
      NSRect rect = [[array objectAtIndex:i] frame];

      screen->min_x = MIN (screen->min_x, rect.origin.x);
      max_x = MAX (max_x, rect.origin.x + rect.size.width);

      screen->min_y = MIN (screen->min_y, rect.origin.y);
      max_y = MAX (max_y, rect.origin.y + rect.size.height);
    }

  screen->width = max_x - screen->min_x;
  screen->height = max_y - screen->min_y;

  screen->n_screens = [array count];
  screen->screen_rects = g_new0 (GdkRectangle, screen->n_screens);

  for (i = 0; i < screen->n_screens; i++)
    {
      NSScreen *nsscreen;
      NSRect rect;

      nsscreen = [array objectAtIndex:i];
      rect = [nsscreen frame];

      screen->screen_rects[i].x = rect.origin.x - screen->min_x;
      screen->screen_rects[i].y
          = screen->height - (rect.origin.y + rect.size.height) + screen->min_y;
      screen->screen_rects[i].width = rect.size.width;
      screen->screen_rects[i].height = rect.size.height;
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
  _gdk_root->width = gdk_screen_get_width (screen);
  _gdk_root->height = gdk_screen_get_height (screen);

  windows = gdk_screen_get_toplevel_windows (screen);

  for (list = windows; list; list = list->next)
    _gdk_quartz_window_update_position (list->data);

  g_list_free (windows);
}

static void
process_display_reconfiguration (GdkQuartzScreen *screen)
{
  int width, height;

  width = gdk_screen_get_width (GDK_SCREEN (screen));
  height = gdk_screen_get_height (GDK_SCREEN (screen));

  gdk_quartz_screen_calculate_layout (GDK_QUARTZ_SCREEN (screen));

  _gdk_quartz_screen_update_window_sizes (GDK_SCREEN (screen));

  if (screen->emit_monitors_changed)
    {
      g_signal_emit_by_name (screen, "monitors-changed");
      screen->emit_monitors_changed = FALSE;
    }

  if (width != gdk_screen_get_width (GDK_SCREEN (screen))
      || height != gdk_screen_get_height (GDK_SCREEN (screen)))
    g_signal_emit_by_name (screen, "size-changed");
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
        screen->screen_changed_id = gdk_threads_add_idle (screen_changed_idle,
                                                          screen);
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
gdk_quartz_screen_get_number (GdkScreen *screen)
{
  return 0;
}

gchar *
_gdk_windowing_substitute_screen_number (const gchar *display_name,
					 int          screen_number)
{
  if (screen_number != 0)
    return NULL;

  return g_strdup (display_name);
}

static gint
gdk_quartz_screen_get_width (GdkScreen *screen)
{
  return GDK_QUARTZ_SCREEN (screen)->width;
}

static gint
gdk_quartz_screen_get_height (GdkScreen *screen)
{
  return GDK_QUARTZ_SCREEN (screen)->height;
}

static gint
get_mm_from_pixels (NSScreen *screen, int pixels)
{
  /* userSpaceScaleFactor is in "pixels per point", 
   * 72 is the number of points per inch, 
   * and 25.4 is the number of millimeters per inch.
   */
#if MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_3
  float dpi = [screen userSpaceScaleFactor] * 72.0;
#else
  float dpi = 96.0 / 72.0;
#endif

  return (pixels / dpi) * 25.4;
}

static NSScreen *
get_nsscreen_for_monitor (gint monitor_num)
{
  NSArray *array;
  NSScreen *screen;

  GDK_QUARTZ_ALLOC_POOL;

  array = [NSScreen screens];
  screen = [array objectAtIndex:monitor_num];

  GDK_QUARTZ_RELEASE_POOL;

  return screen;
}

static gint
gdk_quartz_screen_get_width_mm (GdkScreen *screen)
{
  return get_mm_from_pixels (get_nsscreen_for_monitor (0),
                             GDK_QUARTZ_SCREEN (screen)->width);
}

static gint
gdk_quartz_screen_get_height_mm (GdkScreen *screen)
{
  return get_mm_from_pixels (get_nsscreen_for_monitor (0),
                             GDK_QUARTZ_SCREEN (screen)->height);
}

static gint
gdk_quartz_screen_get_n_monitors (GdkScreen *screen)
{
  return GDK_QUARTZ_SCREEN (screen)->n_screens;
}

static gint
gdk_quartz_screen_get_primary_monitor (GdkScreen *screen)
{
  return 0;
}

static gint
gdk_quartz_screen_get_monitor_width_mm (GdkScreen *screen,
                                        gint       monitor_num)
{
  return get_mm_from_pixels (get_nsscreen_for_monitor (monitor_num),
                             GDK_QUARTZ_SCREEN (screen)->screen_rects[monitor_num].width);
}

static gint
gdk_quartz_screen_get_monitor_height_mm (GdkScreen *screen,
                                         gint       monitor_num)
{
  return get_mm_from_pixels (get_nsscreen_for_monitor (monitor_num),
                             GDK_QUARTZ_SCREEN (screen)->screen_rects[monitor_num].height);
}

static gchar *
gdk_quartz_screen_get_monitor_plug_name (GdkScreen *screen,
                                         gint       monitor_num)
{
  /* FIXME: Is there some useful name we could use here? */
  return NULL;
}

static void
gdk_quartz_screen_get_monitor_geometry (GdkScreen    *screen,
                                        gint          monitor_num,
                                        GdkRectangle *dest)
{
  *dest = GDK_QUARTZ_SCREEN (screen)->screen_rects[monitor_num];
}

static void
gdk_quartz_screen_get_monitor_workarea (GdkScreen    *screen,
                                        gint          monitor_num,
                                        GdkRectangle *dest)
{
  GdkQuartzScreen *quartz_screen = GDK_QUARTZ_SCREEN (screen);
  NSArray *array;
  NSScreen *nsscreen;
  NSRect rect;

  GDK_QUARTZ_ALLOC_POOL;

  array = [NSScreen screens];
  nsscreen = [array objectAtIndex:monitor_num];
  rect = [nsscreen visibleFrame];

  dest->x = rect.origin.x - quartz_screen->min_x;
  dest->y = quartz_screen->height - (rect.origin.y + rect.size.height) + quartz_screen->min_y;
  dest->width = rect.size.width;
  dest->height = rect.size.height;

  GDK_QUARTZ_RELEASE_POOL;
}

static gchar *
gdk_quartz_screen_make_display_name (GdkScreen *screen)
{
  return g_strdup (gdk_display_get_name (_gdk_display));
}

static GdkWindow *
gdk_quartz_screen_get_active_window (GdkScreen *screen)
{
  return NULL;
}

static GList *
gdk_quartz_screen_get_window_stack (GdkScreen *screen)
{
  return NULL;
}

static gboolean
gdk_quartz_screen_is_composited (GdkScreen *screen)
{
  return TRUE;
}

static void
gdk_quartz_screen_class_init (GdkQuartzScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->dispose = gdk_quartz_screen_dispose;
  object_class->finalize = gdk_quartz_screen_finalize;

  screen_class->get_display = gdk_quartz_screen_get_display;
  screen_class->get_width = gdk_quartz_screen_get_width;
  screen_class->get_height = gdk_quartz_screen_get_height;
  screen_class->get_width_mm = gdk_quartz_screen_get_width_mm;
  screen_class->get_height_mm = gdk_quartz_screen_get_height_mm;
  screen_class->get_number = gdk_quartz_screen_get_number;
  screen_class->get_root_window = gdk_quartz_screen_get_root_window;
  screen_class->get_n_monitors = gdk_quartz_screen_get_n_monitors;
  screen_class->get_primary_monitor = gdk_quartz_screen_get_primary_monitor;
  screen_class->get_monitor_width_mm = gdk_quartz_screen_get_monitor_width_mm;
  screen_class->get_monitor_height_mm = gdk_quartz_screen_get_monitor_height_mm;
  screen_class->get_monitor_plug_name = gdk_quartz_screen_get_monitor_plug_name;
  screen_class->get_monitor_geometry = gdk_quartz_screen_get_monitor_geometry;
  screen_class->get_monitor_workarea = gdk_quartz_screen_get_monitor_workarea;
  screen_class->is_composited = gdk_quartz_screen_is_composited;
  screen_class->make_display_name = gdk_quartz_screen_make_display_name;
  screen_class->get_active_window = gdk_quartz_screen_get_active_window;
  screen_class->get_window_stack = gdk_quartz_screen_get_window_stack;
  screen_class->broadcast_client_message = _gdk_quartz_screen_broadcast_client_message;
  screen_class->get_setting = _gdk_quartz_screen_get_setting;
  screen_class->get_rgba_visual = _gdk_quartz_screen_get_rgba_visual;
  screen_class->get_system_visual = _gdk_quartz_screen_get_system_visual;
  screen_class->visual_get_best_depth = _gdk_quartz_screen_visual_get_best_depth;
  screen_class->visual_get_best_type = _gdk_quartz_screen_visual_get_best_type;
  screen_class->visual_get_best = _gdk_quartz_screen_visual_get_best;
  screen_class->visual_get_best_with_depth = _gdk_quartz_screen_visual_get_best_with_depth;
  screen_class->visual_get_best_with_type = _gdk_quartz_screen_visual_get_best_with_type;
  screen_class->visual_get_best_with_both = _gdk_quartz_screen_visual_get_best_with_both;
  screen_class->query_depths = _gdk_quartz_screen_query_depths;
  screen_class->query_visual_types = _gdk_quartz_screen_query_visual_types;
  screen_class->list_visuals = _gdk_quartz_screen_list_visuals;
}
