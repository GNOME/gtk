/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include "config.h"

#include <math.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "gailmisc.h"

/* IMPORTANT!!! This source file does NOT contain the implementation
 * code for AtkUtil - for that code, please see gail/gail.c.
 */

/**
 * SECTION:gailmisc
 * @Short_description: GailMisc is a set of utility functions which may be
 *   useful to implementors of Atk interfaces for custom widgets.
 * @Title: GailMisc
 *
 * GailMisc is a set of utility function which are used in the implemementation
 * of Atk interfaces for GTK+ widgets. They may be useful to implementors of
 * Atk interfaces for custom widgets.
 */


/**
 * gail_misc_get_extents_from_pango_rectangle:
 * @widget: The widget that contains the PangoLayout, that contains
 *   the PangoRectangle
 * @char_rect: The #PangoRectangle from which to calculate extents
 * @x_layout: The x-offset at which the widget displays the
 *   PangoLayout that contains the PangoRectangle, relative to @widget
 * @y_layout: The y-offset at which the widget displays the
 *   PangoLayout that contains the PangoRectangle, relative to @widget
 * @x: The x-position of the #PangoRectangle relative to @coords
 * @y: The y-position of the #PangoRectangle relative to @coords
 * @width: The width of the #PangoRectangle
 * @height: The  height of the #PangoRectangle
 * @coords: An #AtkCoordType enumeration
 *
 * Gets the extents of @char_rect in device coordinates,
 * relative to either top-level window or screen coordinates as
 * specified by @coords.
 **/
void
gail_misc_get_extents_from_pango_rectangle (GtkWidget      *widget,
                                            PangoRectangle *char_rect,
                                            gint           x_layout,
                                            gint           y_layout,
                                            gint           *x,
                                            gint           *y,
                                            gint           *width,
                                            gint           *height,
                                            AtkCoordType   coords)
{
  gint x_window, y_window, x_toplevel, y_toplevel;

  gail_misc_get_origins (widget, &x_window, &y_window, 
                         &x_toplevel, &y_toplevel);

  *x = (char_rect->x / PANGO_SCALE) + x_layout + x_window;
  *y = (char_rect->y / PANGO_SCALE) + y_layout + y_window;
  if (coords == ATK_XY_WINDOW)
    {
      *x -= x_toplevel;
      *y -= y_toplevel;
    }
  else if (coords != ATK_XY_SCREEN)
    {
      *x = 0;
      *y = 0;
      *height = 0;
      *width = 0;
      return;
    }
  *height = char_rect->height / PANGO_SCALE;
  *width = char_rect->width / PANGO_SCALE;

  return;
}

/**
 * gail_misc_get_index_at_point_in_layout:
 * @widget: A #GtkWidget
 * @layout: The #PangoLayout from which to get the index at the
 *   specified point.
 * @x_layout: The x-offset at which the widget displays the
 *   #PangoLayout, relative to @widget
 * @y_layout: The y-offset at which the widget displays the
 *   #PangoLayout, relative to @widget
 * @x: The x-coordinate relative to @coords at which to
 *   calculate the index
 * @y: The y-coordinate relative to @coords at which to
 *   calculate the index
 * @coords: An #AtkCoordType enumeration
 *
 * Gets the byte offset at the specified @x and @y in a #PangoLayout.
 *
 * Returns: the byte offset at the specified @x and @y in a
 *   #PangoLayout
 **/
gint
gail_misc_get_index_at_point_in_layout (GtkWidget   *widget,
                                        PangoLayout *layout,
                                        gint        x_layout,
                                        gint        y_layout,
                                        gint        x,
                                        gint        y,
                                        AtkCoordType coords)
{
  gint index, x_window, y_window, x_toplevel, y_toplevel;
  gint x_temp, y_temp;
  gboolean ret;

  gail_misc_get_origins (widget, &x_window, &y_window, 
                         &x_toplevel, &y_toplevel);
  x_temp =  x - x_layout - x_window;
  y_temp =  y - y_layout - y_window;
  if (coords == ATK_XY_WINDOW)
    {
      x_temp += x_toplevel;  
      y_temp += y_toplevel;
    }
  else if (coords != ATK_XY_SCREEN)
    return -1;

  ret = pango_layout_xy_to_index (layout, 
                                  x_temp * PANGO_SCALE,
                                  y_temp * PANGO_SCALE,
                                  &index, NULL);
  if (!ret)
    {
      if (x_temp < 0 || y_temp < 0)
        index = 0;
      else
        index = -1; 
    }
  return index;
}


/**
 * gail_misc_get_origins:
 * @widget: a #GtkWidget
 * @x_window: the x-origin of the widget->window
 * @y_window: the y-origin of the widget->window
 * @x_toplevel: the x-origin of the toplevel window for widget->window
 * @y_toplevel: the y-origin of the toplevel window for widget->window
 *
 * Gets the origin of the widget window, and the origin of the
 * widgets top-level window.
 **/
void
gail_misc_get_origins (GtkWidget *widget,
                       gint      *x_window,
                       gint      *y_window,
                       gint      *x_toplevel,
                       gint      *y_toplevel)
{
  GdkWindow *window;

  if (GTK_IS_TREE_VIEW (widget))
    window = gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget));
  else
    window = gtk_widget_get_window (widget);

  gdk_window_get_origin (window, x_window, y_window);
  window = gdk_window_get_toplevel (gtk_widget_get_window (widget));
  gdk_window_get_origin (window, x_toplevel, y_toplevel);
}
