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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "gdkinputprivate.h"
#include "gdkdisplay-x11.h"
#include "gdkalias.h"

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* forward declarations */

static void gdk_input_check_proximity (GdkDisplay *display);

void 
_gdk_input_init(GdkDisplay *display)
{
  _gdk_init_input_core (display);
  GDK_DISPLAY_X11 (display)->input_ignore_core = FALSE;
  _gdk_input_common_init (display, FALSE);
}

gboolean
gdk_device_set_mode (GdkDevice      *device,
		     GdkInputMode    mode)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;
  GdkInputMode old_mode;
  GdkInputWindow *input_window;
  GdkDisplayX11 *display_impl;

  if (GDK_IS_CORE (device))
    return FALSE;

  gdkdev = (GdkDevicePrivate *)device;

  if (device->mode == mode)
    return TRUE;

  old_mode = device->mode;
  device->mode = mode;

  display_impl = GDK_DISPLAY_X11 (gdkdev->display);

  if (mode == GDK_MODE_WINDOW)
    {
      device->has_cursor = FALSE;
      for (tmp_list = display_impl->input_windows; tmp_list; tmp_list = tmp_list->next)
	{
	  input_window = (GdkInputWindow *)tmp_list->data;
	  if (input_window->mode != GDK_EXTENSION_EVENTS_CURSOR)
	    _gdk_input_enable_window (input_window->window, gdkdev);
	  else
	    if (old_mode != GDK_MODE_DISABLED)
	      _gdk_input_disable_window (input_window->window, gdkdev);
	}
    }
  else if (mode == GDK_MODE_SCREEN)
    {
      device->has_cursor = TRUE;
      for (tmp_list = display_impl->input_windows; tmp_list; tmp_list = tmp_list->next)
	_gdk_input_enable_window (((GdkInputWindow *)tmp_list->data)->window,
				  gdkdev);
    }
  else  /* mode == GDK_MODE_DISABLED */
    {
      for (tmp_list = display_impl->input_windows; tmp_list; tmp_list = tmp_list->next)
	{
	  input_window = (GdkInputWindow *)tmp_list->data;
	  if (old_mode != GDK_MODE_WINDOW ||
	      input_window->mode != GDK_EXTENSION_EVENTS_CURSOR)
	    _gdk_input_disable_window (input_window->window, gdkdev);
	}
    }

  return TRUE;
  
}

static void
gdk_input_check_proximity (GdkDisplay *display)
{
  gint new_proximity = 0;
  GdkDisplayX11 *display_impl = GDK_DISPLAY_X11 (display);
  GList *tmp_list = display_impl->input_devices;

  while (tmp_list && !new_proximity) 
    {
      GdkDevicePrivate *gdkdev = (GdkDevicePrivate *)(tmp_list->data);

      if (gdkdev->info.mode != GDK_MODE_DISABLED 
	  && !GDK_IS_CORE (gdkdev)
	  && gdkdev->xdevice)
	{
	  XDeviceState *state = XQueryDeviceState(display_impl->xdisplay,
						  gdkdev->xdevice);
	  XInputClass *xic;
	  int i;
	  
	  xic = state->data;
	  for (i=0; i<state->num_classes; i++)
	    {
	      if (xic->class == ValuatorClass)
		{
		  XValuatorState *xvs = (XValuatorState *)xic;
		  if ((xvs->mode & ProximityState) == InProximity)
		    {
		      new_proximity = TRUE;
		    }
		  break;
		}
	      xic = (XInputClass *)((char *)xic + xic->length);
	    }

	  XFreeDeviceState (state);
 	}
      tmp_list = tmp_list->next;
    }

  display_impl->input_ignore_core = new_proximity;
}

void
_gdk_input_configure_event (XConfigureEvent *xevent,
			    GdkWindow       *window)
{
  GdkInputWindow *input_window;
  gint root_x, root_y;

  input_window = _gdk_input_window_find(window);
  g_return_if_fail (input_window != NULL);

  _gdk_input_get_root_relative_geometry(GDK_WINDOW_XDISPLAY (window),
					GDK_WINDOW_XWINDOW (window),
					&root_x, &root_y, NULL, NULL);

  input_window->root_x = root_x;
  input_window->root_y = root_y;
}

void 
_gdk_input_enter_event (XCrossingEvent *xevent, 
			GdkWindow      *window)
{
  GdkInputWindow *input_window;
  gint root_x, root_y;

  input_window = _gdk_input_window_find (window);
  g_return_if_fail (input_window != NULL);

  gdk_input_check_proximity(GDK_WINDOW_DISPLAY (window));

  _gdk_input_get_root_relative_geometry(GDK_WINDOW_XDISPLAY (window),
					GDK_WINDOW_XWINDOW(window),
					&root_x, &root_y, NULL, NULL);

  input_window->root_x = root_x;
  input_window->root_y = root_y;
}

gboolean 
_gdk_input_other_event (GdkEvent *event, 
			XEvent *xevent, 
			GdkWindow *window)
{
  GdkInputWindow *input_window;
  
  GdkDevicePrivate *gdkdev;
  gint return_val;
  GdkDisplayX11 *display_impl = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));

  input_window = _gdk_input_window_find(window);
  g_return_val_if_fail (input_window != NULL, FALSE);

  /* This is a sort of a hack, as there isn't any XDeviceAnyEvent -
     but it's potentially faster than scanning through the types of
     every device. If we were deceived, then it won't match any of
     the types for the device anyways */
  gdkdev = _gdk_input_find_device (GDK_WINDOW_DISPLAY (window),
				   ((XDeviceButtonEvent *)xevent)->deviceid);
  if (!gdkdev)
    return FALSE;			/* we don't handle it - not an XInput event */

  /* FIXME: It would be nice if we could just get rid of the events 
     entirely, instead of having to ignore them */
  if (gdkdev->info.mode == GDK_MODE_DISABLED ||
      (gdkdev->info.mode == GDK_MODE_WINDOW 
       && input_window->mode == GDK_EXTENSION_EVENTS_CURSOR))
    return FALSE;
  
  if (!display_impl->input_ignore_core)
    gdk_input_check_proximity(GDK_WINDOW_DISPLAY (window));

  return_val = _gdk_input_common_other_event (event, xevent, 
					      input_window, gdkdev);

  if (return_val && event->type == GDK_PROXIMITY_OUT &&
      display_impl->input_ignore_core)
    gdk_input_check_proximity(GDK_WINDOW_DISPLAY (window));

  return return_val;
}

gboolean
_gdk_input_enable_window(GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  /* FIXME: watchout, gdkdev might be core pointer, never opened */
  _gdk_input_common_select_events (window, gdkdev);
  return TRUE;
}

gboolean
_gdk_input_disable_window(GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  _gdk_input_common_select_events (window, gdkdev);
  return TRUE;
}

gint 
_gdk_input_grab_pointer (GdkWindow *     window,
			 gint            owner_events,
			 GdkEventMask    event_mask,
			 GdkWindow *     confine_to,
			 guint32         time)
{
  GdkInputWindow *input_window, *new_window;
  gboolean need_ungrab;
  GdkDevicePrivate *gdkdev;
  GList *tmp_list;
  XEventClass event_classes[GDK_MAX_DEVICE_CLASSES];
  gint num_classes;
  gint result;
  GdkDisplayX11 *display_impl  = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));

  tmp_list = display_impl->input_windows;
  new_window = NULL;
  need_ungrab = FALSE;

  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;

      if (input_window->window == window)
	new_window = input_window;
      else if (input_window->grabbed)
	{
	  input_window->grabbed = FALSE;
	  need_ungrab = TRUE;
	}

      tmp_list = tmp_list->next;
    }

  if (new_window)
    {
      new_window->grabbed = TRUE;
      
      tmp_list = display_impl->input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (!GDK_IS_CORE (gdkdev) && gdkdev->xdevice)
	    {
	      _gdk_input_common_find_events (window, gdkdev,
					     event_mask,
					     event_classes, &num_classes);
#ifdef G_ENABLE_DEBUG
	      if (_gdk_debug_flags & GDK_DEBUG_NOGRABS)
		result = GrabSuccess;
	      else
#endif
		result = XGrabDevice (display_impl->xdisplay, gdkdev->xdevice,
				      GDK_WINDOW_XWINDOW (window),
				      owner_events, num_classes, event_classes,
				      GrabModeAsync, GrabModeAsync, time);
	      
	      /* FIXME: if failure occurs on something other than the first
		 device, things will be badly inconsistent */
	      if (result != Success)
		return result;
	    }
	  tmp_list = tmp_list->next;
	}
    }
  else
    { 
      tmp_list = display_impl->input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (!GDK_IS_CORE (gdkdev) && gdkdev->xdevice &&
	      ((gdkdev->button_state != 0) || need_ungrab))
	    {
	      XUngrabDevice (display_impl->xdisplay, gdkdev->xdevice, time);
	      gdkdev->button_state = 0;
	    }
	  
	  tmp_list = tmp_list->next;
	}
    }

  return Success;
      
}

void 
_gdk_input_ungrab_pointer (GdkDisplay *display, 
			   guint32 time)
{
  GdkInputWindow *input_window = NULL; /* Quiet GCC */
  GdkDevicePrivate *gdkdev;
  GList *tmp_list;
  GdkDisplayX11 *display_impl = GDK_DISPLAY_X11 (display);

  tmp_list = display_impl->input_windows;
  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;
      if (input_window->grabbed)
	break;
      tmp_list = tmp_list->next;
    }

  if (tmp_list)			/* we found a grabbed window */
    {
      input_window->grabbed = FALSE;

      tmp_list = display_impl->input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (!GDK_IS_CORE (gdkdev) && gdkdev->xdevice)
	    XUngrabDevice( display_impl->xdisplay, gdkdev->xdevice, time);

	  tmp_list = tmp_list->next;
	}
    }
}

#define __GDK_INPUT_XFREE_C__
#include "gdkaliasdef.c"
