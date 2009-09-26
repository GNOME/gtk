/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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

#include "gdkdevicemanager-xi.h"
#include "gdkeventtranslator.h"
#include "gdkdevice-xi.h"
#include "gdkintl.h"
#include "gdkx.h"

#include <X11/extensions/XInput.h>

#define GDK_DEVICE_MANAGER_XI_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GDK_TYPE_DEVICE_MANAGER_XI, GdkDeviceManagerXIPrivate))

typedef struct GdkDeviceManagerXIPrivate GdkDeviceManagerXIPrivate;

struct GdkDeviceManagerXIPrivate
{
  GHashTable *id_table;
  gint event_base;
  GList *devices;
};

static void gdk_device_manager_xi_constructed  (GObject      *object);
static void gdk_device_manager_xi_finalize     (GObject      *object);
static void gdk_device_manager_xi_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec);
static void gdk_device_manager_xi_get_property (GObject      *object,
                                                guint         prop_id,
                                                GValue       *value,
                                                GParamSpec   *pspec);

static void     gdk_device_manager_xi_event_translator_init  (GdkEventTranslatorIface *iface);
static gboolean gdk_device_manager_xi_translate_event (GdkEventTranslator *translator,
                                                       GdkDisplay         *display,
                                                       GdkEvent           *event,
                                                       XEvent             *xevent);
static GList *  gdk_device_manager_xi_get_devices     (GdkDeviceManager  *device_manager,
                                                       GdkDeviceType      type);


G_DEFINE_TYPE_WITH_CODE (GdkDeviceManagerXI, gdk_device_manager_xi, GDK_TYPE_DEVICE_MANAGER_CORE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_EVENT_TRANSLATOR,
                                                gdk_device_manager_xi_event_translator_init))

enum {
  PROP_0,
  PROP_EVENT_BASE
};

static void
gdk_device_manager_xi_class_init (GdkDeviceManagerXIClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);

  object_class->constructed = gdk_device_manager_xi_constructed;
  object_class->finalize = gdk_device_manager_xi_finalize;
  object_class->set_property = gdk_device_manager_xi_set_property;
  object_class->get_property = gdk_device_manager_xi_get_property;

  device_manager_class->get_devices = gdk_device_manager_xi_get_devices;

  g_object_class_install_property (object_class,
				   PROP_EVENT_BASE,
				   g_param_spec_int ("event-base",
                                                     P_("Event base"),
                                                     P_("Event base for XInput events"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (GdkDeviceManagerXIPrivate));
}

static void
gdk_device_manager_xi_init (GdkDeviceManagerXI *device_manager)
{
  GdkDeviceManagerXIPrivate *priv;

  priv = GDK_DEVICE_MANAGER_XI_GET_PRIVATE (device_manager);
  priv->id_table = g_hash_table_new_full (NULL, NULL, NULL,
                                          (GDestroyNotify) g_object_unref);
}

static void
translate_class_info (GdkDevice   *device,
                      XDeviceInfo *info)
{
  XAnyClassPtr class;
  gint i, j;

  class = info->inputclassinfo;

  for (i = 0; i < info->num_classes; i++)
    {
      switch (class->class) {
      case ButtonClass:
	break;
      case KeyClass:
	{
#if 0
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
#endif

	  break;
	}
      case ValuatorClass:
	{
	  XValuatorInfo *xvi = (XValuatorInfo *)class;

          device->num_axes = xvi->num_axes;
	  device->axes = g_new0 (GdkDeviceAxis, xvi->num_axes);
#if 0
	  gdkdev->axes = g_new (GdkAxisInfo, xvi->num_axes);

          for (j = 0; j < xvi->num_axes; j++)
	    {
	      gdkdev->axes[j].resolution =
		gdkdev->axes[j].xresolution = xvi->axes[j].resolution;
	      gdkdev->axes[j].min_value =
		gdkdev->axes[j].xmin_value = xvi->axes[j].min_value;
	      gdkdev->axes[j].max_value =
		gdkdev->axes[j].xmax_value = xvi->axes[j].max_value;
	      gdkdev->info.axes[j].use = GDK_AXIS_IGNORE;
	    }
#endif
	  j=0;

          if (j < xvi->num_axes)
	    gdk_device_set_axis_use (device, j++, GDK_AXIS_X);
	  if (j < xvi->num_axes)
	    gdk_device_set_axis_use (device, j++, GDK_AXIS_Y);
	  if (j < xvi->num_axes)
	    gdk_device_set_axis_use (device, j++, GDK_AXIS_PRESSURE);
	  if (j < xvi->num_axes)
	    gdk_device_set_axis_use (device, j++, GDK_AXIS_XTILT);
	  if (j < xvi->num_axes)
	    gdk_device_set_axis_use (device, j++, GDK_AXIS_YTILT);
	  if (j < xvi->num_axes)
	    gdk_device_set_axis_use (device, j++, GDK_AXIS_WHEEL);

	  break;
	}
      }

      class = (XAnyClassPtr) (((char *) class) + class->length);
    }
}

static GdkDevice *
create_device (GdkDisplay  *display,
               XDeviceInfo *info)
{
  GdkDevice *device;

  device = g_object_new (GDK_TYPE_DEVICE_XI,
                         "name", info->name,
                         "input-source", GDK_SOURCE_MOUSE,
                         "input-mode", GDK_MODE_DISABLED,
                         "has-cursor", FALSE,
                         "display", display,
                         "device-id", info->id,
                         NULL);
  translate_class_info (device, info);

  return device;
}

static void
gdk_device_manager_xi_constructed (GObject *object)
{
  GdkDeviceManagerXIPrivate *priv;
  XDeviceInfo *devices;
  gint i, num_devices;
  GdkDisplay *display;

  priv = GDK_DEVICE_MANAGER_XI_GET_PRIVATE (object);
  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (object));
  devices = XListInputDevices(GDK_DISPLAY_XDISPLAY (display), &num_devices);

  for(i = 0; i < num_devices; i++)
    {
      GdkDevice *device;

      device = create_device (display, &devices[i]);
      priv->devices = g_list_prepend (priv->devices, device);

      g_hash_table_insert (priv->id_table,
                           GINT_TO_POINTER (devices[i].id),
                           device);
    }

  XFreeDeviceList(devices);

  gdk_x11_register_standard_event_type (display,
                                        priv->event_base,
                                        15 /* Number of events */);

  if (G_OBJECT_CLASS (gdk_device_manager_xi_parent_class)->constructed)
    G_OBJECT_CLASS (gdk_device_manager_xi_parent_class)->constructed (object);
}

static void
gdk_device_manager_xi_finalize (GObject *object)
{
  GdkDeviceManagerXIPrivate *priv;

  priv = GDK_DEVICE_MANAGER_XI_GET_PRIVATE (object);

  g_list_foreach (priv->devices, (GFunc) g_object_unref, NULL);
  g_list_free (priv->devices);

  G_OBJECT_CLASS (gdk_device_manager_xi_parent_class)->finalize (object);
}

static void
gdk_device_manager_xi_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GdkDeviceManagerXIPrivate *priv;

  priv = GDK_DEVICE_MANAGER_XI_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_EVENT_BASE:
      priv->event_base = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_manager_xi_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GdkDeviceManagerXIPrivate *priv;

  priv = GDK_DEVICE_MANAGER_XI_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_EVENT_BASE:
      g_value_set_int (value, priv->event_base);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_manager_xi_event_translator_init (GdkEventTranslatorIface *iface)
{
  iface->translate_event = gdk_device_manager_xi_translate_event;
}

static void
gdk_input_translate_coordinates (GdkDevice *device,
				 GdkWindow *window,
				 gint      *axis_data,
				 gdouble   *axis_out,
				 gdouble   *x_out,
				 gdouble   *y_out)
{
#if 0
  GdkWindowObject *priv, *impl_window;

  int i;
  int x_axis = 0;
  int y_axis = 0;

  double device_width, device_height;
  double x_offset, y_offset, x_scale, y_scale;

  priv = (GdkWindowObject *) window;
  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);

  for (i = 0; i < device->num_axes; i++)
    {
      switch (device->axes[i].use)
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

  device_width = gdkdev->axes[x_axis].max_value - gdkdev->axes[x_axis].min_value;
  device_height = gdkdev->axes[y_axis].max_value - gdkdev->axes[y_axis].min_value;

  if (device->mode == GDK_MODE_SCREEN)
    {
      x_scale = gdk_screen_get_width (gdk_drawable_get_screen (window)) / device_width;
      y_scale = gdk_screen_get_height (gdk_drawable_get_screen (window)) / device_height;

      x_offset = - impl_window->input_window->root_x - priv->abs_x;
      y_offset = - impl_window->input_window->root_y - priv->abs_y;
    }
  else				/* GDK_MODE_WINDOW */
    {
      double x_resolution = gdkdev->axes[x_axis].resolution;
      double y_resolution = gdkdev->axes[y_axis].resolution;
      double device_aspect;
      /*
       * Some drivers incorrectly report the resolution of the device
       * as zero (in partiular linuxwacom < 0.5.3 with usb tablets).
       * This causes the device_aspect to become NaN and totally
       * breaks windowed mode.  If this is the case, the best we can
       * do is to assume the resolution is non-zero is equal in both
       * directions (which is true for many devices).  The absolute
       * value of the resolution doesn't matter since we only use the
       * ratio.
       */
      if ((x_resolution == 0) || (y_resolution == 0))
	{
	  x_resolution = 1;
	  y_resolution = 1;
	}
      device_aspect = (device_height * y_resolution) /
	(device_width * x_resolution);
      if (device_aspect * priv->width >= priv->height)
	{
	  /* device taller than window */
	  x_scale = priv->width / device_width;
	  y_scale = (x_scale * x_resolution) / y_resolution;

	  x_offset = 0;
	  y_offset = - (device_height * y_scale - priv->height) / 2;
	}
      else
	{
	  /* window taller than device */
	  y_scale = priv->height / device_height;
	  x_scale = (y_scale * y_resolution) / x_resolution;

	  y_offset = 0;
	  x_offset = - (device_width * x_scale - priv->width) / 2;
	}
    }

  for (i=0; i<gdkdev->info.num_axes; i++)
    {
      switch (gdkdev->info.axes[i].use)
	{
	case GDK_AXIS_X:
	  axis_out[i] = x_offset + x_scale * (axis_data[x_axis] -
	    gdkdev->axes[x_axis].min_value);
	  if (x_out)
	    *x_out = axis_out[i];
	  break;
	case GDK_AXIS_Y:
	  axis_out[i] = y_offset + y_scale * (axis_data[y_axis] -
	    gdkdev->axes[y_axis].min_value);
	  if (y_out)
	    *y_out = axis_out[i];
	  break;
	default:
	  axis_out[i] =
	    (device->axes[i].max * (axis_data[i] - gdkdev->axes[i].min_value) +
	     device->axes[i].min * (gdkdev->axes[i].max_value - axis_data[i])) /
	    (gdkdev->axes[i].max_value - gdkdev->axes[i].min_value);
	  break;
	}
    }
#endif
}

/* combine the state of the core device and the device state
 * into one - for now we do this in a simple-minded manner -
 * we just take the keyboard portion of the core device and
 * the button portion (all of?) the device state.
 * Any button remapping should go on here.
 */
static guint
gdk_input_translate_state (guint state, guint device_state)
{
  return device_state | (state & 0xFF);
}

static GdkDevice *
lookup_device (GdkDeviceManagerXI *device_manager,
               XEvent             *xevent)
{
  GdkDeviceManagerXIPrivate *priv;
  guint32 device_id;

  priv = GDK_DEVICE_MANAGER_XI_GET_PRIVATE (device_manager);

  /* This is a sort of a hack, as there isn't any XDeviceAnyEvent -
     but it's potentially faster than scanning through the types of
     every device. If we were deceived, then it won't match any of
     the types for the device anyways */
  device_id = ((XDeviceButtonEvent *)xevent)->deviceid;

  return g_hash_table_lookup (priv->id_table, GINT_TO_POINTER (device_id));
}

static gboolean
gdk_device_manager_xi_translate_event (GdkEventTranslator *translator,
                                       GdkDisplay         *display,
                                       GdkEvent           *event,
                                       XEvent             *xevent)
{
  GdkEventTranslatorIface *parent_iface;
  GdkWindowObject *priv, *impl_window;
  GdkInputWindow *input_window;
  GdkDeviceXI *device_xi;
  GdkDevice *device;
  GdkWindow *window;

  parent_iface = g_type_interface_peek_parent (GDK_EVENT_TRANSLATOR_GET_IFACE (translator));

  if (parent_iface->translate_event (translator, display, event, xevent))
    return TRUE;

  device = lookup_device (GDK_DEVICE_MANAGER_XI (translator), xevent);
  device_xi = GDK_DEVICE_XI (device);

  if (!device)
    return FALSE;

  window = gdk_window_lookup_for_display (display, xevent->xany.window);
  priv = (GdkWindowObject *) window;

  if (!window)
    return FALSE;

  impl_window = (GdkWindowObject *) _gdk_window_get_impl_window (window);
  input_window = impl_window->input_window;

  if ((xevent->type == device_xi->button_press_type) ||
      (xevent->type == device_xi->button_release_type))
    {
      XDeviceButtonEvent *xdbe = (XDeviceButtonEvent *) xevent;

      if (xdbe->type == device_xi->button_press_type)
	{
	  event->button.type = GDK_BUTTON_PRESS;
#if 0
	  gdkdev->button_state |= 1 << xdbe->button;
#endif
	}
      else
	{
	  event->button.type = GDK_BUTTON_RELEASE;
#if 0
	  gdkdev->button_state &= ~(1 << xdbe->button);
#endif
	}

      event->button.device = device;
      event->button.window = window;
      event->button.time = xdbe->time;

      event->button.axes = g_new (gdouble, device->num_axes);
      gdk_input_translate_coordinates (device, window, xdbe->axis_data,
				       event->button.axes,
				       &event->button.x, &event->button.y);
#if 0
      event->button.x_root = event->button.x + priv->abs_x + input_window->root_x;
      event->button.y_root = event->button.y + priv->abs_y + input_window->root_y;
#endif
      event->button.state = gdk_input_translate_state (xdbe->state, xdbe->device_state);
      event->button.button = xdbe->button;

      if (event->button.type == GDK_BUTTON_PRESS)
	_gdk_event_button_generate (gdk_drawable_get_display (event->button.window),
				    event);

      GDK_NOTE (EVENTS,
	g_print ("button %s:\t\twindow: %ld  device: %ld  x,y: %f %f  button: %d\n",
		 (event->button.type == GDK_BUTTON_PRESS) ? "press" : "release",
		 xdbe->window,
		 xdbe->deviceid,
		 event->button.x, event->button.y,
		 xdbe->button));

      /* Update the timestamp of the latest user interaction, if the event has
       * a valid timestamp.
       */
      if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
	gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
				      gdk_event_get_time (event));
      return TRUE;
  }

  if ((xevent->type == device_xi->key_press_type) ||
      (xevent->type == device_xi->key_release_type))
    {
      XDeviceKeyEvent *xdke = (XDeviceKeyEvent *) xevent;

      GDK_NOTE (EVENTS,
	g_print ("device key %s:\twindow: %ld  device: %ld  keycode: %d\n",
		 (event->key.type == GDK_KEY_PRESS) ? "press" : "release",
		 xdke->window,
		 xdke->deviceid,
		 xdke->keycode));

#if 0
      if (xdke->keycode < gdkdev->min_keycode ||
	  xdke->keycode >= gdkdev->min_keycode + gdkdev->info.num_keys)
	{
	  g_warning ("Invalid device key code received");
	  return FALSE;
	}

      event->key.keyval = device->keys[xdke->keycode - gdkdev->min_keycode].keyval;
#endif

      if (event->key.keyval == 0)
	{
	  GDK_NOTE (EVENTS,
	    g_print ("\t\ttranslation - NONE\n"));

	  return FALSE;
	}

      event->key.type = (xdke->type == device_xi->key_press_type) ?
	GDK_KEY_PRESS : GDK_KEY_RELEASE;

      event->key.window = window;
      event->key.time = xdke->time;

#if 0
      event->key.state = gdk_input_translate_state (xdke->state, xdke->device_state)
	| device->keys[xdke->keycode - device_xi->min_keycode].modifiers;
#endif

      /* Add a string translation for the key event */
      if ((event->key.keyval >= 0x20) && (event->key.keyval <= 0xFF))
	{
	  event->key.length = 1;
	  event->key.string = g_new (gchar, 2);
	  event->key.string[0] = (gchar) event->key.keyval;
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

      /* Update the timestamp of the latest user interaction, if the event has
       * a valid timestamp.
       */
      if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
	gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
				      gdk_event_get_time (event));
      return TRUE;
    }

  if (xevent->type == device_xi->motion_notify_type)
    {
      XDeviceMotionEvent *xdme = (XDeviceMotionEvent *) xevent;

      event->motion.device = device;

      event->motion.axes = g_new (gdouble, device->num_axes);
      gdk_input_translate_coordinates (device, window, xdme->axis_data,
                                       event->motion.axes,
                                       &event->motion.x, &event->motion.y);
#if 0
      event->motion.x_root = event->motion.x + priv->abs_x + input_window->root_x;
      event->motion.y_root = event->motion.y + priv->abs_y + input_window->root_y;
#endif

      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = window;
      event->motion.time = xdme->time;
      event->motion.state = gdk_input_translate_state (xdme->state,
                                                       xdme->device_state);
      event->motion.is_hint = xdme->is_hint;

      GDK_NOTE (EVENTS,
	g_print ("motion notify:\t\twindow: %ld  device: %ld  x,y: %f %f  state %#4x  hint: %s\n",
		 xdme->window,
		 xdme->deviceid,
		 event->motion.x, event->motion.y,
		 event->motion.state,
		 (xdme->is_hint) ? "true" : "false"));


      /* Update the timestamp of the latest user interaction, if the event has
       * a valid timestamp.
       */
      if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
	gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
				      gdk_event_get_time (event));
      return TRUE;
    }

  if (xevent->type == device_xi->proximity_in_type ||
      xevent->type == device_xi->proximity_out_type)
    {
      XProximityNotifyEvent *xpne = (XProximityNotifyEvent *) xevent;

      event->proximity.device = device;
      event->proximity.type = (xevent->type == device_xi->proximity_in_type) ?
	GDK_PROXIMITY_IN : GDK_PROXIMITY_OUT;
      event->proximity.window = window;
      event->proximity.time = xpne->time;

      /* Update the timestamp of the latest user interaction, if the event has
       * a valid timestamp.
       */
      if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
	gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
				      gdk_event_get_time (event));
      return TRUE;
  }

  return FALSE;
}

static GList *
gdk_device_manager_xi_get_devices (GdkDeviceManager *device_manager,
                                   GdkDeviceType     type)
{
  GdkDeviceManagerXIPrivate *priv;

  if (type == GDK_DEVICE_TYPE_MASTER)
    return GDK_DEVICE_MANAGER_CLASS (gdk_device_manager_xi_parent_class)->get_devices (device_manager, type);
  else if (type == GDK_DEVICE_TYPE_FLOATING)
    {
      priv = GDK_DEVICE_MANAGER_XI_GET_PRIVATE (device_manager);
      return g_list_copy (priv->devices);
    }
  else
    return NULL;
}
