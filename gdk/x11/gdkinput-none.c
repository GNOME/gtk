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

#ifdef XINPUT_NONE

static void gdk_input_none_get_pointer (GdkWindow       *window,
					guint32	  deviceid,
					gdouble         *x,
					gdouble         *y,
					gdouble         *pressure,
					gdouble         *xtilt,
					gdouble         *ytilt,
					GdkModifierType *mask);

void
gdk_input_init (void)
{
  gdk_input_vtable.set_mode           = NULL;
  gdk_input_vtable.set_axes           = NULL;
  gdk_input_vtable.set_key            = NULL;
  gdk_input_vtable.motion_events      = NULL;
  gdk_input_vtable.get_pointer        = gdk_input_none_get_pointer;
  gdk_input_vtable.grab_pointer       = NULL;
  gdk_input_vtable.ungrab_pointer     = NULL;
  gdk_input_vtable.configure_event    = NULL;
  gdk_input_vtable.enter_event        = NULL;
  gdk_input_vtable.other_event        = NULL;
  gdk_input_vtable.window_none_event  = NULL;
  gdk_input_vtable.enable_window      = NULL;
  gdk_input_vtable.disable_window     = NULL;

  gdk_input_devices = g_list_append (NULL, &gdk_input_core_info);

  gdk_input_ignore_core = FALSE;
}

static void
gdk_input_none_get_pointer (GdkWindow       *window,
			    guint32          deviceid,
			    gdouble         *x,
			    gdouble         *y,
			    gdouble         *pressure,
			    gdouble         *xtilt,
			    gdouble         *ytilt,
			    GdkModifierType *mask)
{
  gint x_int, y_int;

  gdk_window_get_pointer (window, &x_int, &y_int, mask);

  if (x) *x = x_int;
  if (y) *y = y_int;
  if (pressure) *pressure = 0.5;
  if (xtilt) *xtilt = 0;
  if (ytilt) *ytilt = 0;
}

#endif /* XINPUT_NONE */
