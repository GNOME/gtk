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

#ifdef XINPUT_GXI

/* #define DEBUG_SWITCHING */

#include <gxid_lib.h>

/* Forward declarations */
static void gdk_input_gxi_select_notify (GdkDevicePrivate *gdkdev);
static gint gdk_input_gxi_set_mode (guint32 deviceid, GdkInputMode mode);
static gint gdk_input_is_extension_device (guint32 deviceid);
static void gdk_input_gxi_configure_event (XConfigureEvent *xevent, 
					   GdkWindow *window);
static void gdk_input_gxi_enter_event (XCrossingEvent *xevent, 
				       GdkWindow *window);
static gint gdk_input_gxi_other_event (GdkEvent *event, 
				       XEvent *xevent, 
				       GdkWindow *window);
static void gdk_input_gxi_update_device (GdkDevicePrivate *gdkdev);

static gint gdk_input_gxi_window_none_event (GdkEvent *event, XEvent *xevent);
static gint gdk_input_gxi_enable_window (GdkWindow *window, 
					 GdkDevicePrivate *gdkdev);
static gint gdk_input_gxi_disable_window (GdkWindow *window, 
					  GdkDevicePrivate *gdkdev);
static Window gdk_input_find_root_child(Display *dpy, Window w);
static void gdk_input_compute_obscuring(GdkInputWindow *input_window);
static gint gdk_input_is_obscured(GdkInputWindow *input_window, gdouble x, 
				  gdouble y);
static GdkTimeCoord *gdk_input_gxi_motion_events (GdkWindow *window,
						  guint32 deviceid,
						  guint32 start,
						  guint32 stop,
						  gint *nevents_return);
static void gdk_input_gxi_get_pointer (GdkWindow       *window,
				       guint32	   deviceid,
				       gdouble         *x,
				       gdouble         *y,
				       gdouble         *pressure,
				       gdouble         *xtilt,
				       gdouble         *ytilt,
				       GdkModifierType *mask);
static gint gdk_input_gxi_grab_pointer (GdkWindow *     window,
					gint            owner_events,
					GdkEventMask    event_mask,
					GdkWindow *     confine_to,
					guint32         time);
static void gdk_input_gxi_ungrab_pointer (guint32 time);

/* Local variables */

static GdkDevicePrivate *gdk_input_current_device;
static GdkDevicePrivate *gdk_input_core_pointer;

void
gdk_input_init(void)
{
  GList *tmp_list;
  
  gdk_input_vtable.set_mode           = gdk_input_gxi_set_mode;
  gdk_input_vtable.set_axes           = gdk_input_common_set_axes;
  gdk_input_vtable.set_key            = gdk_input_common_set_key;
  gdk_input_vtable.motion_events      = gdk_input_gxi_motion_events;
  gdk_input_vtable.get_pointer	      = gdk_input_gxi_get_pointer;
  gdk_input_vtable.grab_pointer	      = gdk_input_gxi_grab_pointer;
  gdk_input_vtable.ungrab_pointer     = gdk_input_gxi_ungrab_pointer;
  gdk_input_vtable.configure_event    = gdk_input_gxi_configure_event;
  gdk_input_vtable.enter_event        = gdk_input_gxi_enter_event;
  gdk_input_vtable.other_event        = gdk_input_gxi_other_event;
  gdk_input_vtable.window_none_event  = gdk_input_gxi_window_none_event;
  gdk_input_vtable.enable_window      = gdk_input_gxi_enable_window;
  gdk_input_vtable.disable_window     = gdk_input_gxi_disable_window;

  gdk_input_ignore_core = FALSE;
  gdk_input_core_pointer = NULL;

  if (!gdk_input_gxid_host) 
    {
      gdk_input_gxid_host = getenv("GXID_HOST");
    }
  if (!gdk_input_gxid_port) 
    {
      char *t = getenv("GXID_PORT");
      if (t)
	gdk_input_gxid_port = atoi(t);
    }
  
  gdk_input_common_init(TRUE);

  /* find initial core pointer */
  
  for (tmp_list = gdk_input_devices; tmp_list; tmp_list = tmp_list->next) 
    {
      GdkDevicePrivate *gdkdev = (GdkDevicePrivate *)tmp_list->data;
      if (gdk_input_is_extension_device(gdkdev->info.deviceid))
	{
	  gdk_input_gxi_select_notify (gdkdev);
	}
      else
	{
	  if (gdkdev->info.deviceid != GDK_CORE_POINTER)
	    gdk_input_core_pointer = gdkdev;
	}
    }
}

static void
gdk_input_gxi_select_notify (GdkDevicePrivate *gdkdev)
{
  XEventClass class;

  ChangeDeviceNotify  (gdkdev->xdevice, gdkdev->changenotify_type, class);

  XSelectExtensionEvent (gdk_display, gdk_root_window, &class, 1);
}

/* Set the core pointer. Device should already be enabled. */
static gint
gdk_input_gxi_set_core_pointer(GdkDevicePrivate *gdkdev)
{
  int x_axis,y_axis;

  g_return_val_if_fail(gdkdev->xdevice,FALSE);

  x_axis = gdkdev->axis_for_use[GDK_AXIS_X];
  y_axis = gdkdev->axis_for_use[GDK_AXIS_Y];

  g_return_val_if_fail(x_axis != -1 && y_axis != -1,FALSE);

  /* core_pointer might not be up to date so we check with the server
     before change the pointer */

  if ( !gdk_input_is_extension_device(gdkdev->info.deviceid) )
    {
#if 0
      if (gdkdev != gdk_input_core_pointer)
	g_warning("core pointer inconsistency");
#endif      
      return TRUE;
    }

  if ( XChangePointerDevice(gdk_display,gdkdev->xdevice, x_axis, y_axis) 
       != Success )
    {
      return FALSE;
    }
  else
    {
      gdk_input_gxi_update_device (gdk_input_core_pointer);
      gdk_input_core_pointer = gdkdev;
      return TRUE;
    }
}


/* FIXME, merge with gdk_input_xfree_set_mode */

static gint
gdk_input_gxi_set_mode (guint32 deviceid, GdkInputMode mode)
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

  if (old_mode != GDK_MODE_DISABLED)
    {
      for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	{
	  input_window = (GdkInputWindow *)tmp_list->data;
	  if (input_window->mode != GDK_EXTENSION_EVENTS_CURSOR)
	    gdk_input_disable_window (input_window->window, gdkdev);
	}
    }
  
  if (mode != GDK_MODE_DISABLED)
    {
      for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	{
	  input_window = (GdkInputWindow *)tmp_list->data;
	  if (input_window->mode != GDK_EXTENSION_EVENTS_CURSOR)
	    if (!gdk_input_enable_window(input_window->window, gdkdev))
	      {
		gdk_input_set_mode(deviceid, old_mode);
		return FALSE;
	      }
	}
    }

  return TRUE;

}

gint
gdk_input_is_extension_device (guint32 deviceid)
{
  XDeviceInfo   *devices;
  int num_devices, loop;

  if (deviceid == GDK_CORE_POINTER)
    return FALSE;
  
  devices = XListInputDevices(gdk_display, &num_devices);
  for(loop=0; loop<num_devices; loop++)
    {
      if ((devices[loop].id == deviceid) && (devices[loop].use == IsXExtensionDevice)) 
	{
	  XFreeDeviceList(devices);
	  return TRUE;
	}
    }

  XFreeDeviceList(devices);
  return FALSE;
}

static void
gdk_input_gxi_configure_event (XConfigureEvent *xevent, GdkWindow *window)
{
  GdkInputWindow *input_window;
  gint root_x, root_y;

  input_window = gdk_input_window_find(window);
  g_return_if_fail (input_window != NULL);

  gdk_input_get_root_relative_geometry(gdk_display,GDK_WINDOW_XWINDOW(window),
				 &root_x, &root_y, NULL, NULL);
  input_window->root_x = root_x;
  input_window->root_y = root_y;
  gdk_input_compute_obscuring(input_window);
}

static void
gdk_input_gxi_enter_event (XCrossingEvent *xevent, GdkWindow *window)
{
  GdkInputWindow *input_window;

  input_window = gdk_input_window_find(window);
  g_return_if_fail (input_window != NULL);

  gdk_input_compute_obscuring(input_window);
}

static gint 
gdk_input_gxi_other_event (GdkEvent *event, 
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

  if (gdkdev->info.mode == GDK_MODE_DISABLED ||
      input_window->mode == GDK_EXTENSION_EVENTS_CURSOR)
    return FALSE;
  
  if (gdkdev != gdk_input_current_device &&
      xevent->type != gdkdev->changenotify_type)
    {
      gdk_input_current_device = gdkdev;
    }

  return_val = gdk_input_common_other_event (event, xevent, 
					     input_window, gdkdev);

  if (return_val > 0 && event->type == GDK_MOTION_NOTIFY &&
      (!gdkdev->button_state) && (!input_window->grabbed) &&
      ((event->motion.x < 0) || (event->motion.y < 0) ||
       (event->motion.x > ((GdkWindowPrivate *)window)->width) || 
       (event->motion.y > ((GdkWindowPrivate *)window)->height) ||
       gdk_input_is_obscured(input_window,event->motion.x,event->motion.y)))
    {
#ifdef DEBUG_SWITCHING
      g_print("gdkinput: Setting core pointer to %d on motion at (%f,%f)\n",
	      gdkdev->info.deviceid,event->motion.x,event->motion.y);
      g_print("   window geometry is: %dx%d\n",
	      ((GdkWindowPrivate *)window)->width,
	      ((GdkWindowPrivate *)window)->height);
#endif      
      gdk_input_gxi_set_core_pointer(gdkdev);
      return FALSE;
    }
  else
    return return_val;

}

static void
gdk_input_gxi_update_device (GdkDevicePrivate *gdkdev)
{
  GList *t;

  if (gdk_input_is_extension_device (gdkdev->info.deviceid))
    {
      if (!gdkdev->xdevice)
	{
	  gdkdev->xdevice = XOpenDevice(gdk_display, gdkdev->info.deviceid);
	  gdk_input_gxi_select_notify (gdkdev);
	  gdkdev->needs_update = 1;
	}
      if (gdkdev->needs_update && gdkdev->xdevice)
	{
	  for (t = gdk_input_windows; t; t = t->next)
	    gdk_input_common_select_events (((GdkInputWindow *)t->data)->window,
					 gdkdev);
	  gdkdev->needs_update = 0;
	}
    }
}

static gint 
gdk_input_gxi_window_none_event (GdkEvent *event, XEvent *xevent)
{
  GdkDevicePrivate *gdkdev = 
    gdk_input_find_device(((XDeviceButtonEvent *)xevent)->deviceid);

  if (!gdkdev) {
    return -1;			/* we don't handle it - not an XInput event */
  }

  if (xevent->type == gdkdev->changenotify_type)
    {
      if (gdk_input_core_pointer != gdkdev)
	{
#ifdef DEBUG_SWITCHING
	  g_print("ChangeNotify from %d to %d:\n",
		  gdk_input_core_pointer->info.deviceid,
		  gdkdev->info.deviceid);
#endif
	  gdk_input_gxi_update_device (gdk_input_core_pointer);
	  gdk_input_core_pointer = gdkdev;
	}
    }
		
  return FALSE;
}

static gint
gdk_input_gxi_enable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  GdkInputWindow *input_window;

  input_window = gdk_input_window_find (window);
  g_return_val_if_fail (input_window != NULL, FALSE);

  if (!gdkdev->claimed)
    {
      if (gxid_claim_device(gdk_input_gxid_host, gdk_input_gxid_port,
			    gdkdev->info.deviceid,
			    GDK_WINDOW_XWINDOW(window), FALSE) !=
	  GXID_RETURN_OK)
	{
	  g_warning("Could not get device (is gxid running?)\n");
	  return FALSE;
	}
      gdkdev->claimed = TRUE;
    }

  if (gdkdev->xdevice && gdkdev != gdk_input_core_pointer)
    gdk_input_common_select_events(window, gdkdev);
  else
    gdkdev->needs_update = TRUE;
  
  return TRUE;
}

static gint
gdk_input_gxi_disable_window(GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  GdkInputWindow *input_window;

  input_window = gdk_input_window_find (window);
  g_return_val_if_fail (input_window != NULL, FALSE);

  if (gdkdev->claimed)
    {
      gxid_release_device(gdk_input_gxid_host, gdk_input_gxid_port,
			  gdkdev->info.deviceid,
			  GDK_WINDOW_XWINDOW(window));

      gdkdev->claimed = FALSE;
    }

  if (gdkdev->xdevice && gdkdev != gdk_input_core_pointer)
    gdk_input_common_select_events(window, gdkdev);
  else
    gdkdev->needs_update = TRUE;
  
  return TRUE;
}

static gint
gdk_input_is_obscured(GdkInputWindow *input_window, gdouble x, gdouble y)
{
  int i;
  for (i=0;i<input_window->num_obscuring;i++)
    {
      GdkRectangle *rect = &input_window->obscuring[i];
      if ((x >= rect->x) &&
	  (y >= rect->y) &&
	  (x < rect->x + rect->width) &&
	  (y < rect->y + rect->height))
	return TRUE;
    }
  return FALSE;
}

/* If this routine needs fixing, the corresponding routine
   in gxid.c will need it too. */

static Window 
gdk_input_find_root_child(Display *dpy, Window w)
{
  Window root,parent;
  Window *children;
  int nchildren;

  parent = w;
  do 
    {
      w = parent;
      XQueryTree(dpy,w,&root,&parent,&children,&nchildren);
      if (children) XFree(children);
    } 
  while (parent != root);
  
  return w;
}

void
gdk_input_compute_obscuring(GdkInputWindow *input_window)
{
  int i;
  int x,y,width,height;
  int xc,yc,widthc,heightc,border_widthc,depthc;

  Window root,parent;
  Window *children;
  int nchildren;

  Window w = GDK_WINDOW_XWINDOW(input_window->window);
  Window root_child = gdk_input_find_root_child(gdk_display,w);
  gdk_input_get_root_relative_geometry(gdk_display,w,&x,&y,&width,&height);

  input_window->root_x = x;
  input_window->root_y = y;

  XQueryTree(gdk_display,GDK_ROOT_WINDOW(),
	     &root,&parent,&children,&nchildren);


  if (input_window->obscuring)
    g_free(input_window->obscuring);
  input_window->obscuring = 0;
  input_window->num_obscuring = 0;

  for (i=0;i<nchildren;i++)
    if (children[i] == root_child)
      break;

  if (i>=nchildren-1)
    {
      if (nchildren)
	XFree(children);
      return;
    }

  input_window->obscuring = g_new(GdkRectangle,(nchildren-i-1));

  for (i=i+1;i<nchildren;i++)
    {
      int xmin, xmax, ymin, ymax;
      XGetGeometry(gdk_display,children[i],&root,&xc,&yc,&widthc,&heightc,
		   &border_widthc, &depthc);
      xmin = xc>x ? xc : x;
      xmax = (xc+widthc)<(x+width) ? xc+widthc : x+width;
      ymin = yc>y ? yc : y;
      ymax = (yc+heightc)<(y+height) ? yc+heightc : y+height;
      if ((xmin < xmax) && (ymin < ymax))
	{
	  XWindowAttributes attributes;
	  XGetWindowAttributes(gdk_display,children[i],&attributes);
	  if (attributes.map_state == IsViewable)
	    {
	      GdkRectangle *rect = &input_window->obscuring[input_window->num_obscuring];
	      
	      /* we store the whole window, not just the obscuring part */
	      rect->x = xc - x;
	      rect->y = yc - y;
	      rect->width = widthc;
	      rect->height = heightc;
	      input_window->num_obscuring++;
	    }
	}
    }

  if (nchildren)
    XFree(children);
}

static void 
gdk_input_gxi_get_pointer     (GdkWindow       *window,
			       guint32	   deviceid,
			       gdouble         *x,
			       gdouble         *y,
			       gdouble         *pressure,
			       gdouble         *xtilt,
			       gdouble         *ytilt,
			       GdkModifierType *mask)
{
  GdkDevicePrivate *gdkdev;

  gdkdev = gdk_input_find_device (deviceid);
  g_return_if_fail (gdkdev != NULL);

  if (gdkdev == gdk_input_core_pointer)
    gdk_input_common_get_pointer (window, GDK_CORE_POINTER, x, y,
				  pressure, xtilt, ytilt, mask);
  else
    gdk_input_common_get_pointer (window, deviceid, x, y,
				  pressure, xtilt, ytilt, mask);
}

static GdkTimeCoord *
gdk_input_gxi_motion_events (GdkWindow *window,
			     guint32 deviceid,
			     guint32 start,
			     guint32 stop,
			     gint *nevents_return)
{
  GdkDevicePrivate *gdkdev;

  gdkdev = gdk_input_find_device (deviceid);
  g_return_val_if_fail (gdkdev != NULL, NULL);
  

  if (gdkdev == gdk_input_core_pointer)
    return gdk_input_motion_events (window, GDK_CORE_POINTER, start, stop,
				    nevents_return);
  else
    return gdk_input_common_motion_events (window, deviceid, start, stop,
					   nevents_return);
  
}

static gint 
gdk_input_gxi_grab_pointer (GdkWindow *     window,
			    gint            owner_events,
			    GdkEventMask    event_mask,
			    GdkWindow *     confine_to,
			    guint32         time)
{
  GList *tmp_list;
  GdkInputWindow *input_window;
  GdkDevicePrivate *gdkdev;

  tmp_list = gdk_input_windows;
  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;

      if (input_window->window == window)
	input_window->grabbed = TRUE;
      else if (input_window->grabbed)
	input_window->grabbed = FALSE;

      tmp_list = tmp_list->next;
    }

  tmp_list = gdk_input_devices;
  while (tmp_list)
    {
      gdkdev = (GdkDevicePrivate *)tmp_list->data;
      if (gdkdev->info.deviceid != GDK_CORE_POINTER && 
	  gdkdev->xdevice &&
	  (gdkdev->button_state != 0))
	gdkdev->button_state = 0;
      
      tmp_list = tmp_list->next;
    }

  return Success;
}

static void 
gdk_input_gxi_ungrab_pointer (guint32 time)
{
  GdkInputWindow *input_window;
  GList *tmp_list;

  tmp_list = gdk_input_windows;
  while (tmp_list)
    {
      input_window = (GdkInputWindow *)tmp_list->data;
      if (input_window->grabbed)
	input_window->grabbed = FALSE;
      tmp_list = tmp_list->next;
    }
}

#endif /* XINPUT_GXI */
