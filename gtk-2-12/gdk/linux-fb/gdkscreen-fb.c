/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include "gdk.h"
#include "gdkprivate-fb.h"

static GdkColormap *default_colormap = NULL;

GdkDisplay *
gdk_screen_get_display (GdkScreen *screen)
{
  return _gdk_display;
}

GdkWindow *
gdk_screen_get_root_window (GdkScreen *screen)
{
  return _gdk_parent_root;
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

int
gdk_screen_get_n_monitors (GdkScreen *screen)
{
  return 1;
}

void
gdk_screen_get_monitor_geometry (GdkScreen    *screen, 
				 gint          num_monitor,
				 GdkRectangle *dest)
{
  dest->x = 0;
  dest->y = 0;
  dest->width = gdk_screen_width ();
  dest->height = gdk_screen_height ();
}

GdkColormap *
gdk_screen_get_rgba_colormap (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return NULL;
}
  
GdkVisual *
gdk_screen_get_rgba_visual (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return NULL;
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
  return g_strdup (display_name);
}

gchar *
gdk_screen_make_display_name (GdkScreen *screen)
{
  return g_strdup ("");
}
