/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2015 Red Hat
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "gdkseatdefaultprivate.h"
#include "gdkdevicetoolprivate.h"
#include "gdkinternals.h"

typedef struct _GdkSeatDefaultPrivate GdkSeatDefaultPrivate;

struct _GdkSeatDefaultPrivate
{
  GdkDevice *master_pointer;
  GdkDevice *master_keyboard;
  GList *slave_pointers;
  GList *slave_keyboards;
  GdkSeatCapabilities capabilities;

  GPtrArray *tools;
};

#define KEYBOARD_EVENTS (GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK |    \
                         GDK_FOCUS_CHANGE_MASK)
#define TOUCH_EVENTS    (GDK_TOUCH_MASK)
#define POINTER_EVENTS  (GDK_POINTER_MOTION_MASK |                      \
                         GDK_BUTTON_PRESS_MASK |                        \
                         GDK_BUTTON_RELEASE_MASK |                      \
                         GDK_SCROLL_MASK | GDK_SMOOTH_SCROLL_MASK |     \
                         GDK_ENTER_NOTIFY_MASK |                        \
                         GDK_LEAVE_NOTIFY_MASK |                        \
                         GDK_PROXIMITY_IN_MASK |                        \
                         GDK_PROXIMITY_OUT_MASK)

G_DEFINE_TYPE_WITH_PRIVATE (GdkSeatDefault, gdk_seat_default, GDK_TYPE_SEAT)

static void
gdk_seat_dispose (GObject *object)
{
  GdkSeatDefault *seat = GDK_SEAT_DEFAULT (object);
  GdkSeatDefaultPrivate *priv = gdk_seat_default_get_instance_private (seat);
  GList *l;

  if (priv->master_pointer)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), priv->master_pointer);
      g_clear_object (&priv->master_pointer);
    }

  if (priv->master_keyboard)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), priv->master_keyboard);
      g_clear_object (&priv->master_pointer);
    }

  for (l = priv->slave_pointers; l; l = l->next)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), l->data);
      g_object_unref (l->data);
    }

  for (l = priv->slave_keyboards; l; l = l->next)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), l->data);
      g_object_unref (l->data);
    }

  if (priv->tools)
    {
      g_ptr_array_unref (priv->tools);
      priv->tools = NULL;
    }

  g_list_free (priv->slave_pointers);
  g_list_free (priv->slave_keyboards);
  priv->slave_pointers = NULL;
  priv->slave_keyboards = NULL;

  G_OBJECT_CLASS (gdk_seat_default_parent_class)->dispose (object);
}

static GdkSeatCapabilities
gdk_seat_default_get_capabilities (GdkSeat *seat)
{
  GdkSeatDefaultPrivate *priv;

  priv = gdk_seat_default_get_instance_private (GDK_SEAT_DEFAULT (seat));

  return priv->capabilities;
}

static GdkGrabStatus
gdk_seat_default_grab (GdkSeat                *seat,
                       GdkSurface              *surface,
                       GdkSeatCapabilities     capabilities,
                       gboolean                owner_events,
                       GdkCursor              *cursor,
                       const GdkEvent         *event,
                       GdkSeatGrabPrepareFunc  prepare_func,
                       gpointer                prepare_func_data)
{
  GdkSeatDefaultPrivate *priv;
  guint32 evtime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
  GdkGrabStatus status = GDK_GRAB_SUCCESS;
  gboolean was_visible;

  priv = gdk_seat_default_get_instance_private (GDK_SEAT_DEFAULT (seat));
  was_visible = gdk_surface_is_visible (surface);

  if (prepare_func)
    (prepare_func) (seat, surface, prepare_func_data);

  if (!gdk_surface_is_visible (surface))
    {
      g_critical ("Surface %p has not been made visible in GdkSeatGrabPrepareFunc",
                  surface);
      return GDK_GRAB_NOT_VIEWABLE;
    }

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  if (capabilities & GDK_SEAT_CAPABILITY_ALL_POINTING)
    {
      /* ALL_POINTING spans 3 capabilities; get the mask for the ones we have */
      GdkEventMask pointer_evmask = 0;

      /* We let tablet styli take over the pointer cursor */
      if (capabilities & (GDK_SEAT_CAPABILITY_POINTER |
                          GDK_SEAT_CAPABILITY_TABLET_STYLUS))
        {
          pointer_evmask |= POINTER_EVENTS;
        }

      if (capabilities & GDK_SEAT_CAPABILITY_TOUCH)
        pointer_evmask |= TOUCH_EVENTS;

      status = gdk_device_grab (priv->master_pointer, surface,
                                GDK_OWNERSHIP_NONE, owner_events,
                                pointer_evmask, cursor,
                                evtime);
    }

  if (status == GDK_GRAB_SUCCESS &&
      capabilities & GDK_SEAT_CAPABILITY_KEYBOARD)
    {
      status = gdk_device_grab (priv->master_keyboard, surface,
                                GDK_OWNERSHIP_NONE, owner_events,
                                KEYBOARD_EVENTS, cursor,
                                evtime);

      if (status != GDK_GRAB_SUCCESS)
        {
          if (capabilities & ~GDK_SEAT_CAPABILITY_KEYBOARD)
            gdk_device_ungrab (priv->master_pointer, evtime);
        }
    }

  if (status != GDK_GRAB_SUCCESS && !was_visible)
    gdk_surface_hide (surface);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return status;
}

static void
gdk_seat_default_ungrab (GdkSeat *seat)
{
  GdkSeatDefaultPrivate *priv;

  priv = gdk_seat_default_get_instance_private (GDK_SEAT_DEFAULT (seat));

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gdk_device_ungrab (priv->master_pointer, GDK_CURRENT_TIME);
  gdk_device_ungrab (priv->master_keyboard, GDK_CURRENT_TIME);
  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static GdkDevice *
gdk_seat_default_get_master (GdkSeat             *seat,
                             GdkSeatCapabilities  capability)
{
  GdkSeatDefaultPrivate *priv;

  priv = gdk_seat_default_get_instance_private (GDK_SEAT_DEFAULT (seat));

  /* There must be only one flag set */
  switch ((guint) capability)
    {
    case GDK_SEAT_CAPABILITY_POINTER:
    case GDK_SEAT_CAPABILITY_TOUCH:
      return priv->master_pointer;
    case GDK_SEAT_CAPABILITY_KEYBOARD:
      return priv->master_keyboard;
    default:
      g_warning ("Unhandled capability %x", capability);
      break;
    }

  return NULL;
}

static GdkSeatCapabilities
device_get_capability (GdkDevice *device)
{
  GdkInputSource source;

  source = gdk_device_get_source (device);

  switch (source)
    {
    case GDK_SOURCE_KEYBOARD:
      return GDK_SEAT_CAPABILITY_KEYBOARD;
    case GDK_SOURCE_TOUCHSCREEN:
      return GDK_SEAT_CAPABILITY_TOUCH;
    case GDK_SOURCE_PEN:
    case GDK_SOURCE_ERASER:
    case GDK_SOURCE_CURSOR:
      return GDK_SEAT_CAPABILITY_TABLET_STYLUS;
    case GDK_SOURCE_TABLET_PAD:
      return GDK_SEAT_CAPABILITY_TABLET_PAD;
    case GDK_SOURCE_MOUSE:
    case GDK_SOURCE_TOUCHPAD:
    case GDK_SOURCE_TRACKPOINT:
    default:
      return GDK_SEAT_CAPABILITY_POINTER;
    }

  return GDK_SEAT_CAPABILITY_NONE;
}

static GList *
append_filtered (GList               *list,
                 GList               *devices,
                 GdkSeatCapabilities  capabilities)
{
  GList *l;

  for (l = devices; l; l = l->next)
    {
      GdkSeatCapabilities device_cap;

      device_cap = device_get_capability (l->data);

      if ((device_cap & capabilities) != 0)
        list = g_list_prepend (list, l->data);
    }

  return list;
}

static GList *
gdk_seat_default_get_slaves (GdkSeat             *seat,
                             GdkSeatCapabilities  capabilities)
{
  GdkSeatDefaultPrivate *priv;
  GList *devices = NULL;

  priv = gdk_seat_default_get_instance_private (GDK_SEAT_DEFAULT (seat));

  if (capabilities & (GDK_SEAT_CAPABILITY_ALL_POINTING))
    devices = append_filtered (devices, priv->slave_pointers, capabilities);

  if (capabilities & (GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD))
    devices = append_filtered (devices, priv->slave_keyboards, capabilities);

  return devices;
}

static GdkDeviceTool *
gdk_seat_default_get_tool (GdkSeat *seat,
                           guint64  serial,
                           guint64  hw_id)
{
  GdkSeatDefaultPrivate *priv;
  GdkDeviceTool *tool;
  guint i;

  priv = gdk_seat_default_get_instance_private (GDK_SEAT_DEFAULT (seat));

  if (!priv->tools)
    return NULL;

  for (i = 0; i < priv->tools->len; i++)
    {
      tool = g_ptr_array_index (priv->tools, i);

      if (tool->serial == serial && tool->hw_id == hw_id)
        return tool;
    }

  return NULL;
}

static GList *
gdk_seat_default_get_master_pointers (GdkSeat             *seat,
                                      GdkSeatCapabilities  capabilities)
{
  GList *masters = NULL;

  if (capabilities & GDK_SEAT_CAPABILITY_ALL_POINTING)
    masters = g_list_prepend (masters, gdk_seat_get_pointer (seat));

  return masters;
}

static void
gdk_seat_default_class_init (GdkSeatDefaultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->dispose = gdk_seat_dispose;

  seat_class->get_capabilities = gdk_seat_default_get_capabilities;

  seat_class->grab = gdk_seat_default_grab;
  seat_class->ungrab = gdk_seat_default_ungrab;

  seat_class->get_master = gdk_seat_default_get_master;
  seat_class->get_slaves = gdk_seat_default_get_slaves;
  seat_class->get_master_pointers = gdk_seat_default_get_master_pointers;

  seat_class->get_tool = gdk_seat_default_get_tool;
}

static void
gdk_seat_default_init (GdkSeatDefault *seat)
{
}

GdkSeat *
gdk_seat_default_new_for_master_pair (GdkDevice *pointer,
                                      GdkDevice *keyboard)
{
  GdkSeatDefaultPrivate *priv;
  GdkDisplay *display;
  GdkSeat *seat;

  display = gdk_device_get_display (pointer);

  seat = g_object_new (GDK_TYPE_SEAT_DEFAULT,
                       "display", display,
                       NULL);

  priv = gdk_seat_default_get_instance_private (GDK_SEAT_DEFAULT (seat));
  priv->master_pointer = g_object_ref (pointer);
  priv->master_keyboard = g_object_ref (keyboard);

  gdk_seat_device_added (seat, priv->master_pointer);
  gdk_seat_device_added (seat, priv->master_keyboard);

  return seat;
}

void
gdk_seat_default_add_slave (GdkSeatDefault *seat,
                            GdkDevice      *device)
{
  GdkSeatDefaultPrivate *priv;
  GdkSeatCapabilities capability;

  g_return_if_fail (GDK_IS_SEAT_DEFAULT (seat));
  g_return_if_fail (GDK_IS_DEVICE (device));

  priv = gdk_seat_default_get_instance_private (seat);
  capability = device_get_capability (device);

  if (capability & GDK_SEAT_CAPABILITY_ALL_POINTING)
    priv->slave_pointers = g_list_prepend (priv->slave_pointers, g_object_ref (device));
  else if (capability & (GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD))
    priv->slave_keyboards = g_list_prepend (priv->slave_keyboards, g_object_ref (device));
  else
    {
      g_critical ("Unhandled capability %x for device '%s'",
                  capability, gdk_device_get_name (device));
      return;
    }

  priv->capabilities |= capability;

  gdk_seat_device_added (GDK_SEAT (seat), device);
}

void
gdk_seat_default_remove_slave (GdkSeatDefault *seat,
                               GdkDevice      *device)
{
  GdkSeatDefaultPrivate *priv;
  GList *l;

  g_return_if_fail (GDK_IS_SEAT_DEFAULT (seat));
  g_return_if_fail (GDK_IS_DEVICE (device));

  priv = gdk_seat_default_get_instance_private (seat);

  if (g_list_find (priv->slave_pointers, device))
    {
      priv->slave_pointers = g_list_remove (priv->slave_pointers, device);

      priv->capabilities &= ~(GDK_SEAT_CAPABILITY_ALL_POINTING);
      for (l = priv->slave_pointers; l; l = l->next)
        priv->capabilities |= device_get_capability (GDK_DEVICE (l->data));

      gdk_seat_device_removed (GDK_SEAT (seat), device);
    }
  else if (g_list_find (priv->slave_keyboards, device))
    {
      priv->slave_keyboards = g_list_remove (priv->slave_keyboards, device);

      priv->capabilities &= ~(GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD);
      for (l = priv->slave_keyboards; l; l = l->next)
        priv->capabilities |= device_get_capability (GDK_DEVICE (l->data));

      gdk_seat_device_removed (GDK_SEAT (seat), device);
    }
}

void
gdk_seat_default_add_tool (GdkSeatDefault *seat,
                           GdkDeviceTool  *tool)
{
  GdkSeatDefaultPrivate *priv;

  g_return_if_fail (GDK_IS_SEAT_DEFAULT (seat));
  g_return_if_fail (tool != NULL);

  priv = gdk_seat_default_get_instance_private (seat);

  if (!priv->tools)
    priv->tools = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  g_ptr_array_add (priv->tools, g_object_ref (tool));
  g_signal_emit_by_name (seat, "tool-added", tool);
}

void
gdk_seat_default_remove_tool (GdkSeatDefault *seat,
                              GdkDeviceTool  *tool)
{
  GdkSeatDefaultPrivate *priv;

  g_return_if_fail (GDK_IS_SEAT_DEFAULT (seat));
  g_return_if_fail (tool != NULL);

  priv = gdk_seat_default_get_instance_private (seat);

  if (tool != gdk_seat_get_tool (GDK_SEAT (seat), tool->serial, tool->hw_id))
    return;

  g_signal_emit_by_name (seat, "tool-removed", tool);
  g_ptr_array_remove (priv->tools, tool);
}
