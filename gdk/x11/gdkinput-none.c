/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include "gdkinputprivate.h"

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

void
gdk_input_init (void)
{
  gdk_input_devices = g_list_append (NULL, gdk_core_pointer);

  gdk_input_ignore_core = FALSE;
}

void 
gdk_device_get_state (GdkDevice       *device,
		      GdkWindow       *window,
		      gdouble         *axes,
		      GdkModifierType *mask)
{
  gint x_int, y_int;

  g_return_if_fail (device != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_window_get_pointer (window, &x_int, &y_int, mask);

  if (axes)
    {
      axes[0] = x_int;
      axes[1] = y_int;
    }
}

gboolean
_gdk_device_get_history (GdkDevice         *device,
			 GdkWindow         *window,
			 guint32            start,
			 guint32            stop,
			 GdkTimeCoord    ***events,
			 gint              *n_events)
{
  g_warning ("gdk_device_get_history() called for invalid device");
  return FALSE;
}

gboolean
_gdk_input_enable_window(GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  return TRUE;
}

gboolean
_gdk_input_disable_window(GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  return TRUE;
}

gint 
_gdk_input_window_none_event (GdkEvent         *event,
			     XEvent           *xevent)
{
  return -1;
}

gint 
_gdk_input_other_event (GdkEvent *event, 
			XEvent *xevent, 
			GdkWindow *window)
{
  return -1;
}

void
_gdk_input_configure_event (XConfigureEvent *xevent,
			    GdkWindow       *window)
{
}

void 
_gdk_input_enter_event (XCrossingEvent *xevent, 
			GdkWindow      *window)
{
}

gint 
_gdk_input_grab_pointer (GdkWindow *     window,
			 gint            owner_events,
			 GdkEventMask    event_mask,
			 GdkWindow *     confine_to,
			 guint32         time)
{
  return Success;
}

void
_gdk_input_ungrab_pointer (guint32         time)
{
}

gboolean
gdk_device_set_mode (GdkDevice   *device,
		     GdkInputMode mode)
{
  return FALSE;
}

