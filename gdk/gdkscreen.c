/*
 * gdkscreen.c
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "gdkalias.h"
#include "gdk.h"		/* For gdk_rectangle_intersect() */
#include "gdkcolor.h"
#include "gdkwindow.h"
#include "gdkscreen.h"

static void gdk_screen_class_init  (GdkScreenClass *klass);
static void gdk_screen_dispose     (GObject        *object);

enum
{
  SIZE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static gpointer parent_class = NULL;

GType
gdk_screen_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{
	  sizeof (GdkScreenClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) gdk_screen_class_init,
	  NULL,			/* class_finalize */
	  NULL,			/* class_data */
	  sizeof (GdkScreen),
	  0,			/* n_preallocs */
	  (GInstanceInitFunc) NULL,
	};
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
					    "GdkScreen", &object_info, 0);
    }

  return object_type;
}

static void
gdk_screen_class_init (GdkScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
  
  object_class->dispose = gdk_screen_dispose;
  
  /**
   * GdkScreen::size-changed:
   * @screen: the object on which the signal is emitted
   * 
   * The ::size_changed signal is emitted when the pixel width or 
   * height of a screen changes.
   *
   * Since: 2.2
   */
  signals[SIZE_CHANGED] =
    g_signal_new ("size_changed",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkScreenClass, size_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);
}

static void
gdk_screen_dispose (GObject *object)
{
  GdkScreen *screen = GDK_SCREEN (object);
  gint i;

  for (i = 0; i < 32; ++i)
    {
      if (screen->exposure_gcs[i])
	g_object_unref (screen->exposure_gcs[i]);

      if (screen->normal_gcs[i])
	g_object_unref (screen->normal_gcs[i]);
    }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

void 
_gdk_screen_close (GdkScreen *screen)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));

  if (!screen->closed)
    {
      screen->closed = TRUE;
      g_object_run_dispose (G_OBJECT (screen));
    }
}

/* Fallback used when the monitor "at" a point or window
 * doesn't exist.
 */
static gint
get_nearest_monitor (GdkScreen *screen,
		     gint       x,
		     gint       y)
{
  gint num_monitors, i;
  gint nearest_dist = G_MAXINT;
  gint nearest_monitor = 0;
  
  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);

  num_monitors = gdk_screen_get_n_monitors (screen);
  
  for (i = 0; i < num_monitors; i++)
    {
      GdkRectangle monitor;
      gint dist_x, dist_y;
      
      gdk_screen_get_monitor_geometry (screen, i, &monitor);

      if (x < monitor.x)
	dist_x = monitor.x - x;
      else if (x >= monitor.x + monitor.width)
	dist_x = x - (monitor.x + monitor.width) + 1;
      else
	dist_x = 0;

      if (y < monitor.y)
	dist_y = monitor.y - y;
      else if (y >= monitor.y + monitor.height)
	dist_y = y - (monitor.y + monitor.height) + 1;
      else
	dist_y = 0;

      if (MIN (dist_x, dist_y) < nearest_dist)
	{
	  nearest_dist = MIN (dist_x, dist_y);
	  nearest_monitor = i;
	}
    }

  return nearest_monitor;
}

/**
 * gdk_screen_get_monitor_at_point:
 * @screen: a #GdkScreen.
 * @x: the x coordinate in the virtual screen.
 * @y: the y coordinate in the virtual screen.
 *
 * Returns the monitor number in which the point (@x,@y) is located.
 *
 * Returns: the monitor number in which the point (@x,@y) lies, or
 *   a monitor close to (@x,@y) if the point is not in any monitor.
 *
 * Since: 2.2
 **/
gint 
gdk_screen_get_monitor_at_point (GdkScreen *screen,
				 gint       x,
				 gint       y)
{
  gint num_monitors, i;
  
  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);

  num_monitors = gdk_screen_get_n_monitors (screen);
  
  for (i=0;i<num_monitors;i++)
    {
      GdkRectangle monitor;
      
      gdk_screen_get_monitor_geometry (screen, i, &monitor);

      if (x >= monitor.x &&
          x < monitor.x + monitor.width &&
          y >= monitor.y &&
          y < (monitor.y + monitor.height))
        return i;
    }

  return get_nearest_monitor (screen, x, y);
}

/**
 * gdk_screen_get_monitor_at_window:
 * @screen: a #GdkScreen.
 * @window: a #GdkWindow
 * @returns: the monitor number in which most of @window is located,
 *           or if @window does not intersect any monitors, a monitor,
 *           close to @window.
 *
 * Returns the number of the monitor in which the largest area of the 
 * bounding rectangle of @window resides.
 *
 * Since: 2.2
 **/
gint 
gdk_screen_get_monitor_at_window (GdkScreen      *screen,
				  GdkWindow	 *window)
{
  gint num_monitors, i, area = 0, screen_num = -1;
  GdkRectangle win_rect;
  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);
  
  gdk_window_get_geometry (window, &win_rect.x, &win_rect.y, &win_rect.width,
			   &win_rect.height, NULL);
  gdk_window_get_origin (window, &win_rect.x, &win_rect.y);
  num_monitors = gdk_screen_get_n_monitors (screen);
  
  for (i=0;i<num_monitors;i++)
    {
      GdkRectangle tmp_monitor, intersect;
      
      gdk_screen_get_monitor_geometry (screen, i, &tmp_monitor);
      gdk_rectangle_intersect (&win_rect, &tmp_monitor, &intersect);
      
      if (intersect.width * intersect.height > area)
	{ 
	  area = intersect.width * intersect.height;
	  screen_num = i;
	}
    }
  if (screen_num >= 0)
    return screen_num;
  else
    return get_nearest_monitor (screen,
				win_rect.x + win_rect.width / 2,
				win_rect.y + win_rect.height / 2);
}

/**
 * gdk_screen_width:
 * 
 * Returns the width of the default screen in pixels.
 * 
 * Return value: the width of the default screen in pixels.
 **/
gint
gdk_screen_width (void)
{
  return gdk_screen_get_width (gdk_screen_get_default());
}

/**
 * gdk_screen_height:
 * 
 * Returns the height of the default screen in pixels.
 * 
 * Return value: the height of the default screen in pixels.
 **/
gint
gdk_screen_height (void)
{
  return gdk_screen_get_height (gdk_screen_get_default());
}

/**
 * gdk_screen_width_mm:
 * 
 * Returns the width of the default screen in millimeters.
 * Note that on many X servers this value will not be correct.
 * 
 * Return value: the width of the default screen in millimeters,
 * though it is not always correct.
 **/
gint
gdk_screen_width_mm (void)
{
  return gdk_screen_get_width_mm (gdk_screen_get_default());
}

/**
 * gdk_screen_height_mm:
 * 
 * Returns the height of the default screen in millimeters.
 * Note that on many X servers this value will not be correct.
 * 
 * Return value: the height of the default screen in millimeters,
 * though it is not always correct.
 **/
gint
gdk_screen_height_mm (void)
{
  return gdk_screen_get_height_mm (gdk_screen_get_default ());
}
