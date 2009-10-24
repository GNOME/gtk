/* gdkscreen-quartz.c
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "gdk.h"
#include "gdkprivate-quartz.h"
 
/* FIXME: If we want to do it properly, this should be stored
 * in a proper GdkScreen subclass.
 */
static GdkColormap *default_colormap = NULL;
static int n_screens = 0;
static GdkRectangle *screen_rects = NULL;

static guint screen_changed_id = 0;


static void
screen_rects_init (void)
{
  NSArray *array;
  NSRect largest_rect;
  int i;

  GDK_QUARTZ_ALLOC_POOL;

  array = [NSScreen screens];

  n_screens = [array count];
  screen_rects = g_new0 (GdkRectangle, n_screens);

  /* FIXME: as stated above the get_width() and get_height() functions
   * in this file, we only support horizontal screen layouts for now.
   */

  /* Find the monitor with the largest height.  All monitors should be
   * offset to this one in the GDK screen space instead of offset to
   * the screen with the menu bar.
   */
  largest_rect = [[array objectAtIndex:0] frame];
  for (i = 1; i < [array count]; i++)
    {
      NSRect rect = [[array objectAtIndex:i] frame];

      if (rect.size.height > largest_rect.size.height)
        largest_rect = [[array objectAtIndex:i] frame];
    }

  for (i = 0; i < n_screens; i++)
    {
      NSScreen *nsscreen;
      NSRect rect;

      nsscreen = [array objectAtIndex:i];
      rect = [nsscreen frame];

      screen_rects[i].x = rect.origin.x;
      screen_rects[i].width = rect.size.width;
      screen_rects[i].height = rect.size.height;

      if (largest_rect.size.height - rect.size.height == 0)
        screen_rects[i].y = 0;
      else
        screen_rects[i].y = largest_rect.size.height - rect.size.height + largest_rect.origin.y;
    }

  GDK_QUARTZ_RELEASE_POOL;
}

static void
screen_rects_free (void)
{
  n_screens = 0;

  g_free (screen_rects);
  screen_rects = NULL;
}


static void
process_display_reconfiguration (void)
{
  screen_rects_free ();
  screen_rects_init ();

  /* FIXME: We should only emit this when the size of screen really
   * has changed.  We need to start bookkeeping width, height once
   * we have a proper GdkScreen subclass.
   */
  g_signal_emit_by_name (_gdk_screen, "size-changed");
}

static gboolean
screen_changed_idle (gpointer data)
{
  process_display_reconfiguration ();

  screen_changed_id = 0;

  return FALSE;
}

static void
screen_changed (CGDirectDisplayID            display,
                CGDisplayChangeSummaryFlags  flags,
                void                        *userInfo)
{
  if (flags & kCGDisplayBeginConfigurationFlag)
    {
      /* Ignore the begin configuration signal. */

      /* FIXME: We can most probably use this flag to properly
       * emit monitors-changed.
       */
      return;
    }
  else
    {
      /* At this point Cocoa does not know about the new screen data
       * yet, so we delay our refresh into an idle handler.
       */

      if (!screen_changed_id)
        screen_changed_id = gdk_threads_add_idle (screen_changed_idle, NULL);
    }
}

void
_gdk_quartz_screen_init (void)
{
  gdk_screen_set_default_colormap (_gdk_screen,
                                   gdk_screen_get_system_colormap (_gdk_screen));

  screen_rects_init ();

  CGDisplayRegisterReconfigurationCallback (screen_changed,
                                            _gdk_screen);
}

GdkDisplay *
gdk_screen_get_display (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return _gdk_display;
}


GdkWindow *
gdk_screen_get_root_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return _gdk_root;
}

gint
gdk_screen_get_number (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

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

GdkColormap*
gdk_screen_get_default_colormap (GdkScreen *screen)
{
  return default_colormap;
}

void
gdk_screen_set_default_colormap (GdkScreen   *screen,
				 GdkColormap *colormap)
{
  GdkColormap *old_colormap;
  
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));

  old_colormap = default_colormap;

  default_colormap = g_object_ref (colormap);
  
  if (old_colormap)
    g_object_unref (old_colormap);
}

/* FIXME: note on the get_width() and the get_height() methods.  For
 * now we only support screen layouts where the screens are laid out
 * horizontally.  Mac OS X also supports laying out the screens vertically
 * and the screens having "non-standard" offsets from eachother.  In the
 * future we need a much more sophiscated algorithm to translate these
 * layouts to GDK coordinate space and GDK screen layout.
 */
gint
gdk_screen_get_width (GdkScreen *screen)
{
  int i;
  int width;
  NSArray *array;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  GDK_QUARTZ_ALLOC_POOL;
  array = [NSScreen screens];

  width = 0;
  for (i = 0; i < [array count]; i++) 
    {
      NSRect rect = [[array objectAtIndex:i] frame];
      width += rect.size.width;
    }

  GDK_QUARTZ_RELEASE_POOL;

  return width;
}

gint
gdk_screen_get_height (GdkScreen *screen)
{
  int i;
  int height;
  NSArray *array;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  GDK_QUARTZ_ALLOC_POOL;
  array = [NSScreen screens];

  height = 0;
  for (i = 0; i < [array count]; i++) 
    {
      NSRect rect = [[array objectAtIndex:i] frame];
      height = MAX (height, rect.size.height);
    }

  GDK_QUARTZ_RELEASE_POOL;

  return height;
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

gint
gdk_screen_get_width_mm (GdkScreen *screen)
{
  int i;
  gint width;
  NSArray *array;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  GDK_QUARTZ_ALLOC_POOL;
  array = [NSScreen screens];

  width = 0;
  for (i = 0; i < [array count]; i++)
    {
      NSScreen *screen = [array objectAtIndex:i];
      NSRect rect = [screen frame];
      width += get_mm_from_pixels (screen, rect.size.width);
    }

  GDK_QUARTZ_RELEASE_POOL;

  return width;
}

gint
gdk_screen_get_height_mm (GdkScreen *screen)
{
  int i;
  gint height;
  NSArray *array;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  GDK_QUARTZ_ALLOC_POOL;
  array = [NSScreen screens];

  height = 0;
  for (i = 0; i < [array count]; i++)
    {
      NSScreen *screen = [array objectAtIndex:i];
      NSRect rect = [screen frame];
      gint h = get_mm_from_pixels (screen, rect.size.height);
      height = MAX (height, h);
    }

  GDK_QUARTZ_RELEASE_POOL;

  return height;
}

int
gdk_screen_get_n_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return n_screens;
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

gint
gdk_screen_get_monitor_width_mm	(GdkScreen *screen,
				 gint       monitor_num)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  g_return_val_if_fail (monitor_num < gdk_screen_get_n_monitors (screen), 0);
  g_return_val_if_fail (monitor_num >= 0, 0);

  return get_mm_from_pixels (get_nsscreen_for_monitor (monitor_num),
                             screen_rects[monitor_num].width);
}

gint
gdk_screen_get_monitor_height_mm (GdkScreen *screen,
                                  gint       monitor_num)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  g_return_val_if_fail (monitor_num < gdk_screen_get_n_monitors (screen), 0);
  g_return_val_if_fail (monitor_num >= 0, 0);

  return get_mm_from_pixels (get_nsscreen_for_monitor (monitor_num),
                             screen_rects[monitor_num].height);
}

gchar *
gdk_screen_get_monitor_plug_name (GdkScreen *screen,
				  gint       monitor_num)
{
  /* FIXME: Is there some useful name we could use here? */
  return NULL;
}

void
gdk_screen_get_monitor_geometry (GdkScreen    *screen, 
				 gint          monitor_num,
				 GdkRectangle *dest)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (monitor_num < gdk_screen_get_n_monitors (screen));
  g_return_if_fail (monitor_num >= 0);

  *dest = screen_rects[monitor_num];
}

gchar *
gdk_screen_make_display_name (GdkScreen *screen)
{
  return g_strdup (gdk_display_get_name (_gdk_display));
}

GdkWindow *
gdk_screen_get_active_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return NULL;
}

GList *
gdk_screen_get_window_stack (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return NULL;
}

gboolean
gdk_screen_is_composited (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  return TRUE;
}
