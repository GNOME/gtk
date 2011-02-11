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

#include "gdkx11devicemanager-xi2.h"
#include "gdkx11device-xi2.h"

#include "gdkdevicemanagerprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkeventtranslator.h"
#include "gdkprivate-x11.h"
#include "gdkintl.h"
#include "gdkkeysyms.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

#include <string.h>

struct _GdkX11DeviceManagerXI2
{
  GdkDeviceManager parent_object;

  GHashTable *id_table;

  GList *devices;

  gint opcode;
};

struct _GdkX11DeviceManagerXI2Class
{
  GdkDeviceManagerClass parent_class;
};

#define HAS_FOCUS(toplevel) ((toplevel)->has_focus || (toplevel)->has_pointer_focus)


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

static GList * gdk_x11_device_manager_xi2_list_devices (GdkDeviceManager *device_manager,
                                                        GdkDeviceType     type);
static GdkDevice * gdk_x11_device_manager_xi2_get_client_pointer (GdkDeviceManager *device_manager);

static void     gdk_x11_device_manager_xi2_event_translator_init (GdkEventTranslatorIface *iface);

static gboolean gdk_x11_device_manager_xi2_translate_event (GdkEventTranslator *translator,
                                                            GdkDisplay         *display,
                                                            GdkEvent           *event,
                                                            XEvent             *xevent);
static GdkEventMask gdk_x11_device_manager_xi2_get_handled_events   (GdkEventTranslator *translator);
static void         gdk_x11_device_manager_xi2_select_window_events (GdkEventTranslator *translator,
                                                                     Window              window,
                                                                     GdkEventMask        event_mask);


G_DEFINE_TYPE_WITH_CODE (GdkX11DeviceManagerXI2, gdk_x11_device_manager_xi2, GDK_TYPE_X11_DEVICE_MANAGER_CORE,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_EVENT_TRANSLATOR,
                                                gdk_x11_device_manager_xi2_event_translator_init))


enum {
  PROP_0,
  PROP_OPCODE
};

static void
gdk_x11_device_manager_xi2_class_init (GdkX11DeviceManagerXI2Class *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = gdk_x11_device_manager_xi2_constructed;
  object_class->dispose = gdk_x11_device_manager_xi2_dispose;
  object_class->set_property = gdk_x11_device_manager_xi2_set_property;
  object_class->get_property = gdk_x11_device_manager_xi2_get_property;

  device_manager_class->list_devices = gdk_x11_device_manager_xi2_list_devices;
  device_manager_class->get_client_pointer = gdk_x11_device_manager_xi2_get_client_pointer;

  g_object_class_install_property (object_class,
                                   PROP_OPCODE,
                                   g_param_spec_int ("opcode",
                                                     P_("Opcode"),
                                                     P_("Opcode for XInput2 requests"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
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
_gdk_x11_device_manager_xi2_select_events (GdkDeviceManager *device_manager,
                                           Window            xwindow,
                                           XIEventMask      *event_mask)
{
  GdkDisplay *display;
  Display *xdisplay;

  display = gdk_device_manager_get_display (device_manager);
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  XISelectEvents (xdisplay, xwindow, event_mask, 1);
}

static void
translate_valuator_class (GdkDisplay          *display,
                          GdkDevice           *device,
                          XIValuatorClassInfo *info,
                          gint                 n_valuator)
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

  for (i = GDK_AXIS_IGNORE; i <= GDK_AXIS_LAST; i++)
    {
      if (label_atoms[i] == info->label)
        {
          use = i;
          break;
        }
    }

  if (info->label != None)
    label = gdk_x11_xatom_to_atom_for_display (display, info->label);
  else
    label = GDK_NONE;

  _gdk_device_add_axis (device,
                        label,
                        use,
                        info->min,
                        info->max,
                        info->resolution);
}

static void
translate_device_classes (GdkDisplay      *display,
                          GdkDevice       *device,
                          XIAnyClassInfo **classes,
                          guint            n_classes)
{
  gint i, n_valuator = 0;

  g_object_freeze_notify (G_OBJECT (device));

  for (i = 0; i < n_classes; i++)
    {
      XIAnyClassInfo *class_info = classes[i];

      switch (class_info->type)
        {
        case XIKeyClass:
          {
            XIKeyClassInfo *key_info = (XIKeyClassInfo *) class_info;
            gint i;

            _gdk_device_set_keys (device, key_info->num_keycodes);

            for (i = 0; i < key_info->num_keycodes; i++)
              gdk_device_set_key (device, i, key_info->keycodes[i], 0);
          }
          break;
        case XIValuatorClass:
          translate_valuator_class (display, device,
                                    (XIValuatorClassInfo *) class_info,
                                    n_valuator);
          n_valuator++;
          break;
        default:
          /* Ignore */
          break;
        }
    }

  g_object_thaw_notify (G_OBJECT (device));
}

static GdkDevice *
create_device (GdkDeviceManager *device_manager,
               GdkDisplay       *display,
               XIDeviceInfo     *dev)
{
  GdkInputSource input_source;
  GdkDeviceType type;
  GdkDevice *device;
  GdkInputMode mode;

  if (dev->use == XIMasterKeyboard || dev->use == XISlaveKeyboard)
    input_source = GDK_SOURCE_KEYBOARD;
  else
    {
      gchar *tmp_name;

      tmp_name = g_ascii_strdown (dev->name, -1);

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

  device = g_object_new (GDK_TYPE_X11_DEVICE_XI2,
                         "name", dev->name,
                         "type", type,
                         "input-source", input_source,
                         "input-mode", mode,
                         "has-cursor", (dev->use == XIMasterPointer),
                         "display", display,
                         "device-manager", device_manager,
                         "device-id", dev->deviceid,
                         NULL);

  translate_device_classes (display, device, dev->classes, dev->num_classes);

  return device;
}

static GdkDevice *
add_device (GdkX11DeviceManagerXI2 *device_manager,
            XIDeviceInfo           *dev,
            gboolean                emit_signal)
{
  GdkDisplay *display;
  GdkDevice *device;

  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (device_manager));
  device = create_device (GDK_DEVICE_MANAGER (device_manager), display, dev);

  g_hash_table_replace (device_manager->id_table,
                        GINT_TO_POINTER (dev->deviceid),
                        g_object_ref (device));

  device_manager->devices = g_list_append (device_manager->devices, device);

  if (emit_signal)
    {
      if (dev->use == XISlavePointer || dev->use == XISlaveKeyboard)
        {
          GdkDevice *master;

          /* The device manager is already constructed, then
           * keep the hierarchy coherent for the added device.
           */
          master = g_hash_table_lookup (device_manager->id_table,
                                        GINT_TO_POINTER (dev->attachment));

          _gdk_device_set_associated_device (device, master);
          _gdk_device_add_slave (master, device);
        }

      g_signal_emit_by_name (device_manager, "device-added", device);
    }

  return device;
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
      device_manager->devices = g_list_remove (device_manager->devices, device);

      g_signal_emit_by_name (device_manager, "device-removed", device);

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
}

static void
relate_slaves (gpointer key,
               gpointer value,
               gpointer user_data)
{
  GdkX11DeviceManagerXI2 *device_manager;
  GdkDevice *slave, *master;

  device_manager = user_data;
  slave = g_hash_table_lookup (device_manager->id_table, key);
  master = g_hash_table_lookup (device_manager->id_table, value);

  _gdk_device_set_associated_device (slave, master);
  _gdk_device_add_slave (master, slave);
}

static void
gdk_x11_device_manager_xi2_constructed (GObject *object)
{
  GdkX11DeviceManagerXI2 *device_manager;
  GdkDisplay *display;
  GdkScreen *screen;
  GHashTable *masters, *slaves;
  Display *xdisplay;
  XIDeviceInfo *info, *dev;
  int ndevices, i;
  XIEventMask event_mask;
  unsigned char mask[2] = { 0 };

  device_manager = GDK_X11_DEVICE_MANAGER_XI2 (object);
  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (object));
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  masters = g_hash_table_new (NULL, NULL);
  slaves = g_hash_table_new (NULL, NULL);

  info = XIQueryDevice (xdisplay, XIAllDevices, &ndevices);

  /* Initialize devices list */
  for (i = 0; i < ndevices; i++)
    {
      dev = &info[i];
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
  screen = gdk_display_get_default_screen (display);
  XISetMask (mask, XI_HierarchyChanged);
  XISetMask (mask, XI_DeviceChanged);

  event_mask.deviceid = XIAllDevices;
  event_mask.mask_len = sizeof (mask);
  event_mask.mask = mask;

  _gdk_x11_device_manager_xi2_select_events (GDK_DEVICE_MANAGER (object),
                                             GDK_WINDOW_XID (gdk_screen_get_root_window (screen)),
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

static GList *
gdk_x11_device_manager_xi2_list_devices (GdkDeviceManager *device_manager,
                                         GdkDeviceType     type)
{
  GdkX11DeviceManagerXI2 *device_manager_xi2;
  GList *cur, *list = NULL;

  device_manager_xi2 = GDK_X11_DEVICE_MANAGER_XI2 (device_manager);

  for (cur = device_manager_xi2->devices; cur; cur = cur->next)
    {
      GdkDevice *dev = cur->data;

      if (type == gdk_device_get_device_type (dev))
        list = g_list_prepend (list, dev);
    }

  return list;
}

static GdkDevice *
gdk_x11_device_manager_xi2_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkX11DeviceManagerXI2 *device_manager_xi2;
  GdkDisplay *display;
  int device_id;

  device_manager_xi2 = (GdkX11DeviceManagerXI2 *) device_manager;
  display = gdk_device_manager_get_display (device_manager);

  XIGetClientPointer (GDK_DISPLAY_XDISPLAY (display),
                      None, &device_id);

  return g_hash_table_lookup (device_manager_xi2->id_table,
                              GINT_TO_POINTER (device_id));
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
    case PROP_OPCODE:
      device_manager->opcode = g_value_get_int (value);
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
    case PROP_OPCODE:
      g_value_set_int (value, device_manager->opcode);
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
  iface->select_window_events = gdk_x11_device_manager_xi2_select_window_events;
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

  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (device_manager));
  xdisplay = GDK_DISPLAY_XDISPLAY (display);

  for (i = 0; i < ev->num_info; i++)
    {
      if (ev->info[i].flags & XIDeviceEnabled)
        {
          info = XIQueryDevice (xdisplay, ev->info[i].deviceid, &ndevices);
          add_device (device_manager, &info[0], TRUE);
          XIFreeDeviceInfo (info);
        }
      else if (ev->info[i].flags & XIDeviceDisabled)
        remove_device (device_manager, ev->info[i].deviceid);
      else if (ev->info[i].flags & XISlaveAttached ||
               ev->info[i].flags & XISlaveDetached)
        {
          GdkDevice *master, *slave;

          slave = g_hash_table_lookup (device_manager->id_table,
                                       GINT_TO_POINTER (ev->info[i].deviceid));

          /* Remove old master info */
          master = gdk_device_get_associated_device (slave);

          if (master)
            {
              _gdk_device_remove_slave (master, slave);
              _gdk_device_set_associated_device (slave, NULL);

              g_signal_emit_by_name (device_manager, "device-changed", master);
            }

          /* Add new master if it's an attachment event */
          if (ev->info[i].flags & XISlaveAttached)
            {
              info = XIQueryDevice (xdisplay, ev->info[i].deviceid, &ndevices);

              master = g_hash_table_lookup (device_manager->id_table,
                                            GINT_TO_POINTER (info->attachment));

              _gdk_device_set_associated_device (slave, master);
              _gdk_device_add_slave (master, slave);

              g_signal_emit_by_name (device_manager, "device-changed", master);
            }

          g_signal_emit_by_name (device_manager, "device-changed", slave);
        }
    }
}

static void
handle_device_changed (GdkX11DeviceManagerXI2 *device_manager,
                       XIDeviceChangedEvent   *ev)
{
  GdkDisplay *display;
  GdkDevice *device;

  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (device_manager));
  device = g_hash_table_lookup (device_manager->id_table,
                                GUINT_TO_POINTER (ev->deviceid));

  _gdk_device_reset_axes (device);
  translate_device_classes (display, device, ev->classes, ev->num_classes);

  g_signal_emit_by_name (G_OBJECT (device), "changed");
}

static GdkCrossingMode
translate_crossing_mode (gint mode)
{
  switch (mode)
    {
    case NotifyNormal:
      return GDK_CROSSING_NORMAL;
    case NotifyGrab:
      return GDK_CROSSING_GRAB;
    case NotifyUngrab:
      return GDK_CROSSING_UNGRAB;
    default:
      g_assert_not_reached ();
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
    }
}

static gboolean
set_screen_from_root (GdkDisplay *display,
                      GdkEvent   *event,
                      Window      xrootwin)
{
  GdkScreen *screen;

  screen = _gdk_x11_display_screen_for_xrootwin (display, xrootwin);

  if (screen)
    {
      gdk_event_set_screen (event, screen);

      return TRUE;
    }

  return FALSE;
}

static void
set_user_time (GdkEvent *event)
{
  GdkWindow *window;
  guint32 time;

  window = gdk_window_get_toplevel (event->any.window);
  g_return_if_fail (GDK_IS_WINDOW (window));

  time = gdk_event_get_time (event);

  /* If an event doesn't have a valid timestamp, we shouldn't use it
   * to update the latest user interaction time.
   */
  if (time != GDK_CURRENT_TIME)
    gdk_x11_window_set_user_time (window, time);
}

static void
generate_focus_event (GdkWindow *window,
                      GdkDevice *device,
                      GdkDevice *source_device,
                      gboolean   in)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (window);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = in;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, source_device);

  gdk_event_put (event);
  gdk_event_free (event);
}

static void
handle_focus_change (GdkWindow *window,
                     GdkDevice *device,
                     GdkDevice *source_device,
                     gint       detail,
                     gint       mode,
                     gboolean   in)
{
  GdkToplevelX11 *toplevel;
  gboolean had_focus;

  toplevel = _gdk_x11_window_get_toplevel (window);

  if (!toplevel)
    return;

  had_focus = HAS_FOCUS (toplevel);

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
          mode != NotifyGrab &&
          mode != NotifyUngrab)
        toplevel->has_pointer_focus = (in) ? FALSE : TRUE;

      /* fall through */
    case NotifyNonlinear:
    case NotifyNonlinearVirtual:
      if (mode != NotifyGrab &&
          mode != NotifyUngrab)
        toplevel->has_focus_window = (in) ? TRUE : FALSE;
      /* We pretend that the focus moves to the grab
       * window, so we pay attention to NotifyGrab
       * NotifyUngrab, and ignore NotifyWhileGrabbed
       */
      if (mode != NotifyWhileGrabbed)
        toplevel->has_focus = (in) ? TRUE : FALSE;
      break;
    case NotifyPointer:
      /* The X server sends NotifyPointer/NotifyGrab,
       * but the pointer focus is ignored while a
       * grab is in effect
       */
      if (mode != NotifyGrab &&
          mode != NotifyUngrab)
        toplevel->has_pointer_focus = (in) ? TRUE :FALSE;
      break;
    case NotifyInferior:
    case NotifyPointerRoot:
    case NotifyDetailNone:
      break;
    }

  if (HAS_FOCUS (toplevel) != had_focus)
    generate_focus_event (window, device, source_device, (in) ? TRUE : FALSE);
}

static gdouble *
translate_axes (GdkDevice       *device,
                gdouble          x,
                gdouble          y,
                GdkWindow       *window,
                XIValuatorState *valuators)
{
  guint n_axes, i;
  gdouble *axes;
  gdouble *vals;

  g_object_get (device, "n-axes", &n_axes, NULL);

  axes = g_new0 (gdouble, n_axes);
  vals = valuators->values;

  for (i = 0; i < valuators->mask_len * 8; i++)
    {
      GdkAxisUse use;
      gdouble val;

      if (!XIMaskIsSet (valuators->mask, i))
        continue;

      use = _gdk_device_get_axis_use (device, i);
      val = *vals++;

      switch (use)
        {
        case GDK_AXIS_X:
        case GDK_AXIS_Y:
          if (gdk_device_get_mode (device) == GDK_MODE_WINDOW)
            _gdk_device_translate_window_coord (device, window, i, val, &axes[i]);
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

  return axes;
}

static gboolean
is_parent_of (GdkWindow *parent,
              GdkWindow *child)
{
  GdkWindow *w;

  w = child;
  while (w != NULL)
    {
      if (w == parent)
        return TRUE;

      w = gdk_window_get_parent (w);
    }

  return FALSE;
}

static GdkWindow *
get_event_window (GdkEventTranslator *translator,
                  XIEvent            *ev)
{
  GdkDisplay *display;
  GdkWindow *window = NULL;

  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (translator));

  switch (ev->evtype)
    {
    case XI_KeyPress:
    case XI_KeyRelease:
    case XI_ButtonPress:
    case XI_ButtonRelease:
    case XI_Motion:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) ev;

        window = gdk_x11_window_lookup_for_display (display, xev->event);

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

            if (info &&
                (!is_parent_of (info->window, window) ||
                 !info->owner_events))
              {
                /* Report key event against grab window */
                window = info->window;
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

        window = gdk_x11_window_lookup_for_display (display, xev->event);
      }
      break;
    }

  return window;
}

static gboolean
gdk_x11_device_manager_xi2_translate_event (GdkEventTranslator *translator,
                                            GdkDisplay         *display,
                                            GdkEvent           *event,
                                            XEvent             *xevent)
{
  GdkX11DeviceManagerXI2 *device_manager;
  GdkEventTranslatorIface *parent_iface;
  XGenericEventCookie *cookie;
  gboolean return_val = TRUE;
  GdkWindow *window;
  XIEvent *ev;
  Display *dpy;

  dpy = GDK_DISPLAY_XDISPLAY (display);
  device_manager = (GdkX11DeviceManagerXI2 *) translator;
  cookie = &xevent->xcookie;

  parent_iface = g_type_interface_peek_parent (GDK_EVENT_TRANSLATOR_GET_IFACE (translator));

  /* The X input methods (when triggered via XFilterEvent) generate
   * a core key press event with keycode 0 to signal the end of a
   * key sequence. We use the core translate_event implementation
   * to translate this event.
   *
   * This is just a bandaid fix to keep xim working with a single
   * keyboard until XFilterEvent learns about XI2.
   */
  if (xevent->type == KeyPress && xevent->xkey.keycode == 0 &&
      parent_iface->translate_event (translator, display, event, xevent))
    {
      GdkDevice *device;
      /* The core device manager sets a core device on the event.
       * We need to override that with an XI2 device, since we are
       * using XI2.
       */
      device = gdk_x11_device_manager_xi2_get_client_pointer ((GdkDeviceManager *)device_manager);
      gdk_event_set_device (event, gdk_device_get_associated_device (device));

      return TRUE;
    }

  if (!XGetEventData (dpy, cookie))
    return FALSE;

  if (cookie->type != GenericEvent ||
      cookie->extension != device_manager->opcode)
    {
      XFreeEventData (dpy, cookie);
      return FALSE;
    }

  ev = (XIEvent *) cookie->data;

  window = get_event_window (translator, ev);

  if (window && GDK_WINDOW_DESTROYED (window))
    {
      XFreeEventData (dpy, cookie);
      return FALSE;
    }

  if (ev->evtype == XI_Motion ||
      ev->evtype == XI_ButtonRelease)
    {
      if (_gdk_x11_moveresize_handle_event (xevent))
        {
          XFreeEventData (dpy, cookie);
          return FALSE;
        }
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
    case XI_KeyPress:
    case XI_KeyRelease:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) ev;
        GdkKeymap *keymap = gdk_keymap_get_for_display (display);
        GdkModifierType consumed, state;
        GdkDevice *device, *source_device;

        event->key.type = xev->evtype == XI_KeyPress ? GDK_KEY_PRESS : GDK_KEY_RELEASE;

        event->key.window = window;

        event->key.time = xev->time;
        event->key.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);
        event->key.group = _gdk_x11_get_group_for_state (display, event->key.state);

        event->key.hardware_keycode = xev->detail;
        event->key.is_modifier = _gdk_x11_keymap_key_is_modifier (keymap, event->key.hardware_keycode);

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

        _gdk_x11_event_translate_keyboard_string (&event->key);

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
        GdkDevice *source_device;

        switch (xev->detail)
          {
          case 4:
          case 5:
          case 6:
          case 7:
             /* Button presses of button 4-7 are scroll events */
            if (ev->evtype == XI_ButtonPress)
              {
                event->scroll.type = GDK_SCROLL;

                if (xev->detail == 4)
                  event->scroll.direction = GDK_SCROLL_UP;
                else if (xev->detail == 5)
                  event->scroll.direction = GDK_SCROLL_DOWN;
                else if (xev->detail == 6)
                  event->scroll.direction = GDK_SCROLL_LEFT;
                else
                  event->scroll.direction = GDK_SCROLL_RIGHT;

                event->scroll.window = window;
                event->scroll.time = xev->time;
                event->scroll.x = (gdouble) xev->event_x;
                event->scroll.y = (gdouble) xev->event_y;
                event->scroll.x_root = (gdouble) xev->root_x;
                event->scroll.y_root = (gdouble) xev->root_y;

                event->scroll.device = g_hash_table_lookup (device_manager->id_table,
                                                            GUINT_TO_POINTER (xev->deviceid));

                source_device = g_hash_table_lookup (device_manager->id_table,
                                                     GUINT_TO_POINTER (xev->sourceid));
                gdk_event_set_source_device (event, source_device);

                event->scroll.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);
                break;
              }
            /* else (XI_ButtonRelease) fall thru */
          default:
            event->button.type = (ev->evtype == XI_ButtonPress) ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE;

            event->button.window = window;
            event->button.time = xev->time;
            event->button.x = (gdouble) xev->event_x;
            event->button.y = (gdouble) xev->event_y;
            event->button.x_root = (gdouble) xev->root_x;
            event->button.y_root = (gdouble) xev->root_y;

            event->button.device = g_hash_table_lookup (device_manager->id_table,
                                                        GUINT_TO_POINTER (xev->deviceid));

            source_device = g_hash_table_lookup (device_manager->id_table,
                                                 GUINT_TO_POINTER (xev->sourceid));
            gdk_event_set_source_device (event, source_device);

            event->button.axes = translate_axes (event->button.device,
                                                 event->button.x,
                                                 event->button.y,
                                                 event->button.window,
                                                 &xev->valuators);

            if (gdk_device_get_mode (event->button.device) == GDK_MODE_WINDOW)
              {
                GdkDevice *device = event->button.device;

                /* Update event coordinates from axes */
                gdk_device_get_axis (device, event->button.axes, GDK_AXIS_X, &event->button.x);
                gdk_device_get_axis (device, event->button.axes, GDK_AXIS_Y, &event->button.y);
              }

            event->button.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);
            event->button.button = xev->detail;
          }

        if (!set_screen_from_root (display, event, xev->root))
          {
            return_val = FALSE;
            break;
          }

        set_user_time (event);

        break;
      }
    case XI_Motion:
      {
        XIDeviceEvent *xev = (XIDeviceEvent *) ev;
        GdkDevice *source_device;

        event->motion.type = GDK_MOTION_NOTIFY;

        event->motion.window = window;

        event->motion.time = xev->time;
        event->motion.x = (gdouble) xev->event_x;
        event->motion.y = (gdouble) xev->event_y;
        event->motion.x_root = (gdouble) xev->root_x;
        event->motion.y_root = (gdouble) xev->root_y;

        event->motion.device = g_hash_table_lookup (device_manager->id_table,
                                                    GINT_TO_POINTER (xev->deviceid));

        source_device = g_hash_table_lookup (device_manager->id_table,
                                             GUINT_TO_POINTER (xev->sourceid));
        gdk_event_set_source_device (event, source_device);

        event->motion.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);

        /* There doesn't seem to be motion hints in XI */
        event->motion.is_hint = FALSE;

        event->motion.axes = translate_axes (event->motion.device,
                                             event->motion.x,
                                             event->motion.y,
                                             event->motion.window,
                                             &xev->valuators);

        if (gdk_device_get_mode (event->motion.device) == GDK_MODE_WINDOW)
          {
            GdkDevice *device = event->motion.device;

            /* Update event coordinates from axes */
            gdk_device_get_axis (device, event->motion.axes, GDK_AXIS_X, &event->motion.x);
            gdk_device_get_axis (device, event->motion.axes, GDK_AXIS_Y, &event->motion.y);
          }
      }
      break;
    case XI_Enter:
    case XI_Leave:
      {
        XIEnterEvent *xev = (XIEnterEvent *) ev;
        GdkDevice *device, *source_device;

        event->crossing.type = (ev->evtype == XI_Enter) ? GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY;

        event->crossing.x = (gdouble) xev->event_x;
        event->crossing.y = (gdouble) xev->event_y;
        event->crossing.x_root = (gdouble) xev->root_x;
        event->crossing.y_root = (gdouble) xev->root_y;
        event->crossing.time = xev->time;
        event->crossing.focus = xev->focus;

        event->crossing.window = window;
        event->crossing.subwindow = gdk_x11_window_lookup_for_display (display, xev->child);

        device = g_hash_table_lookup (device_manager->id_table,
                                      GINT_TO_POINTER (xev->deviceid));
        gdk_event_set_device (event, device);

        source_device = g_hash_table_lookup (device_manager->id_table,
                                             GUINT_TO_POINTER (xev->sourceid));
        gdk_event_set_source_device (event, source_device);

        event->crossing.mode = translate_crossing_mode (xev->mode);
        event->crossing.detail = translate_notify_type (xev->detail);
        event->crossing.state = _gdk_x11_device_xi2_translate_state (&xev->mods, &xev->buttons, &xev->group);
      }
      break;
    case XI_FocusIn:
    case XI_FocusOut:
      {
        XIEnterEvent *xev = (XIEnterEvent *) ev;
        GdkDevice *device, *source_device;

        device = g_hash_table_lookup (device_manager->id_table,
                                      GINT_TO_POINTER (xev->deviceid));

        source_device = g_hash_table_lookup (device_manager->id_table,
                                             GUINT_TO_POINTER (xev->sourceid));

        handle_focus_change (window, device, source_device,
                             xev->detail, xev->mode,
                             (ev->evtype == XI_FocusIn) ? TRUE : FALSE);

        return_val = FALSE;
      }
    default:
      return_val = FALSE;
      break;
    }

  event->any.send_event = cookie->send_event;

  if (return_val)
    {
      if (event->any.window)
        g_object_ref (event->any.window);

      if (((event->any.type == GDK_ENTER_NOTIFY) ||
           (event->any.type == GDK_LEAVE_NOTIFY)) &&
          (event->crossing.subwindow != NULL))
        g_object_ref (event->crossing.subwindow);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.window = NULL;
      event->any.type = GDK_NOTHING;
    }

  XFreeEventData (dpy, cookie);

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
          GDK_POINTER_MOTION_HINT_MASK |
          GDK_BUTTON1_MOTION_MASK |
          GDK_BUTTON2_MOTION_MASK |
          GDK_BUTTON3_MOTION_MASK |
          GDK_BUTTON_MOTION_MASK |
          GDK_FOCUS_CHANGE_MASK);
}

static void
gdk_x11_device_manager_xi2_select_window_events (GdkEventTranslator *translator,
                                                 Window              window,
                                                 GdkEventMask        evmask)
{
  GdkDeviceManager *device_manager;
  XIEventMask event_mask;

  device_manager = GDK_DEVICE_MANAGER (translator);

  event_mask.deviceid = XIAllMasterDevices;
  event_mask.mask = _gdk_x11_device_xi2_translate_event_mask (evmask, &event_mask.mask_len);

  _gdk_x11_device_manager_xi2_select_events (device_manager, window, &event_mask);
  g_free (event_mask.mask);
}
