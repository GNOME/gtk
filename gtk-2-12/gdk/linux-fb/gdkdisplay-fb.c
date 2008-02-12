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

GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  if (gdk_display == NULL || _gdk_display != NULL)
    return NULL; /* single display only */

  _gdk_display = g_object_new (GDK_TYPE_DISPLAY, NULL);
  _gdk_screen = g_object_new (GDK_TYPE_SCREEN, NULL);

  _gdk_visual_init ();
  gdk_screen_set_default_colormap (_gdk_screen,
                                   gdk_screen_get_system_colormap (_gdk_screen));
  _gdk_windowing_window_init ();
  _gdk_windowing_image_init ();
  _gdk_events_init ();
  _gdk_input_init ();
  _gdk_dnd_init ();

  g_signal_emit_by_name (gdk_display_manager_get (),
			 "display_opened", _gdk_display);

  return _gdk_display;
}

G_CONST_RETURN gchar *
gdk_display_get_name (GdkDisplay *display)
{
  return gdk_get_display_arg_name ();
}

int
gdk_display_get_n_screens (GdkDisplay *display)
{
  return 1;
}

GdkScreen *
gdk_display_get_screen (GdkDisplay *display,
			gint        screen_num)
{
  return _gdk_screen;
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  return _gdk_screen;
}
