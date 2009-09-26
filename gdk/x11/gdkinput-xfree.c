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

#include "config.h"
#include "gdkinputprivate.h"
#include "gdkdisplay-x11.h"
#include "gdkalias.h"
#include "gdkdevice-xi.h"

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

/* forward declarations */

static void gdk_input_check_proximity (GdkDisplay *display);

gboolean
gdk_device_set_mode (GdkDevice      *device,
		     GdkInputMode    mode)
{
  GList *tmp_list;
  GdkInputWindow *input_window;
  GdkDisplayX11 *display_impl;

  if (GDK_IS_CORE (device))
    return FALSE;

  if (device->mode == mode)
    return TRUE;

  device->mode = mode;

  if (mode == GDK_MODE_WINDOW)
    device->has_cursor = FALSE;
  else if (mode == GDK_MODE_SCREEN)
    device->has_cursor = TRUE;

  display_impl = GDK_DISPLAY_X11 (gdk_device_get_display (device));
  for (tmp_list = display_impl->input_windows; tmp_list; tmp_list = tmp_list->next)
    {
      input_window = (GdkInputWindow *)tmp_list->data;
      _gdk_input_select_events (input_window->impl_window, device);
    }

  return TRUE;
}

static void
gdk_input_check_proximity (GdkDisplay *display)
{
  GdkDisplayX11 *display_impl = GDK_DISPLAY_X11 (display);
  GList *tmp_list = display_impl->input_devices;
  gint new_proximity = 0;

  while (tmp_list && !new_proximity)
    {
      GdkDevice *device = tmp_list->data;

      if (device->mode != GDK_MODE_DISABLED &&
	  !GDK_IS_CORE (device) &&
          GDK_IS_DEVICE_XI (device))
	{
          GdkDeviceXI *device_xi = GDK_DEVICE_XI (device);

          if (device_xi->xdevice)
            {
              XDeviceState *state = XQueryDeviceState(display_impl->xdisplay,
                                                      device_xi->xdevice);
              XInputClass *xic;
              int i;

              xic = state->data;
              for (i = 0; i < state->num_classes; i++)
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
                  xic = (XInputClass *) ((char *) xic + xic->length);
                }

              XFreeDeviceState (state);
            }
	}

      tmp_list = tmp_list->next;
    }

  display->ignore_core_events = new_proximity;
}

void
_gdk_input_configure_event (XConfigureEvent *xevent,
			    GdkWindow       *window)
{
  GdkWindowObject *priv = (GdkWindowObject *)window;
  GdkInputWindow *input_window;
  gint root_x, root_y;

  input_window = priv->input_window;
  if (input_window != NULL)
    {
      _gdk_input_get_root_relative_geometry (window, &root_x, &root_y);
      input_window->root_x = root_x;
      input_window->root_y = root_y;
    }
}

void
_gdk_input_crossing_event (GdkWindow *window,
			   gboolean enter)
{
  GdkDisplay *display = GDK_WINDOW_DISPLAY (window);
  GdkWindowObject *priv = (GdkWindowObject *)window;
  GdkInputWindow *input_window;
  gint root_x, root_y;

  if (enter)
    {
      gdk_input_check_proximity(display);

      input_window = priv->input_window;
      if (input_window != NULL)
	{
	  _gdk_input_get_root_relative_geometry (window, &root_x, &root_y);
	  input_window->root_x = root_x;
	  input_window->root_y = root_y;
	}
    }
  else
    display->ignore_core_events = FALSE;
}

#if 0
static GdkEventType
get_input_event_type (GdkDeviceXI *device,
                      XEvent *xevent,
		      int *core_x, int *core_y)
{
  if (xevent->type == device->button_press_type)
    {
      XDeviceButtonEvent *xie = (XDeviceButtonEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_BUTTON_PRESS;
    }

  if (xevent->type == device->button_release_type)
    {
      XDeviceButtonEvent *xie = (XDeviceButtonEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_BUTTON_RELEASE;
    }

  if (xevent->type == device->key_press_type)
    {
      XDeviceKeyEvent *xie = (XDeviceKeyEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_KEY_PRESS;
    }

  if (xevent->type == device->key_release_type)
    {
      XDeviceKeyEvent *xie = (XDeviceKeyEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_KEY_RELEASE;
    }

  if (xevent->type == device->motion_notify_type)
    {
      XDeviceMotionEvent *xie = (XDeviceMotionEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_MOTION_NOTIFY;
    }

  if (xevent->type == device->proximity_in_type)
    {
      XProximityNotifyEvent *xie = (XProximityNotifyEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_PROXIMITY_IN;
    }

  if (xevent->type == device->proximity_out_type)
    {
      XProximityNotifyEvent *xie = (XProximityNotifyEvent *)(xevent);
      *core_x = xie->x;
      *core_y = xie->y;
      return GDK_PROXIMITY_OUT;
    }

  *core_x = 0;
  *core_y = 0;
  return GDK_NOTHING;
}

gboolean
_gdk_input_other_event (GdkEvent *event,
			XEvent *xevent,
			GdkWindow *event_window)
{
  GdkWindow *window;
  GdkWindowObject *priv;
  GdkInputWindow *iw;
  GdkDevicePrivate *gdkdev;
  gint return_val;
  GdkEventType event_type;
  int x, y;
  GdkDisplay *display = GDK_WINDOW_DISPLAY (event_window);
  GdkDisplayX11 *display_impl = GDK_DISPLAY_X11 (display);

  /* This is a sort of a hack, as there isn't any XDeviceAnyEvent -
     but it's potentially faster than scanning through the types of
     every device. If we were deceived, then it won't match any of
     the types for the device anyways */
  gdkdev = _gdk_input_find_device (display,
				   ((XDeviceButtonEvent *)xevent)->deviceid);
  if (!gdkdev)
    return FALSE;			/* we don't handle it - not an XInput event */

  event_type = get_input_event_type (device, xevent, &x, &y);
  if (event_type == GDK_NOTHING)
    return FALSE;

  /* If we're not getting any event window its likely because we're outside the
     window and there is no grab. We should still report according to the
     implicit grab though. */
  iw = ((GdkWindowObject *)event_window)->input_window;

  if (iw->button_down_window)
    window = iw->button_down_window;
  else
    window = _gdk_window_get_input_window_for_event (event_window,
                                                     event_type,
                                                     x, y,
                                                     xevent->xany.serial);
  priv = (GdkWindowObject *)window;
  if (window == NULL)
    return FALSE;

  if (gdkdev->info.mode == GDK_MODE_DISABLED ||
      priv->extension_events == 0 ||
      !(gdkdev->info.has_cursor || (priv->extension_events & GDK_ALL_DEVICES_MASK)))
    return FALSE;

  if (!display->ignore_core_events && priv->extension_events != 0)
    gdk_input_check_proximity (GDK_WINDOW_DISPLAY (window));

  return_val = _gdk_input_common_other_event (event, xevent, window, gdkdev);

  if (return_val && event->type == GDK_BUTTON_PRESS)
    iw->button_down_window = window;
  if (return_val && event->type == GDK_BUTTON_RELEASE)
    iw->button_down_window = NULL;

  if (return_val && event->type == GDK_PROXIMITY_OUT &&
      display->ignore_core_events)
    gdk_input_check_proximity (GDK_WINDOW_DISPLAY (window));

  return return_val;
}

#endif

gint
_gdk_input_grab_pointer (GdkWindow      *window,
			 GdkWindow      *native_window, /* This is the toplevel */
			 gint            owner_events,
			 GdkEventMask    event_mask,
			 GdkWindow *     confine_to,
			 guint32         time)
{
  GdkInputWindow *input_window;
  GdkWindowObject *priv, *impl_window;
  gboolean need_ungrab;
  GdkDeviceXI *device;
  GList *tmp_list;
  XEventClass event_classes[GDK_MAX_DEVICE_CLASSES];
  gint num_classes;
  gint result;
  GdkDisplayX11 *display_impl  = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));

  tmp_list = display_impl->input_windows;
  need_ungrab = FALSE;

  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;

      if (input_window->grabbed)
	{
	  input_window->grabbed = FALSE;
	  need_ungrab = TRUE;
	  break;
	}
      tmp_list = tmp_list->next;
    }

  priv = (GdkWindowObject *)window;
  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);
  input_window = impl_window->input_window;
  if (priv->extension_events)
    {
      g_assert (input_window != NULL);
      input_window->grabbed = TRUE;

      tmp_list = display_impl->input_devices;
      while (tmp_list)
	{
          device = tmp_list->data;

	  if (!GDK_IS_CORE (GDK_DEVICE (device)) && device->xdevice)
	    {
	      _gdk_input_common_find_events (GDK_DEVICE (device), event_mask,
					     event_classes, &num_classes);
#ifdef G_ENABLE_DEBUG
	      if (_gdk_debug_flags & GDK_DEBUG_NOGRABS)
		result = GrabSuccess;
	      else
#endif
		result = XGrabDevice (display_impl->xdisplay, device->xdevice,
				      GDK_WINDOW_XWINDOW (native_window),
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
          device = tmp_list->data;

	  if (!GDK_IS_CORE (GDK_DEVICE (device)) && device->xdevice &&
	      ((device->button_state != 0) || need_ungrab))
	    {
	      XUngrabDevice (display_impl->xdisplay, device->xdevice, time);
	      device->button_state = 0;
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
  GdkDeviceXI *device;
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
          device = tmp_list->data;

	  if (!GDK_IS_CORE (GDK_DEVICE (device)) && device->xdevice)
	    XUngrabDevice (display_impl->xdisplay, device->xdevice, time);

	  tmp_list = tmp_list->next;
	}
    }
}

#define __GDK_INPUT_XFREE_C__
#include "gdkaliasdef.c"
