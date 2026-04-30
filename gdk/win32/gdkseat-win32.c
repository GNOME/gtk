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

#include "gdkseat-win32.h"
#include "gdkdevicetoolprivate.h"
#include "gdkwin32.h"

typedef struct _GdkWin32SeatPrivate GdkWin32SeatPrivate;

struct _GdkWin32SeatPrivate
{
  GdkDevice *logical_pointer;
  GdkDevice *logical_keyboard;
  GList *physical_pointers;
  GList *physical_keyboards;
  GdkSeatCapabilities capabilities;

  GPtrArray *tools;
};

G_DEFINE_TYPE_WITH_PRIVATE (GdkWin32Seat, gdk_win32_seat, GDK_TYPE_SEAT)

static void
gdk_win32_seat_dispose (GObject *object)
{
  GdkWin32Seat *seat = GDK_WIN32_SEAT (object);
  GdkWin32SeatPrivate *priv = gdk_win32_seat_get_instance_private (seat);
  GList *l;

  if (priv->logical_pointer)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), priv->logical_pointer);
      g_clear_object (&priv->logical_pointer);
    }

  if (priv->logical_keyboard)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), priv->logical_keyboard);
      g_clear_object (&priv->logical_pointer);
    }

  for (l = priv->physical_pointers; l; l = l->next)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), l->data);
      g_object_unref (l->data);
    }

  for (l = priv->physical_keyboards; l; l = l->next)
    {
      gdk_seat_device_removed (GDK_SEAT (seat), l->data);
      g_object_unref (l->data);
    }

  g_clear_pointer (&priv->tools, g_ptr_array_unref);

  g_list_free (priv->physical_pointers);
  g_list_free (priv->physical_keyboards);
  priv->physical_pointers = NULL;
  priv->physical_keyboards = NULL;

  G_OBJECT_CLASS (gdk_win32_seat_parent_class)->dispose (object);
}

static GdkSeatCapabilities
gdk_win32_seat_get_capabilities (GdkSeat *seat)
{
  GdkWin32SeatPrivate *priv;

  priv = gdk_win32_seat_get_instance_private (GDK_WIN32_SEAT (seat));

  return priv->capabilities;
}

static GdkGrabStatus
gdk_win32_seat_grab (GdkSeat    *seat,
                     GdkSurface *surface)
{
  GdkWin32SeatPrivate *priv;
  GdkWin32Display *win32_display = GDK_WIN32_DISPLAY (gdk_seat_get_display (seat));
  guint32 evtime = GDK_CURRENT_TIME;
  GdkGrabStatus status = GDK_GRAB_SUCCESS;
  gboolean was_visible;

  priv = gdk_win32_seat_get_instance_private (GDK_WIN32_SEAT (seat));
  was_visible = gdk_surface_get_mapped (surface);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

  SetCapture (GDK_SURFACE_HWND (surface));

  status = gdk_device_grab (priv->logical_pointer, surface,
                            TRUE,
                            NULL,
                            evtime);

  if (status == GDK_GRAB_SUCCESS)
    {
      status = gdk_device_grab (priv->logical_keyboard, surface,
                                TRUE,
                                NULL,
                                evtime);

      if (status != GDK_GRAB_SUCCESS)
        ReleaseCapture ();
    }

  if (status != GDK_GRAB_SUCCESS && !was_visible)
    gdk_surface_hide (surface);

  G_GNUC_END_IGNORE_DEPRECATIONS;

  return status;
}

static void
gdk_win32_seat_ungrab (GdkSeat *seat)
{
  GdkWin32SeatPrivate *priv;

  priv = gdk_win32_seat_get_instance_private (GDK_WIN32_SEAT (seat));

  ReleaseCapture ();

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS;
  gdk_device_ungrab (priv->logical_pointer, GDK_CURRENT_TIME);
  gdk_device_ungrab (priv->logical_keyboard, GDK_CURRENT_TIME);
  G_GNUC_END_IGNORE_DEPRECATIONS;
}

static GdkDevice *
gdk_win32_seat_get_logical_device (GdkSeat             *seat,
                                   GdkSeatCapabilities  capability)
{
  GdkWin32SeatPrivate *priv;

  priv = gdk_win32_seat_get_instance_private (GDK_WIN32_SEAT (seat));

  /* There must be only one flag set */
  switch ((guint) capability)
    {
    case GDK_SEAT_CAPABILITY_POINTER:
    case GDK_SEAT_CAPABILITY_TOUCH:
      return priv->logical_pointer;
    case GDK_SEAT_CAPABILITY_KEYBOARD:
      return priv->logical_keyboard;
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
gdk_win32_seat_get_devices (GdkSeat             *seat,
                            GdkSeatCapabilities  capabilities)
{
  GdkWin32SeatPrivate *priv;
  GList *devices = NULL;

  priv = gdk_win32_seat_get_instance_private (GDK_WIN32_SEAT (seat));

  if (capabilities & (GDK_SEAT_CAPABILITY_ALL_POINTING))
    devices = append_filtered (devices, priv->physical_pointers, capabilities);

  if (capabilities & (GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD))
    devices = append_filtered (devices, priv->physical_keyboards, capabilities);

  return devices;
}

static GList *
gdk_win32_seat_get_tools (GdkSeat *seat)
{
  GdkWin32SeatPrivate *priv;
  GdkDeviceTool *tool;
  GList *tools = NULL;
  guint i;

  priv = gdk_win32_seat_get_instance_private (GDK_WIN32_SEAT (seat));

  if (!priv->tools)
    return NULL;

  for (i = 0; i < priv->tools->len; i++)
    {
      tool = g_ptr_array_index (priv->tools, i);
      tools = g_list_prepend (tools, tool);
    }

  return tools;
}

static void
gdk_win32_seat_class_init (GdkWin32SeatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->dispose = gdk_win32_seat_dispose;

  seat_class->get_capabilities = gdk_win32_seat_get_capabilities;

  seat_class->grab = gdk_win32_seat_grab;
  seat_class->ungrab = gdk_win32_seat_ungrab;

  seat_class->get_logical_device = gdk_win32_seat_get_logical_device;
  seat_class->get_devices = gdk_win32_seat_get_devices;
  seat_class->get_tools = gdk_win32_seat_get_tools;
}

static void
gdk_win32_seat_init (GdkWin32Seat *seat)
{
}

GdkSeat *
gdk_win32_seat_new_for_logical_pair (GdkDevice *pointer,
                                     GdkDevice *keyboard)
{
  GdkWin32SeatPrivate *priv;
  GdkDisplay *display;
  GdkSeat *seat;

  display = gdk_device_get_display (pointer);

  seat = g_object_new (GDK_TYPE_WIN32_SEAT,
                       "display", display,
                       NULL);

  priv = gdk_win32_seat_get_instance_private (GDK_WIN32_SEAT (seat));
  priv->logical_pointer = g_object_ref (pointer);
  priv->logical_keyboard = g_object_ref (keyboard);

  gdk_seat_device_added (seat, priv->logical_pointer);
  gdk_seat_device_added (seat, priv->logical_keyboard);

  return seat;
}

void
gdk_win32_seat_add_physical_device (GdkWin32Seat *seat,
                                    GdkDevice  *device)
{
  GdkWin32SeatPrivate *priv;
  GdkSeatCapabilities capability;

  g_return_if_fail (GDK_IS_WIN32_SEAT (seat));
  g_return_if_fail (GDK_IS_DEVICE (device));

  priv = gdk_win32_seat_get_instance_private (seat);
  capability = device_get_capability (device);

  if (capability & GDK_SEAT_CAPABILITY_ALL_POINTING)
    priv->physical_pointers = g_list_prepend (priv->physical_pointers, g_object_ref (device));
  else if (capability & (GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD))
    priv->physical_keyboards = g_list_prepend (priv->physical_keyboards, g_object_ref (device));
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
gdk_win32_seat_remove_physical_device (GdkWin32Seat *seat,
                                       GdkDevice  *device)
{
  GdkWin32SeatPrivate *priv;
  GList *l;

  g_return_if_fail (GDK_IS_WIN32_SEAT (seat));
  g_return_if_fail (GDK_IS_DEVICE (device));

  priv = gdk_win32_seat_get_instance_private (seat);

  if (g_list_find (priv->physical_pointers, device))
    {
      priv->physical_pointers = g_list_remove (priv->physical_pointers, device);

      priv->capabilities &= ~(GDK_SEAT_CAPABILITY_ALL_POINTING);
      for (l = priv->physical_pointers; l; l = l->next)
        priv->capabilities |= device_get_capability (GDK_DEVICE (l->data));

      gdk_seat_device_removed (GDK_SEAT (seat), device);
      g_object_unref (device);
    }
  else if (g_list_find (priv->physical_keyboards, device))
    {
      priv->physical_keyboards = g_list_remove (priv->physical_keyboards, device);

      priv->capabilities &= ~(GDK_SEAT_CAPABILITY_KEYBOARD | GDK_SEAT_CAPABILITY_TABLET_PAD);
      for (l = priv->physical_keyboards; l; l = l->next)
        priv->capabilities |= device_get_capability (GDK_DEVICE (l->data));

      gdk_seat_device_removed (GDK_SEAT (seat), device);
      g_object_unref (device);
    }
}

void
gdk_win32_seat_add_tool (GdkWin32Seat  *seat,
                         GdkDeviceTool *tool)
{
  GdkWin32SeatPrivate *priv;

  g_return_if_fail (GDK_IS_WIN32_SEAT (seat));
  g_return_if_fail (tool != NULL);

  priv = gdk_win32_seat_get_instance_private (seat);

  if (!priv->tools)
    priv->tools = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  g_ptr_array_add (priv->tools, g_object_ref (tool));
  g_signal_emit_by_name (seat, "tool-added", tool);
}

void
gdk_win32_seat_remove_tool (GdkWin32Seat  *seat,
                            GdkDeviceTool *tool)
{
  GdkWin32SeatPrivate *priv;

  g_return_if_fail (GDK_IS_WIN32_SEAT (seat));
  g_return_if_fail (tool != NULL);

  priv = gdk_win32_seat_get_instance_private (seat);

  if (tool != gdk_seat_get_tool (GDK_SEAT (seat), tool->serial, tool->hw_id, tool->type))
    return;

  g_signal_emit_by_name (seat, "tool-removed", tool);
  g_ptr_array_remove (priv->tools, tool);
}
