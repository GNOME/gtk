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

#include "gdkscreen.h"
#include "gdkcolor.h"

static void gdk_screen_class_init (GObjectClass * klass);
static gpointer parent_class = NULL;

GtkType
gdk_screen_get_type (void)
{

  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info = {
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
gdk_screen_class_init (GObjectClass * klass)
{
  parent_class = g_type_class_peek_parent (klass);
}

/**
 * gdk_screen_get_default_colormap:
 * @screen: a #GdkScreen
 *
 * Returns the default @screen colormap
 * 
 * Returns: default #GdkColormap.
 */
GdkColormap *
gdk_screen_get_default_colormap (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  return GDK_SCREEN_GET_CLASS (screen)->get_default_colormap (screen);
}

/**
 * gdk_screen_set_default_colormap:
 * @screen: a #GdkScreen
 * @colormap: a #GdkColormap
 *
 * Sets the default @colormap for the @screen
 */
void
gdk_screen_set_default_colormap (GdkScreen * screen, GdkColormap * colormap)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (GDK_IS_COLORMAP (colormap));
  GDK_SCREEN_GET_CLASS (screen)->set_default_colormap (screen, colormap);
}

/**
 * gdk_screen_get_root_window:
 * @screen: a #GdkScreen
 *
 * Returns the root window of @screen. 
 * 
 * Returns: a #GdkWindow.
 */
GdkWindow *
gdk_screen_get_root_window (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  return GDK_SCREEN_GET_CLASS (screen)->get_root_window (screen);
}

/**
 * gdk_screen_get_display:
 * @screen: a #GdkScreen
 *
 * Returns the display to which the @screen belongs.
 * 
 * Returns: a #GdkDisplay.
 */
GdkDisplay *
gdk_screen_get_display (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  return GDK_SCREEN_GET_CLASS (screen)->get_display (screen);
}

/**
 * gdk_screen_get_number:
 * @screen: a #GdkScreen
 *
 * Returns the screen number with respect to the display
 * 
 * Returns: screen number.
 */
gint
gdk_screen_get_number (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  return GDK_SCREEN_GET_CLASS (screen)->get_screen_num (screen);
}

/**
 * gdk_screen_get_window_at_pointer:
 * @screen: a #GdkScreen
 * @win_x: return location for origin of the window under the pointer
 * @win_y: return location for origin of the window under the pointer
 * 
 * Obtains the window underneath the mouse pointer, returning the location
 * of that window in @win_x, @win_y for @screen. Returns %NULL if the window 
 * under the mouse pointer is not known to GDK (for example, belongs to
 * another application).
 * 
 * Returns: window under the mouse pointer
 */
GdkWindow *
gdk_screen_get_window_at_pointer (GdkScreen * screen,
				  gint * win_x, gint * win_y)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  return GDK_SCREEN_GET_CLASS (screen)->get_window_at_pointer (screen,
							       win_x, win_y);
}

/**
 * gdk_screen_get_width:
 * @screen: a #GdkScreen
 *
 * Returns Returns the width of @screen in pixels
 * 
 * Returns: the width of @screen in pixels.
 */
gint
gdk_screen_get_width (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  return GDK_SCREEN_GET_CLASS (screen)->get_width (screen);
}

/**
 * gdk_screen_get_height:
 * @screen: a #GdkScreen
 *
 * Returns Returns the height of @screen in pixels
 * 
 * Returns: the height of @screen in pixels.
 */
gint
gdk_screen_get_height (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  return GDK_SCREEN_GET_CLASS (screen)->get_height (screen);
}

/**
 * gdk_screen_get_width_mm:
 * @screen: a #GdkScreen
 *
 * Returns the width of @screen in millimeters. 
 * Note that on many X servers this value will not be correct.
 * 
 * Returns: the width of @screen in pixels.
 *	    Note that on many X servers this value will not be correct.
 */
gint
gdk_screen_get_width_mm (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  return GDK_SCREEN_GET_CLASS (screen)->get_width_mm (screen);
}

/**
 * gdk_screen_get_height_mm:
 * @screen: a #GdkScreen
 *
 * Returns the height of @screen in millimeters. 
 * Note that on many X servers this value will not be correct.
 * 
 * Returns: the heigth of @screen in pixels.
 *	    Note that on many X servers this value will not be correct.
 */

gint
gdk_screen_get_height_mm (GdkScreen * screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  return GDK_SCREEN_GET_CLASS (screen)->get_height_mm (screen);
}

/**
 * gdk_screen_close:
 * @screen: a #GdkScreen
 *
 * Closes the @screen connection and cleanup its resources.
 * Note that this function is called automatically by gdk_display_close().
 * 
 */

void 
gdk_screen_close (GdkScreen *screen)
{
  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_object_run_dispose (G_OBJECT (screen));
}

/**
 * gdk_screen_use_virtual_screen:
 * @screen : a #GdkScreen.
 *
 * Returns TRUE if @screen is uses multiple montiors as
 * a single virtual screen (e.g. Xinerama mode under X).
 *
 * Returns: TRUE if multiple monitors are used as a single
 *          Screen.
 */
gboolean 
gdk_screen_use_virtual_screen (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);
  return GDK_SCREEN_GET_CLASS (screen)->use_virtual_screen (screen);
}

/**
 * gdk_screen_get_num_monitors:
 * @screen : a #GdkScreen.
 *
 * Returns the number of monitors being part of the virtual screen
 *
 * Returns: number of monitors part of the virtual screen or
 *          0 if @screen is not in virtual screen mode.
 */
gint 
gdk_screen_get_num_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  return GDK_SCREEN_GET_CLASS (screen)->get_num_monitors (screen);
}

/**
 * gdk_screen_get_monitor_geometry:
 * @screen : a #GdkScreen.
 * @num_monitor : the monitor number. 
 *
 * Returns a #GdkRectangle representing the size and start
 * coordinates of the screen with respect to the virtual screen.
 * 
 * Note that the virtual screen coordinates can be retrieved via 
 * gdk_screen_get_[width|height]. 
 *
 * Returns: the size and start position of the monitor wrt to
 *	    the virtual screen.
 */
GdkRectangle *
gdk_screen_get_monitor_geometry (GdkScreen *screen, gint num_monitor)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  return GDK_SCREEN_GET_CLASS (screen)->get_monitor_geometry (screen,
							      num_monitor);
}

/**
 * gdk_screen_get_monitor_num_at_point:
 * @screen : a #GdkScreen.
 * @x : the x coordinate in the virtual screen.
 * @y : the y coordinate in the virtual screen.
 *
 * Returns the monitor number in which the point (@x,@y) is located.
 *
 * Returns: the monitor number in which the point (@x,@y) belong.
 */
gint 
gdk_screen_get_monitor_num_at_point (GdkScreen *screen, gint x, gint y)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  return GDK_SCREEN_GET_CLASS (screen)->get_monitor_num_at_point (screen, x,y);
}

/**
 * gdk_screen_get_monitor_num_at_window:
 * @screen : a #GdkScreen.
 * @anid : a #GdkDrawable ID.
 *
 * Returns the monitor number in which the largest area of the bounding rectangle
 * of @anid resides.
 *
 * Returns: the monitor number in which most of @anid is located.
 */
gint 
gdk_screen_get_monitor_num_at_window (GdkScreen *screen, GdkNativeWindow anid)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);
  return GDK_SCREEN_GET_CLASS (screen)->get_monitor_num_at_window (screen, 
								    anid);
}
