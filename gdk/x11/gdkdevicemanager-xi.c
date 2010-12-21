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

#include "config.h"

#include "gdkx11devicemanager-xi.h"
#include "gdkdevicemanagerprivate-core.h"
#include "gdkdeviceprivate-xi.h"

#include "gdkdevicemanagerprivate.h"
#include "gdkeventtranslator.h"
#include "gdkintl.h"
#include "gdkprivate-x11.h"

#include <X11/extensions/XInput.h>


struct _GdkX11DeviceManagerXI
{
  GdkX11DeviceManagerCore parent_object;

  GHashTable *id_table;
  gint event_base;
  GList *devices;
  gboolean ignore_core_events;
};

struct _GdkX11DeviceManagerXIClass
{
  GdkX11DeviceManagerCoreClass parent_class;
};


static void gdk_x11_device_manager_xi_constructed  (GObject      *object);
static void gdk_x11_device_manager_xi_dispose      (GObject      *object);
static void gdk_x11_device_manager_xi_set_property (GObject      *object,
                                                    guint         prop_id,
                                                    const GValue *value,
                                                    GParamSpec   *pspec);
static void gdk_x11_device_manager_xi_get_property (GObject      *object,
                                                    guint         prop_id,
                                                    GValue       *value,
                                                    GParamSpec   *pspec);

static void     gdk_x11_device_manager_xi_event_translator_init  (GdkEventTranslatorIface *iface);
static gboolean gdk_x11_device_manager_xi_translate_event (GdkEventTranslator *translator,
                                                           GdkDisplay         *display,
                                                           GdkEvent           *event,
                                                           XEvent             *xevent);
static GList *  gdk_x11_device_manager_xi_list_devices    (GdkDeviceManager  *device_manager,
                                                           GdkDeviceType      type);


G_DEFINE_TYPE_WITH_CODE (GdkX11DeviceManagerXI, gdk_x11_device_manager_xi, GDK_TYPE_X11_DEVICE_MANAGER_CORE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_EVENT_TRANSLATOR,
                                                gdk_x11_device_manager_xi_event_translator_init))

enum {
  PROP_0,
  PROP_EVENT_BASE
};

static void
gdk_x11_device_manager_xi_class_init (GdkX11DeviceManagerXIClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);

  object_class->constructed = gdk_x11_device_manager_xi_constructed;
  object_class->dispose = gdk_x11_device_manager_xi_dispose;
  object_class->set_property = gdk_x11_device_manager_xi_set_property;
  object_class->get_property = gdk_x11_device_manager_xi_get_property;

  device_manager_class->list_devices = gdk_x11_device_manager_xi_list_devices;

  g_object_class_install_property (object_class,
                                   PROP_EVENT_BASE,
                                   g_param_spec_int ("event-base",
                                                     P_("Event base"),
                                                     P_("Event base for XInput events"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static GdkFilterReturn
window_input_info_filter (GdkXEvent *xevent,
                          GdkEvent  *event,
                          gpointer   user_data)
{
  GdkDeviceManager *device_manager;
  GdkDisplay *display;
  GdkWindow *window;
  XEvent *xev;

  device_manager = user_data;
  xev = (XEvent *) xevent;

  display = gdk_device_manager_get_display (device_manager);
  window = gdk_x11_window_lookup_for_display (display, xev->xany.window);

  if (window && xev->type == ConfigureNotify)
    _gdk_x11_device_xi_update_window_info (window);

  return GDK_FILTER_CONTINUE;
}

static void
gdk_x11_device_manager_xi_init (GdkX11DeviceManagerXI *device_manager)
{
  device_manager->id_table = g_hash_table_new_full (NULL, NULL, NULL,
                                                   (GDestroyNotify) g_object_unref);

  gdk_window_add_filter (NULL, window_input_info_filter, device_manager);
}

static void
translate_class_info (GdkDevice   *device,
                      XDeviceInfo *info)
{
  GdkX11DeviceXI *device_xi;
  XAnyClassPtr class;
  gint i, j;

  device_xi = GDK_X11_DEVICE_XI (device);
  class = info->inputclassinfo;

  for (i = 0; i < info->num_classes; i++)
    {
      switch (class->class)
        {
        case ButtonClass:
          break;
        case KeyClass:
          {
            XKeyInfo *xki = (XKeyInfo *)class;
            guint num_keys;

            num_keys = xki->max_keycode - xki->min_keycode + 1;
            _gdk_device_set_keys (device, num_keys);

            device_xi->min_keycode = xki->min_keycode;

            break;
          }
        case ValuatorClass:
          {
            XValuatorInfo *xvi = (XValuatorInfo *)class;

            for (j = 0; j < xvi->num_axes; j++)
              {
                GdkAxisUse use;

                switch (j)
                  {
                  case 0:
                    use = GDK_AXIS_X;
                    break;
                  case 1:
                    use = GDK_AXIS_Y;
                    break;
                  case 2:
                    use = GDK_AXIS_PRESSURE;
                    break;
                  case 3:
                    use = GDK_AXIS_XTILT;
                    break;
                  case 4:
                    use = GDK_AXIS_YTILT;
                    break;
                  case 5:
                    use = GDK_AXIS_WHEEL;
                    break;
                  default:
                    use = GDK_AXIS_IGNORE;
                  }

                _gdk_device_add_axis (device,
                                      GDK_NONE,
                                      use,
                                      xvi->axes[j].min_value,
                                      xvi->axes[j].max_value,
                                      xvi->axes[j].resolution);
              }

            break;
          }
      }

      class = (XAnyClassPtr) (((char *) class) + class->length);
    }
}

/* old versions of XI.h don't define these */
#ifndef IsXExtensionKeyboard
#define IsXExtensionKeyboard 3
#define IsXExtensionPointer  4
#endif

static GdkDevice *
create_device (GdkDeviceManager *device_manager,
               GdkDisplay       *display,
               XDeviceInfo      *info)
{
  GdkInputSource input_source;
  GdkDevice *device;

  if (info->use != IsXExtensionPointer &&
      info->use != IsXExtensionKeyboard)
    return NULL;

  if (info->use == IsXExtensionKeyboard)
    input_source = GDK_SOURCE_KEYBOARD;
  else
    {
      gchar *tmp_name;

      tmp_name = g_ascii_strdown (info->name, -1);

      if (strstr (tmp_name, "eraser"))
        input_source = GDK_SOURCE_ERASER;
      else if (strstr (tmp_name, "cursor"))
        input_source = GDK_SOURCE_CURSOR;
      else if (strstr (tmp_name, "wacom") ||
               strstr (tmp_name, "pen"))
        input_source = GDK_SOURCE_PEN;
      else
        input_source = GDK_SOURCE_MOUSE;

      g_free (tmp_name);
    }

  device = g_object_new (GDK_TYPE_X11_DEVICE_XI,
                         "name", info->name,
                         "type", GDK_DEVICE_TYPE_FLOATING,
                         "input-source", input_source,
                         "input-mode", GDK_MODE_DISABLED,
                         "has-cursor", FALSE,
                         "display", display,
                         "device-manager", device_manager,
                         "device-id", info->id,
                         NULL);
  translate_class_info (device, info);

  return device;
}

static void
gdk_x11_device_manager_xi_constructed (GObject *object)
{
  GdkX11DeviceManagerXI *device_manager;
  XDeviceInfo *devices;
  gint i, num_devices;
  GdkDisplay *display;

  device_manager = GDK_X11_DEVICE_MANAGER_XI (object);
  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (object));
  devices = XListInputDevices (GDK_DISPLAY_XDISPLAY (display), &num_devices);

  for(i = 0; i < num_devices; i++)
    {
      GdkDevice *device;

      device = create_device (GDK_DEVICE_MANAGER (object),
                              display, &devices[i]);
      if (device)
        {
          device_manager->devices = g_list_prepend (device_manager->devices, device);
          g_hash_table_insert (device_manager->id_table,
                               GINT_TO_POINTER (devices[i].id),
                               g_object_ref (device));
        }
    }

  XFreeDeviceList (devices);

  gdk_x11_register_standard_event_type (display,
                                        device_manager->event_base,
                                        15 /* Number of events */);

  if (G_OBJECT_CLASS (gdk_x11_device_manager_xi_parent_class)->constructed)
    G_OBJECT_CLASS (gdk_x11_device_manager_xi_parent_class)->constructed (object);
}

static void
gdk_x11_device_manager_xi_dispose (GObject *object)
{
  GdkX11DeviceManagerXI *device_manager;

  device_manager = GDK_X11_DEVICE_MANAGER_XI (object);
  g_list_foreach (device_manager->devices, (GFunc) g_object_unref, NULL);
  g_list_free (device_manager->devices);
  device_manager->devices = NULL;

  if (device_manager->id_table != NULL)
    {
      g_hash_table_destroy (device_manager->id_table);
      device_manager->id_table = NULL;
    }

  gdk_window_remove_filter (NULL, window_input_info_filter, object);

  G_OBJECT_CLASS (gdk_x11_device_manager_xi_parent_class)->dispose (object);
}

static void
gdk_x11_device_manager_xi_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GdkX11DeviceManagerXI *device_manager;

  device_manager = GDK_X11_DEVICE_MANAGER_XI (object);

  switch (prop_id)
    {
    case PROP_EVENT_BASE:
      device_manager->event_base = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_x11_device_manager_xi_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GdkX11DeviceManagerXI *device_manager;

  device_manager = GDK_X11_DEVICE_MANAGER_XI (object);

  switch (prop_id)
    {
    case PROP_EVENT_BASE:
      g_value_set_int (value, device_manager->event_base);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_x11_device_manager_xi_event_translator_init (GdkEventTranslatorIface *iface)
{
  iface->translate_event = gdk_x11_device_manager_xi_translate_event;
}

/* combine the state of the core device and the device state
 * into one - for now we do this in a simple-minded manner -
 * we just take the keyboard portion of the core device and
 * the button portion (all of?) the device state.
 * Any button remapping should go on here.
 */
static guint
translate_state (guint state, guint device_state)
{
  return device_state | (state & 0xFF);
}

static GdkDevice *
lookup_device (GdkX11DeviceManagerXI *manager,
               XEvent                *xevent)
{
  GdkX11DeviceManagerXI *device_manager;
  guint32 device_id;

  device_manager = GDK_X11_DEVICE_MANAGER_XI (manager);

  /* This is a sort of a hack, as there isn't any XDeviceAnyEvent -
     but it's potentially faster than scanning through the types of
     every device. If we were deceived, then it won't match any of
     the types for the device anyways */
  device_id = ((XDeviceButtonEvent *)xevent)->deviceid;

  return g_hash_table_lookup (device_manager->id_table, GINT_TO_POINTER (device_id));
}

static gboolean
gdk_x11_device_manager_xi_translate_event (GdkEventTranslator *translator,
                                           GdkDisplay         *display,
                                           GdkEvent           *event,
                                           XEvent             *xevent)
{
  GdkX11DeviceManagerXI *device_manager;
  GdkEventTranslatorIface *parent_iface;
  GdkX11DeviceXI *device_xi;
  GdkDevice *device;
  GdkWindow *window;

  parent_iface = g_type_interface_peek_parent (GDK_EVENT_TRANSLATOR_GET_IFACE (translator));
  device_manager = GDK_X11_DEVICE_MANAGER_XI (translator);

  if (!device_manager->ignore_core_events &&
      parent_iface->translate_event (translator, display, event, xevent))
    return TRUE;

  device = lookup_device (device_manager, xevent);
  device_xi = GDK_X11_DEVICE_XI (device);

  if (!device)
    return FALSE;

  window = gdk_x11_window_lookup_for_display (display, xevent->xany.window);

  if (!window)
    return FALSE;

  if ((xevent->type == device_xi->button_press_type) ||
      (xevent->type == device_xi->button_release_type))
    {
      XDeviceButtonEvent *xdbe = (XDeviceButtonEvent *) xevent;

      event->button.type = (xdbe->type == device_xi->button_press_type) ?
        GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE;

      event->button.device = device;
      event->button.window = g_object_ref (window);
      event->button.time = xdbe->time;

      event->button.x_root = (gdouble) xdbe->x_root;
      event->button.y_root = (gdouble) xdbe->y_root;

      event->button.axes = g_new0 (gdouble, gdk_device_get_n_axes (device));
      _gdk_x11_device_xi_update_axes (device, xdbe->axes_count,
                                      xdbe->first_axis, xdbe->axis_data);
      _gdk_x11_device_xi_translate_axes (device, window,
                                         device_xi->axis_data,
                                         event->button.axes,
                                         &event->button.x,
                                         &event->button.y);

      event->button.state = translate_state (xdbe->state, xdbe->device_state);
      event->button.button = xdbe->button;

      if (event->button.type == GDK_BUTTON_PRESS)
        _gdk_event_button_generate (gdk_window_get_display (event->button.window),
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

      if (xdke->keycode < device_xi->min_keycode ||
          xdke->keycode >= device_xi->min_keycode + gdk_device_get_n_keys (device))
        {
          g_warning ("Invalid device key code received");
          return FALSE;
        }

      gdk_device_get_key (device, xdke->keycode - device_xi->min_keycode,
                          &event->key.keyval,
                          &event->key.state);

      if (event->key.keyval == 0)
        {
          GDK_NOTE (EVENTS,
            g_print ("\t\ttranslation - NONE\n"));

          return FALSE;
        }

      event->key.type = (xdke->type == device_xi->key_press_type) ?
        GDK_KEY_PRESS : GDK_KEY_RELEASE;

      event->key.window = g_object_ref (window);
      event->key.time = xdke->time;

      event->key.state |= translate_state (xdke->state, xdke->device_state);

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

      if (device_xi->in_proximity)
        device_manager->ignore_core_events = TRUE;

      event->motion.x_root = (gdouble) xdme->x_root;
      event->motion.y_root = (gdouble) xdme->y_root;

      event->motion.axes = g_new0 (gdouble, gdk_device_get_n_axes (device));
      _gdk_x11_device_xi_update_axes (device, xdme->axes_count,
                                      xdme->first_axis, xdme->axis_data);
      _gdk_x11_device_xi_translate_axes (device, window,
                                         device_xi->axis_data,
                                         event->motion.axes,
                                         &event->motion.x,
                                         &event->motion.y);

      event->motion.type = GDK_MOTION_NOTIFY;
      event->motion.window = g_object_ref (window);
      event->motion.time = xdme->time;
      event->motion.state = translate_state (xdme->state,
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

      if (xevent->type == device_xi->proximity_in_type)
        {
          event->proximity.type = GDK_PROXIMITY_IN;
          device_xi->in_proximity = TRUE;
          device_manager->ignore_core_events = TRUE;
        }
      else
        {
          event->proximity.type = GDK_PROXIMITY_OUT;
          device_xi->in_proximity = FALSE;
          device_manager->ignore_core_events = FALSE;
        }

      event->proximity.device = device;
      event->proximity.window = g_object_ref (window);
      event->proximity.time = xpne->time;

      /* Update the timestamp of the latest user interaction, if the event has
       * a valid timestamp.
       */
      if (gdk_event_get_time (event) != GDK_CURRENT_TIME)
        gdk_x11_window_set_user_time (gdk_window_get_toplevel (window),
                                      gdk_event_get_time (event));
      return TRUE;
    }

  if (xevent->type == device_xi->state_notify_type)
    {
      XDeviceStateNotifyEvent *xdse = (XDeviceStateNotifyEvent *) xevent;
      XInputClass *input_class = (XInputClass *) xdse->data;
      int i;

      for (i = 0; i < xdse->num_classes; i++)
        {
          if (input_class->class == ValuatorClass)
            _gdk_x11_device_xi_update_axes (device, gdk_device_get_n_axes (device), 0,
                                            ((XValuatorState *)input_class)->valuators);

          input_class = (XInputClass *)(((char *)input_class)+input_class->length);
        }

      GDK_NOTE (EVENTS,
                g_print ("device state notify:\t\twindow: %ld  device: %ld\n",
                         xdse->window,
                         xdse->deviceid));

      return FALSE;
    }

  return FALSE;
}

static GList *
gdk_x11_device_manager_xi_list_devices (GdkDeviceManager *manager,
                                        GdkDeviceType     type)
{
  GdkX11DeviceManagerXI *device_manager;

  device_manager = GDK_X11_DEVICE_MANAGER_XI (manager);

  if (type == GDK_DEVICE_TYPE_MASTER)
    return GDK_DEVICE_MANAGER_CLASS (gdk_x11_device_manager_xi_parent_class)->list_devices (manager, type);
  else if (type == GDK_DEVICE_TYPE_FLOATING)
    {
      return g_list_copy (device_manager->devices);
    }
  else
    return NULL;
}
