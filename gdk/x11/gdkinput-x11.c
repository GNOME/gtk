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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gdkinputprivate.h"
#include "gdkinternals.h"
#include "gdkx.h"
#include "gdk.h"		/* For gdk_error_trap_push()/pop() */

#include <string.h>

/* Forward declarations */
static GdkDevicePrivate *gdk_input_device_new(XDeviceInfo *device, 
					      gint include_core);
static void gdk_input_translate_coordinates(GdkDevicePrivate *gdkdev,
					    GdkInputWindow   *input_window,
					    gint             *axis_data,
					    gdouble          *axis_out,
					    gdouble          *x_out,
					    gdouble          *y_out);
static guint gdk_input_translate_state(guint state, guint device_state);

/* Global variables */

GdkDevicePrivate *
gdk_input_find_device (guint32 id)
{
  GList *tmp_list = gdk_input_devices;
  GdkDevicePrivate *gdkdev;
  while (tmp_list)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (gdkdev->deviceid == id)
	return gdkdev;
      tmp_list = tmp_list->next;
    }
  return NULL;
}

void
gdk_input_get_root_relative_geometry(Display *dpy, Window w, int *x_ret, int *y_ret,
				     int *width_ret, int *height_ret)
{
  Window root, parent, child;
  Window *children;
  guint nchildren;
  gint x,y;
  guint width, height;
  guint border_widthc, depthc;
   
  XQueryTree (dpy, w, &root, &parent, &children, &nchildren);
  if (children)
    XFree(children);
  
  XGetGeometry (dpy, w, &root, &x, &y, &width, &height, &border_widthc, &depthc);

  XTranslateCoordinates (dpy, w, root, 0, 0, &x, &y, &child);
 
  if (x_ret)
    *x_ret = x;
  if (y_ret)
    *y_ret = y;
  if (width_ret)
    *width_ret = width;
  if (height_ret)
    *height_ret = height;
}

static GdkDevicePrivate *
gdk_input_device_new (XDeviceInfo *device, gint include_core)
{
  GdkDevicePrivate *gdkdev;
  gchar *tmp_name;
  XAnyClassPtr class;
  gint i,j;

  gdkdev = g_new(GdkDevicePrivate,1);

  gdkdev->deviceid = device->id;
  if (device->name[0])
    gdkdev->info.name = g_strdup (device->name);
 else
   /* XFree86 3.2 gives an empty name to the default core devices,
      (fixed in 3.2A) */
   gdkdev->info.name = g_strdup ("pointer");

  gdkdev->info.mode = GDK_MODE_DISABLED;

  /* Try to figure out what kind of device this is by its name -
     could invite a very, very, long list... Lowercase name
     for comparison purposes */

  tmp_name = g_strdup (gdkdev->info.name);
  g_strdown (tmp_name);
  
  if (!strcmp (tmp_name, "pointer"))
    gdkdev->info.source = GDK_SOURCE_MOUSE;
  else if (!strcmp (tmp_name, "wacom") ||
	   !strcmp (tmp_name, "pen"))
    gdkdev->info.source = GDK_SOURCE_PEN;
  else if (!strcmp (tmp_name, "eraser"))
    gdkdev->info.source = GDK_SOURCE_ERASER;
  else if (!strcmp (tmp_name, "cursor"))
    gdkdev->info.source = GDK_SOURCE_CURSOR;
  else
    gdkdev->info.source = GDK_SOURCE_PEN;

  g_free(tmp_name);

  gdkdev->xdevice = NULL;

  /* step through the classes */

  gdkdev->info.num_axes = 0;
  gdkdev->info.num_keys = 0;
  gdkdev->info.axes = NULL;
  gdkdev->info.keys = NULL;
  gdkdev->axes = 0;
  gdkdev->info.has_cursor = 0;
  gdkdev->needs_update = FALSE;
  gdkdev->claimed = FALSE;
  gdkdev->button_state = 0;

  class = device->inputclassinfo;
  for (i=0;i<device->num_classes;i++) 
    {
      switch (class->class) {
      case ButtonClass:
	break;
      case KeyClass:
	{
	  XKeyInfo *xki = (XKeyInfo *)class;
	  /* Hack to catch XFree86 3.3.1 bug. Other devices better
	   * not have exactly 25 keys... 
	   */
	  if ((xki->min_keycode == 8) && (xki->max_keycode == 32))
	    {
	      gdkdev->info.num_keys = 32;
	      gdkdev->min_keycode = 1;
	    }
	  else
	    {
	      gdkdev->info.num_keys = xki->max_keycode - xki->min_keycode + 1;
	      gdkdev->min_keycode = xki->min_keycode;
	    }
	  gdkdev->info.keys = g_new (GdkDeviceKey, gdkdev->info.num_keys);

	  for (j=0; j<gdkdev->info.num_keys; j++)
	    {
	      gdkdev->info.keys[j].keyval = 0;
	      gdkdev->info.keys[j].modifiers = 0;
	    }

	  break;
	}
      case ValuatorClass:
	{
	  XValuatorInfo *xvi = (XValuatorInfo *)class;
	  gdkdev->info.num_axes = xvi->num_axes;
	  gdkdev->axes = g_new (GdkAxisInfo, xvi->num_axes);
	  gdkdev->info.axes = g_new0 (GdkDeviceAxis, xvi->num_axes);
	  for (j=0;j<xvi->num_axes;j++)
	    {
	      gdkdev->axes[j].resolution = 
		gdkdev->axes[j].xresolution = xvi->axes[j].resolution;
	      gdkdev->axes[j].min_value =
		gdkdev->axes[j].xmin_value = xvi->axes[j].min_value;
	      gdkdev->axes[j].max_value =
		gdkdev->axes[j].xmax_value = xvi->axes[j].max_value;
	      gdkdev->info.axes[j].use = GDK_AXIS_IGNORE;
	    }
	  j=0;
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_X);
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_Y);
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_PRESSURE);
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_XTILT);
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_YTILT);
	  if (j<xvi->num_axes)
	    gdk_device_set_axis_use (&gdkdev->info, j++, GDK_AXIS_WHEEL);
		       
	  break;
	}
      }
      class = (XAnyClassPtr)(((char *)class) + class->length);
    }
  /* return NULL if no axes */
  if (!gdkdev->info.num_axes || !gdkdev->axes ||
      (!include_core && device->use == IsXPointer))
    goto error;

  if (device->use != IsXPointer)
    {
      gdk_error_trap_push ();
      gdkdev->xdevice = XOpenDevice(gdk_display, gdkdev->deviceid);

      /* return NULL if device is not ready */
      if (gdk_error_trap_pop ())
	goto error;
    }

  gdkdev->buttonpress_type = 0;
  gdkdev->buttonrelease_type = 0;
  gdkdev->keypress_type = 0;
  gdkdev->keyrelease_type = 0;
  gdkdev->motionnotify_type = 0;
  gdkdev->proximityin_type = 0;
  gdkdev->proximityout_type = 0;
  gdkdev->changenotify_type = 0;

  return gdkdev;

 error:

  g_free (gdkdev->info.name);
  if (gdkdev->axes)
    g_free (gdkdev->axes);
  if (gdkdev->info.keys)
    g_free (gdkdev->info.keys);
  if (gdkdev->info.axes)
    g_free (gdkdev->info.axes);
  g_free (gdkdev);
  
  return NULL;
}

void
gdk_input_common_find_events(GdkWindow *window,
			     GdkDevicePrivate *gdkdev,
			     gint mask,
			     XEventClass *classes,
			     int *num_classes)
{
  gint i;
  XEventClass class;
  
  i = 0;
  /* We have to track press and release events in pairs to keep
     track of button state correctly and implement grabbing for
     the gxi support */
  if (mask & GDK_BUTTON_PRESS_MASK || mask & GDK_BUTTON_RELEASE_MASK)
    {
      DeviceButtonPress (gdkdev->xdevice, gdkdev->buttonpress_type,
			     class);
      if (class != 0)
	  classes[i++] = class;
      DeviceButtonPressGrab (gdkdev->xdevice, 0, class);
      if (class != 0)
	  classes[i++] = class;
      DeviceButtonRelease (gdkdev->xdevice, gdkdev->buttonrelease_type,
			   class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_POINTER_MOTION_MASK)
    {
      DeviceMotionNotify  (gdkdev->xdevice, gdkdev->motionnotify_type, class);
      if (class != 0)
	  classes[i++] = class;
    }
  else
    if (mask & (GDK_BUTTON1_MOTION_MASK | GDK_BUTTON2_MOTION_MASK |
		GDK_BUTTON3_MOTION_MASK | GDK_BUTTON_MOTION_MASK |
		GDK_POINTER_MOTION_HINT_MASK))
      {
	/* Make sure gdkdev->motionnotify_type is set */
	DeviceMotionNotify  (gdkdev->xdevice, gdkdev->motionnotify_type, class);
      }
  if (mask & GDK_BUTTON1_MOTION_MASK)
    {
      DeviceButton1Motion  (gdkdev->xdevice, 0, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_BUTTON2_MOTION_MASK)
    {
      DeviceButton2Motion  (gdkdev->xdevice, 0, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_BUTTON3_MOTION_MASK)
    {
      DeviceButton3Motion  (gdkdev->xdevice, 0, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_BUTTON_MOTION_MASK)
    {
      DeviceButtonMotion  (gdkdev->xdevice, 0, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_POINTER_MOTION_HINT_MASK)
    {
      /* We'll get into trouble if the macros change, but at least we'll
	 know about it, and we avoid warnings now */
      DevicePointerMotionHint (gdkdev->xdevice, 0, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_KEY_PRESS_MASK)
    {
      DeviceKeyPress (gdkdev->xdevice, gdkdev->keypress_type, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_KEY_RELEASE_MASK)
    {
      DeviceKeyRelease (gdkdev->xdevice, gdkdev->keyrelease_type, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_PROXIMITY_IN_MASK)
    {
      ProximityIn   (gdkdev->xdevice, gdkdev->proximityin_type, class);
      if (class != 0)
	  classes[i++] = class;
    }
  if (mask & GDK_PROXIMITY_OUT_MASK)
    {
      ProximityOut  (gdkdev->xdevice, gdkdev->proximityout_type, class);
      if (class != 0)
	  classes[i++] = class;
    }

  *num_classes = i;
}

void
gdk_input_common_select_events(GdkWindow *window,
			       GdkDevicePrivate *gdkdev)
{
  XEventClass classes[GDK_MAX_DEVICE_CLASSES];
  gint num_classes;

  if (gdkdev->info.mode == GDK_MODE_DISABLED)
    gdk_input_common_find_events(window, gdkdev, 0, classes, &num_classes);
  else
    gdk_input_common_find_events(window, gdkdev, 
				 ((GdkWindowObject *)window)->extension_events,
				 classes, &num_classes);
  
  XSelectExtensionEvent (gdk_display,
			 GDK_WINDOW_XWINDOW(window),
			 classes, num_classes);
}

gint 
gdk_input_common_init(gint include_core)
{
  char **extensions;
  XDeviceInfo   *devices;
  int num_devices;
  int num_extensions, loop;
  Display *display = gdk_display;

  /* Init XInput extension */
  
  extensions = XListExtensions(display, &num_extensions);
  for (loop = 0; loop < num_extensions &&
	 (strcmp(extensions[loop], "XInputExtension") != 0); loop++);
  XFreeExtensionList(extensions);
  gdk_input_devices = NULL;
  if (loop < num_extensions)
    {
      /* XInput extension found */

      devices = XListInputDevices(display, &num_devices);
  
      for(loop=0; loop<num_devices; loop++)
	{
	  GdkDevicePrivate *gdkdev = gdk_input_device_new(&devices[loop],
						      include_core);
	  if (gdkdev)
	    gdk_input_devices = g_list_append(gdk_input_devices, gdkdev);
	}
      XFreeDeviceList(devices);
    }

  gdk_input_devices = g_list_append (gdk_input_devices, gdk_core_pointer);

  return TRUE;
}

static void
gdk_input_translate_coordinates (GdkDevicePrivate *gdkdev,
				 GdkInputWindow   *input_window,
				 gint             *axis_data,
				 gdouble          *axis_out,
				 gdouble          *x_out,
				 gdouble          *y_out)
{
  GdkWindowImplX11 *impl;
  int i;
  int x_axis = 0;
  int y_axis = 0;

  double device_width, device_height;
  double x_offset, y_offset, x_scale, y_scale;

  impl = GDK_WINDOW_IMPL_X11 (((GdkWindowObject *) input_window->window)->impl);

  for (i=0; i<gdkdev->info.num_axes; i++)
    {
      switch (gdkdev->info.axes[i].use)
	{
	case GDK_AXIS_X:
	  x_axis = i;
	  break;
	case GDK_AXIS_Y:
	  y_axis = i;
	  break;
	default:
	  break;
	}
    }
  
  device_width = gdkdev->axes[x_axis].max_value - 
		   gdkdev->axes[x_axis].min_value;
  device_height = gdkdev->axes[y_axis].max_value - 
                    gdkdev->axes[y_axis].min_value;

  if (gdkdev->info.mode == GDK_MODE_SCREEN) 
    {
      x_scale = gdk_screen_width() / device_width;
      y_scale = gdk_screen_height() / device_height;

      x_offset = - input_window->root_x;
      y_offset = - input_window->root_y;
    }
  else				/* GDK_MODE_WINDOW */
    {
      double device_aspect = (device_height*gdkdev->axes[y_axis].resolution) /
	(device_width*gdkdev->axes[x_axis].resolution);

      if (device_aspect * impl->width >= impl->height)
	{
	  /* device taller than window */
	  x_scale = impl->width / device_width;
	  y_scale = (x_scale * gdkdev->axes[x_axis].resolution)
	    / gdkdev->axes[y_axis].resolution;

	  x_offset = 0;
	  y_offset = -(device_height * y_scale - 
			       impl->height)/2;
	}
      else
	{
	  /* window taller than device */
	  y_scale = impl->height / device_height;
	  x_scale = (y_scale * gdkdev->axes[y_axis].resolution)
	    / gdkdev->axes[x_axis].resolution;

	  y_offset = 0;
	  x_offset = - (device_width * x_scale - impl->width)/2;
	}
    }

  for (i=0; i<gdkdev->info.num_axes; i++)
    {
      switch (gdkdev->info.axes[i].use)
	{
	case GDK_AXIS_X:
	  axis_out[i] = x_offset + x_scale*axis_data[x_axis];
	  if (x_out)
	    *x_out = axis_out[i];
	  break;
	case GDK_AXIS_Y:
	  axis_out[i] = y_offset + y_scale*axis_data[y_axis];
	  if (y_out)
	    *y_out = axis_out[i];
	  break;
	default:
	  axis_out[i] =
	    (gdkdev->info.axes[i].max * (axis_data[i] - gdkdev->axes[i].min_value) +
	     gdkdev->info.axes[i].min * (gdkdev->axes[i].max_value - axis_data[i])) /
	    (gdkdev->axes[i].max_value - gdkdev->axes[i].min_value);
	  break;
	}
    }
}

/* combine the state of the core device and the device state
 * into one - for now we do this in a simple-minded manner -
 * we just take the keyboard portion of the core device and
 * the button portion (all of?) the device state.
 * Any button remapping should go on here.
 */
static guint
gdk_input_translate_state(guint state, guint device_state)
{
  return device_state | (state & 0xFF);
}


gint 
gdk_input_common_other_event (GdkEvent         *event,
			      XEvent           *xevent,
			      GdkInputWindow   *input_window,
			      GdkDevicePrivate *gdkdev)
{
  if ((xevent->type == gdkdev->buttonpress_type) ||
      (xevent->type == gdkdev->buttonrelease_type)) 
    {
      XDeviceButtonEvent *xdbe = (XDeviceButtonEvent *)(xevent);

      if (xdbe->type == gdkdev->buttonpress_type)
	{
	  event->button.type = GDK_BUTTON_PRESS;
	  gdkdev->button_state |= 1 << xdbe->button;
	}
      else
	{
	  event->button.type = GDK_BUTTON_RELEASE;
	  gdkdev->button_state &= ~(1 << xdbe->button);
	}
      event->button.device = &gdkdev->info;
      event->button.window = input_window->window;
      event->button.time = xdbe->time;

      event->button.axes = g_new (gdouble, gdkdev->info.num_axes);
      gdk_input_translate_coordinates (gdkdev,input_window, xdbe->axis_data,
				       event->button.axes, 
				       &event->button.x,&event->button.y);
      event->button.state = gdk_input_translate_state(xdbe->state,xdbe->device_state);
      event->button.button = xdbe->button;

      GDK_NOTE (EVENTS,
	g_print ("button %s:\t\twindow: %ld  device: %ld  x,y: %f %f  button: %d\n",
		 (event->button.type == GDK_BUTTON_PRESS) ? "press" : "release",
		 xdbe->window,
		 xdbe->deviceid,
		 event->button.x, event->button.y,
		 xdbe->button));

      return TRUE;
  }

  if ((xevent->type == gdkdev->keypress_type) ||
      (xevent->type == gdkdev->keyrelease_type))
    {
      XDeviceKeyEvent *xdke = (XDeviceKeyEvent *)(xevent);

      GDK_NOTE (EVENTS,
	g_print ("device key %s:\twindow: %ld  device: %ld  keycode: %d\n",
		 (event->key.type == GDK_KEY_PRESS) ? "press" : "release",
		 xdke->window,
		 xdke->deviceid,
		 xdke->keycode));

      if (xdke->keycode < gdkdev->min_keycode ||
	  xdke->keycode >= gdkdev->min_keycode + gdkdev->info.num_keys)
	{
	  g_warning ("Invalid device key code received");
	  return FALSE;
	}
      
      event->key.keyval = gdkdev->info.keys[xdke->keycode - gdkdev->min_keycode].keyval;

      if (event->key.keyval == 0) 
	{
	  GDK_NOTE (EVENTS,
	    g_print ("\t\ttranslation - NONE\n"));
	  
	  return FALSE;
	}

      event->key.type = (xdke->type == gdkdev->keypress_type) ?
	GDK_KEY_PRESS : GDK_KEY_RELEASE;

      event->key.window = input_window->window;
      event->key.time = xdke->time;

      event->key.state = gdk_input_translate_state(xdke->state, xdke->device_state)
	| gdkdev->info.keys[xdke->keycode - gdkdev->min_keycode].modifiers;

      /* Add a string translation for the key event */
      if ((event->key.keyval >= 0x20) && (event->key.keyval <= 0xFF))
	{
	  event->key.length = 1;
	  event->key.string = g_new (gchar, 2);
	  event->key.string[0] = (gchar)event->key.keyval;
	  event->key.string[1] = 0;
	}
      else
	{
	  event->key.length = 0;
	  event->key.string = g_new0 (gchar, 1);
	}

      GDK_NOTE (EVENTS,
	g_print ("\t\ttranslation - keyval: %d modifiers: %#x\n",
		 event->key.keyval,
		 event->key.state));

      return TRUE;
    }

  if (xevent->type == gdkdev->motionnotify_type) 
    {
      XDeviceMotionEvent *xdme = (XDeviceMotionEvent *)(xevent);

      event->motion.device = &gdkdev->info;
      
      event->motion.axes = g_new (gdouble, gdkdev->info.num_axes);
      gdk_input_translate_coordinates(gdkdev,input_window,xdme->axis_data,
				      event->motion.axes,
				      &event->motion.x,&event->motion.y);

      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = input_window->window;
      event->motion.time = xdme->time;
      event->motion.state = gdk_input_translate_state(xdme->state,
						      xdme->device_state);
      event->motion.is_hint = xdme->is_hint;

      GDK_NOTE (EVENTS,
	g_print ("motion notify:\t\twindow: %ld  device: %ld  x,y: %f %f  state %#4x  hint: %s\n",
		 xdme->window,
		 xdme->deviceid,
		 event->motion.x, event->motion.y,
		 event->motion.state,
		 (xdme->is_hint) ? "true" : "false"));
      
      
      return TRUE;
    }

  if (xevent->type == gdkdev->proximityin_type ||
      xevent->type == gdkdev->proximityout_type)
    {
      XProximityNotifyEvent *xpne = (XProximityNotifyEvent *)(xevent);

      event->proximity.device = &gdkdev->info;
      event->proximity.type = (xevent->type == gdkdev->proximityin_type)?
  	GDK_PROXIMITY_IN:GDK_PROXIMITY_OUT;
      event->proximity.window = input_window->window;
      event->proximity.time = xpne->time;
      
      return TRUE;
  }

  return -1;			/* wasn't one of our event types */
}

gboolean
_gdk_device_get_history (GdkDevice         *device,
			 GdkWindow         *window,
			 guint32            start,
			 guint32            stop,
			 GdkTimeCoord    ***events,
			 gint              *n_events)
{
  GdkTimeCoord **coords;
  XDeviceTimeCoord *device_coords;
  GdkInputWindow *input_window;
  GdkDevicePrivate *gdkdev;
  gint mode_return;
  gint axis_count_return;
  gint i;

  gdkdev = (GdkDevicePrivate *)device;
  input_window = gdk_input_window_find (window);

  g_return_val_if_fail (input_window != NULL, FALSE);

  device_coords = XGetDeviceMotionEvents (gdk_display,
					  gdkdev->xdevice,
					  start, stop,
					  n_events, &mode_return,
					  &axis_count_return);

  if (device_coords)
    {
      coords = _gdk_device_allocate_history (device, *n_events);
      
      for (i=0; i<*n_events; i++)
	gdk_input_translate_coordinates (gdkdev, input_window,
					 device_coords[i].data,
					 coords[i]->axes, NULL, NULL);
      XFreeDeviceMotionEvents (device_coords);

      *events = coords;

      return TRUE;
    }
  else
    {
      *events = NULL;
      *n_events = 0;

      return FALSE;
    }
}

void 
gdk_device_get_state (GdkDevice       *device,
		      GdkWindow       *window,
		      gdouble         *axes,
		      GdkModifierType *mask)
{
  gint i;

  g_return_if_fail (device != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_IS_CORE (device))
    {
      gint x_int, y_int;
      
      gdk_window_get_pointer (window, &x_int, &y_int, mask);

      if (axes)
	{
	  axes[0] = x_int;
	  axes[1] = y_int;
	}
    }
  else
    {
      GdkDevicePrivate *gdkdev;
      GdkInputWindow *input_window;
      XDeviceState *state;
      XInputClass *input_class;
      
      if (mask)
	gdk_window_get_pointer (window, NULL, NULL, mask);
      
      gdkdev = (GdkDevicePrivate *)device;
      input_window = gdk_input_window_find (window);
      g_return_if_fail (input_window != NULL);

      state = XQueryDeviceState (gdk_display, gdkdev->xdevice);
      input_class = state->data;
      for (i=0; i<state->num_classes; i++)
	{
	  switch (input_class->class)
	    {
	    case ValuatorClass:
	      if (axes)
		gdk_input_translate_coordinates (gdkdev, input_window,
						 ((XValuatorState *)input_class)->valuators,
						 axes, NULL, NULL);
	      break;
	      
	    case ButtonClass:
	      if (mask)
		{
		  *mask &= 0xFF;
		  if (((XButtonState *)input_class)->num_buttons > 0)
		    *mask |= ((XButtonState *)input_class)->buttons[0] << 7;
		  /* GDK_BUTTON1_MASK = 1 << 8, and button n is stored
		   * in bit 1<<(n%8) in byte n/8. n = 1,2,... */
		}
	      break;
	    }
	  input_class = (XInputClass *)(((char *)input_class)+input_class->length);
	}
      XFreeDeviceState (state);
    }
}
