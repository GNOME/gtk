/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2002 Hans Breuer
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

#include "gdk.h" /* gdk_get_display_arg_name() */
#include "gdkdisplay.h"
#include "gdkscreen.h"
#include "gdkprivate-win32.h"

GdkDisplay *_gdk_display = NULL;
GdkScreen  *_gdk_screen  = NULL;

void
_gdk_windowing_set_default_display (GdkDisplay *display)
{
  g_assert (_gdk_display == display);
}

GdkDisplay *
gdk_open_display (const gchar *display_name)
{
  if (_gdk_display != NULL)
    return NULL; /* single display only */

  _gdk_display = g_object_new (GDK_TYPE_DISPLAY, NULL);
  _gdk_screen = g_object_new (GDK_TYPE_SCREEN, NULL);

  gdk_set_default_display (_gdk_display);

  _gdk_visual_init ();
  gdk_screen_set_default_colormap (_gdk_screen,
                                   gdk_screen_get_system_colormap (_gdk_screen));
  _gdk_windowing_window_init ();
  _gdk_windowing_image_init ();
  _gdk_events_init ();
  _gdk_input_init ();
  _gdk_dnd_init ();

  return _gdk_display;
}

G_CONST_RETURN gchar*
gdk_display_get_display_name (GdkDisplay *display)
{
  return gdk_get_display_arg_name ();
}

gint
gdk_display_get_n_screens (GdkDisplay * display)
{
  return 1;
}

GdkScreen *
gdk_display_get_screen (GdkDisplay *display,
			gint        screen_num)
{
  g_return_val_if_fail (screen_num == 0, _gdk_screen);
  
  return _gdk_screen;
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay * display)
{
  return _gdk_screen;
}

