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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkx11devicemanager-xi2.h"
#include "gdkx11device-xi2.h"

#include "gdkdeviceprivate.h"
#include "gdkdevicetoolprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkeventtranslator.h"
#include "gdkprivate-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkintl.h"
#include "gdkkeysyms.h"
#include "gdkinternals.h"
#include "gdkseatdefaultprivate.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>

#include <string.h>

#ifdef G_ENABLE_DEBUG
static const char notify_modes[][19] = {
  "NotifyNormal",
  "NotifyGrab",
  "NotifyUngrab",
  "NotifyWhileGrabbed"
};

static const char notify_details[][23] = {
  "NotifyAncestor",
  "NotifyVirtual",
  "NotifyInferior",
  "NotifyNonlinear",
  "NotifyNonlinearVirtual",
  "NotifyPointer",
  "NotifyPointerRoot",
  "NotifyDetailNone"
};
#endif

#define HAS_FOCUS(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

static const char *wacom_type_atoms[] = {
  "STYLUS",
  "CURSOR",
  "ERASER",
  "PAD",
  "TOUCH"
};
#define N_WACOM_TYPE_ATOMS G_N_ELEMENTS (wacom_type_atoms)

enum {
  WACOM_TYPE_STYLUS,
  WACOM_TYPE_CURSOR,
  WACOM_TYPE_ERASER,
  WACOM_TYPE_PAD,
  WACOM_TYPE_TOUCH,
};

struct _GdkX11DeviceManagerXI2
{
  GObject parent_object;

  GdkDisplay *display;
  GHashTable *id_table;

  GList *devices;

  gint opcode;
  gint major;
  gint minor;
};

struct _GdkX11DeviceManagerXI2Class
{
  GObjectClass parent_class;
};

static void     gdk_x11_device_manager_xi2_event_translator_init (GdkEventTranslatorIface *iface);

G_DEFINE_TYPE_WITH_CODE (GdkX11DeviceManagerXI2, gdk_x11_device_manager_xi2, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_EVENT_TRANSLATOR,
                                                gdk_x11_device_manager_xi2_event_translator_init))

static void    gdk_x11_device_manager_xi2_constructed  (GObject      *object);
static void    gdk_x11_device_manager_xi2_dispose      (GObject      *object);
static void    gdk_x11_device_manager_xi2_set_property (GObject      *object,
                                                        guint         prop_id,
                                                        const GValue *value,
                                                        GParamSpec   *pspec);
static void    gdk_x11_device_manager_xi2_get_property (GObject      *object,
                                                        guint         prop_id,
                                                        GValue       *value,
                                                        GParamSpec   *pspec);

static gboolean gdk_x11_device_manager_xi2_translate_event (GdkEventTranslator *translator,
                                                            GdkDisplay         *display,
                                                            GdkEvent           *event,
                                                            const XEvent       *xevent);
static GdkEventMask gdk_x11_device_manager_xi2_get_handled_events   (GdkEventTranslator *translator);
static void         gdk_x11_device_manager_xi2_select_surface_events (GdkEventTranslator *translator,
                                                                     Window              window,
                                                                     GdkEventMask        event_mask);
static GdkSurface *  gdk_x11_device_manager_xi2_get_surface         (GdkEventTranslator *translator,
                                                                     const XEvent       *xevent);

enum {
  PROP_0,
  PROP_DISPLAY,
  PROP_OPCODE,
  PROP_MAJOR,
  PROP_MINOR
};

static void
gdk_x11_device_manager_xi2_class_init (GdkX11DeviceManagerXI2Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gdk_x11_device_manager_xi2_constructed;
  object_class->dispose = gdk_x11_device_manager_xi2_dispose;
  object_class->set_property = gdk_x11_device_manager_xi2_set_property;
  object_class->get_property = gdk_x11_device_manager_xi2_get_property;

  g_object_class_install_property (object_class,
                                   PROP_DISPLAY,
                                   g_param_spec_object ("display",
                                                        "Display",
                                                        "Display for the device manager",
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_OPCODE,
                                   g_param_spec_int ("opcode",
                                                     P_("Opcode"),
                                                     P_("Opcode for XInput2 requests"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_MAJOR,
                                   g_param_spec_int ("major",
                                                     P_("Major"),
                                                     P_("Major version number"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_MINOR,
                                   g_param_spec_int ("minor",
                                                     P_("Minor"),
                                                     P_("Minor version number"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_STATIC_STRINGS));
}

static void
gdk_x11_device_manager_xi2_init (GdkX11DeviceManagerXI2 *device_manager)
{
  device_manager->id_table = g_hash_table_new_full (g_direct_hash,
                                                    g_direct_equal,
                                                    NULL,
                                                    (GDestroyNotify) g_object_unref);
}

static void
_gdk_x11_device_manager_xi2_select_events (GdkX11DeviceManagerXI2 *device_manager,
                                           Window                  xwindow,
                                           XIEventMask            *event_mask)
{
  GdkDisplay *display;
  Display *xdisplay;

  display = device_manager->display;
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  XISelectEvents (xdisplay, xwindow, event_mask, 1);
}

static void
translate_valuator_class (GdkDisplay          *display,
                          GdkDevice           *device,
                          Atom                 valuator_label,
                          gdouble              min,
                          gdouble              max,
                          gdouble              resolution)
{
  static gboolean initialized = FALSE;
  static Atom label_atoms [GDK_AXIS_LAST] = { 0 };
  GdkAxisUse use = GDK_AXIS_IGNORE;
  GdkAtom label;
  gint i;

  if (!initialized)
    {
      label_atoms [GDK_AXIS_X] = gdk_x11_get_xatom_by_name_for_display (display, "Abs X");
      label_atoms [GDK_AXIS_Y] = gdk_x11_get_xatom_by_name_for_display (display, "Abs Y");
      label_atoms [GDK_AXIS_PRESSURE] = gdk_x11_get_xatom_by_name_for_display (display, "Abs Pressure");
      label_atoms [GDK_AXIS_XTILT] = gdk_x11_get_xatom_by_name_for_display (display, "Abs Tilt X");
      label_atoms [GDK_AXIS_YTILT] = gdk_x11_get_xatom_by_name_for_display (display, "Abs Tilt Y");
      label_atoms [GDK_AXIS_WHEEL] = gdk_x11_get_xatom_by_name_for_display (display, "Abs Wheel");
      initialized = TRUE;
    }

  for (i = GDK_AXIS_IGNORE; i < GDK_AXIS_LAST; i++)
    {
      if (label_atoms[i] == valuator_label)
        {
          use = i;
          break;
        }
    }

  if (valuator_label != None)
    label = gdk_x11_xatom_to_atom_for_display (display, valuator_label);
  else
    label = NULL;

  _gdk_device_add_axis (device, label, use, min, max, resolution);
  GDK_DISPLAY_NOTE (display, INPUT, g_message ("\n\taxis: %s %s", (const char *)label, use == GDK_AXIS_IGNORE ? "(ignored)" : "(used)"));
}

static void
translate_device_classes (GdkDisplay      *display,
                          GdkDevice       *device,
                          XIAnyClassInfo **classes,
                          guint            n_classes)
{
  gint i;

  g_object_freeze_notify (G_OBJECT (device));

  for (i = 0; i < n_classes; i++)
    {
      XIAnyClassInfo *class_info = classes[i];

      switch (class_info->type)
        {
        case XIKeyClass:
          {
            XIKeyClassInfo *key_info = (XIKeyClassInfo *) class_info;
            gint j;

            _gdk_device_set_keys (device, key_info->num_keycodes);

            for (j = 0; j < key_info->num_keycodes; j++)
              gdk_device_set_key (device, j, key_info->keycodes[j], 0);
          }
          break;
        case XIValuatorClass:
          {
            XIValuatorClassInfo *valuator_info = (XIValuatorClassInfo *) class_info;
            translate_valuator_class (display, device,
                                      valuator_info->label,
                                      valuator_info->min,
                                      valuator_info->max,
                                      valuator_info->resolution);
          }
          break;
#ifdef XINPUT_2_2
        case XIScrollClass:
          {
            XIScrollClassInfo *scroll_info = (XIScrollClassInfo *) class_info;
            GdkScrollDirection direction;

            if (scroll_info->scroll_type == XIScrollTypeVertical)
              direction = GDK_SCROLL_DOWN;
            else
              direction = GDK_SCROLL_RIGHT;

            GDK_DISPLAY_NOTE (display, INPUT,
                      g_message ("\n\tscroll valuator %d: %s, increment %f",
                                 scroll_info->number,
                                 scroll_info->scroll_type == XIScrollTypeVertical
                                                ? "vertical"
                                                : "horizontal",
                                 scroll_info->increment));

            _gdk_x11_device_xi2_add_scroll_valuator (GDK_X11_DEVICE_XI2 (device),
                                                     scroll_info->number,
                                                     direction,
                                                     scroll_info->increment);
          }
#endif /* XINPUT_2_2 */
        default:
          /* Ignore */
          break;
        }
    }

  g_object_thaw_notify (G_OBJECT (device));
}

static gboolean
is_touch_device (XIAnyClassInfo **classes,
                 guint            n_classes,
                 GdkInputSource  *device_type,
                 gint            *num_touches)
{
#ifdef XINPUT_2_2
  guint i;

  for (i = 0; i < n_classes; i++)
    {
      XITouchClassInfo *class = (XITouchClassInfo *) classes[i];

      if (class->type != XITouchClass)
        continue;

      if (class->num_touches > 0)
        {
          if (class->mode == XIDirectTouch)
            *device_type = GDK_SOURCE_TOUCHSCREEN;
          else if (class->mode == XIDependentTouch)
            *device_type = GDK_SOURCE_TOUCHPAD;
          else
            continue;

          *num_touches = class->num_touches;

          return TRUE;
        }
    }
#endif

  return FALSE;
}

static gboolean
has_abs_axes (GdkDisplay      *display,
              XIAnyClassInfo **classes,
              guint            n_classes)
{
  gboolean has_x = FALSE, has_y = FALSE;
  Atom abs_x, abs_y;
  guint i;

  abs_x = gdk_x11_get_xatom_by_name_for_display (display, "Abs X");
  abs_y = gdk_x11_get_xatom_by_name_for_display (display, "Abs Y");

  for (i = 0; i < n_classes; i++)
    {
      XIValuatorClassInfo *class = (XIValuatorClassInfo *) classes[i];

      if (class->type != XIValuatorClass)
        continue;
      if (class->mode != XIModeAbsolute)
        continue;

      if (class->label == abs_x)
        has_x = TRUE;
      else if (class->label == abs_y)
        has_y = TRUE;

      if (has_x && has_y)
        break;
    }

  return (has_x && has_y);
}

static gboolean
get_device_ids (GdkDisplay    *display,
                XIDeviceInfo  *info,
                gchar        **vendor_id,
                gchar        **product_id)
{
  gulong nitems, bytes_after;
  guint32 *data;
  int rc, format;
  Atom prop, type;

  gdk_x11_display_error_trap_push (display);

  prop = XInternAtom (GDK_DISPLAY_XDISPLAY (display), "Device Product ID", True);

  if (prop == None)
    {
      gdk_x11_display_error_trap_pop_ignored (display);
      return 0;
    }

  rc = XIGetProperty (GDK_DISPLAY_XDISPLAY (display),
                      info->deviceid, prop,
                      0, 2, False, XA_INTEGER, &type, &format, &nitems, &bytes_after,
                      (guchar **) &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (rc != Success || type != XA_INTEGER || format != 32 || nitems != 2)
    return FALSE;

  if (vendor_id)
    *vendor_id = g_strdup_printf ("%.4x", data[0]);
  if (product_id)
    *product_id = g_strdup_printf ("%.4x", data[1]);

  XFree (data);

  return TRUE;
}

static gboolean
has_bool_prop (GdkDisplay   *display,
               XIDeviceInfo *info,
               const char   *prop_name)
{
  gulong nitems, bytes_after;
  guint32 *data;
  int rc, format;
  Atom type;

  gdk_x11_display_error_trap_push (display);

  rc = XIGetProperty (GDK_DISPLAY_XDISPLAY (display),
                      info->deviceid,
                      gdk_x11_get_xatom_by_name_for_display (display, prop_name),
                      0, 1, False, XA_INTEGER, &type, &format, &nitems, &bytes_after,
                      (guchar **) &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (rc != Success || type != XA_INTEGER || format != 8 || nitems != 1)
    return FALSE;

  XFree (data);

  return TRUE;
}

static gboolean
is_touchpad_device (GdkDisplay   *display,
                    XIDeviceInfo *info)
{
  /*
   * Touchpads are heuristically recognized via XI properties that the various
   * Xorg drivers expose:
   *   libinput:  libinput Tapping Enabled
   *   synaptics: Synaptics Off
   *   cmt:       Raw Touch Passthrough
   */
  return has_bool_prop (display, info, "libinput Tapping Enabled") ||
         has_bool_prop (display, info, "Synaptics Off") ||
         has_bool_prop (display, info, "Raw Touch Passthrough");
}

static GdkDevice *
create_device (GdkX11DeviceManagerXI2 *device_manager,
               GdkDisplay             *display,
               XIDeviceInfo           *dev)
{
  GdkInputSource input_source;
  GdkInputSource touch_source;
  GdkDeviceType type;
  GdkDevice *device;
  GdkInputMode mode;
  gint num_touches = 0;
  gchar *vendor_id = NULL, *product_id = NULL;

  if (dev->use == XIMasterKeyboard || dev->use == XISlaveKeyboard)
    input_source = GDK_SOURCE_KEYBOARD;
  else if (is_touchpad_device (display, dev))
    input_source = GDK_SOURCE_TOUCHPAD;
  else if (dev->use == XISlavePointer &&
           is_touch_device (dev->classes, dev->num_classes, &touch_source, &num_touches))
    input_source = touch_source;
  else
    {
      gchar *tmp_name;

      tmp_name = g_ascii_strdown (dev->name, -1);

      if (strstr (tmp_name, "eraser"))
        input_source = GDK_SOURCE_ERASER;
      else if (strstr (tmp_name, "cursor"))
        input_source = GDK_SOURCE_CURSOR;
      else if (strstr (tmp_name, " pad"))
        input_source = GDK_SOURCE_TABLET_PAD;
      else if (strstr (tmp_name, "wacom") ||
               strstr (tmp_name, "pen"))
        input_source = GDK_SOURCE_PEN;
      else if (!strstr (tmp_name, "mouse") &&
               !strstr (tmp_name, "pointer") &&
               !strstr (tmp_name, "qemu usb tablet") &&
               !strstr (tmp_name, "spice vdagent tablet") &&
               !strstr (tmp_name, "virtualbox usb tablet") &&
               has_abs_axes (display, dev->classes, dev->num_classes))
        input_source = GDK_SOURCE_TOUCHSCREEN;
      else if (strstr (tmp_name, "trackpoint") ||
               strstr (tmp_name, "dualpoint stick"))
        input_source = GDK_SOURCE_TRACKPOINT;
      else
        input_source = GDK_SOURCE_MOUSE;

      g_free (tmp_name);
    }

  switch (dev->use)
    {
    case XIMasterKeyboard:
    case XIMasterPointer:
      type = GDK_DEVICE_TYPE_MASTER;
      mode = GDK_MODE_SCREEN;
      break;
    case XISlaveKeyboard:
    case XISlavePointer:
      type = GDK_DEVICE_TYPE_SLAVE;
      mode = GDK_MODE_DISABLED;
      break;
    case XIFloatingSlave:
    default:
      type = GDK_DEVICE_TYPE_FLOATING;
      mode = GDK_MODE_DISABLED;
      break;
    }

  GDK_DISPLAY_NOTE (display, INPUT,
            ({
              const gchar *type_names[] = { "master", "slave", "floating" };
              const gchar *source_names[] = { "mouse", "pen", "eraser", "cursor", "keyboard", "direct touch", "indirect touch", "trackpoint", "pad" };
              const gchar *mode_names[] = { "disabled", "screen", "window" };
              g_message ("input device:\n\tname: %s\n\ttype: %s\n\tsource: %s\n\tmode: %s\n\thas cursor: %d\n\ttouches: %d",
                         dev->name,
                         type_names[type],
                         source_names[input_source],
                         mode_names[mode],
                         dev->use == XIMasterPointer,
                         num_touches);
            }));

  if (dev->use != XIMasterKeyboard &&
      dev->use != XIMasterPointer)
    get_device_ids (display, dev, &vendor_id, &product_id);

  device = g_object_new (GDK_TYPE_X11_DEVICE_XI2,
                         "name", dev->name,
                         "type", type,
                         "input-source", input_source,
                         "input-mode", mode,
                         "has-cursor", (dev->use == XIMasterPointer),
                         "display", display,
                         "device-id", dev->deviceid,
                         "vendor-id", vendor_id,
                         "product-id", product_id,
                         "num-touches", num_touches,
                         NULL);

  translate_device_classes (display, device, dev->classes, dev->num_classes);
  g_free (vendor_id);
  g_free (product_id);

  return device;
}

static void
ensure_seat_for_device_pair (GdkX11DeviceManagerXI2 *device_manager,
                             GdkDevice              *device1,
                             GdkDevice              *device2)
{
  GdkDevice *pointer, *keyboard;
  GdkDisplay *display;
  GdkSeat *seat;

  display = device_manager->display;
  seat = gdk_device_get_seat (device1);

  if (!seat)
    {
      if (gdk_device_get_source (device1) == GDK_SOURCE_KEYBOARD)
        {
          keyboard = device1;
          pointer = device2;
        }
      else
        {
          pointer = device1;
          keyboard = device2;
        }

      seat = gdk_seat_default_new_for_master_pair (pointer, keyboard);
      gdk_display_add_seat (display, seat);
      g_object_unref (seat);
    }
}

static GdkDevice *
add_device (GdkX11DeviceManagerXI2 *device_manager,
            XIDeviceInfo           *dev,
            gboolean                emit_signal)
{
  GdkDisplay *display;
  GdkDevice *device;

  display = device_manager->display;
  device = create_device (device_manager, display, dev);

  g_hash_table_replace (device_manager->id_table,
                        GINT_TO_POINTER (dev->deviceid),
                        g_object_ref (device));

  device_manager->devices = g_list_append (device_manager->devices, device);

  if (emit_signal)
    {
      if (dev->use == XISlavePointer || dev->use == XISlaveKeyboard)
        {
          GdkDevice *master;
          GdkSeat *seat;

          /* The device manager is already constructed, then
           * keep the hierarchy coherent for the added device.
           */
          master = g_hash_table_lookup (device_manager->id_table,
                                        GINT_TO_POINTER (dev->attachment));

          _gdk_device_set_associated_device (device, master);
          _gdk_device_add_slave (master, device);

          seat = gdk_device_get_seat (master);
          gdk_seat_default_add_slave (GDK_SEAT_DEFAULT (seat), device);
        }
      else if (dev->use == XIMasterPointer || dev->use == XIMasterKeyboard)
        {
          GdkDevice *relative;

          relative = g_hash_table_lookup (device_manager->id_table,
                                          GINT_TO_POINTER (dev->attachment));

          if (relative)
            {
              _gdk_device_set_associated_device (device, relative);
              _gdk_device_set_associated_device (relative, device);
              ensure_seat_for_device_pair (device_manager, device, relative);
            }
        }
    }

  return device;
}

static void
detach_from_seat (GdkDevice *device)
{
  GdkSeat *seat = gdk_device_get_seat (device);

  if (!seat)
    return;

  if (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_MASTER)
    gdk_display_remove_seat (gdk_device_get_display (device), seat);
  else if (gdk_device_get_device_type (device) == GDK_DEVICE_TYPE_SLAVE)
    gdk_seat_default_remove_slave (GDK_SEAT_DEFAULT (seat), device);
}

static void
remove_device (GdkX11DeviceManagerXI2 *device_manager,
               gint                    device_id)
{
  GdkDevice *device;

  device = g_hash_table_lookup (device_manager->id_table,
                                GINT_TO_POINTER (device_id));

  if (device)
    {
      detach_from_seat (device);
      device_manager->devices = g_list_remove (device_manager->devices, device);

      g_object_run_dispose (G_OBJECT (device));

      g_hash_table_remove (device_manager->id_table,
                           GINT_TO_POINTER (device_id));
    }
}

static void
relate_masters (gpointer key,
                gpointer value,
                gpointer user_data)
{
  GdkX11DeviceManagerXI2 *device_manager;
  GdkDevice *device, *relative;

  device_manager = user_data;
  device = g_hash_table_lookup (device_manager->id_table, key);
  relative = g_hash_table_lookup (device_manager->id_table, value);

  _gdk_device_set_associated_device (device, relative);
  _gdk_device_set_associated_device (relative, device);
  ensure_seat_for_device_pair (device_manager, device, relative);
}

static void
relate_slaves (gpointer key,
               gpointer value,
               gpointer user_data)
{
  GdkX11DeviceManagerXI2 *device_manager;
  GdkDevice *slave, *master;
  GdkSeat *seat;

  device_manager = user_data;
  slave = g_hash_table_lookup (device_manager->id_table, key);
  master = g_hash_table_lookup (device_manager->id_table, value);

  _gdk_device_set_associated_device (slave, master);
  _gdk_device_add_slave (master, slave);

  seat = gdk_device_get_seat (master);
  gdk_seat_default_add_slave (GDK_SEAT_DEFAULT (seat), slave);
}

static void
gdk_x11_device_manager_xi2_constructed (GObject *object)
{
  GdkX11DeviceManagerXI2 *device_manager;
  GdkDisplay *display;
  GHashTable *masters, *slaves;
  Display *xdisplay;
  XIDeviceInfo *info, *dev;
  int ndevices, i;
  XIEventMask event_mask;
  unsigned char mask[2] = { 0 };

  G_OBJECT_CLASS (gdk_x11_device_manager_xi2_parent_class)->constructed (object);

  device_manager = GDK_X11_DEVICE_MANAGER_XI2 (object);
  display = device_manager->display;
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  g_assert (device_manager->major == 2);

  masters = g_hash_table_new (NULL, NULL);
  slaves = g_hash_table_new (NULL, NULL);

  info = XIQueryDevice (xdisplay, XIAllDevices, &ndevices);

  /* Initialize devices list */
  for (i = 0; i < ndevices; i++)
    {
      dev = &info[i];

      if (!dev->enabled)
	      continue;

      add_device (device_manager, dev, FALSE);

      if (dev->use == XIMasterPointer ||
          dev->use == XIMasterKeyboard)
        {
          g_hash_table_insert (masters,
                               GINT_TO_POINTER (dev->deviceid),
                               GINT_TO_POINTER (dev->attachment));
        }
      else if (dev->use == XISlavePointer ||
               dev->use == XISlaveKeyboard)
        {
          g_hash_table_insert (slaves,
                               GINT_TO_POINTER (dev->deviceid),
                               GINT_TO_POINTER (dev->attachment));
        }
    }

  XIFreeDeviceInfo (info);

  /* Stablish relationships between devices */
  g_hash_table_foreach (masters, relate_masters, object);
  g_hash_table_destroy (masters);

  g_hash_table_foreach (slaves, relate_slaves, object);
  g_hash_table_destroy (slaves);

  /* Connect to hierarchy change events */
  XISetMask (mask, XI_HierarchyChanged);
  XISetMask (mask, XI_DeviceChanged);
  XISetMask (mask, XI_PropertyEvent);

  event_mask.deviceid = XIAllDevices;
  event_mask.mask_len = sizeof (mask);
  event_mask.mask = mask;

  _gdk_x11_device_manager_xi2_select_events (device_manager,
                                             GDK_DISPLAY_XROOTWIN (display),
                                             &event_mask);
}

static void
gdk_x11_device_manager_xi2_dispose (GObject *object)
{
  GdkX11DeviceManagerXI2 *device_manager;

  device_manager = GDK_X11_DEVICE_MANAGER_XI2 (object);

  g_list_free_full (device_manager->devices, g_object_unref);
  device_manager->devices = NULL;

  if (device_manager->id_table)
    {
      g_hash_table_destroy (device_manager->id_table);
      device_manager->id_table = NULL;
    }

  G_OBJECT_CLASS (gdk_x11_device_manager_xi2_parent_class)->dispose (object);
}

static void
gdk_x11_device_manager_xi2_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GdkX11DeviceManagerXI2 *device_manager;

  device_manager = GDK_X11_DEVICE_MANAGER_XI2 (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      device_manager->display = g_value_get_object (value);
      break;
    case PROP_OPCODE:
      device_manager->opcode = g_value_get_int (value);
      break;
    case PROP_MAJOR:
      device_manager->major = g_value_get_int (value);
      break;
    case PROP_MINOR:
      device_manager->minor = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_x11_device_manager_xi2_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GdkX11DeviceManagerXI2 *device_manager;

  device_manager = GDK_X11_DEVICE_MANAGER_XI2 (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, device_manager->display);
      break;
    case PROP_OPCODE:
      g_value_set_int (value, device_manager->opcode);
      break;
    case PROP_MAJOR:
      g_value_set_int (value, device_manager->major);
      break;
    case PROP_MINOR:
      g_value_set_int (value, device_manager->minor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_x11_device_manager_xi2_event_translator_init (GdkEventTranslatorIface *iface)
{
  iface->translate_event = gdk_x11_device_manager_xi2_translate_event;
  iface->get_handled_events = gdk_x11_device_manager_xi2_get_handled_events;
  iface->select_surface_events = gdk_x11_device_manager_xi2_select_surface_events;
  iface->get_surface = gdk_x11_device_manager_xi2_get_surface;
}

static void
handle_hierarchy_changed (GdkX11DeviceManagerXI2 *device_manager,
                          XIHierarchyEvent       *ev)
{
  GdkDisplay *display;
  Display *xdisplay;
  XIDeviceInfo *info;
  int ndevices;
  gint i;

  display = device_manager->display;
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  for (i = 0; i < ev->num_info; i++)
    {
      if (ev->info[i].flags & XIDeviceEnabled)
        {
          gdk_x11_display_error_trap_push (display);
          info = XIQueryDevice (xdisplay, ev->info[i].deviceid, &ndevices);
          gdk_x11_display_error_trap_pop_ignored (display);
          if (info)
            {
              add_device (device_manager, &info[0], TRUE);
              XIFreeDeviceInfo (info);
            }
        }
      else if (ev->info[i].flags & XIDeviceDisabled)
        remove_device (device_manager, ev->info[i].deviceid);
      else if (ev->info[i].flags & XISlaveAttached ||
               ev->info[i].flags & XISlaveDetached)
        {
          GdkDevice *master, *slave;
          GdkSeat *seat;

          slave = g_hash_table_lookup (device_manager->id_table,
                                       GINT_TO_POINTER (ev->info[i].deviceid));

          if (!slave)
            continue;

          /* Remove old master info */
          master = gdk_device_get_associated_device (slave);

          if (master)
            {
              _gdk_device_remove_slave (master, slave);
              _gdk_device_set_associated_device (slave, NULL);

              seat = gdk_device_get_seat (master);
              gdk_seat_default_remove_slave (GDK_SEAT_DEFAULT (seat), slave);
            }

          /* Add new master if it's an attachment event */
          if (ev->info[i].flags & XISlaveAttached)
            {
              gdk_x11_display_error_trap_push (display);
              info = XIQueryDevice (xdisplay, ev->info[i].deviceid, &ndevices);
              gdk_x11_display_error_trap_pop_ignored (display);
              if (info)
                {
                  master = g_hash_table_lookup (device_manager->id_table,
                                                GINT_TO_POINTER (info->attachment));
                  XIFreeDeviceInfo (info);
                }

              if (master)
                {
                  _gdk_device_set_associated_device (slave, master);
                  _gdk_device_add_slave (master, slave);

                  seat = gdk_device_get_seat (master);
                  gdk_seat_default_add_slave (GDK_SEAT_DEFAULT (seat), slave);
                }
            }
        }
    }
}

static void
handle_device_changed (GdkX11DeviceManagerXI2 *device_manager,
                       XIDeviceChangedEvent   *ev)
{
  GdkDisplay *display;
  GdkDevice *device, *source_device;

  display = device_manager->display;
  device = g_hash_table_lookup (device_manager->id_table,
                                GUINT_TO_POINTER (ev->deviceid));
  source_device = g_hash_table_lookup (device_manager->id_table,
                                       GUINT_TO_POINTER (ev->sourceid));

  if (device)
    {
      _gdk_device_reset_axes (device);
      _gdk_device_xi2_unset_scroll_valuators ((GdkX11DeviceXI2 *) device);
      gdk_x11_device_xi2_store_axes (GDK_X11_DEVICE_XI2 (device), NULL, 0);
      translate_device_classes (display, device, ev->classes, ev->num_classes);

      g_signal_emit_by_name (G_OBJECT (device), "changed");
    }

  if (source_device)
    _gdk_device_xi2_reset_scroll_valuators (GDK_X11_DEVICE_XI2 (source_device));
}

static gboolean
device_get_tool_serial_and_id (GdkDevice *device,
                               guint     *serial_id,
                               guint     *id)
{
  GdkDisplay *display;
  gulong nitems, bytes_after;
  guint32 *data;
  int rc, format;
  Atom type;

  display = gdk_device_get_display (device);

  gdk_x11_display_error_trap_push (display);

  rc = XIGetProperty (GDK_DISPLAY_XDISPLAY (display),
                      gdk_x11_device_get_id (device),
                      gdk_x11_get_xatom_by_name_for_display (display, "Wacom Serial IDs"),
                      0, 5, False, XA_INTEGER, &type, &format, &nitems, &bytes_after,
                      (guchar **) &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (rc != Success)
    return FALSE;

  if (type == XA_INTEGER && format == 32)
    {
      if (nitems >= 4)
        *serial_id = data[3];
      if (nitems >= 5)
        *id = data[4];
    }

  XFree (data);

  return TRUE;
}

static GdkDeviceToolType
device_get_tool_type (GdkDevice *device)
{
  GdkDisplay *display;
  gulong nitems, bytes_after;
  guint32 *data;
  int rc, format;
  Atom type;
  Atom device_type;
  Atom types[N_WACOM_TYPE_ATOMS];
  GdkDeviceToolType tool_type = GDK_DEVICE_TOOL_TYPE_UNKNOWN;

  display = gdk_device_get_display (device);
  gdk_x11_display_error_trap_push (display);

  rc = XIGetProperty (GDK_DISPLAY_XDISPLAY (display),
                      gdk_x11_device_get_id (device),
                      gdk_x11_get_xatom_by_name_for_display (display, "Wacom Tool Type"),
                      0, 1, False, XA_ATOM, &type, &format, &nitems, &bytes_after,
                      (guchar **) &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (rc != Success)
    return GDK_DEVICE_TOOL_TYPE_UNKNOWN;

  if (type != XA_ATOM || format != 32 || nitems != 1)
    {
      XFree (data);
      return GDK_DEVICE_TOOL_TYPE_UNKNOWN;
    }

  device_type = *data;
  XFree (data);

  if (device_type == 0)
    return GDK_DEVICE_TOOL_TYPE_UNKNOWN;

  gdk_x11_display_error_trap_push (display);
  rc = XInternAtoms (GDK_DISPLAY_XDISPLAY (display),
                     (char **) wacom_type_atoms,
                     N_WACOM_TYPE_ATOMS,
                     False,
                     types);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (rc == 0)
    return GDK_DEVICE_TOOL_TYPE_UNKNOWN;

  if (device_type == types[WACOM_TYPE_STYLUS])
    tool_type = GDK_DEVICE_TOOL_TYPE_PEN;
  else if (device_type == types[WACOM_TYPE_CURSOR])
    tool_type = GDK_DEVICE_TOOL_TYPE_MOUSE;
  else if (device_type == types[WACOM_TYPE_ERASER])
    tool_type = GDK_DEVICE_TOOL_TYPE_ERASER;
  else if (device_type == types[WACOM_TYPE_TOUCH])
    tool_type = GDK_DEVICE_TOOL_TYPE_UNKNOWN;

  return tool_type;
}

static void
handle_property_change (GdkX11DeviceManagerXI2 *device_manager,
                        XIPropertyEvent        *ev)
{
  GdkDevice *device;

  device = g_hash_table_lookup (device_manager->id_table,
                                GUINT_TO_POINTER (ev->deviceid));

  if (device != NULL &&
      ev->property == gdk_x11_get_xatom_by_name_for_display (gdk_device_get_display (device), "Wacom Serial IDs"))
    {
      GdkDeviceTool *tool = NULL;
      guint serial_id = 0, tool_id = 0;
      GdkSeat *seat;

      if (ev->what != XIPropertyDeleted &&
          device_get_tool_serial_and_id (device, &serial_id, &tool_id))
        {
          seat = gdk_device_get_seat (device);
          tool = gdk_seat_get_tool (seat, serial_id, tool_id);

          if (!tool && serial_id > 0)
            {
              GdkDeviceToolType tool_type;

              tool_type = device_get_tool_type (device);
              if (tool_type != GDK_DEVICE_TOOL_TYPE_UNKNOWN)
                {
                  tool = gdk_device_tool_new (serial_id, tool_id, tool_type, 0);
                  gdk_seat_default_add_tool (GDK_SEAT_DEFAULT (seat), tool);
                }
            }
        }

      gdk_device_update_tool (device, tool);
    }
}

static GdkCrossingMode
translate_crossing_mode (gint mode)
{
  switch (mode)
    {
    case XINotifyNormal:
      return GDK_CROSSING_NORMAL;
    case XINotifyGrab:
    case XINotifyPassiveGrab:
      return GDK_CROSSING_GRAB;
    case XINotifyUngrab:
    case XINotifyPassiveUngrab:
      return GDK_CROSSING_UNGRAB;
    case XINotifyWhileGrabbed:
      /* Fall through, unexpected in pointer crossing events */
    default:
      g_assert_not_reached ();
      return GDK_CROSSING_NORMAL;
    }
}

static GdkNotifyType
translate_notify_type (gint detail)
{
  switch (detail)
    {
    case NotifyInferior:
      return GDK_NOTIFY_INFERIOR;
    case NotifyAncestor:
      return GDK_NOTIFY_ANCESTOR;
    case NotifyVirtual:
      return GDK_NOTIFY_VIRTUAL;
    case NotifyNonlinear:
      return GDK_NOTIFY_NONLINEAR;
    case NotifyNonlinearVirtual:
      return GDK_NOTIFY_NONLINEAR_VIRTUAL;
    default:
      g_assert_not_reached ();
      return GDK_NOTIFY_UNKNOWN;
    }
}

static void
set_user_time (GdkEvent *event)
{
  GdkSurface *surface;
  guint32 time;

  surface = event->any.surface;
  g_return_if_fail (GDK_IS_SURFACE (surface));

  time = gdk_event_get_time (event);

  /* If an event doesn't have a valid timestamp, we shouldn't use it
   * to update the latest user interaction time.
   */
  if (time != GDK_CURRENT_TIME)
    gdk_x11_surface_set_user_time (surface, time);
}

static gdouble *
translate_axes (GdkDevice       *device,
                gdouble          x,
                gdouble          y,
                GdkSurface       *surface,
                XIValuatorState *valuators)
{
  guint n_axes, i;
  gdouble *axes;
  gdouble *vals;

  n_axes = gdk_device_get_n_axes (device);
  axes = g_new0 (gdouble, n_axes);
  vals = valuators->values;

  for (i = 0; i < MIN (valuators->mask_len * 8, n_axes); i++)
    {
      GdkAxisUse use;
      gdouble val;

      if (!XIMaskIsSet (valuators->mask, i))
        {
          axes[i] = gdk_x11_device_xi2_get_last_axis_value (GDK_X11_DEVICE_XI2 (device), i);
          continue;
        }

      use = gdk_device_get_axis_use (device, i);
      val = *vals++;

      switch ((guint) use)
        {
        case GDK_AXIS_X:
        case GDK_AXIS_Y:
          if (gdk_device_get_mode (device) == GDK_MODE_SURFACE)
            _gdk_device_translate_surface_coord (device, surface, i, val, &axes[i]);
          else
            {
              if (use == GDK_AXIS_X)
                axes[i] = x;
              else
                axes[i] = y;
            }
          break;
        default:
          _gdk_device_translate_axis (device, i, val, &axes[i]);
          break;
        }
    }

  gdk_x11_device_xi2_store_axes (GDK_X11_DEVICE_XI2 (device), axes, n_axes);

  return axes;
}

static gboolean
get_event_surface (GdkEventTranslator *translator,
                  XIEvent            *ev,
                  GdkSurface         **surface_p)
{
  GdkDisplay *display;
  GdkSurface *surface = NULL;
  gboolean should_have_window = TRUE;

  display = GDK_X11_DEVICE_MANAGER_XI2 (translator)->display;

  switch (ev->evtype)
    {
    case XI_KeyPress:
    case XI_KeyRelease:
    case XI_ButtonPress:
    case XI_ButtonRelease:
    case XI_Motion:
#ifdef XINPUT_2_2
    case XI_TouchUpdate:
    case XI_TouchBegin:
    case XI_TouchEnd:
#endif /* XINPUT_2_2 */
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) ev;

        surface = gdk_x11_surface_lookup_for_display (display, xev->event);

        /* Apply keyboard grabs to non-native windows */
        if (ev->evtype == XI_KeyPress || ev->evtype == XI_KeyRelease)
          {
            GdkDeviceGrabInfo *info;
            GdkDevice *device;
            gulong serial;

            device = g_hash_table_lookup (GDK_X11_DEVICE_MANAGER_XI2 (translator)->id_table,
                                          GUINT_TO_POINTER (((XIDeviceEvent *) ev)->deviceid));

            serial = _gdk_display_get_next_serial (display);
            info = _gdk_display_has_device_grab (display, device, serial);

            if (info && !info->owner_events)
              {
                /* Report key event against grab surface */
                surface = info->surface;
              }
          }
      }
      break;
    case XI_Enter:
    case XI_Leave:
    case XI_FocusIn:
    case XI_FocusOut:
      {
        XIEnterEvent *xev = (XIEnterEvent *) ev;

        surface = gdk_x11_surface_lookup_for_display (display, xev->event);
      }
      break;
    default:
      should_have_window = FALSE;
      break;
    }

  *surface_p = surface;

  if (should_have_window && !surface)
    return FALSE;

  return TRUE;
}

static gboolean
scroll_valuators_changed (GdkX11DeviceXI2 *device,
                          XIValuatorState *valuators,
                          gdouble         *dx,
                          gdouble         *dy)
{
  gboolean has_scroll_valuators = FALSE;
  GdkScrollDirection direction;
  guint n_axes, i, n_val;
  gdouble *vals;

  n_axes = gdk_device_get_n_axes (GDK_DEVICE (device));
  vals = valuators->values;
  *dx = *dy = 0;
  n_val = 0;

  for (i = 0; i < MIN (valuators->mask_len * 8, n_axes); i++)
    {
      gdouble delta;

      if (!XIMaskIsSet (valuators->mask, i))
        continue;

      if (_gdk_x11_device_xi2_get_scroll_delta (device, i, vals[n_val],
                                                &direction, &delta))
        {
          has_scroll_valuators = TRUE;

          if (direction == GDK_SCROLL_UP ||
              direction == GDK_SCROLL_DOWN)
            *dy = delta;
          else
            *dx = delta;
        }

      n_val++;
    }

  return has_scroll_valuators;
}

/* We only care about focus events that indicate that _this_
 * surface (not an ancestor or child) got or lost the focus
 */
static void
_gdk_device_manager_xi2_handle_focus (GdkSurface *surface,
                                      Window      original,
                                      GdkDevice  *device,
                                      GdkDevice  *source_device,
                                      gboolean    focus_in,
                                      int         detail,
                                      int         mode)
{
  GdkToplevelX11 *toplevel;
  GdkX11Screen *x11_screen;
  gboolean had_focus;

  g_return_if_fail (GDK_IS_SURFACE (surface));
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (source_device == NULL || GDK_IS_DEVICE (source_device));

  GDK_DISPLAY_NOTE (gdk_surface_get_display (surface), EVENTS,
            g_message ("focus out:\t\twindow: %ld, detail: %s, mode: %s",
                       GDK_SURFACE_XID (surface),
                       notify_details[detail],
                       notify_modes[mode]));

  toplevel = _gdk_x11_surface_get_toplevel (surface);

  if (!toplevel)
    return;

  if (toplevel->focus_window == original)
    return;

  had_focus = HAS_FOCUS (toplevel);
  x11_screen = GDK_X11_SCREEN (GDK_SURFACE_SCREEN (surface));

  switch (detail)
    {
    case NotifyAncestor:
    case NotifyVirtual:
      /* When the focus moves from an ancestor of the window to
       * the window or a descendent of the window, *and* the
       * pointer is inside the window, then we were previously
       * receiving keystroke events in the has_pointer_focus
       * case and are now receiving them in the
       * has_focus_window case.
       */
      if (toplevel->has_pointer &&
          !x11_screen->wmspec_check_window &&
          mode != NotifyGrab &&
          mode != XINotifyPassiveGrab &&
          mode != XINotifyPassiveUngrab &&
          mode != NotifyUngrab)
        toplevel->has_pointer_focus = (focus_in) ? FALSE : TRUE;
      G_GNUC_FALLTHROUGH;

    case NotifyNonlinear:
    case NotifyNonlinearVirtual:
      if (mode != NotifyGrab &&
          mode != XINotifyPassiveGrab &&
          mode != XINotifyPassiveUngrab &&
          mode != NotifyUngrab)
        toplevel->has_focus_window = (focus_in) ? TRUE : FALSE;
      /* We pretend that the focus moves to the grab
       * window, so we pay attention to NotifyGrab
       * NotifyUngrab, and ignore NotifyWhileGrabbed
       */
      if (mode != NotifyWhileGrabbed)
        toplevel->has_focus = (focus_in) ? TRUE : FALSE;
      break;
    case NotifyPointer:
      /* The X server sends NotifyPointer/NotifyGrab,
       * but the pointer focus is ignored while a
       * grab is in effect
       */
      if (!x11_screen->wmspec_check_window &&
          mode != NotifyGrab &&
          mode != XINotifyPassiveGrab &&
          mode != XINotifyPassiveUngrab &&
          mode != NotifyUngrab)
        toplevel->has_pointer_focus = (focus_in) ? TRUE : FALSE;
      break;
    case NotifyInferior:
    case NotifyPointerRoot:
    case NotifyDetailNone:
    default:
      break;
    }

  if (HAS_FOCUS (toplevel) != had_focus)
    {
      GdkEvent *event;

      event = gdk_event_new (GDK_FOCUS_CHANGE);
      event->any.surface = g_object_ref (surface);
      event->any.send_event = FALSE;
      event->focus_change.in = focus_in;
      gdk_event_set_device (event, device);
      if (source_device)
        gdk_event_set_source_device (event, source_device);

      gdk_display_put_event (gdk_surface_get_display (surface), event);
      g_object_unref (event);
    }
}

static gboolean
gdk_x11_device_manager_xi2_translate_event (GdkEventTranslator *translator,
                                            GdkDisplay         *display,
                                            GdkEvent           *event,
                                            const XEvent       *xevent)
{
  GdkX11DeviceManagerXI2 *device_manager;
  const XGenericEventCookie *cookie;
  GdkDevice *device, *source_device;
  gboolean return_val = TRUE;
  GdkSurface *surface;
  GdkX11Surface *impl;
  int scale;
  XIEvent *ev;

  device_manager = (GdkX11DeviceManagerXI2 *) translator;
  cookie = &xevent->xcookie;

  if (xevent->type != GenericEvent ||
      cookie->extension != device_manager->opcode)
    return FALSE;

  ev = (XIEvent *) cookie->data;

  if (!ev)
    return FALSE;

  if (!get_event_surface (translator, ev, &surface))
    return FALSE;

  if (surface && GDK_SURFACE_DESTROYED (surface))
    return FALSE;

  scale = 1;
  if (surface)
    {
      impl = GDK_X11_SURFACE (surface);
      scale = impl->surface_scale;
    }

  if (ev->evtype == XI_Motion ||
      ev->evtype == XI_ButtonRelease)
    {
      if (_gdk_x11_moveresize_handle_event (xevent))
        return FALSE;
    }

  switch (ev->evtype)
    {
    case XI_HierarchyChanged:
      handle_hierarchy_changed (device_manager,
                                (XIHierarchyEvent *) ev);
      return_val = FALSE;
      break;
    case XI_DeviceChanged:
      handle_device_changed (device_manager,
                             (XIDeviceChangedEvent *) ev);
      return_val = FALSE;
      break;
    case XI_PropertyEvent:
      handle_property_change (device_manager,
                              (XIPropertyEvent *) ev);
      return_val = FALSE;
      break;
    case XI_KeyPress:
    case XI_KeyRelease:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) ev;
        GdkKeymap *keymap = gdk_display_get_keymap (display);
        GdkModifierType consumed, state;

        GDK_DISPLAY_NOTE (display, EVENTS,
                  g_message ("key %s:\twindow %ld\n"
                             "\tdevice:%u\n"
                             "\tsource device:%u\n"
                             "\tkey number: %u\n",
                             (ev->evtype == XI_KeyPress) ? "press" : "release",
                             xev->event,
                             xev->deviceid,
                             xev->sourceid,
                             xev->detail));

        event->any.type = xev->evtype == XI_KeyPress ? GDK_KEY_PRESS : GDK_KEY_RELEASE;

        event->any.surface = surface;

        event->key.time = xev->time;
        event->key.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);
        event->key.group = xev->group.effective;

        event->key.hardware_keycode = xev->detail;
        gdk_event_set_scancode (event, xev->detail);
        event->key.is_modifier = gdk_x11_keymap_key_is_modifier (keymap, event->key.hardware_keycode);

        device = g_hash_table_lookup (device_manager->id_table,
                                      GUINT_TO_POINTER (xev->deviceid));
        gdk_event_set_device (event, device);

        source_device = g_hash_table_lookup (device_manager->id_table,
                                             GUINT_TO_POINTER (xev->sourceid));
        gdk_event_set_source_device (event, source_device);

        event->key.keyval = GDK_KEY_VoidSymbol;

        gdk_keymap_translate_keyboard_state (keymap,
                                             event->key.hardware_keycode,
                                             event->key.state,
                                             event->key.group,
                                             &event->key.keyval,
                                             NULL, NULL, &consumed);

        state = event->key.state & ~consumed;
        _gdk_x11_keymap_add_virt_mods (keymap, &state);
        event->key.state |= state;

        if (ev->evtype == XI_KeyPress)
          set_user_time (event);

        /* FIXME: emulate autorepeat on key
         * release? XI2 seems attached to Xkb.
         */
      }

      break;
    case XI_ButtonPress:
    case XI_ButtonRelease:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) ev;

        GDK_DISPLAY_NOTE (display, EVENTS,
                  g_message ("button %s:\twindow %ld\n"
                             "\tdevice:%u\n"
                             "\tsource device:%u\n"
                             "\tbutton number: %u\n"
                             "\tx,y: %.2f %.2f",
                             (ev->evtype == XI_ButtonPress) ? "press" : "release",
                             xev->event,
                             xev->deviceid,
                             xev->sourceid,
                             xev->detail,
                             xev->event_x, xev->event_y));

#ifdef XINPUT_2_2
        if (xev->flags & XIPointerEmulated)
          return FALSE;
#endif

        if (ev->evtype == XI_ButtonRelease &&
            (xev->detail >= 4 && xev->detail <= 7))
          return FALSE;
        else if (ev->evtype == XI_ButtonPress &&
                 (xev->detail >= 4 && xev->detail <= 7))
          {
            /* Button presses of button 4-7 are scroll events */
            event->any.type = GDK_SCROLL;

            if (xev->detail == 4)
              event->scroll.direction = GDK_SCROLL_UP;
            else if (xev->detail == 5)
              event->scroll.direction = GDK_SCROLL_DOWN;
            else if (xev->detail == 6)
              event->scroll.direction = GDK_SCROLL_LEFT;
            else
              event->scroll.direction = GDK_SCROLL_RIGHT;

            event->any.surface = surface;
            event->scroll.time = xev->time;
            event->scroll.x = (gdouble) xev->event_x / scale;
            event->scroll.y = (gdouble) xev->event_y / scale;
            event->scroll.x_root = (gdouble) xev->root_x / scale;
            event->scroll.y_root = (gdouble) xev->root_y / scale;
            event->scroll.delta_x = 0;
            event->scroll.delta_y = 0;

            device = g_hash_table_lookup (device_manager->id_table,
                                          GUINT_TO_POINTER (xev->deviceid));
            gdk_event_set_device (event, device);

            source_device = g_hash_table_lookup (device_manager->id_table,
                                                 GUINT_TO_POINTER (xev->sourceid));
            gdk_event_set_source_device (event, source_device);

            event->scroll.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);
          }
        else
          {
            event->any.type = (ev->evtype == XI_ButtonPress) ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE;

            event->any.surface = surface;
            event->button.time = xev->time;
            event->button.x = (gdouble) xev->event_x / scale;
            event->button.y = (gdouble) xev->event_y / scale;
            event->button.x_root = (gdouble) xev->root_x / scale;
            event->button.y_root = (gdouble) xev->root_y / scale;

            device = g_hash_table_lookup (device_manager->id_table,
                                          GUINT_TO_POINTER (xev->deviceid));
            gdk_event_set_device (event, device);

            source_device = g_hash_table_lookup (device_manager->id_table,
                                                 GUINT_TO_POINTER (xev->sourceid));
            gdk_event_set_source_device (event, source_device);
            gdk_event_set_device_tool (event, source_device->last_tool);

            event->button.axes = translate_axes (device,
                                                 event->button.x,
                                                 event->button.y,
                                                 event->any.surface,
                                                 &xev->valuators);

            if (gdk_device_get_mode (device) == GDK_MODE_SURFACE)
              {
                /* Update event coordinates from axes */
                gdk_device_get_axis (device, event->button.axes, GDK_AXIS_X, &event->button.x);
                gdk_device_get_axis (device, event->button.axes, GDK_AXIS_Y, &event->button.y);
              }

            event->button.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);

            event->button.button = xev->detail;
          }

        if (return_val == FALSE)
          break;

        gdk_event_set_display (event, display);

        if (ev->evtype == XI_ButtonPress)
	  set_user_time (event);

        break;
      }

    case XI_Motion:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) ev;
        gdouble delta_x, delta_y;

#ifdef XINPUT_2_2
        if (xev->flags & XIPointerEmulated)
          return FALSE;
#endif

        source_device = g_hash_table_lookup (device_manager->id_table,
                                             GUINT_TO_POINTER (xev->sourceid));
        device = g_hash_table_lookup (device_manager->id_table,
                                      GUINT_TO_POINTER (xev->deviceid));

        /* When scrolling, X might send events twice here; once with both the
         * device and the source device set to the physical device, and once
         * with the device set to the master device.
         * Since we are only interested in the latter, and
         * scroll_valuators_changed() updates the valuator cache for the
         * source device, we need to explicitly ignore the first event in
         * order to get the correct delta for the second.
         */
        if (gdk_device_get_device_type (device) != GDK_DEVICE_TYPE_SLAVE &&
            scroll_valuators_changed (GDK_X11_DEVICE_XI2 (source_device),
                                      &xev->valuators, &delta_x, &delta_y))
          {
            event->any.type = GDK_SCROLL;
            event->scroll.direction = GDK_SCROLL_SMOOTH;

            if (delta_x == 0.0 && delta_y == 0.0)
              event->scroll.is_stop = TRUE;

            GDK_DISPLAY_NOTE (display, EVENTS,
                     g_message ("smooth scroll: \n\tdevice: %u\n\tsource device: %u\n\twindow %ld\n\tdeltas: %f %f",
                                xev->deviceid, xev->sourceid,
                                xev->event, delta_x, delta_y));


            event->any.surface = surface;
            event->scroll.time = xev->time;
            event->scroll.x = (gdouble) xev->event_x / scale;
            event->scroll.y = (gdouble) xev->event_y / scale;
            event->scroll.x_root = (gdouble) xev->root_x / scale;
            event->scroll.y_root = (gdouble) xev->root_y / scale;
            event->scroll.delta_x = delta_x;
            event->scroll.delta_y = delta_y;

            gdk_event_set_device (event, device);
            gdk_event_set_source_device (event, source_device);

            event->scroll.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);
            break;
          }

        event->any.type = GDK_MOTION_NOTIFY;
        event->any.surface = surface;
        event->motion.time = xev->time;
        event->motion.x = (gdouble) xev->event_x / scale;
        event->motion.y = (gdouble) xev->event_y / scale;
        event->motion.x_root = (gdouble) xev->root_x / scale;
        event->motion.y_root = (gdouble) xev->root_y / scale;

        gdk_event_set_device (event, device);
        gdk_event_set_source_device (event, source_device);
        gdk_event_set_device_tool (event, source_device->last_tool);

        event->motion.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);

        event->motion.axes = translate_axes (device,
                                             event->motion.x,
                                             event->motion.y,
                                             event->any.surface,
                                             &xev->valuators);

        if (gdk_device_get_mode (device) == GDK_MODE_SURFACE)
          {
            /* Update event coordinates from axes */
            gdk_device_get_axis (device, event->motion.axes, GDK_AXIS_X, &event->motion.x);
            gdk_device_get_axis (device, event->motion.axes, GDK_AXIS_Y, &event->motion.y);
          }
      }
      break;

#ifdef XINPUT_2_2
    case XI_TouchBegin:
    case XI_TouchEnd:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) ev;

        GDK_DISPLAY_NOTE (display, EVENTS,
                 g_message ("touch %s:\twindow %ld\n\ttouch id: %u\n\tpointer emulating: %s",
                            ev->evtype == XI_TouchBegin ? "begin" : "end",
                            xev->event,
                            xev->detail,
                            xev->flags & XITouchEmulatingPointer ? "true" : "false"));

        if (ev->evtype == XI_TouchBegin)
          event->any.type = GDK_TOUCH_BEGIN;
        else if (ev->evtype == XI_TouchEnd)
          event->any.type = GDK_TOUCH_END;

        event->any.surface = surface;
        event->touch.time = xev->time;
        event->touch.x = (gdouble) xev->event_x / scale;
        event->touch.y = (gdouble) xev->event_y / scale;
        event->touch.x_root = (gdouble) xev->root_x / scale;
        event->touch.y_root = (gdouble) xev->root_y / scale;

        device = g_hash_table_lookup (device_manager->id_table,
                                      GUINT_TO_POINTER (xev->deviceid));
        gdk_event_set_device (event, device);

        source_device = g_hash_table_lookup (device_manager->id_table,
                                             GUINT_TO_POINTER (xev->sourceid));
        gdk_event_set_source_device (event, source_device);

        event->touch.axes = translate_axes (device,
                                            event->touch.x,
                                            event->touch.y,
                                            event->any.surface,
                                            &xev->valuators);

        if (gdk_device_get_mode (device) == GDK_MODE_SURFACE)
          {
            /* Update event coordinates from axes */
            gdk_device_get_axis (device, event->touch.axes, GDK_AXIS_X, &event->touch.x);
            gdk_device_get_axis (device, event->touch.axes, GDK_AXIS_Y, &event->touch.y);
          }

        event->touch.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);

        if (ev->evtype == XI_TouchBegin)
          event->touch.state |= GDK_BUTTON1_MASK;

        event->touch.sequence = GUINT_TO_POINTER (xev->detail);

        if (xev->flags & XITouchEmulatingPointer)
          {
            event->touch.emulating_pointer = TRUE;
            gdk_event_set_pointer_emulated (event, TRUE);
          }

        if (return_val == FALSE)
          break;

        gdk_event_set_display (event, display);

        if (ev->evtype == XI_TouchBegin)
          set_user_time (event);
      }
      break;

    case XI_TouchUpdate:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) ev;

        GDK_DISPLAY_NOTE (display, EVENTS,
                 g_message ("touch update:\twindow %ld\n\ttouch id: %u\n\tpointer emulating: %s",
                            xev->event,
                            xev->detail,
                            xev->flags & XITouchEmulatingPointer ? "true" : "false"));

        event->any.surface = surface;
        event->touch.sequence = GUINT_TO_POINTER (xev->detail);
        event->any.type = GDK_TOUCH_UPDATE;
        event->touch.time = xev->time;
        event->touch.x = (gdouble) xev->event_x / scale;
        event->touch.y = (gdouble) xev->event_y / scale;
        event->touch.x_root = (gdouble) xev->root_x / scale;
        event->touch.y_root = (gdouble) xev->root_y / scale;

        device = g_hash_table_lookup (device_manager->id_table,
                                      GINT_TO_POINTER (xev->deviceid));
        gdk_event_set_device (event, device);

        source_device = g_hash_table_lookup (device_manager->id_table,
                                             GUINT_TO_POINTER (xev->sourceid));
        gdk_event_set_source_device (event, source_device);

        event->touch.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);

        event->touch.state |= GDK_BUTTON1_MASK;

        if (xev->flags & XITouchEmulatingPointer)
          {
            event->touch.emulating_pointer = TRUE;
            gdk_event_set_pointer_emulated (event, TRUE);
          }

        event->touch.axes = translate_axes (device,
                                            event->touch.x,
                                            event->touch.y,
                                            event->any.surface,
                                            &xev->valuators);

        if (gdk_device_get_mode (device) == GDK_MODE_SURFACE)
          {
            /* Update event coordinates from axes */
            gdk_device_get_axis (device, event->touch.axes, GDK_AXIS_X, &event->touch.x);
            gdk_device_get_axis (device, event->touch.axes, GDK_AXIS_Y, &event->touch.y);
          }
      }
      break;
#endif  /* XINPUT_2_2 */

    case XI_Enter:
    case XI_Leave:
      {
        XIEnterEvent *xev = (XIEnterEvent *) ev;

        GDK_DISPLAY_NOTE (display, EVENTS,
                  g_message ("%s notify:\twindow %ld\n\tsubwindow:%ld\n"
                             "\tdevice: %u\n\tsource device: %u\n"
                             "\tnotify type: %u\n\tcrossing mode: %u",
                             (ev->evtype == XI_Enter) ? "enter" : "leave",
                             xev->event, xev->child,
                             xev->deviceid, xev->sourceid,
                             xev->detail, xev->mode));

        event->any.type = (ev->evtype == XI_Enter) ? GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY;

        event->crossing.x = (gdouble) xev->event_x / scale;
        event->crossing.y = (gdouble) xev->event_y / scale;
        event->crossing.x_root = (gdouble) xev->root_x / scale;
        event->crossing.y_root = (gdouble) xev->root_y / scale;
        event->crossing.time = xev->time;
        event->crossing.focus = xev->focus;

        event->any.surface = surface;
        event->crossing.child_surface = gdk_x11_surface_lookup_for_display (display, xev->child);

        device = g_hash_table_lookup (device_manager->id_table,
                                      GINT_TO_POINTER (xev->deviceid));
        gdk_event_set_device (event, device);

        source_device = g_hash_table_lookup (device_manager->id_table,
                                             GUINT_TO_POINTER (xev->sourceid));
        gdk_event_set_source_device (event, source_device);

        if (ev->evtype == XI_Enter &&
            xev->detail != XINotifyInferior && xev->mode != XINotifyPassiveUngrab &&
	    gdk_surface_get_surface_type (surface) == GDK_SURFACE_TOPLEVEL)
          {
            if (gdk_device_get_device_type (source_device) != GDK_DEVICE_TYPE_MASTER)
              _gdk_device_xi2_reset_scroll_valuators (GDK_X11_DEVICE_XI2 (source_device));
            else
              {
                GList *slaves, *l;

                slaves = gdk_device_list_slave_devices (source_device);

                for (l = slaves; l; l = l->next)
                  _gdk_device_xi2_reset_scroll_valuators (GDK_X11_DEVICE_XI2 (l->data));

                g_list_free (slaves);
              }
          }

        event->crossing.mode = translate_crossing_mode (xev->mode);
        event->crossing.detail = translate_notify_type (xev->detail);
        event->crossing.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);
      }
      break;
    case XI_FocusIn:
    case XI_FocusOut:
      {
        if (surface)
          {
            XIEnterEvent *xev = (XIEnterEvent *) ev;

            device = g_hash_table_lookup (device_manager->id_table,
                                          GINT_TO_POINTER (xev->deviceid));

            source_device = g_hash_table_lookup (device_manager->id_table,
                                                 GUINT_TO_POINTER (xev->sourceid));

            _gdk_device_manager_xi2_handle_focus (surface,
                                                  xev->event,
                                                  device,
                                                  source_device,
                                                  (ev->evtype == XI_FocusIn) ? TRUE : FALSE,
                                                  xev->detail,
                                                  xev->mode);
          }

        return_val = FALSE;
      }
      break;
    default:
      return_val = FALSE;
      break;
    }

  event->any.send_event = cookie->send_event;

  if (return_val)
    {
      if (event->any.surface)
        g_object_ref (event->any.surface);

      if (((event->any.type == GDK_ENTER_NOTIFY) ||
           (event->any.type == GDK_LEAVE_NOTIFY)) &&
          (event->crossing.child_surface != NULL))
        g_object_ref (event->crossing.child_surface);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.surface = NULL;
      event->any.type = GDK_NOTHING;
    }

  return return_val;
}

static GdkEventMask
gdk_x11_device_manager_xi2_get_handled_events (GdkEventTranslator *translator)
{
  return (GDK_KEY_PRESS_MASK |
          GDK_KEY_RELEASE_MASK |
          GDK_BUTTON_PRESS_MASK |
          GDK_BUTTON_RELEASE_MASK |
          GDK_SCROLL_MASK |
          GDK_ENTER_NOTIFY_MASK |
          GDK_LEAVE_NOTIFY_MASK |
          GDK_POINTER_MOTION_MASK |
          GDK_BUTTON1_MOTION_MASK |
          GDK_BUTTON2_MOTION_MASK |
          GDK_BUTTON3_MOTION_MASK |
          GDK_BUTTON_MOTION_MASK |
          GDK_FOCUS_CHANGE_MASK |
          GDK_TOUCH_MASK);
}

static void
gdk_x11_device_manager_xi2_select_surface_events (GdkEventTranslator *translator,
                                                 Window              window,
                                                 GdkEventMask        evmask)
{
  XIEventMask event_mask;

  event_mask.deviceid = XIAllMasterDevices;
  event_mask.mask = _gdk_x11_device_xi2_translate_event_mask (GDK_X11_DEVICE_MANAGER_XI2 (translator),
                                                              evmask,
                                                              &event_mask.mask_len);

  _gdk_x11_device_manager_xi2_select_events (GDK_X11_DEVICE_MANAGER_XI2 (translator), window, &event_mask);
  g_free (event_mask.mask);
}

static GdkSurface *
gdk_x11_device_manager_xi2_get_surface (GdkEventTranslator *translator,
                                        const XEvent       *xevent)
{
  GdkX11DeviceManagerXI2 *device_manager;
  XIEvent *ev;
  GdkSurface *surface = NULL;

  device_manager = (GdkX11DeviceManagerXI2 *) translator;

  if (xevent->type != GenericEvent ||
      xevent->xcookie.extension != device_manager->opcode)
    return NULL;

  ev = (XIEvent *) xevent->xcookie.data;
  if (!ev)
    return NULL;

  get_event_surface (translator, ev, &surface);
  return surface;
}

GdkDevice *
_gdk_x11_device_manager_xi2_lookup (GdkX11DeviceManagerXI2 *device_manager_xi2,
                                    gint                    device_id)
{
  return g_hash_table_lookup (device_manager_xi2->id_table,
                              GINT_TO_POINTER (device_id));
}
