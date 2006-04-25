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

#include <config.h>
#include "gdk.h"
#include "gdkprivate-quartz.h"

static GdkColormap *default_colormap = NULL;

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

gint
gdk_screen_get_width (GdkScreen *screen)
{
  int i;
  int width;
  NSArray *array;
  NSAutoreleasePool *pool;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  pool = [[NSAutoreleasePool alloc] init];
  array = [NSScreen screens];

  width = 0;
  for (i = 0; i < [array count]; i++) 
    {
      NSRect rect = [[array objectAtIndex:i] frame];
      width += rect.size.width;
    }

  [pool release];

  return width;
}

gint
gdk_screen_get_height (GdkScreen *screen)
{
  int i;
  int height;
  NSArray *array;
  NSAutoreleasePool *pool;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  pool = [[NSAutoreleasePool alloc] init];
  array = [NSScreen screens];

  height = 0;
  for (i = 0; i < [array count]; i++) 
    {
      NSRect rect = [[array objectAtIndex:i] frame];
      height = MAX (height, rect.size.height);
    }

  [pool release];

  return height;
}

static gint
get_mm_from_pixels (NSScreen *screen, int pixels)
{
  /* userSpaceScaleFactor is in "pixels per point", 
   * 72 is the number of points per inch, 
   * and 25.4 is the number of millimeters per inch.
   */
  return ((pixels / [screen userSpaceScaleFactor]) / 72) * 25.4;
}

gint
gdk_screen_get_width_mm (GdkScreen *screen)
{
  int i;
  gint width;
  NSArray *array;
  NSAutoreleasePool *pool;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  pool = [[NSAutoreleasePool alloc] init];
  array = [NSScreen screens];

  width = 0;
  for (i = 0; i < [array count]; i++)
    {
      NSScreen *screen = [array objectAtIndex:i];
      NSRect rect = [screen frame];
      width += get_mm_from_pixels (screen, rect.size.width);
    }

  [pool release];

  return width;
}

gint
gdk_screen_get_height_mm (GdkScreen *screen)
{
  int i;
  gint height;
  NSArray *array;
  NSAutoreleasePool *pool;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  pool = [[NSAutoreleasePool alloc] init];
  array = [NSScreen screens];

  height = 0;
  for (i = 0; i < [array count]; i++)
    {
      NSScreen *screen = [array objectAtIndex:i];
      NSRect rect = [screen frame];
      gint h = get_mm_from_pixels (screen, rect.size.height);
      height = MAX (height, h);
    }

  [pool release];

  return height;
}

int
gdk_screen_get_n_monitors (GdkScreen *screen)
{
  int n;
  GDK_QUARTZ_ALLOC_POOL;
  NSArray *array = [NSScreen screens];

  n = [array count];

  GDK_QUARTZ_RELEASE_POOL;

  return n;
}

void
gdk_screen_get_monitor_geometry (GdkScreen    *screen, 
				 gint          monitor_num,
				 GdkRectangle *dest)
{
  NSArray *array;
  NSRect rect;
  NSAutoreleasePool *pool;

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (monitor_num < gdk_screen_get_n_monitors (screen));
  g_return_if_fail (monitor_num >= 0);

  pool = [[NSAutoreleasePool alloc] init];
  array = [NSScreen screens];
  rect = [[array objectAtIndex:monitor_num] frame];
  
  dest->x = rect.origin.x;
  dest->y = rect.origin.y;
  dest->width = rect.size.width;
  dest->height = rect.size.height;

  [pool release];
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
