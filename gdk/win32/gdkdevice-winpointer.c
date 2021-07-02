/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 the GTK team
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

#include <gdk/gdksurface.h>

#include <windows.h>

#include "gdkwin32.h"
#include "gdkdevice-winpointer.h"
#include "gdkdisplay-win32.h"

G_DEFINE_TYPE (GdkDeviceWinpointer, gdk_device_winpointer, GDK_TYPE_DEVICE)

static GdkModifierType
get_keyboard_mask (void)
{
  GdkModifierType mask = 0;
  BYTE kbd[256];

  GetKeyboardState (kbd);
  if (kbd[VK_SHIFT] & 0x80)
    mask |= GDK_SHIFT_MASK;
  if (kbd[VK_CAPITAL] & 0x80)
    mask |= GDK_LOCK_MASK;
  if (kbd[VK_CONTROL] & 0x80)
    mask |= GDK_CONTROL_MASK;
  if (kbd[VK_MENU] & 0x80)
    mask |= GDK_ALT_MASK;

  return mask;
}

static void
gdk_device_winpointer_set_surface_cursor (GdkDevice *device,
                                          GdkSurface *window,
                                          GdkCursor *cursor)
{
}

void
gdk_device_winpointer_query_state (GdkDevice        *device,
                                   GdkSurface       *window,
                                   GdkSurface      **child_window,
                                   double           *win_x,
                                   double           *win_y,
                                   GdkModifierType  *mask)
{
  GdkDeviceWinpointer *device_winpointer;
  POINT point;
  HWND hwnd, hwndc;
  int scale;

  device_winpointer = GDK_DEVICE_WINPOINTER (device);
  if (window)
    {
      scale = GDK_WIN32_SURFACE (window)->surface_scale;
      hwnd = GDK_SURFACE_HWND (window);
    }
  else
    {
      GdkDisplay *display = gdk_device_get_display (device);

      scale = GDK_WIN32_DISPLAY (display)->surface_scale;
      hwnd = NULL;
    }

  GetCursorPos (&point);

  if (hwnd)
    ScreenToClient (hwnd, &point);

  if (win_x)
    *win_x = point.x / scale;

  if (win_y)
    *win_y = point.y / scale;

  if (!window)
    {
      if (win_x)
        *win_x += _gdk_offset_x;

      if (win_y)
        *win_y += _gdk_offset_y;
    }

  if (hwnd && child_window)
    {
      hwndc = ChildWindowFromPoint (hwnd, point);

      if (hwndc && hwndc != hwnd)
        *child_window = gdk_win32_handle_table_lookup (hwndc);
      else
        *child_window = NULL; /* Direct child unknown to gdk */
    }

  if (mask)
    {
      *mask = get_keyboard_mask ();
      *mask |= device_winpointer->last_button_mask;
    }
}

static GdkGrabStatus
gdk_device_winpointer_grab (GdkDevice    *device,
                            GdkSurface    *window,
                            gboolean      owner_events,
                            GdkEventMask  event_mask,
                            GdkSurface    *confine_to,
                            GdkCursor    *cursor,
                            guint32       time_)
{
  return GDK_GRAB_SUCCESS;
}

static void
gdk_device_winpointer_ungrab (GdkDevice *device,
                              guint32    time_)
{
}

static GdkSurface *
gdk_device_winpointer_surface_at_position (GdkDevice       *device,
                                           double          *win_x,
                                           double          *win_y,
                                           GdkModifierType *mask)
{
  return NULL;
}

static void
gdk_device_winpointer_init (GdkDeviceWinpointer *device_winpointer)
{
  device_winpointer->device_handle = NULL;
  device_winpointer->start_cursor_id = 0;
  device_winpointer->end_cursor_id = 0;

  device_winpointer->origin_x = 0;
  device_winpointer->origin_y = 0;
  device_winpointer->scale_x = 0.0;
  device_winpointer->scale_y = 0.0;

  device_winpointer->last_axis_data = NULL;
  device_winpointer->num_axes = 0;
  device_winpointer->last_button_mask = 0;
}

static void
gdk_device_winpointer_finalize (GObject *object)
{
  GdkDeviceWinpointer *device_winpointer = GDK_DEVICE_WINPOINTER (object);

  g_clear_object (&device_winpointer->tool_pen);
  g_clear_object (&device_winpointer->tool_eraser);

  g_free (device_winpointer->last_axis_data);

  G_OBJECT_CLASS (gdk_device_winpointer_parent_class)->finalize (object);
}

static void
gdk_device_winpointer_class_init (GdkDeviceWinpointerClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_device_winpointer_finalize;
  device_class->set_surface_cursor = gdk_device_winpointer_set_surface_cursor;
  device_class->grab = gdk_device_winpointer_grab;
  device_class->ungrab = gdk_device_winpointer_ungrab;
  device_class->surface_at_position = gdk_device_winpointer_surface_at_position;
}
