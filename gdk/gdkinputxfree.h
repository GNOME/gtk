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

#ifdef XINPUT_XFREE

/* forward declarations */

static gint gdk_input_xfree_set_mode (guint32 deviceid, GdkInputMode mode);
static void gdk_input_check_proximity (void);
static void gdk_input_xfree_configure_event (XConfigureEvent *xevent, 
					     GdkWindow *window);
static void gdk_input_xfree_enter_event (XCrossingEvent *xevent, 
				       GdkWindow *window);
static gint gdk_input_xfree_other_event (GdkEvent *event, 
					 XEvent *xevent, 
					 GdkWindow *window);
static gint gdk_input_xfree_enable_window(GdkWindow *window, 
					  GdkDevicePrivate *gdkdev);
static gint gdk_input_xfree_disable_window(GdkWindow *window,
					   GdkDevicePrivate *gdkdev);
static gint gdk_input_xfree_grab_pointer (GdkWindow *     window,
					gint            owner_events,
					GdkEventMask    event_mask,
					GdkWindow *     confine_to,
					guint32         time);
static void gdk_input_xfree_ungrab_pointer (guint32 time);

void 
gdk_input_init(void)
{
  gdk_input_vtable.set_mode           = gdk_input_xfree_set_mode;
  gdk_input_vtable.set_axes           = gdk_input_common_set_axes;
  gdk_input_vtable.set_key            = gdk_input_common_set_key;
  gdk_input_vtable.motion_events      = gdk_input_common_motion_events;
  gdk_input_vtable.get_pointer	      = gdk_input_common_get_pointer;
  gdk_input_vtable.grab_pointer	      = gdk_input_xfree_grab_pointer;
  gdk_input_vtable.ungrab_pointer     = gdk_input_xfree_ungrab_pointer;
  gdk_input_vtable.configure_event    = gdk_input_xfree_configure_event;
  gdk_input_vtable.enter_event        = gdk_input_xfree_enter_event;
  gdk_input_vtable.other_event        = gdk_input_xfree_other_event;
  gdk_input_vtable.window_none_event  = NULL;
  gdk_input_vtable.enable_window      = gdk_input_xfree_enable_window;
  gdk_input_vtable.disable_window     = gdk_input_xfree_disable_window;

  gdk_input_ignore_core = FALSE;
  gdk_input_common_init(FALSE);
}

static gint
gdk_input_xfree_set_mode (guint32 deviceid, GdkInputMode mode)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;
  GdkInputMode old_mode;
  GdkInputWindow *input_window;

  gdkdev = gdk_input_find_device(deviceid);
  g_return_val_if_fail (gdkdev != NULL,FALSE);
  old_mode = gdkdev->info.mode;

  if (gdkdev->info.mode == mode)
    return TRUE;

  gdkdev->info.mode = mode;

  if (mode == GDK_MODE_WINDOW)
    {
      gdkdev->info.has_cursor = FALSE;
      for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	{
	  input_window = (GdkInputWindow *)tmp_list->data;
	  if (input_window->mode != GDK_EXTENSION_EVENTS_CURSOR)
	    gdk_input_enable_window (input_window->window, gdkdev);
	  else
	    if (old_mode != GDK_MODE_DISABLED)
	      gdk_input_disable_window (input_window->window, gdkdev);
	}
    }
  else if (mode == GDK_MODE_SCREEN)
    {
      gdkdev->info.has_cursor = TRUE;
      for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	gdk_input_enable_window (((GdkInputWindow *)tmp_list->data)->window,
				 gdkdev);
    }
  else  /* mode == GDK_MODE_DISABLED */
    {
      for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	{
	  input_window = (GdkInputWindow *)tmp_list->data;
	  if (old_mode != GDK_MODE_WINDOW ||
	      input_window->mode != GDK_EXTENSION_EVENTS_CURSOR)
	    gdk_input_disable_window (input_window->window, gdkdev);
	}
    }

  return TRUE;
  
}

static void
gdk_input_check_proximity (void)
{
  gint new_proximity = 0;
  GList *tmp_list = gdk_input_devices;

  while (tmp_list && !new_proximity) 
    {
      GdkDevicePrivate *gdkdev = (GdkDevicePrivate *)(tmp_list->data);

      if (gdkdev->info.mode != GDK_MODE_DISABLED 
	  && gdkdev->info.deviceid != GDK_CORE_POINTER
	  && gdkdev->xdevice)
	{
	  XDeviceState *state = XQueryDeviceState(GDK_DISPLAY(),
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
 	}
      tmp_list = tmp_list->next;
    }

  gdk_input_ignore_core = new_proximity;
}

static void
gdk_input_xfree_configure_event (XConfigureEvent *xevent, GdkWindow *window)
{
  GdkInputWindow *input_window;
  gint root_x, root_y;

  input_window = gdk_input_window_find(window);
  g_return_if_fail (window != NULL);

  gdk_input_get_root_relative_geometry(GDK_DISPLAY(),GDK_WINDOW_XWINDOW(window),
				 &root_x, 
				 &root_y, NULL, NULL);

  input_window->root_x = root_x;
  input_window->root_y = root_y;
}

static void 
gdk_input_xfree_enter_event (XCrossingEvent *xevent, 
			     GdkWindow      *window)
{
  GdkInputWindow *input_window;
  gint root_x, root_y;

  input_window = gdk_input_window_find(window);
  g_return_if_fail (window != NULL);

  gdk_input_check_proximity();

  gdk_input_get_root_relative_geometry(GDK_DISPLAY(),GDK_WINDOW_XWINDOW(window),
				 &root_x, 
				 &root_y, NULL, NULL);

  input_window->root_x = root_x;
  input_window->root_y = root_y;
}

static gint 
gdk_input_xfree_other_event (GdkEvent *event, 
			     XEvent *xevent, 
			     GdkWindow *window)
{
  GdkInputWindow *input_window;

  GdkDevicePrivate *gdkdev;
  gint return_val;

  input_window = gdk_input_window_find(window);
  g_return_val_if_fail (window != NULL, -1);

  /* This is a sort of a hack, as there isn't any XDeviceAnyEvent -
     but it's potentially faster than scanning through the types of
     every device. If we were deceived, then it won't match any of
     the types for the device anyways */
  gdkdev = gdk_input_find_device(((XDeviceButtonEvent *)xevent)->deviceid);

  if (!gdkdev) {
    return -1;			/* we don't handle it - not an XInput event */
  }

  /* FIXME: It would be nice if we could just get rid of the events 
     entirely, instead of having to ignore them */
  if (gdkdev->info.mode == GDK_MODE_DISABLED ||
      (gdkdev->info.mode == GDK_MODE_WINDOW 
       && input_window->mode == GDK_EXTENSION_EVENTS_CURSOR))
    return FALSE;
  
  if (!gdk_input_ignore_core)
    gdk_input_check_proximity();

  return_val = gdk_input_common_other_event (event, xevent, 
					     input_window, gdkdev);

  if (return_val > 0 && event->type == GDK_PROXIMITY_OUT &&
      gdk_input_ignore_core)
    gdk_input_check_proximity();

  return return_val;
}

static gint
gdk_input_xfree_enable_window(GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  /* FIXME: watchout, gdkdev might be core pointer, never opened */
  gdk_input_common_select_events (window, gdkdev);
  return TRUE;
}

static gint
gdk_input_xfree_disable_window(GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  gdk_input_common_select_events (window, gdkdev);
  return TRUE;
}

static gint 
gdk_input_xfree_grab_pointer (GdkWindow *     window,
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

  tmp_list = gdk_input_windows;
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
      
      tmp_list = gdk_input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (gdkdev->info.deviceid != GDK_CORE_POINTER &&
	      gdkdev->xdevice)
	    {
	      gdk_input_common_find_events (window, gdkdev,
					    event_mask,
					    event_classes, &num_classes);
	      
	      result = XGrabDevice( GDK_DISPLAY(), gdkdev->xdevice,
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
      tmp_list = gdk_input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (gdkdev->info.deviceid != GDK_CORE_POINTER && gdkdev->xdevice &&
	      ((gdkdev->button_state != 0) || need_ungrab))
	    {
	      XUngrabDevice( gdk_display, gdkdev->xdevice, time);
	      gdkdev->button_state = 0;
	    }
	  
	  tmp_list = tmp_list->next;
	}
    }

  return Success;
      
}

static void 
gdk_input_xfree_ungrab_pointer (guint32 time)
{
  GdkInputWindow *input_window;
  GdkDevicePrivate *gdkdev;
  GList *tmp_list;

  tmp_list = gdk_input_windows;
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

      tmp_list = gdk_input_devices;
      while (tmp_list)
	{
	  gdkdev = (GdkDevicePrivate *)tmp_list->data;
	  if (gdkdev->info.deviceid != GDK_CORE_POINTER && gdkdev->xdevice)
	    XUngrabDevice( gdk_display, gdkdev->xdevice, time);

	  tmp_list = tmp_list->next;
	}
    }
}

#endif /* XINPUT_XFREE */
