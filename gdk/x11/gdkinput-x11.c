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

#if defined(XINPUT_GXI) || defined(XINPUT_XFREE)

/* Forward declarations */
static void gdk_input_get_root_relative_geometry (Display *dpy, Window w, 
						  int *x_ret, int *y_ret,
						  int *width_ret, 
						  int *height_ret);
static GdkDevicePrivate *gdk_input_device_new(XDeviceInfo *device, 
					      gint include_core);
static void gdk_input_common_find_events(GdkWindow *window,
					 GdkDevicePrivate *gdkdev,
					 gint mask,
					 XEventClass *classes,
					 int *num_classes);
static void gdk_input_common_select_events(GdkWindow *window,
					   GdkDevicePrivate *gdkdev);
static void gdk_input_translate_coordinates(GdkDevicePrivate *gdkdev,
					    GdkInputWindow *input_window,
					    gint *axis_data,
					    gdouble *x, gdouble *y,
					    gdouble *pressure,
					    gdouble *xtilt, gdouble *ytilt);
static guint gdk_input_translate_state(guint state, guint device_state);
static gint gdk_input_common_init(gint include_core);
static gint  gdk_input_common_other_event (GdkEvent *event, 
					   XEvent *xevent, 
					   GdkInputWindow *input_window,
					   GdkDevicePrivate *gdkdev);
static void gdk_input_common_set_axes (guint32 deviceid, GdkAxisUse *axes);
static GdkTimeCoord * gdk_input_common_motion_events (GdkWindow *window,
						      guint32 deviceid,
						      guint32 start,
						      guint32 stop,
						      gint *nevents_return);
static void  gdk_input_common_get_pointer     (GdkWindow       *window,
					       guint32	   deviceid,
					       gdouble         *x,
					       gdouble         *y,
					       gdouble         *pressure,
					       gdouble         *xtilt,
					       gdouble         *ytilt,
					       GdkModifierType *mask);

#define GDK_MAX_DEVICE_CLASSES 13

/* Global variables */

static gint gdk_input_root_width;
static gint gdk_input_root_height;

static void
gdk_input_get_root_relative_geometry(Display *dpy, Window w, int *x_ret, int *y_ret,
			       int *width_ret, int *height_ret)
{
  Window root,parent;
  Window *children;
  guint nchildren;
  gint x,y;
  guint width, height;
  gint xc,yc;
  guint widthc,heightc,border_widthc,depthc;
  
  XQueryTree(dpy,w,&root,&parent,&children,&nchildren);
  if (children) XFree(children);
  XGetGeometry(dpy,w,&root,&x,&y,&width,&height,&border_widthc,
	       &depthc);
  x += border_widthc;
  y += border_widthc;

  while (root != parent)
    {
      w = parent;
      XQueryTree(dpy,w,&root,&parent,&children,&nchildren);
      if (children) XFree(children);
      XGetGeometry(dpy,w,&root,&xc,&yc,&widthc,&heightc,
		   &border_widthc,&depthc);
      x += xc + border_widthc;
      y += yc + border_widthc;
    }

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
gdk_input_device_new(XDeviceInfo *device, gint include_core)
{
  GdkDevicePrivate *gdkdev;
  gchar *tmp_name, *p;
  XAnyClassPtr class;
  gint i,j;

  gdkdev = g_new(GdkDevicePrivate,1);

  gdkdev->info.deviceid = device->id;
  if (device->name[0]) {
    gdkdev->info.name = g_new(char, strlen(device->name)+1);
    strcpy(gdkdev->info.name,device->name);
  } else {
    /* XFree86 3.2 gives an empty name to the default core devices,
       (fixed in 3.2A) */
    gdkdev->info.name = g_strdup("pointer");
    strcpy(gdkdev->info.name,"pointer");
    gdkdev->info.source = GDK_SOURCE_MOUSE;
  }

  gdkdev->info.mode = GDK_MODE_DISABLED;

  /* Try to figure out what kind of device this is by its name -
     could invite a very, very, long list... Lowercase name
     for comparison purposes */

  tmp_name = g_strdup(gdkdev->info.name);
  for (p = tmp_name; *p; p++)
    {
      if (*p >= 'A' && *p <= 'Z')
	*p += 'a' - 'A';
    }
  
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
	{
	  break;
	}
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
	  gdkdev->axes = g_new(GdkAxisInfo, xvi->num_axes);
	  gdkdev->info.axes = g_new(GdkAxisUse, xvi->num_axes);
	  for (j=0;j<xvi->num_axes;j++)
	    {
	      gdkdev->axes[j].resolution = 
		gdkdev->axes[j].xresolution = xvi->axes[j].resolution;
	      gdkdev->axes[j].min_value =
		gdkdev->axes[j].xmin_value = xvi->axes[j].min_value;
	      gdkdev->axes[j].max_value =
		gdkdev->axes[j].xmax_value = xvi->axes[j].max_value;
	      gdkdev->info.axes[j] = GDK_AXIS_IGNORE;
	    }
	  j=0;
	  if (j<xvi->num_axes)
	    gdkdev->info.axes[j++] = GDK_AXIS_X;
	  if (j<xvi->num_axes)
	    gdkdev->info.axes[j++] = GDK_AXIS_Y;
	  if (j<xvi->num_axes)
	    gdkdev->info.axes[j++] = GDK_AXIS_PRESSURE;
	  if (j<xvi->num_axes)
	    gdkdev->info.axes[j++] = GDK_AXIS_XTILT;
	  if (j<xvi->num_axes)
	    gdkdev->info.axes[j++] = GDK_AXIS_YTILT;
	  
	  /* set up reverse lookup on axis use */
	  for (j=GDK_AXIS_IGNORE;j<GDK_AXIS_LAST;j++)
	    gdkdev->axis_for_use[j] = -1;
	  
	  for (j=0;j<xvi->num_axes;j++)
	    if (gdkdev->info.axes[j] != GDK_AXIS_IGNORE)
	      gdkdev->axis_for_use[gdkdev->info.axes[j]] = j;
		       
	  break;
	}
      }
      class = (XAnyClassPtr)(((char *)class) + class->length);
    }
  /* return NULL if no axes */
  if (!gdkdev->info.num_axes || !gdkdev->axes ||
      (!include_core && device->use == IsXPointer))
    {
      g_free(gdkdev->info.name);
      if (gdkdev->axes)
	g_free(gdkdev->axes);
      if (gdkdev->info.keys)
	g_free(gdkdev->info.keys);
      g_free(gdkdev);
      return NULL;
    }

  if (device->use != IsXPointer)
    {
      int error_warn = gdk_error_warnings;

      gdk_error_warnings = 0;
      gdk_error_code = 0;
      gdkdev->xdevice = XOpenDevice(gdk_display, gdkdev->info.deviceid);
      gdk_error_warnings = error_warn;

      /* return NULL if device is not ready */
      if (gdk_error_code)
	{
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
}

static void
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

static void
gdk_input_common_select_events(GdkWindow *window,
			       GdkDevicePrivate *gdkdev)
{
  XEventClass classes[GDK_MAX_DEVICE_CLASSES];
  gint num_classes;

  if (gdkdev->info.mode == GDK_MODE_DISABLED)
    gdk_input_common_find_events(window, gdkdev, 0, classes, &num_classes);
  else
    gdk_input_common_find_events(window, gdkdev, 
				 ((GdkWindowPrivate *)window)->extension_events,
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

  /* Init global vars */
  gdk_window_get_geometry(NULL,	/* use root window */
			  NULL,NULL,
			  &gdk_input_root_width,&gdk_input_root_height, 
			  NULL);

  /* Init XInput extension */
  
  extensions = XListExtensions(display, &num_extensions);
  for (loop = 0; loop < num_extensions &&
	 (strcmp(extensions[loop], "XInputExtension") != 0); loop++);
  XFreeExtensionList(extensions);
  if (loop == num_extensions)	/* XInput extension not found */
    return FALSE;

  gdk_input_devices = 0;
  devices = XListInputDevices(display, &num_devices);
  
  for(loop=0; loop<num_devices; loop++)
    {
      GdkDevicePrivate *gdkdev = gdk_input_device_new(&devices[loop],
						      include_core);
      if (gdkdev)
	gdk_input_devices = g_list_append(gdk_input_devices, gdkdev);
    }
  XFreeDeviceList(devices);

  gdk_input_devices = g_list_append (gdk_input_devices, &gdk_input_core_info);

  return TRUE;
}

static void
gdk_input_translate_coordinates (GdkDevicePrivate *gdkdev,
				 GdkInputWindow *input_window,
				 gint *axis_data,
				 gdouble *x, gdouble *y, gdouble *pressure,
				 gdouble *xtilt, gdouble *ytilt)
{
  GdkWindowPrivate *win_priv;

  int x_axis, y_axis, pressure_axis, xtilt_axis, ytilt_axis;

  double device_width, device_height;
  double x_offset, y_offset, x_scale, y_scale;

  win_priv = (GdkWindowPrivate *) input_window->window;

  x_axis = gdkdev->axis_for_use[GDK_AXIS_X];
  y_axis = gdkdev->axis_for_use[GDK_AXIS_Y];
  pressure_axis = gdkdev->axis_for_use[GDK_AXIS_PRESSURE];
  xtilt_axis = gdkdev->axis_for_use[GDK_AXIS_XTILT];
  ytilt_axis = gdkdev->axis_for_use[GDK_AXIS_YTILT];

  device_width = gdkdev->axes[x_axis].max_value - 
		   gdkdev->axes[x_axis].min_value;
  device_height = gdkdev->axes[y_axis].max_value - 
                    gdkdev->axes[y_axis].min_value;

  if (gdkdev->info.mode == GDK_MODE_SCREEN) 
    {
      x_scale = gdk_input_root_width / device_width;
      y_scale = gdk_input_root_height / device_height;

      x_offset = - input_window->root_x;
      y_offset = - input_window->root_y;
    }
  else				/* GDK_MODE_WINDOW */
    {
      double device_aspect = (device_height*gdkdev->axes[y_axis].resolution) /
	(device_width*gdkdev->axes[x_axis].resolution);

      if (device_aspect * win_priv->width >= win_priv->height)
	{
	  /* device taller than window */
	  x_scale = win_priv->width / device_width;
	  y_scale = (x_scale * gdkdev->axes[x_axis].resolution)
	    / gdkdev->axes[y_axis].resolution;

	  x_offset = 0;
	  y_offset = -(device_height * y_scale - 
			       win_priv->height)/2;
	}
      else
	{
	  /* window taller than device */
	  y_scale = win_priv->height / device_height;
	  x_scale = (y_scale * gdkdev->axes[y_axis].resolution)
	    / gdkdev->axes[x_axis].resolution;

	  y_offset = 0;
	  x_offset = - (device_width * x_scale - win_priv->width)/2;
	}
    }
  
  if (x) *x = x_offset + x_scale*axis_data[x_axis];
  if (y) *y = y_offset + y_scale*axis_data[y_axis];

  if (pressure)
    {
      if (pressure_axis != -1)
	*pressure = ((double)axis_data[pressure_axis] 
		     - gdkdev->axes[pressure_axis].min_value) 
	  / (gdkdev->axes[pressure_axis].max_value 
	     - gdkdev->axes[pressure_axis].min_value);
      else
	*pressure = 0.5;
    }

  if (xtilt)
    {
      if (xtilt_axis != -1)
	{
	  *xtilt = 2. * (double)(axis_data[xtilt_axis] - 
				 (gdkdev->axes[xtilt_axis].min_value +
				  gdkdev->axes[xtilt_axis].max_value)/2) /
	    (gdkdev->axes[xtilt_axis].max_value -
	     gdkdev->axes[xtilt_axis].min_value);
	}
      else *xtilt = 0;
    }
  
  if (ytilt)
    {
      if (ytilt_axis != -1)
	{
	  *ytilt = 2. * (double)(axis_data[ytilt_axis] - 
				 (gdkdev->axes[ytilt_axis].min_value +
				  gdkdev->axes[ytilt_axis].max_value)/2) /
	    (gdkdev->axes[ytilt_axis].max_value -
	     gdkdev->axes[ytilt_axis].min_value);
	}
      else
	*ytilt = 0;
    }
}

/* combine the state of the core device and the device state
   into one - for now we do this in a simple-minded manner -
   we just take the keyboard portion of the core device and
   the button portion (all of?) the device state.
   Any button remapping should go on here. */
static guint
gdk_input_translate_state(guint state, guint device_state)
{
  return device_state | (state & 0xFF);
}

static gint 
gdk_input_common_other_event (GdkEvent *event, 
			      XEvent *xevent, 
			      GdkInputWindow *input_window,
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
      event->button.window = input_window->window;
      event->button.time = xdbe->time;
      event->button.source = gdkdev->info.source;
      event->button.deviceid = xdbe->deviceid;

      gdk_input_translate_coordinates (gdkdev,input_window, xdbe->axis_data,
				       &event->button.x,&event->button.y,
				       &event->button.pressure,
				       &event->button.xtilt, 
				       &event->button.ytilt);
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

      gdk_input_translate_coordinates(gdkdev,input_window,xdme->axis_data,
				      &event->motion.x,&event->motion.y,
				      &event->motion.pressure,
				      &event->motion.xtilt, 
				      &event->motion.ytilt);

      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = input_window->window;
      event->motion.time = xdme->time;
      event->motion.deviceid = xdme->deviceid;
      event->motion.state = gdk_input_translate_state(xdme->state,
						      xdme->device_state);
      event->motion.is_hint = xdme->is_hint;
      event->motion.source = gdkdev->info.source;
      event->motion.deviceid = xdme->deviceid;

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

      event->proximity.type = (xevent->type == gdkdev->proximityin_type)?
  	GDK_PROXIMITY_IN:GDK_PROXIMITY_OUT;
      event->proximity.window = input_window->window;
      event->proximity.time = xpne->time;
      event->proximity.source = gdkdev->info.source;
      event->proximity.deviceid = xpne->deviceid;
      
      return TRUE;
  }

  return -1;			/* wasn't one of our event types */
}

static void
gdk_input_common_set_axes (guint32 deviceid, GdkAxisUse *axes)
{
  int i;
  GdkDevicePrivate *gdkdev = gdk_input_find_device(deviceid);
  g_return_if_fail (gdkdev != NULL);

  for (i=GDK_AXIS_IGNORE;i<GDK_AXIS_LAST;i++)
    {
      gdkdev->axis_for_use[i] = -1;
    }

  for (i=0;i<gdkdev->info.num_axes;i++)
    {
      gdkdev->info.axes[i] = axes[i];
      gdkdev->axis_for_use[axes[i]] = i;
    }
}

void gdk_input_common_set_key (guint32 deviceid,
			       guint   index,
			       guint   keyval,
			       GdkModifierType modifiers)
{
  GdkDevicePrivate *gdkdev = gdk_input_find_device(deviceid);
  
  gdkdev = gdk_input_find_device (deviceid);
  g_return_if_fail (gdkdev != NULL);
  g_return_if_fail (index < gdkdev->info.num_keys);

  gdkdev->info.keys[index].keyval = keyval;
  gdkdev->info.keys[index].modifiers = modifiers;
}

static GdkTimeCoord *
gdk_input_common_motion_events (GdkWindow *window,
				guint32 deviceid,
				guint32 start,
				guint32 stop,
				gint *nevents_return)
{
  GdkTimeCoord *coords;
  XDeviceTimeCoord *device_coords;
  GdkInputWindow *input_window;
  GdkDevicePrivate *gdkdev;

  int mode_return;
  int axis_count_return;
  int i;

  gdkdev = gdk_input_find_device (deviceid);
  input_window = gdk_input_window_find (window);

  g_return_val_if_fail (gdkdev != NULL, NULL);
  g_return_val_if_fail (gdkdev->xdevice != NULL, NULL);
  g_return_val_if_fail (input_window != NULL, NULL);

  device_coords = XGetDeviceMotionEvents (gdk_display,
					  gdkdev->xdevice,
					  start, stop,
					  nevents_return, &mode_return,
					  &axis_count_return);

  if (device_coords)
    {
      coords = g_new (GdkTimeCoord, *nevents_return);
      
      for (i=0; i<*nevents_return; i++)
	{
	  gdk_input_translate_coordinates (gdkdev, input_window,
					   device_coords[i].data,
					   &coords[i].x, &coords[i].y,
					   &coords[i].pressure,
					   &coords[i].xtilt, &coords[i].ytilt);
	}
      XFreeDeviceMotionEvents (device_coords);

      return coords;
    }
  else
    return NULL;
}

static void 
gdk_input_common_get_pointer     (GdkWindow       *window,
				  guint32	   deviceid,
				  gdouble         *x,
				  gdouble         *y,
				  gdouble         *pressure,
				  gdouble         *xtilt,
				  gdouble         *ytilt,
				  GdkModifierType *mask)
{
  GdkDevicePrivate *gdkdev;
  GdkInputWindow *input_window;
  XDeviceState *state;
  XInputClass *input_class;
  gint x_int, y_int;
  gint i;

  /* we probably need to get the mask in any case */

  if (deviceid == GDK_CORE_POINTER)
    {
      gdk_window_get_pointer (window, &x_int, &y_int, mask);
      if (x) *x = x_int;
      if (y) *y = y_int;
      if (pressure) *pressure = 0.5;
      if (xtilt) *xtilt = 0;
      if (ytilt) *ytilt = 0;
    }
  else
    {
      if (mask)
	gdk_window_get_pointer (window, NULL, NULL, mask);
      
      gdkdev = gdk_input_find_device (deviceid);
      input_window = gdk_input_window_find (window);

      g_return_if_fail (gdkdev != NULL);
      g_return_if_fail (gdkdev->xdevice != NULL);
      g_return_if_fail (input_window != NULL);

      state = XQueryDeviceState (gdk_display, gdkdev->xdevice);
      input_class = state->data;
      for (i=0; i<state->num_classes; i++)
	{
	  switch (input_class->class)
	    {
	    case ValuatorClass:
	      gdk_input_translate_coordinates (gdkdev, input_window,
					       ((XValuatorState *)input_class)->valuators,
					       x, y, pressure,
					       xtilt, ytilt);
						       
						       
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
    }
}

#endif
