/* GDK - The GIMP Drawing Kit
 * gdkdeprecated.c
 * 
 * Copyright 1995-2011 Red Hat Inc.
 *
 * Benjamin Otte <otte@gnome.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#define GDK_DISABLE_DEPRECATION_WARNINGS

#include "config.h"

#include "gdkdisplay.h"
#include "gdkmain.h"
#include "gdkwindow.h"

/**
 * gdk_window_at_pointer:
 * @win_x: (out) (allow-none): return location for origin of the window under the pointer
 * @win_y: (out) (allow-none): return location for origin of the window under the pointer
 *
 * Obtains the window underneath the mouse pointer, returning the
 * location of that window in @win_x, @win_y. Returns %NULL if the
 * window under the mouse pointer is not known to GDK (if the window
 * belongs to another application and a #GdkWindow hasn’t been created
 * for it with gdk_window_foreign_new())
 *
 * NOTE: For multihead-aware widgets or applications use
 * gdk_display_get_window_at_pointer() instead.
 *
 * Returns: (transfer none): window under the mouse pointer
 *
 * Deprecated: 3.0: Use gdk_device_get_window_at_position() instead.
 **/
GdkWindow*
gdk_window_at_pointer (gint *win_x,
		       gint *win_y)
{
  return gdk_display_get_window_at_pointer (gdk_display_get_default (), win_x, win_y);
}

