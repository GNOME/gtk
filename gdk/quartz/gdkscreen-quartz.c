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
/* GdkScreen is deprecated, but we need to support it still so silence the warnings.*/
#define GDK_DISABLE_DEPRECATION_WARNINGS 1
#include <gdk/gdk.h>

#include "gdkprivate-quartz.h"
#include "gdkdisplay-quartz.h"
#include "gdkmonitor-quartz.h"
#include "gdkinternal-quartz.h"

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

static void  gdk_quartz_screen_dispose          (GObject          *object);
static void  gdk_quartz_screen_finalize         (GObject          *object);
static void  gdk_quartz_screen_calculate_layout (GdkQuartzScreen  *screen,
                                                 GdkQuartzDisplay *display);
static void  gdk_quartz_screen_reconfigure      (GdkQuartzDisplay *display,
                                                 GdkQuartzScreen  *screen);

static const double dpi = 72.0;

G_DEFINE_TYPE (GdkQuartzScreen, gdk_quartz_screen, GDK_TYPE_SCREEN);

static void
gdk_quartz_screen_init (GdkQuartzScreen *quartz_screen)
{
  GdkScreen *screen = GDK_SCREEN (quartz_screen);
  /* Screen resolution is used exclusively to pass to Pango for font
   * scaling. There's a long discussion in
   * https://bugzilla.gnome.org/show_bug.cgi?id=787867 exploring how
   * screen resolution and pangocairo-coretext interact. The summary
   * is that MacOS takes care of scaling fonts for Retina screens and
   * that while the Apple Documentation goes on about "points" they're
   * CSS points (96/in), not typeography points (72/in) and
   * pangocairo-coretext needs to default to that scaling factor.
   */

  g_signal_connect (_gdk_display, "monitors-changed",
                    G_CALLBACK (gdk_quartz_screen_reconfigure), quartz_screen);
  /* The first monitors-changed should have fired already. */
  _gdk_screen_set_resolution (screen, dpi);
  gdk_quartz_screen_calculate_layout (quartz_screen, NULL);
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
gdk_quartz_screen_calculate_layout (GdkQuartzScreen *screen,
                                    GdkQuartzDisplay *display)
{
  if (!display)
    display = GDK_QUARTZ_DISPLAY (gdk_screen_get_display (GDK_SCREEN (screen)));

/* Display geometry is the origin and size in AppKit coordinates. AppKit computes */
  screen->width = (int)trunc (display->geometry.size.width);
  screen->height = (int)trunc (display->geometry.size.height);
  screen->orig_x = -(int)trunc (display->geometry.origin.x);
  screen->orig_y = (int)trunc (display->geometry.origin.y);
  screen->mm_width = (int)trunc (display->size.width);
  screen->mm_height = (int)trunc (display->size.height);

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
    {
      if (GDK_WINDOW_TYPE(list->data) == GDK_WINDOW_OFFSCREEN)
        continue;
      _gdk_quartz_window_update_position (list->data);
    }

  g_list_free (windows);
}

static void
gdk_quartz_screen_reconfigure (GdkQuartzDisplay *display, GdkQuartzScreen *screen)
{
  int width, height;

  width = gdk_screen_get_width (GDK_SCREEN (screen));
  height = gdk_screen_get_height (GDK_SCREEN (screen));

  gdk_quartz_screen_calculate_layout (screen, display);

  _gdk_quartz_screen_update_window_sizes (GDK_SCREEN (screen));

  g_signal_emit_by_name (screen, "monitors-changed");

  if (width != gdk_screen_get_width (GDK_SCREEN (screen))
      || height != gdk_screen_get_height (GDK_SCREEN (screen)))
    g_signal_emit_by_name (screen, "size-changed");
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

static gint
gdk_quartz_screen_get_width_mm (GdkScreen *screen)
{
  return GDK_QUARTZ_SCREEN (screen)->mm_width;
}

static gint
gdk_quartz_screen_get_height_mm (GdkScreen *screen)
{
  return GDK_QUARTZ_SCREEN (screen)->mm_height;
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
